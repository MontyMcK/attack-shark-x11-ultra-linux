# attack shark x11 ultra on linux

bought an x11 ultra carbon, plugged it into linux, and found out that none of the
existing tools can talk to it. all four of the community projects out there target
the older x11 (`1d57:fa55` / `fa60`). the ultra is different silicon running a
completely different protocol, so it just sits there ignoring everything they send.

so i pulled the vendor's web driver apart and worked out the real protocol.
everything in [PROTOCOL.md](PROTOCOL.md) was checked against an actual mouse.
where the minified javascript and the hardware disagreed i went with the hardware,
and i wrote down which was which so you do not have to repeat it.

## the short version

the ultra does 2000, 4000 and 8000 Hz, and the old protocol cannot express any of
them. its report rate byte is `1000/hz`, which hits 1 at 1000 Hz and has nowhere
left to go. the ultra adds a second branch, `hz/125`, for everything above that:

| Hz | byte |
|---:|---:|
| 125 | 0x08 |
| 250 | 0x04 |
| 500 | 0x02 |
| 1000 | 0x01 |
| 2000 | 0x10 |
| 4000 | 0x20 |
| 8000 | 0x40 |

the bottom four are byte for byte identical to the old x11. the top three are new.
that one extra branch is the whole reason this needed its own driver.

worth knowing: the vendor's own encoder and decoder disagree here. the encoder
would write `0x80` for 8000 Hz, the decoder reads `0x40` as 8000 Hz. i read offset
0 off a mouse that an external tool measured at 7989 Hz and got `0x40`, so the
decoder is the one telling the truth. reading the bundle alone points you the
wrong way.

## two usb ids, one mouse

this one cost me an evening. on the dongle the mouse enumerates as `3554:f517`.
on the cable it is `3554:f515`. same cid, same mid, same paired address, same
protocol, same everything. if you match only `f517` then your tool works great
right up until you plug the mouse in to charge, at which point it vanishes and
you go looking for a bug that is not there.

match both, or skip the product id entirely and match on the vendor usage page
`0xFF02` with report id 8.

## what's in here

| | |
|---|---|
| `PROTOCOL.md` | the whole protocol writeup. start here |
| `x11ultra.py` | python transport. handshake, flash read/write, report rate, dpi codec |
| `probe.py` | read only. dumps what your mouse currently holds, touches nothing |
| `udev/` | rule so you do not need root |
| `qt-gui/` | ultra support for the existing qt gui, see below |

run the probe first:

```sh
sudo cp udev/70-attack-shark-x11-ultra.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
python3 probe.py
```

you should get something like:

```
handshake OK  cid=124 mid=11 type=5 (wireless 8k)
```

## what works

- report rate, all seven values including 8000 Hz
- angle snapping, ripple control, motion sync, lift off distance, debounce
- battery level and charging state
- raw config flash read and write if you want to poke at it yourself
- dpi, read and write, 50 up to 60000. the codec round trips all 900 valid
  values and reproduces my mouse's six existing stage records byte for byte.
  writes are verified on hardware too: stage 0 moved 800 to 850, landed as
  exactly the bytes the encoder predicts, read back as 850, and writing 800
  again restored the original record byte for byte

## what doesn't

- lighting effects. i can read the per stage dpi colours, they are plain RGB with
  a checksum at `DPIColor`, and they match the manual's table. but the effect
  modes at `Light` and the `DPIEffect*` offsets i have not decoded, so i left them
  alone rather than write bytes i do not understand into flash
- macros and key remapping. the offsets are in the eeprom map, the payload format
  is not worked out
- sleep and deep sleep timers. same story, `SleepTime` is in the map but i have
  not decoded what it wants

## gotchas that will waste your time

**reads fail unless you retry.** the radio is busy carrying 8 kHz of motion data
and a single read mostly gets you nothing. resend the same packet up to 5 times
with a ~250 ms window each and only accept a reply whose first 3 bytes echo yours
(5 bytes for flash reads). with retries every read became 100% reliable for me.
without them most of them returned nothing at all and it looks like the device is
unsupported when it is just busy.

**DeviceOnLine lies.** command 3 reports `online=0` on a mouse that is very much
alive and pushing 34k motion events in 20 seconds. do not gate anything on it. the
paired address it hands back is correct, so it is probably a dongle link field
that does not mean what the name says.

**the mouse sleeps.** when it is idle the dongle keeps answering the handshake but
flash reads time out. move the mouse and it comes back. do not treat that as
"device not supported", just retry.

**settings are stored twice.** single byte values live as `[v, 85-v]`. write both
bytes. read them both and you get a free sanity check.

## qt gui

there is already a good gui for the plain x11 by
[iago-fragnan](https://github.com/iago-fragnan/attack-shark-x11-linux). rather
than write a second one i added an ultra path to it.

`qt-gui/ultra.cpp` and `ultra.h` are self contained and mine. the patch is the
glue that hooks them into his app.

```sh
git clone https://github.com/iago-fragnan/attack-shark-x11-linux.git
cd attack-shark-x11-linux
cp /path/to/qt-gui/ultra.{h,cpp} .
git apply /path/to/qt-gui/ultra-support.patch
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
./build/attackshark-x11
```

it detects the ultra, swaps the polling rate list for the full seven entries,
adds a DPI panel where you pick a stage, set its value and mark which stage the
mouse is actually using, and puts the active DPI in the tile the LED readout was
wasting. his x11 path is completely untouched when an ultra is not plugged in.
it also skips the pkexec prompt on the ultra, which does not need root once the
udev rule is in.

apply only rewrites the stages you actually changed, and every value is checked
before the device is opened, so a bad entry cannot leave you with half a stage
table written.

the colour dropdown and the sleep sliders are greyed out rather than left
looking live. they are in the eeprom map but the payloads are not decoded, and a
control that silently does nothing when you hit apply is worse than one that
tells you it cannot.

i have not sent this upstream yet.

## the vendor json

the dpi step size is not in the javascript, it comes from two json files the web
driver fetches at runtime:

- `https://controlhub.top/AttackShark/cfg.json`
- `https://controlhub.top/AttackShark/sensor.json`

they are the vendor's files so i am not mirroring them here. the values you
actually need are written up in PROTOCOL.md. for reference my unit is cid 124 /
mid 11, which maps to sensor 3950 with 5 keys.

## warning

this writes to the config flash on your mouse. i have not bricked mine, the
writes are single settings with a checksum and they land immediately with no
commit step, and a full 256 byte compare before and after a write showed zero
unintended changes. but back up first:

```python
from x11ultra import X11Ultra
with X11Ultra() as m:
    print(m.read_block(0, 256))
```

it is your mouse. i am not responsible for it.

## not affiliated

independent work, no connection to attack shark, guangzhou shijunxingcheng, or
pixart. no vendor source code in here. the protocol was worked out from
observable behaviour on hardware i own. trademarks belong to whoever owns them
and are only used to say which mouse this is for.
