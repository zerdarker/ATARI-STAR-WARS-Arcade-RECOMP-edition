# selector_src — the game selection page

`selection.exe`: a vector page that offers Star Wars (`../c_src`, `sw_win.exe`)
and The Empire Strikes Back (`../esb_src`, `esb.exe`), each title logo in a
frame, drawn with the games' own beam renderer and options-menu font. Arrows,
mouse or stick pick a game; Enter, a click or a button plays it; the page comes
back when the game quits; Esc leaves.

The logos are drawn by the games themselves on first run
(`--turbo --hidden --no-nvram --shot pN logos\gameN.vec`: a `.vec` shot writes
the picture's segments as text), and the page keeps the blue and cyan strokes
below the score line. No ROM data is in this program.

`selection.ini` (next to the exe): `[gameN] title, exe, logo_picture`,
`[main] fullscreen, last`, `[joystick] ignore`. Delete `logos\` to redraw them.

Build: `cmd /c ".\build_selector.bat"` (VS 2022 Build Tools; the framework files
are compiled from `..\c_src\platform\windows`).
