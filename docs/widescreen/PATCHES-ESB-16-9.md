# The Empire Strikes Back 16:9 — what was changed

Same PC engine as Star Wars 16:9 (window, vector renderer, vector OPTIONS menu
on **F1**, ARCADE 1983 / MODERN PC presets, 120 Hz+ smoothness, autofire, fixed
audio). Starts in fullscreen (`[main] fullscreen=1` in `esb.ini`).

The difference with Star Wars: the upstream repository only contains the *Star
Wars* program translated to C. ESB therefore runs its **original ROM** on two
emulated 6809 CPUs (based on the repository's `sw_emu`). Sources: `esb_src`,
build `cmd /c esb_src\build_esb.bat`.

## Controls

| Action | Mouse | Keyboard | Controller |
|---|---|---|---|
| Lasers (triggers) | left button | Left Ctrl, Alt | buttons 1, 2 |
| **Tow cable** (thumb buttons) | **right button**, middle button | Space, Shift | buttons 3, 4 |

The game fires the tow cable only from a thumb button (ROM `$96AA`, mask `$30`
→ routine `$9590`). Autofire applies to the triggers only; otherwise a held
cable button would spend all four cables.

## ROMs

`roms\esb.zip` = your own MAME `esb` set (**not included — supply your
own**), used as is. Files are recognised by CRC, as MAME does, so any version of
the set works. **Nothing is written to the zip**: every change is applied in
memory when the game loads.

## Hardware added to the emulator

- second ROM bank at `$A000-$FFFF` (switched with the first by W.BANK);
- the **137412-101 Slapstic** protection chip at `$8000-$9FFF`, four 8 KB
  pages, state machine from MAME (`slapstic.cpp`);
- the 6809's dead bus cycles in indexed addressing (a re-read of the next
  opcode byte, `$FFFF` accesses), from MAME (`m6809.lst`): ESB's protection is
  keyed on them (`LDA 0,X` at `$9DFE` → `$9E00`, `$FFFF`, `$9B5E`); without
  them the game crashed a few seconds into play;
- 64 KB sound map (ROM at `$4000-$7FFF` and `$C000-$FFFF`);
- ESB's factory DIP setting `dsw0=243` (0xF3: 4 shields, moderate, attract music).

## 16:9 widening (in memory)

| What | Where | Original | 16:9 |
|---|---|---|---|
| 3D object X clamp | bank 2 `$D9B3/$D9B8/$D9BD/$D9C2` | ±534 (510+24) | ±704 (680+24) |
| Star visibility cone | bank 1 `$7B5A`, `$7C6A` (`SUBD $5070`) | X | X × 1.31 (routine at `$7F00`) |
| Snowspeeder guns | vector ROM `$3424` / `$33B0` | dx 0 | ±60 |
| Space-phase guns | vector ROM `$349E` / `$3498` | dx 0 | ±88 |
| Laser origins | Slapstic page 3, table `$96B5` | ±168/±200/±150, ±395/±390 | + ±60 / ±88 |

Rule (as Star Wars' ±480 → ±650): each gun sits at 4/3 of its original offset
from the centre, and the lasers start from the moved gun.
