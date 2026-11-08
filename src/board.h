//  Description:
//
//		board.h -- Sraz rules engine, ported from the original DOS program.
//
//		Every rule here is traced to the disassembly of SRAZ.EXE; the rule
//		numbers (R-xx) refer to docs/RULES.md. The engine is pure: it holds no
//		questions, timers or UI, so the native game, the LAN host and the
//		regression tests all drive the same code. Rows and columns are zero
//		based, row 0 is the top of the board.
//
#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

#define BOARD_MAX_SIZE 10
#define BOARD_NPLAYERS 4
#define BOARD_NPEGS 5
#define BOARD_MAX_SCORE 14
#define BOARD_SCORE_PER_RANK 5
#define BOARD_MAX_RANK 2

// Terrain codes. 1..9 are question categories (9 = random). 11..14 are the
// home/flag tile of player 0..3 (recoloured when captured).
#define BOARD_HOLE 0
#define BOARD_CAT_RANDOM 9
#define BOARD_FLAG_BASE 10

// Outcome of an action, returned by the board_* move functions.
#define BOARD_OUT_NONE 0     // Nothing moved.
#define BOARD_OUT_MOVED 1    // The peg moved to the target tile.
#define BOARD_OUT_WON 2      // Battle won, the attacker advanced.
#define BOARD_OUT_LOST 3     // Battle lost.

// Kind of target tile, returned by board_target_kind().
#define BOARD_TARGET_NONE 0
#define BOARD_TARGET_NORMAL 1
#define BOARD_TARGET_FLAG 2
#define BOARD_TARGET_BATTLE 3

typedef struct
{
	int alive;
	int r, c;
	int score;     // 0..14, rank is score / 5.
	int attempts;  // Steps left this turn.
} board_peg;

typedef struct
{
	board_peg peg[BOARD_NPEGS];
	int       pegs_left;  // Living pegs, zero means eliminated.
	int       active;     // Seat in use.
	int       home_r, home_c;
	int       sel;  // Last selected peg, restored at the start of the turn.
} board_player;

typedef struct
{
	int          n, nplayers;
	int          terrain[BOARD_MAX_SIZE][BOARD_MAX_SIZE];
	int          occ[BOARD_MAX_SIZE][BOARD_MAX_SIZE];  // 0 or 1 + p * 5 + k.
	board_player p[BOARD_NPLAYERS];
	int          turn;
	int          winner;  // -1 running, 0..3 winner, -2 stalemate.
	uint32_t     rng;
} board_state;

/**
  * @brief  Create a board in the starting position of the original game.
  *
  * Flags sit in the corners (P0 top left, P1 bottom right, P2 top right,
  * P3 bottom left), five pegs per player stand in a ring around the flag,
  * n - nplayers holes are scattered and the remaining tiles carry n * (n - 1)
  * categories split evenly (R-20 .. R-25).
  *
  * @param  b Board state.
  * @param  n Board size 6, 8 or 10; other values select 8.
  * @param  nplayers Number of players, 2 to 4.
  * @param  seed Seed of the board's own random generator.
  * @retval None.
  */
void board_init( board_state *b, int n, int nplayers, uint32_t seed );

/**
  * @brief  Draw a pseudo random number from the board's generator.
  * @param  b Board state.
  * @param  m Exclusive upper bound, at least 1.
  * @retval Number in 0 .. m - 1.
  */
int board_rand( board_state *b, int m );

/**
  * @brief  Convert a peg score to a rank.
  * @param  score Peg score.
  * @retval 0 Kmet, 1 Vojnik, 2 Vitez.
  */
int board_rank( int score );

/**
  * @brief  Locate the peg standing on a tile.
  * @param  b Board state.
  * @param  r Tile row.
  * @param  c Tile column.
  * @param  pi Receives the owner index, may be NULL.
  * @param  pg Receives the peg index, may be NULL.
  * @retval 1 if occupied, 0 otherwise.
  */
int board_occupant( const board_state *b, int r, int c, int *pi, int *pg );

/**
  * @brief  Classify a tile as a move target for the player on turn (R-32).
  *
  * The tile must be an orthogonal neighbour of the peg, not a hole, not the
  * player's own flag-coloured tile and free of the player's own pegs.
  *
  * @param  b Board state.
  * @param  pi Player index.
  * @param  pg Peg index.
  * @param  r Tile row.
  * @param  c Tile column.
  * @retval BOARD_TARGET_NONE, _NORMAL, _FLAG (enemy flag tile) or _BATTLE.
  */
int board_target_kind( const board_state *b, int pi, int pg, int r, int c );

/**
  * @brief  Check whether a peg may still act this turn.
  * @param  b Board state.
  * @param  pi Player index.
  * @param  pg Peg index.
  * @retval 1 if the peg is alive, has attempts and a legal neighbour.
  */
int board_peg_can_act( const board_state *b, int pi, int pg );

/**
  * @brief  Check whether any peg of a player may still act.
  * @param  b Board state.
  * @param  pi Player index.
  * @retval 1 if some peg can act, 0 otherwise.
  */
int board_player_can_act( const board_state *b, int pi );

/**
  * @brief  Find the next actable peg, cycling from a starting peg.
  * @param  b Board state.
  * @param  pi Player index.
  * @param  from Peg index to start after, may equal an actable peg.
  * @param  dir +1 for the next peg, -1 for the previous one.
  * @retval Peg index, -1 if no peg can act.
  */
int board_next_peg( const board_state *b, int pi, int from, int dir );

/**
  * @brief  Check whether the player still owns an unoccupied flag tile (R-41).
  *
  * Points are only awarded while this holds.
  *
  * @param  b Board state.
  * @param  pi Player index.
  * @retval 1 if the condition holds, 0 otherwise.
  */
int board_owns_free_flag( const board_state *b, int pi );

/**
  * @brief  Resolve a one-question step onto a free tile (R-40).
  *
  * A correct answer moves the peg and scores a point; either way one attempt
  * is used. Does not end the turn.
  *
  * @param  b Board state.
  * @param  pi Player index.
  * @param  pg Peg index.
  * @param  r Target row, kind BOARD_TARGET_NORMAL.
  * @param  c Target column.
  * @param  correct Whether the question was answered correctly.
  * @retval BOARD_OUT_MOVED or BOARD_OUT_NONE.
  */
int board_step( board_state *b, int pi, int pg, int r, int c, int correct );

/**
  * @brief  Resolve the question for an enemy flag tile (R-42).
  *
  * A correct answer recolours the tile and moves the peg in. The caller must
  * end the turn afterwards in either case.
  *
  * @param  b Board state.
  * @param  pi Player index.
  * @param  pg Peg index.
  * @param  r Target row, kind BOARD_TARGET_FLAG.
  * @param  c Target column.
  * @param  correct Whether the question was answered correctly.
  * @retval BOARD_OUT_MOVED or BOARD_OUT_NONE.
  */
int board_flag_attack( board_state *b, int pi, int pg, int r, int c,
                       int correct );

/**
  * @brief  Resolve a two-question battle (R-43).
  *
  * The second question is only asked when the first was right; pass q2 as 0
  * otherwise. The caller must end the turn afterwards.
  *
  * @param  b Board state.
  * @param  pi Attacking player index.
  * @param  pg Attacking peg index.
  * @param  r Target row, kind BOARD_TARGET_BATTLE.
  * @param  c Target column.
  * @param  q1 First answer correct.
  * @param  q2 Second answer correct.
  * @retval BOARD_OUT_WON or BOARD_OUT_LOST.
  */
int board_battle( board_state *b, int pi, int pg, int r, int c, int q1,
                  int q2 );

/**
  * @brief  Pass the turn to the next player able to act (R-31, R-50).
  *
  * Eliminated players and players without a legal move are skipped. Sets
  * winner when only one army is left, or -2 if nobody can move at all.
  *
  * @param  b Board state.
  * @retval None.
  */
void board_end_turn( board_state *b );

/**
  * @brief  Find the player leading on pegs, then score (stalemate tie-break).
  * @param  b Board state.
  * @retval Player index, -1 if nobody is alive.
  */
int board_leader( const board_state *b );

#endif
