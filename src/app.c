//  Description:
//
//		app.c -- application logic: options file, game setup, LAN glue.
//
//		LAN roles (the host is authoritative, as in lan.h):
//
//		  host    owns the sg_game, applies commands of the guests through
//		          sg_move / sg_answer / sg_pass, publishes a snapshot ten times
//		          a second and whenever the game epoch changes.
//		  guest   sends LAN_MOVE / LAN_ANSWER / LAN_PASS with the epoch of the
//		          last snapshot and mirrors every snapshot with sg_apply().
//
//		sraz.cfg holds one key=value pair per line.
//

//------------------------------------------------------------------------------
//---------------------------------- Includes ----------------------------------
//------------------------------------------------------------------------------

#include "app.h"
#include "snd.h"
#include "strs.h"
#include "vg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//------------------------------------------------------------------------------
//------------------------- Local macros and constants -------------------------
//------------------------------------------------------------------------------

#define APP_PUBLISH_MS 100

//------------------------------------------------------------------------------
//------------------------------- Local functions ------------------------------
//------------------------------------------------------------------------------

/**
  * @brief  Build a path next to the executable.
  * @param  a Application state.
  * @param  name File name.
  * @param  out Receives the path.
  * @param  cap Size of out.
  * @retval None.
  */
static void app_path( const app *a, const char *name, char *out, size_t cap )
{
	snprintf( out, cap, "%.1024s%s", a->base, name );
}

/**
  * @brief  Parse a TCP port without accepting signs or junk.
  * @param  text Decimal text.
  * @retval Port 1..65535, -1 if invalid.
  */
static int app_parse_port( const char *text )
{
	long  v;
	char *end;

	if ( !text[0] || strspn( text, "0123456789" ) != strlen( text ) )
	{
		return -1;
	}
	v = strtol( text, &end, 10 );
	return *end || v < 1 || v > 65535 ? -1 : (int)v;
}

/**
  * @brief  Seed for a new board.
  * @param  a Application state.
  * @retval Nonzero seed.
  */
static uint32_t app_seed( const app *a )
{
	return ( ( (uint32_t)time( NULL ) * 2654435761u ) ^ ( a->now * 40503u ) ) | 1u;
}

/**
  * @brief  Make the first lobby board so snapshots are always valid.
  * @param  a Application state.
  * @retval None.
  */
static void app_lobby_board( app *a )
{
	memset( &a->game, 0, sizeof a->game );
	sg_start( &a->game, NULL, 8, 2, NULL, a->timer_sec, a->qlang,
	          app_seed( a ), 1 );
	a->game.scene = SG_SCENE_LOBBY;
	a->pub_at = 0;
}

/**
  * @brief  Apply a command from a guest.
  * @param  a Application state (host).
  * @param  player Seat of the sender.
  * @param  c Command.
  * @retval None.
  */
static void app_lan_command( app *a, int player, const lan_command *c )
{
	if ( c->epoch != a->game.epoch || player < 1 ||
	     player >= a->game.board.nplayers )
	{
		return;
	}
	switch ( c->kind )
	{
		case LAN_MOVE:
			sg_move( &a->game, player, c->peg, c->row, c->col, a->now );
			break;
		case LAN_ANSWER:
			sg_answer( &a->game, player, c->answer, a->now );
			break;
		case LAN_PASS:
			sg_pass( &a->game, player, a->now );
			break;
		default:
			break;
	}
}

/**
  * @brief  Turn game events into sounds and messages.
  * @param  a Application state.
  * @retval None.
  */
static void app_events( app *a )
{
	char msg[160];
	int  ev, i;

	while ( ( ev = sg_event( &a->game ) ) != SG_EV_NONE )
	{
		switch ( ev )
		{
			case SG_EV_CORRECT: snd_play( SND_CORRECT ); break;
			case SG_EV_WRONG: snd_play( SND_WRONG ); break;
			case SG_EV_LOST: snd_play( SND_LOST ); break;
			case SG_EV_TURN: snd_play( SND_TURN ); break;
			case SG_EV_WIN: snd_play( SND_WIN ); break;
			case SG_EV_SKIP:
				for ( i = 0; i < BOARD_NPLAYERS; i++ )
				{
					if ( a->game.skipped[i] )
					{
						snprintf( msg, sizeof msg, tr( a->lang, S_SKIPPED ),
						          a->game.names[i] );
						app_msg( a, msg, 0 );
						break;
					}
				}
				break;
			default:
				break;
		}
	}
}

/**
  * @brief  Follow the host's state on a guest: pick the right screen.
  * @param  a Application state (guest).
  * @retval None.
  */
static void app_guest_screen( app *a )
{
	int scene = a->game.scene;

	if ( !a->lan_joined )
	{
		return;
	}
	if ( scene != SG_SCENE_OVER )
	{
		a->winner_ack = 0;
	}
	if ( scene == SG_SCENE_LOBBY )
	{
		if ( a->screen == APP_GAME || a->screen == APP_WINNER )
		{
			app_goto( a, APP_JOIN );
		}
	}
	else if ( scene == SG_SCENE_OVER )
	{
		if ( a->screen == APP_GAME && !a->winner_ack )
		{
			app_goto( a, APP_WINNER );
		}
	}
	else if ( a->screen != APP_GAME )
	{
		app_goto( a, APP_GAME );
		a->sel_turn = -1;
		a->quit_dlg = 0;
	}
}

//------------------------------------------------------------------------------
//------------------------------- Public API -----------------------------------
//------------------------------------------------------------------------------

void app_defaults( app *a )
{
	int i;

	a->sound = 1;
	a->lang = 0;
	a->fullscreen = 0;
	a->dark = 1;
	a->army = 0;
	a->join_addr[0] = '\0';
	snprintf( a->join_port, sizeof a->join_port, "%d", LAN_DEFAULT_PORT );
	snprintf( a->host_port, sizeof a->host_port, "%d", LAN_DEFAULT_PORT );
	a->my_name[0] = '\0';
	a->players = 2;
	a->board = 8;
	a->timer_sec = 15;
	a->qlang = 0;
	for ( i = 0; i < BOARD_NPLAYERS; i++ )
	{
		a->names[i][0] = '\0';  // Empty: the default name of the language is used.
	}
	a->quiz_lang = -1;
	a->sel_turn = -1;
}

void app_default_name( const app *a, int seat, char *out, size_t cap )
{
	if ( seat < 0 )
	{
		snprintf( out, cap, "%s", tr( a->lang, S_DEFAULT_NAME ) );
	}
	else
	{
		snprintf( out, cap, "%s %d", tr( a->lang, S_DEFAULT_NAME ), seat + 1 );
	}
}

void app_player_name( const app *a, int seat, char *out, size_t cap )
{
	if ( a->names[seat][0] )
	{
		snprintf( out, cap, "%.12s", a->names[seat] );
	}
	else
	{
		app_default_name( a, seat, out, cap );
	}
}

void app_load_cfg( app *a )
{
	char  path[APP_PATH_MAX], line[256], *eq, *v;
	FILE *f;
	int   n;

	app_path( a, "sraz.cfg", path, sizeof path );
	f = fopen( path, "r" );
	if ( !f )
	{
		return;
	}
	while ( fgets( line, sizeof line, f ) )
	{
		line[strcspn( line, "\r\n" )] = '\0';
		eq = strchr( line, '=' );
		if ( !eq )
		{
			continue;
		}
		*eq = '\0';
		v = eq + 1;
		n = atoi( v );
		if ( !strcmp( line, "sound" ) ) { a->sound = n != 0; }
		else if ( !strcmp( line, "lang" ) ) { a->lang = n == 1; a->qlang = a->lang; }
		else if ( !strcmp( line, "fullscreen" ) ) { a->fullscreen = n != 0; }
		else if ( !strcmp( line, "dark" ) ) { a->dark = n != 0; }
		else if ( !strcmp( line, "army" ) ) { a->army = n < 0 || n > 3 ? 0 : n; }
		else if ( !strcmp( line, "board" ) ) { a->board = n == 10 ? 10 : 8; }
		else if ( !strcmp( line, "time" ) )
		{
			a->timer_sec = n == 9 || n == 15 || n == 30 || n == 60 || n == 600
			                   ? n : 15;
		}
		else if ( !strcmp( line, "join_addr" ) )
		{
			snprintf( a->join_addr, sizeof a->join_addr, "%.63s", v );
		}
		else if ( !strcmp( line, "join_port" ) && app_parse_port( v ) > 0 )
		{
			snprintf( a->join_port, sizeof a->join_port, "%s", v );
		}
		else if ( !strcmp( line, "host_port" ) && app_parse_port( v ) > 0 )
		{
			snprintf( a->host_port, sizeof a->host_port, "%s", v );
		}
		else if ( !strcmp( line, "name" ) )
		{
			snprintf( a->my_name, sizeof a->my_name, "%.12s", v );
		}
	}
	fclose( f );
}

void app_save_cfg( const app *a )
{
	char  path[APP_PATH_MAX];
	FILE *f;

	app_path( a, "sraz.cfg", path, sizeof path );
	f = fopen( path, "w" );
	if ( !f )
	{
		return;
	}
	fprintf( f, "sound=%d\nlang=%d\nfullscreen=%d\ndark=%d\narmy=%d\n", a->sound,
	         a->lang, a->fullscreen, a->dark, a->army );
	fprintf( f, "board=%d\ntime=%d\njoin_addr=%s\njoin_port=%s\n", a->board,
	         a->timer_sec, a->join_addr, a->join_port );
	fprintf( f, "host_port=%s\nname=%s\n", a->host_port, a->my_name );
	fclose( f );
}

void app_goto( app *a, app_screen s )
{
	a->screen = s;
	a->t0 = a->now;
	a->focus = 0;
	a->quit_dlg = 0;
}

void app_msg( app *a, const char *text, int err )
{
	snprintf( a->msg, sizeof a->msg, "%s", text );
	a->msg_until = a->now + 3200;
	a->msg_err = err;
}

int app_load_quiz( app *a, int lang )
{
	char path[APP_PATH_MAX], err[64];
	int  rc;

	if ( a->quiz_lang == lang && a->quiz.q )
	{
		return 0;
	}
	if ( a->data_override )
	{
		snprintf( path, sizeof path, "%s", a->data_override );
	}
	else
	{
		snprintf( path, sizeof path, "%.1024squestions/%s", a->base,
		          lang == 1 ? "cro" : "en" );
	}
	rc = quiz_load( &a->quiz, path, lang == 1 ? QUIZ_LANGUAGE_CRO
	                                          : QUIZ_LANGUAGE_EN );
	if ( rc != 0 )
	{
		snprintf( err, sizeof err, "Cannot load the questions (error %d)", rc );
		app_msg( a, err, 1 );
		return rc;
	}
	a->quiz_lang = lang;
	return 0;
}

int app_player_art( const app *a, int seat )
{
	return vg_army_player( a->army, seat );
}

int app_start_local( app *a )
{
	char names[BOARD_NPLAYERS][LAN_NAME_SIZE];
	int  i;

	if ( app_load_quiz( a, a->qlang ) != 0 )
	{
		return -1;
	}
	memset( names, 0, sizeof names );
	for ( i = 0; i < BOARD_NPLAYERS; i++ )
	{
		app_player_name( a, i, names[i], sizeof names[i] );
	}
	sg_start( &a->game, &a->quiz, a->board, a->players,
	          (const char( * )[LAN_NAME_SIZE])names, a->timer_sec, a->qlang,
	          app_seed( a ), 0xF );
	sgs_reset( &a->sel, &a->game.board, 0 );
	a->sel_turn = 0;
	app_goto( a, APP_GAME );
	return 0;
}

int app_host_open( app *a )
{
	char name[LAN_NAME_SIZE];
	int  port = app_parse_port( a->host_port ), rc;

	if ( port < 0 )
	{
		app_msg( a, tr( a->lang, S_BAD_PORT ), 1 );
		return -1;
	}
	if ( a->my_name[0] )
	{
		snprintf( name, sizeof name, "%.12s", a->my_name );
	}
	else
	{
		app_player_name( a, 0, name, sizeof name );
	}
	rc = lan_host( &a->lan, (uint16_t)port, name );
	if ( rc != 0 )
	{
		snprintf( a->lan_note, sizeof a->lan_note,
		          "Cannot open the lobby (error %d). Is the port in use?", rc );
		app_msg( a, a->lan_note, 1 );
		return -1;
	}
	a->naddr = lan_local_addresses( a->addrs, 4 );
	app_lobby_board( a );
	app_save_cfg( a );
	return 0;
}

int app_host_start( app *a )
{
	int n = lan_player_count( &a->lan );

	if ( a->lan.role != LAN_HOST || n < 2 )
	{
		return -1;
	}
	if ( app_load_quiz( a, a->qlang ) != 0 )
	{
		return -1;
	}
	if ( lan_begin( &a->lan ) != 0 )
	{
		return -1;
	}
	sg_start( &a->game, &a->quiz, a->board, n,
	          (const char( * )[LAN_NAME_SIZE])a->lan.names, a->timer_sec,
	          a->qlang, app_seed( a ), 1 );
	sgs_reset( &a->sel, &a->game.board, 0 );
	a->sel_turn = 0;
	a->pub_at = 0;
	app_goto( a, APP_GAME );
	return 0;
}

int app_join_start( app *a )
{
	char name[LAN_NAME_SIZE];
	int  port = app_parse_port( a->join_port ), rc;

	if ( !a->join_addr[0] )
	{
		app_msg( a, tr( a->lang, S_BAD_ADDR ), 1 );
		return -1;
	}
	if ( port < 0 )
	{
		app_msg( a, tr( a->lang, S_BAD_PORT ), 1 );
		return -1;
	}
	if ( a->my_name[0] )
	{
		snprintf( name, sizeof name, "%.12s", a->my_name );
	}
	else
	{
		app_default_name( a, -1, name, sizeof name );  // Seat is not known yet.
	}
	rc = lan_join( &a->lan, a->join_addr, (uint16_t)port, name, a->now );
	if ( rc != 0 )
	{
		snprintf( a->lan_note, sizeof a->lan_note,
		          "Cannot connect (error %d). Check the address.", rc );
		app_msg( a, a->lan_note, 1 );
		return -1;
	}
	a->lan_joined = 0;
	memset( &a->game, 0, sizeof a->game );
	a->game.scene = SG_SCENE_LOBBY;
	a->winner_ack = 0;
	app_save_cfg( a );
	return 0;
}

void app_lan_leave( app *a, app_screen to )
{
	lan_close( &a->lan );
	memset( &a->lan, 0, sizeof a->lan );
	a->lan_joined = 0;
	memset( &a->game, 0, sizeof a->game );
	a->game.scene = SG_SCENE_LOBBY;
	app_goto( a, to );
}

void app_host_lobby( app *a )
{
	lan_lobby( &a->lan );
	app_lobby_board( a );
	a->game.epoch = a->pub_epoch + 1;
	app_goto( a, APP_HOST );
}

int app_my_turn( const app *a )
{
	int turn = a->game.board.turn;

	if ( a->lan.role == LAN_CLIENT )
	{
		return turn == a->lan.local_player;
	}
	if ( a->lan.role == LAN_HOST )
	{
		return turn == 0;
	}
	return ( a->game.local_mask >> turn ) & 1;
}

void app_do_move( app *a, int peg, int r, int c )
{
	lan_command cmd;

	if ( !app_my_turn( a ) )
	{
		return;
	}
	if ( a->lan.role == LAN_CLIENT )
	{
		memset( &cmd, 0, sizeof cmd );
		cmd.kind = LAN_MOVE;
		cmd.epoch = a->game.epoch;
		cmd.peg = peg;
		cmd.row = r;
		cmd.col = c;
		lan_send_command( &a->lan, &cmd );
	}
	else
	{
		sg_move( &a->game, a->game.board.turn, peg, r, c, a->now );
	}
	sgs_cancel( &a->sel );
}

void app_do_answer( app *a, int slot )
{
	lan_command cmd;

	if ( !app_my_turn( a ) || a->game.answered )
	{
		return;
	}
	if ( a->lan.role == LAN_CLIENT )
	{
		if ( a->answer_sent && a->answer_sent_epoch == a->game.epoch )
		{
			return;
		}
		memset( &cmd, 0, sizeof cmd );
		cmd.kind = LAN_ANSWER;
		cmd.epoch = a->game.epoch;
		cmd.answer = slot;
		if ( lan_send_command( &a->lan, &cmd ) == 0 )
		{
			a->answer_sent = 1;
			a->answer_sent_epoch = a->game.epoch;
		}
	}
	else
	{
		sg_answer( &a->game, a->game.board.turn, slot, a->now );
	}
}

void app_do_pass( app *a )
{
	lan_command cmd;
	char        msg[160];

	if ( !app_my_turn( a ) || a->game.scene != SG_SCENE_BOARD )
	{
		return;
	}
	if ( a->lan.role == LAN_CLIENT )
	{
		memset( &cmd, 0, sizeof cmd );
		cmd.kind = LAN_PASS;
		cmd.epoch = a->game.epoch;
		lan_send_command( &a->lan, &cmd );
	}
	else
	{
		snprintf( msg, sizeof msg, tr( a->lang, S_PASSED ),
		          a->game.names[a->game.board.turn] );
		app_msg( a, msg, 0 );
		sg_pass( &a->game, a->game.board.turn, a->now );
	}
	sgs_cancel( &a->sel );
}

void app_save_load( app *a, int save )
{
	char path[APP_PATH_MAX];
	int  rc;

	if ( a->lan.role != LAN_OFF )
	{
		app_msg( a, tr( a->lang, save ? S_SAVE_LAN : S_LOAD_LAN ), 1 );
		return;
	}
	app_path( a, "sraz.sav", path, sizeof path );
	if ( save )
	{
		if ( a->screen != APP_GAME || a->game.scene != SG_SCENE_BOARD )
		{
			app_msg( a, tr( a->lang, S_SAVE_BUSY ), 1 );
			return;
		}
		rc = sg_save( &a->game, path );
		app_msg( a, tr( a->lang, rc == 0 ? S_SAVED : S_SAVE_FAIL ), rc != 0 );
		return;
	}
	if ( a->screen == APP_GAME && a->game.scene == SG_SCENE_QUESTION )
	{
		app_msg( a, tr( a->lang, S_SAVE_BUSY ), 1 );
		return;
	}
	if ( a->screen != APP_GAME )
	{
		memset( &a->game, 0, sizeof a->game );
	}
	a->game.quiz = &a->quiz;
	rc = sg_load( &a->game, path );
	if ( rc != 0 )
	{
		app_msg( a, tr( a->lang, rc == -1 ? S_NO_SAVE : S_SAVE_FAIL ), 1 );
		return;
	}
	if ( app_load_quiz( a, a->game.language ) != 0 )
	{
		return;
	}
	a->game.quiz = &a->quiz;
	a->game.local_mask = 0xF;
	a->game.qindex = -1;
	a->qlang = a->game.language;
	sgs_reset( &a->sel, &a->game.board, a->game.board.turn );
	a->sel_turn = a->game.board.turn;
	app_msg( a, tr( a->lang, S_LOADED ), 0 );
	if ( a->screen != APP_GAME )
	{
		app_goto( a, APP_GAME );
	}
}

void app_toggle_fullscreen( app *a )
{
	a->fullscreen = !a->fullscreen;
	SDL_SetWindowFullscreen( a->win, a->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP
	                                               : 0 );
	app_save_cfg( a );
}

void app_update( app *a )
{
	lan_event    ev;
	lan_snapshot st;
	int          n, host = a->lan.role == LAN_HOST;
	int          client = a->lan.role == LAN_CLIENT;
	char         error[160];

	snd_enable( a->sound );
	if ( a->lan.role != LAN_OFF )
	{
		for ( n = 0; n < 64; n++ )
		{
			if ( lan_poll( &a->lan, &ev, &st, a->now ) <= 0 )
			{
				break;
			}
			if ( ev.type == LAN_EVENT_ERROR )
			{
				snprintf( error, sizeof error, "%s", a->lan.error );
				app_lan_leave( a, host ? APP_HOST : APP_JOIN );
				app_msg( a, error[0] ? error : tr( a->lang, S_LEFT_LAN ), 1 );
				return;
			}
			if ( ev.type == LAN_EVENT_JOINED && client )
			{
				a->lan_joined = 1;
			}
			else if ( ev.type == LAN_EVENT_STATE && client )
			{
				a->lan_joined = 1;
				sg_apply( &a->game, &st, a->now );
				app_guest_screen( a );
			}
			else if ( ev.type == LAN_EVENT_COMMAND && host )
			{
				app_lan_command( a, ev.player, &ev.command );
			}
			else if ( ev.type == LAN_EVENT_LEFT && host &&
			          a->game.scene != SG_SCENE_LOBBY )
			{
				app_host_lobby( a );
				app_msg( a, tr( a->lang, S_PLAYER_LEFT ), 1 );
			}
		}
	}
	host = a->lan.role == LAN_HOST;
	if ( host )
	{
		if ( a->game.scene == SG_SCENE_LOBBY )
		{
			memcpy( a->game.names, a->lan.names, sizeof a->game.names );
			a->game.timer_sec = a->timer_sec;
			a->game.language = a->qlang;
		}
	}
	if ( a->lan.role != LAN_CLIENT && a->screen == APP_GAME )
	{
		sg_tick( &a->game, a->now );
		if ( a->game.scene == SG_SCENE_OVER )
		{
			app_goto( a, APP_WINNER );
		}
	}
	if ( a->lan.role == LAN_CLIENT && a->screen == APP_GAME &&
	     a->game.scene == SG_SCENE_QUESTION && !a->game.answered &&
	     app_my_turn( a ) && sg_remaining_ms( &a->game, a->now ) == 0 )
	{
		// The host cannot see the highlighted answer, so the guest sends it.
		app_do_answer( a, a->game.hilite );
	}
	if ( a->screen == APP_GAME && a->game.scene == SG_SCENE_BOARD )
	{
		if ( a->sel_turn != a->game.board.turn )
		{
			sgs_reset( &a->sel, &a->game.board, a->game.board.turn );
			a->sel_turn = a->game.board.turn;
		}
		else
		{
			sgs_validate( &a->sel, &a->game.board, a->game.board.turn );
		}
	}
	app_events( a );
	if ( a->lan.role == LAN_HOST &&
	     ( a->now - a->pub_at >= APP_PUBLISH_MS ||
	       a->game.epoch != a->pub_epoch ) )
	{
		sg_snapshot( &a->game, &a->sel, lan_player_count( &a->lan ), a->now,
		             &st );
		lan_publish( &a->lan, &st );
		a->pub_at = a->now;
		a->pub_epoch = a->game.epoch;
	}
}
