//  Description:
//
//		quiz.c -- decode the original Sraz question banks.
//
//		0011.100: eight little-endian cumulative category counts, then one
//		little-endian offset per question into 0011.200.
//		0011.200: five length-prefixed strings per record (question, correct
//		answer, three distractors). Byte k decodes as encoded - cipher + k + 1.
//		The cipher is 0x80 for questions and 0x1e for answers.
//		Only Croatian banks map ASCII punctuation to Croatian diacritics.
//

//------------------------------------------------------------------------------
//------------------------------- Includes -------------------------------------
//------------------------------------------------------------------------------

#include "quiz.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//------------------------------------------------------------------------------
//---------------------------- Local variables ---------------------------------
//------------------------------------------------------------------------------

static const char *const quiz_category_names[QUIZ_LANGUAGE_COUNT][QUIZ_NCAT] = {
    { "Sport", "History", "Geography", "Nature", "Music", "General knowledge",
      "Art", "Science" },
    { "Sport", "Povijest", "Zemljopis", "Priroda", "Glazba", "Opće znanje",
      "Umjetnost", "Znanost" } };

//------------------------------------------------------------------------------
//----------------------------- Local functions --------------------------------
//------------------------------------------------------------------------------

/**
  * @brief  Read a complete binary file.
  * @param  path File to read.
  * @param  out Receives allocated bytes on success.
  * @param  size Receives the byte count.
  * @retval 0 on success, -2 read failure, -4 allocation failure.
  */
static int quiz_read_file( const char *path, unsigned char **out, size_t *size )
{
	FILE          *file;
	unsigned char *data;
	long           length;
	int            failed;

	file = fopen( path, "rb" );
	if ( !file )
	{
		return -2;
	}
	if ( fseek( file, 0, SEEK_END ) != 0 || ( length = ftell( file ) ) <= 0 )
	{
		fclose( file );
		return -2;
	}
	rewind( file );
	data = malloc( (size_t)length );
	if ( !data )
	{
		fclose( file );
		return -4;
	}
	failed = fread( data, 1, (size_t)length, file ) != (size_t)length;
	if ( fclose( file ) != 0 || failed )
	{
		free( data );
		return -2;
	}
	*out = data;
	*size = (size_t)length;
	return 0;
}

/**
  * @brief  Read an unaligned little-endian index word.
  * @param  p Four readable bytes.
  * @retval Decoded 32-bit value.
  */
static uint32_t quiz_read_u32le( const unsigned char *p )
{
	return (uint32_t)p[0] | ( (uint32_t)p[1] << 8 ) | ( (uint32_t)p[2] << 16 ) |
	       ( (uint32_t)p[3] << 24 );
}

/**
  * @brief  Map one original Croatian glyph to UTF-8.
  * @param  c Original single-byte character code.
  * @retval UTF-8 replacement, or NULL for an unchanged character.
  */
static const char *quiz_cro_glyph( unsigned char c )
{
	switch ( c )
	{
		case '{':
			return "š";
		case '[':
			return "Š";
		case '}':
			return "ć";
		case ']':
			return "Ć";
		case '~':
			return "č";
		case '^':
			return "Č";
		case 0x60:
			return "ž";
		case '@':
			return "Ž";
		case '|':
			return "đ";
		case '\\':
			return "Đ";
		default:
			return NULL;
	}
}

/**
  * @brief  Decode one string without crossing the source or output buffers.
  * @param  data Original bank bytes.
  * @param  size Readable byte count.
  * @param  off Receives the next string offset.
  * @param  cipher Per-field cipher offset.
  * @param  language Controls the Croatian glyph mapping.
  * @param  out Receives a terminated UTF-8 string.
  * @retval 0 on success, -3 truncated or invalid string.
  */
static int quiz_decode_string( const unsigned char *data, size_t size,
                               size_t *off, int cipher, quiz_language language,
                               char out[QUIZ_TEXT_SIZE] )
{
	size_t        count, i, written = 0;
	unsigned char c;
	const char   *glyph;

	if ( *off >= size )
	{
		return -3;
	}
	count = data[( *off )++];
	if ( count > size - *off )
	{
		return -3;
	}
	for ( i = 0; i < count; i++ )
	{
		c = (unsigned char)( data[*off + i] - cipher + (int)i + 1 );
		glyph = language == QUIZ_LANGUAGE_CRO ? quiz_cro_glyph( c ) : NULL;
		if ( c == 0 || written + ( glyph ? 2 : 1 ) >= QUIZ_TEXT_SIZE )
		{
			return -3;
		}
		if ( glyph )
		{
			out[written++] = glyph[0];
			out[written++] = glyph[1];
		}
		else
		{
			out[written++] = (char)c;
		}
	}
	out[written] = '\0';
	*off += count;
	return 0;
}

/**
  * @brief  Resolve a category to a nonempty half-open question range.
  * @param  qz Loaded bank.
  * @param  cat Category from 1 to 8, or 0 for any category.
  * @param  lo Receives the first index.
  * @param  hi Receives the index after the last question.
  * @retval 0 on success, -1 invalid or empty range.
  */
static int quiz_range( const quiz_bank *qz, int cat, long *lo, long *hi )
{
	if ( !qz || !qz->q || qz->total <= 0 || cat < 0 || cat > QUIZ_NCAT )
	{
		return -1;
	}
	*lo = cat <= 1 ? 0 : (long)qz->bound[cat - 2];
	*hi = cat == 0 ? qz->total : (long)qz->bound[cat - 1];
	return *hi > *lo ? 0 : -1;
}

//------------------------------------------------------------------------------
//-------------------------------- Public API ----------------------------------
//------------------------------------------------------------------------------

int quiz_load( quiz_bank *qz, const char *dir, quiz_language language )
{
	char           path[1024];
	unsigned char *index = NULL, *data = NULL;
	size_t         index_size = 0, data_size = 0, off;
	quiz_bank      loaded = { 0 };
	uint32_t       previous = 0;
	long           i;
	int            cat, answer, result, length;

	if ( !qz || !dir || language < 0 || language >= QUIZ_LANGUAGE_COUNT )
	{
		return -1;
	}
	length = snprintf( path, sizeof path, "%s/0011.100", dir );
	if ( length < 0 || (size_t)length >= sizeof path )
	{
		return -5;
	}
	result = quiz_read_file( path, &index, &index_size );
	if ( result != 0 )
	{
		return result;
	}
	length = snprintf( path, sizeof path, "%s/0011.200", dir );
	if ( length < 0 || (size_t)length >= sizeof path )
	{
		free( index );
		return -5;
	}
	result = quiz_read_file( path, &data, &data_size );
	if ( result != 0 )
	{
		free( index );
		return result;
	}
	result = -3;
	if ( index_size < QUIZ_NCAT * 4 )
	{
		goto cleanup;
	}
	for ( cat = 0; cat < QUIZ_NCAT; cat++ )
	{
		loaded.bound[cat] = quiz_read_u32le( index + cat * 4 );
		if ( loaded.bound[cat] <= previous )
		{
			goto cleanup;
		}
#if LONG_MAX < UINT32_MAX
		if ( loaded.bound[cat] > LONG_MAX )
		{
			goto cleanup;
		}
#endif
		previous = loaded.bound[cat];
	}
	loaded.total = (long)previous;
	loaded.language = language;
	if ( (size_t)loaded.total > ( index_size - QUIZ_NCAT * 4 ) / 4 ||
	     (size_t)loaded.total > SIZE_MAX / sizeof *loaded.q )
	{
		goto cleanup;
	}
	loaded.q = calloc( (size_t)loaded.total, sizeof *loaded.q );
	loaded.used = calloc( (size_t)loaded.total, sizeof *loaded.used );
	if ( !loaded.q || !loaded.used )
	{
		result = -4;
		goto cleanup;
	}
	for ( i = 0; i < loaded.total; i++ )
	{
		off = quiz_read_u32le( index + ( QUIZ_NCAT + (size_t)i ) * 4 );
		if ( quiz_decode_string( data, data_size, &off, 0x80, language,
		                         loaded.q[i].q ) != 0 )
		{
			goto cleanup;
		}
		for ( answer = 0; answer < 4; answer++ )
		{
			if ( quiz_decode_string( data, data_size, &off, 0x1e, language,
			                         loaded.q[i].a[answer] ) != 0 )
			{
				goto cleanup;
			}
		}
	}
	quiz_free( qz );
	*qz = loaded;
	memset( &loaded, 0, sizeof loaded );
	result = 0;

cleanup:
	free( index );
	free( data );
	quiz_free( &loaded );
	return result;
}

void quiz_free( quiz_bank *qz )
{
	if ( qz )
	{
		free( qz->q );
		free( qz->used );
		memset( qz, 0, sizeof *qz );
	}
}

const char *quiz_category_name( quiz_language language, int cat )
{
	if ( language < 0 || language >= QUIZ_LANGUAGE_COUNT || cat < 0 ||
	     cat >= QUIZ_NCAT )
	{
		return "";
	}
	return quiz_category_names[language][cat];
}

int quiz_category_of( const quiz_bank *qz, long idx )
{
	int cat;

	if ( !qz || idx < 0 || idx >= qz->total )
	{
		return -1;
	}
	for ( cat = 0; cat < QUIZ_NCAT; cat++ )
	{
		if ( (uint32_t)idx < qz->bound[cat] )
		{
			return cat;
		}
	}
	return -1;
}

long quiz_random_index( const quiz_bank *qz, int cat )
{
	long lo, hi;

	if ( quiz_range( qz, cat, &lo, &hi ) != 0 )
	{
		return -1;
	}
	return lo + rand() % ( hi - lo );
}

void quiz_reset_used( quiz_bank *qz )
{
	if ( qz && qz->used )
	{
		memset( qz->used, 0, (size_t)qz->total );
	}
}

long quiz_pick_unused( quiz_bank *qz, int cat )
{
	long lo, hi, i, available = 0, pick;

	if ( quiz_range( qz, cat, &lo, &hi ) != 0 || !qz->used )
	{
		return -1;
	}
	for ( i = lo; i < hi; i++ )
	{
		if ( !qz->used[i] )
		{
			available++;
		}
	}
	if ( available == 0 )
	{
		// Repeat only after every question in this range has been asked.
		memset( qz->used + lo, 0, (size_t)( hi - lo ) );
		available = hi - lo;
	}
	pick = rand() % available;
	for ( i = lo; i < hi; i++ )
	{
		if ( !qz->used[i] && pick-- == 0 )
		{
			qz->used[i] = 1;
			return i;
		}
	}
	return -1;
}

int quiz_shuffle( int order[4] )
{
	int i, j, tmp;

	if ( !order )
	{
		return -1;
	}
	for ( i = 0; i < 4; i++ )
	{
		order[i] = i;
	}
	for ( i = 3; i > 0; i-- )
	{
		j = rand() % ( i + 1 );
		tmp = order[i];
		order[i] = order[j];
		order[j] = tmp;
	}
	for ( i = 0; i < 4; i++ )
	{
		if ( order[i] == 0 )
		{
			return i;
		}
	}
	return -1;
}
