//  Description:
//
//		ui.c -- widgets and the non-game screens.
//
//		Widgets are immediate mode: calling ui_button() draws the button and
//		reports a click in the same call. Every widget takes the next index
//		of the frame, the focused one reacts to the keyboard, and ui_frame()
//		moves the focus with Up / Down / Tab after the screen has been drawn
//		(and so knows how many widgets there were). Layout is in canvas units
//		of the 960 x 540 design canvas.
//

//------------------------------------------------------------------------------
//---------------------------------- Includes ----------------------------------
//------------------------------------------------------------------------------

#include "app.h"
#include "../art/art_data.h"
#include "snd.h"
#include "strs.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

//------------------------------------------------------------------------------
//------------------------- Local macros and constants -------------------------
//------------------------------------------------------------------------------

#define UI_SPLASH_IN 700
#define UI_SPLASH_HOLD 3300
#define UI_SPLASH_END 3950

//------------------------------------------------------------------------------
//--------------------------- Local types and structures -----------------------
//------------------------------------------------------------------------------

typedef struct
{
	float x, y, w, h;
} ui_rect;

//------------------------------------------------------------------------------
//------------------------------ Local variables -------------------------------
//------------------------------------------------------------------------------

static const int ui_boards[2] = { 8, 10 };
static const int ui_times[5] = { 9, 15, 30, 60, 600 };

//------------------------------------------------------------------------------
//------------------------------- Local functions ------------------------------
//------------------------------------------------------------------------------

uint32_t ui_color( const app *a, int idx )
{
	return vg_ui_color( a->dark, idx );
}

/**
  * @brief  Mix two colours.
  * @param  c0 First colour 0xRRGGBB.
  * @param  c1 Second colour 0xRRGGBB.
  * @param  t Weight of the second colour, 0..1.
  * @retval Mixed colour.
  */
static uint32_t ui_mix( uint32_t c0, uint32_t c1, float t )
{
	uint32_t out = 0;
	int      s;

	for ( s = 16; s >= 0; s -= 8 )
	{
		float v = (float)( ( c0 >> s ) & 255 ) * ( 1.0f - t ) +
		          (float)( ( c1 >> s ) & 255 ) * t;

		out |= (uint32_t)( v + 0.5f ) << s;
	}
	return out;
}

/**
  * @brief  Test whether the mouse is inside a rectangle.
  * @param  a Application state.
  * @param  r Rectangle.
  * @retval 1 if inside.
  */
static int ui_inside( const app *a, ui_rect r )
{
	return a->in.mx >= r.x && a->in.mx < r.x + r.w && a->in.my >= r.y &&
	       a->in.my < r.y + r.h;
}

/**
  * @brief  Take the next widget index and let a click focus it.
  * @param  a Application state.
  * @param  r Widget rectangle.
  * @retval 1 if the widget has the focus.
  */
static int ui_take( app *a, ui_rect r )
{
	int id = a->wid++;

	if ( a->in.click && ui_inside( a, r ) )
	{
		a->focus = id;
	}
	return a->focus == id;
}

/**
  * @brief  Draw a panel.
  * @param  a Application state.
  * @param  r Rectangle.
  * @retval None.
  */
static void ui_panel( app *a, ui_rect r )
{
	gfx_rrect( &a->g, r.x, r.y, r.w, r.h, 12, ui_color( a, ART_UI_PANEL ),
	           a->dark ? 0.55f : 0.65f );
	gfx_ring( &a->g, r.x, r.y, r.w, r.h, 12, 1.2f,
	          ui_color( a, ART_UI_BORDER ), 0.7f );
}

/**
  * @brief  Draw a button and report activation.
  * @param  a Application state.
  * @param  r Rectangle.
  * @param  label UTF-8 caption.
  * @param  kind 0 plain, 1 primary (accent).
  * @param  enabled Zero draws it dimmed and inert.
  * @retval 1 when clicked or activated with Enter.
  */
static int ui_button( app *a, ui_rect r, const char *label, int kind,
                      int enabled )
{
	int      focus = ui_take( a, r ), hover = ui_inside( a, r ), act;
	uint32_t bg, fg;

	act = enabled && ( ( a->in.click && hover ) || ( focus && a->in.enter ) );
	if ( focus && a->in.enter )
	{
		a->in.enter = 0;
	}
	if ( kind == 1 )
	{
		bg = ui_color( a, ART_UI_ACCENT );
		fg = 0xFFFFFF;
	}
	else
	{
		bg = ui_color( a, ART_UI_PANEL_HI );
		fg = ui_color( a, ART_UI_TEXT );
	}
	if ( hover && enabled )
	{
		bg = ui_mix( bg, 0xFFFFFF, 0.12f );
	}
	gfx_rrect( &a->g, r.x, r.y + 2, r.w, r.h, 9, 0x000000,
	           enabled ? 0.22f : 0.08f );
	gfx_rrect( &a->g, r.x, r.y, r.w, r.h, 9, bg, enabled ? 1.0f : 0.45f );
	if ( focus && enabled )
	{
		gfx_ring( &a->g, r.x - 2, r.y - 2, r.w + 4, r.h + 4, 11, 2.2f,
		          ui_color( a, ART_UI_ACCENT2 ), 1.0f );
	}
	gfx_text_fit( &a->g, 18, 10.5f, r.x + 8, r.y, r.w - 16, r.h, label,
	              enabled ? fg : ui_mix( fg, bg, 0.5f ), GFX_CENTRE );
	return act;
}

/**
  * @brief  Draw a left / right chooser.
  * @param  a Application state.
  * @param  r Rectangle.
  * @param  value Text of the current value, may be NULL if drawn by caller.
  * @retval -1 or +1 when the value should change, 0 otherwise.
  */
static int ui_choice( app *a, ui_rect r, const char *value )
{
	int      focus = ui_take( a, r ), step = 0, hover = ui_inside( a, r );
	uint32_t fg = ui_color( a, ART_UI_TEXT );

	if ( a->in.click && hover )
	{
		step = a->in.mx < r.x + r.w * 0.3f ? -1 : 1;
	}
	if ( focus )
	{
		if ( a->in.left )
		{
			step = -1;
			a->in.left = 0;
		}
		else if ( a->in.right || a->in.enter )
		{
			step = 1;
			a->in.right = a->in.enter = 0;
		}
	}
	gfx_rrect( &a->g, r.x, r.y, r.w, r.h, 9, ui_color( a, ART_UI_PANEL_HI ),
	           hover ? 1.0f : 0.85f );
	if ( focus )
	{
		gfx_ring( &a->g, r.x - 2, r.y - 2, r.w + 4, r.h + 4, 11, 2.2f,
		          ui_color( a, ART_UI_ACCENT2 ), 1.0f );
	}
	gfx_text( &a->g, 20, r.x + 16, r.y + ( r.h - gfx_line_height( &a->g, 20 ) ) * 0.5f,
	          "<", ui_color( a, ART_UI_TEXT_DIM ), GFX_CENTRE, 1.0f );
	gfx_text( &a->g, 20, r.x + r.w - 16, r.y + ( r.h - gfx_line_height( &a->g, 20 ) ) * 0.5f,
	          ">", ui_color( a, ART_UI_TEXT_DIM ), GFX_CENTRE, 1.0f );
	if ( value )
	{
		gfx_text( &a->g, 18, r.x + r.w * 0.5f,
		          r.y + ( r.h - gfx_line_height( &a->g, 18 ) ) * 0.5f, value,
		          fg, GFX_CENTRE, 1.0f );
	}
	return step;
}

/**
  * @brief  Remove the last UTF-8 character of a string.
  * @param  s String.
  * @retval None.
  */
static void ui_pop_char( char *s )
{
	size_t n = strlen( s );

	while ( n > 0 )
	{
		n--;
		if ( ( (unsigned char)s[n] & 0xC0 ) != 0x80 )
		{
			break;
		}
	}
	s[n] = '\0';
}

/**
  * @brief  Count UTF-8 characters.
  * @param  s String.
  * @retval Number of code points.
  */
static int ui_chars( const char *s )
{
	int n = 0;

	for ( ; *s; s++ )
	{
		n += ( (unsigned char)*s & 0xC0 ) != 0x80;
	}
	return n;
}

/**
  * @brief  Draw a text field and edit its contents when focused.
  * @param  a Application state.
  * @param  r Rectangle.
  * @param  buf Text buffer.
  * @param  cap Buffer size in bytes.
  * @param  maxc Longest text in characters.
  * @param  allowed Characters accepted, NULL for any.
  * @param  hint Text shown while empty and unfocused, may be NULL.
  * @retval None.
  */
static void ui_field( app *a, ui_rect r, char *buf, size_t cap, int maxc,
                      const char *allowed, const char *hint )
{
	int         focus = ui_take( a, r );
	const char *p;
	char        one[8];
	size_t      n, k;
	float       tw;

	if ( focus )
	{
		if ( a->in.back )
		{
			ui_pop_char( buf );
		}
		for ( p = a->in.text; *p; )
		{
			n = 1;
			while ( ( (unsigned char)p[n] & 0xC0 ) == 0x80 )
			{
				n++;
			}
			if ( n < sizeof one && ui_chars( buf ) < maxc &&
			     strlen( buf ) + n < cap &&
			     ( !allowed || ( n == 1 && strchr( allowed, *p ) ) ) &&
			     (unsigned char)*p >= 32 )
			{
				memcpy( one, p, n );
				one[n] = '\0';
				k = strlen( buf );
				memcpy( buf + k, one, n + 1 );
			}
			p += n;
		}
		if ( a->in.enter )
		{
			a->in.enter = 0;
			a->in.down = 1;  // Enter moves on to the next field.
		}
	}
	gfx_rrect( &a->g, r.x, r.y, r.w, r.h, 8, ui_color( a, ART_UI_BG ), 1.0f );
	gfx_ring( &a->g, r.x, r.y, r.w, r.h, 8, focus ? 2.2f : 1.2f,
	          focus ? ui_color( a, ART_UI_ACCENT2 ) : ui_color( a, ART_UI_BORDER ),
	          1.0f );
	if ( !buf[0] && !focus && hint )
	{
		gfx_text( &a->g, 18, r.x + 12, r.y + ( r.h - 22 ) * 0.5f, hint,
		          ui_color( a, ART_UI_TEXT_DIM ), GFX_LEFT, 0.6f );
	}
	gfx_clip( &a->g, r.x + 4, r.y, r.w - 8, r.h );
	tw = gfx_text( &a->g, 18, r.x + 12, r.y + ( r.h - 22 ) * 0.5f, buf,
	               ui_color( a, ART_UI_TEXT ), GFX_LEFT, 1.0f );
	if ( focus && ( a->now / 500 ) % 2 == 0 )
	{
		gfx_fill( &a->g, r.x + 13 + tw, r.y + 8, 1.6f, r.h - 16,
		          ui_color( a, ART_UI_TEXT ), 1.0f );
	}
	gfx_unclip( &a->g );
}

/**
  * @brief  Draw a screen title.
  * @param  a Application state.
  * @param  title Text.
  * @retval None.
  */
static void ui_title( app *a, const char *title )
{
	gfx_text( &a->g, 34, 480, 26, title, ui_color( a, ART_UI_TEXT ), GFX_CENTRE,
	          1.0f );
	gfx_rrect( &a->g, 440, 72, 80, 4, 2, ui_color( a, ART_UI_ACCENT ), 1.0f );
}

/**
  * @brief  Draw a row label on the left of a control.
  * @param  a Application state.
  * @param  x Left edge.
  * @param  y Row top.
  * @param  h Row height.
  * @param  text Label.
  * @retval None.
  */
static void ui_label( app *a, float x, float y, float h, const char *text )
{
	gfx_text( &a->g, 18, x, y + ( h - gfx_line_height( &a->g, 18 ) ) * 0.5f,
	          text, ui_color( a, ART_UI_TEXT ), GFX_LEFT, 1.0f );
}

/**
  * @brief  Draw a normal row label (function-pointer twin of ui_label_s).
  * @param  a Application state.
  * @param  x Left edge.
  * @param  y Row top.
  * @param  h Row height.
  * @param  text Label.
  * @retval None.
  */
static void ui_label_l( app *a, float x, float y, float h, const char *text )
{
	ui_label( a, x, y, h, text );
}

/**
  * @brief  Draw a smaller row label.
  * @param  a Application state.
  * @param  x Left edge.
  * @param  y Row top.
  * @param  h Row height.
  * @param  text Label.
  * @retval None.
  */
static void ui_label_s( app *a, float x, float y, float h, const char *text )
{
	gfx_text( &a->g, 16, x, y + ( h - gfx_line_height( &a->g, 16 ) ) * 0.5f,
	          text, ui_color( a, ART_UI_TEXT ), GFX_LEFT, 1.0f );
}

/**
  * @brief  Draw a small peg icon of a seat.
  * @param  a Application state.
  * @param  seat Player seat.
  * @param  x Left.
  * @param  y Top.
  * @param  h Height.
  * @retval None.
  */
static void ui_peg_icon( app *a, int seat, float x, float y, float h )
{
	vg_tint t;

	vg_tint_make( app_player_art( a, seat ), 0, &t );
	gfx_art_fit( &a->g, ART_PEG_0, x, y, h * 0.8f, h, &t, 1.0f );
}

/**
  * @brief  Format a value that is a number of seconds.
  * @param  a Application state.
  * @param  sec Seconds.
  * @param  out Receives the text.
  * @param  cap Size of out.
  * @retval None.
  */
static void ui_secs( const app *a, int sec, char *out, size_t cap )
{
	snprintf( out, cap, tr( a->lang, S_SECONDS ), sec );
}

/**
  * @brief  Step an index in a list of values.
  * @param  list Values.
  * @param  n Number of values.
  * @param  cur Current value.
  * @param  step -1 or +1.
  * @retval The next value, wrapping around.
  */
static int ui_cycle( const int *list, int n, int cur, int step )
{
	int i, k = 0;

	for ( i = 0; i < n; i++ )
	{
		if ( list[i] == cur )
		{
			k = i;
		}
	}
	return list[( k + step + n ) % n];
}

/**
  * @brief  Draw the game option rows (board, time, question language).
  * @param  a Application state.
  * @param  x Label column.
  * @param  cx Control column.
  * @param  cw Control width.
  * @param  y First row top.
  * @param  pitch Row pitch.
  * @retval None.
  */
static void ui_game_options( app *a, float x, float cx, float cw, float y,
                             float pitch )
{
	char text[32];
	int  s;
	static const int qlangs[2] = { 0, 1 };
	void ( *ui_label )( app *, float, float, float, const char * ) =
	    cw < 200 ? ui_label_s : ui_label_l;

	ui_label( a, x, y, 40, tr( a->lang, S_BOARD ) );
	snprintf( text, sizeof text, "%d x %d", a->board, a->board );
	if ( ( s = ui_choice( a, ( ui_rect ){ cx, y, cw, 40 }, text ) ) != 0 )
	{
		a->board = ui_cycle( ui_boards, 2, a->board, s );
	}
	y += pitch;
	ui_label( a, x, y, 40, tr( a->lang, S_TIME ) );
	ui_secs( a, a->timer_sec, text, sizeof text );
	if ( ( s = ui_choice( a, ( ui_rect ){ cx, y, cw, 40 }, text ) ) != 0 )
	{
		a->timer_sec = ui_cycle( ui_times, 5, a->timer_sec, s );
	}
	y += pitch;
	ui_label( a, x, y, 40, tr( a->lang, S_QLANG ) );
	if ( ( s = ui_choice( a, ( ui_rect ){ cx, y, cw, 40 },
	                      a->qlang == 1 ? "Hrvatski" : "English" ) ) != 0 )
	{
		a->qlang = ui_cycle( qlangs, 2, a->qlang, s );
	}
}

/**
  * @brief  Draw the list of the four seats of a lobby.
  * @param  a Application state.
  * @param  x Left.
  * @param  y Top.
  * @param  w Width.
  * @retval None.
  */
static void ui_player_list( app *a, float x, float y, float w )
{
	int         i;
	const char *tag;
	char        name[LAN_NAME_SIZE];
	float       ry;
	int         conn;

	for ( i = 0; i < BOARD_NPLAYERS; i++ )
	{
		ry = y + (float)i * 38.0f;
		conn = a->demo_lobby ? i < 3 : a->lan.connected[i];
		snprintf( name, sizeof name, "%s", a->demo_lobby
		                                       ? ( i == 0 ? "Marko" : i == 1 ? "Ana" : "Ivan" )
		                                       : a->lan.names[i] );
		gfx_rrect( &a->g, x, ry, w, 33, 9, ui_color( a, ART_UI_BG ),
		           conn ? 0.9f : 0.4f );
		if ( conn )
		{
			ui_peg_icon( a, i, x + 10, ry + 3, 27 );
			gfx_text( &a->g, 17, x + 48, ry + 6, name,
			          ui_color( a, ART_UI_TEXT ), GFX_LEFT, 1.0f );
			tag = i == 0 ? tr( a->lang, S_HOST_TAG )
			             : ( a->lan.role == LAN_CLIENT &&
			                         i == a->lan.local_player
			                     ? tr( a->lang, S_YOU ) : NULL );
			if ( tag )
			{
				gfx_text( &a->g, 14, x + w - 12, ry + 9, tag,
				          ui_color( a, ART_UI_TEXT_DIM ), GFX_RIGHT, 1.0f );
			}
		}
		else
		{
			gfx_text( &a->g, 16, x + 48, ry + 6, "-",
			          ui_color( a, ART_UI_TEXT_DIM ), GFX_LEFT, 0.6f );
		}
	}
}

/**
  * @brief  Draw the Back button and report it (also Esc).
  * @param  a Application state.
  * @param  x Left.
  * @param  y Top.
  * @param  label Caption.
  * @retval 1 if pressed.
  */
static int ui_back( app *a, float x, float y, const char *label )
{
	int hit = ui_button( a, ( ui_rect ){ x, y, 150, 44 }, label, 0, 1 );

	if ( a->in.esc )
	{
		a->in.esc = 0;
		hit = 1;
	}
	return hit;
}

//------------------------------------------------------------------------------
//--------------------------------- Screens ------------------------------------
//------------------------------------------------------------------------------

/**
  * @brief  Splash screen: title art fades in, holds and fades out.
  * @param  a Application state.
  * @retval None.
  */
static void ui_draw_splash( app *a )
{
	uint32_t t = a->now - a->t0;
	float    al = 1.0f;

	if ( t < UI_SPLASH_IN )
	{
		al = (float)t / UI_SPLASH_IN;
	}
	else if ( t > UI_SPLASH_HOLD )
	{
		al = 1.0f - (float)( t - UI_SPLASH_HOLD ) /
		                (float)( UI_SPLASH_END - UI_SPLASH_HOLD );
	}
	al = al < 0.0f ? 0.0f : al;
	gfx_art_fit( &a->g, ART_TITLE_BOARD, 150, 40, 660, 360, NULL, al );
	gfx_text( &a->g, 76, 480, 392, "SRAZ", ui_color( a, ART_UI_TEXT ),
	          GFX_CENTRE, al );
	gfx_text( &a->g, 22, 480, 482, tr( a->lang, S_SUBTITLE ),
	          ui_color( a, ART_UI_TEXT_DIM ), GFX_CENTRE, al );
	if ( a->in.key_any || t > UI_SPLASH_END )
	{
		app_goto( a, APP_MENU );
	}
}

/**
  * @brief  Main menu.
  * @param  a Application state.
  * @retval None.
  */
static void ui_draw_menu( app *a )
{
	static const struct { int id; app_screen to; } items[5] = {
		{ S_PLAY, APP_SETUP }, { S_HOST, APP_HOST }, { S_JOIN, APP_JOIN },
		{ S_OPTIONS, APP_OPTIONS }, { S_QUIT, APP_SPLASH }
	};
	char     text[64];
	int      i;
	ui_rect  r;
	vg_tint  tint;

	gfx_art_fit( &a->g, ART_EMBLEM, 80, 150, 190, 228, NULL, 0.95f );
	gfx_art_fit( &a->g, ART_TITLE_BOARD, 690, 190, 250, 156, NULL, 0.95f );
	gfx_art_fit( &a->g, ART_LOGO_MARK, 438, 20, 84, 84, NULL, 1.0f );
	gfx_text( &a->g, 62, 480, 116, "SRAZ", ui_color( a, ART_UI_TEXT ),
	          GFX_CENTRE, 1.0f );
	gfx_text( &a->g, 19, 480, 200, tr( a->lang, S_SUBTITLE ),
	          ui_color( a, ART_UI_TEXT_DIM ), GFX_CENTRE, 1.0f );
	for ( i = 0; i < 5; i++ )
	{
		r = ( ui_rect ){ 330, 246.0f + (float)i * 53.0f, 300, 44 };
		if ( ui_button( a, r, tr( a->lang, items[i].id ), i == 0, 1 ) )
		{
			if ( i == 4 )
			{
				a->running = 0;
			}
			else
			{
				app_goto( a, items[i].to );
			}
		}
	}
	snprintf( text, sizeof text, "%s %s", tr( a->lang, S_VERSION ),
	          SRAZ_VERSION );
	gfx_text( &a->g, 13, 480, 516, text, ui_color( a, ART_UI_TEXT_DIM ),
	          GFX_CENTRE, 0.8f );
	memset( &tint, 0, sizeof tint );
	tint.tok[ART_TOK_C] = ui_color( a, ART_UI_TEXT );
	r = ( ui_rect ){ 908, 14, 38, 38 };
	if ( ui_inside( a, r ) )
	{
		gfx_rrect( &a->g, r.x, r.y, r.w, r.h, 9, ui_color( a, ART_UI_PANEL_HI ),
		           1.0f );
		if ( a->in.click )
		{
			a->sound = !a->sound;
			app_save_cfg( a );
		}
	}
	gfx_art_fit( &a->g, a->sound ? ART_ICON_SOUND_ON : ART_ICON_SOUND_OFF,
	             r.x + 7, r.y + 7, 24, 24, &tint, 1.0f );
	if ( a->in.esc )
	{
		a->in.esc = 0;
	}
}

/**
  * @brief  Setup of a hot-seat game.
  * @param  a Application state.
  * @retval None.
  */
static void ui_draw_setup( app *a )
{
	char    text[32], hint[32];
	int     i, s;
	ui_rect r;
	float   x, y;

	ui_title( a, tr( a->lang, S_SETUP_TITLE ) );
	ui_panel( a, ( ui_rect ){ 150, 92, 660, 352 } );
	ui_label( a, 180, 108, 40, tr( a->lang, S_PLAYERS ) );
	snprintf( text, sizeof text, "%d", a->players );
	if ( ( s = ui_choice( a, ( ui_rect ){ 520, 108, 260, 40 }, text ) ) != 0 )
	{
		a->players = a->players + s;
		a->players = a->players < 2 ? 4 : ( a->players > 4 ? 2 : a->players );
	}
	for ( i = 0; i < a->players; i++ )
	{
		x = i % 2 == 0 ? 180.0f : 490.0f;
		y = 162.0f + (float)( i / 2 ) * 50.0f;
		ui_peg_icon( a, i, x, y + 2, 36 );
		r = ( ui_rect ){ x + 32, y, 240, 40 };
		app_default_name( a, i, hint, sizeof hint );
		ui_field( a, r, a->names[i], sizeof a->names[i], APP_NAME_MAX, NULL,
		          hint );
	}
	ui_game_options( a, 180, 520, 260, 268, 52 );
	if ( ui_back( a, 150, 466, tr( a->lang, S_BACK ) ) )
	{
		app_goto( a, APP_MENU );
		app_save_cfg( a );
	}
	if ( ui_button( a, ( ui_rect ){ 610, 466, 200, 44 },
	                tr( a->lang, S_START ), 1, 1 ) )
	{
		app_save_cfg( a );
		app_start_local( a );
	}
}

/**
  * @brief  Copy an address and port to the clipboard.
  * @param  a Application state.
  * @param  addr Dotted address.
  * @retval None.
  */
static void ui_copy_address( app *a, const char *addr )
{
	char text[96];

	snprintf( text, sizeof text, "%s:%s", addr, a->host_port );
	SDL_SetClipboardText( text );
	app_msg( a, tr( a->lang, S_COPIED ), 0 );
}

/**
  * @brief  LAN host screen: options before the lobby, address and players
  *         once it is open.
  * @param  a Application state.
  * @retval None.
  */
static void ui_draw_host( app *a )
{
	int      open = a->lan.role == LAN_HOST || a->demo_lobby, i, n, ready, s;
	char     text[96];
	float    y;
	ui_rect  r;

	ui_title( a, tr( a->lang, S_HOST_TITLE ) );
	if ( !open )
	{
		ui_panel( a, ( ui_rect ){ 190, 92, 580, 330 } );
		ui_label( a, 220, 108, 40, tr( a->lang, S_YOUR_NAME ) );
		app_default_name( a, 0, text, sizeof text );
		ui_field( a, ( ui_rect ){ 440, 108, 300, 40 }, a->my_name,
		          sizeof a->my_name, APP_NAME_MAX, NULL, text );
		ui_label( a, 220, 160, 40, tr( a->lang, S_PORT ) );
		ui_field( a, ( ui_rect ){ 440, 160, 300, 40 }, a->host_port,
		          sizeof a->host_port, 5, "0123456789", NULL );
		ui_game_options( a, 220, 440, 300, 212, 52 );
		if ( ui_back( a, 190, 450, tr( a->lang, S_BACK ) ) )
		{
			app_goto( a, APP_MENU );
			app_save_cfg( a );
		}
		if ( ui_button( a, ( ui_rect ){ 570, 450, 200, 44 },
		                tr( a->lang, S_OPEN_LOBBY ), 1, 1 ) )
		{
			app_host_open( a );
		}
		return;
	}
	n = a->demo_lobby ? 3 : lan_player_count( &a->lan );
	ready = n >= 2;
	ui_panel( a, ( ui_rect ){ 40, 92, 500, 376 } );
	gfx_text_wrap( &a->g, 17, 62, 106, 456, tr( a->lang, S_GIVE_ADDR ),
	               ui_color( a, ART_UI_TEXT_DIM ), GFX_LEFT, 1.0f, 1 );
	y = 136;
	if ( a->naddr == 0 )
	{
		gfx_text_wrap( &a->g, 17, 62, y, 456, tr( a->lang, S_NO_ADDR ),
		               ui_color( a, ART_UI_BAD ), GFX_LEFT, 1.0f, 1 );
		y += 66;
	}
	for ( i = 0; i < a->naddr && i < 2; i++ )
	{
		r = ( ui_rect ){ 62, y, 456, 54 };
		gfx_rrect( &a->g, r.x, r.y, r.w, r.h, 12, ui_color( a, ART_UI_BG ),
		           1.0f );
		gfx_ring( &a->g, r.x, r.y, r.w, r.h, 12, 2.0f,
		          ui_color( a, ART_UI_ACCENT ), 1.0f );
		snprintf( text, sizeof text, "%s : %s", a->addrs[i], a->host_port );
		gfx_text( &a->g, 30, r.x + 16, r.y + 8, text,
		          ui_color( a, ART_UI_TEXT ), GFX_LEFT, 1.0f );
		if ( ui_button( a, ( ui_rect ){ r.x + r.w - 92, r.y + 7, 82, 40 },
		                tr( a->lang, S_COPY ), 0, 1 ) )
		{
			ui_copy_address( a, a->addrs[i] );
		}
		y += 64;
	}
	y += 6;
	gfx_text( &a->g, 15, 62, y, tr( a->lang, S_CONNECTED_PLAYERS ),
	          ui_color( a, ART_UI_TEXT_DIM ), GFX_LEFT, 1.0f );
	ui_player_list( a, 62, y + 24, 456 );
	ui_panel( a, ( ui_rect ){ 560, 92, 360, 376 } );
	ui_game_options( a, 582, 748, 150, 112, 58 );
	if ( !ready )
	{
		gfx_text_wrap( &a->g, 15, 582, 312, 316, tr( a->lang, S_WAIT_PLAYERS ),
		               ui_color( a, ART_UI_TEXT_DIM ), GFX_CENTRE, 1.0f, 1 );
	}
	if ( ui_back( a, 40, 484, tr( a->lang, S_CLOSE_LOBBY ) ) )
	{
		if ( !a->demo_lobby )
		{
			app_lan_leave( a, APP_MENU );
		}
	}
	s = ui_button( a, ( ui_rect ){ 720, 482, 200, 48 },
	               tr( a->lang, S_START_GAME ), 1, ready );
	if ( s && !a->demo_lobby )
	{
		app_host_start( a );
	}
}

/**
  * @brief  LAN join screen: address entry, connecting, waiting for the host.
  * @param  a Application state.
  * @retval None.
  */
static void ui_draw_join( app *a )
{
	char    text[96];
	int     dots;

	ui_title( a, tr( a->lang, S_JOIN_TITLE ) );
	ui_panel( a, ( ui_rect ){ 190, 92, 580, a->lan.role == LAN_OFF ? 250 : 330 } );
	if ( a->lan.role == LAN_OFF )
	{
		ui_label( a, 220, 108, 40, tr( a->lang, S_ADDRESS ) );
		ui_field( a, ( ui_rect ){ 440, 108, 300, 40 }, a->join_addr,
		          sizeof a->join_addr, 15, "0123456789.", "192.168.0.112" );
		ui_label( a, 220, 160, 40, tr( a->lang, S_PORT ) );
		ui_field( a, ( ui_rect ){ 440, 160, 300, 40 }, a->join_port,
		          sizeof a->join_port, 5, "0123456789", NULL );
		ui_label( a, 220, 212, 40, tr( a->lang, S_YOUR_NAME ) );
		app_default_name( a, -1, text, sizeof text );
		ui_field( a, ( ui_rect ){ 440, 212, 300, 40 }, a->my_name,
		          sizeof a->my_name, APP_NAME_MAX, NULL, text );
		if ( a->msg[0] && a->msg_err && a->now < a->msg_until + 4000 )
		{
			gfx_text_wrap( &a->g, 16, 220, 270, 520, a->msg,
			               ui_color( a, ART_UI_BAD ), GFX_LEFT, 1.0f, 1 );
		}
		if ( ui_back( a, 190, 366, tr( a->lang, S_BACK ) ) )
		{
			app_goto( a, APP_MENU );
			app_save_cfg( a );
		}
		if ( ui_button( a, ( ui_rect ){ 570, 366, 200, 44 },
		                tr( a->lang, S_CONNECT ), 1, 1 ) )
		{
			app_join_start( a );
		}
		return;
	}
	if ( !a->lan_joined )
	{
		dots = (int)( a->now / 350 ) % 4;
		snprintf( text, sizeof text, "%s", tr( a->lang, S_CONNECTING ) );
		gfx_text( &a->g, 24, 480, 190, text, ui_color( a, ART_UI_TEXT ),
		          GFX_CENTRE, 1.0f );
		snprintf( text, sizeof text, "%s : %s", a->join_addr, a->join_port );
		gfx_text( &a->g, 20, 480, 240, text, ui_color( a, ART_UI_TEXT_DIM ),
		          GFX_CENTRE, 1.0f );
		gfx_rrect( &a->g, 440 + (float)dots * 20, 296, 14, 14, 7,
		           ui_color( a, ART_UI_ACCENT ), 1.0f );
	}
	else
	{
		gfx_text_wrap( &a->g, 19, 220, 108, 520, tr( a->lang, S_WAIT_HOST ),
		               ui_color( a, ART_UI_TEXT ), GFX_CENTRE, 1.0f, 1 );
		ui_player_list( a, 250, 170, 460 );
	}
	if ( ui_back( a, 190, 450, tr( a->lang, S_DISCONNECT ) ) )
	{
		app_lan_leave( a, APP_JOIN );
	}
}

/**
  * @brief  Options screen.
  * @param  a Application state.
  * @retval None.
  */
static void ui_draw_options( app *a )
{
	float y = 108;
	int   s, i;

	ui_title( a, tr( a->lang, S_OPTIONS ) );
	ui_panel( a, ( ui_rect ){ 190, 92, 580, 340 } );
	ui_label( a, 220, y, 40, tr( a->lang, S_SOUND ) );
	if ( ui_choice( a, ( ui_rect ){ 460, y, 280, 40 },
	                tr( a->lang, a->sound ? S_ON : S_OFF ) ) )
	{
		a->sound = !a->sound;
		snd_enable( a->sound );
		snd_play( SND_CLICK );
	}
	y += 52;
	ui_label( a, 220, y, 40, tr( a->lang, S_LANGUAGE ) );
	if ( ui_choice( a, ( ui_rect ){ 460, y, 280, 40 },
	                a->lang == 1 ? "Hrvatski" : "English" ) )
	{
		a->lang = !a->lang;
		a->qlang = a->lang;
	}
	y += 52;
	ui_label( a, 220, y, 40, tr( a->lang, S_FULLSCREEN ) );
	if ( ui_choice( a, ( ui_rect ){ 460, y, 280, 40 },
	                tr( a->lang, a->fullscreen ? S_ON : S_OFF ) ) )
	{
		app_toggle_fullscreen( a );
	}
	y += 52;
	ui_label( a, 220, y, 40, tr( a->lang, S_THEME ) );
	if ( ui_choice( a, ( ui_rect ){ 460, y, 280, 40 },
	                tr( a->lang, a->dark ? S_DARK : S_LIGHT ) ) )
	{
		a->dark = !a->dark;
	}
	y += 52;
	ui_label( a, 220, y, 40, tr( a->lang, S_ARMY ) );
	s = ui_choice( a, ( ui_rect ){ 460, y, 280, 40 }, NULL );
	if ( s )
	{
		a->army = ( a->army + s + 4 ) % 4;
	}
	for ( i = 0; i < 4; i++ )
	{
		ui_peg_icon( a, i, 540 + (float)i * 40, y + 3, 34 );
	}
	if ( ui_back( a, 190, 450, tr( a->lang, S_BACK ) ) )
	{
		app_save_cfg( a );
		app_goto( a, APP_MENU );
	}
}

/**
  * @brief  Draw the message toast.
  * @param  a Application state.
  * @retval None.
  */
static void ui_toast( app *a )
{
	float w;

	if ( !a->msg[0] || a->now >= a->msg_until )
	{
		return;
	}
	w = gfx_text_width( &a->g, 17, a->msg ) + 40;
	w = w > 900 ? 900 : w;
	gfx_rrect( &a->g, 480 - w * 0.5f, 492, w, 36, 18,
	           a->msg_err ? ui_color( a, ART_UI_BAD ) : 0x1D2333, 0.94f );
	gfx_text( &a->g, 17, 480, 499, a->msg, 0xFFFFFF, GFX_CENTRE, 1.0f );
}

//------------------------------------------------------------------------------
//------------------------------- Public API -----------------------------------
//------------------------------------------------------------------------------

int ui_btn( app *a, float x, float y, float w, float h, const char *label,
            int kind, int enabled )
{
	return ui_button( a, ( ui_rect ){ x, y, w, h }, label, kind, enabled );
}

int ui_hit( const app *a, float x, float y, float w, float h )
{
	return ui_inside( a, ( ui_rect ){ x, y, w, h } );
}

void ui_frame( app *a )
{
	a->wid = 0;
	switch ( a->screen )
	{
		case APP_SPLASH: ui_draw_splash( a ); break;
		case APP_MENU: ui_draw_menu( a ); break;
		case APP_SETUP: ui_draw_setup( a ); break;
		case APP_HOST: ui_draw_host( a ); break;
		case APP_JOIN: ui_draw_join( a ); break;
		case APP_OPTIONS: ui_draw_options( a ); break;
		case APP_GAME: ui_game( a ); break;
		case APP_WINNER: ui_winner( a ); break;
	}
	a->nwid = a->wid;
	if ( a->screen != APP_GAME && a->screen != APP_WINNER &&
	     a->screen != APP_SPLASH && a->nwid > 0 )
	{
		if ( a->in.up || a->in.stab )
		{
			a->focus = ( a->focus + a->nwid - 1 ) % a->nwid;
		}
		else if ( a->in.down || a->in.tab )
		{
			a->focus = ( a->focus + 1 ) % a->nwid;
		}
	}
	ui_toast( a );
}
