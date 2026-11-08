Sraz - Borba znanjem (remake)
=============================

A resizable remake of the 1993 Croatian DOS quiz board game "Sraz". Two to four players move five pegs each across a
board. Every tile carries a question category; answer correctly to step onto it, win battles against enemy pegs, rank up
from Kmet (Serf) to Vojnik (Soldier) to Vitez (Knight), and eliminate every other army.

Nothing has to be installed: the program contains everything it needs except the question banks.

Run
---
Windows : unpack, double-click sraz.exe. Keep the "questions" folder next to it. (Windows 10 or 11.)
Linux   : tar xzf sraz-*-linux-x64.tar.gz, then ./sraz. Keep the "questions" folder next to it.
          Needs a desktop (X11 or Wayland) and glibc 2.35 or newer.
macOS   : unzip, open Sraz.app (macOS 11 or newer). It is not notarised: the first time, right-click > Open.

Options are saved in sraz.cfg and the hot-seat game in sraz.sav, next to the program.

Controls
--------
Arrows / Tab   choose peg and target tile        Enter / click   confirm
Space          pass the turn                      Esc             quit dialog / back
1-4            pick an answer                     F4              sound on/off
F5 / F8        save / load                        F11, Alt+Enter  fullscreen

LAN play
--------
One player chooses "Host LAN game"; the lobby shows the computer's network address and the port. The others choose
"Join LAN game" and type that address. Allow the program through the firewall on the private network.

Command line: sraz --help

Licences of the software built in are in the "licenses" folder.
