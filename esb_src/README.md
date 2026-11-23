# esb_src — The Empire Strikes Back

The real ESB ROMs (MAME set `esb`, read from `roms\esb.zip` by CRC; none
included) on two 6809 cores. It started as a copy of
[`../emu_src`](../emu_src/README.md) (`sw_emu`, the same hardware) and is part
of `AtariStarWarsArcadeRE.exe` (`trilogy_src\build_trilogy.bat`), on the platform
layer of [`../c_src`](../c_src/README.md); `--game esb` selects it.

Everything here is in C++ namespace `esb`, so it links beside the Star Wars C
port, whose hardware functions have the same names; `emu_main.cpp` exports the
machine as `esb_app_*` for `plat_win.c`.

What ESB adds to the Star Wars board, and the 16:9 changes made to the ROM in
memory at load time:

- `board.cpp` — second ROM bank at `$A000`, the 137412-101 slapstic;
- `cpu/cpu_m6809.cpp` — the 6809's dead bus cycles in indexed addressing
  (the slapstic sequence depends on them);
- `emu_main.cpp` — loading by CRC, `widescreen_patch()`.

Test: `AtariStarWarsArcadeRE.exe --game esb --turbo --hidden --no-nvram --script esb_src\tests\play.txt --passes 5000`
(here a script "pass" is a picture: ESB's mainline is not Star Wars' `$602F`).
