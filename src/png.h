//  Description:
//
//		png.h -- minimal PNG writer using stored (uncompressed) deflate blocks.
//
//		Needs no zlib; the files are large but every viewer reads them. Used
//		by the headless screenshot mode and the rasteriser tests.
//
#ifndef PNG_H
#define PNG_H

#include <stdint.h>

/**
  * @brief  Write an RGBA image as a PNG file.
  * @param  path Output file name.
  * @param  w Width in pixels.
  * @param  h Height in pixels.
  * @param  rgba w * h * 4 bytes, straight alpha.
  * @retval 0 on success, -1 bad arguments, -2 file error, -3 out of memory.
  */
int png_write( const char *path, int w, int h, const uint8_t *rgba );

#endif
