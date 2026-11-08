//  Description:
//
//		vg.h -- small software vector rasteriser for the Sraz art set.
//
//		Renders the flat path data of art/art_data.h into an RGBA buffer at
//		any pixel size: flattened beziers, anti-aliased non-zero or even-odd
//		fills (exact horizontal coverage, VG_SS sub-scanlines vertically),
//		round-join strokes, linear gradients and layer opacity. It knows
//		nothing about SDL, so the unit tests can use it directly.
//
#ifndef VG_H
#define VG_H

#include <stdint.h>

#define VG_SS 8  // Vertical sub-scanlines per pixel row.
#define VG_NTOKENS 5

// Colour tokens of the art set: player main, dark, light, category, dark.
typedef struct
{
	uint32_t tok[VG_NTOKENS];  // 0xRRGGBB, order $p $pd $pl $c $cd.
} vg_tint;

// A polygon set: contours of points, flattened to straight segments.
typedef struct
{
	float         *pt;      // x, y pairs.
	int            npt, cap;
	int           *start;   // First point of each contour.
	unsigned char *closed;  // Contour ended with a close command.
	int            nc, ccap;
} vg_poly;

/**
  * @brief  Initialise an empty polygon set.
  * @param  p Polygon set.
  * @retval None.
  */
void vg_poly_init( vg_poly *p );

/**
  * @brief  Release a polygon set.
  * @param  p Polygon set, may hold nothing.
  * @retval None.
  */
void vg_poly_free( vg_poly *p );

/**
  * @brief  Remove all contours but keep the allocations.
  * @param  p Polygon set.
  * @retval None.
  */
void vg_poly_clear( vg_poly *p );

/**
  * @brief  Start a new contour.
  * @param  p Polygon set.
  * @param  x X coordinate.
  * @param  y Y coordinate.
  * @retval None.
  */
void vg_move( vg_poly *p, float x, float y );

/**
  * @brief  Append a straight segment to the current contour.
  * @param  p Polygon set.
  * @param  x X coordinate.
  * @param  y Y coordinate.
  * @retval None.
  */
void vg_line( vg_poly *p, float x, float y );

/**
  * @brief  Append a cubic bezier, flattened to line segments.
  * @param  p Polygon set.
  * @param  x1 First control point X.
  * @param  y1 First control point Y.
  * @param  x2 Second control point X.
  * @param  y2 Second control point Y.
  * @param  x End point X.
  * @param  y End point Y.
  * @retval None.
  */
void vg_cubic( vg_poly *p, float x1, float y1, float x2, float y2, float x,
               float y );

/**
  * @brief  Mark the current contour closed.
  * @param  p Polygon set.
  * @retval None.
  */
void vg_close( vg_poly *p );

/**
  * @brief  Append a rounded rectangle contour.
  * @param  p Polygon set.
  * @param  x Left edge.
  * @param  y Top edge.
  * @param  w Width.
  * @param  h Height.
  * @param  r Corner radius, clamped to half the shorter side.
  * @retval None.
  */
void vg_rrect( vg_poly *p, float x, float y, float w, float h, float r );

/**
  * @brief  Turn the outlines of a polygon set into fillable polygons.
  *
  * Every segment becomes a quad and every vertex a disc, all wound the same
  * way, so one non-zero fill gives a stroke with round joins and caps.
  *
  * @param  src Source outlines.
  * @param  width Stroke width in pixels.
  * @param  dst Receives the stroke polygons, appended.
  * @retval None.
  */
void vg_stroke( const vg_poly *src, float width, vg_poly *dst );

/**
  * @brief  Compute anti-aliased coverage of a polygon set.
  * @param  p Polygons; contours are implicitly closed.
  * @param  w Buffer width in pixels.
  * @param  h Buffer height in pixels.
  * @param  evenodd Nonzero selects the even-odd rule, zero non-zero winding.
  * @param  cov Receives w * h coverage values 0..1 (cleared first).
  * @retval None.
  */
void vg_cover( const vg_poly *p, int w, int h, int evenodd, float *cov );

/**
  * @brief  Find the size of an art asset.
  * @param  id Asset index (ART_xxx).
  * @param  w Receives the width in asset units, may be NULL.
  * @param  h Receives the height in asset units, may be NULL.
  * @retval 0 on success, -1 for an unknown asset.
  */
int vg_asset_size( int id, float *w, float *h );

/**
  * @brief  Report which colour tokens an asset uses.
  * @param  id Asset index.
  * @retval Bit mask, bit i set when token i is used; 0 for unknown assets.
  */
uint32_t vg_asset_tokens( int id );

/**
  * @brief  Rasterise an asset, stretched to a pixel box.
  * @param  id Asset index (ART_xxx).
  * @param  w Width in pixels, at least 1.
  * @param  h Height in pixels, at least 1.
  * @param  tint Colour tokens, may be NULL for an all-black tint.
  * @param  rgba Receives w * h * 4 bytes, straight (non-premultiplied) alpha.
  * @retval 0 on success, -1 bad arguments, -2 out of memory.
  */
int vg_render_asset( int id, int w, int h, const vg_tint *tint,
                     uint8_t *rgba );

/**
  * @brief  Rasterise a white rounded rectangle or ring as an alpha mask.
  * @param  w Width in pixels.
  * @param  h Height in pixels.
  * @param  radius Corner radius in pixels.
  * @param  ring Outline width in pixels, 0 for a filled shape.
  * @param  rgba Receives w * h * 4 bytes, white with coverage as alpha.
  * @retval 0 on success, -1 bad arguments, -2 out of memory.
  */
int vg_render_shape( int w, int h, float radius, float ring, uint8_t *rgba );

/**
  * @brief  Rasterise a white check mark as an alpha mask.
  * @param  w Width in pixels.
  * @param  h Height in pixels.
  * @param  stroke Stroke width in pixels.
  * @param  rgba Receives w * h * 4 bytes, white with coverage as alpha.
  * @retval 0 on success, -1 bad arguments, -2 out of memory.
  */
int vg_render_tick( int w, int h, float stroke, uint8_t *rgba );

/**
  * @brief  Build the tint of a player and a category.
  * @param  player art_player_t value, or -1 for none.
  * @param  cat Category 1..9, or 0 for none.
  * @param  out Receives the five tokens.
  * @retval None.
  */
void vg_tint_make( int player, int cat, vg_tint *out );

/**
  * @brief  Look up a player colour.
  * @param  player art_player_t value.
  * @param  shade 0 main, 1 dark, 2 light.
  * @retval 0xRRGGBB.
  */
uint32_t vg_player_color( int player, int shade );

/**
  * @brief  Look up a category colour.
  * @param  cat Category 1..9.
  * @param  shade 0 main, 1 dark, 2 pictogram glyph colour.
  * @retval 0xRRGGBB.
  */
uint32_t vg_category_color( int cat, int shade );

/**
  * @brief  Look up the player behind a seat in an original colour set.
  * @param  set Colour set 0..3.
  * @param  seat Player seat 0..3.
  * @retval art_player_t value.
  */
int vg_army_player( int set, int seat );

/**
  * @brief  Look up a category name.
  * @param  hr Nonzero for Croatian, zero for English.
  * @param  cat Category 1..9.
  * @retval UTF-8 name.
  */
const char *vg_category_name( int hr, int cat );

/**
  * @brief  Look up a user interface colour.
  * @param  dark Nonzero for the dark theme.
  * @param  idx art_ui_t value.
  * @retval 0xRRGGBB.
  */
uint32_t vg_ui_color( int dark, int idx );

#endif
