//  Description:
//
//		snd.c -- synthesised beeps and jingles through SDL audio.
//
//		An effect is a table of { frequency, milliseconds } notes (0 Hz is a
//		rest). The callback walks the table of the effect in progress and
//		renders a 50 % square wave softened with a triangle, with a short
//		attack and release per note so the notes do not click.
//

//------------------------------------------------------------------------------
//---------------------------------- Includes ----------------------------------
//------------------------------------------------------------------------------

#include "snd.h"
#include <SDL.h>
#include <string.h>

//------------------------------------------------------------------------------
//------------------------- Local macros and constants -------------------------
//------------------------------------------------------------------------------

#define SND_RATE 22050
#define SND_VOLUME 0.16f

//------------------------------------------------------------------------------
//--------------------------- Local types and structures -----------------------
//------------------------------------------------------------------------------

typedef struct
{
	int freq, ms;
} snd_note;

//------------------------------------------------------------------------------
//------------------------------ Local variables -------------------------------
//------------------------------------------------------------------------------

static const snd_note snd_correct[] = { { 523, 80 }, { 659, 80 }, { 784, 140 }, { 0, 0 } };
static const snd_note snd_wrong[] = { { 220, 90 }, { 185, 90 }, { 147, 90 }, { 110, 220 }, { 0, 0 } };
static const snd_note snd_lost[] = { { 330, 100 }, { 262, 100 }, { 196, 100 }, { 131, 260 }, { 0, 0 } };
static const snd_note snd_turn[] = { { 440, 55 }, { 476, 55 }, { 512, 55 }, { 548, 90 }, { 0, 0 } };
static const snd_note snd_win[] = {
	{ 523, 120 }, { 659, 120 }, { 784, 120 }, { 1047, 240 }, { 0, 60 },
	{ 784, 120 }, { 1047, 360 }, { 0, 60 }, { 698, 120 }, { 880, 120 },
	{ 1047, 120 }, { 1397, 240 }, { 1319, 120 }, { 1047, 120 }, { 1175, 120 },
	{ 1047, 480 }, { 0, 0 }
};
static const snd_note snd_click[] = { { 1100, 18 }, { 0, 0 } };

static const snd_note *const snd_tables[] = {
	snd_correct, snd_wrong, snd_lost, snd_turn, snd_win, snd_click
};

static SDL_AudioDeviceID   snd_dev;
static volatile int        snd_on = 1;
static const snd_note     *volatile snd_cur;
static int                 snd_idx, snd_left;  // Samples left in the note.
static int                 snd_len;
static float               snd_phase;

//------------------------------------------------------------------------------
//------------------------------- Local functions ------------------------------
//------------------------------------------------------------------------------

/**
  * @brief  SDL audio callback: render the current effect or silence.
  * @param  user Unused.
  * @param  stream Output buffer of signed 16 bit mono samples.
  * @param  len Buffer size in bytes.
  * @retval None.
  */
static void snd_callback( void *user, Uint8 *stream, int len )
{
	Sint16 *out = (Sint16 *)stream;
	int     n = len / 2, i;
	float   v, env, t;

	( void )user;
	for ( i = 0; i < n; i++ )
	{
		v = 0.0f;
		if ( snd_cur )
		{
			if ( snd_left <= 0 )
			{
				snd_idx++;
				if ( snd_cur[snd_idx].freq == 0 && snd_cur[snd_idx].ms == 0 )
				{
					snd_cur = NULL;
				}
				else
				{
					snd_len = snd_cur[snd_idx].ms * SND_RATE / 1000;
					snd_left = snd_len;
				}
			}
			if ( snd_cur )
			{
				snd_left--;
				if ( snd_cur[snd_idx].freq > 0 )
				{
					snd_phase += (float)snd_cur[snd_idx].freq / SND_RATE;
					snd_phase -= (float)(int)snd_phase;
					t = snd_phase < 0.5f ? 4.0f * snd_phase - 1.0f
					                      : 3.0f - 4.0f * snd_phase;
					v = ( snd_phase < 0.5f ? 0.6f : -0.6f ) + 0.4f * t;
					env = 1.0f;
					if ( snd_len - snd_left < 120 )
					{
						env = (float)( snd_len - snd_left ) / 120.0f;
					}
					else if ( snd_left < 200 )
					{
						env = (float)snd_left / 200.0f;
					}
					v *= env * SND_VOLUME;
				}
			}
		}
		out[i] = (Sint16)( v * 32767.0f );
	}
}

//------------------------------------------------------------------------------
//------------------------------- Public API -----------------------------------
//------------------------------------------------------------------------------

int snd_init( void )
{
	SDL_AudioSpec want, have;

	if ( SDL_InitSubSystem( SDL_INIT_AUDIO ) != 0 )
	{
		return -1;
	}
	memset( &want, 0, sizeof want );
	want.freq = SND_RATE;
	want.format = AUDIO_S16SYS;
	want.channels = 1;
	want.samples = 512;
	want.callback = snd_callback;
	snd_dev = SDL_OpenAudioDevice( NULL, 0, &want, &have, 0 );
	if ( snd_dev == 0 || have.format != AUDIO_S16SYS || have.channels != 1 )
	{
		if ( snd_dev )
		{
			SDL_CloseAudioDevice( snd_dev );
			snd_dev = 0;
		}
		return -1;
	}
	SDL_PauseAudioDevice( snd_dev, 0 );
	return 0;
}

void snd_free( void )
{
	if ( snd_dev )
	{
		SDL_CloseAudioDevice( snd_dev );
		snd_dev = 0;
	}
}

void snd_enable( int on )
{
	snd_on = on != 0;
	if ( !snd_on && snd_dev )
	{
		SDL_LockAudioDevice( snd_dev );
		snd_cur = NULL;
		SDL_UnlockAudioDevice( snd_dev );
	}
}

int snd_enabled( void )
{
	return snd_on;
}

void snd_play( int id )
{
	if ( !snd_dev || !snd_on || id < 0 ||
	     id >= (int)( sizeof snd_tables / sizeof snd_tables[0] ) )
	{
		return;
	}
	SDL_LockAudioDevice( snd_dev );
	snd_idx = 0;
	snd_len = snd_tables[id][0].ms * SND_RATE / 1000;
	snd_left = snd_len;
	snd_phase = 0.0f;
	snd_cur = snd_tables[id];
	SDL_UnlockAudioDevice( snd_dev );
}
