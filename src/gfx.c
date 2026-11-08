//  Description:
//
//		gfx.c -- SDL drawing layer: scaled canvas and the raster texture cache.
//
//		Textures are looked up by gfx_key in an open-addressing hash table
//		that is only ever emptied as a whole (on a flush), so there is no
//		deletion logic. A flush is requested after a window resize and applied
//		at the start of the next frame once the size has been stable for a
//		moment, or at once when the cache has grown past its memory budget.
//

//------------------------------------------------------------------------------
//---------------------------------- Includes ----------------------------------
//------------------------------------------------------------------------------

#include "gfx.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

//------------------------------------------------------------------------------
//------------------------- Local macros and constants -------------------------
//------------------------------------------------------------------------------

#define GFX_TAB_CAP 8192
#define GFX_BUDGET ( 192u * 1024u * 1024u )
#define GFX_FLUSH_DELAY 160

#define TEXT_GLYPH_LIMIT 2600
#define GFX_KIND_ART 1
#define GFX_KIND_SHAPE 2
#define GFX_KIND_TICK 3

//------------------------------------------------------------------------------
//------------------------ Local function prototypes ---------------------------
//------------------------------------------------------------------------------

static void gfx_flush( gfx *g );

//------------------------------------------------------------------------------
//------------------------------- Local functions ------------------------------
//------------------------------------------------------------------------------

/**
  * @brief  Hash a cache key.
  * @param  k Key.
  * @retval FNV-1a hash.
  */
static uint32_t gfx_hash( const gfx_key *k )
{
	const uint8_t *p = (const uint8_t *)k;
	uint32_t       h = 2166136261u;
	size_t         i;

	for ( i = 0; i < sizeof *k; i++ )
	{
		h = ( h ^ p[i] ) * 16777619u;
	}
	return h;
}

/**
  * @brief  Create a white-or-coloured RGBA texture from pixel data.
  * @param  g State.
  * @param  w Width.
  * @param  h Height.
  * @param  rgba Straight alpha pixels.
  * @retval Texture, NULL on failure.
  */
static SDL_Texture *gfx_make_tex( gfx *g, int w, int h, const uint8_t *rgba )
{
	SDL_Texture *t;

	t = SDL_CreateTexture( g->ren, SDL_PIXELFORMAT_RGBA32,
	                       SDL_TEXTUREACCESS_STATIC, w, h );
	if ( !t )
	{
		return NULL;
	}
	SDL_UpdateTexture( t, NULL, rgba, w * 4 );
	SDL_SetTextureBlendMode( t, SDL_BLENDMODE_BLEND );
	return t;
}

/**
  * @brief  Find a cached texture or rasterise it.
  * @param  g State.
  * @param  key Cache key.
  * @param  id Asset for GFX_KIND_ART.
  * @param  tint Tint for art, may be NULL.
  * @retval Texture, NULL on failure.
  */
static SDL_Texture *gfx_lookup( gfx *g, const gfx_key *key, int id,
                                const vg_tint *tint )
{
	uint32_t     i = gfx_hash( key ) & ( GFX_TAB_CAP - 1 );
	uint8_t     *px;
	SDL_Texture *t;
	int          rc;

	while ( g->tab[i].used )
	{
		if ( memcmp( &g->tab[i].key, key, sizeof *key ) == 0 )
		{
			return g->tab[i].tex;
		}
		i = ( i + 1 ) & ( GFX_TAB_CAP - 1 );
	}
	if ( g->tab_count >= GFX_TAB_CAP * 3 / 4 )
	{
		g->flush_pending = 1;
		g->flush_at = 0;
		return NULL;
	}
	px = (uint8_t *)malloc( (size_t)key->w * (size_t)key->h * 4 );
	if ( !px )
	{
		return NULL;
	}
	if ( key->kind == GFX_KIND_ART )
	{
		rc = vg_render_asset( id, key->w, key->h, tint, px );
	}
	else if ( key->kind == GFX_KIND_TICK )
	{
		rc = vg_render_tick( key->w, key->h, (float)key->b / 16.0f, px );
	}
	else
	{
		rc = vg_render_shape( key->w, key->h, (float)key->a / 16.0f,
		                      (float)key->b / 16.0f, px );
	}
	t = rc == 0 ? gfx_make_tex( g, key->w, key->h, px ) : NULL;
	free( px );
	if ( !t )
	{
		return NULL;
	}
	g->tab[i].key = *key;
	g->tab[i].tex = t;
	g->tab[i].used = 1;
	g->tab_count++;
	g->tab_bytes += (size_t)key->w * (size_t)key->h * 4;
	g->rasterised++;
	if ( g->tab_bytes > GFX_BUDGET )
	{
		g->flush_pending = 1;
		g->flush_at = 0;
	}
	return t;
}

/**
  * @brief  Destroy every cached texture.
  * @param  g State.
  * @retval None.
  */
static void gfx_flush( gfx *g )
{
	int i;

	for ( i = 0; i < GFX_TAB_CAP; i++ )
	{
		if ( g->tab[i].used )
		{
			SDL_DestroyTexture( g->tab[i].tex );
		}
	}
	memset( g->tab, 0, GFX_TAB_CAP * sizeof *g->tab );
	g->tab_count = 0;
	g->tab_bytes = 0;
	g->flush_pending = 0;
}

//------------------------------------------------------------------------------
//------------------------------- Public API -----------------------------------
//------------------------------------------------------------------------------

int gfx_init( gfx *g, SDL_Window *win, SDL_Renderer *ren )
{
	memset( g, 0, sizeof *g );
	g->win = win;
	g->ren = ren;
	g->tab_cap = GFX_TAB_CAP;
	g->tab = (gfx_entry *)calloc( GFX_TAB_CAP, sizeof *g->tab );
	if ( !g->tab || gfx_text_init( g ) != 0 )
	{
		return -1;
	}
	gfx_metrics( g );
	return 0;
}

void gfx_free( gfx *g )
{
	int i;

	if ( g->tab )
	{
		gfx_flush( g );
		free( g->tab );
	}
	if ( g->gtab )
	{
		for ( i = 0; i < g->gcap; i++ )
		{
			if ( g->gtab[i].used && g->gtab[i].tex )
			{
				SDL_DestroyTexture( g->gtab[i].tex );
			}
		}
		free( g->gtab );
	}
	free( g->font );
	memset( g, 0, sizeof *g );
}

void gfx_metrics( gfx *g )
{
	float sx, sy;

	SDL_GetRendererOutputSize( g->ren, &g->pw, &g->ph );
	g->pw = g->pw < 1 ? 1 : g->pw;
	g->ph = g->ph < 1 ? 1 : g->ph;
	sx = (float)g->pw / GFX_CW;
	sy = (float)g->ph / GFX_CH;
	g->scale = sx < sy ? sx : sy;
	g->ox = ( (float)g->pw - GFX_CW * g->scale ) * 0.5f;
	g->oy = ( (float)g->ph - GFX_CH * g->scale ) * 0.5f;
}

void gfx_request_flush( gfx *g, uint32_t now )
{
	g->flush_pending = 1;
	g->flush_at = now + GFX_FLUSH_DELAY;
}

void gfx_begin( gfx *g, uint32_t bg, uint32_t now )
{
	gfx_metrics( g );
	if ( g->gcount > TEXT_GLYPH_LIMIT )
	{
		gfx_glyph_flush( g );
	}
	if ( g->flush_pending && (int32_t)( now - g->flush_at ) >= 0 )
	{
		gfx_flush( g );
	}
	gfx_color( g, bg, 1.0f );
	SDL_RenderSetClipRect( g->ren, NULL );
	SDL_RenderClear( g->ren );
}

void gfx_to_canvas( const gfx *g, int px, int py, float *x, float *y )
{
	*x = ( (float)px - g->ox ) / g->scale;
	*y = ( (float)py - g->oy ) / g->scale;
}

SDL_Rect gfx_rect( const gfx *g, float x, float y, float w, float h )
{
	SDL_Rect r;
	int      x1, y1;

	r.x = (int)floorf( g->ox + x * g->scale + 0.5f );
	r.y = (int)floorf( g->oy + y * g->scale + 0.5f );
	x1 = (int)floorf( g->ox + ( x + w ) * g->scale + 0.5f );
	y1 = (int)floorf( g->oy + ( y + h ) * g->scale + 0.5f );
	r.w = x1 - r.x < 1 ? 1 : x1 - r.x;
	r.h = y1 - r.y < 1 ? 1 : y1 - r.y;
	return r;
}

void gfx_color( gfx *g, uint32_t color, float alpha )
{
	alpha = alpha < 0.0f ? 0.0f : ( alpha > 1.0f ? 1.0f : alpha );
	SDL_SetRenderDrawBlendMode( g->ren, alpha < 1.0f ? SDL_BLENDMODE_BLEND
	                                                 : SDL_BLENDMODE_NONE );
	SDL_SetRenderDrawColor( g->ren, (Uint8)( ( color >> 16 ) & 255 ),
	                        (Uint8)( ( color >> 8 ) & 255 ),
	                        (Uint8)( color & 255 ),
	                        (Uint8)( alpha * 255.0f + 0.5f ) );
}

void gfx_fill( gfx *g, float x, float y, float w, float h, uint32_t color,
               float alpha )
{
	SDL_Rect r = gfx_rect( g, x, y, w, h );

	gfx_color( g, color, alpha );
	SDL_RenderFillRect( g->ren, &r );
}

/**
  * @brief  Draw a cached shape texture tinted with a colour.
  * @param  g State.
  * @param  x Left.
  * @param  y Top.
  * @param  w Width.
  * @param  h Height.
  * @param  r Radius in canvas units.
  * @param  sw Outline width in canvas units, 0 for filled.
  * @param  color 0xRRGGBB.
  * @param  alpha Opacity.
  * @retval None.
  */
static void gfx_shape( gfx *g, float x, float y, float w, float h, float r,
                       float sw, uint32_t color, float alpha )
{
	SDL_Rect     d = gfx_rect( g, x, y, w, h );
	gfx_key      k;
	SDL_Texture *t;

	memset( &k, 0, sizeof k );
	k.kind = GFX_KIND_SHAPE;
	k.w = d.w;
	k.h = d.h;
	k.a = (int32_t)( r * g->scale * 16.0f + 0.5f );
	k.b = (int32_t)( sw * g->scale * 16.0f + 0.5f );
	if ( sw > 0.0f && k.b < 8 )
	{
		k.b = 8;  // Keep hairlines visible.
	}
	t = gfx_lookup( g, &k, 0, NULL );
	if ( !t )
	{
		return;
	}
	SDL_SetTextureColorMod( t, (Uint8)( ( color >> 16 ) & 255 ),
	                        (Uint8)( ( color >> 8 ) & 255 ),
	                        (Uint8)( color & 255 ) );
	SDL_SetTextureAlphaMod( t, (Uint8)( alpha * 255.0f + 0.5f ) );
	SDL_RenderCopy( g->ren, t, NULL, &d );
}

void gfx_rrect( gfx *g, float x, float y, float w, float h, float r,
                uint32_t color, float alpha )
{
	gfx_shape( g, x, y, w, h, r, 0.0f, color, alpha );
}

void gfx_ring( gfx *g, float x, float y, float w, float h, float r, float sw,
               uint32_t color, float alpha )
{
	gfx_shape( g, x, y, w, h, r, sw, color, alpha );
}

void gfx_art( gfx *g, int id, float x, float y, float w, float h,
              const vg_tint *tint, float alpha )
{
	SDL_Rect     d = gfx_rect( g, x, y, w, h );
	gfx_key      k;
	uint32_t     mask = vg_asset_tokens( id );
	SDL_Texture *t;
	int          i;

	memset( &k, 0, sizeof k );
	k.kind = GFX_KIND_ART;
	k.a = id;
	k.w = d.w;
	k.h = d.h;
	for ( i = 0; i < VG_NTOKENS; i++ )
	{
		k.t[i] = tint && ( mask & ( 1u << i ) ) ? (int32_t)tint->tok[i] : 0;
	}
	t = gfx_lookup( g, &k, id, tint );
	if ( !t )
	{
		return;
	}
	SDL_SetTextureColorMod( t, 255, 255, 255 );
	SDL_SetTextureAlphaMod( t, (Uint8)( alpha * 255.0f + 0.5f ) );
	SDL_RenderCopy( g->ren, t, NULL, &d );
}

void gfx_art_fit( gfx *g, int id, float x, float y, float w, float h,
                  const vg_tint *tint, float alpha )
{
	float aw, ah, s, dw, dh;

	if ( vg_asset_size( id, &aw, &ah ) != 0 )
	{
		return;
	}
	s = w / aw < h / ah ? w / aw : h / ah;
	dw = aw * s;
	dh = ah * s;
	gfx_art( g, id, x + ( w - dw ) * 0.5f, y + ( h - dh ) * 0.5f, dw, dh,
	         tint, alpha );
}

void gfx_clip( gfx *g, float x, float y, float w, float h )
{
	SDL_Rect r = gfx_rect( g, x, y, w, h );

	SDL_RenderSetClipRect( g->ren, &r );
}

void gfx_unclip( gfx *g )
{
	SDL_RenderSetClipRect( g->ren, NULL );
}

void gfx_tick( gfx *g, float x, float y, float w, float h, uint32_t color,
               uint32_t outline )
{
	SDL_Rect     d = gfx_rect( g, x, y, w, h );
	gfx_key      k;
	SDL_Texture *t;
	uint32_t     c;
	int          pass;

	for ( pass = 0; pass < 2; pass++ )
	{
		memset( &k, 0, sizeof k );
		k.kind = GFX_KIND_TICK;
		k.w = d.w;
		k.h = d.h;
		k.b = (int32_t)( ( pass == 0 ? 0.30f : 0.17f ) * (float)d.w * 16.0f );
		t = gfx_lookup( g, &k, 0, NULL );
		if ( !t )
		{
			return;
		}
		c = pass ? color : outline;
		SDL_SetTextureColorMod( t, (Uint8)( ( c >> 16 ) & 255 ),
		                        (Uint8)( ( c >> 8 ) & 255 ), (Uint8)( c & 255 ) );
		SDL_SetTextureAlphaMod( t, 255 );
		SDL_RenderCopy( g->ren, t, NULL, &d );
	}
}
