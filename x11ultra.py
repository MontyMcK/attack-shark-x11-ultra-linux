#!/usr/bin/env python3
"""Attack Shark X11 Ultra (3554:f517) config transport.

Verified against hardware. See PROTOCOL-X11-ULTRA.md.
Values are stored as [v, 85-v] redundancy pairs.
"""
import glob, os, select, random, time

RID = 8

VID = 0x3554
# f517 is the 2.4 GHz dongle, f515 is the cable. Same mouse, same protocol.
PIDS = (0xf517, 0xf515)


def find_device():
    """Path of the hidraw node carrying the config interface, or None.

    The mouse exposes several hidraw nodes and only one of them answers. Pick
    the one whose report descriptor declares vendor usage page 0xFF02 with
    report ID 8, so node renumbering and the dongle/cable PID switch cannot
    break discovery.
    """
    for sysdir in sorted(glob.glob("/sys/class/hidraw/hidraw*")):
        try:
            with open(os.path.join(sysdir, "device/uevent")) as f:
                uevent = f.read()
        except OSError:
            continue
        hid_id = ""
        for line in uevent.splitlines():
            if line.startswith("HID_ID="):
                hid_id = line.split("=", 1)[1]
        parts = hid_id.split(":")
        if len(parts) != 3:
            continue
        try:
            vid, pid = int(parts[1], 16), int(parts[2], 16)
        except ValueError:
            continue
        if vid != VID or pid not in PIDS:
            continue
        try:
            with open(os.path.join(sysdir, "device/report_descriptor"), "rb") as f:
                desc = f.read()
        except OSError:
            continue
        # 06 02 ff = Usage Page (Vendor 0xFF02); 85 08 = Report ID 8
        if b"\x06\x02\xff" in desc and b"\x85\x08" in desc:
            return "/dev/" + os.path.basename(sysdir)
    return None
ENCRYPT, PCDRIVER, ONLINE, WRITE_FLASH, READ_FLASH = 1, 2, 3, 7, 8

OFF = {"ReportRate":0, "maxDpiStage":2, "CurrentDPI":4, "KeyOperation":8,
       "LOD":10, "DPIValue":12, "DPIColor":44, "Light":160, "DebounceTime":169,
       "MotionSync":171, "SleepTime":173, "Angle":175, "Ripple":177,
       "SensorMode":185, "SensorFPS20K":225, "WheelDebounceTime":227}

RATE_TO_CODE = {125:0x08, 250:0x04, 500:0x02, 1000:0x01,
                2000:0x10, 4000:0x20, 8000:0x40}
CODE_TO_RATE = {v: k for k, v in RATE_TO_CODE.items()}

# PAW3950 (this unit: cid 124 / mid 11, per the vendor cfg.json).
# range[0] 50..30000 step 50 DPIex 0 ; range[1] 30100..60000 step 100 DPIex 17
DPI_STEP = 50
DPI_MAX_SIMPLE = 30000
DPI_RECORD_LEN = 4          # per stage, at OFF["DPIValue"] + stage*4


DPI_EX = 17                 # range[1] marker; halves the stored value
DPI_MAX = 60000


def dpi_to_record(dpi):
    """Encode a DPI into its 4-byte flash record (vendor Go() + Lb())."""
    if not (DPI_STEP <= dpi <= DPI_MAX):
        raise ValueError(f"DPI must be {DPI_STEP}..{DPI_MAX}")
    if dpi <= DPI_MAX_SIMPLE:
        if dpi % DPI_STEP:
            raise ValueError(f"DPI must be a multiple of {DPI_STEP}")
        val, ex = dpi // DPI_STEP - 1, 0
    else:
        # Above 30000 the value is stored halved and flagged with DPIex.
        if dpi % (DPI_STEP * 2):
            raise ValueError(f"DPI above {DPI_MAX_SIMPLE} must be a multiple of {DPI_STEP*2}")
        val, ex = (dpi // 2) // DPI_STEP - 1, DPI_EX
    r = [val & 0xFF, val & 0xFF, 0, 0]
    hi = val >> 8
    r[2] = ((hi << 2) | (hi << 6) | ex | (ex << 4)) & 0xFF
    r[3] = (85 - (sum(r[:3]) & 0xFF)) & 0xFF
    return bytes(r)


def record_to_dpi(r):
    """Decode a 4-byte DPI record. Returns None if the checksum fails."""
    if len(r) < 4:
        return None
    if r[3] != (85 - (sum(r[:3]) & 0xFF)) & 0xFF:
        return None
    # r[2] packs: value high bits at 2-3 and 6-7, DPIex at bits 0 and 4.
    # Bit 0 set means DPIex==17, i.e. the stored value is halved.
    doubled = bool(r[2] & 0x01)
    hi = (r[2] >> 2) & 0x03
    dpi = (((hi << 8) | r[0]) + 1) * DPI_STEP
    return dpi * 2 if doubled else dpi


class X11Ultra:
    def __init__(self, node=None, typeoff=0):
        """node=None finds the mouse automatically."""
        self.node, self.typeoff = node, typeoff
        self.fd = None

    def __enter__(self):
        if self.node is None:
            self.node = find_device()
            if self.node is None:
                raise RuntimeError("no Attack Shark X11 Ultra found")
        self.fd = os.open(self.node, os.O_RDWR | os.O_NONBLOCK)
        self._drain()
        if not self.handshake():
            raise RuntimeError("handshake failed")
        self._xfer(self._pkt(PCDRIVER, payload=[1]))
        return self

    def __exit__(self, *a):
        if self.fd is not None:
            os.close(self.fd)

    # ---- framing -------------------------------------------------------
    def _pkt(self, cmd, payload=(), addr=None, ln=None):
        p = bytearray(16); p[0] = cmd
        if addr is not None:
            p[2] = (addr >> 8) & 0xFF; p[3] = addr & 0xFF
            p[4] = ln + self.typeoff
        else:
            p[4] = len(payload) + self.typeoff
        for i, b in enumerate(payload): p[5+i] = b
        p[15] = ((85 - (sum(p[:15]) & 0xFF)) - RID) & 0xFF
        return bytes(p)

    def _drain(self):
        while True:
            r,_,_ = select.select([self.fd], [], [], 0)
            if not r: return
            try: os.read(self.fd, 64)
            except (BlockingIOError, OSError): return

    def _xfer(self, pkt, tries=8, window=0.30):
        """Vendor ul(): resend until the echo matches. Returns Le[] or None."""
        ncheck = 5 if pkt[0] == READ_FLASH else 3
        for _ in range(tries):
            os.write(self.fd, bytes([RID]) + pkt)
            end = time.time() + window
            while time.time() < end:
                r,_,_ = select.select([self.fd], [], [], 0.02)
                if not r: continue
                try: d = os.read(self.fd, 64)
                except BlockingIOError: continue
                if not d or d[0] != RID: continue
                Le = d[1:]
                if all(pkt[i] == Le[i] for i in range(ncheck)):
                    return Le
        return None

    # ---- commands ------------------------------------------------------
    def handshake(self):
        nonce = [random.randrange(256) for _ in range(4)] + [0, 0, 0, 0]
        Le = self._xfer(self._pkt(ENCRYPT, nonce))
        if not Le: return None
        self.cid, self.mid, self.type = Le[9], Le[10], Le[11]
        return True

    def read(self, addr, ln):
        """Read ln bytes (max 10) from config flash."""
        Le = self._xfer(self._pkt(READ_FLASH, addr=addr, ln=ln))
        return bytes(Le[5:5+ln]) if Le else None

    def read_block(self, start, end, rounds=3):
        """Read a range in 10-byte chunks. Returns dict addr->byte, or None.

        Reads contend with 8 kHz motion traffic, so retry the whole sweep
        before giving up rather than failing on a single missed chunk.
        """
        for _ in range(rounds):
            out = {}
            a = start
            ok = True
            while a < end:
                n = min(10, end - a)
                got = self.read(a, n)
                if got is None:
                    ok = False
                    break
                for i, b in enumerate(got): out[a+i] = b
                a += n
            if ok:
                return out
        return None

    def write_value(self, addr, val):
        """Vendor Qe(): write a [v, 85-v] pair. Returns True on ack."""
        pkt = self._pkt(WRITE_FLASH, payload=[val, (85 - val) & 0xFF],
                        addr=addr, ln=2)
        return self._xfer(pkt) is not None

    # ---- convenience ---------------------------------------------------
    def get_rate(self):
        v = self.read(OFF["ReportRate"], 2)
        return CODE_TO_RATE.get(v[0]) if v else None

    def get_dpi_stages(self):
        """Returns [(stage, dpi), ...] for the active stages, or None."""
        n = self.read(OFF["maxDpiStage"], 2)
        if n is None:
            return None
        count = n[0]
        out = []
        for stage in range(count):
            addr = OFF["DPIValue"] + stage * DPI_RECORD_LEN
            rec = self.read(addr, DPI_RECORD_LEN)
            if rec is None:
                return None
            out.append((stage, record_to_dpi(rec)))
        return out

    def set_dpi_stage(self, stage, dpi):
        """Write one DPI stage. Uses the 10-byte bulk write (vendor gt())."""
        rec = dpi_to_record(dpi)
        addr = OFF["DPIValue"] + stage * DPI_RECORD_LEN
        pkt = self._pkt(WRITE_FLASH, payload=list(rec), addr=addr,
                        ln=DPI_RECORD_LEN)
        return self._xfer(pkt) is not None

    def set_rate(self, hz):
        if hz not in RATE_TO_CODE:
            raise ValueError(f"unsupported rate {hz}; pick {sorted(RATE_TO_CODE)}")
        return self.write_value(OFF["ReportRate"], RATE_TO_CODE[hz])
