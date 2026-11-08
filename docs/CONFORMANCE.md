# Conformance of the native version (src/) with docs/RULES.md

Status per rule: **yes** = implemented as written, **partly** = implemented with a documented difference, **no** = not
implemented. Tests live in `src/`: `test_board.c` (rules engine), `test_game.c` (game flow), `test_lan.c` (LAN
transport), `test_vg.c` (rasteriser). "visual" means checked on the headless screenshots (`--shot`, see README) because
the item is about looks. The tests are run by `make test` / `.\build.ps1 -Test`.

| Rule | Status | Implemented in | Tested by |
|---|---|---|---|
| R-10 players 2, 3, 4 (default 2) | yes | `app.c:app_defaults`, `ui.c:ui_draw_setup` | visual (scene 2); `test_board.c` boards with 2-4 players |
| R-11 names, 12 characters, default `IGRAC 1..4` | partly | `ui.c:ui_field` (limit `APP_NAME_MAX`), `game.c:sg_start` | visual; names are not forced to upper case |
| R-12 board 8x8 / 10x10 | yes | `board.c:board_init`, `ui.c:ui_game_options` | `test_board.c` (n = 8, 10); visual (scenes 6, 7) |
| R-13 answer time 9/15/30/60/600 s, default 15 | yes | `ui.c:ui_game_options`, `game.c:sg_tick` | `test_game.c:test_timer`; `lan.c` validates the values |
| R-14 mouse sensitivity, colour scheme, control style | partly | colour set: `app.h` `army`, Options screen; the mouse is always a free pointer and the keyboard always cycles pegs | visual (scene 5) |
| R-15 sound on, F4 toggles; Space starts; Esc quits | partly | `main.c:main_event` (F4), `snd.c`; Start is a button / Enter, Esc goes back | manual |
| R-20 geometry, centre 6x6 replaced by question panel | yes | `ui_game.c:ug_origin`, `ug_draw_tiles`, `ug_draw_question` (tile 76.5 x 51 = original x 1.5) | visual (scenes 6-10 at two aspect ratios) |
| R-21 flags in the corners, 2 players TL and BR | yes | `board.c:board_setup_player` | `test_board.c` (home positions of 2 and 4 players) |
| R-22 five pegs around the flag, flag tile empty and not enterable by its owner | yes | `board.c:board_setup_player`, `board_target_kind` | `test_board.c`; `test_game.c:test_flag_pass` |
| R-23 holes: N - players, black, impassable | yes | `board.c:board_init`; art `hole` | `test_board.c` (hole count) |
| R-24 category distribution, flag tiles asked at random | yes | `board.c:board_init`; `game.c:sg_ask` | `test_board.c` (per-category counts) |
| R-25 the nine categories | yes | `vg.c:vg_category_name`, legend in `ui_game.c:ug_draw_info` | visual |
| R-30 score 0..14, rank = score div 5, pegs I..V | yes | `board.c:board_rank`; numerals in `ui_game.c:ug_draw_peg`; rank art `peg_0/1/2` | `test_board.c` |
| R-31 cyclic turns, eliminated players skipped, rank + 1 attempts | yes | `board.c:board_end_turn`; `game.c:sg_end_turn` (also reports skipped players) | `test_game.c:test_step`, `test_turns` |
| R-32 which pegs can act, enterable tiles | yes | `board.c:board_peg_can_act`, `board_target_kind`; `game.c:sg_move` re-checks | `test_board.c`; `test_game.c:test_step` |
| R-33 turn ends: nobody can act, battle, flag attack, Space | yes | `game.c:sg_resolve`, `sg_pass`, `sg_end_turn` | `test_game.c:test_step`, `test_flag_pass`, `test_battle` |
| R-34 selecting: peg, neighbour, Enter on the peg cancels | yes | `game.c:sgs_arrow/tab/confirm/click/cancel`; mouse in `ui_game.c:ug_board_input` | `test_game.c:test_selection` |
| R-40 normal step: one question, +1 on a right answer, nothing on a wrong one | yes | `board.c:board_step`; `game.c:sg_resolve` | `test_board.c`; `test_game.c:test_step` |
| R-41 +1 only while the mover owns a free flag | yes | `board.c:board_owns_free_flag` | `test_board.c` (score gate) |
| R-42 enemy flag tile: one question, recolour, move in, turn ends | yes | `board.c:board_flag_attack`; `game.c:sg_resolve` | `test_board.c`; `test_game.c:test_flag_pass` |
| R-43 battle: two questions, second only after a right first, -1 / push / kill | yes | `board.c:board_battle`; `game.c:sg_resolve` (second question via `sg_ask`) | `test_board.c`; `test_game.c:test_battle` (all four outcomes) |
| R-44 category of the target tile, no repetition until the category is used up | yes | `game.c:sg_ask` + `quiz.c:quiz_pick_unused` | `test_game.c:test_questions` (125 distinct picks, then reset) |
| R-45 four shuffled answers, keys / mouse, countdown, timeout takes the highlighted answer | partly | `quiz.c:quiz_shuffle`; `ui_game.c:ug_draw_question`; `game.c:sg_tick` | `test_game.c:test_timer`; deviation: the bar is drawn in canvas units, not 264 px, and a remote LAN player's timeout is handled as described below |
| R-46 big tick / cross, jingle, ~1 s pause | partly | `game.c` `SG_RESULT_MS` = 1000, `ui_game.c` (tick drawn in yellow by `gfx.c:gfx_tick`, cross from the art set), `snd.c` | `test_game.c:test_timer` (pause); visual (scenes 9, 10) |
| R-50 the last army standing wins at the turn change | yes | `board.c:board_end_turn`; `game.c:sg_end_turn` | `test_board.c`; `test_game.c:test_turns` |
| R-51 winner screen: text with colour name and player name, waving flag, jingle | partly | `ui_game.c:ui_winner` (text and credits of the original, 8-frame art flag instead of 14 frames, own jingle); the hidden key Alt+P is not implemented | visual (scene 11) |
| R-60 keys: arrows, Enter, Space, Esc dialog D/Y, F4, F5, F8, F9 | partly | `main.c:main_event`, `ui_game.c:ui_game`, `ug_quit_dialog`, `app.c:app_save_load`; save file is own format `sraz.sav`; F9 only toggles a flag that has no visible effect (its original effect is undecoded) | `test_game.c:test_save`, `test_selection` |
| R-61 mouse: movement = arrows, left = Enter, right then left = pass | partly | the mouse works directly instead: click a peg / tile / answer / Pass button, right click cancels | manual |
| R-70 sounds: correct, wrong, peg lost, elimination, turn change, victory | partly | `snd.c` synthesises correct, wrong, peg lost, turn change (rising siren), victory and a click; there is no separate elimination jingle | manual (headless runs use the dummy audio driver) |

## Beyond the original (requirements of this version)

| Feature | Implemented in | Tested by |
|---|---|---|
| Resizable window, uniform scaling of a 960 x 540 canvas, letterbox bars, HiDPI | `gfx.c:gfx_metrics`, `main.c` | visual (960x540, 700x900, 1600x600) |
| Vector art rasterised at the exact pixel size, cached per (asset, size, tint), flushed after a resize | `vg.c`, `gfx.c:gfx_art`, `gfx_request_flush` | `test_vg.c` |
| Text from the embedded font, UTF-8 including the Croatian letters č ć ž š đ, wrapping | `text.c` | visual (Croatian strings, scene 14) |
| LAN host authoritative, guests send move / answer / pass | `app.c:app_update`, `app_lan_command`, `game.c:sg_snapshot/sg_apply` | `test_lan.c` (transport); `test_game.c:test_snapshot`; `build/lan_selftest.sh` (three real processes) |
| Host shows its real LAN addresses with the port | `ui.c:ui_draw_host`, `lan.c:lan_local_addresses` | visual (scene 3) |

## Known deviations and open points

* **Colour set.** The original colour set 1 is red, blue, black, green (`docs/original/part5`, T4); the white army belongs to
  set 4. The default here is set 1; Options -> Army colours switches between the four original sets.
* **Stalemate.** If nobody can move at all, `board_end_turn` reports a stalemate; the game then ends and the player with the
  most pegs, then the highest scores (`board_leader`) is shown as the winner. The original does not define this case.
* **LAN timeouts.** The host cannot see which answer a guest has highlighted. A guest's client therefore sends its
  highlighted answer itself when its own timer reaches zero; the host waits `SG_GRACE_MS` (800 ms) for it and counts no
  answer as wrong.
* **LAN selection.** A guest's peg selection is local until the move is sent. The snapshot carries the host player's own
  selection (`sel_peg`, `target_r`, `target_c`), so spectators see a guest's choice only when its question starts.
* **No 6x6 board** in the UI (the engine supports it). No AI (the original has none).
* **Setup rows of the original not reproduced:** mouse sensitivity, "level" rows and the per-player control style (R-14);
  the original F9 tile style; the Alt+P debug key.
* **The 264 px countdown bar and the 14-frame flag** are replaced by scalable equivalents (art set: 8 frames).
* **Tick colour.** The art set's tick is green; the original's is yellow, so the tick is drawn in yellow by `gfx_tick`
  (R-46); the cross is the art set's red cross.
* **Default language** is English (the original is Croatian); `--lang cro` or Options changes it and the choice is remembered.
* **Question files** are the decoded banks of `questions/en` and `questions/cro`; `quiz_pick_unused` works on whatever the bank
  holds per category.
* **Ambiguities resolved by RULES.md:** the "stripped of a rank on either wrong answer" prose of the original `info.txt`
  is not followed; the disassembly-based battle rule (R-43) is.
