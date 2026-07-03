# RD-03D Radar Desktop (Qt6)

Native replacement for the p5.js web GUI. Reads the newline-delimited JSON telemetry emitted
by `ESP32_RD03D.ino` over USB serial (115200 baud) and renders it with `QGraphicsView`.

## Wire protocol

Two JSON message types, one per line:

```jsonc
// Sent once at boot, and again on the "CFG" serial command
{"type":"cfg","tile":1000,"gw":4,"gh":4,"zones":["A1","B1","C1","D1","A2","B2","C2","D2","A3","B3","C3","D3","B4","C4"]}

// Sent once per loop() iteration (~20 Hz)
{"type":"trk","seq":123,"t":[{"id":1,"x":120,"y":1830,"v":12.3,"z":5}],"zocc":10376}
```

`t[].z` is an index into `cfg.zones[]` (255 = no zone). `zocc` is a bitmask over the same
array (bit *i* set = `zones[i]` occupied). Grid cells whose name doesn't appear in `zones[]`
(e.g. the far corners on a 4x4 grid) are rendered grayed-out/excluded automatically.

## Build

Requires Qt 6 (Core, Gui, Widgets, SerialPort) and a C++17 compiler.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/RadarDesktop        # Linux/macOS
./build/RadarDesktop.exe    # Windows
```

On Linux, add your user to the `dialout` group (or equivalent) to access `/dev/ttyUSB*`
without root.

## Firmware side

Flash the updated `ESP32_RD03D/ESP32_RD03D.ino` (requires the `ArduinoJson` library, v6.21+,
installable via Arduino IDE Library Manager). `DEBUG_RAW_TARGETS` now defaults to `false` so
the USB serial line carries pure JSON; toggle it back on with the `DEBUG` command only for
manual troubleshooting in a plain serial monitor — the desktop app ignores any line that
doesn't parse as JSON, so the two modes can coexist without crashing the parser.
