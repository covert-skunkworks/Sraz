//  Description:
//
//		strs.h -- interface strings in English and Croatian.
//
//		One X-macro list defines both the identifiers and the two texts, so
//		the languages cannot drift out of step. Language numbers follow
//		quiz_language: 0 English, 1 Croatian. Game messages and the winner
//		text follow part 5 of docs/original.
//
#ifndef STRS_H
#define STRS_H

#define STRS_LIST( X ) \
	X( S_SUBTITLE, "A trivia board game", "Društvena igra znanja" ) \
	X( S_PLAY, "Play", "Igraj" ) \
	X( S_HOST, "Host LAN game", "Pokreni LAN igru" ) \
	X( S_JOIN, "Join LAN game", "Pridruži se LAN igri" ) \
	X( S_OPTIONS, "Options", "Postavke" ) \
	X( S_QUIT, "Quit", "Izlaz" ) \
	X( S_BACK, "Back", "Natrag" ) \
	X( S_START, "Start", "Start" ) \
	X( S_PRESS_KEY, "Press any key", "Pritisni bilo koju tipku" ) \
	X( S_SETUP_TITLE, "New game", "Nova igra" ) \
	X( S_PLAYERS, "Players", "Igrači" ) \
	X( S_PLAYER_NAME, "Player %d", "Igrač %d" ) \
	X( S_DEFAULT_NAME, "Player", "Igrac" ) \
	X( S_BOARD, "Board", "Ploča" ) \
	X( S_TIME, "Time to answer", "Vrijeme za odgovor" ) \
	X( S_QLANG, "Question language", "Jezik pitanja" ) \
	X( S_SECONDS, "%d s", "%d s" ) \
	X( S_SOUND, "Sound", "Zvuk" ) \
	X( S_LANGUAGE, "Language", "Jezik" ) \
	X( S_FULLSCREEN, "Fullscreen", "Cijeli zaslon" ) \
	X( S_THEME, "Theme", "Tema" ) \
	X( S_ON, "On", "Uključeno" ) \
	X( S_OFF, "Off", "Isključeno" ) \
	X( S_LIGHT, "Light", "Svijetla" ) \
	X( S_DARK, "Dark", "Tamna" ) \
	X( S_ARMY, "Army colours", "Boje vojski" ) \
	X( S_HOST_TITLE, "Host a LAN game", "LAN igra - domaćin" ) \
	X( S_JOIN_TITLE, "Join a LAN game", "LAN igra - pridruživanje" ) \
	X( S_GIVE_ADDR, "Give this address to the other players:", \
	   "Ovu adresu dajte ostalim igračima:" ) \
	X( S_PORT, "Port", "Port" ) \
	X( S_YOUR_NAME, "Your name", "Vaše ime" ) \
	X( S_OPEN_LOBBY, "Open lobby", "Otvori dvoranu" ) \
	X( S_CLOSE_LOBBY, "Close lobby", "Zatvori dvoranu" ) \
	X( S_CONNECTED_PLAYERS, "Connected players", "Spojeni igrači" ) \
	X( S_WAIT_PLAYERS, "Waiting for at least one more player...", \
	   "Čekam barem još jednog igrača..." ) \
	X( S_START_GAME, "Start game", "Započni igru" ) \
	X( S_NO_ADDR, "No network address found. Is this computer connected?", \
	   "Nije pronađena mrežna adresa. Je li računalo spojeno?" ) \
	X( S_ADDRESS, "Host address", "Adresa domaćina" ) \
	X( S_CONNECT, "Connect", "Spoji se" ) \
	X( S_CONNECTING, "Connecting...", "Spajanje..." ) \
	X( S_WAIT_HOST, "Connected. Waiting for the host to start the game.", \
	   "Spojeno. Čekam da domaćin započne igru." ) \
	X( S_DISCONNECT, "Disconnect", "Odspoji se" ) \
	X( S_YOU, "you", "vi" ) \
	X( S_HOST_TAG, "host", "domaćin" ) \
	X( S_COPY, "Copy", "Kopiraj" ) \
	X( S_COPIED, "Copied to the clipboard", "Kopirano u međuspremnik" ) \
	X( S_BAD_PORT, "Enter a port from 1 to 65535.", \
	   "Unesite port od 1 do 65535." ) \
	X( S_BAD_ADDR, "Enter the host's IPv4 address.", \
	   "Unesite IPv4 adresu domaćina." ) \
	X( S_TURN, "On turn", "Na potezu" ) \
	X( S_YOUR_MOVE, "Your move", "Vaš potez" ) \
	X( S_SELECT_PEG, "Pick a peg and press Enter", \
	   "Odaberite figuru i pritisnite Enter" ) \
	X( S_SELECT_TARGET, "Pick a neighbouring tile", \
	   "Odaberite susjedno polje" ) \
	X( S_WAITING, "Waiting for %s", "Čekam: %s" ) \
	X( S_PASS_HINT, "Space: pass the turn", "Razmak: preskoči potez" ) \
	X( S_PASS_KEY, "Pass (Space)", "Preskoči" ) \
	X( S_RANK0, "Serf", "Kmet" ) \
	X( S_RANK1, "Soldier", "Vojnik" ) \
	X( S_RANK2, "Knight", "Vitez" ) \
	X( S_TO_RANK, "to next rank", "do sljedećeg čina" ) \
	X( S_MAX_RANK, "highest rank", "najviši čin" ) \
	X( S_PEGS_LEFT, "pegs", "figura" ) \
	X( S_RANDOM, "Random", "Slučajno" ) \
	X( S_QUESTION_BATTLE, "Battle - question %d of 2", \
	   "Bitka - pitanje %d od 2" ) \
	X( S_QUESTION_FLAG, "Flag tile - capture it", \
	   "Zastava - osvojite je" ) \
	X( S_QUESTION_STEP, "Question", "Pitanje" ) \
	X( S_QUIT_1, "Do you really want to end the game?", \
	   "Želite li doista prekinuti igru?" ) \
	X( S_QUIT_2, "To be able to continue, save the", \
	   "Ako želite mogućnost nastavka prvo" ) \
	X( S_QUIT_3, "position first: F5", "snimite poziciju: F5" ) \
	X( S_QUIT_BTN, "END GAME (Y/N)", "KRAJ IGRE (D/N)" ) \
	X( S_QUIT_LAN_HOST, "Return everyone to the lobby?", \
	   "Vratiti sve u dvoranu?" ) \
	X( S_SAVED, "Game saved", "Pozicija snimljena" ) \
	X( S_LOADED, "Game loaded", "Pozicija učitana" ) \
	X( S_NO_SAVE, "No saved game found", "Ne postoji snimljena pozicija" ) \
	X( S_SAVE_FAIL, "Cannot save the game", "Ne može se snimiti pozicija" ) \
	X( S_SAVE_BUSY, "Finish the question first", "Prvo završite pitanje" ) \
	X( S_SAVE_LAN, "Not available in a LAN game", \
	   "Nije moguće u LAN igri" ) \
	X( S_SKIPPED, "%s cannot move and is skipped", \
	   "%s ne može igrati i preskače potez" ) \
	X( S_PASSED, "%s passes", "%s preskače potez" ) \
	X( S_LOST_PEG, "A peg was lost", "Izgubljena figura" ) \
	X( S_WIN_1, "ANOTHER SRAZ HAS ENDED.", "JOŠ JEDAN SRAZ JE ZAVRŠEN." ) \
	X( S_WIN_2, "ONLY THE %s ARMY IS LEFT ON THE BOARD.", \
	   "NA PLOČI JE JOŠ SAMO %s VOJSKA." ) \
	X( S_WIN_3, "THE SURVIVING SOLDIERS ARE FOREVER", \
	   "PREŽIVJELI VOJNICI SU ZAUVIJEK" ) \
	X( S_WIN_4, "GRATEFUL AND CHEER", "ZAHVALNI I KLIČU" ) \
	X( S_WIN_5, "CONGRATULATIONS!", "ČESTITAMO!" ) \
	X( S_WIN_STALE, "NOBODY CAN MOVE ANY MORE. THE LEADER IS", \
	   "NITKO SE VIŠE NE MOŽE POMAKNUTI. VODI" ) \
	X( S_WIN_CONT, "Click or press a key to continue", \
	   "Kliknite ili pritisnite tipku za nastavak" ) \
	X( S_COL_RED, "RED", "CRVENA" ) \
	X( S_COL_BLUE, "BLUE", "PLAVA" ) \
	X( S_COL_BLACK, "BLACK", "CRNA" ) \
	X( S_COL_WHITE, "WHITE", "BIJELA" ) \
	X( S_COL_GREEN, "GREEN", "ZELENA" ) \
	X( S_COL_PURPLE, "PURPLE", "LJUBIČASTA" ) \
	X( S_COL_BROWN, "BROWN", "SMEĐA" ) \
	X( S_HELP_KEYS, "F4 sound   F5 save   F8 load   F11 fullscreen   Esc menu", \
	   "F4 zvuk   F5 snimi   F8 učitaj   F11 cijeli zaslon   Esc izbornik" ) \
	X( S_LEFT_LAN, "The host closed the game.", "Domaćin je zatvorio igru." ) \
	X( S_PLAYER_LEFT, "A player left. Back in the lobby.", \
	   "Igrač je otišao. Natrag u dvorani." ) \
	X( S_LOAD_LAN, "Load is not available here", "Učitavanje nije moguće" ) \
	X( S_VERSION, "Version", "Verzija" ) \
	X( S_MODE_LOCAL, "On this computer", "Na ovom računalu" )

typedef enum
{
#define STRS_ENUM( id, en, hr ) id,
	STRS_LIST( STRS_ENUM )
#undef STRS_ENUM
	S_COUNT
} strs_id;

/**
  * @brief  Look up an interface string.
  * @param  lang 0 English, 1 Croatian.
  * @param  id String identifier.
  * @retval UTF-8 text, "?" for unknown identifiers.
  */
const char *tr( int lang, int id );

#endif
