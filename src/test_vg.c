//  Description:
//
//		test_vg.c -- sanity checks of the vector rasteriser and PNG writer.
//
//		Also writes build/vg_sheet.png, a contact sheet of every art asset,
//		for a visual review of the rasteriser.
//
//------------------------------------------------------------------------------
//---------------------------------- Includes ----------------------------------
//------------------------------------------------------------------------------

#include "vg.h"
#include "png.h"
#include "../art/art_data.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//------------------------------------------------------------------------------
//------------------------------ Local variables -------------------------------
//------------------------------------------------------------------------------

static int test_failed;

//------------------------------------------------------------------------------
//------------------------------- Local functions ------------------------------
//------------------------------------------------------------------------------

/**
  * @brief  Report one check.
  * @param  ok Nonzero when the check passed.
  * @param  name Description.
  * @retval None.
  */
static void test_check( int ok, const char *name )
{
	printf( "%s: %s\n", ok ? "PASS" : "FAIL", name );
	test_failed += !ok;
}

/**
  * @brief  Sum a coverage buffer.
  * @param  cov Coverage values.
  * @param  n Count.
  * @retval Sum.
  */
static double test_sum( const float *cov, int n )
{
	double s = 0.0;
	int    i;

	for ( i = 0; i < n; i++ )
	{
		s += cov[i];
	}
	return s;
}

/**
  * @brief  Check areas of basic shapes against geometry.
  * @retval None.
  */
static void test_shapes( void )
{
	static float cov[64 * 64];
	vg_poly      p, s;
	double       area;

	vg_poly_init( &p );
	vg_poly_init( &s );
	vg_move( &p, 10.25f, 10.5f );
	vg_line( &p, 30.25f, 10.5f );
	vg_line( &p, 30.25f, 20.5f );
	vg_line( &p, 10.25f, 20.5f );
	vg_close( &p );
	vg_cover( &p, 64, 64, 0, cov );
	area = test_sum( cov, 64 * 64 );
	test_check( fabs( area - 200.0 ) < 0.01, "rectangle area is 200" );

	vg_poly_clear( &p );
	vg_rrect( &p, 4.0f, 4.0f, 40.0f, 40.0f, 20.0f );
	vg_cover( &p, 64, 64, 0, cov );
	area = test_sum( cov, 64 * 64 );
	test_check( fabs( area - 3.14159265 * 400.0 ) < 12.0,
	            "circle area within 1 percent" );

	vg_poly_clear( &p );
	vg_move( &p, 5, 5 ); vg_line( &p, 45, 5 ); vg_line( &p, 45, 45 );
	vg_line( &p, 5, 45 ); vg_close( &p );
	vg_move( &p, 15, 15 ); vg_line( &p, 35, 15 ); vg_line( &p, 35, 35 );
	vg_line( &p, 15, 35 ); vg_close( &p );
	vg_cover( &p, 64, 64, 1, cov );
	area = test_sum( cov, 64 * 64 );
	test_check( fabs( area - ( 1600.0 - 400.0 ) ) < 0.01,
	            "even-odd leaves a hole" );
	vg_cover( &p, 64, 64, 0, cov );
	area = test_sum( cov, 64 * 64 );
	test_check( fabs( area - 1600.0 ) < 0.01,
	            "non-zero fills equally wound contours solid" );

	vg_poly_clear( &p );
	vg_move( &p, 10, 32 );
	vg_line( &p, 30, 32 );
	vg_stroke( &p, 4.0f, &s );
	vg_cover( &s, 64, 64, 0, cov );
	area = test_sum( cov, 64 * 64 );
	test_check( fabs( area - ( 80.0 + 3.14159265 * 4.0 ) ) < 1.5,
	            "round-capped stroke area" );
	vg_poly_free( &p );
	vg_poly_free( &s );
}

/**
  * @brief  Count pixels with some alpha.
  * @param  rgba Image.
  * @param  n Pixel count.
  * @retval Number of non-transparent pixels.
  */
static int test_opaque( const uint8_t *rgba, int n )
{
	int i, c = 0;

	for ( i = 0; i < n; i++ )
	{
		c += rgba[i * 4 + 3] > 0;
	}
	return c;
}

/**
  * @brief  Render every asset, check it is not empty, tint it, and write a
  *         contact sheet.
  * @retval None.
  */
static void test_assets( void )
{
	enum { CELL = 112, COLS = 8 };
	int      rows = ( ART_COUNT + COLS - 1 ) / COLS, id, empty = 0, x, y;
	int      sw = COLS * CELL, sh = rows * CELL;
	uint8_t *sheet, *img, *a, *b;
	vg_tint  red, blue;
	float    aw, ah, sc;
	int      w, h, k, px, py;

	sheet = (uint8_t *)malloc( (size_t)sw * (size_t)sh * 4 );
	img = (uint8_t *)malloc( 400 * 400 * 4 );
	for ( k = 0; k < sw * sh; k++ )
	{
		sheet[k * 4] = 120; sheet[k * 4 + 1] = 130; sheet[k * 4 + 2] = 150;
		sheet[k * 4 + 3] = 255;
	}
	vg_tint_make( ART_PLAYER_RED, 3, &red );
	for ( id = 0; id < ART_COUNT; id++ )
	{
		vg_asset_size( id, &aw, &ah );
		sc = ( CELL - 8 ) / ( aw > ah ? aw : ah );
		w = (int)( aw * sc );
		h = (int)( ah * sc );
		w = w < 1 ? 1 : w;
		h = h < 1 ? 1 : h;
		if ( vg_render_asset( id, w, h, &red, img ) != 0 ||
		     test_opaque( img, w * h ) == 0 )
		{
			empty++;
			continue;
		}
		x = ( id % COLS ) * CELL + ( CELL - w ) / 2;
		y = ( id / COLS ) * CELL + ( CELL - h ) / 2;
		for ( py = 0; py < h; py++ )
		{
			for ( px = 0; px < w; px++ )
			{
				a = img + ( py * w + px ) * 4;
				b = sheet + ( ( y + py ) * sw + x + px ) * 4;
				for ( k = 0; k < 3; k++ )
				{
					b[k] = (uint8_t)( ( a[k] * a[3] + b[k] * ( 255 - a[3] ) ) /
					                  255 );
				}
			}
		}
	}
	test_check( empty == 0, "all 40 assets render non-empty" );
	vg_tint_make( ART_PLAYER_BLUE, 3, &blue );
	a = (uint8_t *)malloc( 64 * 80 * 4 );
	b = (uint8_t *)malloc( 64 * 80 * 4 );
	vg_render_asset( ART_PEG_0, 64, 80, &red, a );
	vg_render_asset( ART_PEG_0, 64, 80, &blue, b );
	test_check( memcmp( a, b, 64 * 80 * 4 ) != 0, "tint changes a peg" );
	test_check( vg_asset_tokens( ART_PEG_0 ) != 0 &&
	                vg_asset_tokens( ART_HOLE ) == 0,
	            "token masks: peg tinted, hole fixed" );
	free( a );
	free( b );
	test_check( png_write( "build/vg_sheet.png", sw, sh, sheet ) == 0,
	            "contact sheet PNG written" );
	free( sheet );
	free( img );
}

//------------------------------------------------------------------------------
//------------------------------- Public API -----------------------------------
//------------------------------------------------------------------------------

/**
  * @brief  Run the rasteriser tests.
  * @retval 0 if all passed, 1 otherwise.
  */
int main( void )
{
	test_shapes();
	test_assets();
	printf( "%s\n", test_failed ? "vg tests FAILED" : "vg tests passed" );
	return test_failed ? 1 : 0;
}
