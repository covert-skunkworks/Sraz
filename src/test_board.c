//  Description:
//
//		test_board.c -- regression checks of the board rules against docs/RULES.md.
//
#include "board.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/**
  * @brief  Count tiles of a terrain code.
  * @param  b Board state.
  * @param  t Terrain code.
  * @retval Number of tiles.
  */
static int test_count( const board_state *b, int t )
{
	int r, c, n = 0;

	for ( r = 0; r < b->n; r++ )
	{
		for ( c = 0; c < b->n; c++ )
		{
			n += b->terrain[r][c] == t;
		}
	}
	return n;
}

/**
  * @brief  Clear a board to a plain 8x8 arena with two lone pegs for rule tests.
  * @param  b Board state.
  * @retval None.
  */
static void test_arena( board_state *b )
{
	int r, c, p, k;

	board_init( b, 8, 2, 7 );
	memset( b->occ, 0, sizeof b->occ );
	for ( r = 0; r < 8; r++ )
	{
		for ( c = 0; c < 8; c++ )
		{
			b->terrain[r][c] = 1;
		}
	}
	for ( p = 0; p < 2; p++ )
	{
		for ( k = 0; k < BOARD_NPEGS; k++ )
		{
			b->p[p].peg[k].alive = 0;
		}
		b->p[p].pegs_left = 0;
	}
	b->terrain[0][0] = 11;
	b->terrain[7][7] = 12;
}

/**
  * @brief  Put a peg on the arena.
  * @param  b Board state.
  * @param  p Player index.
  * @param  k Peg index.
  * @param  r Row.
  * @param  c Column.
  * @param  score Peg score.
  * @retval None.
  */
static void test_put( board_state *b, int p, int k, int r, int c, int score )
{
	board_peg *g = &b->p[p].peg[k];

	g->alive = 1;
	g->r = r;
	g->c = c;
	g->score = score;
	g->attempts = board_rank( score ) + 1;
	b->occ[r][c] = 1 + p * BOARD_NPEGS + k;
	b->p[p].pegs_left++;
}

int main( void )
{
	board_state b, a;
	int         n, np, cat, r, c, i, seed;

	for ( seed = 1; seed < 200; seed++ )
	{
		for ( n = 6; n <= 10; n += 2 )
		{
			for ( np = 2; np <= 4; np++ )
			{
				int holes, flags = 0, tiles = 0, pegs = 0;

				board_init( &b, n, np, (uint32_t)seed );
				holes = test_count( &b, BOARD_HOLE );
				assert( holes == n - np );
				for ( i = 0; i < np; i++ )
				{
					assert( b.terrain[b.p[i].home_r][b.p[i].home_c] == 11 + i );
					assert( b.occ[b.p[i].home_r][b.p[i].home_c] == 0 );
					flags++;
					pegs += b.p[i].pegs_left;
				}
				assert( pegs == np * 5 );
				for ( r = 0; r < n; r++ )
				{
					for ( c = 0; c < n; c++ )
					{
						if ( b.terrain[r][c] >= 1 && b.terrain[r][c] <= 9 )
						{
							tiles++;
						}
						if ( b.occ[r][c] )
						{
							assert( b.terrain[r][c] >= 1 && b.terrain[r][c] <= 9 );
						}
					}
				}
				assert( tiles == n * ( n - 1 ) );
				for ( cat = 1; cat <= 8; cat++ )
				{
					assert( test_count( &b, cat ) == n * ( n - 1 ) / 9 );
				}
				assert( test_count( &b, 9 ) ==
				        n * ( n - 1 ) / 9 + n * ( n - 1 ) % 9 );
			}
		}
	}
	board_init( &b, 8, 2, 3 );
	assert( b.p[0].home_r == 0 && b.p[0].home_c == 0 );
	assert( b.p[1].home_r == 7 && b.p[1].home_c == 7 );
	assert( b.p[0].peg[0].r == 2 && b.p[0].peg[0].c == 0 );
	assert( b.p[0].peg[4].r == 0 && b.p[0].peg[4].c == 2 );
	board_init( &b, 10, 4, 3 );
	assert( b.p[2].home_r == 0 && b.p[2].home_c == 9 );
	assert( b.p[3].home_r == 9 && b.p[3].home_c == 0 );
	assert( b.p[3].peg[0].r == 7 && b.p[3].peg[0].c == 0 );

	// Rank and attempts (R-30, R-31).
	assert( board_rank( 0 ) == 0 && board_rank( 4 ) == 0 && board_rank( 5 ) == 1 );
	assert( board_rank( 10 ) == 2 && board_rank( 14 ) == 2 );

	// Normal step (R-40): wrong answer keeps the peg and uses the attempt.
	test_arena( &a );
	test_put( &a, 0, 0, 3, 3, 0 );
	test_put( &a, 1, 0, 6, 6, 0 );
	assert( board_target_kind( &a, 0, 0, 3, 4 ) == BOARD_TARGET_NORMAL );
	assert( board_target_kind( &a, 0, 0, 4, 4 ) == BOARD_TARGET_NONE );
	assert( board_step( &a, 0, 0, 3, 4, 0 ) == BOARD_OUT_NONE );
	assert( a.p[0].peg[0].c == 3 && a.p[0].peg[0].attempts == 0 );
	assert( !board_peg_can_act( &a, 0, 0 ) );
	board_end_turn( &a );
	assert( a.turn == 1 && a.p[1].peg[0].attempts == 1 );
	board_end_turn( &a );
	assert( a.turn == 0 && a.p[0].peg[0].attempts == 1 );
	assert( board_step( &a, 0, 0, 3, 4, 1 ) == BOARD_OUT_MOVED );
	assert( a.p[0].peg[0].c == 4 && a.p[0].peg[0].score == 1 );

	// Score gate (R-41): no points once the own flag is occupied or lost.
	a.terrain[0][0] = 12;
	a.terrain[7][7] = 12;
	board_step( &a, 0, 0, 3, 5, 1 );
	assert( a.p[0].peg[0].score == 1 );

	// Own flag tile is not enterable, enemy flag is a flag target (R-42).
	test_arena( &a );
	test_put( &a, 0, 0, 0, 1, 0 );
	test_put( &a, 1, 0, 7, 6, 0 );
	assert( board_target_kind( &a, 0, 0, 0, 0 ) == BOARD_TARGET_NONE );
	assert( board_target_kind( &a, 1, 0, 7, 7 ) == BOARD_TARGET_NONE );
	test_arena( &a );
	test_put( &a, 0, 0, 7, 6, 0 );
	test_put( &a, 1, 0, 3, 3, 0 );
	assert( board_target_kind( &a, 0, 0, 7, 7 ) == BOARD_TARGET_FLAG );
	assert( board_flag_attack( &a, 0, 0, 7, 7, 0 ) == BOARD_OUT_NONE );
	assert( a.terrain[7][7] == 12 );
	assert( board_flag_attack( &a, 0, 0, 7, 7, 1 ) == BOARD_OUT_MOVED );
	assert( a.terrain[7][7] == 11 && a.p[0].peg[0].r == 7 && a.p[0].peg[0].c == 7 );
	assert( a.p[0].peg[0].score == 1 );

	// Battle (R-43): Q1 wrong no penalty, Q2 wrong -1.
	test_arena( &a );
	test_put( &a, 0, 0, 3, 3, 6 );
	test_put( &a, 1, 0, 3, 4, 7 );
	assert( board_target_kind( &a, 0, 0, 3, 4 ) == BOARD_TARGET_BATTLE );
	assert( board_battle( &a, 0, 0, 3, 4, 0, 0 ) == BOARD_OUT_LOST );
	assert( a.p[0].peg[0].score == 6 );
	assert( board_battle( &a, 0, 0, 3, 4, 1, 0 ) == BOARD_OUT_LOST );
	assert( a.p[0].peg[0].score == 5 );
	// Won battle: defender -5 and pushed, attacker +2 and advances.
	assert( board_battle( &a, 0, 0, 3, 4, 1, 1 ) == BOARD_OUT_WON );
	assert( a.p[0].peg[0].score == 7 && a.p[0].peg[0].c == 4 );
	assert( a.p[1].peg[0].score == 2 && a.p[1].peg[0].c == 5 );
	assert( a.occ[3][3] == 0 );
	// Defender with score < 5 dies.
	test_arena( &a );
	test_put( &a, 0, 0, 3, 3, 0 );
	test_put( &a, 1, 0, 3, 4, 4 );
	assert( board_battle( &a, 0, 0, 3, 4, 1, 1 ) == BOARD_OUT_WON );
	assert( !a.p[1].peg[0].alive && a.p[1].pegs_left == 0 );
	assert( a.p[0].peg[0].c == 4 && a.p[0].peg[0].score == 2 );
	// Chain push: A(3,3) hits B(3,4) with C(3,5) behind; C ends up on (3,6).
	test_arena( &a );
	test_put( &a, 0, 0, 3, 3, 0 );
	test_put( &a, 1, 0, 3, 4, 8 );
	test_put( &a, 1, 1, 3, 5, 0 );
	assert( board_battle( &a, 0, 0, 3, 4, 1, 1 ) == BOARD_OUT_WON );
	assert( a.p[1].peg[0].c == 5 && a.p[1].peg[1].c == 6 && a.p[0].peg[0].c == 4 );
	// Push into a hole destroys the last peg of the chain.
	test_arena( &a );
	test_put( &a, 0, 0, 3, 3, 0 );
	test_put( &a, 1, 0, 3, 4, 9 );
	a.terrain[3][5] = BOARD_HOLE;
	assert( board_battle( &a, 0, 0, 3, 4, 1, 1 ) == BOARD_OUT_WON );
	assert( !a.p[1].peg[0].alive && a.p[0].peg[0].c == 4 );

	// Win by elimination at the turn change (R-50).
	test_arena( &a );
	test_put( &a, 0, 0, 3, 3, 0 );
	test_put( &a, 1, 0, 6, 6, 0 );
	a.p[1].peg[0].alive = 0;
	a.p[1].pegs_left = 0;
	board_end_turn( &a );
	assert( a.winner == 0 );

	puts( "test_board: all checks passed" );
	return 0;
}
