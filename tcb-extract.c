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

int search_for_tcbs(const unsigned char *data, size_t dataLength, uint32_t *offsets, char *isCompressed)
{
    int numFound = 0;

    for(unsigned int i = 0; i < dataLength; i++)
    {
        if(data[i] == 'T' && data[i+1] == 'C' && data[i+2] == 'B')
        {
            if(data[i+3] == 0
                && data[i+4] == 0
                && data[i+5] == 0
                && data[i+6] == 0
                && data[i+7] == 0
                && data[i+8] == 0
                && data[i+9] == 0
                && data[i+10] == 0)
            {
                offsets[numFound] = i;
                isCompressed[numFound] = 0;
                numFound++;
            }
            else if(i > 0 && (data[i-1] == 0x10 || data[i-1] == 0x90))
            {
                offsets[numFound] = i - 1;
                isCompressed[numFound] = 1;
                numFound++;
            }
        }
    }

    return numFound;
}

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
        printf("Error opening %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    /* Get input data */
    printf("Loading %s...\n", argv[2]);
    fseek(compressed, 0, SEEK_END);
    int length = ftell(compressed);
    fseek(compressed, 0, SEEK_SET);
    unsigned char *compressedData = malloc(length);
    fread(compressedData, length, 1, compressed);
    fclose(compressed);

    /* Search for TCBs */
    printf("Searching for TCBs...\n");
    uint32_t offsets[8000];
    char isCompressed[8000] = {0};
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
        numEntries = search_for_tcbs(compressedData, length, offsets, isCompressed);
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

        printf("Extracting files: %04d/%d [%d%%]\r", i+1, numEntries, ((i+1)*100)/numEntries);

        int len = offsets[i+1] - offsets[i];
        char filename[100];
        snprintf(filename, 100, "%08X.tcb", offsets[i]);
        FILE *out = fopen(filename, "wb");

        if(isCompressed[i])
        {
            int decLen = decompress(compressedData + offsets[i], len, buffer, 1024*1024*10);
            fwrite(buffer, decLen, 1, out);
        }
        else
        {
            fwrite(compressedData + offsets[i], len, 1, out);
        }

        fclose(out);
    }

    printf("\n");
    free(compressedData);
    free(buffer);

    return 0;
}
