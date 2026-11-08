//  Description:
//
//		lan.h -- nonblocking LAN sessions for one host and three guests.
//
//		The host owns all game rules and question selection. Guests receive
//		renderable snapshots and send only move/answer intentions. No socket
//		or compiler-specific structure layout appears in the wire protocol.
//
#ifndef LAN_H
#define LAN_H

#include "board.h"
#include "quiz.h"
#include <stdint.h>

#define LAN_DEFAULT_PORT 34567
#define LAN_NAME_SIZE 24
#define LAN_ADDRESS_SIZE 64

typedef enum
{
	LAN_OFF,
	LAN_HOST,
	LAN_CLIENT
} lan_role;

typedef enum
{
	LAN_EVENT_JOINED = 1,
	LAN_EVENT_LEFT,
	LAN_EVENT_STATE,
	LAN_EVENT_COMMAND,
	LAN_EVENT_ERROR
} lan_event_type;

typedef enum
{
	LAN_MOVE = 1,  // peg, row, col: step onto a neighbouring tile.
	LAN_ANSWER,    // answer: chosen display slot 0..3.
	LAN_PASS       // End the turn (Space in the original).
} lan_command_type;

typedef struct
{
	uint32_t epoch;
	int      kind, peg, row, col, answer;
} lan_command;

typedef struct
{
	board_state   board;
	quiz_question question;  // Answers are already in their display order.
	char          names[BOARD_NPLAYERS][LAN_NAME_SIZE];
	uint32_t      epoch, remaining_ms;
	int           scene, language, nplayers, timer_sec;
	int           battle, question_number, answered, correct, picked;
	int           was_correct, winner;
	int           question_index, category;
	int           sel_peg, target_r, target_c;  // Selection, -1 if none.
} lan_snapshot;

typedef struct
{
	void    *impl;
	lan_role role;
	int      local_player;
	int      connected[BOARD_NPLAYERS];
	char     names[BOARD_NPLAYERS][LAN_NAME_SIZE];
	char     error[160];
} lan_session;

typedef struct
{
	lan_event_type type;
	int            player;
	lan_command    command;
} lan_event;

/**
  * @brief  Listen for up to three guests; the host occupies player zero.
  * @param  session Zero-initialised session, or one previously closed.
  * @param  port TCP port, nonzero.
  * @param  name Host player name, 1 to LAN_NAME_SIZE - 1 bytes.
  * @retval 0 on success; -1 argument, -2 memory, -3 socket, -4 bind/listen error.
  */
int lan_host( lan_session *session, uint16_t port, const char *name );

/**
  * @brief  Begin a nonblocking connection to a host's numeric IPv4 address.
  * @param  session Zero-initialised session, or one previously closed.
  * @param  address Numeric IPv4 address.
  * @param  port Host TCP port, nonzero.
  * @param  name Guest player name, 1 to LAN_NAME_SIZE - 1 bytes.
  * @param  now Current monotonic time in milliseconds.
  * @retval 0 started; -1 argument, -2 memory, -3 socket, -5 connection error.
  */
int lan_join( lan_session *session, const char *address, uint16_t port,
              const char *name, uint32_t now );

/**
  * @brief  Release sockets and session allocations.
  * @param  session Session to close, may be NULL.
  * @retval None.
  */
void lan_close( lan_session *session );

/**
  * @brief  Service sockets and return at most one session event.
  * @param  session Active session.
  * @param  event Receives one event.
  * @param  state Receives a snapshot for LAN_EVENT_STATE.
  * @param  now Current monotonic time in milliseconds.
  * @retval 1 for an event, 0 if no event is pending, -1 invalid arguments.
  */
int lan_poll( lan_session *session, lan_event *event, lan_snapshot *state,
              uint32_t now );

/**
  * @brief  Compact lobby seats and refuse new joins until the lobby reopens.
  * @param  session Host session with two to four connected players.
  * @retval 0 on success, -1 invalid session or insufficient players.
  */
int lan_begin( lan_session *session );

/**
  * @brief  Reopen a host lobby after a game or a disconnection.
  * @param  session Host session.
  * @retval None.
  */
void lan_lobby( lan_session *session );

/**
  * @brief  Queue a host snapshot, skipping guests with pending output.
  * @param  session Host session.
  * @param  state Renderable state; correct must be -1 before an answer.
  * @retval 0 on success, -1 invalid arguments.
  */
int lan_publish( lan_session *session, const lan_snapshot *state );

/**
  * @brief  Queue an intention for the host to validate.
  * @param  session Connected client session.
  * @param  command Move or answer with its server-issued epoch.
  * @retval 0 queued, -1 invalid arguments, -6 output queue full.
  */
int lan_send_command( lan_session *session, const lan_command *command );

/**
  * @brief  Count the currently connected players, including the host.
  * @param  session Active session.
  * @retval Player count from zero to four.
  */
int lan_player_count( const lan_session *session );

/**
  * @brief  List the IPv4 addresses of this computer on its networks.
  *
  * The address used to reach other networks comes first. Loopback and
  * link-local addresses are never listed, so the result can be shown to
  * other players as the address to join.
  *
  * @param  out Receives dotted-decimal addresses.
  * @param  max Capacity of out, at least 1.
  * @retval Number of addresses written, zero if the computer has none.
  */
int lan_local_addresses( char out[][LAN_ADDRESS_SIZE], int max );

#endif
