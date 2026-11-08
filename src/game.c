//  Description:
//
//		game.c -- the Sraz game flow on top of the rules engine.
//
//		Flow of one action (R-33 .. R-46):
//
//		  board scene   select peg, select target      sg_move()
//		  question      one question, or for a battle   sg_ask()
//		                a second one if the first was
//		                right                           sg_answer()
//		  result pause  tick or cross, SG_RESULT_MS     sg_tick()
//		  resolution    board_step / flag_attack /      sg_resolve()
//		                battle, then continue or end
//		                the turn                        sg_end_turn()
//
//		Save file: "SRAZSAV1", then version, sizeof( board_state ), language,
//		timer, names and the board state itself, all in host byte order.
//

//------------------------------------------------------------------------------
//---------------------------------- Includes ----------------------------------
//------------------------------------------------------------------------------

#include "game.h"
#include <stdio.h>
#include <string.h>

//------------------------------------------------------------------------------
//------------------------- Local macros and constants -------------------------
//------------------------------------------------------------------------------

#define SG_SAVE_MAGIC "SRAZSAV1"
#define SG_SAVE_VERSION 1u

//------------------------------------------------------------------------------
//------------------------ Local function prototypes ---------------------------
//------------------------------------------------------------------------------

static void sg_push( sg_game *g, int ev );
static int  sg_ask( sg_game *g, uint32_t now );
static void sg_resolve( sg_game *g, uint32_t now );
static void sg_end_turn( sg_game *g );

//------------------------------------------------------------------------------
//------------------------------- Local functions ------------------------------
//------------------------------------------------------------------------------

/**
  * @brief  Queue an event, dropping the oldest when full.
  * @param  g Game.
  * @param  ev SG_EV_xxx.
  * @retval None.
  */
static void sg_push( sg_game *g, int ev )
{
	g->ev[g->ev_tail] = ev;
	g->ev_tail = ( g->ev_tail + 1 ) % SG_EV_MAX;
	if ( g->ev_tail == g->ev_head )
	{
		g->ev_head = ( g->ev_head + 1 ) % SG_EV_MAX;
	}
}

/**
  * @brief  Count all living pegs.
  * @param  b Board.
  * @retval Total number of pegs on the board.
  */
static int sg_total_pegs( const board_state *b )
{
	int i, t = 0;

	for ( i = 0; i < b->nplayers; i++ )
	{
		t += b->p[i].pegs_left;
	}
	return t;
}

/**
  * @brief  Pick a question for the pending target and open the timer (R-44).
  *
  * The category is that of the target tile; random tiles and flag tiles
  * draw one of the eight categories at random. quiz_pick_unused() makes sure
  * no question repeats until a category is exhausted.
  *
  * @param  g Game.
  * @param  now Time in milliseconds.
  * @retval 0 on success, -1 if no question could be picked.
  */
static int sg_ask( sg_game *g, uint32_t now )
{
	int  t = g->board.terrain[g->tr][g->tc], cat, i, c;
	long idx;

	if ( !g->quiz )
	{
		return -1;
	}
	cat = t >= 1 && t <= QUIZ_NCAT ? t : 1 + board_rand( &g->board, QUIZ_NCAT );
	idx = quiz_pick_unused( g->quiz, cat );
	if ( idx < 0 )
	{
		idx = quiz_pick_unused( g->quiz, 0 );
	}
	if ( idx < 0 )
	{
		return -1;
	}
	c = quiz_category_of( g->quiz, idx );
	g->category = c >= 0 ? c + 1 : cat;
	g->qindex = idx;
	g->correct_slot = quiz_shuffle( g->order );
	memcpy( g->cur.q, g->quiz->q[idx].q, sizeof g->cur.q );
	for ( i = 0; i < 4; i++ )
	{
		memcpy( g->cur.a[i], g->quiz->q[idx].a[g->order[i]],
		        sizeof g->cur.a[i] );
	}
	g->answered = 0;
	g->picked = -1;
	g->was_correct = 0;
	g->hilite = 0;
	g->q_start = now;
	g->scene = SG_SCENE_QUESTION;
	g->epoch++;
	return 0;
}

/**
  * @brief  Pass the turn on and detect the end of the game (R-31, R-50).
  * @param  g Game.
  * @retval None.
  */
static void sg_end_turn( sg_game *g )
{
	board_state *b = &g->board;
	int          prev = b->turn, i, any = 0;

	memset( g->skipped, 0, sizeof g->skipped );
	board_end_turn( b );
	for ( i = ( prev + 1 ) % b->nplayers; i != b->turn;
	      i = ( i + 1 ) % b->nplayers )
	{
		g->skipped[i] = 1;
		any = 1;
	}
	if ( any && b->winner == -1 )
	{
		sg_push( g, SG_EV_SKIP );
	}
	g->epoch++;
	g->peg = -1;
	if ( b->winner != -1 )
	{
		g->leader = board_leader( b );
		g->winner = b->winner >= 0 ? b->winner : g->leader;
		g->scene = SG_SCENE_OVER;
		sg_push( g, SG_EV_WIN );
		return;
	}
	g->scene = SG_SCENE_BOARD;
	sg_push( g, SG_EV_TURN );
}

/**
  * @brief  Apply an answered question to the board (R-40 .. R-43).
  * @param  g Game.
  * @param  now Time in milliseconds.
  * @retval None.
  */
static void sg_resolve( sg_game *g, uint32_t now )
{
	board_state *b = &g->board;
	int          pl = b->turn, before = sg_total_pegs( b ), end = 0;

	if ( g->kind == BOARD_TARGET_BATTLE )
	{
		if ( g->qnum == 1 && g->was_correct )
		{
			g->q1 = 1;
			g->qnum = 2;
			if ( sg_ask( g, now ) != 0 )
			{
				board_battle( b, pl, g->peg, g->tr, g->tc, 1, 0 );
				end = 1;
			}
			else
			{
				return;
			}
		}
		else if ( g->qnum == 1 )
		{
			board_battle( b, pl, g->peg, g->tr, g->tc, 0, 0 );
			end = 1;
		}
		else
		{
			board_battle( b, pl, g->peg, g->tr, g->tc, 1, g->was_correct );
			end = 1;
		}
	}
	else if ( g->kind == BOARD_TARGET_FLAG )
	{
		board_flag_attack( b, pl, g->peg, g->tr, g->tc, g->was_correct );
		end = 1;
	}
	else
	{
		if ( board_step( b, pl, g->peg, g->tr, g->tc, g->was_correct ) ==
		     BOARD_OUT_MOVED )
		{
			sg_push( g, SG_EV_MOVE );
		}
		end = !board_player_can_act( b, pl );
	}
	b->p[pl].sel = g->peg;
	if ( sg_total_pegs( b ) < before )
	{
		sg_push( g, SG_EV_LOST );
	}
	if ( end )
	{
		sg_end_turn( g );
	}
	else
	{
		g->scene = SG_SCENE_BOARD;
		g->peg = -1;
		g->epoch++;
	}
}

//------------------------------------------------------------------------------
//------------------------------- Public API -----------------------------------
//------------------------------------------------------------------------------

int sg_start( sg_game *g, quiz_bank *quiz, int n, int nplayers,
              const char names[][LAN_NAME_SIZE], int timer_sec, int language,
              uint32_t seed, int local_mask )
{
	int i;

	if ( !g || nplayers < 2 || nplayers > BOARD_NPLAYERS )
	{
		return -1;
	}
	memset( g, 0, sizeof *g );
	g->quiz = quiz;
	g->language = language == 1 ? 1 : 0;
	g->timer_sec = timer_sec;
	g->local_mask = local_mask;
	board_init( &g->board, n, nplayers, seed );
	for ( i = 0; i < BOARD_NPLAYERS; i++ )
	{
		if ( names && names[i][0] )
		{
			snprintf( g->names[i], sizeof g->names[i], "%s", names[i] );
		}
		else
		{
			snprintf( g->names[i], sizeof g->names[i], "%s %d",
			          g->language == 1 ? "Igrac" : "Player", i + 1 );
		}
	}
	if ( quiz && quiz->q )
	{
		quiz_reset_used( quiz );
	}
	g->scene = SG_SCENE_BOARD;
	g->winner = -1;
	g->leader = -1;
	g->peg = -1;
	g->qindex = -1;
	g->epoch = 1;
	return 0;
}

int sg_move( sg_game *g, int player, int peg, int r, int c, uint32_t now )
{
	board_state *b = &g->board;
	int          kind;

	if ( g->guest || g->scene != SG_SCENE_BOARD || b->winner != -1 ||
	     player != b->turn )
	{
		return -1;
	}
	if ( peg < 0 || peg >= BOARD_NPEGS || !board_peg_can_act( b, player, peg ) )
	{
		return -2;
	}
	kind = board_target_kind( b, player, peg, r, c );
	if ( kind == BOARD_TARGET_NONE )
	{
		return -2;
	}
	g->peg = peg;
	g->tr = r;
	g->tc = c;
	g->kind = kind;
	g->qnum = 1;
	g->q1 = 0;
	b->p[player].sel = peg;
	if ( sg_ask( g, now ) != 0 )
	{
		g->scene = SG_SCENE_BOARD;
		return -2;
	}
	return 0;
}

int sg_answer( sg_game *g, int player, int slot, uint32_t now )
{
	if ( g->guest || g->scene != SG_SCENE_QUESTION || g->answered ||
	     player != g->board.turn || slot < -1 || slot > 3 )
	{
		return -1;
	}
	g->picked = slot;
	g->was_correct = slot >= 0 && slot == g->correct_slot;
	g->answered = 1;
	g->res_start = now;
	g->epoch++;
	sg_push( g, g->was_correct ? SG_EV_CORRECT : SG_EV_WRONG );
	return 0;
}

int sg_pass( sg_game *g, int player, uint32_t now )
{
	( void )now;
	if ( g->guest || g->scene != SG_SCENE_BOARD || g->board.winner != -1 ||
	     player != g->board.turn )
	{
		return -1;
	}
	sg_end_turn( g );
	return 0;
}

void sg_tick( sg_game *g, uint32_t now )
{
	uint32_t limit;
	int      local;

	if ( g->guest || g->scene != SG_SCENE_QUESTION )
	{
		return;
	}
	if ( !g->answered )
	{
		local = ( g->local_mask >> g->board.turn ) & 1;
		limit = (uint32_t)g->timer_sec * 1000u + ( local ? 0u : SG_GRACE_MS );
		if ( now - g->q_start >= limit )
		{
			// R-45: on timeout the highlighted answer is taken. A remote
			// player's highlight is unknown to the host, so that is wrong.
			sg_answer( g, g->board.turn, local ? g->hilite : -1, now );
		}
	}
	else if ( now - g->res_start >= SG_RESULT_MS )
	{
		sg_resolve( g, now );
	}
}

uint32_t sg_remaining_ms( const sg_game *g, uint32_t now )
{
	uint32_t total = (uint32_t)g->timer_sec * 1000u, el;

	if ( g->scene != SG_SCENE_QUESTION || g->answered )
	{
		return 0;
	}
	el = now - g->q_start;
	return el < total ? total - el : 0;
}

int sg_event( sg_game *g )
{
	int ev;

	if ( g->ev_head == g->ev_tail )
	{
		return SG_EV_NONE;
	}
	ev = g->ev[g->ev_head];
	g->ev_head = ( g->ev_head + 1 ) % SG_EV_MAX;
	return ev;
}

void sg_snapshot( const sg_game *g, const sg_sel *sel, int nplayers,
                  uint32_t now, lan_snapshot *s )
{
	memset( s, 0, sizeof *s );
	s->board = g->board;
	s->epoch = g->epoch;
	s->scene = g->scene;
	s->language = g->language;
	s->nplayers = nplayers < 1 ? 1 : ( nplayers > BOARD_NPLAYERS
	                                       ? BOARD_NPLAYERS : nplayers );
	s->timer_sec = g->timer_sec;
	s->battle = g->kind == BOARD_TARGET_BATTLE;
	s->question_number = g->qnum;
	s->answered = g->answered;
	s->correct = g->answered ? g->correct_slot : -1;
	s->picked = g->picked;
	s->was_correct = g->was_correct;
	s->winner = g->winner;
	s->sel_peg = -1;
	s->target_r = s->target_c = -1;
	memcpy( s->names, g->names, sizeof s->names );
	if ( g->scene == SG_SCENE_QUESTION )
	{
		s->question = g->cur;
		s->category = g->category >= 1 ? g->category - 1 : 0;
		s->question_index = g->qindex > 0 ? (int)g->qindex : 0;
		s->remaining_ms = sg_remaining_ms( g, now );
		s->sel_peg = g->peg;
		s->target_r = g->tr;
		s->target_c = g->tc;
	}
	else if ( g->scene == SG_SCENE_BOARD && sel && sel->peg >= 0 )
	{
		s->sel_peg = sel->peg;
		s->target_r = sel->cr;
		s->target_c = sel->cc;
	}
}

void sg_apply( sg_game *g, const lan_snapshot *s, uint32_t now )
{
	int old_scene = g->scene, old_turn = g->board.turn, old_answered;

	old_answered = old_scene == SG_SCENE_QUESTION && g->answered;
	g->guest = 1;
	g->board = s->board;
	g->epoch = s->epoch;
	g->scene = s->scene;
	g->language = s->language;
	g->timer_sec = s->timer_sec;
	memcpy( g->names, s->names, sizeof g->names );
	g->qnum = s->question_number;
	g->answered = s->answered;
	g->correct_slot = s->correct;
	g->picked = s->picked;
	g->was_correct = s->was_correct;
	g->winner = s->winner;
	g->peg = s->sel_peg;
	g->tr = s->target_r;
	g->tc = s->target_c;
	g->category = s->category + 1;
	g->qindex = s->question_index;
	g->kind = BOARD_TARGET_NORMAL;
	if ( s->battle )
	{
		g->kind = BOARD_TARGET_BATTLE;
	}
	else if ( s->scene == SG_SCENE_QUESTION && s->target_r >= 0 &&
	          s->target_c >= 0 &&
	          s->board.terrain[s->target_r][s->target_c] > BOARD_FLAG_BASE )
	{
		g->kind = BOARD_TARGET_FLAG;
	}
	if ( s->scene == SG_SCENE_QUESTION )
	{
		g->cur = s->question;
		g->q_start = now - ( (uint32_t)s->timer_sec * 1000u - s->remaining_ms );
	}
	if ( s->scene == SG_SCENE_QUESTION && s->answered && !old_answered )
	{
		sg_push( g, s->was_correct ? SG_EV_CORRECT : SG_EV_WRONG );
	}
	else if ( s->scene == SG_SCENE_BOARD && old_scene != SG_SCENE_LOBBY &&
	          s->board.turn != old_turn )
	{
		sg_push( g, SG_EV_TURN );
	}
	else if ( s->scene == SG_SCENE_OVER && old_scene != SG_SCENE_OVER )
	{
		sg_push( g, SG_EV_WIN );
	}
}

//------------------------------------------------------------------------------
//------------------------------- Selection ------------------------------------
//------------------------------------------------------------------------------

void sgs_reset( sg_sel *s, const board_state *b, int player )
{
	int last = b->p[player].sel;

	s->peg = -1;
	s->cr = s->cc = -1;
	last = last < 0 || last >= BOARD_NPEGS ? 0 : last;
	s->cpeg = board_peg_can_act( b, player, last )
	              ? last
	              : board_next_peg( b, player, last, 1 );
}

void sgs_cancel( sg_sel *s )
{
	s->peg = -1;
	s->cr = s->cc = -1;
}

/**
  * @brief  Select a peg and put the cursor on its tile.
  * @param  s Selection.
  * @param  b Board.
  * @param  player Player on turn.
  * @param  peg Peg index.
  * @retval None.
  */
static void sgs_pick( sg_sel *s, const board_state *b, int player, int peg )
{
	s->peg = peg;
	s->cpeg = peg;
	s->cr = b->p[player].peg[peg].r;
	s->cc = b->p[player].peg[peg].c;
}

void sgs_validate( sg_sel *s, const board_state *b, int player )
{
	if ( s->peg >= 0 && !board_peg_can_act( b, player, s->peg ) )
	{
		sgs_cancel( s );
	}
	if ( s->cpeg < 0 || !board_peg_can_act( b, player, s->cpeg ) )
	{
		s->cpeg = board_next_peg( b, player, s->cpeg < 0 ? 0 : s->cpeg, 1 );
	}
}

void sgs_tab( sg_sel *s, const board_state *b, int player, int dir )
{
	int from = s->peg >= 0 ? s->peg : s->cpeg, k;

	from = from < 0 ? ( dir > 0 ? BOARD_NPEGS - 1 : 0 ) : from;
	k = board_next_peg( b, player, from, dir );
	if ( k < 0 )
	{
		return;
	}
	if ( s->peg >= 0 )
	{
		sgs_pick( s, b, player, k );
	}
	else
	{
		s->cpeg = k;
	}
}

void sgs_arrow( sg_sel *s, const board_state *b, int player, int dr, int dc )
{
	const board_peg *p;
	int              nr, nc;

	if ( s->peg < 0 )
	{
		sgs_tab( s, b, player, dr + dc > 0 ? 1 : -1 );
		return;
	}
	p = &b->p[player].peg[s->peg];
	if ( s->cr - p->r == -dr && s->cc - p->c == -dc && ( dr || dc ) )
	{
		s->cr = p->r;  // Back onto the peg itself.
		s->cc = p->c;
		return;
	}
	nr = p->r + dr;
	nc = p->c + dc;
	if ( board_target_kind( b, player, s->peg, nr, nc ) != BOARD_TARGET_NONE )
	{
		s->cr = nr;
		s->cc = nc;
	}
}

int sgs_confirm( sg_sel *s, const board_state *b, int player, int *peg,
                 int *r, int *c )
{
	const board_peg *p;

	if ( s->peg < 0 )
	{
		if ( s->cpeg >= 0 && board_peg_can_act( b, player, s->cpeg ) )
		{
			sgs_pick( s, b, player, s->cpeg );
		}
		return 0;
	}
	p = &b->p[player].peg[s->peg];
	if ( s->cr == p->r && s->cc == p->c )
	{
		sgs_cancel( s );
		return 0;
	}
	if ( board_target_kind( b, player, s->peg, s->cr, s->cc ) ==
	     BOARD_TARGET_NONE )
	{
		return 0;
	}
	*peg = s->peg;
	*r = s->cr;
	*c = s->cc;
	return 1;
}

int sgs_click( sg_sel *s, const board_state *b, int player, int r, int c,
               int *peg )
{
	int oi, og;

	if ( board_occupant( b, r, c, &oi, &og ) && oi == player )
	{
		if ( s->peg == og )
		{
			sgs_cancel( s );
		}
		else if ( board_peg_can_act( b, player, og ) )
		{
			sgs_pick( s, b, player, og );
		}
		return 0;
	}
	if ( s->peg >= 0 &&
	     board_target_kind( b, player, s->peg, r, c ) != BOARD_TARGET_NONE )
	{
		s->cr = r;
		s->cc = c;
		*peg = s->peg;
		return 1;
	}
	return 0;
}

//------------------------------------------------------------------------------
//------------------------------- Save and load --------------------------------
//------------------------------------------------------------------------------

int sg_save( const sg_game *g, const char *path )
{
	FILE    *f;
	uint32_t hdr[4];

	if ( g->scene != SG_SCENE_BOARD )
	{
		return -1;
	}
	f = fopen( path, "wb" );
	if ( !f )
	{
		return -2;
	}
	hdr[0] = SG_SAVE_VERSION;
	hdr[1] = (uint32_t)sizeof g->board;
	hdr[2] = (uint32_t)g->language;
	hdr[3] = (uint32_t)g->timer_sec;
	if ( fwrite( SG_SAVE_MAGIC, 1, 8, f ) != 8 ||
	     fwrite( hdr, sizeof hdr[0], 4, f ) != 4 ||
	     fwrite( g->names, sizeof g->names, 1, f ) != 1 ||
	     fwrite( &g->board, sizeof g->board, 1, f ) != 1 )
	{
		fclose( f );
		return -2;
	}
	return fclose( f ) == 0 ? 0 : -2;
}

int sg_load( sg_game *g, const char *path )
{
	FILE        *f = fopen( path, "rb" );
	char         magic[8];
	uint32_t     hdr[4];
	char         names[BOARD_NPLAYERS][LAN_NAME_SIZE];
	board_state  b;
	int          i, r, c;

	if ( !f )
	{
		return -1;
	}
	if ( fread( magic, 1, 8, f ) != 8 || memcmp( magic, SG_SAVE_MAGIC, 8 ) ||
	     fread( hdr, sizeof hdr[0], 4, f ) != 4 || hdr[0] != SG_SAVE_VERSION ||
	     hdr[1] != sizeof b || hdr[2] > 1 ||
	     fread( names, sizeof names, 1, f ) != 1 ||
	     fread( &b, sizeof b, 1, f ) != 1 )
	{
		fclose( f );
		return -2;
	}
	fclose( f );
	if ( ( b.n != 8 && b.n != 10 ) || b.nplayers < 2 ||
	     b.nplayers > BOARD_NPLAYERS || b.turn < 0 || b.turn >= b.nplayers ||
	     b.winner != -1 )
	{
		return -2;
	}
	for ( i = 0; i < b.nplayers; i++ )
	{
		for ( r = 0; r < BOARD_NPEGS; r++ )
		{
			c = b.p[i].peg[r].alive;
			if ( c && ( b.p[i].peg[r].r < 0 || b.p[i].peg[r].r >= b.n ||
			            b.p[i].peg[r].c < 0 || b.p[i].peg[r].c >= b.n ) )
			{
				return -2;
			}
		}
		names[i][LAN_NAME_SIZE - 1] = '\0';
	}
	g->board = b;
	memcpy( g->names, names, sizeof g->names );
	g->language = (int)hdr[2];
	g->timer_sec = (int)hdr[3];
	g->scene = SG_SCENE_BOARD;
	g->winner = -1;
	g->peg = -1;
	g->guest = 0;
	g->epoch++;
	return 0;
}
