//  Description:
//
//		board.c -- Sraz rules engine, ported from SRAZ.EXE (see docs/RULES.md).
//
//		Function names in the comments (sub_xxxxx) are those of the IDA listing
//		in Sraz_for_DOS/SRAZ.asm so every rule can be re-checked against it.
//

//------------------------------------------------------------------------------
//---------------------------------- Includes ----------------------------------
//------------------------------------------------------------------------------

#include "board.h"
#include <string.h>

//------------------------------------------------------------------------------
//------------------------- Local macros and constants -------------------------
//------------------------------------------------------------------------------

#define BOARD_TMP_FREE 255      // Unassigned tile (original 0xFF).
#define BOARD_TMP_RESERVED 200  // Next to a flag: never a hole (0xC8).

//------------------------------------------------------------------------------
//------------------------------ Local variables -------------------------------
//------------------------------------------------------------------------------

static const int board_dir[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };

//------------------------------------------------------------------------------
//------------------------------ Local functions -------------------------------
//------------------------------------------------------------------------------

/**
  * @brief  Check whether a coordinate lies on the board.
  * @param  b Board state.
  * @param  r Tile row.
  * @param  c Tile column.
  * @retval 1 if the condition holds, 0 otherwise.
  */
static int board_inside( const board_state *b, int r, int c )
{
	return r >= 0 && r < b->n && c >= 0 && c < b->n;
}

/**
  * @brief  Read the terrain of a tile, holes for off-board coordinates.
  * @param  b Board state.
  * @param  r Tile row.
  * @param  c Tile column.
  * @retval Terrain code.
  */
static int board_terrain( const board_state *b, int r, int c )
{
	return board_inside( b, r, c ) ? b->terrain[r][c] : BOARD_HOLE;
}

/**
  * @brief  Read the occupant code of a tile, empty for off-board coordinates.
  * @param  b Board state.
  * @param  r Tile row.
  * @param  c Tile column.
  * @retval 0 or 1 + player * 5 + peg.
  */
static int board_occ( const board_state *b, int r, int c )
{
	return board_inside( b, r, c ) ? b->occ[r][c] : 0;
}

/**
  * @brief  Place a peg record on a tile (sub_16609).
  * @param  b Board state.
  * @param  pi Player index.
  * @param  pg Peg index.
  * @param  r Tile row.
  * @param  c Tile column.
  * @retval None.
  */
static void board_place_peg( board_state *b, int pi, int pg, int r, int c )
{
	board_peg *p = &b->p[pi].peg[pg];

	p->alive = 1;
	p->r = r;
	p->c = c;
	p->score = 0;
	p->attempts = 1;
	b->occ[r][c] = 1 + pi * BOARD_NPEGS + pg;
}

/**
  * @brief  Remove a peg from the game (sub_17366).
  * @param  b Board state.
  * @param  pi Player index.
  * @param  pg Peg index.
  * @retval None.
  */
static void board_kill( board_state *b, int pi, int pg )
{
	board_peg *p = &b->p[pi].peg[pg];

	if ( p->alive && board_inside( b, p->r, p->c ) &&
	     b->occ[p->r][p->c] == 1 + pi * BOARD_NPEGS + pg )
	{
		b->occ[p->r][p->c] = 0;
	}
	p->alive = 0;
	p->score = 0;
	p->attempts = 0;
	if ( b->p[pi].pegs_left > 0 )
	{
		b->p[pi].pegs_left--;
	}
}

/**
  * @brief  Award a point if the rules allow it (sub_17139).
  * @param  b Board state.
  * @param  pi Player index.
  * @param  pg Peg index.
  * @retval None.
  */
static void board_gain( board_state *b, int pi, int pg )
{
	board_peg *p = &b->p[pi].peg[pg];

	if ( p->score != BOARD_MAX_SCORE && board_owns_free_flag( b, pi ) )
	{
		p->score++;
	}
}

/**
  * @brief  Move the peg on a tile to an empty tile (sub_171C4).
  * @param  b Board state.
  * @param  sr Source row.
  * @param  sc Source column.
  * @param  dr Destination row.
  * @param  dc Destination column.
  * @retval None.
  */
static void board_move_record( board_state *b, int sr, int sc, int dr, int dc )
{
	int code = b->occ[sr][sc];
	int pi = ( code - 1 ) / BOARD_NPEGS;
	int pg = ( code - 1 ) % BOARD_NPEGS;

	b->p[pi].peg[pg].r = dr;
	b->p[pi].peg[pg].c = dc;
	b->occ[dr][dc] = code;
	b->occ[sr][sc] = 0;
}

/**
  * @brief  Award a point, then move a peg (sub_172B7).
  * @param  b Board state.
  * @param  pi Player index.
  * @param  pg Peg index.
  * @param  dr Destination row.
  * @param  dc Destination column.
  * @retval None.
  */
static void board_award_and_move( board_state *b, int pi, int pg, int dr,
                                  int dc )
{
	board_gain( b, pi, pg );
	board_move_record( b, b->p[pi].peg[pg].r, b->p[pi].peg[pg].c, dr, dc );
}

/**
  * @brief  Recolour a tile to the owner of a peg standing on a tile (sub_164B3).
  * @param  b Board state.
  * @param  r Tile row.
  * @param  c Tile column.
  * @param  pi Player index whose colour the tile takes.
  * @retval None.
  */
static void board_set_owner( board_state *b, int r, int c, int pi )
{
	b->terrain[r][c] = BOARD_FLAG_BASE + 1 + pi;
}

/**
  * @brief  Place a category tile on a random unassigned cell (sub_16932).
  * @param  b Board state.
  * @param  tmp Construction map.
  * @param  type Category 1..9.
  * @retval None.
  */
static void board_place_category( board_state *b, int tmp[][BOARD_MAX_SIZE],
                                  int type )
{
	int total = b->n * b->n;
	int v = board_rand( b, total );

	for ( ;; )
	{
		int r = v / b->n, c = v % b->n;

		if ( tmp[r][c] >= BOARD_TMP_RESERVED )
		{
			tmp[r][c] = type;
			return;
		}
		v = ( v + 1 ) % total;
	}
}

/**
  * @brief  Set up flag and starting pegs of a player (sub_16715).
  * @param  b Board state.
  * @param  tmp Construction map.
  * @param  pi Player index.
  * @retval None.
  */
static void board_setup_player( board_state *b, int tmp[][BOARD_MAX_SIZE],
                                int pi )
{
	static const int corner[BOARD_NPLAYERS][2] = { { 0, 0 }, { 1, 1 }, { 0, 1 },
	                                               { 1, 0 } };
	int              last = b->n - 1;
	int              fr = corner[pi][0] ? last : 0;
	int              fc = corner[pi][1] ? last : 0;
	int              dy = fr == 0 ? 1 : -1;
	int              dx = fc == 0 ? 1 : -1;
	board_player    *pl = &b->p[pi];

	pl->active = 1;
	pl->pegs_left = BOARD_NPEGS;
	pl->home_r = fr;
	pl->home_c = fc;
	pl->sel = 0;
	tmp[fr][fc] = BOARD_FLAG_BASE + 1 + pi;
	board_place_peg( b, pi, 0, fr + 2 * dy, fc );
	board_place_peg( b, pi, 1, fr + dy, fc );
	board_place_peg( b, pi, 2, fr + dy, fc + dx );
	board_place_peg( b, pi, 3, fr, fc + dx );
	board_place_peg( b, pi, 4, fr, fc + 2 * dx );
	tmp[fr + 2 * dy][fc + dx] = BOARD_TMP_RESERVED;
	tmp[fr + dy][fc + 2 * dx] = BOARD_TMP_RESERVED;
}

//------------------------------------------------------------------------------
//--------------------------------- Public API ---------------------------------
//------------------------------------------------------------------------------

int board_rand( board_state *b, int m )
{
	uint32_t x = b->rng ? b->rng : 0x9e3779b9u;

	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	b->rng = x;
	return m > 1 ? (int)( x % (uint32_t)m ) : 0;
}

int board_rank( int score )
{
	int rank = score / BOARD_SCORE_PER_RANK;

	return rank > BOARD_MAX_RANK ? BOARD_MAX_RANK : ( rank < 0 ? 0 : rank );
}

void board_init( board_state *b, int n, int nplayers, uint32_t seed )
{
	int tmp[BOARD_MAX_SIZE][BOARD_MAX_SIZE];
	int r, c, i, rounds, rest, holes;

	memset( b, 0, sizeof *b );
	if ( n != 6 && n != 8 && n != 10 )
	{
		n = 8;
	}
	nplayers = nplayers < 2 ? 2 : ( nplayers > 4 ? 4 : nplayers );
	b->n = n;
	b->nplayers = nplayers;
	b->turn = 0;
	b->winner = -1;
	b->rng = seed ? seed : 1u;
	for ( r = 0; r < n; r++ )
	{
		for ( c = 0; c < n; c++ )
		{
			tmp[r][c] = BOARD_TMP_FREE;
		}
	}
	for ( i = 0; i < nplayers; i++ )
	{
		board_setup_player( b, tmp, i );
	}
	// sub_16887: holes only on unassigned cells that carry no peg.
	for ( holes = nplayers; holes < n; holes++ )
	{
		for ( ;; )
		{
			int v = board_rand( b, n * n );

			r = v / n;
			c = v % n;
			if ( tmp[r][c] == BOARD_TMP_FREE && b->occ[r][c] == 0 )
			{
				tmp[r][c] = BOARD_HOLE;
				break;
			}
		}
	}
	// Pegs stand on tiles that are ordinary category tiles.
	for ( r = 0; r < n; r++ )
	{
		for ( c = 0; c < n; c++ )
		{
			if ( tmp[r][c] == BOARD_TMP_FREE && b->occ[r][c] != 0 )
			{
				tmp[r][c] = BOARD_TMP_RESERVED;  // Assignable, not a hole.
			}
		}
	}
	rounds = n * ( n - 1 ) / 9;
	rest = n * ( n - 1 ) % 9;
	for ( i = 0; i < rounds; i++ )
	{
		int t;

		for ( t = 1; t <= 9; t++ )
		{
			board_place_category( b, tmp, t );
		}
	}
	for ( i = 0; i < rest; i++ )
	{
		board_place_category( b, tmp, BOARD_CAT_RANDOM );
	}
	for ( r = 0; r < n; r++ )
	{
		for ( c = 0; c < n; c++ )
		{
			b->terrain[r][c] = tmp[r][c] >= BOARD_TMP_RESERVED ? BOARD_CAT_RANDOM
			                                                   : tmp[r][c];
		}
	}
}

int board_occupant( const board_state *b, int r, int c, int *pi, int *pg )
{
	int code = board_occ( b, r, c );

	if ( code == 0 )
	{
		return 0;
	}
	if ( pi )
	{
		*pi = ( code - 1 ) / BOARD_NPEGS;
	}
	if ( pg )
	{
		*pg = ( code - 1 ) % BOARD_NPEGS;
	}
	return 1;
}

int board_target_kind( const board_state *b, int pi, int pg, int r, int c )
{
	const board_peg *p = &b->p[pi].peg[pg];
	int              dr = r - p->r, dc = c - p->c, t, oi;

	if ( !p->alive || !board_inside( b, r, c ) )
	{
		return BOARD_TARGET_NONE;
	}
	if ( ( dr < 0 ? -dr : dr ) + ( dc < 0 ? -dc : dc ) != 1 )
	{
		return BOARD_TARGET_NONE;
	}
	t = b->terrain[r][c];
	// sub_16BAE: hole, own colour tile or own peg block the step.
	if ( t == BOARD_HOLE || t == BOARD_FLAG_BASE + 1 + pi )
	{
		return BOARD_TARGET_NONE;
	}
	if ( board_occupant( b, r, c, &oi, NULL ) )
	{
		return oi == pi ? BOARD_TARGET_NONE : BOARD_TARGET_BATTLE;
	}
	return t > BOARD_FLAG_BASE ? BOARD_TARGET_FLAG : BOARD_TARGET_NORMAL;
}

int board_peg_can_act( const board_state *b, int pi, int pg )
{
	const board_peg *p = &b->p[pi].peg[pg];
	int              d;

	if ( !p->alive || p->attempts < 1 || p->attempts > 3 )
	{
		return 0;
	}
	for ( d = 0; d < 4; d++ )
	{
		if ( board_target_kind( b, pi, pg, p->r + board_dir[d][0],
		                        p->c + board_dir[d][1] ) != BOARD_TARGET_NONE )
		{
			return 1;
		}
	}
	return 0;
}

int board_player_can_act( const board_state *b, int pi )
{
	int k;

	if ( !b->p[pi].active || b->p[pi].pegs_left == 0 )
	{
		return 0;
	}
	for ( k = 0; k < BOARD_NPEGS; k++ )
	{
		if ( board_peg_can_act( b, pi, k ) )
		{
			return 1;
		}
	}
	return 0;
}

int board_next_peg( const board_state *b, int pi, int from, int dir )
{
	int i, k = from;

	for ( i = 0; i < BOARD_NPEGS; i++ )
	{
		k = ( k + dir + BOARD_NPEGS ) % BOARD_NPEGS;
		if ( board_peg_can_act( b, pi, k ) )
		{
			return k;
		}
	}
	return board_peg_can_act( b, pi, from ) ? from : -1;
}

int board_owns_free_flag( const board_state *b, int pi )
{
	int i;

	// sub_16F98: some home tile has the player's colour and no peg on it.
	for ( i = 0; i < b->nplayers; i++ )
	{
		int r = b->p[i].home_r, c = b->p[i].home_c;

		if ( b->terrain[r][c] == BOARD_FLAG_BASE + 1 + pi && b->occ[r][c] == 0 )
		{
			return 1;
		}
	}
	return 0;
}

int board_step( board_state *b, int pi, int pg, int r, int c, int correct )
{
	board_peg *p = &b->p[pi].peg[pg];
	int        out = BOARD_OUT_NONE;

	// sub_179AE: the attempt is spent whatever the answer.
	if ( correct )
	{
		board_award_and_move( b, pi, pg, r, c );
		out = BOARD_OUT_MOVED;
	}
	p->attempts--;
	b->p[pi].sel = pg;
	return out;
}

int board_flag_attack( board_state *b, int pi, int pg, int r, int c,
                       int correct )
{
	if ( !correct )
	{
		return BOARD_OUT_NONE;
	}
	board_set_owner( b, r, c, pi );  // sub_172DF: recolour, then move.
	board_award_and_move( b, pi, pg, r, c );
	return BOARD_OUT_MOVED;
}

int board_battle( board_state *b, int pi, int pg, int r, int c, int q1, int q2 )
{
	board_peg *a = &b->p[pi].peg[pg];
	int        di = 0, dg = 0, ar = a->r, ac = a->c;
	int        dr = r - ar, dc = c - ac;
	int        vr, vc, lr, lc;
	board_peg *d;

	// sub_17432: no penalty if question one fails, -1 if only two fails.
	if ( !q1 )
	{
		return BOARD_OUT_LOST;
	}
	if ( !q2 )
	{
		if ( a->score > 0 )
		{
			a->score--;
		}
		return BOARD_OUT_LOST;
	}
	board_gain( b, pi, pg );
	board_occupant( b, r, c, &di, &dg );
	d = &b->p[di].peg[dg];
	d->score -= BOARD_SCORE_PER_RANK;
	if ( d->score < 0 )
	{
		board_kill( b, di, dg );
		board_award_and_move( b, pi, pg, r, c );
		if ( b->terrain[r][c] > BOARD_FLAG_BASE )
		{
			board_set_owner( b, r, c, pi );
		}
		return BOARD_OUT_WON;
	}
	if ( b->terrain[r][c] > BOARD_FLAG_BASE )
	{
		board_set_owner( b, r, c, pi );
	}
	// Push the defender, and any chain of pegs behind it, one tile along the
	// attack line; the last peg of the chain dies if the next tile is a hole.
	vr = r;
	vc = c;
	do
	{
		vr += dr;
		vc += dc;
	} while ( board_occ( b, vr, vc ) != 0 );
	lr = vr - dr;
	lc = vc - dc;
	if ( board_terrain( b, vr, vc ) == BOARD_HOLE )
	{
		int code = b->occ[lr][lc];

		board_kill( b, ( code - 1 ) / BOARD_NPEGS, ( code - 1 ) % BOARD_NPEGS );
	}
	else
	{
		int code = b->occ[lr][lc];

		if ( b->terrain[vr][vc] > BOARD_FLAG_BASE )
		{
			board_set_owner( b, vr, vc, ( code - 1 ) / BOARD_NPEGS );
		}
		board_move_record( b, lr, lc, vr, vc );
	}
	while ( lr != r || lc != c )
	{
		int pr = lr - dr, pc = lc - dc;
		int code = b->occ[pr][pc];

		if ( b->terrain[lr][lc] > BOARD_FLAG_BASE )
		{
			board_set_owner( b, lr, lc, ( code - 1 ) / BOARD_NPEGS );
		}
		board_move_record( b, pr, pc, lr, lc );
		lr = pr;
		lc = pc;
	}
	board_award_and_move( b, pi, pg, r, c );
	return BOARD_OUT_WON;
}

void board_end_turn( board_state *b )
{
	int tries, i, k, total;

	if ( b->winner != -1 )
	{
		return;
	}
	for ( tries = 0; tries < b->nplayers * 2; tries++ )
	{
		b->turn = ( b->turn + 1 ) % b->nplayers;
		// sub_16E74: every peg gets rank + 1 attempts at the start of a turn.
		for ( k = 0; k < BOARD_NPEGS; k++ )
		{
			b->p[b->turn].peg[k].attempts =
			    board_rank( b->p[b->turn].peg[k].score ) + 1;
		}
		total = 0;
		for ( i = 0; i < b->nplayers; i++ )
		{
			total += b->p[i].pegs_left;
		}
		if ( b->p[b->turn].pegs_left == total )
		{
			b->winner = b->turn;
			return;
		}
		if ( board_player_can_act( b, b->turn ) )
		{
			return;
		}
	}
	b->winner = -2;  // Nobody can move.
}

int board_leader( const board_state *b )
{
	int i, best = -1, bi = -1;

	for ( i = 0; i < b->nplayers; i++ )
	{
		int k, sum = 0;

		for ( k = 0; k < BOARD_NPEGS; k++ )
		{
			sum += b->p[i].peg[k].alive ? b->p[i].peg[k].score : 0;
		}
		sum += b->p[i].pegs_left * 1000;
		if ( b->p[i].pegs_left > 0 && sum > best )
		{
			best = sum;
			bi = i;
		}
	}
	return bi;
}
