//  Description:
//
//		snd.h -- synthesised beeps and jingles through SDL audio.
//
//		No sound files: each effect is a short list of notes rendered by a
//		small square/triangle oscillator in the audio callback. If the audio
//		device cannot be opened the module stays silent and every call is a
//		harmless no-op.
//
#ifndef SND_H
#define SND_H

#define SND_CORRECT 0
#define SND_WRONG 1
#define SND_LOST 2
#define SND_TURN 3
#define SND_WIN 4
#define SND_CLICK 5

/**
  * @brief  Open the audio device.
  * @retval 0 on success, -1 if audio is unavailable (play stays silent).
  */
int snd_init( void );

/**
  * @brief  Close the audio device.
  * @retval None.
  */
void snd_free( void );

/**
  * @brief  Switch all sound on or off (F4).
  * @param  on Nonzero for sound.
  * @retval None.
  */
void snd_enable( int on );

/**
  * @brief  Report whether sound is switched on.
  * @retval 1 if on, 0 if off.
  */
int snd_enabled( void );

/**
  * @brief  Start an effect, replacing the one that is playing.
  * @param  id SND_xxx.
  * @retval None.
  */
void snd_play( int id );

#endif
