//  Description:
//
//		text.c -- UTF-8 text rendering with stb_truetype and a glyph cache.
//
//		Each (code point, pixel size) pair is rasterised once into a white
//		texture whose alpha is the glyph coverage; drawing tints it with the
//		texture colour modulation. Sizes are whole pixels derived from the
//		canvas scale, so text stays sharp at any window size. Advances are
//		accumulated as floats and only the glyph origin is rounded.
//

//------------------------------------------------------------------------------
//---------------------------------- Includes ----------------------------------
//------------------------------------------------------------------------------

#include "gfx.h"
#include "../third_party/stb/stb_truetype.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

//------------------------------------------------------------------------------
//------------------------- Local macros and constants -------------------------
//------------------------------------------------------------------------------

#define TEXT_GLYPHS 4096
#define TEXT_MAX_LINES 96

extern const unsigned char font_ttf[];
extern const unsigned long font_ttf_len;

//------------------------------------------------------------------------------
//--------------------------- Local types and structures -----------------------
//------------------------------------------------------------------------------

typedef struct
{
	const char *s;
	int         n;
} text_line;

//------------------------------------------------------------------------------
//------------------------------- Local functions ------------------------------
//------------------------------------------------------------------------------

/**
  * @brief  Decode the next UTF-8 code point.
  * @param  s In: position, out: position after the code point.
  * @retval Code point, '?' for malformed input, 0 at the end.
  */
static int text_next( const char **s )
{
	const unsigned char *p = (const unsigned char *)*s;
	int                  cp, n, i;

	if ( !*p )
	{
		return 0;
	}
	if ( *p < 0x80 )
	{
		*s += 1;
		return *p;
	}
	if ( ( *p & 0xE0 ) == 0xC0 )
	{
		cp = *p & 0x1F;
		n = 1;
	}
	else if ( ( *p & 0xF0 ) == 0xE0 )
	{
		cp = *p & 0x0F;
		n = 2;
	}
	else if ( ( *p & 0xF8 ) == 0xF0 )
	{
		cp = *p & 0x07;
		n = 3;
	}
	else
	{
		*s += 1;
		return '?';
	}
	for ( i = 1; i <= n; i++ )
	{
		if ( ( p[i] & 0xC0 ) != 0x80 )
		{
			*s += i;
			return '?';
		}
		cp = ( cp << 6 ) | ( p[i] & 0x3F );
	}
	*s += n + 1;
	return cp;
}

/**
  * @brief  Convert a canvas font size to whole pixels.
  * @param  g State.
  * @param  size Canvas units.
  * @retval Pixel height, at least 6.
  */
static int text_px( const gfx *g, float size )
{
	int px = (int)floorf( size * g->scale + 0.5f );

	return px < 6 ? 6 : px;
}

/**
  * @brief  Find or build a glyph.
  * @param  g State.
  * @param  cp Code point.
  * @param  px Pixel size.
  * @retval Cached glyph, NULL if the table is full.
  */
static gfx_glyph *text_glyph( gfx *g, int cp, int px )
{
	stbtt_fontinfo *f = (stbtt_fontinfo *)g->font;
	uint32_t        i = ( (uint32_t)cp * 2654435761u ^ (uint32_t)px * 40503u ) &
	             ( TEXT_GLYPHS - 1 );
	gfx_glyph      *e;
	unsigned char  *bmp, *rgba;
	float           sc;
	int             adv, lsb, w, h, xo, yo, k;

	for ( k = 0; k < TEXT_GLYPHS; k++ )
	{
		e = &g->gtab[( i + (uint32_t)k ) & ( TEXT_GLYPHS - 1 )];
		if ( !e->used )
		{
			break;
		}
		if ( e->cp == cp && e->px == px )
		{
			return e;
		}
	}
	if ( k == TEXT_GLYPHS )
	{
		return NULL;
	}
	sc = stbtt_ScaleForPixelHeight( f, (float)px );
	stbtt_GetCodepointHMetrics( f, cp, &adv, &lsb );
	e->used = 1;
	e->cp = cp;
	e->px = px;
	e->adv = (float)adv * sc;
	e->tex = NULL;
	e->w = e->h = 0;
	bmp = stbtt_GetCodepointBitmap( f, sc, sc, cp, &w, &h, &xo, &yo );
	if ( bmp && w > 0 && h > 0 )
	{
		rgba = (unsigned char *)malloc( (size_t)w * (size_t)h * 4 );
		if ( rgba )
		{
			for ( k = 0; k < w * h; k++ )
			{
				rgba[k * 4] = rgba[k * 4 + 1] = rgba[k * 4 + 2] = 255;
				rgba[k * 4 + 3] = bmp[k];
			}
			e->tex = SDL_CreateTexture( g->ren, SDL_PIXELFORMAT_RGBA32,
			                            SDL_TEXTUREACCESS_STATIC, w, h );
			if ( e->tex )
			{
				SDL_UpdateTexture( e->tex, NULL, rgba, w * 4 );
				SDL_SetTextureBlendMode( e->tex, SDL_BLENDMODE_BLEND );
				e->w = w;
				e->h = h;
				e->xoff = xo;
				e->yoff = yo;
			}
			free( rgba );
		}
	}
	if ( bmp )
	{
		stbtt_FreeBitmap( bmp, NULL );
	}
	g->gcount++;
	return e;
}

/**
  * @brief  Measure a run of text in pixels.
  * @param  g State.
  * @param  px Pixel size.
  * @param  s Text.
  * @param  n Byte count.
  * @retval Width in pixels.
  */
static float text_measure( gfx *g, int px, const char *s, int n )
{
	const char *end = s + n;
	gfx_glyph  *gl;
	float       w = 0.0f;
	int         cp;

	while ( s < end && ( cp = text_next( &s ) ) != 0 )
	{
		gl = text_glyph( g, cp, px );
		w += gl ? gl->adv : (float)px * 0.5f;
	}
	return w;
}

/**
  * @brief  Break a paragraph into lines no wider than a limit.
  * @param  g State.
  * @param  px Pixel size.
  * @param  maxw Limit in pixels.
  * @param  str Text.
  * @param  lines Receives the lines (pointers into str).
  * @retval Number of lines.
  */
static int text_layout( gfx *g, int px, float maxw, const char *str,
                        text_line *lines, int max )
{
	const char *p = str, *ls = str, *le = str, *we, *q;
	float       cw = 0.0f, ww, sp, cwid;
	int         n = 0, cp;

	sp = text_measure( g, px, " ", 1 );
	for ( ;; )
	{
		if ( *p == '\n' || *p == '\0' )
		{
			if ( n < max )
			{
				lines[n].s = ls;
				lines[n].n = (int)( le - ls );
				n++;
			}
			if ( *p == '\0' )
			{
				break;
			}
			p++;
			ls = le = p;
			cw = 0.0f;
			continue;
		}
		if ( *p == ' ' )
		{
			p++;
			continue;
		}
		we = p;
		while ( *we && *we != ' ' && *we != '\n' )
		{
			we++;
		}
		ww = text_measure( g, px, p, (int)( we - p ) );
		if ( le > ls && cw + sp + ww > maxw )
		{
			if ( n < max )
			{
				lines[n].s = ls;
				lines[n].n = (int)( le - ls );
				n++;
			}
			ls = le = p;
			cw = 0.0f;
		}
		if ( le == ls )
		{
			ls = p;
		}
		if ( ww > maxw )
		{
			// A single word wider than the column is broken between letters.
			cw = 0.0f;
			q = p;
			while ( q < we )
			{
				const char *nx = q;
				gfx_glyph  *gl;

				cp = text_next( &nx );
				gl = text_glyph( g, cp, px );
				cwid = gl ? gl->adv : (float)px * 0.5f;
				if ( cw + cwid > maxw && q > ls )
				{
					if ( n < max )
					{
						lines[n].s = ls;
						lines[n].n = (int)( q - ls );
						n++;
					}
					ls = q;
					cw = 0.0f;
				}
				cw += cwid;
				q = nx;
			}
			le = we;
			p = we;
			continue;
		}
		cw = le > ls ? cw + sp + ww : ww;
		le = we;
		p = we;
	}
	return n;
}

//------------------------------------------------------------------------------
//------------------------------- Public API -----------------------------------
//------------------------------------------------------------------------------

int gfx_text_init( gfx *g )
{
	stbtt_fontinfo *f = (stbtt_fontinfo *)calloc( 1, sizeof *f );

	g->gtab = (gfx_glyph *)calloc( TEXT_GLYPHS, sizeof *g->gtab );
	g->gcap = TEXT_GLYPHS;
	if ( !f || !g->gtab || !stbtt_InitFont( f, font_ttf, 0 ) )
	{
		free( f );
		return -1;
	}
	g->font = f;
	return 0;
}

void gfx_glyph_flush( gfx *g )
{
	int i;

	for ( i = 0; i < g->gcap; i++ )
	{
		if ( g->gtab[i].used && g->gtab[i].tex )
		{
			SDL_DestroyTexture( g->gtab[i].tex );
		}
	}
	memset( g->gtab, 0, (size_t)g->gcap * sizeof *g->gtab );
	g->gcount = 0;
}

float gfx_line_height( gfx *g, float size )
{
	stbtt_fontinfo *f = (stbtt_fontinfo *)g->font;
	int             asc, desc, gap, px = text_px( g, size );
	float           sc = stbtt_ScaleForPixelHeight( f, (float)px );

	stbtt_GetFontVMetrics( f, &asc, &desc, &gap );
	return (float)( asc - desc + gap ) * sc / g->scale;
}

float gfx_text_width( gfx *g, float size, const char *str )
{
	return text_measure( g, text_px( g, size ), str, (int)strlen( str ) ) /
	       g->scale;
}

/**
  * @brief  Draw a run of text at pixel coordinates.
  * @param  g State.
  * @param  px Pixel size.
  * @param  x Pen X in pixels.
  * @param  y Top of the line in pixels.
  * @param  s Text.
  * @param  n Byte count.
  * @param  color 0xRRGGBB.
  * @param  alpha Opacity.
  * @retval None.
  */
static void text_run( gfx *g, int px, float x, float y, const char *s, int n,
                      uint32_t color, float alpha )
{
	stbtt_fontinfo *f = (stbtt_fontinfo *)g->font;
	const char     *end = s + n;
	gfx_glyph      *gl;
	SDL_Rect        d;
	int             asc, desc, gap, cp, base;

	stbtt_GetFontVMetrics( f, &asc, &desc, &gap );
	base = (int)floorf( y + (float)asc * stbtt_ScaleForPixelHeight( f, (float)px ) +
	                    0.5f );
	while ( s < end && ( cp = text_next( &s ) ) != 0 )
	{
		gl = text_glyph( g, cp, px );
		if ( !gl )
		{
			continue;
		}
		if ( gl->tex )
		{
			d.x = (int)floorf( x + 0.5f ) + gl->xoff;
			d.y = base + gl->yoff;
			d.w = gl->w;
			d.h = gl->h;
			SDL_SetTextureColorMod( gl->tex, (Uint8)( ( color >> 16 ) & 255 ),
			                        (Uint8)( ( color >> 8 ) & 255 ),
			                        (Uint8)( color & 255 ) );
			SDL_SetTextureAlphaMod( gl->tex, (Uint8)( alpha * 255.0f + 0.5f ) );
			SDL_RenderCopy( g->ren, gl->tex, NULL, &d );
		}
		x += gl->adv;
	}
}

float gfx_text( gfx *g, float size, float x, float y, const char *str,
                uint32_t color, int align, float alpha )
{
	int   px = text_px( g, size );
	float w = text_measure( g, px, str, (int)strlen( str ) );
	float ax = g->ox + x * g->scale;

	if ( align == GFX_CENTRE )
	{
		ax -= w * 0.5f;
	}
	else if ( align == GFX_RIGHT )
	{
		ax -= w;
	}
	text_run( g, px, ax, g->oy + y * g->scale, str, (int)strlen( str ), color,
	          alpha );
	return w / g->scale;
}

float gfx_text_wrap( gfx *g, float size, float x, float y, float w,
                     const char *str, uint32_t color, int align, float alpha,
                     int draw )
{
	text_line lines[TEXT_MAX_LINES];
	int       px = text_px( g, size ), n, i;
	float     lh = gfx_line_height( g, size ), lw, ax;

	n = text_layout( g, px, w * g->scale, str, lines, TEXT_MAX_LINES );
	if ( draw )
	{
		for ( i = 0; i < n; i++ )
		{
			lw = text_measure( g, px, lines[i].s, lines[i].n );
			ax = g->ox + x * g->scale;
			if ( align == GFX_CENTRE )
			{
				ax += ( w * g->scale - lw ) * 0.5f;
			}
			else if ( align == GFX_RIGHT )
			{
				ax += w * g->scale - lw;
			}
			text_run( g, px, ax, g->oy + ( y + (float)i * lh ) * g->scale,
			          lines[i].s, lines[i].n, color, alpha );
		}
	}
	return (float)n * lh;
}

float gfx_text_fit( gfx *g, float max_size, float min_size, float x, float y,
                    float w, float h, const char *str, uint32_t color,
                    int align )
{
	float size = max_size, th;

	for ( ;; )
	{
		th = gfx_text_wrap( g, size, x, y, w, str, color, align, 1.0f, 0 );
		if ( th <= h || size <= min_size )
		{
			break;
		}
		size -= 0.5f;
	}
	th = gfx_text_wrap( g, size, x, y + ( h - th > 0.0f ? ( h - th ) * 0.5f : 0.0f ),
	                    w, str, color, align, 1.0f, 1 );
	return size;
}
