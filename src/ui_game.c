//  Description:
//
//		ui_game.c -- the game screen and the winner screen.
//
//		Geometry mirrors the original scaled by 1.5 (canvas units):
//
//		  tile 76.5 x 51 (original 51 x 34),
//		  board origin x0 = (10 - N) * 76.5 / 2 + 6, y0 = (10 - N) * 51 / 2 + 13.5,
//		  the centre 6 x 6 tiles are covered by the question panel while a
//		  question is open, the info panel stands to the right of the board.
//
//		Z-order of a board cell: tile or hole, category marker, flag overlay,
//		peg, target frame, cursor frame (art/README.md).
//

//------------------------------------------------------------------------------
//---------------------------------- Includes ----------------------------------
//------------------------------------------------------------------------------

#include "app.h"
#include "../art/art_data.h"
#include "snd.h"
#include "strs.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

//------------------------------------------------------------------------------
//------------------------- Local macros and constants -------------------------
//------------------------------------------------------------------------------

#define UG_TW 76.5f
#define UG_TH 51.0f
#define UG_PX 159.0f   // Question panel: centre 6 x 6 tiles.
#define UG_PY 115.5f
#define UG_PW 459.0f
#define UG_PH 306.0f
#define UG_INFO_X 779.0f
#define UG_INFO_W 175.0f

#define UG_INK 0x1D2333

//------------------------------------------------------------------------------
//------------------------------ Local variables -------------------------------
//------------------------------------------------------------------------------

static const char *const ug_roman[BOARD_NPEGS] = { "I", "II", "III", "IV", "V" };

static const char *const ug_credits[] = {
	"@PROGRAMER:", "JOŠKO BILIĆ", "",
	"@DODATNO PROGRAMIRANJE:", "MLADEN RADAS", "ŽELJKO ĐURANIĆ", "",
	"@AMIGA PROGRAMERI:", "DOMAGOJ MALOVIĆ", "GORAN LJUBOJEVIĆ", "",
	"@CRTEŽI I SLIKE:", "VIKTOR BUBLIĆ", "VELJKO KUKULJ", "JOŠKO BILIĆ", "",
	"@ZVUK I EFEKTI:", "JASMINKA JURAŠKOVIĆ", "DOMAGOJ MALOVIĆ",
	"GORAN LJUBOJEVIĆ", "",
	"@IZVEDBA I ADAPTACIJA GLAZBE:", "JASMIN KOTUR", "",
	"@IDEJA:", "VIKTOR BUBLIĆ", "DOMAGOJ MALOVIĆ", "",
	"@PRIMJEDBE NA IDEJU:", "VELJKO KUKULJ", "",
	"@PRODUKCIJA:", "32 BITA D.O.O.", "",
	"@PRODUCENT:", "VELJKO KUKULJ", ""
};

//------------------------------------------------------------------------------
//------------------------------- Local functions ------------------------------
//------------------------------------------------------------------------------

/**
  * @brief  Origin of the board.
  * @param  n Board size.
  * @param  x0 Receives the left edge.
  * @param  y0 Receives the top edge.
  * @retval None.
  */
static void ug_origin( int n, float *x0, float *y0 )
{
	*x0 = (float)( 10 - n ) * UG_TW / 2.0f + 6.0f;
	*y0 = (float)( 10 - n ) * UG_TH / 2.0f + 13.5f;
}

/**
  * @brief  Whether a tile lies under the question panel.
  * @param  n Board size.
  * @param  r Row.
  * @param  c Column.
  * @retval 1 if it is one of the centre 6 x 6 tiles.
  */
static int ug_centre( int n, int r, int c )
{
	int lo = ( n - 6 ) / 2;

	return r >= lo && r < lo + 6 && c >= lo && c < lo + 6;
}

/**
  * @brief  Find the tile under a canvas point.
  * @param  n Board size.
  * @param  x Canvas X.
  * @param  y Canvas Y.
  * @param  r Receives the row.
  * @param  c Receives the column.
  * @retval 1 if the point is on the board.
  */
static int ug_tile_at( int n, float x, float y, int *r, int *c )
{
	float x0, y0;

	ug_origin( n, &x0, &y0 );
	if ( x < x0 || y < y0 )
	{
		return 0;
	}
	*c = (int)( ( x - x0 ) / UG_TW );
	*r = (int)( ( y - y0 ) / UG_TH );
	return *r < n && *c < n;
}

/**
  * @brief  Tint for a seat and a category.
  * @param  a Application state.
  * @param  seat Seat.
  * @param  cat Category 1..9 or 0.
  * @param  t Receives the tint.
  * @retval None.
  */
static void ug_tint( const app *a, int seat, int cat, vg_tint *t )
{
	vg_tint_make( app_player_art( a, seat ), cat, t );
}

/**
  * @brief  Draw the tiles, markers and flags of the board.
  * @param  a Application state.
  * @param  q Nonzero when the centre is covered by the question panel.
  * @retval None.
  */
static void ug_draw_tiles( app *a, int q )
{
	const board_state *b = &a->game.board;
	float              x0, y0, tx, ty;
	int                r, c, t;
	vg_tint            tint;

	ug_origin( b->n, &x0, &y0 );
	for ( r = 0; r < b->n; r++ )
	{
		for ( c = 0; c < b->n; c++ )
		{
			if ( q && ug_centre( b->n, r, c ) )
			{
				continue;
			}
			tx = x0 + (float)c * UG_TW;
			ty = y0 + (float)r * UG_TH;
			t = b->terrain[r][c];
			if ( t == BOARD_HOLE )
			{
				gfx_art( &a->g, ART_HOLE, tx, ty, UG_TW, UG_TH, NULL, 1.0f );
				continue;
			}
			gfx_art( &a->g, ART_TILE, tx, ty, UG_TW, UG_TH, NULL, 1.0f );
			if ( t >= 1 && t <= 9 )
			{
				vg_tint_make( 0, t, &tint );
				gfx_art( &a->g, ART_MARKER_1 + t - 1, tx + 4.5f, ty + 4.5f,
				         24.0f, 19.5f, &tint, 1.0f );
			}
			else if ( t > BOARD_FLAG_BASE )
			{
				ug_tint( a, t - BOARD_FLAG_BASE - 1, 0, &tint );
				gfx_art( &a->g, ART_FLAG_TILE, tx, ty, UG_TW, UG_TH, &tint,
				         1.0f );
			}
		}
	}
}

/**
  * @brief  Draw one peg with its numeral and attempts.
  * @param  a Application state.
  * @param  p Player.
  * @param  k Peg index.
  * @param  pips Nonzero to show the attempts left.
  * @retval None.
  */
static void ug_draw_peg( app *a, int p, int k, int pips )
{
	const board_state *b = &a->game.board;
	const board_peg   *pg = &b->p[p].peg[k];
	float              x0, y0, tx, ty, w = 35.2f, h = 44.0f;
	vg_tint            tint;
	int                i;

	ug_origin( b->n, &x0, &y0 );
	tx = x0 + (float)pg->c * UG_TW;
	ty = y0 + (float)pg->r * UG_TH;
	ug_tint( a, p, 0, &tint );
	gfx_art( &a->g, ART_PEG_0 + board_rank( pg->score ),
	         tx + ( UG_TW - w ) * 0.5f, ty + ( UG_TH - h ) * 0.5f - 4.0f, w, h,
	         &tint, 1.0f );
	gfx_rrect( &a->g, tx + UG_TW - 21.0f, ty + UG_TH - 16.5f, 18.0f, 13.5f,
	           5.0f, 0x000000, 0.55f );
	gfx_text( &a->g, 10.5f, tx + UG_TW - 12.0f, ty + UG_TH - 16.0f,
	          ug_roman[k], 0xFFFFFF, GFX_CENTRE, 1.0f );
	if ( pips )
	{
		for ( i = 0; i < pg->attempts && i < 3; i++ )
		{
			gfx_rrect( &a->g, tx + UG_TW - 13.5f - (float)i * 8.5f, ty + 3.0f,
			           8.0f, 8.0f, 4.0f, 0x000000, 0.7f );
			gfx_rrect( &a->g, tx + UG_TW - 12.5f - (float)i * 8.5f, ty + 4.0f,
			           6.0f, 6.0f, 3.0f, 0xFFD54A, 1.0f );
		}
	}
}

/**
  * @brief  Draw the frames of the selection, targets and cursor.
  * @param  a Application state.
  * @param  q Nonzero when a question is open.
  * @retval None.
  */
static void ug_draw_frames( app *a, int q )
{
	const board_state *b = &a->game.board;
	const sg_game     *g = &a->game;
	float              x0, y0;
	int                turn = b->turn, d, r, c, mine = app_my_turn( a );
	static const int   dr[4] = { -1, 1, 0, 0 };
	static const int   dc[4] = { 0, 0, -1, 1 };

	ug_origin( b->n, &x0, &y0 );
	if ( q )
	{
		if ( g->peg >= 0 && g->peg < BOARD_NPEGS )
		{
			gfx_art( &a->g, ART_CURSOR_FRAME,
			         x0 + (float)b->p[turn].peg[g->peg].c * UG_TW,
			         y0 + (float)b->p[turn].peg[g->peg].r * UG_TH, UG_TW,
			         UG_TH, NULL, 1.0f );
		}
		if ( g->tr >= 0 && g->tc >= 0 && !ug_centre( b->n, g->tr, g->tc ) )
		{
			gfx_art( &a->g, ART_TARGET_FRAME, x0 + (float)g->tc * UG_TW,
			         y0 + (float)g->tr * UG_TH, UG_TW, UG_TH, NULL, 1.0f );
		}
		return;
	}
	if ( g->scene != SG_SCENE_BOARD || b->winner != -1 )
	{
		return;
	}
	if ( mine )
	{
		if ( a->sel.peg >= 0 )
		{
			const board_peg *p = &b->p[turn].peg[a->sel.peg];

			for ( d = 0; d < 4; d++ )
			{
				r = p->r + dr[d];
				c = p->c + dc[d];
				if ( board_target_kind( b, turn, a->sel.peg, r, c ) !=
				     BOARD_TARGET_NONE )
				{
					gfx_art( &a->g, ART_TARGET_FRAME, x0 + (float)c * UG_TW,
					         y0 + (float)r * UG_TH, UG_TW, UG_TH, NULL, 1.0f );
				}
			}
			gfx_ring( &a->g, x0 + (float)p->c * UG_TW + 2,
			          y0 + (float)p->r * UG_TH + 2, UG_TW - 4, UG_TH - 4, 6,
			          2.0f, 0xFFFFFF, 0.9f );
			gfx_art( &a->g, ART_CURSOR_FRAME, x0 + (float)a->sel.cc * UG_TW,
			         y0 + (float)a->sel.cr * UG_TH, UG_TW, UG_TH, NULL, 1.0f );
		}
		else if ( a->sel.cpeg >= 0 )
		{
			const board_peg *p = &b->p[turn].peg[a->sel.cpeg];

			gfx_art( &a->g, ART_CURSOR_FRAME, x0 + (float)p->c * UG_TW,
			         y0 + (float)p->r * UG_TH, UG_TW, UG_TH, NULL, 1.0f );
		}
	}
	else if ( g->peg >= 0 && g->peg < BOARD_NPEGS && g->tr >= 0 )
	{
		gfx_art( &a->g, ART_CURSOR_FRAME, x0 + (float)g->tc * UG_TW,
		         y0 + (float)g->tr * UG_TH, UG_TW, UG_TH, NULL, 1.0f );
	}
}

/**
  * @brief  Draw a flat answer button of the question panel.
  * @param  a Application state.
  * @param  i Slot 0..3.
  * @param  y Top.
  * @param  state 0 normal, 1 highlighted, 2 correct, 3 wrong, 4 dimmed.
  * @retval None.
  */
static void ug_draw_answer( app *a, int i, float y, int state )
{
	float    x = UG_PX + 16.0f, w = UG_PW - 32.0f, h = 30.0f;
	uint32_t bg = 0xE9ECF2, fg = UG_INK, ring = 0xB5BCCB;
	char     num[4];

	if ( state == 1 )
	{
		bg = 0xFFE9A3;
		ring = 0xE0A800;
	}
	else if ( state == 2 )
	{
		bg = 0x2FB04A;
		fg = 0xFFFFFF;
		ring = 0x1B7A31;
	}
	else if ( state == 3 )
	{
		bg = 0xE53935;
		fg = 0xFFFFFF;
		ring = 0x9A1B19;
	}
	gfx_rrect( &a->g, x, y, w, h, 8, bg, state == 4 ? 0.55f : 1.0f );
	gfx_ring( &a->g, x, y, w, h, 8, 1.4f, ring, state == 4 ? 0.5f : 1.0f );
	gfx_rrect( &a->g, x + 6, y + 5, 20, 20, 10,
	           state >= 2 && state <= 3 ? 0xFFFFFF : UG_INK, 0.9f );
	snprintf( num, sizeof num, "%d", i + 1 );
	gfx_text( &a->g, 13, x + 16, y + 6.5f, num,
	          state >= 2 && state <= 3 ? ring : 0xFFFFFF, GFX_CENTRE, 1.0f );
	gfx_text_fit( &a->g, 15, 9.5f, x + 34, y + 2, w - 44, h - 4,
	              a->game.cur.a[i], fg, GFX_LEFT );
}

/**
  * @brief  Draw the question panel and handle answering.
  * @param  a Application state.
  * @retval None.
  */
static void ug_draw_question( app *a )
{
	sg_game       *g = &a->game;
	float          sy = UG_PH / 300.0f, ay, frac, bw;
	char           text[160];
	uint32_t       col, left;
	int            i, state, cat, tile_cat, mine = app_my_turn( a ), hov = -1;
	vg_tint        tint;
	static float   last_mx, last_my;

	gfx_art( &a->g, ART_PANEL, UG_PX, UG_PY, UG_PW, UG_PH, NULL, 1.0f );
	cat = g->category >= 1 && g->category <= 8 ? g->category : 1;
	tile_cat = g->tr >= 0 ? g->board.terrain[g->tr][g->tc] : cat;
	vg_tint_make( 0, cat, &tint );
	gfx_art( &a->g, ART_MARKER_1 + cat - 1, UG_PX + 16, UG_PY + 14 * sy, 30,
	         24, &tint, 1.0f );
	snprintf( text, sizeof text, "%s", vg_category_name( a->lang == 1, cat ) );
	if ( tile_cat < 1 || tile_cat > 8 )
	{
		snprintf( text, sizeof text, "%s: %s", tr( a->lang, S_RANDOM ),
		          vg_category_name( a->lang == 1, cat ) );
	}
	gfx_text( &a->g, 20, UG_PX + UG_PW * 0.5f + 14, UG_PY + 10 * sy + 8, text,
	          0xFFFFFF, GFX_CENTRE, 1.0f );
	left = sg_remaining_ms( g, a->now );
	snprintf( text, sizeof text, "%u", (unsigned)( ( left + 999 ) / 1000 ) );
	if ( g->timer_sec <= 60 && !g->answered )
	{
		gfx_text( &a->g, 20, UG_PX + UG_PW - 26, UG_PY + 10 * sy + 8, text,
		          0xFFD54A, GFX_RIGHT, 1.0f );
	}
	if ( g->kind == BOARD_TARGET_BATTLE )
	{
		snprintf( text, sizeof text, tr( a->lang, S_QUESTION_BATTLE ), g->qnum );
	}
	else if ( g->kind == BOARD_TARGET_FLAG )
	{
		snprintf( text, sizeof text, "%s", tr( a->lang, S_QUESTION_FLAG ) );
	}
	else
	{
		snprintf( text, sizeof text, "%s", tr( a->lang, S_QUESTION_STEP ) );
	}
	gfx_text( &a->g, 12, UG_PX + 22, UG_PY + 67, text, 0x5A6275, GFX_LEFT, 1.0f );
	gfx_text_fit( &a->g, 16.5f, 10.5f, UG_PX + 22, UG_PY + 83, UG_PW - 44, 54,
	              g->cur.q, UG_INK, GFX_LEFT );
	if ( mine && !g->answered && ( a->in.mx != last_mx || a->in.my != last_my ) )
	{
		for ( i = 0; i < 4; i++ )
		{
			ay = UG_PY + 142.0f + (float)i * 36.0f;
			if ( a->in.mx >= UG_PX + 16 && a->in.mx < UG_PX + UG_PW - 16 &&
			     a->in.my >= ay && a->in.my < ay + 30 )
			{
				g->hilite = i;
			}
		}
	}
	last_mx = a->in.mx;
	last_my = a->in.my;
	for ( i = 0; i < 4; i++ )
	{
		ay = UG_PY + 142.0f + (float)i * 36.0f;
		state = 0;
		if ( g->answered )
		{
			state = i == g->picked ? ( g->was_correct ? 2 : 3 ) : 4;  // The right answer is not revealed.
		}
		else if ( i == g->hilite && mine )
		{
			state = 1;
		}
		ug_draw_answer( a, i, ay, state );
		if ( a->in.mx >= UG_PX + 16 && a->in.mx < UG_PX + UG_PW - 16 &&
		     a->in.my >= ay && a->in.my < ay + 30 )
		{
			hov = i;
		}
	}
	if ( mine && !g->answered )
	{
		if ( a->in.click && hov >= 0 )
		{
			app_do_answer( a, hov );
		}
		else if ( a->in.digit )
		{
			app_do_answer( a, a->in.digit - 1 );
		}
		else if ( a->in.enter )
		{
			app_do_answer( a, g->hilite );
		}
		else if ( a->in.up )
		{
			g->hilite = ( g->hilite + 3 ) % 4;
		}
		else if ( a->in.down || a->in.tab )
		{
			g->hilite = ( g->hilite + 1 ) % 4;
		}
	}
	// Countdown bar over the full answer time.
	frac = g->timer_sec > 0 ? (float)left / ( (float)g->timer_sec * 1000.0f )
	                        : 0.0f;
	frac = g->answered ? 0.0f : frac;
	col = frac > 0.5f ? ui_color( a, ART_UI_TIMER_OK )
	                  : ( frac > 0.25f ? ui_color( a, ART_UI_TIMER_WARN )
	                                   : ui_color( a, ART_UI_TIMER_BAD ) );
	bw = UG_PW - 40.0f;
	gfx_rrect( &a->g, UG_PX + 20, UG_PY + 290, bw, 8, 4, 0xC9CED8, 1.0f );
	if ( frac > 0.0f )
	{
		gfx_rrect( &a->g, UG_PX + 20, UG_PY + 290, bw * frac < 8 ? 8 : bw * frac,
		           8, 4, col, 1.0f );
	}
	if ( g->answered )
	{
		gfx_rrect( &a->g, UG_PX + 4, UG_PY + 56 * sy, UG_PW - 8,
		           UG_PH - 56 * sy - 6, 10, 0x000000, 0.32f );
		if ( g->was_correct )
		{
			gfx_tick( &a->g, UG_PX + UG_PW * 0.5f - 60, UG_PY + 108, 120, 120,
			          0xFFD400, 0x4A3B00 );
		}
		else
		{
			gfx_art( &a->g, ART_CROSS, UG_PX + UG_PW * 0.5f - 60, UG_PY + 108,
			         120, 120, NULL, 1.0f );
		}
	}
}

/**
  * @brief  Draw the info panel on the right of the board.
  * @param  a Application state.
  * @retval None.
  */
static void ug_draw_info( app *a )
{
	const board_state *b = &a->game.board;
	const sg_game     *g = &a->game;
	int                turn = b->turn, i, k, rank, prog, mine;
	float              x = UG_INFO_X, y = 10, ry;
	uint32_t           pc;
	char               text[96];
	vg_tint            tint;
	const board_peg   *pg;
	const char        *hint;

	pc = vg_player_color( app_player_art( a, turn ), 0 );
	mine = app_my_turn( a );
	gfx_rrect( &a->g, x, y, UG_INFO_W, 520, 12, ui_color( a, ART_UI_PANEL ),
	           a->dark ? 0.6f : 0.7f );
	gfx_ring( &a->g, x, y, UG_INFO_W, 520, 12, 1.2f,
	          ui_color( a, ART_UI_BORDER ), 0.7f );
	gfx_rrect( &a->g, x + 8, y + 8, UG_INFO_W - 16, 5, 2.5f, pc, 1.0f );
	ug_tint( a, turn, 0, &tint );
	gfx_art( &a->g, ART_PAWN_BIG, x + 10, y + 22, 50, 100, &tint, 1.0f );
	gfx_text_fit( &a->g, 17, 10.5f, x + 66, y + 24, UG_INFO_W - 74, 24,
	              g->names[turn], ui_color( a, ART_UI_TEXT ), GFX_LEFT );
	gfx_text( &a->g, 12, x + 66, y + 50, tr( a->lang, S_TURN ),
	          ui_color( a, ART_UI_TEXT_DIM ), GFX_LEFT, 1.0f );
	// Peg in focus: selected, highlighted, or the one being played.
	k = g->scene == SG_SCENE_QUESTION ? g->peg : ( a->sel.peg >= 0 ? a->sel.peg
	                                                               : a->sel.cpeg );
	if ( !mine && g->scene == SG_SCENE_BOARD )
	{
		k = -1;
	}
	if ( k >= 0 && k < BOARD_NPEGS && b->p[turn].peg[k].alive )
	{
		pg = &b->p[turn].peg[k];
		rank = board_rank( pg->score );
		snprintf( text, sizeof text, "%s %s", tr( a->lang, S_RANK0 + rank ),
		          ug_roman[k] );
		gfx_text( &a->g, 14, x + 66, y + 72, text, ui_color( a, ART_UI_TEXT ),
		          GFX_LEFT, 1.0f );
		prog = pg->score - BOARD_SCORE_PER_RANK * rank;
		prog = prog > 5 ? 5 : prog;
		for ( i = 0; i < 5; i++ )
		{
			gfx_rrect( &a->g, x + 66 + (float)i * 20, y + 94, 16, 9, 4.5f,
			           i < prog ? ui_color( a, ART_UI_ACCENT2 )
			                    : ui_color( a, ART_UI_BORDER ),
			           i < prog ? 1.0f : 0.6f );
		}
		gfx_text( &a->g, 11, x + 66, y + 108,
		          rank >= BOARD_MAX_RANK ? tr( a->lang, S_MAX_RANK )
		                                 : tr( a->lang, S_TO_RANK ),
		          ui_color( a, ART_UI_TEXT_DIM ), GFX_LEFT, 1.0f );
	}
	hint = "";
	if ( g->scene == SG_SCENE_BOARD && g->board.winner == -1 )
	{
		if ( !mine )
		{
			snprintf( text, sizeof text, tr( a->lang, S_WAITING ),
			          g->names[turn] );
			hint = text;
		}
		else
		{
			hint = tr( a->lang, a->sel.peg >= 0 ? S_SELECT_TARGET
			                                    : S_SELECT_PEG );
		}
	}
	gfx_text_wrap( &a->g, 12, x + 10, y + 130, UG_INFO_W - 20, hint,
	               ui_color( a, ART_UI_TEXT ), GFX_LEFT, 1.0f, 1 );
	// Scoreboard.
	for ( i = 0; i < b->nplayers; i++ )
	{
		ry = y + 172 + (float)i * 23;
		if ( i == turn )
		{
			gfx_rrect( &a->g, x + 6, ry - 2, UG_INFO_W - 12, 22, 7,
			           vg_player_color( app_player_art( a, i ), 0 ), 0.28f );
		}
		ug_tint( a, i, 0, &tint );
		gfx_art_fit( &a->g, ART_PEG_0, x + 10, ry - 1, 14, 19, &tint, 1.0f );
		gfx_text_fit( &a->g, 13, 9.5f, x + 30, ry + 1, UG_INFO_W - 70, 18,
		              g->names[i], ui_color( a, ART_UI_TEXT ), GFX_LEFT );
		snprintf( text, sizeof text, "%d", b->p[i].pegs_left );
		gfx_text( &a->g, 14, x + UG_INFO_W - 12, ry + 1, text,
		          b->p[i].pegs_left ? ui_color( a, ART_UI_TEXT )
		                            : ui_color( a, ART_UI_BAD ),
		          GFX_RIGHT, 1.0f );
	}
	gfx_fill( &a->g, x + 10, y + 270, UG_INFO_W - 20, 1.2f,
	          ui_color( a, ART_UI_BORDER ), 0.8f );
	// Legend: the eight categories and "Random".
	for ( i = 1; i <= 9; i++ )
	{
		ry = y + 278 + (float)( i - 1 ) * 21;
		vg_tint_make( 0, i, &tint );
		gfx_art( &a->g, ART_MARKER_1 + i - 1, x + 12, ry, 24, 19.5f, &tint,
		         1.0f );
		gfx_text( &a->g, 13, x + 44, ry + 1.5f,
		          i == 9 ? tr( a->lang, S_RANDOM )
		                 : vg_category_name( a->lang == 1, i ),
		          ui_color( a, ART_UI_TEXT ), GFX_LEFT, 1.0f );
	}
}

/**
  * @brief  Draw the pass button and the sound icon under the legend.
  * @param  a Application state.
  * @retval None.
  */
static void ug_draw_buttons( app *a )
{
	float   y = 10 + 478;
	vg_tint tint;

	if ( ui_btn( a, UG_INFO_X + 10, y, 108, 32, tr( a->lang, S_PASS_KEY ), 0,
	             app_my_turn( a ) && a->game.scene == SG_SCENE_BOARD ) )
	{
		app_do_pass( a );
	}
	memset( &tint, 0, sizeof tint );
	tint.tok[ART_TOK_C] = ui_color( a, ART_UI_TEXT );
	if ( ui_hit( a, UG_INFO_X + 126, y, 40, 32 ) )
	{
		gfx_rrect( &a->g, UG_INFO_X + 126, y, 40, 32, 9,
		           ui_color( a, ART_UI_PANEL_HI ), 1.0f );
		if ( a->in.click )
		{
			a->sound = !a->sound;
			snd_enable( a->sound );
			app_save_cfg( a );
		}
	}
	gfx_art_fit( &a->g, a->sound ? ART_ICON_SOUND_ON : ART_ICON_SOUND_OFF,
	             UG_INFO_X + 134, y + 4, 24, 24, &tint, 1.0f );
}

/**
  * @brief  Draw and handle the quit dialog.
  * @param  a Application state.
  * @retval None.
  */
static void ug_quit_dialog( app *a )
{
	float x = 240, y = 150, w = 480, h = 240, cx = 480;
	char  text[96];
	int   yes = 0, no = 0;

	gfx_fill( &a->g, 0, 0, 960, 540, 0x000000, 0.55f );
	gfx_rrect( &a->g, x, y, w, h, 14, ui_color( a, ART_UI_PANEL ), 1.0f );
	gfx_ring( &a->g, x, y, w, h, 14, 2.0f, ui_color( a, ART_UI_ACCENT ), 1.0f );
	if ( a->lan.role == LAN_HOST )
	{
		gfx_text( &a->g, 20, cx, y + 34, tr( a->lang, S_QUIT_LAN_HOST ),
		          ui_color( a, ART_UI_TEXT ), GFX_CENTRE, 1.0f );
	}
	else if ( a->lan.role == LAN_CLIENT )
	{
		gfx_text( &a->g, 20, cx, y + 34, tr( a->lang, S_QUIT_1 ),
		          ui_color( a, ART_UI_TEXT ), GFX_CENTRE, 1.0f );
	}
	else
	{
		gfx_text( &a->g, 20, cx, y + 28, tr( a->lang, S_QUIT_1 ),
		          ui_color( a, ART_UI_TEXT ), GFX_CENTRE, 1.0f );
		gfx_text( &a->g, 16, cx, y + 70, tr( a->lang, S_QUIT_2 ),
		          ui_color( a, ART_UI_TEXT_DIM ), GFX_CENTRE, 1.0f );
		gfx_text( &a->g, 16, cx, y + 94, tr( a->lang, S_QUIT_3 ),
		          ui_color( a, ART_UI_TEXT_DIM ), GFX_CENTRE, 1.0f );
	}
	snprintf( text, sizeof text, "%s", tr( a->lang, S_QUIT_BTN ) );
	gfx_text( &a->g, 18, cx, y + 132, text, ui_color( a, ART_UI_ACCENT2 ),
	          GFX_CENTRE, 1.0f );
	yes = ui_btn( a, cx - 150, y + 176, 140, 40, a->lang ? "Da (D)" : "Yes (Y)",
	              1, 1 );
	no = ui_btn( a, cx + 10, y + 176, 140, 40, a->lang ? "Ne (N)" : "No (N)", 0, 1 );
	if ( a->in.dkey == 'D' || a->in.dkey == 'Y' )
	{
		yes = 1;
	}
	else if ( a->in.dkey || a->in.esc || a->in.key_any )
	{
		no = a->in.dkey || a->in.esc || a->in.key_any;
	}
	if ( yes )
	{
		a->quit_dlg = 0;
		if ( a->lan.role == LAN_HOST )
		{
			app_host_lobby( a );
		}
		else if ( a->lan.role == LAN_CLIENT )
		{
			app_lan_leave( a, APP_JOIN );
		}
		else
		{
			app_goto( a, APP_MENU );
		}
	}
	else if ( no )
	{
		a->quit_dlg = 0;
	}
}

/**
  * @brief  Handle keyboard and mouse on the board.
  * @param  a Application state.
  * @retval None.
  */
static void ug_board_input( app *a )
{
	sg_game *g = &a->game;
	int      peg, r, c, tr_, tc;

	if ( !app_my_turn( a ) || g->scene != SG_SCENE_BOARD ||
	     g->board.winner != -1 )
	{
		return;
	}
	if ( a->in.up ) { sgs_arrow( &a->sel, &g->board, g->board.turn, -1, 0 ); }
	if ( a->in.down ) { sgs_arrow( &a->sel, &g->board, g->board.turn, 1, 0 ); }
	if ( a->in.left ) { sgs_arrow( &a->sel, &g->board, g->board.turn, 0, -1 ); }
	if ( a->in.right ) { sgs_arrow( &a->sel, &g->board, g->board.turn, 0, 1 ); }
	if ( a->in.tab ) { sgs_tab( &a->sel, &g->board, g->board.turn, 1 ); }
	if ( a->in.stab ) { sgs_tab( &a->sel, &g->board, g->board.turn, -1 ); }
	if ( a->in.enter &&
	     sgs_confirm( &a->sel, &g->board, g->board.turn, &peg, &tr_, &tc ) )
	{
		app_do_move( a, peg, tr_, tc );
	}
	if ( a->in.space )
	{
		app_do_pass( a );
	}
	if ( a->in.rclick )
	{
		sgs_cancel( &a->sel );
	}
	if ( a->in.click && ug_tile_at( g->board.n, a->in.mx, a->in.my, &r, &c ) &&
	     sgs_click( &a->sel, &g->board, g->board.turn, r, c, &peg ) )
	{
		app_do_move( a, peg, r, c );
	}
}

//------------------------------------------------------------------------------
//------------------------------- Public API -----------------------------------
//------------------------------------------------------------------------------

void ui_game( app *a )
{
	sg_game *g = &a->game;
	int      q = g->scene == SG_SCENE_QUESTION, p, k;

	if ( a->in.fkey == 5 )
	{
		app_save_load( a, 1 );
	}
	else if ( a->in.fkey == 8 )
	{
		app_save_load( a, 0 );
	}
	else if ( a->in.fkey == 9 )
	{
		a->tile_style = !a->tile_style;
	}
	if ( !a->quit_dlg )
	{
		if ( a->in.esc )
		{
			if ( a->sel.peg >= 0 && !q )
			{
				sgs_cancel( &a->sel );
			}
			else
			{
				a->quit_dlg = 1;
			}
			a->in.esc = 0;
			a->in.key_any = 0;
		}
		else if ( !q )
		{
			ug_board_input( a );
		}
	}
	ug_draw_tiles( a, q );
	for ( p = 0; p < g->board.nplayers; p++ )
	{
		for ( k = 0; k < BOARD_NPEGS; k++ )
		{
			if ( g->board.p[p].peg[k].alive &&
			     !( q && ug_centre( g->board.n, g->board.p[p].peg[k].r,
			                        g->board.p[p].peg[k].c ) ) )
			{
				ug_draw_peg( a, p, k, p == g->board.turn && !q &&
				                          g->scene == SG_SCENE_BOARD );
			}
		}
	}
	ug_draw_frames( a, q );
	if ( q )
	{
		ug_draw_question( a );
	}
	ug_draw_info( a );
	if ( !a->quit_dlg )
	{
		ug_draw_buttons( a );
	}
	if ( g->board.n == 8 )
	{
		gfx_text( &a->g, 12, 6 + 2 * UG_TW / 2 + 4 * UG_TW, 486,
		          tr( a->lang, S_HELP_KEYS ), ui_color( a, ART_UI_TEXT_DIM ),
		          GFX_CENTRE, 0.9f );
	}
	if ( a->quit_dlg )
	{
		ug_quit_dialog( a );
	}
}

void ui_winner( app *a )
{
	const sg_game *g = &a->game;
	int            w = g->winner >= 0 ? g->winner : 0, art, frame, i, n;
	char           text[200], name[LAN_NAME_SIZE], col[32];
	vg_tint        tint;
	float          y, sc, lh;
	uint32_t       t = a->now - a->t0, ink = ui_color( a, ART_UI_TEXT );
	const char    *s;

	art = app_player_art( a, w );
	vg_tint_make( art, 0, &tint );
	frame = (int)( a->now / 90 ) % 8;
	gfx_art_fit( &a->g, ART_FLAG_WAVE_0 + frame, 50, 90, 330, 330, &tint, 1.0f );
	snprintf( name, sizeof name, "%s", g->names[w] );
	for ( i = 0; name[i]; i++ )
	{
		name[i] = (char)toupper( (unsigned char)name[i] );
	}
	snprintf( col, sizeof col, "%s", tr( a->lang, S_COL_RED + art ) );
	y = 40;
	gfx_text( &a->g, 26, 660, y, tr( a->lang, S_WIN_1 ), ink, GFX_CENTRE, 1.0f );
	y += 52;
	snprintf( text, sizeof text, tr( a->lang, S_WIN_2 ), col );
	y += gfx_text_wrap( &a->g, 21, 410, y, 500, text, ink, GFX_CENTRE, 1.0f, 1 );
	y += 18;
	gfx_text( &a->g, 20, 660, y, tr( a->lang, S_WIN_3 ), ink, GFX_CENTRE, 1.0f );
	y += 30;
	gfx_text( &a->g, 20, 660, y, tr( a->lang, S_WIN_4 ), ink, GFX_CENTRE, 1.0f );
	y += 44;
	snprintf( text, sizeof text, "\"%s!\" \"%s!\"", name, name );
	gfx_text_fit( &a->g, 30, 16, 410, y, 500, 38, text,
	              ui_color( a, ART_UI_ACCENT2 ), GFX_CENTRE );
	y += 50;
	gfx_text( &a->g, 30, 660, y, tr( a->lang, S_WIN_5 ), ink, GFX_CENTRE, 1.0f );
	// Credits of the original, scrolling upward and looping.
	gfx_clip( &a->g, 410, 292, 500, 200 );
	n = (int)( sizeof ug_credits / sizeof ug_credits[0] );
	lh = 24;
	sc = fmodf( (float)a->now * 0.025f, (float)n * lh );
	for ( i = -n; i < n; i++ )
	{
		float yy = 492.0f - sc + (float)i * lh;

		if ( yy < 270 || yy > 500 )
		{
			continue;
		}
		s = ug_credits[( i + n ) % n];
		if ( s[0] == '@' )
		{
			gfx_text( &a->g, 15, 660, yy, s + 1, ui_color( a, ART_UI_ACCENT ),
			          GFX_CENTRE, 1.0f );
		}
		else
		{
			gfx_text( &a->g, 16, 660, yy, s, ui_color( a, ART_UI_TEXT_DIM ),
			          GFX_CENTRE, 1.0f );
		}
	}
	gfx_unclip( &a->g );
	gfx_text( &a->g, 14, 480, 512, tr( a->lang, S_WIN_CONT ),
	          ui_color( a, ART_UI_TEXT_DIM ), GFX_CENTRE,
	          0.6f + 0.4f * (float)( ( a->now / 400 ) % 2 ) );
	if ( t > 700 && ( a->in.click || a->in.key_any ) )
	{
		if ( a->lan.role == LAN_HOST )
		{
			app_host_lobby( a );
		}
		else if ( a->lan.role == LAN_CLIENT )
		{
			a->winner_ack = 1;
			app_goto( a, APP_JOIN );
		}
		else
		{
			app_goto( a, APP_MENU );
		}
	}
}
