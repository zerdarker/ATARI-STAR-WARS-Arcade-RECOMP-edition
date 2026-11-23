# selector_src — the game selection page

The page `AtariStarWarsArcadeRE.exe` opens when it is run without `--game`: Star Wars
and The Empire Strikes Back, each title logo in a frame, drawn with the games'
own beam renderer and options-menu font, plus a **GITHUB PAGE** and an
**UPDATE** button. Arrows, mouse or stick choose; Enter, a click or a button
selects; the exe runs itself with `--game starwars` / `--game esb` and the page
comes back when the game quits; Esc leaves.

The logos are drawn by the games themselves on first run
(`--game X --turbo --hidden --no-nvram --shot pN logos\gameN.vec`: a `.vec`
shot writes the picture's segments as text), and the page keeps the blue and
cyan strokes below the score line. No ROM data is in this program.

The music is the Star Wars attract-mode theme, recorded from the game on first
run (`--wav`, with a script that pokes `MUSTIM` at `$4B34` so the game plays it
at once instead of every ~6.7 minutes), cut to the 12.8 s of music and looped
with a 2.5 s pause. It stops while a game runs. `[main] music=0` turns it off.

**UPDATE** reads `releases/latest` of the GitHub repository (`REPO` in
`selector.c`), compares its tag with `TRILOGY_VERSION`, downloads the release
zip, takes `AtariStarWarsArcadeRE.exe` out of it, renames the running exe to `.old`,
puts the new one in its place and restarts. It needs the repository to be
public. `..\trilogy_src\tests\check_release.c` exercises it against a public
repository.

`selection.ini` (next to the exe): `[gameN] title, logo_picture`,
`[main] fullscreen, last, music`, `[joystick] ignore`. Delete `logos\` to
redraw them. Built by `..\trilogy_src\build_trilogy.bat`.
