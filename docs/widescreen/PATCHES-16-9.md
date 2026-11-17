# Star Wars 16:9 — what was changed

Built from Tim Cottrill's C port of Atari's Star Wars
(Star-Wars-Full-Disassembly-and-C-Port), branch `widescreen-16-9`. Unlike a
stretched picture, the field of view itself is widened: stars, lasers and 3D
objects use the full 16:9 width, and the four X-wing guns sit at the edges of
the screen.

Build: `cmd /c c_src\build_win_bt.bat` (Visual Studio 2022 Build Tools).
The game reads `sw_win.ini` (`rom_dir=roms`) and your MAME set
`roms\starwars.zip` (**not included — supply your own**).

## 1. 3D object limits — `c_src\gen\sw_defs.h`

| Constant | Original (4:3) | 16:9 | Role |
|---|---|---|---|
| `VGEDGR` / `VGEDGL` | ±510 (`0x01FE`/`0xFE02`) | ±680 (`0x02A8`/`0xFD58`) | X clamp of 3D objects |
| `VGLIMR` / `VGLIML` | ±480 (`0x01E0`/`0xFE20`) | ±730 (`0x02DA`/`0xFD26`) | laser origins (off screen, at the guns) |
| `VGHUDR` / `VGHUDL` | — (new) | ±650 (`0x028A`/`0xFD76`) | HUD anchors (score, wave), **on** screen |

## 2. Star visibility cone — `c_src\gen\wsstar.c`

The two 45° width tests (`|Y| <= X`) are widened by 16:9 ÷ 4:3 = 4/3: at
labels `L7DF3` and `L7EEE`, `rd16(M_XPS)` becomes `(rd16(M_XPS) * 4) / 3`.
Height tests are unchanged.

## 3. HUD — `c_src\gen\wsmain.c` (`vwmes`, `vwmtwz`)

The four X positions of the score (left), the bonus shown, the wave number and
the tower count (right) use `VGHUDL`/`VGHUDR` instead of `VGLIML`/`VGLIMR`
(otherwise the score leaves the screen on the left).

## 4. Picture geometry — `c_src\platform\windows\plat_win.c`

- `PIC_HW = PIC_HH * 16/9` (wider picture space);
- `AVG_HWX` = AVG half-width × (16/9)/(4/3) ≈ ±683 visible units;
  `xform_point` and `clip_seg` clip to `AVG_XMIN/AVG_XMAX`;
- `DESIGN_W = 1280` (instead of 960, which pillarboxed); default window 1280×720.

## 5. Gun positions — vector ROM, in memory — `c_src\app.cpp`

The four cockpit guns are `VOFF` moves in the vector ROM. Their X words
(13-bit, big-endian) are set when the ROMs load; **your zip is never modified**:

| Address | Shape | Original | 16:9 |
|---|---|---|---|
| `0x341C` | VGSTTR (right, middle) | +480 | **+650** |
| `0x34B6` | VGSTBR (right, bottom) | +480 | **+650** |
| `0x35CE` | VGSTTL (left, middle) | −480 | **−650** |
| `0x3668` | VGSTBL (left, bottom) | −480 | **−650** |

±650 is symmetric, keeps the gun bodies visible and touches the edges with the
cursor centred. Note: the hood (and the guns) move opposite to the stick
(`vwplan`, `SI_RSX/RSY`), so symmetry must be measured with the cursor really
centred. The game auto-calibrates its potentiometers: a test script must sweep
the range before settling at the centre (see `c_src\tests\scenarios\centered.txt`).

The ROM set is read straight from `starwars.zip`: chips are found by CRC and
assembled exactly as `disasm\image.py` does (checked byte for byte). The
`.bin` images of `disasm\build` remain a fallback for the test harness.

## 6. Autofire — `c_src\platform\windows\plat_win.c`

`[input] autofire=1` (default) and `autofire_hz=10` in `sw_win.ini`: while a
trigger (mouse, keyboard or controller) is held, the signal is chopped at
`autofire_hz` shots per second, so the game sees repeated presses.
`autofire=0` restores the original behaviour (one shot per press).

## 7. Vector OPTIONS menu and smoothness modes — `plat_win.c`

Shown at launch and on **F1** at any time, drawn with the vector beam
(AVG-style stroke font, black background like an operator menu):

| Item | Values | Effect |
|---|---|---|
| PRESET | ARCADE 1983 / MODERN PC / CUSTOM | applies a full set of settings |
| FULLSCREEN | ON/OFF | immediate (`[main] fullscreen` at startup) |
| VSYNC | ON/OFF | display sync |
| SMOOTHNESS | ORIGINAL / DISPLAY 120HZ+ | `present_monitor`: presents every display refresh; the phosphor decay is recomputed at the monitor's rate (120/144/240 Hz) without changing game speed |
| PHOSPHOR | 0–40 ms | persistence (`ph_tau`) |
| LINE WIDTH | 0.6–4.0 px | beam width, applied live |
| BRIGHTNESS | 0.6–3.0 | brightness gain |
| AUTOFIRE / FIRE RATE | ON/OFF, 1–30/s | the autofire of section 6 |
| MOUSE / INVERT PITCH | sensitivity, ON/OFF | controls |

- **ARCADE 1983**: vsync, original timing, no trail, line 2.0, no autofire.
- **MODERN PC**: presentation at the monitor's rate (120 Hz+), 12 ms phosphor,
  autofire 10/s.
- Any manual change switches the preset to CUSTOM. ENTER on PLAY (or F1/Esc)
  closes the menu and saves everything to `sw_win.ini`.
- While the menu is open, triggers and coins are ignored; the attract mode keeps
  running behind it.

## 8. Audio crackle fixed — `plat_win.c` / `mixer.c`

1. `stream_push` truncated any block over 512 frames and **dropped the rest**,
   while the audio pump hands over up to 2048 frames when a loop iteration takes
   more than ~11.6 ms: a constant deficit, about 45 dropouts per second.
   Fixed: `plat_audio_push` slices blocks into 512-frame pieces.
2. The latency cushion (6 silent blocks at startup) was never rebuilt after a
   stall. Fixed: `stream_push` re-primes when the queue drains.

`stream_stats` counters are logged to `sw_win.log` every second, only when
something goes wrong (`starved`/`flushed`). `[sound] latency_blocks` (1–8,
default 6) in `sw_win.ini`.

## 9. Controllers and flight yoke — `joystick.c`, `plat_win.c`

Every connected controller is read (before: only the first one Windows lists,
which can be a mouse dock posing as a joystick — a yoke plugged in later was
ignored). `[joystick]` keys in `sw_win.ini`:

- `ignore=ROCCAT`: device names to skip (`;`-separated);
- `button_fire_r=1`, `button_fire_l=2`, `button_thumb_r=3`, `button_thumb_l=4`,
  `button_coin=7`: numbers as in Windows' "Game Controllers" panel (0 = none),
  to adjust if a yoke numbers its buttons differently.

## Automated check

```
cd c_src
sw_win.exe --turbo --hidden --no-nvram --script tests\scenarios\centered.txt ^
  --passes 700 --shot p650 check.png --shot-size 1280x720
```
