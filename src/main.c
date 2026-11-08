//  Description:
//
//		main.c -- Sraz (native, SDL2): command line, window, event loop and
//		the headless screenshot mode.
//
//		Screenshot mode (--shot out.png --scene N [--size WxH]) renders one
//		scene with SDL's dummy video driver and software renderer, reads the
//		pixels back and writes a PNG without zlib. Scenes:
//
//		   0 splash         1 main menu      2 setup          3 LAN host lobby
//		   4 LAN join       5 options        6 board 8x8, 4 players
//		   7 board 10x10, 3 players          8 question       9 correct result
//		  10 wrong result  11 winner        12 selection and targets (10x10)
//		  13 quit dialog   14 battle, second question, long text
//

//------------------------------------------------------------------------------
//---------------------------------- Includes ----------------------------------
//------------------------------------------------------------------------------

#include "app.h"
#include "png.h"
#include "snd.h"
#include "strs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

//------------------------------------------------------------------------------
//------------------------------ Local variables -------------------------------
//------------------------------------------------------------------------------

static app main_app;

//------------------------------------------------------------------------------
//------------------------------- Local functions ------------------------------
//------------------------------------------------------------------------------

/**
  * @brief  Print the command line help.
  * @retval None.
  */
static void main_help( void )
{
	printf( "Sraz %s - a trivia board game (SDL2)\n\n"
	        "usage: sraz [options]\n"
	        "  --host            open a LAN lobby at once\n"
	        "  --join ADDR       join the LAN game of the host at ADDR\n"
	        "  --port N          TCP port for --host / --join (default %d)\n"
	        "  --name S          your player name\n"
	        "  --lang en|cro     interface and question language\n"
	        "  --theme light|dark  user interface theme\n"
	        "  --data DIR        folder with 0011.100 and 0011.200\n"
	        "  --assets DIR      accepted for compatibility; the art is built in\n"
	        "  --shot FILE.png   render one scene to a PNG and exit\n"
	        "  --scene N         scene for --shot (0..14, see main.c)\n"
	        "  --size WxH        window size for --shot (default 960x540)\n"
	        "  --selftest MS     test aid: play random moves, quit after MS ms\n"
	        "  --help            this text\n"
	        "  --version         print the version\n",
	        SRAZ_VERSION, LAN_DEFAULT_PORT );
}

/**
  * @brief  Re-attach to the parent console so --help output is visible.
  * @retval None.
  */
static void main_console( void )
{
#ifdef _WIN32
	if ( GetStdHandle( STD_OUTPUT_HANDLE ) == NULL &&
	     AttachConsole( ATTACH_PARENT_PROCESS ) )
	{
		freopen( "CONOUT$", "w", stdout );
		freopen( "CONOUT$", "w", stderr );
	}
#endif
}

/**
  * @brief  Translate an SDL event into the frame's input flags.
  * @param  a Application state.
  * @param  e Event.
  * @retval None.
  */
static void main_event( app *a, const SDL_Event *e )
{
	SDL_Keycode k;
	int         px, py, ww, wh;
	float       fx = 1.0f, fy = 1.0f;

	switch ( e->type )
	{
		case SDL_QUIT:
			a->running = 0;
			break;
		case SDL_WINDOWEVENT:
			if ( e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
			     e->window.event == SDL_WINDOWEVENT_RESIZED ||
			     e->window.event == SDL_WINDOWEVENT_EXPOSED )
			{
				gfx_request_flush( &a->g, SDL_GetTicks() );
			}
			break;
		case SDL_TEXTINPUT:
			strncat( a->in.text, e->text.text,
			         sizeof a->in.text - strlen( a->in.text ) - 1 );
			break;
		case SDL_MOUSEMOTION:
		case SDL_MOUSEBUTTONDOWN:
			px = e->type == SDL_MOUSEMOTION ? e->motion.x : e->button.x;
			py = e->type == SDL_MOUSEMOTION ? e->motion.y : e->button.y;
			SDL_GetWindowSize( a->win, &ww, &wh );
			if ( ww > 0 && wh > 0 )
			{
				fx = (float)a->g.pw / (float)ww;
				fy = (float)a->g.ph / (float)wh;
			}
			gfx_to_canvas( &a->g, (int)( (float)px * fx ), (int)( (float)py * fy ),
			               &a->in.mx, &a->in.my );
			if ( e->type == SDL_MOUSEBUTTONDOWN )
			{
				a->in.click |= e->button.button == SDL_BUTTON_LEFT;
				a->in.rclick |= e->button.button == SDL_BUTTON_RIGHT;
				a->in.key_any = 1;
			}
			break;
		case SDL_KEYDOWN:
			k = e->key.keysym.sym;
			if ( ( k == SDLK_RETURN || k == SDLK_KP_ENTER ) &&
			     ( e->key.keysym.mod & KMOD_ALT ) )
			{
				if ( !e->key.repeat )
				{
					app_toggle_fullscreen( a );
				}
				break;
			}
			if ( k == SDLK_F11 )
			{
				if ( !e->key.repeat )
				{
					app_toggle_fullscreen( a );
				}
				break;
			}
			if ( k == SDLK_F4 )
			{
				if ( !e->key.repeat )
				{
					a->sound = !a->sound;
					app_save_cfg( a );
				}
				break;
			}
			if ( k == SDLK_LSHIFT || k == SDLK_RSHIFT || k == SDLK_LCTRL ||
			     k == SDLK_RCTRL || k == SDLK_LALT || k == SDLK_RALT )
			{
				break;
			}
			a->in.key_any = 1;
			if ( k == SDLK_UP ) { a->in.up = 1; }
			else if ( k == SDLK_DOWN ) { a->in.down = 1; }
			else if ( k == SDLK_LEFT ) { a->in.left = 1; }
			else if ( k == SDLK_RIGHT ) { a->in.right = 1; }
			else if ( k == SDLK_BACKSPACE ) { a->in.back = 1; }
			else if ( k == SDLK_TAB )
			{
				a->in.tab |= !( e->key.keysym.mod & KMOD_SHIFT );
				a->in.stab |= ( e->key.keysym.mod & KMOD_SHIFT ) != 0;
			}
			else if ( e->key.repeat )
			{
				break;
			}
			else if ( k == SDLK_RETURN || k == SDLK_KP_ENTER ) { a->in.enter = 1; }
			else if ( k == SDLK_ESCAPE ) { a->in.esc = 1; }
			else if ( k == SDLK_SPACE ) { a->in.space = 1; }
			else if ( k >= SDLK_1 && k <= SDLK_4 ) { a->in.digit = (int)( k - SDLK_1 ) + 1; }
			else if ( k >= SDLK_KP_1 && k <= SDLK_KP_4 ) { a->in.digit = (int)( k - SDLK_KP_1 ) + 1; }
			else if ( k == SDLK_F5 ) { a->in.fkey = 5; }
			else if ( k == SDLK_F8 ) { a->in.fkey = 8; }
			else if ( k == SDLK_F9 ) { a->in.fkey = 9; }
			else if ( k == SDLK_d ) { a->in.dkey = 'D'; }
			else if ( k == SDLK_y ) { a->in.dkey = 'Y'; }
			else if ( k == SDLK_n ) { a->in.dkey = 'N'; }
			break;
		default:
			break;
	}
}

/**
  * @brief  Clear the per-frame input, keeping the mouse position.
  * @param  a Application state.
  * @retval None.
  */
static void main_clear_input( app *a )
{
	float mx = a->in.mx, my = a->in.my;

	memset( &a->in, 0, sizeof a->in );
	a->in.mx = mx;
	a->in.my = my;
}

/**
  * @brief  Play random legal actions to get a believable mid-game board.
  * @param  a Application state with a started game.
  * @param  actions Number of actions to play.
  * @retval None.
  */
static void main_play( app *a, int actions )
{
	sg_game *g = &a->game;
	uint32_t now = 1000;
	int      i, k, d, r, c, kind;
	static const int dr[4] = { -1, 0, 1, 0 };
	static const int dc[4] = { 0, 1, 0, -1 };

	for ( i = 0; i < actions && g->scene != SG_SCENE_OVER; i++ )
	{
		int p = g->board.turn, done = 0;

		for ( k = 0; k < BOARD_NPEGS && !done; k++ )
		{
			int kk = ( k + i ) % BOARD_NPEGS;

			if ( !board_peg_can_act( &g->board, p, kk ) )
			{
				continue;
			}
			for ( d = 0; d < 4 && !done; d++ )
			{
				r = g->board.p[p].peg[kk].r + dr[( d + i ) % 4];
				c = g->board.p[p].peg[kk].c + dc[( d + i ) % 4];
				kind = board_target_kind( &g->board, p, kk, r, c );
				if ( kind != BOARD_TARGET_NONE &&
				     sg_move( g, p, kk, r, c, now ) == 0 )
				{
					done = 1;
				}
			}
		}
		if ( !done )
		{
			sg_pass( g, p, now );
			continue;
		}
		while ( g->scene == SG_SCENE_QUESTION )
		{
			sg_answer( g, g->board.turn,
			           ( i * 7 + g->qnum ) % 5 ? g->correct_slot
			                                    : ( g->correct_slot + 1 ) % 4,
			           now );
			now += SG_RESULT_MS;
			sg_tick( g, now );
		}
	}
	while ( sg_event( g ) != SG_EV_NONE )
	{
	}
}

/**
  * @brief  Self-test player used by --selftest: starts the hosted game and
  *         plays random legal moves and random answers for the local seat.
  * @param  a Application state.
  * @param  last Time of the last action, updated here.
  * @retval None.
  */
static void main_selftest( app *a, uint32_t *last )
{
	sg_game *g = &a->game;
	int      p = g->board.turn, k, d, r, c, n = 0, ok;
	static const int dr[4] = { -1, 0, 1, 0 };
	static const int dc[4] = { 0, 1, 0, -1 };

	if ( a->lan.role == LAN_HOST && a->screen == APP_HOST &&
	     lan_player_count( &a->lan ) >= 2 && a->now - a->t0 > 1500 )
	{
		app_host_start( a );
		return;
	}
	if ( a->screen != APP_GAME || a->now - *last < 250 || !app_my_turn( a ) )
	{
		return;
	}
	*last = a->now;
	if ( g->scene == SG_SCENE_QUESTION && !g->answered )
	{
		app_do_answer( a, (int)( a->now / 250 ) % 4 );
	}
	else if ( g->scene == SG_SCENE_BOARD )
	{
		ok = 0;
		for ( k = 0; k < BOARD_NPEGS && !ok; k++ )
		{
			for ( d = 0; d < 4 && !ok; d++ )
			{
				r = g->board.p[p].peg[k].r + dr[( d + (int)a->now / 250 ) % 4];
				c = g->board.p[p].peg[k].c + dc[( d + (int)a->now / 250 ) % 4];
				if ( g->board.p[p].peg[k].alive &&
				     board_peg_can_act( &g->board, p, k ) &&
				     board_target_kind( &g->board, p, k, r, c ) !=
				         BOARD_TARGET_NONE )
				{
					app_do_move( a, k, r, c );
					ok = 1;
				}
			}
		}
		if ( !ok )
		{
			app_do_pass( a );
		}
		n++;
	}
	( void )n;
}

/**
  * @brief  Set up the state of a screenshot scene.
  * @param  a Application state.
  * @param  scene Scene number.
  * @retval None.
  */
static void main_scene( app *a, int scene )
{
	static const char names[4][LAN_NAME_SIZE] = { "Marko", "Ana", "Ivan", "Luka" };
	int  board = scene == 7 || scene == 12 || scene == 14 ? 10 : 8;
	int  players = scene == 7 ? 3 : 4, i, k, d, tr_ = 0, tc = 0;
	sg_game *g = &a->game;
	static const int dr[4] = { -1, 0, 1, 0 };
	static const int dc[4] = { 0, 1, 0, -1 };

	a->now = 2000;
	a->t0 = 500;
	a->msg[0] = '\0';
	if ( scene <= 5 )
	{
		static const app_screen scr[6] = { APP_SPLASH, APP_MENU, APP_SETUP,
		                                   APP_HOST, APP_JOIN, APP_OPTIONS };

		a->screen = scr[scene];
		a->players = 3;
		snprintf( a->names[0], sizeof a->names[0], "Marko" );
		snprintf( a->names[1], sizeof a->names[1], "Ana" );
		snprintf( a->names[2], sizeof a->names[2], "Ivan" );
		if ( scene == 0 )
		{
			a->t0 = 500;
		}
		if ( scene == 3 )
		{
			a->demo_lobby = 1;
			a->naddr = lan_local_addresses( a->addrs, 4 );
			if ( a->naddr == 0 )
			{
				snprintf( a->addrs[0], sizeof a->addrs[0], "192.168.0.112" );
				a->naddr = 1;
			}
		}
		if ( scene == 4 )
		{
			snprintf( a->join_addr, sizeof a->join_addr, "192.168.0.112" );
		}
		return;
	}
	if ( scene == 11 )
	{
		a->screen = APP_WINNER;
		memset( g, 0, sizeof *g );
		memcpy( g->names, names, sizeof g->names );
		g->winner = 1;
		g->scene = SG_SCENE_OVER;
		a->t0 = 0;
		a->now = 4300;
		return;
	}
	a->have_audio = 0;
	if ( app_load_quiz( a, a->qlang ) != 0 )
	{
		return;
	}
	a->board = board;
	a->players = players;
	for ( i = 0; i < 4; i++ )
	{
		snprintf( a->names[i], sizeof a->names[i], "%s", names[i] );
	}
	app_start_local( a );
	a->now = 2000;
	main_play( a, scene == 7 ? 40 : 60 );
	sgs_reset( &a->sel, &g->board, g->board.turn );
	a->sel_turn = g->board.turn;
	a->t0 = 0;
	if ( scene == 12 )
	{
		if ( a->sel.cpeg >= 0 )
		{
			sgs_confirm( &a->sel, &g->board, g->board.turn, &i, &k, &d );
			for ( i = 0; i < 4; i++ )
			{
				sgs_arrow( &a->sel, &g->board, g->board.turn, dr[i], dc[i] );
			}
		}
	}
	if ( scene == 8 || scene == 9 || scene == 10 || scene == 14 )
	{
		int p = g->board.turn;

		for ( k = 0; k < BOARD_NPEGS && g->scene == SG_SCENE_BOARD; k++ )
		{
			if ( !board_peg_can_act( &g->board, p, k ) )
			{
				continue;
			}
			for ( d = 0; d < 4 && g->scene == SG_SCENE_BOARD; d++ )
			{
				tr_ = g->board.p[p].peg[k].r + dr[d];
				tc = g->board.p[p].peg[k].c + dc[d];
				if ( board_target_kind( &g->board, p, k, tr_, tc ) !=
				         BOARD_TARGET_NONE &&
				     ( scene != 14 ||
				       board_target_kind( &g->board, p, k, tr_, tc ) ==
				           BOARD_TARGET_BATTLE ) )
				{
					sg_move( g, p, k, tr_, tc, a->now - 6200 );
				}
			}
		}
		if ( g->scene == SG_SCENE_BOARD && scene != 14 )
		{
			return;
		}
		if ( scene == 14 && g->scene == SG_SCENE_BOARD )
		{
			// No battle on the board: move an enemy peg next to ours.
			board_state *b = &g->board;
			int          e = ( p + 1 ) % b->nplayers, ek = 0;

			for ( k = 0; k < BOARD_NPEGS; k++ )
			{
				if ( !board_peg_can_act( b, p, k ) )
				{
					continue;
				}
				for ( d = 0; d < 4; d++ )
				{
					tr_ = b->p[p].peg[k].r + dr[d];
					tc = b->p[p].peg[k].c + dc[d];
					if ( board_target_kind( b, p, k, tr_, tc ) ==
					     BOARD_TARGET_NORMAL )
					{
						while ( ek < BOARD_NPEGS && !b->p[e].peg[ek].alive )
						{
							ek++;
						}
						b->occ[b->p[e].peg[ek].r][b->p[e].peg[ek].c] = 0;
						b->p[e].peg[ek].r = tr_;
						b->p[e].peg[ek].c = tc;
						b->occ[tr_][tc] = 1 + e * BOARD_NPEGS + ek;
						sg_move( g, p, k, tr_, tc, a->now - 6200 );
						k = BOARD_NPEGS;
						break;
					}
				}
			}
		}
		if ( scene == 14 && g->scene == SG_SCENE_QUESTION )
		{
			g->kind = BOARD_TARGET_BATTLE;
			g->qnum = 2;
			snprintf( g->cur.q, sizeof g->cur.q,
			          "Which of the following statements about the long history "
			          "of the ancient trade routes connecting the Mediterranean "
			          "with the lands of the East is, according to most scholars, "
			          "completely correct?" );
			snprintf( g->cur.a[2], sizeof g->cur.a[2],
			          "A rather long answer that has to be wrapped into two lines "
			          "to fit the button of the question panel" );
		}
		g->hilite = 1;
		if ( scene == 9 )
		{
			sg_answer( g, g->board.turn, g->correct_slot, a->now );
		}
		else if ( scene == 10 )
		{
			sg_answer( g, g->board.turn, ( g->correct_slot + 2 ) % 4, a->now );
		}
	}
	if ( scene == 13 )
	{
		a->quit_dlg = 1;
	}
	g->hilite = g->hilite;
}

/**
  * @brief  Render one scene and write it as a PNG.
  * @param  a Application state.
  * @param  path Output file.
  * @param  scene Scene number.
  * @retval 0 on success, -1 on failure.
  */
static int main_shot( app *a, const char *path, int scene )
{
	uint8_t *px;
	int      w = a->g.pw, h = a->g.ph, rc;

	main_scene( a, scene );
	px = (uint8_t *)malloc( (size_t)w * (size_t)h * 4 );
	if ( !px )
	{
		return -1;
	}
	gfx_begin( &a->g, ui_color( a, 0 ), a->now );
	ui_frame( a );
	SDL_RenderReadPixels( a->g.ren, NULL, SDL_PIXELFORMAT_RGBA32, px, w * 4 );
	rc = png_write( path, w, h, px );
	free( px );
	if ( rc != 0 )
	{
		fprintf( stderr, "cannot write %s (%d)\n", path, rc );
		return -1;
	}
	printf( "wrote %s (%d x %d, scene %d)\n", path, w, h, scene );
	return 0;
}

//------------------------------------------------------------------------------
//------------------------------- Public API -----------------------------------
//------------------------------------------------------------------------------

/**
  * @brief  Program entry.
  * @param  argc Argument count.
  * @param  argv Arguments, see main_help().
  * @retval 0 on normal exit, 1 on error.
  */
int main( int argc, char **argv )
{
	app        *a = &main_app;
	const char *shot = NULL, *join = NULL, *base;
	int         scene = 0, i, host = 0, ww = 0, wh = 0, rc = 0, selftest = 0, theme_set = 0;
	uint32_t    last_act = 0;
	Uint32      wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
	SDL_Window *win;
	SDL_Renderer *ren;
	SDL_Event   e;
	char       *sdl_base;
	size_t      n;

	app_defaults( a );
	sdl_base = SDL_GetBasePath();
	base = sdl_base ? sdl_base : "./";
	snprintf( a->base, sizeof a->base, "%s", base );
	n = strlen( a->base );
	if ( n > 0 && a->base[n - 1] != '/' && a->base[n - 1] != '\\' )
	{
		strncat( a->base, "/", sizeof a->base - n - 1 );
	}
	SDL_free( sdl_base );
	app_load_cfg( a );
	for ( i = 1; i < argc; i++ )
	{
		if ( !strcmp( argv[i], "--help" ) || !strcmp( argv[i], "-h" ) )
		{
			main_console();
			main_help();
			return 0;
		}
		else if ( !strcmp( argv[i], "--version" ) )
		{
			main_console();
			printf( "sraz %s\n", SRAZ_VERSION );
			return 0;
		}
		else if ( !strcmp( argv[i], "--host" ) )
		{
			host = 1;
		}
		else if ( i + 1 < argc && !strcmp( argv[i], "--join" ) )
		{
			join = argv[++i];
		}
		else if ( i + 1 < argc && !strcmp( argv[i], "--port" ) )
		{
			snprintf( a->host_port, sizeof a->host_port, "%.5s", argv[i + 1] );
			snprintf( a->join_port, sizeof a->join_port, "%.5s", argv[++i] );
		}
		else if ( i + 1 < argc && !strcmp( argv[i], "--name" ) )
		{
			snprintf( a->my_name, sizeof a->my_name, "%.12s", argv[++i] );
			snprintf( a->names[0], sizeof a->names[0], "%.12s", a->my_name );
		}
		else if ( i + 1 < argc && !strcmp( argv[i], "--lang" ) )
		{
			a->lang = !strcmp( argv[++i], "cro" ) || !strcmp( argv[i], "hr" );
			a->qlang = a->lang;
		}
		else if ( i + 1 < argc && !strcmp( argv[i], "--theme" ) )
		{
			a->dark = strcmp( argv[++i], "light" ) != 0;
			theme_set = 1;
		}
		else if ( i + 1 < argc && !strcmp( argv[i], "--data" ) )
		{
			a->data_override = argv[++i];
		}
		else if ( i + 1 < argc && !strcmp( argv[i], "--assets" ) )
		{
			i++;
		}
		else if ( i + 1 < argc && !strcmp( argv[i], "--shot" ) )
		{
			shot = argv[++i];
		}
		else if ( i + 1 < argc && !strcmp( argv[i], "--scene" ) )
		{
			scene = atoi( argv[++i] );
		}
		else if ( i + 1 < argc && !strcmp( argv[i], "--selftest" ) )
		{
			selftest = atoi( argv[++i] );
		}
		else if ( i + 1 < argc && !strcmp( argv[i], "--size" ) )
		{
			sscanf( argv[++i], "%dx%d", &ww, &wh );
		}
		else
		{
			main_console();
			fprintf( stderr, "unknown option %s (try --help)\n", argv[i] );
			return 1;
		}
	}
	if ( shot )
	{
		main_console();
		SDL_setenv( "SDL_VIDEODRIVER", "dummy", 0 );
		SDL_SetHint( SDL_HINT_RENDER_DRIVER, "software" );
		wflags = SDL_WINDOW_HIDDEN;
		a->dark = theme_set ? a->dark : 1;
		ww = ww < 160 ? 960 : ww;
		wh = wh < 90 ? 540 : wh;
	}
	else if ( ww < 160 || wh < 90 )
	{
		ww = 1280;
		wh = 720;
	}
	else if ( a->fullscreen )
	{
		wflags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}
	SDL_SetMainReady();
	if ( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_TIMER ) != 0 )
	{
		fprintf( stderr, "SDL_Init failed: %s\n", SDL_GetError() );
		return 1;
	}
	win = SDL_CreateWindow( "Sraz", SDL_WINDOWPOS_CENTERED,
	                        SDL_WINDOWPOS_CENTERED, ww, wh, wflags );
	if ( !win )
	{
		fprintf( stderr, "cannot create the window: %s\n", SDL_GetError() );
		return 1;
	}
	SDL_SetWindowMinimumSize( win, 640, 360 );
	ren = shot ? SDL_CreateRenderer( win, -1, SDL_RENDERER_SOFTWARE )
	           : SDL_CreateRenderer( win, -1, SDL_RENDERER_ACCELERATED |
	                                              SDL_RENDERER_PRESENTVSYNC );
	if ( !ren )
	{
		ren = SDL_CreateRenderer( win, -1, SDL_RENDERER_SOFTWARE );
	}
	if ( !ren || gfx_init( &a->g, win, ren ) != 0 )
	{
		fprintf( stderr, "cannot start the renderer: %s\n", SDL_GetError() );
		return 1;
	}
	a->win = win;
	a->running = 1;
	if ( shot )
	{
		rc = main_shot( a, shot, scene );
		gfx_free( &a->g );
		SDL_DestroyRenderer( ren );
		SDL_DestroyWindow( win );
		SDL_Quit();
		return rc == 0 ? 0 : 1;
	}
	a->have_audio = snd_init() == 0;
	snd_enable( a->sound );
	a->now = SDL_GetTicks();
	app_goto( a, APP_SPLASH );
	if ( host )
	{
		app_goto( a, APP_HOST );
		app_host_open( a );
	}
	else if ( join )
	{
		snprintf( a->join_addr, sizeof a->join_addr, "%.63s", join );
		app_goto( a, APP_JOIN );
		app_join_start( a );
	}
	SDL_StartTextInput();
	while ( a->running )
	{
		a->now = SDL_GetTicks();
		while ( SDL_PollEvent( &e ) )
		{
			main_event( a, &e );
		}
		if ( a->in.fkey == 8 && a->screen == APP_MENU )
		{
			app_save_load( a, 0 );
		}
		app_update( a );
		if ( selftest )
		{
			main_selftest( a, &last_act );
			if ( a->now > (uint32_t)selftest )
			{
				printf( "selftest: role=%d screen=%d scene=%d turn=%d epoch=%u "
				        "pegs=%d,%d,%d,%d\n", (int)a->lan.role, (int)a->screen,
				        a->game.scene, a->game.board.turn,
				        (unsigned)a->game.epoch, a->game.board.p[0].pegs_left,
				        a->game.board.p[1].pegs_left,
				        a->game.board.p[2].pegs_left,
				        a->game.board.p[3].pegs_left );
				a->running = 0;
			}
		}
		gfx_begin( &a->g, ui_color( a, 0 ), a->now );
		ui_frame( a );
		SDL_RenderPresent( ren );
		main_clear_input( a );
		SDL_Delay( 4 );
	}
	app_save_cfg( a );
	lan_close( &a->lan );
	quiz_free( &a->quiz );
	gfx_free( &a->g );
	snd_free();
	SDL_DestroyRenderer( ren );
	SDL_DestroyWindow( win );
	SDL_Quit();
	return 0;
}
