//  Description:
//
//		quiz.h -- load and select Sraz questions in English or Croatian.
//
//		The correct answer stays at index zero; the UI shuffles its indices.
//
#ifndef QUIZ_H
#define QUIZ_H

#include <stdint.h>

#define QUIZ_NCAT 8
#define QUIZ_TEXT_SIZE 512

typedef enum
{
	QUIZ_LANGUAGE_EN,
	QUIZ_LANGUAGE_CRO,
	QUIZ_LANGUAGE_COUNT
} quiz_language;

typedef struct
{
	char q[QUIZ_TEXT_SIZE];
	char a[4][QUIZ_TEXT_SIZE];
} quiz_question;

typedef struct
{
	quiz_question *q;  // a[0] is always the correct answer.
	long           total;
	uint32_t       bound[QUIZ_NCAT];
	unsigned char *used;
	quiz_language  language;
} quiz_bank;

/**
  * @brief  Load or replace a bank from 0011.100 and 0011.200.
  *
  * Initialise the bank to zero before its first load. Failed loads leave
  * the previous bank intact.
  *
  * @param  qz Receives the decoded bank and owns its allocations.
  * @param  dir Directory containing the original question files.
  * @param  language Language used for decoding and category labels.
  * @retval 0 on success, -1 invalid argument, -2 read failure,
  *         -3 invalid data, -4 allocation failure, -5 path too long.
  */
int quiz_load( quiz_bank *qz, const char *dir, quiz_language language );

/**
  * @brief  Release a bank and clear its state.
  * @param  qz Bank to release, may be NULL.
  * @retval None.
  */
void quiz_free( quiz_bank *qz );

/**
  * @brief  Get a translated category label.
  * @param  language Label language.
  * @param  cat Category index from zero to QUIZ_NCAT - 1.
  * @retval Label, or an empty string for invalid arguments.
  */
const char *quiz_category_name( quiz_language language, int cat );

/**
  * @brief  Find the category of a question.
  * @param  qz Loaded bank.
  * @param  idx Question index.
  * @retval Category index from zero to QUIZ_NCAT - 1, -1 invalid index.
  */
int quiz_category_of( const quiz_bank *qz, long idx );

/**
  * @brief  Pick a random question in a category or the entire bank.
  * @param  qz Loaded bank.
  * @param  cat Category from 1 to 8, or 0 for any category.
  * @retval Question index, -1 invalid or empty range.
  */
long quiz_random_index( const quiz_bank *qz, int cat );

/**
  * @brief  Clear the used-question marks for a new game.
  * @param  qz Loaded bank.
  * @retval None.
  */
void quiz_reset_used( quiz_bank *qz );

/**
  * @brief  Pick and mark an unused question, resetting an exhausted range.
  * @param  qz Loaded bank.
  * @param  cat Category from 1 to 8, or 0 for any category.
  * @retval Question index, -1 invalid or empty range.
  */
long quiz_pick_unused( quiz_bank *qz, int cat );

/**
  * @brief  Shuffle answer indices and locate the correct answer.
  * @param  order Receives a permutation of 0, 1, 2, 3.
  * @retval Slot containing answer zero, -1 if order is NULL.
  */
int quiz_shuffle( int order[4] );

#endif
