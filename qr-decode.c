#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <png.h>

#include "process-zbar.h"
#include "process-quirc.h"

struct timeval time_start;
struct timeval time_stop;

void start(void)
{
	gettimeofday(&time_start, NULL);
}

void stop(char num)
{
	unsigned long us;

	gettimeofday(&time_stop, NULL);

	us = ((time_stop.tv_usec > time_start.tv_usec) ?
		time_stop.tv_usec - time_start.tv_usec :
		time_start.tv_usec - time_stop.tv_usec);

	printf("[ %02d ] %lu s %lu us\n", num,
		time_stop.tv_sec - time_start.tv_sec, us);
}

int width, height, row_bytes;
png_byte color_type;
png_byte bit_depth;
png_bytep *row_pointers;

void get_png_data(char *filename)
{
	FILE *fp = fopen(filename, "rb");
		if (!fp) {
			printf("Error reading file.\n");
			exit(1);
		}

	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if(!png) abort();

	png_infop info = png_create_info_struct(png);
	if(!info) abort();

	if(setjmp(png_jmpbuf(png))) abort();

	png_init_io(png, fp);

	png_read_info(png, info);

	width		= png_get_image_width(png, info);
	height		= png_get_image_height(png, info);
	color_type	= png_get_color_type(png, info);
	bit_depth	= png_get_bit_depth(png, info);
	row_bytes	= png_get_rowbytes(png,info);

	fprintf(stderr, "image width: %d height: %d "
		"bit depth: %d colour type: %d channels: %d\n",
		width, height, bit_depth, color_type,
		png_get_channels(png, info));
	fflush(stderr);

	if (color_type != PNG_COLOR_TYPE_GRAY &&
	    color_type != PNG_COLOR_TYPE_GRAY_ALPHA) {
		printf("Image is not grayscale.\n");
		exit(1);
	}

	// Read any color_type into 8bit depth, RGBA format.
	// See http://www.libpng.org/pub/png/libpng-manual.txt

	if(bit_depth == 16)
		png_set_strip_16(png);

	if(color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png);

	// PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
	if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
		fprintf(stderr, "Expanding bit depth %d to 8\n", bit_depth);
		fflush(stderr);
		png_set_expand_gray_1_2_4_to_8(png);
	}

	if(png_get_valid(png, info, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png);

	// These color_type don't have an alpha channel then fill it with 0xff.
	if(color_type == PNG_COLOR_TYPE_RGB ||
		 color_type == PNG_COLOR_TYPE_GRAY ||
		 color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

	if(color_type == PNG_COLOR_TYPE_GRAY ||
		 color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png);

	png_read_update_info(png, info);

	row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
	for(int y = 0; y < height; y++) {
		row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png,info));
	}

	png_read_image(png, row_pointers);

	fclose(fp);
}

void clean_up()
{
	for(int y = 0; y < height; y++) {
		free(row_pointers[y]);
	}
	free(row_pointers);
}

int main(int argc, char *argv[])
{
	unsigned char *buffer;
	int x, y;
	png_bytep row;
	png_bytep px;

	/* get png data */
	get_png_data(argv[1]);

	/* allocate buffer */
	buffer   = malloc(width * height);

	fprintf(stderr, "row width: %d image width: %d\n", row_bytes, width);
	fflush(stderr);

	for(y = 0; y < height; y++) {
		row = row_pointers[y];
		for(x = 0; x < width; x++) {
			px = &(row[x * 4]);
			/* we dont need the other two channels, or alpha */
			buffer[x + (y*width)] = px[0];
		}
	}

	start();
	process_quirc(buffer, width, height);
	stop(0);

	start();
	process_zbar(buffer, width, height);
	stop(1);

	/* free memory */
	free(buffer);
	clean_up();

	return 0;
}
