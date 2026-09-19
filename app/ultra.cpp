#include "ultra.h"

#include <QFile>

#include <libudev.h>

#include <cerrno>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

namespace ultra {
namespace {

constexpr uint8_t kReportId = 8;      // vendor `Wt`
constexpr int     kPacketLen = 16;

enum Cmd : uint8_t {
    CmdEncrypt    = 1,
    CmdPcDriver   = 2,
    CmdBattery    = 4,
    CmdWriteFlash = 7,
    CmdReadFlash  = 8,
};

// Settings are stored as [value, 85-value] redundancy pairs.
inline uint8_t pairByte(uint8_t v) { return static_cast<uint8_t>(85 - v); }

void buildPacket(uint8_t out[kPacketLen], uint8_t cmd, const uint8_t *payload,
                 int payloadLen, int addr = -1, int len = -1)
{
    std::memset(out, 0, kPacketLen);
    out[0] = cmd;
    if (addr >= 0) {
        out[2] = static_cast<uint8_t>((addr >> 8) & 0xFF);
        out[3] = static_cast<uint8_t>(addr & 0xFF);
        out[4] = static_cast<uint8_t>(len);
    } else {
        out[4] = static_cast<uint8_t>(payloadLen);
    }
    for (int i = 0; i < payloadLen && (5 + i) < 15; ++i)
        out[5 + i] = payload[i];

    unsigned sum = 0;
    for (int i = 0; i < 15; ++i)
        sum += out[i];
    out[15] = static_cast<uint8_t>((85 - (sum & 0xFF)) - kReportId);
}

// The vendor's ul(): the radio is busy carrying 8 kHz motion reports, so a
// single send usually gets no reply. Resend until the echo matches. Without
// this the device looks unresponsive and appears unsupported.
bool transfer(int fd, const uint8_t pkt[kPacketLen], uint8_t reply[kPacketLen],
              int tries = 8, int windowMs = 300)
{
    const int echoBytes = (pkt[0] == CmdReadFlash) ? 5 : 3;

    uint8_t wire[kPacketLen + 1];
    wire[0] = kReportId;
    std::memcpy(wire + 1, pkt, kPacketLen);

    for (int attempt = 0; attempt < tries; ++attempt) {
        if (::write(fd, wire, sizeof(wire)) < 0)
            continue;

        struct timespec start {};
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (;;) {
            struct timespec now {};
            clock_gettime(CLOCK_MONOTONIC, &now);
            const long elapsed = (now.tv_sec - start.tv_sec) * 1000
                               + (now.tv_nsec - start.tv_nsec) / 1000000;
            if (elapsed >= windowMs)
                break;

            struct pollfd p { fd, POLLIN, 0 };
            if (::poll(&p, 1, 20) <= 0)
                continue;

            uint8_t buf[64];
            const ssize_t n = ::read(fd, buf, sizeof(buf));
            if (n <= 1 || buf[0] != kReportId)
                continue;

            // hidraw prepends the report id, so vendor Le[i] == buf[i+1].
            const uint8_t *le = buf + 1;
            bool match = true;
            for (int i = 0; i < echoBytes; ++i) {
                if (pkt[i] != le[i]) { match = false; break; }
            }
            if (match) {
                if (reply)
                    std::memcpy(reply, le, kPacketLen);
                return true;
            }
        }
    }
    return false;
}

bool handshake(int fd, int *deviceType = nullptr)
{
    // 4 random bytes then 4 zeros; the reply carries cid/mid/type.
    uint8_t nonce[8] = {0};
    for (int i = 0; i < 4; ++i)
        nonce[i] = static_cast<uint8_t>(rand() & 0xFF);

    uint8_t pkt[kPacketLen], reply[kPacketLen];
    buildPacket(pkt, CmdEncrypt, nonce, 8);
    if (!transfer(fd, pkt, reply))
        return false;
    if (deviceType)
        *deviceType = reply[11];

    const uint8_t on = 1;
    buildPacket(pkt, CmdPcDriver, &on, 1);
    transfer(fd, pkt, nullptr);   // advisory; the device works without it
    return true;
}

bool readFlash(int fd, int addr, int len, uint8_t *out)
{
    uint8_t pkt[kPacketLen], reply[kPacketLen];
    buildPacket(pkt, CmdReadFlash, nullptr, 0, addr, len);
    if (!transfer(fd, pkt, reply))
        return false;
    for (int i = 0; i < len && i < 10; ++i)
        out[i] = reply[5 + i];
    return true;
}

// Vendor gt(): bulk write, 10 bytes max per packet.
bool writeBytes(int fd, int addr, const uint8_t *data, int len)
{
    if (len < 1 || len > 10)
        return false;
    uint8_t pkt[kPacketLen];
    buildPacket(pkt, CmdWriteFlash, data, len, addr, len);
    return transfer(fd, pkt, nullptr);
}

bool writeValue(int fd, int addr, uint8_t value)
{
    const uint8_t payload[2] = { value, pairByte(value) };
    return writeBytes(fd, addr, payload, 2);
}

// Open the node and get a session going.
int openDevice(const QString &path, int *deviceType = nullptr)
{
    const int fd = ::open(path.toUtf8().constData(), O_RDWR | O_NONBLOCK);
    if (fd < 0)
        return -1;
    if (!handshake(fd, deviceType)) {
        ::close(fd);
        return -1;
    }
    return fd;
}

// The config channel lives on the interface exposing usage page 0xFF02 with
// report id 8. Match on the descriptor rather than the interface number so
// node renumbering cannot break discovery.
bool descriptorHasVendorCollection(const QString &sysPath)
{
    QFile f(sysPath + QStringLiteral("/device/report_descriptor"));
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray d = f.readAll();
    for (int i = 0; i + 4 < d.size(); ++i) {
        // 06 02 ff = Usage Page (Vendor 0xFF02), 09 xx, a1 01, 85 08 = Report ID 8
        if (static_cast<uint8_t>(d[i]) == 0x06
            && static_cast<uint8_t>(d[i+1]) == 0x02
            && static_cast<uint8_t>(d[i+2]) == 0xff) {
            for (int j = i; j + 1 < d.size() && j < i + 16; ++j) {
                if (static_cast<uint8_t>(d[j]) == 0x85
                    && static_cast<uint8_t>(d[j+1]) == kReportId)
                    return true;
            }
        }
    }
    return false;
}

} // namespace

uint8_t rateToCode(int hz)
{
    for (const auto &r : kRates)
        if (r.hz == hz) return r.code;
    return 0;
}

int codeToRate(uint8_t code)
{
    for (const auto &r : kRates)
        if (r.code == code) return r.hz;
    return 0;
}

bool dpiToRecord(int dpi, uint8_t out[kDpiRecordLen])
{
    if (dpi < kDpiStep || dpi > kDpiMax)
        return false;

    int val, ex;
    if (dpi <= kDpiMaxSimple) {
        if (dpi % kDpiStep)
            return false;
        val = dpi / kDpiStep - 1;
        ex  = 0;
    } else {
        // Second sensor range: stored halved, flagged with DPIex.
        if (dpi % (kDpiStep * 2))
            return false;
        val = (dpi / 2) / kDpiStep - 1;
        ex  = 17;
    }

    const int hi = val >> 8;
    out[0] = static_cast<uint8_t>(val & 0xFF);          // X low
    out[1] = static_cast<uint8_t>(val & 0xFF);          // Y low, equal unless split
    // Value high bits sit at 2-3 and 6-7, DPIex at bits 0 and 4.
    out[2] = static_cast<uint8_t>(((hi << 2) | (hi << 6) | ex | (ex << 4)) & 0xFF);
    out[3] = static_cast<uint8_t>((85 - ((out[0] + out[1] + out[2]) & 0xFF)) & 0xFF);
    return true;
}

int recordToDpi(const uint8_t rec[kDpiRecordLen])
{
    const uint8_t want =
        static_cast<uint8_t>((85 - ((rec[0] + rec[1] + rec[2]) & 0xFF)) & 0xFF);
    if (rec[3] != want)
        return 0;
    // Bit 0 of rec[2] is DPIex, i.e. the stored value is halved.
    const bool doubled = (rec[2] & 0x01) != 0;
    const int  hi      = (rec[2] >> 2) & 0x03;
    const int  dpi     = (((hi << 8) | rec[0]) + 1) * kDpiStep;
    return doubled ? dpi * 2 : dpi;
}

int snapDpi(int dpi)
{
    if (dpi < kDpiStep) return kDpiStep;
    if (dpi > kDpiMax)  return kDpiMax;
    const int step = (dpi > kDpiMaxSimple) ? kDpiStep * 2 : kDpiStep;
    return ((dpi + step / 2) / step) * step;
}

bool colorToRecord(uint32_t rgb, uint8_t out[4])
{
    out[0] = static_cast<uint8_t>((rgb >> 16) & 0xFF);
    out[1] = static_cast<uint8_t>((rgb >> 8) & 0xFF);
    out[2] = static_cast<uint8_t>(rgb & 0xFF);
    out[3] = static_cast<uint8_t>((85 - ((out[0] + out[1] + out[2]) & 0xFF)) & 0xFF);
    return true;
}

uint32_t recordToColor(const uint8_t rec[4])
{
    const uint8_t want =
        static_cast<uint8_t>((85 - ((rec[0] + rec[1] + rec[2]) & 0xFF)) & 0xFF);
    if (rec[3] != want)
        return 0xFFFFFFFFu;
    return (static_cast<uint32_t>(rec[0]) << 16)
         | (static_cast<uint32_t>(rec[1]) << 8)
         |  static_cast<uint32_t>(rec[2]);
}

// Vendor F6(). Deliberately not a formula: the table has gaps at 5 and 9.
uint8_t brightnessToRaw(int level)
{
    switch (level) {
    case 1:  return 16;
    case 5:  return 128;
    case 9:  return 230;
    case 10: return 255;
    case 2: case 3: case 4: case 6: case 7: case 8:
        return static_cast<uint8_t>(30 * (level - 1));
    default: return 128;
    }
}

// Vendor N6(), the exact inverse for every value brightnessToRaw emits.
int brightnessFromRaw(uint8_t raw)
{
    if (raw % 30 == 0)
        return raw / 30 + 1;
    switch (raw) {
    case 16:  return 1;
    case 128: return 5;
    case 230: return 9;
    case 255: return 10;
    default:  return 5;
    }
}

QString findDevice()
{
    struct udev *udev = udev_new();
    if (!udev)
        return {};

    QString match;
    struct udev_enumerate *e = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(e, "hidraw");
    udev_enumerate_scan_devices(e);

    struct udev_list_entry *entry;
    udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e)) {
        const char *sysPath = udev_list_entry_get_name(entry);
        struct udev_device *dev = udev_device_new_from_syspath(udev, sysPath);
        if (!dev)
            continue;

        struct udev_device *usb =
            udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
        const char *devnode = udev_device_get_devnode(dev);
        if (usb && devnode) {
            const char *vid = udev_device_get_sysattr_value(usb, "idVendor");
            const char *pid = udev_device_get_sysattr_value(usb, "idProduct");
            if (vid && pid
                && QString::fromUtf8(vid).toUInt(nullptr, 16) == kVid
                && isUltraPid(QString::fromUtf8(pid).toUShort(nullptr, 16))
                && descriptorHasVendorCollection(QString::fromUtf8(sysPath))) {
                match = QString::fromUtf8(devnode);
            }
        }
        udev_device_unref(dev);
        if (!match.isEmpty())
            break;
    }

    udev_enumerate_unref(e);
    udev_unref(udev);
    return match;
}

Settings readSettings(const QString &hidrawPath)
{
    Settings s;

    // Reads compete with 8 kHz motion traffic, so a miss says nothing about
    // whether the device is there. Reopen and retry the whole snapshot before
    // reporting failure -- the rate read in particular gates everything.
    for (int round = 0; round < 3 && !s.valid; ++round) {
        int devType = -1;
        const int fd = openDevice(hidrawPath, &devType);
        if (fd < 0)
            continue;
        s.deviceType = devType;

        uint8_t buf[10];
        // A missed read used to leave its field at 0, which looks exactly like
        // a real zero. The UI would then show that zero and Apply would write
        // it back, silently wiping the real setting. Track every read and throw
        // the whole snapshot away if any of them missed.
        bool allOk = true;
        auto readByte = [&](int addr, int &dest) {
            if (readFlash(fd, addr, 2, buf))
                dest = buf[0];
            else
                allOk = false;
        };
        int rateCode = 0, motion = 0, angle = 0, ripple = 0;
        readByte(OffReportRate, rateCode);
        readByte(OffLOD, s.lod);
        readByte(OffDebounceTime, s.debounceMs);
        readByte(OffMotionSync, motion);
        readByte(OffAngle, angle);
        readByte(OffRipple, ripple);
        readByte(OffMaxDpiStage, s.maxDpiStage);
        readByte(OffCurrentDPI, s.currentDpiStage);

        s.dpiStages.clear();
        s.dpiColors.clear();
        const int stages = qBound(0, s.maxDpiStage, kDpiMaxStages);
        for (int st = 0; st < stages; ++st) {
            uint8_t rec[kDpiRecordLen] = {0};
            const bool got = readFlash(fd, OffDPIValue + st * kDpiRecordLen,
                                       kDpiRecordLen, rec);
            if (!got)
                allOk = false;
            s.dpiStages.append(got ? recordToDpi(rec) : 0);

            uint8_t col[kDpiRecordLen] = {0};
            const bool gotCol = readFlash(fd, OffDPIColor + st * kDpiRecordLen,
                                          kDpiRecordLen, col);
            if (!gotCol)
                allOk = false;
            s.dpiColors.append(gotCol ? recordToColor(col) : 0xFFFFFFFFu);
        }
        s.dpiColors.resize(s.dpiStages.size());

        int fps = 0;
        readByte(OffSensorFPS20K, fps);
        s.fps20k = fps != 0;

        // One 8 byte read covers mode, brightness, speed and state, each as a
        // [v, 85-v] pair, matching the vendor's en(DPIEffectMode, 8).
        uint8_t fx[8] = {0};
        if (readFlash(fd, OffDPIEffectMode, 8, fx)) {
            s.lightMode  = (fx[6] != 0) ? fx[0] : LightOff;
            s.brightness = brightnessFromRaw(fx[2]);
            s.lightSpeed = qBound(kLightSpeedMin, int(fx[4]), kLightSpeedMax);
        } else {
            allOk = false;
        }

        s.reportRateHz = codeToRate(static_cast<uint8_t>(rateCode));
        s.motionSync = motion != 0;
        s.angleSnap  = angle != 0;
        s.ripple     = ripple != 0;
        s.valid = allOk && s.reportRateHz != 0;

        ::close(fd);
    }
    return s;
}

Battery readBattery(const QString &hidrawPath)
{
    Battery b;
    const int fd = openDevice(hidrawPath);
    if (fd < 0)
        return b;

    uint8_t pkt[kPacketLen], reply[kPacketLen];
    buildPacket(pkt, CmdBattery, nullptr, 0);
    if (transfer(fd, pkt, reply)) {
        b.charging = reply[6] == 1;
        // Some firmware reports a pre-scaled level in [10] instead of [5].
        b.percent = (reply[9] == 1) ? reply[10] : reply[5];
        b.millivolts = (reply[7] << 8) + reply[8];
        b.valid = true;
    }
    ::close(fd);
    return b;
}

bool applyWritable(const QString &hidrawPath, const Writable &w)
{
    // Validate before opening so a rejected value costs nothing.
    uint8_t rateCode = 0;
    if (w.reportRateHz != 0) {
        rateCode = rateToCode(w.reportRateHz);
        if (rateCode == 0)
            return false;
    }
    if (w.lod > 2 || w.debounceMs > 30)
        return false;
    if (w.lightMode > LightBreathing)
        return false;
    if (w.brightness > kBrightnessMax
        || (w.brightness >= 0 && w.brightness < kBrightnessMin))
        return false;
    if (w.lightSpeed > kLightSpeedMax
        || (w.lightSpeed >= 0 && w.lightSpeed < kLightSpeedMin))
        return false;

    const int fd = openDevice(hidrawPath);
    if (fd < 0)
        return false;

    bool ok = true;
    if (rateCode != 0)
        ok &= writeValue(fd, OffReportRate, rateCode);
    if (w.lod >= 0)
        ok &= writeValue(fd, OffLOD, static_cast<uint8_t>(w.lod));
    if (w.debounceMs >= 0)
        ok &= writeValue(fd, OffDebounceTime, static_cast<uint8_t>(w.debounceMs));
    if (w.angleSnap >= 0)
        ok &= writeValue(fd, OffAngle, w.angleSnap ? 1 : 0);
    if (w.ripple >= 0)
        ok &= writeValue(fd, OffRipple, w.ripple ? 1 : 0);
    if (w.motionSync >= 0)
        ok &= writeValue(fd, OffMotionSync, w.motionSync ? 1 : 0);
    if (w.fps20k >= 0)
        ok &= writeValue(fd, OffSensorFPS20K, w.fps20k ? 1 : 0);

    // Brightness and speed first, so switching an effect on already has the
    // values it is meant to run at.
    if (w.brightness >= 0)
        ok &= writeValue(fd, OffDPIEffectBrightness, brightnessToRaw(w.brightness));
    if (w.lightSpeed >= 0)
        ok &= writeValue(fd, OffDPIEffectSpeed, static_cast<uint8_t>(w.lightSpeed));

    if (w.lightMode == LightOff) {
        // Off leaves the mode byte alone, exactly as the vendor's Bb() does,
        // so the last effect is still there when it is switched back on.
        ok &= writeValue(fd, OffDPIEffectState, 0);
    } else if (w.lightMode > 0) {
        ok &= writeValue(fd, OffDPIEffectMode, static_cast<uint8_t>(w.lightMode));
        ok &= writeValue(fd, OffDPIEffectState, 1);
    }

    ::close(fd);
    return ok;
}

bool applyDpiColor(const QString &hidrawPath, int stage, uint32_t rgb)
{
    if (stage < 0 || stage >= kDpiMaxStages)
        return false;
    uint8_t rec[4];
    colorToRecord(rgb, rec);

    const int fd = openDevice(hidrawPath);
    if (fd < 0)
        return false;
    const bool ok = writeBytes(fd, OffDPIColor + stage * kDpiRecordLen, rec,
                               kDpiRecordLen);
    ::close(fd);
    return ok;
}

bool applyDpi(const QString &hidrawPath, const QList<DpiWrite> &writes,
              int currentStage)
{
    if (writes.isEmpty() && currentStage < 0)
        return true;
    if (currentStage >= kDpiMaxStages)
        return false;

    // Validate up front: a half-written stage table is worse than no write.
    for (const DpiWrite &w : writes) {
        uint8_t rec[kDpiRecordLen];
        if (w.stage < 0 || w.stage >= kDpiMaxStages || !dpiToRecord(w.dpi, rec))
            return false;
    }

    const int fd = openDevice(hidrawPath);
    if (fd < 0)
        return false;

    bool ok = true;
    for (const DpiWrite &w : writes) {
        uint8_t rec[kDpiRecordLen];
        dpiToRecord(w.dpi, rec);
        ok &= writeBytes(fd, OffDPIValue + w.stage * kDpiRecordLen, rec,
                         kDpiRecordLen);
    }
    if (ok && currentStage >= 0)
        ok &= writeValue(fd, OffCurrentDPI, static_cast<uint8_t>(currentStage));

    ::close(fd);
    return ok;
}

} // namespace ultra
