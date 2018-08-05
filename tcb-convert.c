#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include "lodepng/lodepng.h"
#include "pngquant/libimagequant.h"

typedef struct tcbHeader {
	char magic[4];
	char padding[12];

	uint32_t totalSize;
	uint32_t unk1;
	uint32_t dataLength;
	uint32_t unk3;

	uint16_t unk4;
	uint16_t imageType;
	uint16_t width;
	uint16_t height;
	uint8_t extra[0x28];
} tcbHeader_t;

typedef struct rgbaImageData {
	uint32_t *rgba;
	unsigned int width;
	unsigned int height;
} rgbaImageData_t;

#define LONIBBLE(x)  ((uint8_t) ((uint8_t) (x) & (uint8_t) 0x0F))
#define HINIBBLE(x)  ((uint8_t) ((uint8_t) (x) >> (uint8_t) 4))

/**
 * Filters palette colors to/from the format used by PS2 games.
 *
 * @param unfiltered		pointer to array of unfiltered RGBA colors.
 * @param filtered			pointer to store array of filtered RGBA colors.
 * @param lengeth			number of elements in unfiltered/filtered color array.
 *
 * @pre unfiltered and filtered point to allocated memory of length*4 bytes.
 * @post filtered colors copied to filtered.
 */
void PaletteFilter(uint32_t *unfiltered, uint32_t *filtered, int length)
{
	int parts = length / 32;
	int stripes = 2;
	int colors = 8;
	int blocks = 2;

	int i = 0;
	for (int part = 0; part < parts; part++)
	{
		for (int block = 0; block < blocks; block++)
		{
			for (int stripe = 0; stripe < stripes; stripe++)
			{

				for (int color = 0; color < colors; color++)
				{
					filtered[i++] = unfiltered[ part * colors * stripes * blocks + block * colors + stripe * stripes * colors + color ];
				}
			}
		}
	}
}

int tcb_to_rgba(uint8_t *tcbData, int tcbLength, rgbaImageData_t *imageData);
int rgba_to_tcb(uint8_t *tcbData, int tcbLength, rgbaImageData_t *imageData);

int png_decode(char *pngPath, rgbaImageData_t *imageData);
int png_encode(char *pngPath, rgbaImageData_t *imageData);

int main(int argc, char *argv[])
{
	FILE *tcbFile;
	int tcbLength;
	uint8_t *tcbData;
	char *tcbPath;
	char pngPath[260];
	char mode;
	rgbaImageData_t imageData;

	if(argc != 3)
	{
		printf("TCB Converter\n");
		printf("Usage:\n");
		printf("Convert TCB to PNG: %s e <image.tcb>\n", argv[0]);
		printf("Inject PNG into TCB: %s i <image.tcb>\n", argv[0]);
		return EXIT_FAILURE;
	}

	mode = tolower(argv[1][0]);
	tcbPath = argv[2];
	strncpy(pngPath, tcbPath, 260);
	strncat(pngPath, ".png", 260);

	tcbFile = fopen(tcbPath, "rb");
	if(!tcbFile)
	{
		printf("Error opening %s\n", tcbPath);
		return EXIT_FAILURE;
	}

	fseek(tcbFile, 0, SEEK_END);
	tcbLength = ftell(tcbFile);
	fseek(tcbFile, 0, SEEK_SET);
	tcbData = malloc(tcbLength);
	if(!tcbData)
	{
		printf("malloc error\n");
		fclose(tcbFile);
		return EXIT_FAILURE;
	}
	fread(tcbData, 1, tcbLength, tcbFile);
	fclose(tcbFile);

	if(mode == 'i')
	{
		printf("Injecting image from %s into %s...\n", pngPath, tcbPath);

		if(png_decode(pngPath, &imageData))
		{
			free(tcbData);
			return EXIT_FAILURE;
		}

		if(rgba_to_tcb(tcbData, tcbLength, &imageData))
		{
			free(tcbData);
			free(imageData.rgba);
			return EXIT_FAILURE;
		}

		printf("Success!\n");
	}
	else if(mode == 'e')
	{
		printf("Extracting image from %s to %s...\n", tcbPath, pngPath);

		if(tcb_to_rgba(tcbData, tcbLength, &imageData))
		{
			free(tcbData);
			return EXIT_FAILURE;
		}

		if(png_encode(pngPath, &imageData))
		{
			free(tcbData);
			free(imageData.rgba);
			return EXIT_FAILURE;
		}

		printf("Success!\n");
	}
	else
	{
		printf("Error: Unsupported mode '%c'\n", mode);
		free(tcbData);
		return EXIT_FAILURE;
	}

	free(tcbData);
	free(imageData.rgba);

    return EXIT_SUCCESS;
}

/**
 * Convert TCB data to RGBA data.
 *
 * @param tcbData		pointer to data from a TCB image.
 * @param tcbLength		size of tcbData.
 * @param imageData		pointer to struct to store RGBA image data.
 *
 * @post Image data from TCB is converted to 32-bit RGBA and stored in imageData->rgba.
 * @return 1 on success, 0 on failure
 */
int tcb_to_rgba(uint8_t *tcbData, int tcbLength, rgbaImageData_t *imageData)
{
	tcbHeader_t *header;
	uint8_t index, alpha;
	uint8_t *image;
	uint32_t *palette;

	if (!tcbData || tcbLength <= 0 || !imageData)
		return EXIT_FAILURE;

	header = (tcbHeader_t*) tcbData;
	image = (uint8_t*) (tcbData + 0x50);
	palette = (uint32_t*) (tcbData + header->dataLength + 0x50);

	if(strncmp(header->magic, "TCB\0", 4))
	{
		printf("ERROR: Not a TCB file.\n");
		return EXIT_FAILURE;
	}

	imageData->rgba = malloc(header->width * header->height * 4);
	imageData->width = header->width;
	imageData->height = header->height;

	if(header->imageType == 0x05) // 8bpp
	{
		uint32_t *newPalette = calloc(256, 4);
        PaletteFilter(palette, newPalette, 256);
        palette = newPalette;

		for(int i = 0; i < (header->width * header->height); i++)
		{
			index = image[i];

			alpha = palette[index] >> 24;
			if(alpha > 0)
				alpha = (alpha << 1) - 1;  // scale from 0-128 to 0-255 range

			imageData->rgba[i] = palette[index] | (alpha << 24); // 255 alpha channel
		}

		free(newPalette);
	}
	else if(header->imageType == 0x04) // 4bpp
	{
		for(int i = 0; i < (header->width * header->height); i++)
		{
			index = image[i/2];

			if(i%2)
				index = HINIBBLE(index);
			else
				index = LONIBBLE(index);

			alpha = palette[index] >> 24;
			if(alpha > 0)
				alpha = (alpha << 1) - 1;  // scale from 0-128 to 0-255 range

			imageData->rgba[i] = palette[index] | (alpha << 24);
		}
	}

	return EXIT_SUCCESS;
}

/**
 * Inject RGBA data into existing TCB data.
 *
 * @param tcbData		pointer to data from TCB image.
 * @param tcbLength		size of tcbData.
 * @param imageData		pointer to struct with RGBA image data.
 *
 * @post Image data from rgbaImageData is quantized and injected into tcbData.
 * @return 0 on success, 1 on failure.
 */
int rgba_to_tcb(uint8_t *tcbData, int tcbLength, rgbaImageData_t *imageData)
{
	tcbHeader_t *header;
	uint8_t *image;
	uint32_t *palette;
	int len;

	if(!tcbData || tcbLength <= 0 || !imageData)
		return EXIT_FAILURE;

	header = (tcbHeader_t*) tcbData;
	image = (uint8_t*) (tcbData + 0x50);
	palette = (uint32_t*) (tcbData + header->dataLength + 0x50);

	if(header->width != imageData->width || header->height != imageData->height)
	{
		printf("Image size mismatch. PNG dimensions are %ux%u, but it needs to be %ux%u to be injected into the TCB.\n", imageData->width, imageData->height, header->width, header->height);
		return EXIT_FAILURE;
	}

	liq_attr *attr = liq_attr_create();
	if(header->imageType == 0x04) // 4-bit color
	{
		liq_set_max_colors(attr, 16);
	}
	else if(header->imageType == 0x05);
	else
	{
		printf("Unsupported image type 0x%02X.\n", header->imageType);
		return EXIT_FAILURE;
	}

	/* Quantize image and create new image map and palette */
	liq_image *imagel = liq_image_create_rgba(attr, imageData->rgba, imageData->width, imageData->height, 0);
	liq_result *res = liq_quantize_image(attr, imagel);

	if(header->imageType == 0x05)
	{
		liq_write_remapped_image(res, imagel, image, imageData->height * imageData->width);
	}
	else if (header->imageType == 0x04) // 4-bit color
	{
		uint8_t *fulldata = malloc(imageData->width * imageData->height);
		liq_write_remapped_image(res, imagel, (uint32_t *)fulldata, imageData->width * imageData->height);

		// Convert to 4-bit
		for(int i = 0; i < (imageData->width * imageData->height)/2; i++)
		{
			uint8_t pixel1 = *((uint8_t *)fulldata + i*2);
			uint8_t pixel2 = *((uint8_t *)fulldata + i*2 + 1);

			image[i] = pixel1 | (pixel2 << 4);
		}

		free(fulldata);
	}

	/* Update palette in TCB */
	const liq_palette *pal = liq_get_palette(res);
	memcpy(palette, pal->entries, pal->count*4);

	for(int i = 0; i < pal->count; i++)
	{
		uint8_t alpha = palette[i] >> 24;
		if(alpha > 0)
			alpha = (alpha >> 1) + 1; // scale down to 0-128 range
	}

	if(header->imageType == 0x05) // Filter palette data
	{
		uint32_t *filtered = malloc(256 * 4);
		PaletteFilter((uint32_t *) palette, filtered, 256);
		memcpy((void *) palette, filtered, 256 * 4);
		free(filtered);
	}

	liq_attr_destroy(attr);
	liq_image_destroy(imagel);
	liq_result_destroy(res);

	return EXIT_SUCCESS;
}

/**
 * Decode PNG file to RGBA data.
 *
 * @param pngPath		Path to PNG file.
 * @param imageData		pointer to struct to store RGBA image data.
 * @return 0 on success, 1 on failure.
 */
int png_decode(char *pngPath, rgbaImageData_t *imageData)
{
	int ret;
	if(!pngPath || !imageData)
		return EXIT_FAILURE;

	ret = lodepng_decode32_file((unsigned char **)&imageData->rgba, &imageData->width, &imageData->height, pngPath);
	if(ret != 0)
	{
		printf("PNG decoding error %u: %s\n", ret, lodepng_error_text(ret));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

/**
 * Encode PNG file with RGBA data.
 *
 * @param pngPath		Path to PNG file.
 * @param imageData		pointer to struct with RGBA image data.
 * @return 0 on success, 1 on failure.
 */
int png_encode(char *pngPath, rgbaImageData_t *imageData)
{
	int ret;
	if(!pngPath || !imageData)
		return EXIT_FAILURE;

	ret = lodepng_encode32_file(pngPath, (const unsigned char *)imageData->rgba, imageData->width, imageData->height);

	if(ret != 0)
	{
		printf("PNG encoding error %u: %s\n", ret, lodepng_error_text(ret));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
