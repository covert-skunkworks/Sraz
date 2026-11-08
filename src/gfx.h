//  Description:
//
//		gfx.h -- SDL drawing layer: scaled canvas, art texture cache, text.
//
//		All coordinates are canvas units of a fixed 960 x 540 design canvas.
//		The canvas is scaled uniformly to the window and centred, so every
//		screen keeps its proportions; the bars around it are cleared to the
//		background colour. Vector art and rounded shapes are rasterised by
//		vg.c at the exact pixel size they are drawn at and cached as textures
//		keyed by (kind, size, tint); the cache is flushed after a resize.
//		Text is rendered with stb_truetype from the embedded font into a cache
//		of single-glyph textures.
//
#ifndef GFX_H
#define GFX_H

#include "vg.h"
#include <SDL.h>
#include <stdint.h>

#define GFX_CW 960.0f
#define GFX_CH 540.0f

#define GFX_LEFT 0
#define GFX_CENTRE 1
#define GFX_RIGHT 2

typedef struct
{
	int32_t kind, w, h, a, b, t[VG_NTOKENS];
} gfx_key;

typedef struct
{
	gfx_key      key;
	SDL_Texture *tex;
	int          used;
} gfx_entry;

typedef struct
{
	int          cp, px, used;
	SDL_Texture *tex;
	int          w, h, xoff, yoff;
	float        adv;
} gfx_glyph;

typedef struct
{
	SDL_Window   *win;
	SDL_Renderer *ren;
	int           pw, ph;       // Output size in pixels.
	float         scale;        // Pixels per canvas unit.
	float         ox, oy;       // Canvas origin in pixels.
	gfx_entry    *tab;
	int           tab_cap, tab_count;
	size_t        tab_bytes;
	gfx_glyph    *gtab;
	int           gcap, gcount;
	void         *font;         // stbtt_fontinfo, owned by text.c.
	int           flush_pending;
	uint32_t      flush_at;
	int           rasterised;   // Textures built so far, for tests.
} gfx;

/**
  * @brief  Prepare the drawing layer for a window and renderer.
  * @param  g Receives the state.
  * @param  win Window.
  * @param  ren Renderer of that window.
  * @retval 0 on success, -1 on failure.
  */
int gfx_init( gfx *g, SDL_Window *win, SDL_Renderer *ren );

/**
  * @brief  Release all textures and caches.
  * @param  g State from gfx_init.
  * @retval None.
  */
void gfx_free( gfx *g );

/**
  * @brief  Recompute the canvas scale from the renderer's output size.
  * @param  g State.
  * @retval None.
  */
void gfx_metrics( gfx *g );

/**
  * @brief  Ask for the caches to be flushed once resizing has settled.
  * @param  g State.
  * @param  now Current time in milliseconds.
  * @retval None.
  */
void gfx_request_flush( gfx *g, uint32_t now );

/**
  * @brief  Start a frame: apply a pending flush and clear to a colour.
  * @param  g State.
  * @param  bg Background colour 0xRRGGBB.
  * @param  now Current time in milliseconds.
  * @retval None.
  */
void gfx_begin( gfx *g, uint32_t bg, uint32_t now );

/**
  * @brief  Convert window pixels to canvas units.
  * @param  g State.
  * @param  px Pixel X in output pixels.
  * @param  py Pixel Y in output pixels.
  * @param  x Receives canvas X.
  * @param  y Receives canvas Y.
  * @retval None.
  */
void gfx_to_canvas( const gfx *g, int px, int py, float *x, float *y );

/**
  * @brief  Convert a canvas rectangle to a pixel rectangle on shared edges.
  * @param  g State.
  * @param  x Left.
  * @param  y Top.
  * @param  w Width.
  * @param  h Height.
  * @retval Pixel rectangle at least one pixel wide and tall.
  */
SDL_Rect gfx_rect( const gfx *g, float x, float y, float w, float h );

/**
  * @brief  Fill a rectangle.
  * @param  g State.
  * @param  x Left.
  * @param  y Top.
  * @param  w Width.
  * @param  h Height.
  * @param  color 0xRRGGBB.
  * @param  alpha Opacity 0..1.
  * @retval None.
  */
void gfx_fill( gfx *g, float x, float y, float w, float h, uint32_t color,
               float alpha );

/**
  * @brief  Fill an anti-aliased rounded rectangle.
  * @param  g State.
  * @param  x Left.
  * @param  y Top.
  * @param  w Width.
  * @param  h Height.
  * @param  r Corner radius in canvas units.
  * @param  color 0xRRGGBB.
  * @param  alpha Opacity 0..1.
  * @retval None.
  */
void gfx_rrect( gfx *g, float x, float y, float w, float h, float r,
                uint32_t color, float alpha );

/**
  * @brief  Draw an anti-aliased rounded outline.
  * @param  g State.
  * @param  x Left.
  * @param  y Top.
  * @param  w Width.
  * @param  h Height.
  * @param  r Corner radius in canvas units.
  * @param  sw Outline width in canvas units.
  * @param  color 0xRRGGBB.
  * @param  alpha Opacity 0..1.
  * @retval None.
  */
void gfx_ring( gfx *g, float x, float y, float w, float h, float r, float sw,
               uint32_t color, float alpha );

/**
  * @brief  Draw an art asset stretched to a rectangle.
  * @param  g State.
  * @param  id Asset (ART_xxx).
  * @param  x Left.
  * @param  y Top.
  * @param  w Width.
  * @param  h Height.
  * @param  tint Colour tokens, may be NULL.
  * @param  alpha Opacity 0..1.
  * @retval None.
  */
void gfx_art( gfx *g, int id, float x, float y, float w, float h,
              const vg_tint *tint, float alpha );

/**
  * @brief  Draw an art asset scaled uniformly to fit and centred in a box.
  * @param  g State.
  * @param  id Asset (ART_xxx).
  * @param  x Box left.
  * @param  y Box top.
  * @param  w Box width.
  * @param  h Box height.
  * @param  tint Colour tokens, may be NULL.
  * @param  alpha Opacity 0..1.
  * @retval None.
  */
void gfx_art_fit( gfx *g, int id, float x, float y, float w, float h,
                  const vg_tint *tint, float alpha );

/**
  * @brief  Draw a check mark with an outline.
  * @param  g State.
  * @param  x Left.
  * @param  y Top.
  * @param  w Width.
  * @param  h Height.
  * @param  color Mark colour 0xRRGGBB.
  * @param  outline Outline colour 0xRRGGBB.
  * @retval None.
  */
void gfx_tick( gfx *g, float x, float y, float w, float h, uint32_t color,
               uint32_t outline );

/**
  * @brief  Restrict drawing to a canvas rectangle.
  * @param  g State.
  * @param  x Left.
  * @param  y Top.
  * @param  w Width.
  * @param  h Height.
  * @retval None.
  */
void gfx_clip( gfx *g, float x, float y, float w, float h );

/**
  * @brief  Remove the clip rectangle.
  * @param  g State.
  * @retval None.
  */
void gfx_unclip( gfx *g );

/**
  * @brief  Load the embedded font.
  * @param  g State.
  * @retval 0 on success, -1 on failure.
  */
int gfx_text_init( gfx *g );

/**
  * @brief  Measure a line of text.
  * @param  g State.
  * @param  size Font size in canvas units.
  * @param  str UTF-8 text without newlines.
  * @retval Width in canvas units.
  */
float gfx_text_width( gfx *g, float size, const char *str );

/**
  * @brief  Height of one text line.
  * @param  g State.
  * @param  size Font size in canvas units.
  * @retval Line pitch in canvas units.
  */
float gfx_line_height( gfx *g, float size );

/**
  * @brief  Draw one line of text.
  * @param  g State.
  * @param  size Font size in canvas units.
  * @param  x Anchor X (left, centre or right edge by align).
  * @param  y Top of the line.
  * @param  str UTF-8 text.
  * @param  color 0xRRGGBB.
  * @param  align GFX_LEFT, GFX_CENTRE or GFX_RIGHT.
  * @param  alpha Opacity 0..1.
  * @retval Width drawn in canvas units.
  */
float gfx_text( gfx *g, float size, float x, float y, const char *str,
                uint32_t color, int align, float alpha );

/**
  * @brief  Draw or measure a word-wrapped paragraph.
  * @param  g State.
  * @param  size Font size in canvas units.
  * @param  x Left edge of the column.
  * @param  y Top of the first line.
  * @param  w Column width.
  * @param  str UTF-8 text, newlines start new lines.
  * @param  color 0xRRGGBB.
  * @param  align GFX_LEFT, GFX_CENTRE or GFX_RIGHT within the column.
  * @param  alpha Opacity 0..1.
  * @param  draw Zero measures only.
  * @retval Height of the paragraph in canvas units.
  */
float gfx_text_wrap( gfx *g, float size, float x, float y, float w,
                     const char *str, uint32_t color, int align, float alpha,
                     int draw );

/**
  * @brief  Draw a paragraph shrunk until it fits a box.
  * @param  g State.
  * @param  max_size Largest font size to try.
  * @param  min_size Smallest font size to try.
  * @param  x Left edge.
  * @param  y Top edge.
  * @param  w Width.
  * @param  h Height available.
  * @param  str UTF-8 text.
  * @param  color 0xRRGGBB.
  * @param  align GFX_LEFT, GFX_CENTRE or GFX_RIGHT.
  * @retval Font size used.
  */
float gfx_text_fit( gfx *g, float max_size, float min_size, float x, float y,
                    float w, float h, const char *str, uint32_t color,
                    int align );

/**
  * @brief  Destroy every cached glyph texture.
  * @param  g State.
  * @retval None.
  */
void gfx_glyph_flush( gfx *g );

/**
  * @brief  Set the draw colour from 0xRRGGBB and an opacity.
  * @param  g State.
  * @param  color 0xRRGGBB.
  * @param  alpha Opacity 0..1.
  * @retval None.
  */
void gfx_color( gfx *g, uint32_t color, float alpha );

#endif
