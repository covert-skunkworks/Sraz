//  Description:
//
//		app.h -- application state shared by the screens, the LAN glue and main.
//
//		The program is a small set of screens (splash, menu, setup, LAN host,
//		LAN join, options, game, winner) driven by one immediate-mode loop:
//		input events are collected into app_input, each frame the current
//		screen draws itself and its widgets consume that input, then the
//		input flags are cleared. Game rules and the question flow live in
//		game.c; this layer only decides what to show and whom to ask.
//
#ifndef APP_H
#define APP_H

#include "game.h"
#include "gfx.h"
#include "lan.h"
#include "quiz.h"
#include <SDL.h>

#ifndef SRAZ_VERSION
#define SRAZ_VERSION "0.0"
#endif

#define APP_NAME_MAX 12  // Longest local player name (R-11).
#define APP_BASE_MAX 1024
#define APP_PATH_MAX 1100

typedef enum
{
	APP_SPLASH,
	APP_MENU,
	APP_SETUP,
	APP_HOST,
	APP_JOIN,
	APP_OPTIONS,
	APP_GAME,
	APP_WINNER
} app_screen;

// Input collected from SDL events for the frame being drawn.
typedef struct
{
	float mx, my;           // Mouse in canvas units.
	int   click;            // Left button went down this frame.
	int   rclick;           // Right button went down this frame.
	int   enter, esc, space, tab, stab, up, down, left, right, back;
	int   key_any;          // Any key or click, for the splash and winner.
	int   digit;            // 1..4 pressed, else 0.
	int   fkey;             // 4, 5, 8, 9, 11 for F-keys, else 0.
	int   dkey;             // D, Y, J or N letter pressed (quit dialog).
	char  text[64];         // Typed characters.
} app_input;

typedef struct
{
	gfx          g;
	SDL_Window  *win;
	int          running;
	app_screen   screen;
	uint32_t     now, t0;   // Frame time and time the screen was entered.
	app_input    in;

	// Options, kept in sraz.cfg next to the executable.
	int  sound, lang, fullscreen, dark, army;
	char join_addr[LAN_ADDRESS_SIZE], join_port[8], host_port[8];
	char my_name[LAN_NAME_SIZE];

	// New game setup shared by the local and the LAN host screens.
	int  players, board, timer_sec, qlang;
	char names[BOARD_NPLAYERS][LAN_NAME_SIZE];

	// Widgets: the focused one and the running index during a frame.
	int focus, wid, nwid;

	// Game.
	sg_game   game;
	sg_sel    sel;
	quiz_bank quiz;
	int       quiz_lang;    // Language loaded in quiz, -1 none.
	int       quit_dlg;
	int       winner_ack;   // LAN: winner screen dismissed for this game.
	int       answer_sent;  // Guest: epoch of the answer already sent.
	uint32_t  answer_sent_epoch;
	int       tile_style;   // F9.
	int       sel_turn;     // Turn the selection belongs to, -1 none.

	// LAN.
	lan_session lan;
	int         lan_joined;
	uint32_t    pub_at, pub_epoch;
	char        addrs[4][LAN_ADDRESS_SIZE];
	int         naddr;
	char        lan_note[160];
	int         demo_lobby;  // Screenshots: show a made-up lobby.

	// Messages.
	char     msg[160];
	uint32_t msg_until;
	int      msg_err;

	// Paths and command line.
	char        base[APP_BASE_MAX];
	const char *data_override;
	int         have_audio;
} app;

/**
  * @brief  Set the defaults of every option and the setup screen.
  * @param  a Application state.
  * @retval None.
  */
void app_defaults( app *a );

/**
  * @brief  Default player name in the interface language ("Player 1",
  *         "Igrac 1").
  * @param  a Application state.
  * @param  seat Seat 0..3, or -1 for the name without a number.
  * @param  out Output buffer.
  * @param  cap Buffer size.
  * @retval None.
  */
void app_default_name( const app *a, int seat, char *out, size_t cap );

/**
  * @brief  Name of a local player: the one typed in, else the default.
  * @param  a Application state.
  * @param  seat Seat 0..3.
  * @param  out Output buffer.
  * @param  cap Buffer size.
  * @retval None.
  */
void app_player_name( const app *a, int seat, char *out, size_t cap );

/**
  * @brief  Read sraz.cfg from the executable's folder, if it exists.
  * @param  a Application state.
  * @retval None.
  */
void app_load_cfg( app *a );

/**
  * @brief  Write sraz.cfg next to the executable.
  * @param  a Application state.
  * @retval None.
  */
void app_save_cfg( const app *a );

/**
  * @brief  Switch screen and restart its clock and widget focus.
  * @param  a Application state.
  * @param  s New screen.
  * @retval None.
  */
void app_goto( app *a, app_screen s );

/**
  * @brief  Show a message for a few seconds.
  * @param  a Application state.
  * @param  text Message.
  * @param  err Nonzero for an error colour.
  * @retval None.
  */
void app_msg( app *a, const char *text, int err );

/**
  * @brief  Load the question bank of a language if not already loaded.
  * @param  a Application state.
  * @param  lang 0 English, 1 Croatian.
  * @retval 0 on success, quiz_load() error code otherwise.
  */
int app_load_quiz( app *a, int lang );

/**
  * @brief  Start a hot-seat game with the setup screen's settings.
  * @param  a Application state.
  * @retval 0 on success, -1 if the questions could not be loaded.
  */
int app_start_local( app *a );

/**
  * @brief  Open a LAN lobby as host.
  * @param  a Application state.
  * @retval 0 on success, -1 on error (message set).
  */
int app_host_open( app *a );

/**
  * @brief  Start the game from the host's lobby.
  * @param  a Application state.
  * @retval 0 on success, -1 on error (message set).
  */
int app_host_start( app *a );

/**
  * @brief  Begin joining a host.
  * @param  a Application state.
  * @retval 0 on success, -1 on invalid input or error (message set).
  */
int app_join_start( app *a );

/**
  * @brief  Close any LAN session and go back to a screen.
  * @param  a Application state.
  * @param  to Screen to show.
  * @retval None.
  */
void app_lan_leave( app *a, app_screen to );

/**
  * @brief  Host: abandon the running game and reopen the lobby.
  * @param  a Application state.
  * @retval None.
  */
void app_host_lobby( app *a );

/**
  * @brief  Per-frame logic: LAN traffic, timers, sounds, screen changes.
  * @param  a Application state.
  * @retval None.
  */
void app_update( app *a );

/**
  * @brief  Send or apply a move chosen by the local player.
  * @param  a Application state.
  * @param  peg Peg index.
  * @param  r Target row.
  * @param  c Target column.
  * @retval None.
  */
void app_do_move( app *a, int peg, int r, int c );

/**
  * @brief  Send or apply an answer chosen by the local player.
  * @param  a Application state.
  * @param  slot Display slot 0..3.
  * @retval None.
  */
void app_do_answer( app *a, int slot );

/**
  * @brief  Send or apply a pass chosen by the local player.
  * @param  a Application state.
  * @retval None.
  */
void app_do_pass( app *a );

/**
  * @brief  Whether the local user may act in the current turn.
  * @param  a Application state.
  * @retval 1 if the player on turn is played on this computer.
  */
int app_my_turn( const app *a );

/**
  * @brief  Save or load the hot-seat game (F5 / F8).
  * @param  a Application state.
  * @param  save Nonzero to save, zero to load.
  * @retval None.
  */
void app_save_load( app *a, int save );

/**
  * @brief  Toggle fullscreen (F11 / Alt+Enter).
  * @param  a Application state.
  * @retval None.
  */
void app_toggle_fullscreen( app *a );

/**
  * @brief  Colour of a player's army for the current colour set.
  * @param  a Application state.
  * @param  seat Player seat.
  * @retval art_player_t value.
  */
int app_player_art( const app *a, int seat );

/**
  * @brief  Draw the current screen and handle its widgets.
  * @param  a Application state.
  * @retval None.
  */
void ui_frame( app *a );

/**
  * @brief  Draw the game screen and handle its input.
  * @param  a Application state.
  * @retval None.
  */
void ui_game( app *a );

/**
  * @brief  Draw the winner screen.
  * @param  a Application state.
  * @retval None.
  */
void ui_winner( app *a );

/**
  * @brief  Draw a button in canvas units and report activation.
  * @param  a Application state.
  * @param  x Left.
  * @param  y Top.
  * @param  w Width.
  * @param  h Height.
  * @param  label Caption.
  * @param  kind 0 plain, 1 primary.
  * @param  enabled Zero draws it dimmed and inert.
  * @retval 1 when clicked or activated with Enter.
  */
int ui_btn( app *a, float x, float y, float w, float h, const char *label,
            int kind, int enabled );

/**
  * @brief  Test whether the mouse is inside a rectangle.
  * @param  a Application state.
  * @param  x Left.
  * @param  y Top.
  * @param  w Width.
  * @param  h Height.
  * @retval 1 if inside.
  */
int ui_hit( const app *a, float x, float y, float w, float h );

/**
  * @brief  Whether the user interface colours are the dark ones.
  * @param  a Application state.
  * @param  idx art_ui_t value.
  * @retval 0xRRGGBB.
  */
uint32_t ui_color( const app *a, int idx );

#endif
