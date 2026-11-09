//  Description:
//
//		test_game.c -- headless tests of the game flow (game.c).
//
//		Drives sg_move / sg_answer / sg_tick with a fake clock, so question
//		counting, the battle sequence, turn passing, timeouts, selection,
//		snapshots and the save file are checked without a window. Rule
//		numbers refer to docs/RULES.md. Run from the src folder (it reads
//		questions/en).
//
//------------------------------------------------------------------------------
//---------------------------------- Includes ----------------------------------
//------------------------------------------------------------------------------

#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//------------------------------------------------------------------------------
//------------------------------ Local variables -------------------------------
//------------------------------------------------------------------------------

static int       test_failed, test_total;
static quiz_bank test_quiz;

//------------------------------------------------------------------------------
//------------------------------- Local functions ------------------------------
//------------------------------------------------------------------------------

/**
  * @brief  Report one check.
  * @param  ok Nonzero when the check passed.
  * @param  name Description.
  * @retval None.
  */
static void test_check( int ok, const char *name )
{
	test_total++;
	printf( "%s: %s\n", ok ? "PASS" : "FAIL", name );
	test_failed += !ok;
}

/**
  * @brief  Empty an 8x8 two-player game down to an arena without pegs.
  * @param  g Game to rebuild.
  * @retval None.
  */
static void test_arena( sg_game *g )
{
	board_state *b = &g->board;
	int          r, c, p, k;

	sg_start( g, &test_quiz, 8, 2, NULL, 15, 0, 7, 1 );
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
  * @param  g Game.
  * @param  p Player.
  * @param  k Peg index.
  * @param  r Row.
  * @param  c Column.
  * @param  score Peg score.
  * @retval None.
  */
static void test_place( sg_game *g, int p, int k, int r, int c, int score )
{
	board_peg *pg = &g->board.p[p].peg[k];

	pg->alive = 1;
	pg->r = r;
	pg->c = c;
	pg->score = score;
	pg->attempts = board_rank( score ) + 1;
	g->board.occ[r][c] = 1 + p * BOARD_NPEGS + k;
	g->board.p[p].pegs_left++;
}

/**
  * @brief  Ask the pending question and answer it, then let the pause run.
  * @param  g Game.
  * @param  right Answer correctly when nonzero.
  * @param  now Clock, advanced by this function.
  * @retval None.
  */
static void test_answer( sg_game *g, int right, uint32_t *now )
{
	int slot = right ? g->correct_slot : ( g->correct_slot + 1 ) % 4;

	sg_answer( g, g->board.turn, slot, *now );
	*now += SG_RESULT_MS - 1;
	sg_tick( g, *now );
	*now += 1;
	sg_tick( g, *now );
}

/**
  * @brief  Test the plain step (R-40) and its question count.
  * @retval None.
  */
static void test_step( void )
{
	sg_game  g;
	uint32_t now = 1000;
	int      rc;

	test_arena( &g );
	test_place( &g, 0, 0, 3, 3, 0 );
	test_place( &g, 0, 1, 5, 5, 0 );
	test_place( &g, 1, 0, 7, 6, 0 );
	test_check( sg_move( &g, 1, 0, 3, 4, now ) == -1,
	            "R-33: only the player on turn may move" );
	test_check( sg_move( &g, 0, 0, 3, 5, now ) == -2,
	            "R-32: a tile two steps away is refused" );
	rc = sg_move( &g, 0, 0, 3, 4, now );
	test_check( rc == 0 && g.scene == SG_SCENE_QUESTION && g.qnum == 1,
	            "R-40: a step asks one question" );
	test_check( sg_move( &g, 0, 1, 5, 6, now ) == -1,
	            "no second move while a question is open" );
	test_answer( &g, 1, &now );
	test_check( g.board.p[0].peg[0].r == 3 && g.board.p[0].peg[0].c == 4 &&
	                g.board.p[0].peg[0].score == 1,
	            "R-40: a right answer moves the peg and scores a point" );
	test_check( g.scene == SG_SCENE_BOARD && g.board.turn == 0,
	            "turn continues while a peg can still act" );
	test_check( g.board.p[0].peg[0].attempts == 0,
	            "the step used the peg's attempt" );
	test_check( sg_move( &g, 0, 0, 3, 5, now ) == -2,
	            "R-32: a peg without attempts cannot act" );
	rc = sg_move( &g, 0, 1, 5, 6, now );
	test_answer( &g, 0, &now );
	test_check( g.board.p[0].peg[1].c == 5 && g.board.p[0].peg[1].score == 0,
	            "R-40: a wrong answer leaves the peg, no penalty" );
	test_check( g.board.turn == 1 && g.scene == SG_SCENE_BOARD,
	            "R-33: turn ends when no peg of the player can act" );
	test_check( g.board.p[1].peg[0].attempts == 1,
	            "R-31: attempts are refilled at the start of a turn" );
}

/**
  * @brief  Test the flag attack (R-42) and the pass key (R-33).
  * @retval None.
  */
static void test_flag_pass( void )
{
	sg_game  g;
	uint32_t now = 1000;

	test_arena( &g );
	test_place( &g, 0, 0, 7, 5, 0 );
	test_place( &g, 0, 1, 2, 2, 0 );
	test_place( &g, 1, 0, 0, 3, 0 );
	test_check( sg_move( &g, 0, 0, 7, 6, now ) == 0 &&
	                g.kind == BOARD_TARGET_NORMAL,
	            "setup: tile next to the enemy flag is ordinary" );
	test_answer( &g, 0, &now );
	test_arena( &g );
	test_place( &g, 0, 0, 7, 6, 0 );
	test_place( &g, 0, 1, 2, 2, 0 );
	test_place( &g, 1, 0, 0, 3, 0 );
	test_check( sg_move( &g, 0, 0, 7, 7, now ) == 0 &&
	                g.kind == BOARD_TARGET_FLAG && g.qnum == 1,
	            "R-42: entering an enemy flag asks one question" );
	test_answer( &g, 1, &now );
	test_check( g.board.terrain[7][7] == 11 && g.board.p[0].peg[0].c == 7,
	            "R-42: the flag is recoloured and the peg moves in" );
	test_check( g.board.turn == 1, "R-33: the turn ends after a flag attack" );
	test_check( sg_pass( &g, 0, now ) == -1, "pass by the wrong player refused" );
	test_check( sg_pass( &g, 1, now ) == 0 && g.board.turn == 0,
	            "R-33: Space passes the turn" );
}

/**
  * @brief  Test the two-question battle (R-43).
  * @retval None.
  */
static void test_battle( void )
{
	sg_game  g;
	uint32_t now = 1000;
	int      before;

	// First question wrong: nothing happens, turn ends, one question only.
	test_arena( &g );
	test_place( &g, 0, 0, 3, 3, 4 );
	test_place( &g, 1, 0, 3, 4, 7 );
	test_check( sg_move( &g, 0, 0, 3, 4, now ) == 0 &&
	                g.kind == BOARD_TARGET_BATTLE,
	            "R-43: moving onto an enemy peg is a battle" );
	test_answer( &g, 0, &now );
	test_check( g.scene == SG_SCENE_BOARD && g.board.turn == 1 &&
	                g.board.p[0].peg[0].score == 4 &&
	                g.board.p[1].peg[0].score == 7,
	            "R-43: first question wrong ends the turn, no change" );

	// First right, second wrong: attacker loses a point.
	test_arena( &g );
	test_place( &g, 0, 0, 3, 3, 4 );
	test_place( &g, 1, 0, 3, 4, 7 );
	sg_move( &g, 0, 0, 3, 4, now );
	before = (int)g.qindex;
	test_answer( &g, 1, &now );
	test_check( g.scene == SG_SCENE_QUESTION && g.qnum == 2 &&
	                (int)g.qindex != before,
	            "R-43: a right first answer brings a second question" );
	test_answer( &g, 0, &now );
	test_check( g.board.p[0].peg[0].score == 3 && g.board.turn == 1 &&
	                g.board.p[0].peg[0].c == 3,
	            "R-43: second question wrong costs the attacker a point" );

	// Both right: defender -5 and pushed, attacker advances.
	test_arena( &g );
	test_place( &g, 0, 0, 3, 3, 4 );
	test_place( &g, 1, 0, 3, 4, 7 );
	sg_move( &g, 0, 0, 3, 4, now );
	test_answer( &g, 1, &now );
	test_answer( &g, 1, &now );
	test_check( g.board.p[1].peg[0].alive && g.board.p[1].peg[0].score == 2 &&
	                g.board.p[1].peg[0].c == 5,
	            "R-43: defender loses 5 and is pushed one tile" );
	test_check( g.board.p[0].peg[0].c == 4 && g.board.p[0].peg[0].score == 6 &&
	                g.board.turn == 1,
	            "R-43: attacker advances (+2) and the turn ends" );

	// Defender with a low score dies.
	test_arena( &g );
	test_place( &g, 0, 0, 3, 3, 0 );
	test_place( &g, 1, 0, 3, 4, 3 );
	test_place( &g, 1, 1, 6, 6, 0 );
	sg_move( &g, 0, 0, 3, 4, now );
	test_answer( &g, 1, &now );
	test_answer( &g, 1, &now );
	test_check( !g.board.p[1].peg[0].alive && g.board.p[1].pegs_left == 1,
	            "R-43: a defender below zero is killed" );
	test_check( sg_event( &g ) != SG_EV_NONE, "events are queued for sound" );
}

/**
  * @brief  Test the timer (R-45), tick and result pause.
  * @retval None.
  */
static void test_timer( void )
{
	sg_game  g;
	uint32_t now = 5000;

	test_arena( &g );
	test_place( &g, 0, 0, 3, 3, 0 );
	test_place( &g, 1, 0, 6, 6, 0 );
	sg_move( &g, 0, 0, 3, 4, now );
	test_check( sg_remaining_ms( &g, now ) == 15000, "answer time is 15 s" );
	test_check( sg_remaining_ms( &g, now + 4000 ) == 11000,
	            "remaining time counts down" );
	g.hilite = g.correct_slot;
	sg_tick( &g, now + 14999 );
	test_check( !g.answered, "no timeout before the time is up" );
	sg_tick( &g, now + 15000 );
	test_check( g.answered && g.was_correct && g.picked == g.correct_slot,
	            "R-45: timeout takes the highlighted answer" );
	sg_tick( &g, now + 15000 + SG_RESULT_MS - 1 );
	test_check( g.scene == SG_SCENE_QUESTION,
	            "R-46: result stays visible for about a second" );
	sg_tick( &g, now + 15000 + SG_RESULT_MS );
	test_check( g.scene == SG_SCENE_BOARD, "result pause ends" );

	// A remote player's timeout is wrong, after a grace period.
	test_arena( &g );
	g.local_mask = 2;
	test_place( &g, 0, 0, 3, 3, 0 );
	test_place( &g, 1, 0, 6, 6, 0 );
	sg_move( &g, 0, 0, 3, 4, now );
	sg_tick( &g, now + 15000 );
	test_check( !g.answered, "remote players get a grace period" );
	sg_tick( &g, now + 15000 + SG_GRACE_MS );
	test_check( g.answered && !g.was_correct && g.picked == -1,
	            "remote timeout counts as wrong" );
}

/**
  * @brief  Test skipping and the winner (R-31, R-50).
  * @retval None.
  */
static void test_turns( void )
{
	sg_game  g;
	uint32_t now = 1000;

	sg_start( &g, &test_quiz, 8, 3, NULL, 15, 0, 3, 7 );
	// Player 1 is boxed in by holes, so the turn must skip it.
	{
		board_state *b = &g.board;
		int          k, d;

		for ( k = 0; k < BOARD_NPEGS; k++ )
		{
			if ( b->p[1].peg[k].alive )
			{
				for ( d = 0; d < 4; d++ )
				{
					static const int dr[4] = { -1, 1, 0, 0 };
					static const int dc[4] = { 0, 0, -1, 1 };
					int r = b->p[1].peg[k].r + dr[d];
					int c = b->p[1].peg[k].c + dc[d];

					if ( r >= 0 && r < b->n && c >= 0 && c < b->n &&
					     !b->occ[r][c] )
					{
						b->terrain[r][c] = BOARD_HOLE;
					}
				}
			}
		}
	}
	test_check( sg_pass( &g, 0, now ) == 0, "player 0 passes" );
	test_check( g.board.turn == 2 && g.skipped[1],
	            "R-31: a player without legal moves is skipped" );

	test_arena( &g );
	test_place( &g, 0, 0, 3, 3, 0 );
	test_place( &g, 1, 0, 3, 4, 0 );
	g.board.p[1].peg[0].alive = 0;
	g.board.p[1].pegs_left = 0;
	g.board.occ[3][4] = 0;
	test_check( sg_pass( &g, 0, now ) == 0, "pass in a one-army position" );
	test_check( g.scene == SG_SCENE_OVER && g.winner == 0,
	            "R-50: the last army standing wins" );
	test_check( sg_move( &g, 0, 0, 3, 4, now ) == -1,
	            "no moves after the game is over" );
}

/**
  * @brief  Test question selection (R-44): no repeats within a category.
  * @retval None.
  */
static void test_questions( void )
{
	static unsigned char seen[10000];
	long                 i, idx, n = 0;
	int                  dup = 0;

	quiz_reset_used( &test_quiz );
	for ( i = 0; i < 125; i++ )
	{
		idx = quiz_pick_unused( &test_quiz, 3 );
		if ( idx < 0 || idx >= 10000 || seen[idx] )
		{
			dup++;
		}
		else
		{
			seen[idx] = 1;
			n++;
		}
	}
	test_check( dup == 0 && n == 125,
	            "R-44: 125 picks of a category are all different" );
	test_check( quiz_pick_unused( &test_quiz, 3 ) >= 0,
	            "R-44: an exhausted category resets" );
}

/**
  * @brief  Draw every question of one bank: each is asked exactly once.
  *
  * The picks run over the whole bank, so a bank larger than the original
  * 1000 English questions (the Croatian one has 1945) is not cut short.
  *
  * @param  dir Question directory.
  * @param  language Bank language.
  * @param  expected Number of questions the bank must hold.
  * @param  name Bank name for the messages.
  * @retval None.
  */
static void test_whole_bank( const char *dir, quiz_language language,
                             long expected, const char *name )
{
	quiz_bank      qz = { 0 };
	unsigned char *seen;
	long           lo, hi, i, idx, wrong = 0, count = 0;
	int            cat;

	if ( quiz_load( &qz, dir, language ) != 0 )
	{
		test_check( 0, "the question bank loads" );
		return;
	}
	test_check( qz.total == expected, name );
	seen = calloc( (size_t)qz.total, 1 );
	test_check( seen != NULL, "memory for the check" );
	if ( !seen )
	{
		quiz_free( &qz );
		return;
	}
	quiz_reset_used( &qz );
	for ( cat = 1; cat <= QUIZ_NCAT; cat++ )
	{
		lo = cat == 1 ? 0 : (long)qz.bound[cat - 2];
		hi = (long)qz.bound[cat - 1];
		for ( i = lo; i < hi; i++ )
		{
			idx = quiz_pick_unused( &qz, cat );
			if ( idx < lo || idx >= hi || seen[idx] )
			{
				wrong++;
			}
			else
			{
				seen[idx] = 1;
				count++;
			}
		}
	}
	test_check( wrong == 0 && count == qz.total,
	            "every question of every category is asked once" );
	free( seen );
	quiz_free( &qz );
}

/**
  * @brief  Test that the whole English and Croatian banks can be used.
  * @retval None.
  */
static void test_banks( void )
{
	test_whole_bank( "questions/en", QUIZ_LANGUAGE_EN, 1000,
	                 "the English bank holds 1000 questions" );
	test_whole_bank( "questions/cro", QUIZ_LANGUAGE_CRO, 1945,
	                 "the Croatian bank holds 1945 questions" );
}

/**
  * @brief  Test the selection helpers (R-34).
  * @retval None.
  */
static void test_selection( void )
{
	sg_game g;
	sg_sel  s;
	int     peg = -1, r = -1, c = -1, k, before;

	test_arena( &g );
	test_place( &g, 0, 0, 3, 3, 0 );
	test_place( &g, 0, 1, 5, 5, 0 );
	test_place( &g, 1, 0, 6, 6, 0 );
	sgs_reset( &s, &g.board, 0 );
	test_check( s.peg == -1 && s.cpeg >= 0, "a peg is highlighted at the start" );
	before = s.cpeg;
	sgs_arrow( &s, &g.board, 0, 0, 1 );
	test_check( s.cpeg != before, "R-34: arrows cycle the pegs that can act" );
	sgs_tab( &s, &g.board, 0, -1 );
	test_check( s.cpeg == before, "Tab cycles back" );
	test_check( sgs_confirm( &s, &g.board, 0, &peg, &r, &c ) == 0 &&
	                s.peg == before,
	            "Enter selects the highlighted peg" );
	k = s.peg;
	sgs_arrow( &s, &g.board, 0, -1, 0 );
	test_check( s.cr == g.board.p[0].peg[k].r - 1, "an arrow moves the cursor" );
	sgs_arrow( &s, &g.board, 0, 1, 0 );
	test_check( s.cr == g.board.p[0].peg[k].r &&
	                s.cc == g.board.p[0].peg[k].c,
	            "the opposite arrow returns to the peg" );
	test_check( sgs_confirm( &s, &g.board, 0, &peg, &r, &c ) == 0 && s.peg == -1,
	            "R-34: Enter on the peg cancels" );
	sgs_click( &s, &g.board, 0, 3, 3, &peg );
	test_check( s.peg == 0, "click selects an own peg" );
	test_check( sgs_click( &s, &g.board, 0, 3, 4, &peg ) == 1 && peg == 0,
	            "click on a legal neighbour confirms a move" );
	test_check( sgs_click( &s, &g.board, 0, 0, 0, &peg ) == 0,
	            "click on a far tile does nothing" );
}

/**
  * @brief  Test LAN snapshots round trip into a guest mirror.
  * @retval None.
  */
static void test_snapshot( void )
{
	sg_game      g, guest;
	lan_snapshot s;
	uint32_t     now = 10000;

	test_arena( &g );
	test_place( &g, 0, 0, 3, 3, 0 );
	test_place( &g, 1, 0, 6, 6, 0 );
	sg_move( &g, 0, 0, 3, 4, now );
	sg_snapshot( &g, NULL, 2, now + 3000, &s );
	memset( &guest, 0, sizeof guest );
	guest.scene = SG_SCENE_LOBBY;
	sg_apply( &guest, &s, now + 3000 );
	test_check( guest.scene == SG_SCENE_QUESTION &&
	                strcmp( guest.cur.q, g.cur.q ) == 0 &&
	                guest.correct_slot == -1 &&
	                guest.tr == 3 && guest.tc == 4,
	            "guest mirrors the question and the target" );
	test_check( sg_remaining_ms( &guest, now + 3000 ) == 12000,
	            "guest timer follows the host remaining time" );
	test_check( s.correct == -1, "the correct slot is hidden before the answer" );
	test_check( sg_answer( &guest, 0, 0, now ) == -1 &&
	                sg_move( &guest, 0, 0, 3, 4, now ) == -1,
	            "a guest mirror never advances itself" );
}

/**
  * @brief  Test the save file.
  * @retval None.
  */
static void test_save( void )
{
	sg_game g, h;
	uint32_t now = 1000;

	sg_start( &g, &test_quiz, 10, 3, NULL, 30, 1, 11, 7 );
	sg_pass( &g, 0, now );
	test_check( sg_save( &g, "build/test_save.bin" ) == 0, "save succeeds" );
	sg_start( &h, &test_quiz, 8, 2, NULL, 15, 0, 5, 3 );
	test_check( sg_load( &h, "build/test_save.bin" ) == 0, "load succeeds" );
	test_check( h.board.n == 10 && h.board.nplayers == 3 &&
	                h.board.turn == g.board.turn && h.timer_sec == 30 &&
	                h.language == 1 &&
	                memcmp( &h.board.terrain, &g.board.terrain,
	                        sizeof g.board.terrain ) == 0,
	            "load restores board and settings" );
	test_check( sg_load( &h, "build/does_not_exist.bin" ) == -1,
	            "a missing file is reported" );
}

//------------------------------------------------------------------------------
//------------------------------- Public API -----------------------------------
//------------------------------------------------------------------------------

/**
  * @brief  Run the flow tests.
  * @retval 0 if all passed, 1 otherwise.
  */
int main( void )
{
	int rc = quiz_load( &test_quiz, "questions/en", QUIZ_LANGUAGE_EN );

	if ( rc != 0 )
	{
		printf( "FAIL: cannot load questions/en (%d); run from src\n", rc );
		return 1;
	}
	test_step();
	test_flag_pass();
	test_battle();
	test_timer();
	test_turns();
	test_questions();
	test_banks();
	test_selection();
	test_snapshot();
	test_save();
	quiz_free( &test_quiz );
	printf( "%d checks, %d failed\n", test_total, test_failed );
	return test_failed ? 1 : 0;
}
