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
