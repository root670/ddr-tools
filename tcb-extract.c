#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include "BemaniLZ/BemaniLZ.h"

#if defined(_WIN32)
	#define MKDIR(a)	_mkdir(a);
#else
	#define MKDIR(a)	mkdir(a, 0777);
#endif

int main(int argc, char *argv[])
{
	if(argc != 3)
	{
		printf("TCB Extractor\n");
		printf("Usage:\n");
		printf("%s <MODE> <file containing TCBs>\n", argv[0]);
		printf("Modes:\n");
		printf("1 - Bruteforce search for compressed and uncompressed TCBs\n");
		printf("2 - Extract from file beginning with table (compressed entries)\n");
		printf("3 - Extract from file beginning with table (uncompressed entries)\n");
		return EXIT_FAILURE;
	}

	FILE *compressed = fopen(argv[2], "rb");
	if(!compressed)
	{
		printf("error opening %s\n", argv[2]);
		return EXIT_FAILURE;
	}

	/* Get input data */
	fseek(compressed, 0, SEEK_END);
	int length = ftell(compressed);
	fseek(compressed, 0, SEEK_SET);
	unsigned char *compressedData = malloc(length);
	fread(compressedData, length, 1, compressed);
	fclose(compressed);

	/* Search for TCBs */
	uint32_t offsets[5000];
	char isCompressed[5000] = {0};
	int numEntries = 0;

	// file has table
	if(argv[1][0] == '2' || argv[1][0] == '3')
	{
		int offset = 0;
		uint32_t firstEntry = (uint32_t)compressedData[0];

		if(firstEntry % 4)
		{
			printf("WARNING: First entry is number of entries to follow (%d).\n", firstEntry);
		}

		printf("first entry at 0x%08X\n", firstEntry);

		memcpy(offsets, compressedData, firstEntry);
		numEntries = firstEntry/4;

		if(argv[1][0] == '2')
		{
			for(int i = 0; i < numEntries; i++)
			{
				isCompressed[i] = 1;
			}
		}
	}
	else // no table, need to search for TCBs
	{
		for(unsigned int i = 0; i < length; i+= 1)
		{
			if(compressedData[i] == 'T' && compressedData[i+1] == 'C' && compressedData[i+2] == 'B')
			{
				if(compressedData[i+3] == 0
					&& compressedData[i+4] == 0
					&& compressedData[i+5] == 0
					&& compressedData[i+6] == 0
					&& compressedData[i+7] == 0
					&& compressedData[i+8] == 0
					&& compressedData[i+9] == 0
					&& compressedData[i+10] == 0)
				{
					printf("Uncompressed TCB at %08X\n", i);
					offsets[numEntries] = i;
					isCompressed[numEntries] = 0;
					numEntries++;
				}
				else if(i > 0 && (compressedData[i-1] == 0x10 || compressedData[i-1] == 0x90))
				{
					printf("Compressed TCB at %08X\n", i);
					offsets[numEntries] = i - 1;
					isCompressed[numEntries] = 1;
					numEntries++;
				}
			}
		}
	}

	offsets[numEntries] = length;

	printf("Found %d TCBs\n", numEntries);
	int numCompressed = 0;
	for(int i = 0; i < sizeof(isCompressed); i++)
	{
		if(isCompressed[i] == 1)
			numCompressed++;
	}
	printf("%d are compressed\n", numCompressed);

	/* Decompress and save TCBs */
	char outFolderName[100];
	snprintf(outFolderName, 100, "_%s", argv[2]);
	MKDIR(outFolderName);
	chdir(outFolderName);

	uint8_t *buffer = malloc(1024*1024*10);

	for(unsigned int i = 0; i < numEntries; i++)
	{
		if (offsets[i+1] == 0)
			break;

		int len = offsets[i+1] - offsets[i];
		char filename[100];
		snprintf(filename, 100, "%08X.tcb", offsets[i]);
		FILE *out = fopen(filename, "wb");

		if(isCompressed[i])
		{
			printf("Decompressing %08X...\n", offsets[i]);
			int decLen = decompress(compressedData + offsets[i], len, buffer, 1024*1024*10);
			fwrite(buffer, decLen, 1, out);
		}
		else
		{
			printf("Extracting %08X...\n", offsets[i]);
			fwrite(compressedData + offsets[i], len, 1, out);
		}

		fclose(out);
	}

	printf("%d entries\n", numEntries);

	free(compressedData);
	free(buffer);

    return 0;
}
