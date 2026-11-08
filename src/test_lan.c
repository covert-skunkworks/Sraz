//  Description:
//
//		test_lan.c -- real loopback sockets for a host and three guests.
//
//		The transport is included to inject split and malformed TCP frames.
//		No SDL, display, question files or external server is needed.
//

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

//------------------------------------------------------------------------------
//---------------------------------- Includes ----------------------------------
//------------------------------------------------------------------------------

#include "lan.c"
#include <assert.h>
#include <time.h>

//------------------------------------------------------------------------------
//------------------------- Local types and structures -------------------------
//------------------------------------------------------------------------------

typedef struct
{
	lan_session  host, guests[4];
	lan_snapshot states[4];
	lan_event    command;
	int          commands, left;
} test_lan_group;

//------------------------------------------------------------------------------
//------------------------------ Local functions -------------------------------
//------------------------------------------------------------------------------

/**
  * @brief  Read a platform monotonic clock.
  * @retval Milliseconds, wrapping after 32 bits.
  */
static uint32_t test_lan_now( void )
{
#ifdef _WIN32
	return GetTickCount();
#else
	struct timespec now;

	assert( clock_gettime( CLOCK_MONOTONIC, &now ) == 0 );
	return (uint32_t)( (uint64_t)now.tv_sec * 1000 +
	                   (uint64_t)now.tv_nsec / 1000000 );
#endif
}

/**
  * @brief  Yield while loopback socket data is delivered.
  * @retval None.
  */
static void test_lan_pause( void )
{
#ifdef _WIN32
	Sleep( 1 );
#else
	struct timespec delay = { 0, 1000000 };

	nanosleep( &delay, NULL );
#endif
}

/**
  * @brief  Poll the host and retain command and disconnect events.
  * @param  group Test connections.
  * @retval None.
  */
static void test_lan_poll_host( test_lan_group *group )
{
	lan_event    event;
	lan_snapshot state;
	int          i;

	for ( i = 0; i < 32; i++ )
	{
		if ( lan_poll( &group->host, &event, &state, test_lan_now() ) <= 0 )
		{
			break;
		}
		if ( event.type == LAN_EVENT_COMMAND )
		{
			group->command = event;
			group->commands++;
		}
		else if ( event.type == LAN_EVENT_LEFT )
		{
			group->left++;
		}
	}
}

/**
  * @brief  Advance all active test connections.
  * @param  group Test connections.
  * @param  steps Number of one-millisecond polling rounds.
  * @retval None.
  */
static void test_lan_pump( test_lan_group *group, int steps )
{
	lan_event    event;
	lan_snapshot state;
	int          step, i, events;

	for ( step = 0; step < steps; step++ )
	{
		test_lan_poll_host( group );
		for ( i = 0; i < 4; i++ )
		{
			if ( !group->guests[i].impl )
			{
				continue;
			}
			for ( events = 0; events < 32; events++ )
			{
				if ( lan_poll( &group->guests[i], &event, &state,
				               test_lan_now() ) <= 0 )
				{
					break;
				}
				if ( event.type == LAN_EVENT_STATE )
				{
					group->states[i] = state;
				}
			}
		}
		test_lan_pause();
	}
}

/**
  * @brief  Find an unused test port and start the host.
  * @param  group Receives a host session.
  * @retval Bound TCP port.
  */
static uint16_t test_lan_host( test_lan_group *group )
{
	int      attempt;
	uint16_t port;

	for ( attempt = 0; attempt < 100; attempt++ )
	{
		port =
		    (uint16_t)( 43000 + ( test_lan_now() % 1000 ) + (uint32_t)attempt );
		if ( lan_host( &group->host, port, "Host" ) == 0 )
		{
			return port;
		}
	}
	assert( !"No test port could be opened" );
	return 0;
}

/**
  * @brief  Create a valid snapshot with a nontrivial board and hidden answer.
  * @param  group Connected sessions.
  * @param  state Receives the snapshot.
  * @retval None.
  */
static void test_lan_state( const test_lan_group *group, lan_snapshot *state )
{
	int i;

	memset( state, 0, sizeof *state );
	state->epoch = 123;
	state->scene = 3;
	state->language = QUIZ_LANGUAGE_EN;
	state->nplayers = 4;
	state->timer_sec = 30;
	state->remaining_ms = 23000;
	state->question_number = 1;
	state->correct = 2;
	state->picked = -1;
	state->winner = -1;
	board_init( &state->board, 10, 4, 5 );
	state->board.turn = 2;
	state->category = 7;
	state->question_index = 875;
	state->sel_peg = -1;
	state->target_r = -1;
	state->target_c = -1;
	memcpy( state->names, group->host.names, sizeof state->names );
	for ( i = 0; i < 4; i++ )
	{
		state->board.occ[state->board.p[i].peg[0].r][state->board.p[i].peg[0].c] = 0;
		state->board.p[i].peg[0].r = i;
		state->board.p[i].peg[0].c = 9 - i;
		state->board.p[i].peg[0].score = 5 + i;
		state->board.occ[i][9 - i] = 1 + i * BOARD_NPEGS;
	}	snprintf( state->question.q, sizeof state->question.q,
	          "Question: Å¡ Ä‡ Ä Å¾ Ä‘?" );
	for ( i = 0; i < 4; i++ )
	{
		snprintf( state->question.a[i], sizeof state->question.a[i],
		          "Choice %d", i + 1 );
	}
}

/**
  * @brief  Verify capacity, portable snapshots, commands and seat compaction.
  * @retval None.
  */
static void test_lan_sessions( void )
{
	test_lan_group group = { 0 };
	lan_snapshot   state;
	lan_command    command = { 123, LAN_MOVE, 4, 5, 6, 2 };
	lan_impl      *impl;
	lan_stream     stream;
	uint8_t        frame[36], invalid[12] = { 0 };
	char           name[LAN_NAME_SIZE];
	uint16_t       port;
	int            i, attempt, before;

	assert( lan_join( &group.guests[0], "invalid", 1, "Guest", 0 ) == -1 );
	port = test_lan_host( &group );
	for ( i = 0; i < 3; i++ )
	{
		snprintf( name, sizeof name, "Guest %d", i + 1 );
		assert( lan_join( &group.guests[i], "127.0.0.1", port, name,
		                  test_lan_now() ) == 0 );
		// Join sequentially so the expected seat numbers are deterministic.
		for ( attempt = 0; attempt < 1000 && group.guests[i].local_player < 1;
		      attempt++ )
		{
			test_lan_pump( &group, 1 );
		}
		assert( group.guests[i].local_player == i + 1 );
	}
	assert( lan_player_count( &group.host ) == 4 );
	assert( lan_join( &group.guests[3], "127.0.0.1", port, "Extra",
	                  test_lan_now() ) == 0 );
	test_lan_pump( &group, 50 );
	assert( group.guests[3].local_player == -1 );
	assert( group.guests[3].error[0] != '\0' );
	assert( lan_player_count( &group.host ) == 4 );
	lan_close( &group.guests[3] );
	assert( lan_begin( &group.host ) == 0 );
	test_lan_pump( &group, 30 );
	assert( strcmp( group.host.names[1], "Guest 1" ) == 0 );
	assert( strcmp( group.host.names[3], "Guest 3" ) == 0 );
	test_lan_state( &group, &state );
	assert( lan_publish( &group.host, &state ) == 0 );
	test_lan_pump( &group, 30 );
	for ( i = 0; i < 3; i++ )
	{
		assert( group.states[i].epoch == 123 );
		assert( group.states[i].board.n == 10 );
		assert( group.states[i].board.turn == 2 );
		assert( group.states[i].board.p[3].peg[0].c == 6 );
		assert( group.states[i].correct == -1 );
		assert( strcmp( group.states[i].question.q, state.question.q ) == 0 );
		assert( strcmp( group.states[i].question.a[2], "Choice 3" ) == 0 );
	}
	state.answered = 1;
	state.picked = 1;
	assert( lan_publish( &group.host, &state ) == 0 );
	test_lan_pump( &group, 30 );
	assert( group.states[2].correct == 2 && group.states[2].picked == 1 );
	assert( lan_send_command( &group.guests[1], &command ) == 0 );
	test_lan_pump( &group, 30 );
	assert( group.commands == 1 && group.command.player == 2 );
	assert( group.command.command.epoch == 123 );
	assert( group.command.command.peg == 4 && group.command.command.col == 6 );

	// Split a complete command across arbitrary TCP boundaries.
	stream = (lan_stream){ frame, sizeof frame, 0, 0 };
	lan_write_word( &stream, LAN_MAGIC );
	lan_write_word( &stream, LAN_MSG_COMMAND );
	lan_write_word( &stream, 24 );
	lan_write_word( &stream, 456 );
	lan_write_word( &stream, LAN_ANSWER );
	lan_write_word( &stream, 0 );
	lan_write_word( &stream, 0 );
	lan_write_word( &stream, 0 );
	lan_write_word( &stream, 3 );
	assert( memcmp( frame, "SRZ1", 4 ) == 0 );
	impl = group.guests[0].impl;
	assert( send( impl->peers[0].socket, (const char *)frame, 5, 0 ) == 5 );
	before = group.commands;
	test_lan_poll_host( &group );
	assert( group.commands == before );
	assert( send( impl->peers[0].socket, (const char *)frame + 5, 9, 0 ) == 9 );
	test_lan_poll_host( &group );
	assert( group.commands == before );
	assert( send( impl->peers[0].socket, (const char *)frame + 14, 22, 0 ) ==
	        22 );
	test_lan_pump( &group, 30 );
	assert( group.commands == before + 1 );
	assert( group.command.command.epoch == 456 );
	assert( group.command.command.answer == 3 );

	// Invalid framing drops only that guest. A later start compacts seat gaps.
	impl = group.guests[1].impl;
	assert( send( impl->peers[0].socket, (const char *)invalid, sizeof invalid,
	              0 ) == sizeof invalid );
	test_lan_pump( &group, 30 );
	assert( group.left == 1 && lan_player_count( &group.host ) == 3 );
	lan_lobby( &group.host );
	assert( lan_begin( &group.host ) == 0 );
	test_lan_pump( &group, 30 );
	assert( group.guests[2].local_player == 2 );
	assert( strcmp( group.host.names[2], "Guest 3" ) == 0 );
	assert( lan_join( &group.guests[3], "127.0.0.1", port, "Late",
	                  test_lan_now() ) == 0 );
	test_lan_pump( &group, 30 );
	assert( group.guests[3].local_player == -1 );
	assert( group.guests[3].error[0] != '\0' );
	lan_close( &group.guests[0] );
	test_lan_pump( &group, 30 );
	assert( group.left == 2 && lan_player_count( &group.host ) == 2 );
	for ( i = 0; i < 4; i++ )
	{
		lan_close( &group.guests[i] );
	}
	lan_close( &group.host );
}

//------------------------------------------------------------------------------
//--------------------------------- Public API ---------------------------------
//------------------------------------------------------------------------------

/**
  * @brief  Run real-socket LAN regression tests.
  * @retval 0 if all assertions pass.
  */
int main( void )
{
	test_lan_sessions();
	puts( "PASS: four players, capacity, snapshots, hidden answers, partial "
	      "frames,"
	      " commands, malformed input, seat compaction and disconnects." );
	return 0;
}
