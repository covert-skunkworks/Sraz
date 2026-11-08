//  Description:
//
//		lan.c -- nonblocking TCP transport and portable Sraz LAN messages.
//
//		Every frame has three big-endian words: magic "SRZ1", message type,
//		payload length. Payloads contain explicit words and bounded UTF-8
//		buffers, never native structs. Receive and transmit queues retain
//		partial frames. Correct answers are revealed only in result snapshots.
//
//		Hello/connection deadlines are five seconds; heartbeats detect a
//		silent broken link after fifteen seconds. Polling never blocks the UI.
//

//------------------------------------------------------------------------------
//---------------------------------- Includes ----------------------------------
//------------------------------------------------------------------------------

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include "lan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//------------------------------------------------------------------------------
//------------------------- Local macros and constants -------------------------
//------------------------------------------------------------------------------

#define LAN_MAGIC UINT32_C( 0x53525a31 )
#define LAN_FRAME_MAX 8192
#define LAN_QUEUE_SIZE 32768
#define LAN_TIMEOUT 15000
#define LAN_HANDSHAKE_TIMEOUT 5000
#ifdef _WIN32
#define LAN_INVALID INVALID_SOCKET
#else
#define LAN_INVALID ( -1 )
#endif

//------------------------------------------------------------------------------
//------------------------- Local types and structures -------------------------
//------------------------------------------------------------------------------

#ifdef _WIN32
typedef SOCKET lan_socket_t;
#else
typedef int lan_socket_t;
#endif

typedef enum
{
	LAN_MSG_HELLO = 1,
	LAN_MSG_WELCOME,
	LAN_MSG_ROSTER,
	LAN_MSG_STATE,
	LAN_MSG_COMMAND,
	LAN_MSG_PING
} lan_message;

typedef struct
{
	lan_socket_t socket;
	char         name[LAN_NAME_SIZE];
	int          ready, connecting, player;
	uint32_t     received_at, ping_at, opened_at;
	size_t       rx_size, tx_size, tx_sent;
	uint8_t      rx[LAN_QUEUE_SIZE], tx[LAN_QUEUE_SIZE];
} lan_peer;

typedef struct
{
	lan_socket_t listener;
	lan_peer     peers[BOARD_NPLAYERS];
	int          started;
	int          sockets_started;
	char         name[LAN_NAME_SIZE];
} lan_impl;

typedef struct
{
	uint8_t *data;
	size_t   size, pos;
	int      failed;
} lan_stream;

//------------------------------------------------------------------------------
//------------------------------ Local functions -------------------------------
//------------------------------------------------------------------------------

/**
  * @brief  Write one big-endian word to a bounded stream.
  * @param  stream Destination cursor.
  * @param  value Word to encode.
  * @retval None.
  */
static void lan_write_word( lan_stream *stream, uint32_t value )
{
	int i;

	if ( stream->size - stream->pos < 4 )
	{
		stream->failed = 1;
		return;
	}
	for ( i = 3; i >= 0; i-- )
	{
		stream->data[stream->pos++] = (uint8_t)( value >> ( i * 8 ) );
	}
}

/**
  * @brief  Read one big-endian word from a bounded stream.
  * @param  stream Source cursor.
  * @retval Decoded word, or zero with the failure flag set on truncation.
  */
static uint32_t lan_read_word( lan_stream *stream )
{
	uint32_t value = 0;
	int      i;

	if ( stream->size - stream->pos < 4 )
	{
		stream->failed = 1;
		return 0;
	}
	for ( i = 0; i < 4; i++ )
	{
		value = ( value << 8 ) | stream->data[stream->pos++];
	}
	return value;
}

/**
  * @brief  Decode a signed wire integer without implementation-defined casts.
  * @param  stream Source cursor.
  * @retval Decoded signed integer.
  */
static int lan_read_int( lan_stream *stream )
{
	uint32_t value = lan_read_word( stream );

	return value <= INT32_MAX ? (int)value : -1 - (int)( UINT32_MAX - value );
}

/**
  * @brief  Copy fixed-width bytes into a wire stream.
  * @param  stream Destination cursor.
  * @param  data Readable source.
  * @param  size Number of bytes.
  * @retval None.
  */
static void lan_write_bytes( lan_stream *stream, const void *data, size_t size )
{
	if ( size > stream->size - stream->pos )
	{
		stream->failed = 1;
		return;
	}
	memcpy( stream->data + stream->pos, data, size );
	stream->pos += size;
}

/**
  * @brief  Read a fixed-width terminated string from the wire.
  * @param  stream Source cursor.
  * @param  out Receives the string.
  * @param  size Destination capacity, including the terminator.
  * @retval None.
  */
static void lan_read_text( lan_stream *stream, char *out, size_t size )
{
	if ( size > stream->size - stream->pos ||
	     !memchr( stream->data + stream->pos, 0, size ) )
	{
		stream->failed = 1;
		return;
	}
	memcpy( out, stream->data + stream->pos, size );
	out[size - 1] = '\0';
	stream->pos += size;
}

/**
  * @brief  Close a native socket.
  * @param  socket Native handle, possibly invalid.
  * @retval None.
  */
static void lan_close_socket( lan_socket_t socket )
{
	if ( socket != LAN_INVALID )
	{
#ifdef _WIN32
		closesocket( socket );
#else
		close( socket );
#endif
	}
}

/**
  * @brief  Recognise a temporary nonblocking socket condition.
  * @retval 1 if the operation should be retried, 0 for a permanent failure.
  */
static int lan_would_block( void )
{
#ifdef _WIN32
	int error = WSAGetLastError();

	return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS ||
	       error == WSAEINTR;
#else
	return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS ||
	       errno == EINTR;
#endif
}

/**
  * @brief  Enable nonblocking I/O and responsive small TCP messages.
  * @param  socket Native socket.
  * @retval 0 on success, -3 socket configuration failure.
  */
static int lan_configure_socket( lan_socket_t socket )
{
	int enabled = 1;
#ifdef _WIN32
	u_long mode = 1;

	if ( ioctlsocket( socket, FIONBIO, &mode ) != 0 )
#else
	int flags = fcntl( socket, F_GETFL, 0 );

	if ( flags < 0 || fcntl( socket, F_SETFL, flags | O_NONBLOCK ) != 0 )
#endif
	{
		return -3;
	}
	setsockopt( socket, IPPROTO_TCP, TCP_NODELAY, (const char *)&enabled,
	            sizeof enabled );
	setsockopt( socket, SOL_SOCKET, SO_KEEPALIVE, (const char *)&enabled,
	            sizeof enabled );
	return 0;
}

/**
  * @brief  Allocate a session and initialise the platform socket runtime.
  * @param  session Receives the transport implementation.
  * @param  name Local player name.
  * @retval 0 on success, -1 invalid name/session, -2 memory, -3 socket runtime.
  */
static int lan_open( lan_session *session, const char *name )
{
	lan_impl *impl;
	int       i;
#ifdef _WIN32
	WSADATA data;
#endif

	if ( !session || session->impl || !name || !name[0] ||
	     strlen( name ) >= LAN_NAME_SIZE )
	{
		return -1;
	}
	memset( session, 0, sizeof *session );
	session->local_player = -1;
	impl = calloc( 1, sizeof *impl );
	if ( !impl )
	{
		return -2;
	}
	impl->listener = LAN_INVALID;
	for ( i = 0; i < BOARD_NPLAYERS; i++ )
	{
		impl->peers[i].socket = LAN_INVALID;
	}
	session->impl = impl;
#ifdef _WIN32
	if ( WSAStartup( MAKEWORD( 2, 2 ), &data ) != 0 )
	{
		lan_close( session );
		return -3;
	}
#endif
	impl->sockets_started = 1;
	snprintf( impl->name, sizeof impl->name, "%s", name );
	return 0;
}

/**
  * @brief  Retain a complete frame for nonblocking transmission.
  * @param  peer Destination connection.
  * @param  type Message type.
  * @param  data Payload bytes.
  * @param  size Payload size.
  * @retval 0 queued, -6 queue full or payload too large.
  */
static int lan_queue( lan_peer *peer, lan_message type, const uint8_t *data,
                      size_t size )
{
	lan_stream stream;

	if ( peer->tx_sent )
	{
		memmove( peer->tx, peer->tx + peer->tx_sent,
		         peer->tx_size - peer->tx_sent );
		peer->tx_size -= peer->tx_sent;
		peer->tx_sent = 0;
	}
	if ( size > LAN_FRAME_MAX || size + 12 > sizeof peer->tx - peer->tx_size )
	{
		return -6;
	}
	stream = (lan_stream){ peer->tx + peer->tx_size,
	                       sizeof peer->tx - peer->tx_size, 0, 0 };
	lan_write_word( &stream, LAN_MAGIC );
	lan_write_word( &stream, (uint32_t)type );
	lan_write_word( &stream, (uint32_t)size );
	if ( size )
	{
		lan_write_bytes( &stream, data, size );
	}
	peer->tx_size += stream.pos;
	return 0;
}

/**
  * @brief  Queue the current connected names for every ready guest.
  * @param  session Host session.
  * @retval None.
  */
static void lan_roster( lan_session *session )
{
	lan_impl  *impl = session->impl;
	uint8_t    data[BOARD_NPLAYERS * ( LAN_NAME_SIZE + 4 )];
	lan_stream stream = { data, sizeof data, 0, 0 };
	int        i;

	for ( i = 0; i < BOARD_NPLAYERS; i++ )
	{
		lan_write_word( &stream, (uint32_t)session->connected[i] );
		lan_write_bytes( &stream, session->names[i], LAN_NAME_SIZE );
	}
	for ( i = 0; i < BOARD_NPLAYERS; i++ )
	{
		if ( impl->peers[i].ready )
		{
			lan_queue( &impl->peers[i], LAN_MSG_ROSTER, data, stream.pos );
		}
	}
}

/**
  * @brief  Drop a connection and record the affected player.
  * @param  session Active session.
  * @param  peer Connection to close.
  * @param  event Receives a disconnect or error event.
  * @param  reason User-visible reason.
  * @retval 1 for a visible event, 0 for an unregistered guest.
  */
static int lan_drop( lan_session *session, lan_peer *peer, lan_event *event,
                     const char *reason )
{
	int player = peer->player, ready = peer->ready;

	lan_close_socket( peer->socket );
	memset( peer, 0, sizeof *peer );
	peer->socket = LAN_INVALID;
	if ( session->role == LAN_CLIENT )
	{
		memset( session->connected, 0, sizeof session->connected );
		session->local_player = -1;
		snprintf( session->error, sizeof session->error, "%s", reason );
		event->type = LAN_EVENT_ERROR;
		event->player = -1;
		return 1;
	}
	if ( ready )
	{
		session->connected[player] = 0;
		session->names[player][0] = '\0';
		lan_roster( session );
		event->type = LAN_EVENT_LEFT;
		event->player = player;
		return 1;
	}
	return 0;
}

/**
  * @brief  Flush queued bytes without blocking.
  * @param  peer Connection to service.
  * @retval 0 if connected, -5 on a permanent send failure.
  */
static int lan_flush( lan_peer *peer )
{
	int sent, flags = 0;

#ifdef MSG_NOSIGNAL
	flags = MSG_NOSIGNAL;
#endif
	while ( peer->tx_sent < peer->tx_size )
	{
		sent = (int)send( peer->socket, (const char *)peer->tx + peer->tx_sent,
		                  (int)( peer->tx_size - peer->tx_sent ), flags );
		if ( sent < 0 )
		{
			return lan_would_block() ? 0 : -5;
		}
		if ( sent == 0 )
		{
			return -5;
		}
		peer->tx_sent += (size_t)sent;
	}
	peer->tx_size = 0;
	peer->tx_sent = 0;
	return 0;
}

/**
  * @brief  Encode a complete board with explicit-width fields.
  * @param  stream Destination stream.
  * @param  board Authoritative board.
  * @retval None.
  */
static void lan_write_board( lan_stream *stream, const board_state *board )
{
	const board_player *player;
	const board_peg    *peg;
	int                 r, c, i, k;

	lan_write_word( stream, (uint32_t)board->n );
	lan_write_word( stream, (uint32_t)board->nplayers );
	lan_write_word( stream, (uint32_t)board->turn );
	lan_write_word( stream, (uint32_t)board->winner );
	for ( r = 0; r < BOARD_MAX_SIZE; r++ )
	{
		for ( c = 0; c < BOARD_MAX_SIZE; c++ )
		{
			lan_write_word( stream, (uint32_t)board->terrain[r][c] );
		}
	}
	for ( i = 0; i < BOARD_NPLAYERS; i++ )
	{
		player = &board->p[i];
		lan_write_word( stream, (uint32_t)player->pegs_left );
		lan_write_word( stream, (uint32_t)player->active );
		lan_write_word( stream, (uint32_t)player->home_r );
		lan_write_word( stream, (uint32_t)player->home_c );
		lan_write_word( stream, (uint32_t)player->sel );
		for ( k = 0; k < BOARD_NPEGS; k++ )
		{
			peg = &player->peg[k];
			lan_write_word( stream, (uint32_t)peg->alive );
			lan_write_word( stream, (uint32_t)peg->r );
			lan_write_word( stream, (uint32_t)peg->c );
			lan_write_word( stream, (uint32_t)peg->score );
			lan_write_word( stream, (uint32_t)peg->attempts );
		}
	}
}

/**
  * @brief  Decode and validate a board before the renderer can use it.
  *
  * The occupancy map is rebuilt from the pegs, it is not part of the wire
  * format.
  *
  * @param  stream Source stream.
  * @param  board Receives the board.
  * @retval None.
  */
static void lan_read_board( lan_stream *stream, board_state *board )
{
	board_player *player;
	board_peg    *peg;
	int           r, c, i, k;

	memset( board->occ, 0, sizeof board->occ );
	board->n = lan_read_int( stream );
	board->nplayers = lan_read_int( stream );
	board->turn = lan_read_int( stream );
	board->winner = lan_read_int( stream );
	if ( ( board->n != 6 && board->n != 8 && board->n != 10 ) ||
	     board->nplayers < 2 || board->nplayers > BOARD_NPLAYERS ||
	     board->turn < 0 || board->turn >= BOARD_NPLAYERS ||
	     board->winner < -2 || board->winner >= BOARD_NPLAYERS )
	{
		stream->failed = 1;
	}
	for ( r = 0; r < BOARD_MAX_SIZE; r++ )
	{
		for ( c = 0; c < BOARD_MAX_SIZE; c++ )
		{
			board->terrain[r][c] = lan_read_int( stream );
			if ( board->terrain[r][c] < 0 ||
			     board->terrain[r][c] > BOARD_FLAG_BASE + BOARD_NPLAYERS )
			{
				stream->failed = 1;
			}
		}
	}
	for ( i = 0; i < BOARD_NPLAYERS; i++ )
	{
		player = &board->p[i];
		player->pegs_left = lan_read_int( stream );
		player->active = lan_read_int( stream );
		player->home_r = lan_read_int( stream );
		player->home_c = lan_read_int( stream );
		player->sel = lan_read_int( stream );
		if ( player->pegs_left < 0 || player->pegs_left > BOARD_NPEGS ||
		     player->home_r < 0 || player->home_r >= BOARD_MAX_SIZE ||
		     player->home_c < 0 || player->home_c >= BOARD_MAX_SIZE ||
		     player->sel < 0 || player->sel >= BOARD_NPEGS )
		{
			stream->failed = 1;
		}
		for ( k = 0; k < BOARD_NPEGS; k++ )
		{
			peg = &player->peg[k];
			peg->alive = lan_read_int( stream );
			peg->r = lan_read_int( stream );
			peg->c = lan_read_int( stream );
			peg->score = lan_read_int( stream );
			peg->attempts = lan_read_int( stream );
			if ( peg->alive )
			{
				if ( peg->r < 0 || peg->r >= board->n || peg->c < 0 ||
				     peg->c >= board->n || peg->score < 0 ||
				     peg->score > BOARD_MAX_SCORE || peg->attempts < 0 ||
				     peg->attempts > 3 )
				{
					stream->failed = 1;
				}
				else
				{
					board->occ[peg->r][peg->c] = 1 + i * BOARD_NPEGS + k;
				}
			}
		}
	}
}

/**
  * @brief  Encode a snapshot without sending any native struct padding.
  * @param  stream Destination stream.
  * @param  state Renderable host state.
  * @retval None.
  */
static void lan_write_state( lan_stream *stream, const lan_snapshot *state )
{
	int i;

	lan_write_word( stream, state->epoch );
	lan_write_word( stream, state->remaining_ms );
	lan_write_word( stream, (uint32_t)state->scene );
	lan_write_word( stream, (uint32_t)state->language );
	lan_write_word( stream, (uint32_t)state->nplayers );
	lan_write_word( stream, (uint32_t)state->timer_sec );
	lan_write_word( stream, (uint32_t)state->battle );
	lan_write_word( stream, (uint32_t)state->question_number );
	lan_write_word( stream, (uint32_t)state->answered );
	lan_write_word( stream,
	                (uint32_t)( state->answered ? state->correct : -1 ) );
	lan_write_word( stream, (uint32_t)state->picked );
	lan_write_word( stream, (uint32_t)state->was_correct );
	lan_write_word( stream, (uint32_t)state->winner );
	lan_write_word( stream, (uint32_t)state->question_index );
	lan_write_word( stream, (uint32_t)state->category );
	lan_write_word( stream, (uint32_t)state->sel_peg );
	lan_write_word( stream, (uint32_t)state->target_r );
	lan_write_word( stream, (uint32_t)state->target_c );
	lan_write_board( stream, &state->board );
	lan_write_bytes( stream, state->names, sizeof state->names );
	lan_write_bytes( stream, state->question.q, sizeof state->question.q );
	for ( i = 0; i < 4; i++ )
	{
		lan_write_bytes( stream, state->question.a[i],
		                 sizeof state->question.a[i] );
	}
}

/**
  * @brief  Decode a snapshot and reject unsafe indices or truncated text.
  * @param  stream Source stream.
  * @param  state Receives a complete snapshot.
  * @retval None.
  */
static void lan_read_state( lan_stream *stream, lan_snapshot *state )
{
	int i;

	memset( state, 0, sizeof *state );
	state->epoch = lan_read_word( stream );
	state->remaining_ms = lan_read_word( stream );
	state->scene = lan_read_int( stream );
	state->language = lan_read_int( stream );
	state->nplayers = lan_read_int( stream );
	state->timer_sec = lan_read_int( stream );
	state->battle = lan_read_int( stream );
	state->question_number = lan_read_int( stream );
	state->answered = lan_read_int( stream );
	state->correct = lan_read_int( stream );
	state->picked = lan_read_int( stream );
	state->was_correct = lan_read_int( stream );
	state->winner = lan_read_int( stream );
	state->question_index = lan_read_int( stream );
	state->category = lan_read_int( stream );
	state->sel_peg = lan_read_int( stream );
	state->target_r = lan_read_int( stream );
	state->target_c = lan_read_int( stream );
	lan_read_board( stream, &state->board );
	for ( i = 0; i < BOARD_NPLAYERS; i++ )
	{
		lan_read_text( stream, state->names[i], LAN_NAME_SIZE );
	}
	lan_read_text( stream, state->question.q, sizeof state->question.q );
	for ( i = 0; i < 4; i++ )
	{
		lan_read_text( stream, state->question.a[i],
		               sizeof state->question.a[i] );
	}
	if ( state->scene < 1 || state->scene > 4 || state->language < 0 ||
	     state->language >= QUIZ_LANGUAGE_COUNT || state->nplayers < 1 ||
	     state->nplayers > BOARD_NPLAYERS ||
	     ( state->timer_sec != 9 && state->timer_sec != 15 &&
	       state->timer_sec != 30 && state->timer_sec != 60 &&
	       state->timer_sec != 600 ) ||
	     state->remaining_ms > (uint32_t)state->timer_sec * 1000 ||
	     state->correct < -1 || state->correct > 3 || state->picked < -1 ||
	     state->picked > 3 || state->sel_peg < -1 ||
	     state->sel_peg >= BOARD_NPEGS || state->target_r < -1 ||
	     state->target_r >= BOARD_MAX_SIZE || state->target_c < -1 ||
	     state->target_c >= BOARD_MAX_SIZE || state->winner < -2 ||
	     state->winner >= BOARD_NPLAYERS || state->category < 0 ||
	     state->category >= QUIZ_NCAT || state->question_index < 0 )
	{
		stream->failed = 1;
	}
}

/**
  * @brief  Accept a bounded number of pending connections without blocking.
  * @param  session Listening host.
  * @param  now Monotonic milliseconds.
  * @retval None.
  */
static void lan_accept( lan_session *session, uint32_t now )
{
	lan_impl    *impl = session->impl;
	lan_socket_t socket;
	lan_peer    *peer;
	int          i, attempts;

	for ( attempts = 0; attempts < 8; attempts++ )
	{
		socket = accept( impl->listener, NULL, NULL );
		if ( socket == LAN_INVALID )
		{
			return;
		}
		for ( i = 0; i < BOARD_NPLAYERS; i++ )
		{
			if ( impl->peers[i].socket == LAN_INVALID )
			{
				break;
			}
		}
		if ( impl->started || lan_player_count( session ) >= BOARD_NPLAYERS ||
		     i == BOARD_NPLAYERS || lan_configure_socket( socket ) != 0 )
		{
			lan_close_socket( socket );
			continue;
		}
		peer = &impl->peers[i];
		memset( peer, 0, sizeof *peer );
		peer->socket = socket;
		peer->opened_at = now;
		peer->received_at = now;
		peer->ping_at = now;
	}
}

/**
  * @brief  Finish a pending TCP connect and queue the initial player name.
  * @param  impl Client transport.
  * @param  peer Host connection.
  * @retval 0 still connecting or ready, -5 connection failure.
  */
static int lan_finish_connect( lan_impl *impl, lan_peer *peer )
{
	fd_set         writes, errors;
	struct timeval timeout = { 0, 0 };
	int            result, error = 0;
#ifdef _WIN32
	int length = sizeof error;
#else
	socklen_t length = sizeof error;
#endif

	FD_ZERO( &writes );
	FD_ZERO( &errors );
	FD_SET( peer->socket, &writes );
	FD_SET( peer->socket, &errors );
	result =
	    select( (int)( peer->socket + 1 ), NULL, &writes, &errors, &timeout );
	if ( result < 0 )
	{
		return lan_would_block() ? 0 : -5;
	}
	if ( result == 0 )
	{
		return 0;
	}
	if ( getsockopt( peer->socket, SOL_SOCKET, SO_ERROR, (char *)&error,
	                 &length ) != 0 ||
	     error )
	{
		return -5;
	}
	peer->connecting = 0;
	return lan_queue( peer, LAN_MSG_HELLO, (const uint8_t *)impl->name,
	                  LAN_NAME_SIZE );
}

/**
  * @brief  Interpret one validated frame according to the session role.
  * @param  session Active session.
  * @param  peer Sender connection.
  * @param  type Frame type.
  * @param  stream Payload cursor.
  * @param  event Receives any application event.
  * @param  state Receives a guest snapshot.
  * @retval 1 application event, 0 internal message, -7 protocol error.
  */
static int lan_message_read( lan_session *session, lan_peer *peer, int type,
                             lan_stream *stream, lan_event *event,
                             lan_snapshot *state )
{
	lan_impl  *impl = session->impl;
	uint8_t    welcome[4];
	lan_stream out = { welcome, sizeof welcome, 0, 0 };
	int        i, result = 0;

	if ( type == LAN_MSG_PING && peer->ready )
	{
		return stream->size == 0 ? 0 : -7;
	}
	if ( session->role == LAN_HOST )
	{
		if ( type == LAN_MSG_HELLO && !peer->ready && !impl->started )
		{
			for ( i = 1; i < BOARD_NPLAYERS && session->connected[i]; i++ )
			{
			}
			if ( i >= BOARD_NPLAYERS )
			{
				return -7;
			}
			lan_read_text( stream, session->names[i], LAN_NAME_SIZE );
			if ( stream->failed || !session->names[i][0] )
			{
				return -7;
			}
			peer->player = i;
			memcpy( peer->name, session->names[i], LAN_NAME_SIZE );
			peer->ready = 1;
			session->connected[i] = 1;
			lan_write_word( &out, (uint32_t)i );
			lan_queue( peer, LAN_MSG_WELCOME, welcome, out.pos );
			lan_roster( session );
			event->type = LAN_EVENT_JOINED;
			event->player = i;
			result = 1;
		}
		else if ( type == LAN_MSG_COMMAND && peer->ready )
		{
			event->command.epoch = lan_read_word( stream );
			event->command.kind = lan_read_int( stream );
			event->command.peg = lan_read_int( stream );
			event->command.row = lan_read_int( stream );
			event->command.col = lan_read_int( stream );
			event->command.answer = lan_read_int( stream );
			event->type = LAN_EVENT_COMMAND;
			event->player = peer->player;
			result = 1;
		}
		else
		{
			return -7;
		}
	}
	else if ( type == LAN_MSG_WELCOME )
	{
		i = lan_read_int( stream );
		if ( i < 1 || i >= BOARD_NPLAYERS )
		{
			return -7;
		}
		session->local_player = i;
		peer->ready = 1;
		event->type = LAN_EVENT_JOINED;
		event->player = i;
		result = 1;
	}
	else if ( type == LAN_MSG_ROSTER && peer->ready )
	{
		for ( i = 0; i < BOARD_NPLAYERS; i++ )
		{
			session->connected[i] = lan_read_int( stream );
			lan_read_text( stream, session->names[i], LAN_NAME_SIZE );
			if ( session->connected[i] != 0 && session->connected[i] != 1 )
			{
				stream->failed = 1;
			}
		}
	}
	else if ( type == LAN_MSG_STATE && peer->ready )
	{
		lan_read_state( stream, state );
		event->type = LAN_EVENT_STATE;
		result = 1;
	}
	else
	{
		return -7;
	}
	return stream->failed || stream->pos != stream->size ? -7 : result;
}

/**
  * @brief  Consume at most one complete buffered frame.
  * @param  session Active session.
  * @param  peer Sender connection.
  * @param  event Receives an application event.
  * @param  state Receives a snapshot.
  * @retval 1 event, 0 incomplete/internal frame, -7 protocol error.
  */
static int lan_parse_frame( lan_session *session, lan_peer *peer,
                            lan_event *event, lan_snapshot *state )
{
	lan_stream header, payload;
	uint32_t   magic, type, size;
	int        result;

	if ( peer->rx_size < 12 )
	{
		return 0;
	}
	header = (lan_stream){ peer->rx, peer->rx_size, 0, 0 };
	magic = lan_read_word( &header );
	type = lan_read_word( &header );
	size = lan_read_word( &header );
	if ( magic != LAN_MAGIC || size > LAN_FRAME_MAX )
	{
		return -7;
	}
	if ( peer->rx_size < size + 12 )
	{
		return 0;
	}
	payload = (lan_stream){ peer->rx + 12, size, 0, 0 };
	result =
	    lan_message_read( session, peer, (int)type, &payload, event, state );
	peer->rx_size -= size + 12;
	memmove( peer->rx, peer->rx + size + 12, peer->rx_size );
	return result;
}

//------------------------------------------------------------------------------
//--------------------------------- Public API ---------------------------------
//------------------------------------------------------------------------------

int lan_host( lan_session *session, uint16_t port, const char *name )
{
	lan_impl          *impl;
	struct sockaddr_in address;
	int                result, enabled = 1;

	if ( !port )
	{
		return -1;
	}
	result = lan_open( session, name );
	if ( result != 0 )
	{
		return result;
	}
	impl = session->impl;
	impl->listener = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if ( impl->listener == LAN_INVALID ||
	     lan_configure_socket( impl->listener ) != 0 )
	{
		lan_close( session );
		return -3;
	}
#ifndef _WIN32
	// Windows uses exclusive binds to avoid silently sharing a game port.
	setsockopt( impl->listener, SOL_SOCKET, SO_REUSEADDR,
	            (const char *)&enabled, sizeof enabled );
#else
	setsockopt( impl->listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
	            (const char *)&enabled, sizeof enabled );
#endif
	memset( &address, 0, sizeof address );
	address.sin_family = AF_INET;
	address.sin_port = htons( port );
	address.sin_addr.s_addr = htonl( INADDR_ANY );
	if ( bind( impl->listener, (struct sockaddr *)&address, sizeof address ) !=
	         0 ||
	     listen( impl->listener, 4 ) != 0 )
	{
		lan_close( session );
		return -4;
	}
	session->role = LAN_HOST;
	session->local_player = 0;
	session->connected[0] = 1;
	snprintf( session->names[0], LAN_NAME_SIZE, "%s", name );
	return 0;
}

int lan_join( lan_session *session, const char *address, uint16_t port,
              const char *name, uint32_t now )
{
	lan_impl          *impl;
	lan_peer          *peer;
	struct sockaddr_in target;
	int                result;

	if ( !address || !port )
	{
		return -1;
	}
	memset( &target, 0, sizeof target );
	target.sin_family = AF_INET;
	target.sin_port = htons( port );
	if ( inet_pton( AF_INET, address, &target.sin_addr ) != 1 )
	{
		return -1;
	}
	result = lan_open( session, name );
	if ( result != 0 )
	{
		return result;
	}
	impl = session->impl;
	peer = &impl->peers[0];
	peer->socket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if ( peer->socket == LAN_INVALID ||
	     lan_configure_socket( peer->socket ) != 0 )
	{
		lan_close( session );
		return -3;
	}
	result = connect( peer->socket, (struct sockaddr *)&target, sizeof target );
	if ( result != 0 && !lan_would_block() )
	{
		lan_close( session );
		return -5;
	}
	peer->connecting = 1;
	peer->received_at = now;
	peer->opened_at = now;
	peer->ping_at = now;
	session->role = LAN_CLIENT;
	return 0;
}

void lan_close( lan_session *session )
{
	lan_impl *impl;
	int       i;

	if ( !session )
	{
		return;
	}
	impl = session->impl;
	if ( impl )
	{
		lan_close_socket( impl->listener );
		for ( i = 0; i < BOARD_NPLAYERS; i++ )
		{
			lan_close_socket( impl->peers[i].socket );
		}
#ifdef _WIN32
		if ( impl->sockets_started )
		{
			WSACleanup();
		}
#endif
		free( impl );
	}
	memset( session, 0, sizeof *session );
	session->local_player = -1;
}

int lan_poll( lan_session *session, lan_event *event, lan_snapshot *state,
              uint32_t now )
{
	lan_impl *impl;
	lan_peer *peer;
	size_t    before;
	int       i, received, result, frames;

	if ( !session || !session->impl || !event || !state )
	{
		return -1;
	}
	impl = session->impl;
	memset( event, 0, sizeof *event );
	if ( session->role == LAN_HOST )
	{
		lan_accept( session, now );
	}
	for ( i = 0; i < BOARD_NPLAYERS; i++ )
	{
		peer = &impl->peers[i];
		if ( peer->socket == LAN_INVALID )
		{
			continue;
		}
		if ( ( !peer->ready &&
		       now - peer->opened_at > LAN_HANDSHAKE_TIMEOUT ) ||
		     now - peer->received_at > LAN_TIMEOUT )
		{
			if ( lan_drop( session, peer, event, "Connection timed out." ) )
			{
				return 1;
			}
			continue;
		}
		if ( peer->connecting )
		{
			if ( lan_finish_connect( impl, peer ) != 0 )
			{
				return lan_drop( session, peer, event,
				                 "Cannot connect to host." );
			}
			if ( peer->connecting )
			{
				continue;
			}
		}
		if ( peer->ready && now - peer->ping_at >= 2000 && peer->tx_size == 0 )
		{
			lan_queue( peer, LAN_MSG_PING, NULL, 0 );
			peer->ping_at = now;
		}
		if ( lan_flush( peer ) != 0 )
		{
			if ( lan_drop( session, peer, event, "Connection lost." ) )
			{
				return 1;
			}
			continue;
		}
		// Parse buffered frames before reading more, including after an event.
		for ( frames = 0; frames < 32; frames++ )
		{
			before = peer->rx_size;
			result = lan_parse_frame( session, peer, event, state );
			if ( result < 0 )
			{
				return lan_drop( session, peer, event,
				                 "Incompatible LAN protocol." );
			}
			if ( result > 0 )
			{
				return result;
			}
			if ( peer->rx_size < before )
			{
				continue;
			}
			if ( peer->rx_size == sizeof peer->rx )
			{
				return lan_drop( session, peer, event,
				                 "LAN input limit exceeded." );
			}
			received =
			    (int)recv( peer->socket, (char *)peer->rx + peer->rx_size,
			               (int)( sizeof peer->rx - peer->rx_size ), 0 );
			if ( received < 0 )
			{
				if ( lan_would_block() )
				{
					break;
				}
				return lan_drop( session, peer, event, "Connection lost." );
			}
			if ( received == 0 )
			{
				return lan_drop( session, peer, event,
				                 "Host closed the connection (full, started or "
				                 "disconnected)." );
			}
			peer->received_at = now;
			peer->rx_size += (size_t)received;
		}
	}
	return 0;
}

int lan_begin( lan_session *session )
{
	lan_impl  *impl;
	lan_peer  *peer;
	uint8_t    data[4];
	lan_stream stream;
	int        i, player = 1;
	char       host_name[LAN_NAME_SIZE];
	char       name[LAN_NAME_SIZE];

	if ( !session || session->role != LAN_HOST ||
	     lan_player_count( session ) < 2 )
	{
		return -1;
	}
	impl = session->impl;
	memcpy( host_name, session->names[0], LAN_NAME_SIZE );
	memset( session->names, 0, sizeof session->names );
	memset( session->connected, 0, sizeof session->connected );
	memcpy( session->names[0], host_name, LAN_NAME_SIZE );
	session->connected[0] = 1;
	for ( i = 0; i < BOARD_NPLAYERS; i++ )
	{
		peer = &impl->peers[i];
		if ( !peer->ready )
		{
			continue;
		}
		memcpy( name, peer->name, LAN_NAME_SIZE );
		memcpy( session->names[player], name, LAN_NAME_SIZE );
		session->connected[player] = 1;
		peer->player = player;
		stream = (lan_stream){ data, sizeof data, 0, 0 };
		lan_write_word( &stream, (uint32_t)player );
		lan_queue( peer, LAN_MSG_WELCOME, data, stream.pos );
		player++;
	}
	impl->started = 1;
	lan_roster( session );
	return 0;
}

void lan_lobby( lan_session *session )
{
	lan_impl *impl;

	if ( session && session->role == LAN_HOST )
	{
		impl = session->impl;
		impl->started = 0;
		lan_roster( session );
	}
}

int lan_publish( lan_session *session, const lan_snapshot *state )
{
	lan_impl  *impl;
	uint8_t    data[LAN_FRAME_MAX];
	lan_stream stream = { data, sizeof data, 0, 0 };
	int        i;

	if ( !session || !session->impl || session->role != LAN_HOST || !state )
	{
		return -1;
	}
	impl = session->impl;
	lan_write_state( &stream, state );
	if ( stream.failed )
	{
		return -1;
	}
	for ( i = 0; i < BOARD_NPLAYERS; i++ )
	{
		if ( impl->peers[i].ready && impl->peers[i].tx_size == 0 )
		{
			lan_queue( &impl->peers[i], LAN_MSG_STATE, data, stream.pos );
		}
	}
	return 0;
}

int lan_send_command( lan_session *session, const lan_command *command )
{
	lan_impl  *impl;
	uint8_t    data[24];
	lan_stream stream = { data, sizeof data, 0, 0 };

	if ( !session || !session->impl || session->role != LAN_CLIENT ||
	     session->local_player < 1 || !command )
	{
		return -1;
	}
	impl = session->impl;
	lan_write_word( &stream, command->epoch );
	lan_write_word( &stream, (uint32_t)command->kind );
	lan_write_word( &stream, (uint32_t)command->peg );
	lan_write_word( &stream, (uint32_t)command->row );
	lan_write_word( &stream, (uint32_t)command->col );
	lan_write_word( &stream, (uint32_t)command->answer );
	return lan_queue( &impl->peers[0], LAN_MSG_COMMAND, data, stream.pos );
}

int lan_player_count( const lan_session *session )
{
	int i, count = 0;

	if ( session )
	{
		for ( i = 0; i < BOARD_NPLAYERS; i++ )
		{
			count += session->connected[i] != 0;
		}
	}
	return count;
}

int lan_local_addresses( char out[][LAN_ADDRESS_SIZE], int max )
{
	struct sockaddr_in to, me;
	struct hostent    *host;
	lan_socket_t       sock;
	char               text[LAN_ADDRESS_SIZE];
	int                count = 0, i, j;
#ifdef _WIN32
	WSADATA data;
	int     length = sizeof me;
#else
	socklen_t length = sizeof me;
#endif

	if ( !out || max < 1 )
	{
		return 0;
	}
#ifdef _WIN32
	if ( WSAStartup( MAKEWORD( 2, 2 ), &data ) != 0 )
	{
		return 0;
	}
#endif
	// The address the system would use to reach another network. Connecting
	// a UDP socket sends nothing, it only selects the outgoing interface.
	sock = socket( AF_INET, SOCK_DGRAM, 0 );
	if ( sock != LAN_INVALID )
	{
		memset( &to, 0, sizeof to );
		to.sin_family = AF_INET;
		to.sin_port = htons( 9 );
		to.sin_addr.s_addr = htonl( 0xc0000201u );  // 192.0.2.1, TEST-NET-1.
		if ( connect( sock, (struct sockaddr *)&to, sizeof to ) == 0 &&
		     getsockname( sock, (struct sockaddr *)&me, &length ) == 0 &&
		     me.sin_addr.s_addr != 0 &&
		     ( ntohl( me.sin_addr.s_addr ) >> 24 ) != 127 )
		{
			snprintf( out[count++], LAN_ADDRESS_SIZE, "%s",
			          inet_ntoa( me.sin_addr ) );
		}
#ifdef _WIN32
		closesocket( sock );
#else
		close( sock );
#endif
	}
	// Further interfaces known by the host name, without loopback and
	// link-local addresses.
	if ( gethostname( text, sizeof text ) == 0 &&
	     ( host = gethostbyname( text ) ) != NULL &&
	     host->h_addrtype == AF_INET )
	{
		for ( i = 0; host->h_addr_list[i] && count < max; i++ )
		{
			struct in_addr addr;
			uint32_t       v;
			int            seen = 0;

			memcpy( &addr, host->h_addr_list[i], sizeof addr );
			v = ntohl( addr.s_addr );
			if ( ( v >> 24 ) == 127 || ( v >> 16 ) == 0xa9fe || v == 0 )
			{
				continue;
			}
			snprintf( text, sizeof text, "%s", inet_ntoa( addr ) );
			for ( j = 0; j < count; j++ )
			{
				seen |= strcmp( out[j], text ) == 0;
			}
			if ( !seen )
			{
				snprintf( out[count++], LAN_ADDRESS_SIZE, "%s", text );
			}
		}
	}
#ifdef _WIN32
	WSACleanup();
#endif
	return count;
}