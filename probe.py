#!/usr/bin/env python3
"""Read-only probe for the Attack Shark X11 Ultra (3554:f517).
Faithful port of the vendor's ul() transport: retries + 200ms windows."""
import os, select, random, time, sys

from x11ultra import find_device

RID = 8
CMD = {"Encrypt":1, "PCDriver":2, "Online":3, "Battery":4,
       "WriteFlash":7, "ReadFlash":8, "Version":18, "DongleVer":29}

def build(cmd, payload=(), addr=None, ln=None, typeoff=0):
    p = bytearray(16); p[0] = cmd
    if addr is not None:
        p[2] = (addr >> 8) & 0xFF; p[3] = addr & 0xFF; p[4] = ln + typeoff
    else:
        p[4] = len(payload) + typeoff
    for i, b in enumerate(payload): p[5+i] = b
    p[15] = ((85 - (sum(p[:15]) & 0xFF)) - RID) & 0xFF
    return bytes(p)

def ul(fd, pkt, tries=5, window=0.25):
    """Send with retries; return Le[] (report-id stripped) on echo match."""
    ncheck = 5 if pkt[0] == CMD["ReadFlash"] else 3
    for _ in range(tries):
        os.write(fd, bytes([RID]) + pkt)
        end = time.time() + window
        while time.time() < end:
            r,_,_ = select.select([fd], [], [], 0.02)
            if not r: continue
            try: d = os.read(fd, 64)
            except BlockingIOError: continue
            if not d or d[0] != RID: continue
            Le = d[1:]
            if Le[1] == 1: return Le            # device-ack path
            if all(pkt[i] == Le[i] for i in range(ncheck)):
                return Le
    return None

node = sys.argv[1] if len(sys.argv) > 1 else find_device()
if node is None:
    print("no Attack Shark X11 Ultra found. is it plugged in?")
    sys.exit(1)
print(f"using {node}\n")

fd = os.open(node, os.O_RDWR | os.O_NONBLOCK)
try:
    nonce = [random.randrange(256) for _ in range(4)] + [0,0,0,0]
    Le = ul(fd, build(CMD["Encrypt"], nonce))
    if not Le: print("handshake FAILED"); sys.exit(1)
    tmap = {0:"wireless 1k",1:"wireless 4k",2:"wired 1k",3:"wired 8k",
            4:"wireless 2k",5:"wireless 8k",6:"chargingBase"}
    print(f"handshake OK  cid={Le[9]} mid={Le[10]} type={Le[11]} ({tmap.get(Le[11],'?')})")

    ul(fd, build(CMD["PCDriver"], [1]))          # announce driver
    Le = ul(fd, build(CMD["Online"]))
    if Le: print(f"online flag={Le[5]}  paired addr={Le[8]:02x}:{Le[7]:02x}:{Le[6]:02x}")

    print("\nflash reads:")
    for name, addr, ln in [("ReportRate",0,2), ("maxDpiStage",2,2), ("CurrentDPI",4,2),
                           ("LOD",10,2), ("DPIValue",12,10), ("DebounceTime",169,2),
                           ("MotionSync",171,2), ("Angle",175,2)]:
        Le = ul(fd, build(CMD["ReadFlash"], addr=addr, ln=ln))
        if Le:
            got = (Le[2] << 8) + Le[3]
            print(f"  {name:<13} addr={got:<4} len={Le[4]&15} data={bytes(Le[5:5+ln]).hex(' ')}")
        else:
            print(f"  {name:<13} no response")
finally:
    os.close(fd)
