# esb_src — The Empire Strikes Back, 16:9, on the C port's PC engine

`esb.exe` runs the **real ESB ROMs** (MAME set `esb`, read from `roms\esb.zip`
by CRC; none included) on two 6809 cores. It started as a copy of
[`../emu_src`](../emu_src/README.md) (`sw_emu`, the same hardware) and carries
the Windows backend of [`../c_src`](../c_src/README.md) as modified on the
`widescreen-16-9` branch: 16:9 picture, vector OPTIONS menu (F1), presets,
120 Hz+ presentation, autofire, audio fix.

What ESB adds to the Star Wars board, and the 16:9 changes made to the ROM in
memory at load time:

- `board.cpp` — second ROM bank at `$A000`, the 137412-101 slapstic;
- `cpu/cpu_m6809.cpp` — the 6809's dead bus cycles in indexed addressing
  (the slapstic sequence depends on them);
- `emu_main.cpp` — loading by CRC, `widescreen_patch()`.

Build: `cmd /c ".\build_esb.bat"` (VS 2022 Build Tools, x64).
Test: `esb.exe --turbo --hidden --no-nvram --script tests\play.txt --passes 5000`
(here a script "pass" is a picture: ESB's mainline is not Star Wars' `$602F`).
