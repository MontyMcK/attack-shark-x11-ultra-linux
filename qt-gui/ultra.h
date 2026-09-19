#ifndef ULTRA_H
#define ULTRA_H

#include <QString>
#include <QList>
#include <QPair>
#include <cstdint>

// Attack Shark X11 Ultra (Compx 3554:f517).
//
// A different device from the plain X11 (Xenta 0x1d57): different silicon and a
// different protocol. The X11 talks libusb control transfers; the Ultra talks
// hidraw report ID 8 on its vendor usage page 0xFF02, and unlike the X11 it can
// do 2000/4000/8000 Hz. See PROTOCOL-X11-ULTRA.md.

namespace ultra {

constexpr uint16_t kVid = 0x3554;

// The Ultra enumerates under different product ids depending on how it is
// connected: 0xf517 on the 2.4 GHz dongle, 0xf515 on the cable. Same silicon,
// same cid/mid, same vendor collection -- only the USB product id differs. Match
// any of them and let the descriptor check confirm it is really the Ultra.
constexpr uint16_t kPids[] = { 0xf517, 0xf515 };

inline bool isUltraPid(uint16_t pid)
{
    for (uint16_t p : kPids)
        if (p == pid) return true;
    return false;
}

// Config-flash offsets (vendor `ae` map).
enum Offset : uint16_t {
    OffReportRate   = 0,
    OffMaxDpiStage  = 2,
    OffCurrentDPI   = 4,
    OffLOD          = 10,
    OffDPIValue     = 12,
    OffDPIColor     = 44,
    OffDebounceTime = 169,
    OffMotionSync   = 171,
    OffSleepTime    = 173,
    OffAngle        = 175,
    OffRipple       = 177,
};

// Every rate the Ultra supports. The first four match the plain X11's table
// byte for byte; the rest are what the X11 protocol cannot express at all.
struct Rate { int hz; uint8_t code; };
static const Rate kRates[] = {
    {125, 0x08}, {250, 0x04}, {500, 0x02}, {1000, 0x01},
    {2000, 0x10}, {4000, 0x20}, {8000, 0x40},
};
constexpr int kRateCount = sizeof(kRates) / sizeof(kRates[0]);

uint8_t rateToCode(int hz);   // 0 if unsupported
int     codeToRate(uint8_t);  // 0 if unrecognised

// DPI. One 4-byte record per stage at OffDPIValue + stage*kDpiRecordLen, stage
// count at OffMaxDpiStage. Above kDpiMaxSimple the sensor switches range: the
// value is stored halved and flagged with DPIex, so the usable step doubles.
constexpr int kDpiStep      = 50;
constexpr int kDpiMaxSimple = 30000;
constexpr int kDpiMax       = 60000;
constexpr int kDpiRecordLen = 4;
constexpr int kDpiMaxStages = 8;

// False if dpi is out of range or off the step boundary for its range.
bool dpiToRecord(int dpi, uint8_t out[kDpiRecordLen]);
// 0 if the record's checksum does not hold.
int  recordToDpi(const uint8_t rec[kDpiRecordLen]);
// Nearest DPI the device can actually store.
int  snapDpi(int dpi);

// Path of the hidraw node carrying the vendor collection, or empty.
QString findDevice();

// Settings snapshot. `valid` is false if the device could not be read.
struct Settings {
    bool valid = false;
    int  reportRateHz = 0;
    int  lod = 0;
    int  debounceMs = 0;
    bool motionSync = false;
    bool angleSnap = false;
    bool ripple = false;
    int  maxDpiStage = 0;
    int  currentDpiStage = 0;
    QList<int> dpiStages;   // decoded DPI per stage, 0 where the read failed
};

Settings readSettings(const QString &hidrawPath);

struct Battery {
    bool valid = false;
    bool charging = false;
    int  percent = 0;
    int  millivolts = 0;
};

Battery readBattery(const QString &hidrawPath);

// Each returns true on success. Writing is immediate; there is no commit step.
bool applySettings(const QString &hidrawPath, int reportRateHz, bool angleSnap,
                   bool rippleControl, bool motionSync, int debounceMs, int lod);

struct DpiWrite { int stage; int dpi; };

// Writes each stage then, if currentStage >= 0, makes that stage the active
// one. Every value is validated before the device is opened so a bad entry
// cannot leave half the stages rewritten.
bool applyDpi(const QString &hidrawPath, const QList<DpiWrite> &writes,
              int currentStage);

} // namespace ultra

#endif // ULTRA_H
