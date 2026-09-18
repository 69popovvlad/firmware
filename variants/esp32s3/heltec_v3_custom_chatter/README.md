# Heltec V3 Custom Chatter

This build adds a two-register 74HC165 keyboard to the stock Heltec V3
Meshtastic configuration.

## Hardware

Schematics and PCB files:
[heltec-v3-chatter-keyboard](https://github.com/69popovvlad/heltec-v3-chatter-keyboard).

## Connections

| Signal | Heltec V3 GPIO |
| --- | ---: |
| 74HC165 PL / LOAD | 33 |
| 74HC165 CLK | 47 |
| 74HC165 DATA | 34 |
| VCC | 3V3 |
| GND | GND |

Both registers share LOAD and CLK. The serial output of the register closest
to the Heltec connects to DATA. The other register feeds that register's serial
input.

## Build

If VS Code or the terminal was started from Git Bash, clear its MSYS marker
before using the ESP-IDF toolchain:

```powershell
Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e heltec-v3-custom-chatter
```

The PlatformIO Build button also uses `heltec-v3-custom-chatter` by default.
If the IDE still reports that MSYS/MinGW is unsupported, close VS Code and
start it from the Windows Start menu or a regular PowerShell window.

## Upload

List serial ports:

```powershell
Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device list
```

Upload, replacing `COM3` with the detected port:

```powershell
Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e heltec-v3-custom-chatter -t upload --upload-port COM3
```

Open the serial monitor:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -b 115200 --port COM3
```

The build also creates a factory image at
`.pio/build/heltec-v3-custom-chatter/firmware-heltec-v3-custom-chatter-*.factory.bin`.

The debug log prints `SR1` and `SR2` in hexadecimal when a key changes. At
idle, both should be `0xff`. If the two bytes are reversed, uncomment
`CUSTOM_CHATTER_SWAP_SHIFT_REGISTERS` in the custom variant's `platformio.ini`
and rebuild.

If a bit stays `0` in the idle log line, that switch (or its solder joint) is
shorted. The scanner ignores a permanently low bit, so the rest of the keyboard
keeps working, but that key itself will never fire again until the hardware is
fixed.

## Scanning

The registers are sampled every 25 ms and a reading has to repeat twice before
it counts, which debounces the switches. Only bits that went from released to
pressed between two stable samples produce an event, so holding one key does not
mask the next one and a stuck switch cannot swallow every later press.

After boot, and after any reading where all sixteen inputs look pressed at once
(a floating `DATA` line), the scanner stays quiet until it sees one clean
"nothing pressed" sample. That stops garbage read while the 74HC165s are still
powering up from driving the region and frequency pickers on its own.

## Build fails with "No module named 'SCons.Tool.FortranCommon'"

Not a problem with this variant. `platform-espressif32` declares a `tool-scons`
package that resolves to `pioarduino/registry .../scons-4.8.1.zip`, which is a
1 KB metadata-only stub - unpacking it leaves `packages/tool-scons` holding
nothing but `package.json` and `tools.json`. PlatformIO's own SCons
(`scons-local-4.11.1`) installs into that same directory, so whichever install
runs last wins. When the platform's stub install fires while a build is already
running, SCons loses its own modules mid-flight.

Fix: point the platform at the same SCons the core uses. In every
`~/.platformio/platforms/espressif32*/platform.json`, replace the `tool-scons`
entry with

```json
"tool-scons": {
  "type": "tool",
  "optional": true,
  "owner": "platformio",
  "package-version": "4.41101.0",
  "version": "https://github.com/pioarduino/scons/releases/download/4.11.1/scons-local-4.11.1.tar.gz"
}
```

then delete `~/.platformio/packages/tool-scons` so the real archive is fetched
once. Both specs now resolve to the same installed package and the second
install is a no-op instead of a wipe. The edit lives in a downloaded platform
package, so redo it if the platform is reinstalled or its version is bumped.

## Mapping

Physical `D0` becomes bit 7 and physical `D7` becomes bit 0 because the
74HC165 chain is read with `LSBFIRST`.

| Register input | Switch | Action |
| --- | --- | --- |
| SR2 D0 | SW1 | key 1 |
| SR2 D1 | SW2 | key 2 |
| SR2 D2 | SW3 | key 3 |
| SR2 D3 | SW4 | key 4 |
| SR2 D4 | SW5 | key 5 |
| SR2 D5 | SW6 | backspace / tab |
| SR2 D6 | SW7 | key 9 |
| SR2 D7 | SW8 | key 8 |
| SR1 D0 | SW16 | select / enter |
| SR1 D1 | SW15 | right |
| SR1 D2 | SW14 | left |
| SR1 D3 | SW13 | cancel |
| SR1 D4 | SW9 | key 7 |
| SR1 D5 | SW10 | key 6 |
| SR1 D6 | SW11 | key 0 / space |
| SR1 D7 | SW12 | shift |

## Modifier chords

`SHIFT` (SW12) does two different jobs, and which one depends on whether it is
tapped or held:

- **Tapped**, it cycles the case level for text entry.
- **Held** while another key is pressed, it turns that key into a chord. The
  case level is restored afterwards, so a chord never changes the case.

| Chord | Action |
| --- | --- |
| `SHIFT` + `◄` | up (scroll node lists and message history) |
| `SHIFT` + `►` | down |
| `SHIFT` + `Backspace` | switch destination (tab) |
| `SHIFT` + `0` | cycle input language |

The board has no dedicated up/down keys. Option pickers and the node picker
accept `◄` / `►` as up/down on their own, but node lists and the message
history in `Screen::handleInputEvent` scroll on `UP`/`DOWN` only, which is what
the arrow chords are for.

## Text input

Text entry is T9 style: tap a key repeatedly to cycle through its characters.
`SHIFT` (SW12) cycles the level, shown as a badge in the bottom right corner of
the compose screen:

| Badge | Level |
| --- | --- |
| `a` / `а` | lower case |
| `A` / `А` | upper case |
| `#` | digits |

### Language

Hold `SHIFT` (SW12) and tap the `0 / space` key (SW11) to switch the input
language between latin and cyrillic. The badge changes to a cyrillic letter
while cyrillic is active. The chord does not type a space and leaves the current
shift level alone.

Backspace deletes at every case level. It used to send tab on the upper case
level, which left no way to delete while typing capitals; tab moved to the
`SHIFT` + `Backspace` chord instead.

Cyrillic layout, four taps per key:

| Key | Characters |
| --- | --- |
| 1 | `.` `,` `?` `ё` (`!` `+` `-` `Ё` shifted) |
| 2 | а б в г |
| 3 | д е ж з |
| 4 | и й к л |
| 5 | м н о п |
| 6 | р с т у |
| 7 | ф х ц ч |
| 8 | ш щ ъ ы |
| 9 | ь э ю я |
| 0 | space |

The layout lives in `CyrillicKeyMap` in `src/input/SerialKeyboard.cpp` and is
written as Unicode code points, so it can be edited the same way as the latin
`KeyMap` next to it. An entry of `0` falls back to the latin table, which is how
punctuation and space stay in place.

Messages are composed and sent as UTF-8, so other Meshtastic nodes and the phone
apps read them normally. Cyrillic is never passed through `InputEvent::kbchar`,
because several byte values in that range are reserved commands - `0xD0 0x90`
(`А`) would otherwise be read as "reboot".

### Display

`-D OLED_RU` in the variant's `platformio.ini` selects the `ArialMT_Plain_*_RU`
fonts, which carry cyrillic glyphs. This covers both typed text and messages
received from other nodes; latin ASCII is unaffected. Drop the flag to go back
to latin-only rendering.
