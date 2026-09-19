# Attack Shark X11 Ultra (`3554:f517` / `3554:f515`) HID protocol

Reverse-engineered from the vendor WebHID driver bundle at
`https://controlhub.top/AttackShark/assets/index-BLQzAg-a.js` (834 KB, minified).
Transport, handshake, flash **reads and writes all verified on real hardware**
2026-09-12 against a live X11 Ultra Carbon.

**This is a different protocol from the plain X11 (`1d57:fa55/fa60`).** All four
existing community projects target `1d57` and cannot talk to this device.

## Transport

| | |
|---|---|
| Interface | vendor usage page `0xFF02`, **report ID 8**, 16 bytes, Input+Output |
| Node here | `/dev/hidraw1` (interface 1) |
| Write | `write(fd, bytes([8]) + packet16)` |
| Read | input report; hidraw prepends the report ID, so vendor `Le[i]` == `data[i+1]` |

### Product id depends on the connection

**Verified 2026-09-20.** The same physical mouse enumerates under two product ids:

| PID | Connection | Handshake `type` |
|---|---|---|
| `f517` | 2.4 GHz dongle | 5 (wireless 8k) |
| `f515` | USB cable | 3 (wired 8k) |

`cid`/`mid` (124/11), the paired address, the vendor collection and the whole
protocol are identical across both. Match **every** known PID, or match on VID
plus the `0xFF02`/report-ID-8 descriptor and ignore the PID entirely: keying on
`f517` alone makes the mouse vanish the moment it is plugged in by cable.

### Packet layout (16 bytes)

```
[0]     command
[1]     0
[2..3]  address, big-endian (flash commands only)
[4]     length + type_offset       # type_offset: mouse=0, keyboard=128
[5..14] payload
[15]    checksum
```

### Checksum

```python
at   = (85 - (sum(packet[0:15]) & 0xFF)) & 0xFF
p[15] = (at - 8) & 0xFF      # 8 == report ID (Wt)
```

## Commands (`Ne`)

| # | Name | # | Name |
|---:|---|---:|---|
| 1 | EncryptionData (handshake) | 14 | GetCurrentConfig |
| 2 | PCDriverStatus | 15 | SetCurrentConfig |
| 3 | DeviceOnLine | 18 | ReadVersionID |
| 4 | BatteryLevel | 22/23 | Set/GetLongRangeMode |
| 5 | DongleEnterPair | 29 | GetDongleVersion |
| 6 | GetPairState | 20/21 | Set/Get4KDongleRGB |
| 7 | WriteFlashData | 24/25 | Set/GetDongleRGBBarMode |
| 8 | ReadFlashData | 9 | ClearSetting |

## Handshake (required before anything else)

Send cmd 1 with 8-byte payload: 4 random bytes then 4 zeros.

Response (observed): `08 01 00 00 00 08 4b b3 7c de 7c 0b 05 00 00 00 60`

```
Le[1]  = 0      status OK
Le[9]  = cid    (0x7c here)
Le[10] = mid    (0x0b here)
Le[11] = type   (0x05 here)
```

### Device type -> max report rate

| type | meaning | max |
|---:|---|---|
| 0 | wireless | 1000 |
| 1 | wireless | 4000 |
| 2 | wired | 1000 |
| 3 | wired | 8000 |
| 4 | wireless | 2000 |
| **5** | **wireless (this unit)** | **8000** |
| 6 | charging base | n/a |

## Flash read / write

Read (`en(addr,len)`), chunked 10 bytes at a time by `Nr(start,end)`:
```
[0]=8  [2..3]=addr  [4]=len+type_offset
```
Write (`gt(addr,data)`), also 10-byte chunks:
```
[0]=7  [2..3]=addr  [4]=chunklen+type_offset  [5..14]=data
```
Response echoes bytes 0..4; data lands at `Le[5..]`, with `addr=(Le[2]<<8)+Le[3]`
and `len=Le[4]&15`.

## Mouse EEPROM map (`ae`)

| Offset | Field | Offset | Field |
|---:|---|---:|---|
| 0 | ReportRate | 171 | MotionSync |
| 2 | maxDpiStage | 173 | SleepTime |
| 4 | CurrentDPI | 175 | Angle (snap) |
| 8 | KeyOperation | 177 | Ripple |
| 10 | LOD | 181/183 | PerformanceState/Performance |
| 12 | DPIValue | 185 | SensorMode |
| 44 | DPIColor | 189/191 | AngleTune/State |
| 76 | DPIEffectMode | 225 | SensorFPS20K |
| 78 | DPIEffectBrightness | 227 | WheelDebounceTime |
| 80 | DPIEffectSpeed | 229 | DebounceReleaseTime |
| 82 | DPIEffectState | 233 | FlywheelState |
| 96 | KeyFunction | 239..249 | L/R Trigger, FastTrigger, TactileFeedback |
| 160 | Light | 256 | ShortcutKey |
| 169 | DebounceTime | 768 | Macro |
| 6912 | Sensor3955DPI | 6987 | EndEeprom |

## Report-rate encoding: the reason this device needs a new driver

**Verified against hardware 2026-09-12.** Offset 0 read back `0x40` on a unit
independently measured at 7989 Hz, so `0x40` == 8000 Hz.

```
code = hz > 1000 ?  hz / 125  :  1000 / hz
```

| Hz | code |
|---:|---:|
| 125 | 0x08 |
| 250 | 0x04 |
| 500 | 0x02 |
| 1000 | 0x01 |
| 2000 | 0x10 |
| 4000 | 0x20 |
| 8000 | 0x40 |

The `<=1000` branch is byte-identical to the `1d57` X11 protocol; the `>1000`
branch is new and is what makes 2K/4K/8K reachable. Existing projects implement
only the low branch, so they top out at 1000 Hz by construction.

**Do not trust the vendor's encoder.** Its two codecs disagree on the high
branch: `ReportRate_To_FlashData(hz) = hz/1000*16` would emit `0x80` for 8000,
while `FlashData_To_ReportRate(c) = c/16*2000` reads `0x40` as 8000. Hardware
says the **decoder** is right. Reading the bundle alone points the wrong way.
This was only settled by reading offset 0 off a real mouse.

## Writing (verified)

`write_value(addr, v)` = cmd 7, `[4]=2+type_offset`, payload `[v, 85-v]`
(the vendor's `Qe()`). Verified on offset 175 (angle snap): same-value write
ACKs and is a true no-op; a real toggle 0->1 takes effect and reads back with a
valid redundancy pair; restoring returns the original. A full 256-byte compare
before and after showed **zero** unintended changes.

Writes land immediately, there is no separate commit/save command.

Back up before writing: `read_block(0, 256)` captures the whole settings region.

## DPI gear colours (verified)

4 bytes per gear at `DPIColor + stage*4`, `[R, G, B, checksum]`, with the same
checksum as a DPI record (`85 - (R+G+B)`). Note this is **not** the `[v, 85-v]`
pair layout used by single byte settings.

Read live off this unit, and they line up exactly with the swatches the vendor's
own page shows for the same gears:

| Gear | Bytes | Colour |
|---:|---|---|
| 1 | `ff 00 00 56` | red |
| 2 | `00 ff 00 56` | green |
| 3 | `00 00 ff 56` | blue |
| 4 | `ff ff 00 57` | yellow |
| 5 | `00 ff ff 57` | cyan |
| 6 | `ff 00 ff 57` | magenta |

## Key remap table (partially decoded)

4 bytes per key at `KeyFunction` (96), `[type, mask, 00, checksum]`, checksum
again `85 - sum`. This unit reads `01 01 00 53`, `01 02 00 52`, `01 04 00 50`,
`01 08 00 4c`: type 1 is "mouse button" and the mask is the button bit, so 1, 2,
4, 8 are left, right, middle, back. The other types (keyboard, macro,
multimedia, shortcut) are not mapped yet.

## Value storage: redundancy pairs

Single-byte settings are stored as `[value, 85-value]`, matching the vendor's
`Qe(addr,val)` writer (`n[5]=val; n[6]=85-val`). Confirmed on every field read:

| Field | Offset | Raw | Meaning |
|---|---:|---|---|
| ReportRate | 0 | `40 15` | 8000 Hz |
| maxDpiStage | 2 | `06 4f` | 6 stages (matches the manual) |
| CurrentDPI | 4 | `00 55` | stage 0 (1200, red) |
| LOD | 10 | `01 54` | 1 |
| DebounceTime | 169 | `00 55` | 0 ms |
| MotionSync | 171 | `01 54` | on |
| Angle (snap) | 175 | `00 55` | off |

A writer should emit both bytes; a reader can use the pair to validate.

## Transport reliability

Reads are unreliable single-shot and **must be retried**, the radio is busy
carrying 8 kHz motion data. Port the vendor's `ul()` semantics: resend the
packet up to 5 times, each with a ~200-250 ms window, and accept a reply only
when the first 3 bytes echo (5 bytes for ReadFlashData). With retries in place
all reads became 100% deterministic across repeated runs; without them most
returned nothing.

## Vendor device/sensor database

The web driver fetches two JSON files from its own directory. They are the
vendor's files so they are not mirrored in this repo, but everything you need
out of them is below:

- `https://controlhub.top/AttackShark/cfg.json`: device table keyed by
  `cid`/`mid` (from the handshake), giving sensor type, key count, and feature
  flags. **This unit: cid 124 / mid 11 -> sensor `3950`, 5 keys.**
- `https://controlhub.top/AttackShark/sensor.json`: per-sensor DPI ranges.
  `3950`: range[0] 50..30000 step 50 DPIex 0; range[1] 30100..60000 step 100
  DPIex 17.

Without these the DPI encoding cannot be derived, the step is not in the JS.

## DPI (verified on hardware, both directions)

4 bytes per stage at `DPIValue + stage*4` (sensor `3955` instead uses 6 bytes
at `Sensor3955DPI`; not applicable here). Stage count is at `maxDpiStage`.

```
val   = dpi/50 - 1
r[0]  = val & 0xFF          # X low
r[1]  = val & 0xFF          # Y low (equal unless X/Y are split)
r[2]  = (hi<<2)|(hi<<6) | DPIex | DPIex<<4     # hi = val>>8
r[3]  = 85 - (r[0]+r[1]+r[2])                  # same checksum as elsewhere
```

`r[2]` packs the value's high bits at 2-3 and 6-7, and **DPIex at bits 0 and
4**, so read the range flag from bit 0, not the low nibble. DPIex 17 marks the
>30000 range, where the value is stored halved (`val = (dpi/2)/50 - 1`).

Round-trips cleanly across all 900 valid values (600 low + 300 high). Encoding
each of this unit's six stages reproduces the device's own bytes exactly, 42000
included: `a3 a3 55 ba`.

**Writes verified 2026-09-20.** Stage 0 written 800 -> 850 landed as
`10 10 00 35`, exactly what the encoder produces, and read back as 850. Writing
800 again restored `0f 0f 00 37` byte for byte. Use the 4-byte bulk write
(vendor `gt()`, cmd 7 with `[4]=4`), not the 2-byte `[v, 85-v]` pair writer:
a DPI record is a single 4-byte value with its own checksum at `r[3]`, not a
redundancy pair.

Validate before writing. A DPI that is not on its range's step boundary has no
valid encoding, and a half-written stage table is worse than no write at all.

### This unit's stages, read live

| Stage | Record | DPI | Colour |
|---:|---|---:|---|
| 0 | `0f 0f 00 37` | 800 | red |
| 1 | `1f 1f 00 17` | 1600 | green |
| 2 | `3f 3f 00 d7` | 3200 | blue |
| 3 | `6f 6f 00 77` | 5600 | yellow |
| 4 | `9f 9f 00 17` | 8000 | cyan |
| 5 | `a3 a3 55 ba` | 42000 | magenta |

The colours match the manual's LED table exactly. Stages 2-5 match the manual
too; **stages 0 and 1 are 800/1600 where the manual says 1200/2400**, so those
two were evidently customised on this unit.

DPI colours live at `DPIColor + stage*4` as `[R, G, B, checksum]`.

## Full vendor API surface

The web driver exposes 168 entries, 49 of them mouse setters: DPI (value, XY
split, colour, stage count, current), lighting (mode, colour, brightness,
speed, off-time, power save, moving-off), key remap, macros, shortcut keys,
multimedia, flywheel (state, speeds, times), button triggers (point, fast
trigger, tactile feedback), dynamic sensitivity, virtual centre, sensor mode,
angle tune, FPS20K, wheel/release debounce, performance state, plus dongle and
charging-base commands. Implemented so far: report rate, angle snap, ripple,
motion sync, LOD, debounce, battery, DPI codec.

## Open items

- **`DeviceOnLine` (cmd 3) reports `online=0` on a demonstrably live mouse**
  (34k motion events in 20 s). Do not gate anything on it. It is probably a
  dongle-link field with different semantics than the name implies; the
  paired address it returns (`7e:8c:74`) is populated and correct.
- `PCDriverStatus` (cmd 2) is sent before reads here, mirroring the vendor
  flow; whether it is strictly required is untested.
