//  Description:
//
//		game.h -- the Sraz game flow, independent of SDL and of any clock.
//
//		This is the layer between the rules engine (board.h) and the user
//		interface: it owns the question being asked, the answer timer, the
//		one- or two-question sequence of a battle, the turn change, the result
//		pause, the winner, snapshots for LAN guests and the save file. Time is
//		passed in by the caller in milliseconds, so the same code serves the
//		window, the LAN host and the headless tests.
//
//		Scene numbers equal those carried in lan_snapshot (1 .. 4). A
//		question that has been answered stays in SG_SCENE_QUESTION with
//		answered set while the tick or cross is shown.
//
#ifndef GAME_H
#define GAME_H

#include "board.h"
#include "lan.h"
#include "quiz.h"
#include <stdint.h>

#define SG_SCENE_LOBBY 1
#define SG_SCENE_BOARD 2
#define SG_SCENE_QUESTION 3
#define SG_SCENE_OVER 4

#define SG_RESULT_MS 1000  // Tick or cross display time (R-46).
#define SG_GRACE_MS 800    // Extra time for answers of remote players.
#define SG_EV_MAX 16

// Events for sound and messages, read with sg_event().
#define SG_EV_NONE 0
#define SG_EV_CORRECT 1
#define SG_EV_WRONG 2
#define SG_EV_LOST 3     // A peg was destroyed.
#define SG_EV_TURN 4     // The turn passed to another player.
#define SG_EV_WIN 5
#define SG_EV_MOVE 6     // A peg moved.
#define SG_EV_SKIP 7     // A player was skipped, see skipped[].

typedef struct
{
	board_state   board;
	quiz_bank    *quiz;      // Host or local only, NULL for a LAN guest.
	char          names[BOARD_NPLAYERS][LAN_NAME_SIZE];
	int           language, timer_sec, scene;
	int           peg, tr, tc, kind, qnum, q1;  // Pending action.
	long          qindex;
	int           order[4];
	int           category;         // Category 1..8 of the question asked.
	quiz_question cur;             // Answers in display order.
	int           correct_slot, answered, picked, was_correct, hilite;
	uint32_t      q_start, res_start, epoch;
	int           winner;          // -1 running, 0..3, -2 stalemate (see leader).
	int           leader;          // Winner after a stalemate.
	int           local_mask;      // Seats played on this computer.
	int           skipped[BOARD_NPLAYERS];
	int           ev[SG_EV_MAX], ev_head, ev_tail;
	int           guest;           // Mirror of a host, never advances itself.
} sg_game;

// Selection and cursor of the player on turn (R-34).
typedef struct
{
	int peg;      // Selected peg, -1 if none.
	int cpeg;     // Highlighted peg while none is selected.
	int cr, cc;   // Cursor tile while a peg is selected.
} sg_sel;

/**
  * @brief  Start a new game on a fresh board.
  * @param  g Receives the game; any previous state is discarded.
  * @param  quiz Loaded question bank.
  * @param  n Board size 8 or 10.
  * @param  nplayers Players 2..4.
  * @param  names Player names, NULL for the defaults.
  * @param  timer_sec Answer time in seconds.
  * @param  language Language number of the questions, 0 or 1.
  * @param  seed Seed of the board generator.
  * @param  local_mask Seats played on this computer (bit per seat).
  * @retval 0 on success, -1 on bad arguments.
  */
int sg_start( sg_game *g, quiz_bank *quiz, int n, int nplayers,
              const char names[][LAN_NAME_SIZE], int timer_sec, int language,
              uint32_t seed, int local_mask );

/**
  * @brief  Start the question for a peg's move (R-40 .. R-43).
  * @param  g Game.
  * @param  player Acting player; must be the player on turn.
  * @param  peg Peg index.
  * @param  r Target row.
  * @param  c Target column.
  * @param  now Time in milliseconds.
  * @retval 0 accepted, -1 wrong scene or player, -2 illegal move.
  */
int sg_move( sg_game *g, int player, int peg, int r, int c, uint32_t now );

/**
  * @brief  Answer the current question.
  * @param  g Game.
  * @param  player Answering player; must be the player on turn.
  * @param  slot Display slot 0..3, or -1 for no answer.
  * @param  now Time in milliseconds.
  * @retval 0 accepted, -1 not allowed now.
  */
int sg_answer( sg_game *g, int player, int slot, uint32_t now );

/**
  * @brief  Pass the turn (R-33).
  * @param  g Game.
  * @param  player Passing player; must be the player on turn.
  * @param  now Time in milliseconds.
  * @retval 0 accepted, -1 not allowed now.
  */
int sg_pass( sg_game *g, int player, uint32_t now );

/**
  * @brief  Advance timers: answer timeout and the end of the result pause.
  * @param  g Game.
  * @param  now Time in milliseconds.
  * @retval None.
  */
void sg_tick( sg_game *g, uint32_t now );

/**
  * @brief  Milliseconds left to answer.
  * @param  g Game.
  * @param  now Time in milliseconds.
  * @retval Remaining time, 0 when out of time or no question is open.
  */
uint32_t sg_remaining_ms( const sg_game *g, uint32_t now );

/**
  * @brief  Pop the oldest pending event.
  * @param  g Game.
  * @retval SG_EV_xxx, SG_EV_NONE if the queue is empty.
  */
int sg_event( sg_game *g );

/**
  * @brief  Describe the game as a snapshot for LAN guests.
  * @param  g Game.
  * @param  sel Selection to show while a peg is chosen, may be NULL.
  * @param  nplayers Number of connected players.
  * @param  now Time in milliseconds.
  * @param  s Receives the snapshot.
  * @retval None.
  */
void sg_snapshot( const sg_game *g, const sg_sel *sel, int nplayers,
                  uint32_t now, lan_snapshot *s );

/**
  * @brief  Make a guest game mirror a host snapshot.
  * @param  g Guest game (guest set).
  * @param  s Snapshot.
  * @param  now Time in milliseconds.
  * @retval None.
  */
void sg_apply( sg_game *g, const lan_snapshot *s, uint32_t now );

/**
  * @brief  Reset a selection at the start of a turn.
  * @param  s Selection.
  * @param  b Board.
  * @param  player Player on turn.
  * @retval None.
  */
void sgs_reset( sg_sel *s, const board_state *b, int player );

/**
  * @brief  Handle an arrow key: cycle pegs, or move the target cursor.
  * @param  s Selection.
  * @param  b Board.
  * @param  player Player on turn.
  * @param  dr Row step -1, 0 or 1.
  * @param  dc Column step -1, 0 or 1.
  * @retval None.
  */
void sgs_arrow( sg_sel *s, const board_state *b, int player, int dr, int dc );

/**
  * @brief  Handle Tab: select the next peg that can act.
  * @param  s Selection.
  * @param  b Board.
  * @param  player Player on turn.
  * @param  dir +1 or -1.
  * @retval None.
  */
void sgs_tab( sg_sel *s, const board_state *b, int player, int dir );

/**
  * @brief  Handle Enter: select the highlighted peg, cancel, or confirm.
  * @param  s Selection.
  * @param  b Board.
  * @param  player Player on turn.
  * @param  peg Receives the peg when a move is confirmed.
  * @param  r Receives the target row.
  * @param  c Receives the target column.
  * @retval 1 if a move was confirmed, 0 otherwise.
  */
int sgs_confirm( sg_sel *s, const board_state *b, int player, int *peg,
                 int *r, int *c );

/**
  * @brief  Handle a click on a board tile.
  * @param  s Selection.
  * @param  b Board.
  * @param  player Player on turn.
  * @param  r Tile row.
  * @param  c Tile column.
  * @param  peg Receives the peg when a move is confirmed.
  * @retval 1 if the click confirmed a move onto (r, c), 0 otherwise.
  */
int sgs_click( sg_sel *s, const board_state *b, int player, int r, int c,
               int *peg );

/**
  * @brief  Drop a selection that is no longer valid.
  * @param  s Selection.
  * @param  b Board.
  * @param  player Player on turn.
  * @retval None.
  */
void sgs_validate( sg_sel *s, const board_state *b, int player );

/**
  * @brief  Cancel the peg selection.
  * @param  s Selection.
  * @retval None.
  */
void sgs_cancel( sg_sel *s );

/**
  * @brief  Write the game to a save file.
  * @param  g Game on the board scene.
  * @param  path File name.
  * @retval 0 saved, -1 not on the board scene, -2 file error.
  */
int sg_save( const sg_game *g, const char *path );

/**
  * @brief  Restore a game from a save file.
  * @param  g Game with a loaded question bank in quiz; board and settings are
  *         replaced on success.
  * @param  path File name.
  * @retval 0 loaded, -1 no such file, -2 invalid file.
  */
int sg_load( sg_game *g, const char *path );

#endif
