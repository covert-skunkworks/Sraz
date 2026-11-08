# Sraz

A recreation of **Sraz – Borba znanjem**, a 1993 Croatian DOS quiz board game, for Windows, Linux and macOS. It plays by the
rules of the original program, but with an updated set of questions and a resizable window.

Two to four players, five pegs each, move across a board of tiles that each carry a question category. Answer correctly to
step onto a tile, win battles against enemy pegs, rank up from Serf (Kmet) to Soldier (Vojnik) to Knight (Vitez), and
eliminate every other army. Play on one computer or over a LAN.

**Questions**, in eight categories (Sport, History, Geography, Nature, Music, General, Art, Science):

| Language | Questions |
|---|---|
| English | 1,000 (125 per category) |
| Croatian | 1,945 |

The Croatian bank holds the 1,000 questions of the original game, with outdated facts and errors corrected, plus 945 more.
The question language is chosen when a game is set up.

The rules are in [docs/RULES.md](docs/RULES.md); development notes in [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md).

## Run

Download a package from the Releases page, unpack it and start `sraz` (`sraz.exe` on Windows, `Sraz.app` on macOS).
Keep the `questions` folder next to the program. Nothing else has to be installed.

## Build

You need a C99 compiler, GNU make, pkg-config and the SDL2 development files. Nothing else.

| System | Install | Build |
|---|---|---|
| Windows | [MSYS2](https://www.msys2.org), then `pacman -S make mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-pkgconf` | `make` in the UCRT64 shell |
| Linux | `sudo apt install build-essential pkg-config libsdl2-dev` | `make` |
| macOS | Xcode command line tools, `brew install sdl2` | `make` |

`make` builds `sraz`; `make test` builds and runs the tests, which need neither SDL nor a display.

## Pricing

This tool is provided to everyone for free, however if you would like to say thanks or help support continued development of this and projects to come, feel free to send a coffee:

[<img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="BuyMeACoffee" width="100">](https://www.buymeacoffee.com/covert.skunkworks)

## Licence

GPLv3, see [LICENSE](LICENSE). Built in: [SDL2](https://www.libsdl.org) (zlib), [stb_truetype](https://github.com/nothings/stb)
(public domain), the Droid Sans font (Apache 2.0).
