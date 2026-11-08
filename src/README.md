# Sraz (native, SDL2)

A faithful remake of the 1993 DOS trivia board game *Sraz*, written in C99 with SDL2. The rules are those of the
original program, consolidated in [`../docs/RULES.md`](../docs/RULES.md) (rule numbers `R-xx`); where each one lives in
the code and how it is tested is in [`../docs/CONFORMANCE.md`](../docs/CONFORMANCE.md).

Features: hot-seat play for 2-4 players, LAN play for 2-4 computers, 8x8 or 10x10 board, 9 / 15 / 30 / 60 / 600 s answer
time, English or Croatian questions (1,000 and 1,945 questions), a resizable window with a uniformly scaled 960 x 540
design canvas, all artwork drawn from vector data by a built-in software rasteriser, synthesised sound.

## Build

Needs a C99 compiler, GNU make, pkg-config and SDL2 development files. Nothing else: no SDL2_ttf, no FreeType, no zlib.
The font (DroidSans) is compiled into the executable, the artwork is compiled in (`../art/art_data.h`).

Windows (MSYS2 MinGW64: `pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 make pkg-config`):

~~~powershell
.\build.ps1            # builds sraz_sdl.exe; SDL2 is linked statically, so no DLL is needed beside it
.\build.ps1 -Test      # build and run all tests
.\build.ps1 -Clean
~~~

`build.ps1` puts `C:\msys64\mingw64\bin` (override with `-Toolchain`) and `C:\msys64\usr\bin` (`-Tools`) on the PATH of
that one process only. `sraz_sdl.exe` imports only Windows system DLLs. The old SDL2/FreeType DLLs in this folder belong to
the previous version (`legacy/`) and are not needed.

Linux (`sudo apt install build-essential pkg-config libsdl2-dev`) and macOS (`brew install sdl2`):

~~~sh
make          # builds ./sraz_sdl
make test
~~~

Run it from anywhere; the executable finds `questions/en` and `questions/cro` next to itself. A `VERSION` file holds the
version (shown by `--version` and on the main menu). Options are kept in `sraz.cfg` and a saved game in `sraz.sav`, both
next to the executable.

## Playing

Menu: **Play** (hot-seat on this computer), **Host LAN game**, **Join LAN game**, **Options**, **Quit**. Setup lets you
choose 2-4 players with names (12 characters, default `IGRAC 1..4`), the board, the answer time and the question language.

Game screen: the board is on the left, with the question panel over the centre 6x6 tiles while a question is open; on the
right are the current player, the selected peg's rank and progress, the pegs left of every player and the category legend.

| Input | Action |
|---|---|
| Arrows / Tab / Shift+Tab | cycle through your pegs that can act; with a peg selected the arrows move the cursor to a neighbouring tile |
| Enter or click on a peg | select the peg; Enter on the peg itself, Esc or right click cancels |
| Enter or click on a highlighted tile | move there: this asks the question(s) |
| Space or the Pass button | pass the turn |
| Up / Down, 1-4, mouse | choose an answer; Enter or a click confirms; on timeout the highlighted answer is taken |
| Esc | cancel selection, otherwise the quit dialog (`D` / `Y` = yes, anything else = no) |
| F4 | sound on / off |
| F5 / F8 | save / load the hot-seat game (F8 also works on the main menu) |
| F11 or Alt+Enter | fullscreen |

Rules in short (full list in `docs/RULES.md`): a normal step and an enemy flag tile cost one question, a battle two (the
second only if the first was right). A peg has `rank + 1` attempts per turn; its score 0..14 gives the rank
(Serf / Soldier / Knight per 5 points). Entering an enemy flag or fighting ends the turn; so does having no peg left that
can act, or pressing Space. The last army standing wins.

## LAN game

1. One player chooses **Host LAN game**, optionally edits name and port (default 34567) and presses **Open lobby**. The
   screen lists this computer's real LAN addresses, big, for example
   `Give this address to the other players: 192.168.0.112 : 34567`, each with a **Copy** button.
2. The others choose **Join LAN game**, type that address (the field starts empty, or with the last used address), the port
   and their name, and press **Connect**. They see the connected players while waiting.
3. With 2-4 players connected the host picks board, time and language and presses **Start game**.

The host is authoritative: it owns the board, the questions, the shuffled answers and the timers. Guests send only
move / answer / pass commands and draw the snapshots they receive; only the player on turn can act. Esc on the host asks
whether to return everybody to the lobby (new game, late joins and replacements are possible there); Esc on a guest leaves;
a lost connection returns the host to the lobby. Allow the program through the firewall (private network, the chosen TCP
port). A guest's selection is local until the move is sent, so others see it only once the question starts.

## Command line

~~~
sraz_sdl [--host] [--join ADDR] [--port N] [--name S] [--lang en|cro] [--data DIR] [--assets DIR]
         [--shot FILE.png --scene N [--size WxH]] [--help] [--version]
~~~

`--host` opens a lobby at once, `--join ADDR` connects at once, `--data DIR` points at a folder holding `0011.100` and
`0011.200` (overrides `questions/`), `--assets DIR` is accepted for compatibility (the art is built in).
`--selftest MS` is a test aid: it plays random legal moves for the local seat and quits after MS milliseconds.

## Tests and screenshots

`make test` / `.\build.ps1 -Test` run, without any window or display:

* `test_board` - the rules engine against `docs/RULES.md`;
* `test_lan` - the transport over real loopback sockets (host and three guests);
* `test_game` - the game flow: question counting, battle sequence, timer, turn passing, selection, snapshots, save file;
* `test_vg` - the rasteriser (areas, even-odd, strokes, all 40 assets, tinting) and the PNG writer; it also writes
  `build/vg_sheet.png`, a contact sheet of every asset.

`build/lan_selftest.sh` runs a host and two guests as real processes over loopback with the dummy video driver.

Headless screenshots (dummy video driver + software renderer, a built-in PNG encoder, no zlib):

~~~powershell
.\sraz_sdl.exe --shot build\shots\board.png --scene 6
.\sraz_sdl.exe --shot build\shots\board-tall.png --scene 6 --size 700x900
~~~

Scenes: 0 splash, 1 menu, 2 setup, 3 LAN host lobby, 4 LAN join, 5 options, 6 board 8x8 (4 players), 7 board 10x10
(3 players), 8 question, 9 correct result, 10 wrong result, 11 winner, 12 selection and targets, 13 quit dialog,
14 battle (second question, long text). The default size is 960x540; any other size shows the same layout scaled uniformly
and centred, with bars in the background colour.

## Source map

| file | purpose |
|---|---|
| `board.c/h` | rules engine (pure, no UI) |
| `quiz.c/h` | question banks, unused-question tracking |
| `lan.c/h` | non-blocking LAN host / guest protocol |
| `game.c/h` | game flow: question sequence, timer, turns, selection, snapshots, save file (no SDL, no clock) |
| `vg.c/h` | vector rasteriser for `art/art_data.h` |
| `gfx.c/h`, `text.c` | SDL drawing layer: scaled canvas, texture cache keyed by (asset, size, tint), stb_truetype text |
| `app.c/h` | options, setup, LAN glue, per-frame logic |
| `ui.c`, `ui_game.c` | widgets and screens, game and winner screens |
| `snd.c/h` | synthesised sound |
| `strs.c/h` | English / Croatian interface strings |
| `png.c/h` | PNG writer |
| `main.c` | command line, event loop, screenshot mode |
| `stb_impl.c` | compiles the third-party `../third_party/stb/stb_truetype.h` (untouched, warnings off) |
| `legacy/` | the previous SDL2_ttf based version, built on a wrong rules model, kept for reference |
