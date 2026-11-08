//  Description:
//
//		vg.c -- small software vector rasteriser for the Sraz art set.
//
//		Coverage is computed per sub-scanline: every polygon edge deposits
//		a signed crossing on each sub-scanline it spans, the crossings are
//		sorted and walked with the fill rule, and each inside span adds its
//		exact fractional width to the pixel row. That is exact horizontally
//		and VG_SS-level vertically, which is plenty for artwork that is
//		rasterised once per size and then cached as a texture.
//
//		A stroke is built as a union of quads (one per segment) and discs
//		(one per joint), every piece wound the same way, so a single
//		non-zero fill gives round joins and caps without any miter logic.
//

//------------------------------------------------------------------------------
//---------------------------------- Includes ----------------------------------
//------------------------------------------------------------------------------

#include "vg.h"
#include "../art/art_data.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

//------------------------------------------------------------------------------
//------------------------- Local macros and constants -------------------------
//------------------------------------------------------------------------------

#define VG_KAPPA 0.5522847f
#define VG_PI 3.14159265f

//------------------------------------------------------------------------------
//------------------------ Local function prototypes ---------------------------
//------------------------------------------------------------------------------

static int  vg_grow_pt( vg_poly *p, int extra );
static void vg_disc( vg_poly *p, float cx, float cy, float r );
static void vg_span( float *row, int w, float xa, float xb, float wt );

//------------------------------------------------------------------------------
//------------------------------- Local functions ------------------------------
//------------------------------------------------------------------------------

/**
  * @brief  Make room for more points.
  * @param  p Polygon set.
  * @param  extra Number of additional points.
  * @retval 0 on success, -1 on allocation failure.
  */
static int vg_grow_pt( vg_poly *p, int extra )
{
	float *n;
	int    cap;

	if ( p->npt + extra <= p->cap )
	{
		return 0;
	}
	cap = p->cap ? p->cap * 2 : 256;
	while ( cap < p->npt + extra )
	{
		cap *= 2;
	}
	n = (float *)realloc( p->pt, (size_t)cap * 2 * sizeof *n );
	if ( !n )
	{
		return -1;
	}
	p->pt = n;
	p->cap = cap;
	return 0;
}

/**
  * @brief  Append a filled disc as a positively wound contour.
  * @param  p Polygon set.
  * @param  cx Centre X.
  * @param  cy Centre Y.
  * @param  r Radius in pixels.
  * @retval None.
  */
static void vg_disc( vg_poly *p, float cx, float cy, float r )
{
	int   n, i;
	float a;

	n = (int)( r * 2.5f ) + 8;
	n = n > 40 ? 40 : n;
	for ( i = 0; i < n; i++ )
	{
		a = 2.0f * VG_PI * (float)i / (float)n;
		if ( i == 0 )
		{
			vg_move( p, cx + r, cy );
		}
		else
		{
			vg_line( p, cx + r * cosf( a ), cy + r * sinf( a ) );
		}
	}
	vg_close( p );
}

/**
  * @brief  Add the coverage of a horizontal span to a pixel row.
  * @param  row Row of w floats.
  * @param  w Row width.
  * @param  xa Span start.
  * @param  xb Span end.
  * @param  wt Weight of this sub-scanline.
  * @retval None.
  */
static void vg_span( float *row, int w, float xa, float xb, float wt )
{
	int ia, ib, i;

	xa = xa < 0.0f ? 0.0f : xa;
	xb = xb > (float)w ? (float)w : xb;
	if ( xb <= xa )
	{
		return;
	}
	ia = (int)xa;
	ib = (int)xb;
	if ( ia == ib )
	{
		row[ia] += ( xb - xa ) * wt;
		return;
	}
	row[ia] += ( (float)( ia + 1 ) - xa ) * wt;
	for ( i = ia + 1; i < ib; i++ )
	{
		row[i] += wt;
	}
	if ( ib < w )
	{
		row[ib] += ( xb - (float)ib ) * wt;
	}
}

//------------------------------------------------------------------------------
//--------------------------- Polygon set functions ----------------------------
//------------------------------------------------------------------------------

void vg_poly_init( vg_poly *p )
{
	memset( p, 0, sizeof *p );
}

void vg_poly_free( vg_poly *p )
{
	free( p->pt );
	free( p->start );
	free( p->closed );
	memset( p, 0, sizeof *p );
}

void vg_poly_clear( vg_poly *p )
{
	p->npt = 0;
	p->nc = 0;
}

void vg_move( vg_poly *p, float x, float y )
{
	int           cap;
	int          *s;
	unsigned char *c;

	if ( p->nc >= p->ccap )
	{
		cap = p->ccap ? p->ccap * 2 : 16;
		s = (int *)realloc( p->start, (size_t)cap * sizeof *s );
		if ( !s )
		{
			return;
		}
		p->start = s;
		c = (unsigned char *)realloc( p->closed, (size_t)cap );
		if ( !c )
		{
			return;
		}
		p->closed = c;
		p->ccap = cap;
	}
	if ( vg_grow_pt( p, 1 ) != 0 )
	{
		return;
	}
	p->start[p->nc] = p->npt;
	p->closed[p->nc] = 0;
	p->nc++;
	p->pt[p->npt * 2] = x;
	p->pt[p->npt * 2 + 1] = y;
	p->npt++;
}

void vg_line( vg_poly *p, float x, float y )
{
	if ( p->nc == 0 || p->closed[p->nc - 1] )
	{
		vg_move( p, x, y );
		return;
	}
	if ( vg_grow_pt( p, 1 ) != 0 )
	{
		return;
	}
	p->pt[p->npt * 2] = x;
	p->pt[p->npt * 2 + 1] = y;
	p->npt++;
}

void vg_cubic( vg_poly *p, float x1, float y1, float x2, float y2, float x,
               float y )
{
	float x0, y0, len, t, u, a, b, c, d;
	int   n, i;

	if ( p->nc == 0 || p->npt == 0 )
	{
		vg_move( p, x, y );
		return;
	}
	x0 = p->pt[p->npt * 2 - 2];
	y0 = p->pt[p->npt * 2 - 1];
	len = hypotf( x1 - x0, y1 - y0 ) + hypotf( x2 - x1, y2 - y1 ) +
	      hypotf( x - x2, y - y2 );
	n = 2 + (int)( sqrtf( len ) * 1.1f );
	n = n > 64 ? 64 : n;
	for ( i = 1; i <= n; i++ )
	{
		t = (float)i / (float)n;
		u = 1.0f - t;
		a = u * u * u;
		b = 3.0f * u * u * t;
		c = 3.0f * u * t * t;
		d = t * t * t;
		vg_line( p, a * x0 + b * x1 + c * x2 + d * x,
		         a * y0 + b * y1 + c * y2 + d * y );
	}
}

void vg_close( vg_poly *p )
{
	if ( p->nc > 0 )
	{
		p->closed[p->nc - 1] = 1;
	}
}

void vg_rrect( vg_poly *p, float x, float y, float w, float h, float r )
{
	float k, m = w < h ? w : h;

	r = r > m * 0.5f ? m * 0.5f : r;
	r = r < 0.0f ? 0.0f : r;
	k = r * VG_KAPPA;
	vg_move( p, x + r, y );
	vg_line( p, x + w - r, y );
	vg_cubic( p, x + w - r + k, y, x + w, y + r - k, x + w, y + r );
	vg_line( p, x + w, y + h - r );
	vg_cubic( p, x + w, y + h - r + k, x + w - r + k, y + h, x + w - r,
	          y + h );
	vg_line( p, x + r, y + h );
	vg_cubic( p, x + r - k, y + h, x, y + h - r + k, x, y + h - r );
	vg_line( p, x, y + r );
	vg_cubic( p, x, y + r - k, x + r - k, y, x + r, y );
	vg_close( p );
}

void vg_stroke( const vg_poly *src, float width, vg_poly *dst )
{
	float hw = width * 0.5f, ax, ay, bx, by, dx, dy, len, nx, ny, q[8], area;
	float pdx, pdy, cs;
	int   c, s, e, i, n, j, k, closed, have;

	for ( c = 0; c < src->nc; c++ )
	{
		s = src->start[c];
		e = c + 1 < src->nc ? src->start[c + 1] : src->npt;
		n = e - s;
		closed = src->closed[c] && n > 2;
		have = 0;
		for ( i = 0; i < n; i++ )
		{
			j = s + i;
			k = i + 1 < n ? s + i + 1 : ( closed ? s : -1 );
			if ( k < 0 )
			{
				break;
			}
			ax = src->pt[j * 2];
			ay = src->pt[j * 2 + 1];
			bx = src->pt[k * 2];
			by = src->pt[k * 2 + 1];
			dx = bx - ax;
			dy = by - ay;
			len = hypotf( dx, dy );
			if ( len < 1e-5f )
			{
				continue;
			}
			have = 1;
			nx = -dy / len * hw;
			ny = dx / len * hw;
			q[0] = ax + nx; q[1] = ay + ny; q[2] = bx + nx; q[3] = by + ny;
			q[4] = bx - nx; q[5] = by - ny; q[6] = ax - nx; q[7] = ay - ny;
			area = 0.0f;
			for ( k = 0; k < 4; k++ )
			{
				area += q[k * 2] * q[( ( k + 1 ) & 3 ) * 2 + 1] -
				        q[( ( k + 1 ) & 3 ) * 2] * q[k * 2 + 1];
			}
			if ( area >= 0.0f )
			{
				vg_move( dst, q[0], q[1] );
				vg_line( dst, q[2], q[3] );
				vg_line( dst, q[4], q[5] );
				vg_line( dst, q[6], q[7] );
			}
			else
			{
				vg_move( dst, q[6], q[7] );
				vg_line( dst, q[4], q[5] );
				vg_line( dst, q[2], q[3] );
				vg_line( dst, q[0], q[1] );
			}
			vg_close( dst );
		}
		if ( !have )
		{
			vg_disc( dst, src->pt[s * 2], src->pt[s * 2 + 1], hw );
			continue;
		}
		// Discs make the joins and caps; almost straight joints need none.
		for ( i = 0; i < n; i++ )
		{
			j = s + i;
			if ( !closed && ( i == 0 || i == n - 1 ) )
			{
				vg_disc( dst, src->pt[j * 2], src->pt[j * 2 + 1], hw );
				continue;
			}
			k = i == 0 ? e - 1 : j - 1;
			pdx = src->pt[j * 2] - src->pt[k * 2];
			pdy = src->pt[j * 2 + 1] - src->pt[k * 2 + 1];
			k = i == n - 1 ? s : j + 1;
			dx = src->pt[k * 2] - src->pt[j * 2];
			dy = src->pt[k * 2 + 1] - src->pt[j * 2 + 1];
			len = hypotf( pdx, pdy ) * hypotf( dx, dy );
			cs = len > 1e-9f ? ( pdx * dx + pdy * dy ) / len : -1.0f;
			if ( cs < 0.995f || hw > 4.0f )
			{
				vg_disc( dst, src->pt[j * 2], src->pt[j * 2 + 1], hw );
			}
		}
	}
}

void vg_cover( const vg_poly *p, int w, int h, int evenodd, float *cov )
{
	int           nsub = h * VG_SS, *cnt, *fill, pass, c, s, e, i, a, b, j, j0,
	              j1, total, n, k, m, wind;
	float        *xs, y0, y1, ys, xa = 0.0f, tx, wt = 1.0f / VG_SS;
	signed char  *dr, td;

	memset( cov, 0, (size_t)w * (size_t)h * sizeof *cov );
	if ( p->npt < 3 || w < 1 || h < 1 )
	{
		return;
	}
	cnt = (int *)calloc( (size_t)nsub + 1, sizeof *cnt );
	fill = (int *)calloc( (size_t)nsub + 1, sizeof *fill );
	if ( !cnt || !fill )
	{
		free( cnt );
		free( fill );
		return;
	}
	xs = NULL;
	dr = NULL;
	for ( pass = 0; pass < 2; pass++ )
	{
		for ( c = 0; c < p->nc; c++ )
		{
			s = p->start[c];
			e = c + 1 < p->nc ? p->start[c + 1] : p->npt;
			for ( i = s; i < e; i++ )
			{
				a = i;
				b = i + 1 < e ? i + 1 : s;
				y0 = p->pt[a * 2 + 1];
				y1 = p->pt[b * 2 + 1];
				if ( y0 == y1 )
				{
					continue;
				}
				td = y1 > y0 ? 1 : -1;
				if ( y1 < y0 )
				{
					ys = y0; y0 = y1; y1 = ys;
				}
				// Sub-scanline j samples y = ( j + 0.5 ) / VG_SS.
				j0 = (int)ceilf( y0 * VG_SS - 0.5f );
				j1 = (int)ceilf( y1 * VG_SS - 0.5f ) - 1;
				j0 = j0 < 0 ? 0 : j0;
				j1 = j1 >= nsub ? nsub - 1 : j1;
				for ( j = j0; j <= j1; j++ )
				{
					if ( pass == 0 )
					{
						cnt[j + 1]++;
					}
					else
					{
						ys = ( (float)j + 0.5f ) / VG_SS;
						tx = p->pt[a * 2] +
						     ( ys - p->pt[a * 2 + 1] ) *
						         ( p->pt[b * 2] - p->pt[a * 2] ) /
						         ( p->pt[b * 2 + 1] - p->pt[a * 2 + 1] );
						k = cnt[j] + fill[j]++;
						xs[k] = tx;
						dr[k] = td;
					}
				}
			}
		}
		if ( pass == 0 )
		{
			for ( j = 0; j < nsub; j++ )
			{
				cnt[j + 1] += cnt[j];
			}
			total = cnt[nsub];
			xs = (float *)malloc( ( (size_t)total + 1 ) * sizeof *xs );
			dr = (signed char *)malloc( (size_t)total + 1 );
			if ( !xs || !dr )
			{
				free( xs );
				free( dr );
				free( cnt );
				free( fill );
				return;
			}
		}
	}
	for ( j = 0; j < nsub; j++ )
	{
		n = cnt[j + 1] - cnt[j];
		k = cnt[j];
		for ( i = 1; i < n; i++ )
		{
			tx = xs[k + i];
			td = dr[k + i];
			m = i - 1;
			while ( m >= 0 && xs[k + m] > tx )
			{
				xs[k + m + 1] = xs[k + m];
				dr[k + m + 1] = dr[k + m];
				m--;
			}
			xs[k + m + 1] = tx;
			dr[k + m + 1] = td;
		}
		wind = 0;
		for ( i = 0; i < n; i++ )
		{
			int before = wind != 0;

			wind = evenodd ? ( wind ^ 1 ) : wind + dr[k + i];
			if ( !before && wind != 0 )
			{
				xa = xs[k + i];
			}
			else if ( before && wind == 0 )
			{
				vg_span( cov + (size_t)( j / VG_SS ) * (size_t)w, w, xa,
				         xs[k + i], wt );
			}
		}
	}
	for ( i = 0; i < w * h; i++ )
	{
		cov[i] = cov[i] > 1.0f ? 1.0f : cov[i];
	}
	free( xs );
	free( dr );
	free( cnt );
	free( fill );
}

//------------------------------------------------------------------------------
//------------------------------- Asset rendering ------------------------------
//------------------------------------------------------------------------------

/**
  * @brief  Resolve a paint (fixed colour or tint token) to 0xRRGGBB.
  * @param  kind ART_PAINT_xxx.
  * @param  value Colour or token index.
  * @param  tint Tint in effect, may be NULL.
  * @retval 0xRRGGBB.
  */
static uint32_t vg_paint( int kind, uint32_t value, const vg_tint *tint )
{
	if ( kind == ART_PAINT_TOKEN )
	{
		return tint && value < VG_NTOKENS ? tint->tok[value] : 0u;
	}
	return value;
}

/**
  * @brief  Convert an asset path stream to flattened polygons.
  * @param  p First element of the stream.
  * @param  e One past the last element.
  * @param  sx Horizontal scale.
  * @param  sy Vertical scale.
  * @param  out Receives the contours.
  * @retval None.
  */
static void vg_build_path( const float *p, const float *e, float sx, float sy,
                           vg_poly *out )
{
	while ( p < e )
	{
		switch ( (int)*p++ )
		{
			case ART_PATH_M:
				vg_move( out, p[0] * sx, p[1] * sy );
				p += 2;
				break;
			case ART_PATH_L:
				vg_line( out, p[0] * sx, p[1] * sy );
				p += 2;
				break;
			case ART_PATH_C:
				vg_cubic( out, p[0] * sx, p[1] * sy, p[2] * sx, p[3] * sy,
				          p[4] * sx, p[5] * sy );
				p += 6;
				break;
			default:
				vg_close( out );
				break;
		}
	}
}

/**
  * @brief  Evaluate a gradient at a point of the asset.
  * @param  g Gradient.
  * @param  tint Tint in effect.
  * @param  x Asset-space X.
  * @param  y Asset-space Y.
  * @param  rgb Receives red, green, blue in 0..1.
  * @retval None.
  */
static void vg_grad_at( const art_grad_t *g, const vg_tint *tint, float x,
                        float y, float rgb[3] )
{
	float dx = g->x2 - g->x1, dy = g->y2 - g->y1, d = dx * dx + dy * dy, t, f;
	const art_stop_t *s = &art_stops[g->stop_first];
	uint32_t c0, c1;
	int i, n = g->stop_count;

	t = d > 0.0f ? ( ( x - g->x1 ) * dx + ( y - g->y1 ) * dy ) / d : 0.0f;
	t = t < 0.0f ? 0.0f : ( t > 1.0f ? 1.0f : t );
	if ( t <= s[0].t )
	{
		c0 = c1 = vg_paint( s[0].kind, s[0].value, tint );
		f = 0.0f;
	}
	else
	{
		c0 = c1 = vg_paint( s[n - 1].kind, s[n - 1].value, tint );
		f = 0.0f;
		for ( i = 1; i < n; i++ )
		{
			if ( t <= s[i].t )
			{
				c0 = vg_paint( s[i - 1].kind, s[i - 1].value, tint );
				c1 = vg_paint( s[i].kind, s[i].value, tint );
				f = s[i].t > s[i - 1].t
				        ? ( t - s[i - 1].t ) / ( s[i].t - s[i - 1].t )
				        : 0.0f;
				break;
			}
		}
	}
	rgb[0] = ( (float)( ( c0 >> 16 ) & 255 ) * ( 1.0f - f ) +
	           (float)( ( c1 >> 16 ) & 255 ) * f ) / 255.0f;
	rgb[1] = ( (float)( ( c0 >> 8 ) & 255 ) * ( 1.0f - f ) +
	           (float)( ( c1 >> 8 ) & 255 ) * f ) / 255.0f;
	rgb[2] = ( (float)( c0 & 255 ) * ( 1.0f - f ) +
	           (float)( c1 & 255 ) * f ) / 255.0f;
}

/**
  * @brief  Composite a coverage mask with a layer's paint onto the buffer.
  * @param  buf Premultiplied float RGBA buffer.
  * @param  cov Coverage mask, w * h.
  * @param  w Width.
  * @param  h Height.
  * @param  rgb Solid colour, used when grad is NULL.
  * @param  grad Gradient or NULL.
  * @param  tint Tint for gradient stops.
  * @param  op Opacity.
  * @param  sx Horizontal scale of the asset.
  * @param  sy Vertical scale of the asset.
  * @retval None.
  */
static void vg_composite( float *buf, const float *cov, int w, int h,
                          const float rgb[3], const art_grad_t *grad,
                          const vg_tint *tint, float op, float sx, float sy )
{
	float c[3], a, *d;
	int   x, y, i;

	for ( y = 0; y < h; y++ )
	{
		for ( x = 0; x < w; x++ )
		{
			a = cov[y * w + x] * op;
			if ( a <= 0.0004f )
			{
				continue;
			}
			if ( grad )
			{
				vg_grad_at( grad, tint, ( (float)x + 0.5f ) / sx,
				            ( (float)y + 0.5f ) / sy, c );
			}
			else
			{
				c[0] = rgb[0]; c[1] = rgb[1]; c[2] = rgb[2];
			}
			d = buf + ( (size_t)y * (size_t)w + (size_t)x ) * 4;
			for ( i = 0; i < 3; i++ )
			{
				d[i] = c[i] * a + d[i] * ( 1.0f - a );
			}
			d[3] = a + d[3] * ( 1.0f - a );
		}
	}
}

/**
  * @brief  Convert 0xRRGGBB to floats.
  * @param  c Colour.
  * @param  rgb Receives red, green, blue in 0..1.
  * @retval None.
  */
static void vg_rgb( uint32_t c, float rgb[3] )
{
	rgb[0] = (float)( ( c >> 16 ) & 255 ) / 255.0f;
	rgb[1] = (float)( ( c >> 8 ) & 255 ) / 255.0f;
	rgb[2] = (float)( c & 255 ) / 255.0f;
}

/**
  * @brief  Convert the premultiplied float buffer to straight RGBA bytes.
  * @param  buf Premultiplied float RGBA.
  * @param  n Pixel count.
  * @param  rgba Receives 4 bytes per pixel.
  * @retval None.
  */
static void vg_to_bytes( const float *buf, int n, uint8_t *rgba )
{
	float a, v;
	int   i, k;

	for ( i = 0; i < n; i++ )
	{
		a = buf[i * 4 + 3];
		for ( k = 0; k < 3; k++ )
		{
			v = a > 0.0001f ? buf[i * 4 + k] / a : 0.0f;
			v = v > 1.0f ? 1.0f : v;
			rgba[i * 4 + k] = (uint8_t)( v * 255.0f + 0.5f );
		}
		a = a > 1.0f ? 1.0f : a;
		rgba[i * 4 + 3] = (uint8_t)( a * 255.0f + 0.5f );
	}
}

int vg_asset_size( int id, float *w, float *h )
{
	if ( id < 0 || id >= ART_COUNT )
	{
		return -1;
	}
	if ( w )
	{
		*w = art_asset_table[id].w;
	}
	if ( h )
	{
		*h = art_asset_table[id].h;
	}
	return 0;
}

uint32_t vg_asset_tokens( int id )
{
	const art_asset_t *a;
	const art_layer_t *l;
	const art_grad_t  *g;
	uint32_t           mask = 0;
	int                i, k;

	if ( id < 0 || id >= ART_COUNT )
	{
		return 0;
	}
	a = &art_asset_table[id];
	for ( i = 0; i < a->layer_count; i++ )
	{
		l = &art_layers[a->layer_first + i];
		if ( l->fill_kind == ART_PAINT_TOKEN )
		{
			mask |= 1u << l->fill;
		}
		if ( l->stroke_kind == ART_PAINT_TOKEN )
		{
			mask |= 1u << l->stroke;
		}
		if ( l->grad >= 0 )
		{
			g = &art_grads[l->grad];
			for ( k = 0; k < g->stop_count; k++ )
			{
				if ( art_stops[g->stop_first + k].kind == ART_PAINT_TOKEN )
				{
					mask |= 1u << art_stops[g->stop_first + k].value;
				}
			}
		}
	}
	return mask;
}

int vg_render_asset( int id, int w, int h, const vg_tint *tint, uint8_t *rgba )
{
	const art_asset_t *a;
	const art_layer_t *l;
	vg_poly            poly, outline;
	vg_tint            zero;
	float             *buf, *cov, sx, sy, rgb[3], sw;
	int                i;

	if ( id < 0 || id >= ART_COUNT || w < 1 || h < 1 || !rgba )
	{
		return -1;
	}
	memset( &zero, 0, sizeof zero );
	tint = tint ? tint : &zero;
	a = &art_asset_table[id];
	sx = (float)w / a->w;
	sy = (float)h / a->h;
	buf = (float *)calloc( (size_t)w * (size_t)h * 4, sizeof *buf );
	cov = (float *)malloc( (size_t)w * (size_t)h * sizeof *cov );
	if ( !buf || !cov )
	{
		free( buf );
		free( cov );
		return -2;
	}
	vg_poly_init( &poly );
	vg_poly_init( &outline );
	for ( i = 0; i < a->layer_count; i++ )
	{
		l = &art_layers[a->layer_first + i];
		vg_poly_clear( &poly );
		vg_build_path( &art_path_data[l->path_first],
		               &art_path_data[l->path_first + l->path_len], sx, sy,
		               &poly );
		if ( l->grad >= 0 || l->fill_kind != ART_PAINT_NONE )
		{
			vg_cover( &poly, w, h, l->evenodd, cov );
			vg_rgb( vg_paint( l->fill_kind, l->fill, tint ), rgb );
			vg_composite( buf, cov, w, h, rgb,
			              l->grad >= 0 ? &art_grads[l->grad] : NULL, tint,
			              l->op, sx, sy );
		}
		if ( l->stroke_kind != ART_PAINT_NONE && l->sw > 0.0f )
		{
			vg_poly_clear( &outline );
			sw = l->sw * sqrtf( sx * sy );
			vg_stroke( &poly, sw, &outline );
			vg_cover( &outline, w, h, 0, cov );
			vg_rgb( vg_paint( l->stroke_kind, l->stroke, tint ), rgb );
			vg_composite( buf, cov, w, h, rgb, NULL, tint, l->op, sx, sy );
		}
	}
	vg_to_bytes( buf, w * h, rgba );
	vg_poly_free( &poly );
	vg_poly_free( &outline );
	free( buf );
	free( cov );
	return 0;
}

int vg_render_shape( int w, int h, float radius, float ring, uint8_t *rgba )
{
	vg_poly poly, outline;
	float  *cov;
	int     i;

	if ( w < 1 || h < 1 || !rgba )
	{
		return -1;
	}
	cov = (float *)malloc( (size_t)w * (size_t)h * sizeof *cov );
	if ( !cov )
	{
		return -2;
	}
	vg_poly_init( &poly );
	vg_poly_init( &outline );
	if ( ring > 0.0f )
	{
		vg_rrect( &poly, ring * 0.5f, ring * 0.5f, (float)w - ring,
		          (float)h - ring, radius - ring * 0.5f );
		vg_stroke( &poly, ring, &outline );
		vg_cover( &outline, w, h, 0, cov );
	}
	else
	{
		vg_rrect( &poly, 0.0f, 0.0f, (float)w, (float)h, radius );
		vg_cover( &poly, w, h, 0, cov );
	}
	for ( i = 0; i < w * h; i++ )
	{
		rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 255;
		rgba[i * 4 + 3] = (uint8_t)( cov[i] * 255.0f + 0.5f );
	}
	vg_poly_free( &poly );
	vg_poly_free( &outline );
	free( cov );
	return 0;
}

int vg_render_tick( int w, int h, float stroke, uint8_t *rgba )
{
	vg_poly poly, outline;
	float  *cov;
	int     i;

	if ( w < 1 || h < 1 || !rgba )
	{
		return -1;
	}
	cov = (float *)malloc( (size_t)w * (size_t)h * sizeof *cov );
	if ( !cov )
	{
		return -2;
	}
	vg_poly_init( &poly );
	vg_poly_init( &outline );
	vg_move( &poly, 0.20f * (float)w, 0.54f * (float)h );
	vg_line( &poly, 0.41f * (float)w, 0.76f * (float)h );
	vg_line( &poly, 0.82f * (float)w, 0.25f * (float)h );
	vg_stroke( &poly, stroke, &outline );
	vg_cover( &outline, w, h, 0, cov );
	for ( i = 0; i < w * h; i++ )
	{
		rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 255;
		rgba[i * 4 + 3] = (uint8_t)( cov[i] * 255.0f + 0.5f );
	}
	vg_poly_free( &poly );
	vg_poly_free( &outline );
	free( cov );
	return 0;
}

//------------------------------------------------------------------------------
//--------------------------------- Palettes -----------------------------------
//------------------------------------------------------------------------------

void vg_tint_make( int player, int cat, vg_tint *out )
{
	memset( out, 0, sizeof *out );
	if ( player >= 0 && player < ART_PLAYER_COUNT )
	{
		out->tok[ART_TOK_P] = art_player_colors[player][0];
		out->tok[ART_TOK_PD] = art_player_colors[player][1];
		out->tok[ART_TOK_PL] = art_player_colors[player][2];
	}
	if ( cat >= 1 && cat <= 9 )
	{
		out->tok[ART_TOK_C] = art_category_colors[cat - 1][0];
		out->tok[ART_TOK_CD] = art_category_colors[cat - 1][1];
	}
}

uint32_t vg_player_color( int player, int shade )
{
	player = player < 0 || player >= ART_PLAYER_COUNT ? 0 : player;
	shade = shade < 0 || shade > 2 ? 0 : shade;
	return art_player_colors[player][shade];
}

uint32_t vg_category_color( int cat, int shade )
{
	cat = cat < 1 || cat > 9 ? 9 : cat;
	shade = shade < 0 || shade > 2 ? 0 : shade;
	return art_category_colors[cat - 1][shade];
}

int vg_army_player( int set, int seat )
{
	set = set < 0 || set > 3 ? 0 : set;
	seat = seat < 0 || seat > 3 ? 0 : seat;
	return art_army_sets[set][seat];
}

uint32_t vg_ui_color( int dark, int idx )
{
	idx = idx < 0 || idx >= ART_UI_COUNT ? 0 : idx;
	return dark ? art_ui_dark[idx] : art_ui_light[idx];
}

const char *vg_category_name( int hr, int cat )
{
	cat = cat < 1 || cat > 9 ? 9 : cat;
	return hr ? art_category_names_hr[cat - 1] : art_category_names_en[cat - 1];
}
