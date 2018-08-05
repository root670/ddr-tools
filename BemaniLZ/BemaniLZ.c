#include "BemaniLZ.h"

/* Based on BemaniLZ.cs from Scharfrichter */

static const unsigned int BUFFER_MASK = 0x3FF;

int decompress(uint8_t *src, int srcSize, uint8_t *dst, int dstSize)
{
	int srcOffset = 0;
	int dstOffset = 0;

	uint8_t buffer[0x400];
	int bufferOffset = 0;

	uint8_t data = '\0';
	uint32_t control = 0;
	int32_t length = 0;
	uint32_t distance = 0;
	int loop = 0;

	while(1)
	{
		loop = 0;

		control >>= 1;
		if (control < 0x100)
		{
			control = (uint8_t)src[srcOffset++] | 0xFF00;
			//printf("control=%08X\n", control);
		}

		data = src[srcOffset++];

		// direct copy
		// can do stream of 1 - 8 direct copies
		if ((control & 1) == 0)
		{
			//printf("%08X: direct copy %02X\n", dstOffset, data);
			dst[dstOffset++] = data;
			buffer[bufferOffset] = data;
			bufferOffset = (bufferOffset + 1) & BUFFER_MASK;
			continue;
		}

		// window copy (long distance)
		if ((data & 0x80) == 0)
		{
			/*
			input stream:
			00: 0bbb bbaa
			01: dddd dddd
			distance: [0 - 1023] (00aa dddd dddd)
			length: [2 - 33] (000b bbbb)
			*/
			distance = (uint8_t)src[srcOffset++] | ((data & 0x3) << 8);
			length = (data >> 2) + 2;
			loop = 1;
			//printf("long distance: distance=%08X length=%08X data=%02X\n", distance, length, data);
			//printf("%08X: window copy (long): %d bytes from %08X\n", dstOffset, length, dstOffset - distance);
		}

		// window copy (short distance)
		else if ((data & 0x40) == 0)
		{
			/*
			input stream:
			00: llll dddd
			distance: [1 - 16] (dddd)
			length: [1 - 4] (llll)
			*/
			distance = (data & 0xF) + 1;
			length = (data >> 4) - 7;
			loop = 1;
			//printf("short distance: distance=%08X length=%08X data=%02X\n", distance, length, data);
			//printf("%08X: window copy (short): %d bytes from %08X\n", dstOffset, length, dstOffset - distance);
		}

		if (loop)
		{
			// copy length bytes from window
			while(length-- >= 0)
			{
				data = buffer[(bufferOffset - distance) & BUFFER_MASK];
				dst[dstOffset++] = data;
				buffer[bufferOffset] = data;
				bufferOffset = (bufferOffset + 1) & BUFFER_MASK;
			}
			continue;
		}

		// end of stream
		if (data == 0xFF)
			break;

		// block copy
		// directly copy group of bytes
		/*
		input stream:
		00: llll lll0
		length: [8 - 69]
		directly copy (length+1) bytes
		*/
		length = data - 0xB9;
		//printf("block copy %d bytes\n", length+1);
		while (length-- >= 0)
		{
			data = src[srcOffset++];
			dst[dstOffset++] = data;
			buffer[bufferOffset] = data;
			bufferOffset = (bufferOffset + 1) & BUFFER_MASK;
		}
	}

	return dstOffset;
}
