//  Description:
//
//		png.c -- minimal PNG writer using stored (uncompressed) deflate blocks.
//
//		  signature, IHDR (8 bit RGBA), one IDAT holding a zlib stream of
//		  stored blocks of at most 65535 bytes, IEND.
//		  Each scanline is prefixed with filter byte 0.
//

//------------------------------------------------------------------------------
//---------------------------------- Includes ----------------------------------
//------------------------------------------------------------------------------

#include "png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//------------------------------------------------------------------------------
//------------------------------- Local functions ------------------------------
//------------------------------------------------------------------------------

/**
  * @brief  Continue a CRC-32 over a buffer.
  * @param  crc Running value (start with 0).
  * @param  p Data.
  * @param  n Length.
  * @retval Updated CRC-32.
  */
static uint32_t png_crc( uint32_t crc, const uint8_t *p, size_t n )
{
	static uint32_t table[256];
	static int      ready;
	uint32_t        c, k, i;

	if ( !ready )
	{
		for ( i = 0; i < 256; i++ )
		{
			c = i;
			for ( k = 0; k < 8; k++ )
			{
				c = c & 1 ? 0xEDB88320u ^ ( c >> 1 ) : c >> 1;
			}
			table[i] = c;
		}
		ready = 1;
	}
	c = crc ^ 0xFFFFFFFFu;
	for ( i = 0; i < n; i++ )
	{
		c = table[( c ^ p[i] ) & 255] ^ ( c >> 8 );
	}
	return c ^ 0xFFFFFFFFu;
}

/**
  * @brief  Store a 32-bit value big endian.
  * @param  p Destination.
  * @param  v Value.
  * @retval None.
  */
static void png_be32( uint8_t *p, uint32_t v )
{
	p[0] = (uint8_t)( v >> 24 ); p[1] = (uint8_t)( v >> 16 );
	p[2] = (uint8_t)( v >> 8 );  p[3] = (uint8_t)v;
}

/**
  * @brief  Write one PNG chunk.
  * @param  f Output file.
  * @param  type Four-letter chunk type.
  * @param  data Chunk data.
  * @param  n Data length.
  * @retval None.
  */
static void png_chunk( FILE *f, const char *type, const uint8_t *data,
                       size_t n )
{
	uint8_t  b[4];
	uint32_t crc;

	png_be32( b, (uint32_t)n );
	fwrite( b, 1, 4, f );
	fwrite( type, 1, 4, f );
	crc = png_crc( 0, (const uint8_t *)type, 4 );
	if ( n )
	{
		fwrite( data, 1, n, f );
		crc = png_crc( crc ^ 0, data, n );
	}
	png_be32( b, crc );
	fwrite( b, 1, 4, f );
}

//------------------------------------------------------------------------------
//------------------------------- Public API -----------------------------------
//------------------------------------------------------------------------------

int png_write( const char *path, int w, int h, const uint8_t *rgba )
{
	static const uint8_t sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	uint8_t              ihdr[13], *raw, *z;
	size_t               rawn, zn, pos, left, blk, i;
	uint32_t             a = 1, b = 0;
	FILE                *f;
	int                  y;

	if ( !path || !rgba || w < 1 || h < 1 )
	{
		return -1;
	}
	rawn = (size_t)h * ( (size_t)w * 4 + 1 );
	raw = (uint8_t *)malloc( rawn );
	zn = 2 + rawn + ( rawn / 65535 + 1 ) * 5 + 4;
	z = (uint8_t *)malloc( zn );
	if ( !raw || !z )
	{
		free( raw );
		free( z );
		return -3;
	}
	for ( y = 0; y < h; y++ )
	{
		raw[(size_t)y * ( (size_t)w * 4 + 1 )] = 0;
		memcpy( raw + (size_t)y * ( (size_t)w * 4 + 1 ) + 1,
		        rgba + (size_t)y * (size_t)w * 4, (size_t)w * 4 );
	}
	for ( i = 0; i < rawn; i++ )
	{
		a = ( a + raw[i] ) % 65521u;
		b = ( b + a ) % 65521u;
	}
	pos = 0;
	z[pos++] = 0x78;
	z[pos++] = 0x01;
	for ( left = rawn, i = 0; left > 0; )
	{
		blk = left > 65535 ? 65535 : left;
		z[pos++] = left == blk ? 1 : 0;
		z[pos++] = (uint8_t)( blk & 255 );
		z[pos++] = (uint8_t)( blk >> 8 );
		z[pos++] = (uint8_t)( ~blk & 255 );
		z[pos++] = (uint8_t)( ( ~blk >> 8 ) & 255 );
		memcpy( z + pos, raw + i, blk );
		pos += blk;
		i += blk;
		left -= blk;
	}
	png_be32( z + pos, ( b << 16 ) | a );
	pos += 4;
	f = fopen( path, "wb" );
	if ( !f )
	{
		free( raw );
		free( z );
		return -2;
	}
	fwrite( sig, 1, 8, f );
	png_be32( ihdr, (uint32_t)w );
	png_be32( ihdr + 4, (uint32_t)h );
	ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
	png_chunk( f, "IHDR", ihdr, 13 );
	png_chunk( f, "IDAT", z, pos );
	png_chunk( f, "IEND", NULL, 0 );
	fclose( f );
	free( raw );
	free( z );
	return 0;
}
