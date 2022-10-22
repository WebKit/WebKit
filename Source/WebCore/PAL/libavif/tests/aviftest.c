// Copyright 2020 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

// #define WIN32_MEMORY_LEAK_DETECTION
#ifdef WIN32_MEMORY_LEAK_DETECTION
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "avif/avif.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)

#include <windows.h>

typedef struct NextFilenameData
{
    int didFirstFile;
    HANDLE handle;
    WIN32_FIND_DATA wfd;
} NextFilenameData;

static const char * nextFilename(const char * parentDir, const char * extension, NextFilenameData * nfd)
{
    for (;;) {
        if (nfd->didFirstFile) {
            if (FindNextFile(nfd->handle, &nfd->wfd) == 0) {
                // No more files
                break;
            }
        } else {
            char filenameBuffer[2048];
            snprintf(filenameBuffer, sizeof(filenameBuffer), "%s\\*", parentDir);
            filenameBuffer[sizeof(filenameBuffer) - 1] = 0;
            nfd->handle = FindFirstFile(filenameBuffer, &nfd->wfd);
            if (nfd->handle == INVALID_HANDLE_VALUE) {
                return NULL;
            }
            nfd->didFirstFile = 1;
        }

        // If we get here, we should have a valid wfd
        const char * dot = strrchr(nfd->wfd.cFileName, '.');
        if (dot) {
            ++dot;
            if (!strcmp(dot, extension)) {
                return nfd->wfd.cFileName;
            }
        }
    }

    FindClose(nfd->handle);
    nfd->handle = INVALID_HANDLE_VALUE;
    nfd->didFirstFile = 0;
    return NULL;
}

#else
#include <dirent.h>
typedef struct NextFilenameData
{
    DIR * dir;
} NextFilenameData;

static const char * nextFilename(const char * parentDir, const char * extension, NextFilenameData * nfd)
{
    if (!nfd->dir) {
        nfd->dir = opendir(parentDir);
        if (!nfd->dir) {
            return NULL;
        }
    }

    struct dirent * entry;
    while ((entry = readdir(nfd->dir)) != NULL) {
        const char * dot = strrchr(entry->d_name, '.');
        if (dot) {
            ++dot;
            if (!strcmp(dot, extension)) {
                return entry->d_name;
            }
        }
    }

    closedir(nfd->dir);
    nfd->dir = NULL;
    return NULL;
}
#endif

typedef struct avifIOTestReader
{
    avifIO io;
    avifROData rodata;
    size_t availableBytes;
} avifIOTestReader;

static avifResult avifIOTestReaderRead(struct avifIO * io, uint32_t readFlags, uint64_t offset, size_t size, avifROData * out)
{
    // printf("avifIOTestReaderRead offset %" PRIu64 " size %zu\n", offset, size);

    if (readFlags != 0) {
        // Unsupported readFlags
        return AVIF_RESULT_IO_ERROR;
    }

    avifIOTestReader * reader = (avifIOTestReader *)io;

    // Sanitize/clamp incoming request
    if (offset > reader->rodata.size) {
        // The offset is past the end of the buffer.
        return AVIF_RESULT_IO_ERROR;
    }
    if (offset == reader->rodata.size) {
        // The parser is *exactly* at EOF: return a 0-size pointer to any valid buffer
        offset = 0;
        size = 0;
    }
    uint64_t availableSize = reader->rodata.size - offset;
    if (size > availableSize) {
        size = (size_t)availableSize;
    }

    if (offset > reader->availableBytes) {
        return AVIF_RESULT_WAITING_ON_IO;
    }
    if (size > (reader->availableBytes - offset)) {
        return AVIF_RESULT_WAITING_ON_IO;
    }

    out->data = reader->rodata.data + offset;
    out->size = size;
    return AVIF_RESULT_OK;
}

static void avifIOTestReaderDestroy(struct avifIO * io)
{
    avifFree(io);
}

static avifIOTestReader * avifIOCreateTestReader(const uint8_t * data, size_t size)
{
    avifIOTestReader * reader = avifAlloc(sizeof(avifIOTestReader));
    memset(reader, 0, sizeof(avifIOTestReader));
    reader->io.destroy = avifIOTestReaderDestroy;
    reader->io.read = avifIOTestReaderRead;
    reader->io.sizeHint = size;
    reader->io.persistent = AVIF_TRUE;
    reader->rodata.data = data;
    reader->rodata.size = size;
    return reader;
}

#define FILENAME_MAX_LENGTH 2047

static int runIOTests(const char * dataDir)
{
    printf("AVIF Test Suite: Running IO Tests...\n");

    static const char * ioSuffix = "/io/";

    char ioDir[FILENAME_MAX_LENGTH + 1];
    size_t dataDirLen = strlen(dataDir);
    size_t ioSuffixLen = strlen(ioSuffix);

    if ((dataDirLen + ioSuffixLen) > FILENAME_MAX_LENGTH) {
        printf("Path too long: %s\n", dataDir);
        return 1;
    }
    strcpy(ioDir, dataDir);
    strcat(ioDir, ioSuffix);
    size_t ioDirLen = strlen(ioDir);

    int retCode = 0;

    NextFilenameData nfd;
    memset(&nfd, 0, sizeof(nfd));
    avifRWData fileBuffer = AVIF_DATA_EMPTY;
    const char * filename = nextFilename(ioDir, "avif", &nfd);
    for (; filename != NULL; filename = nextFilename(ioDir, "avif", &nfd)) {
        char fullFilename[FILENAME_MAX_LENGTH + 1];
        size_t filenameLen = strlen(filename);
        if ((ioDirLen + filenameLen) > FILENAME_MAX_LENGTH) {
            printf("Path too long: %s\n", filename);
            return 1;
        }
        strcpy(fullFilename, ioDir);
        strcat(fullFilename, filename);

        FILE * f = fopen(fullFilename, "rb");
        if (!f) {
            printf("Can't open for read: %s\n", filename);
            return 1;
        }
        fseek(f, 0, SEEK_END);
        size_t fileSize = ftell(f);
        fseek(f, 0, SEEK_SET);
        avifRWDataRealloc(&fileBuffer, fileSize);
        if (fread(fileBuffer.data, 1, fileSize, f) != fileSize) {
            printf("Can't read entire file: %s\n", filename);
            fclose(f);
            return 1;
        }
        fclose(f);

        avifDecoder * decoder = avifDecoderCreate();
        avifIOTestReader * io = avifIOCreateTestReader(fileBuffer.data, fileBuffer.size);
        avifDecoderSetIO(decoder, (avifIO *)io);

        for (int pass = 0; pass < 4; ++pass) {
            io->io.persistent = ((pass % 2) == 0);
            decoder->ignoreExif = decoder->ignoreXMP = (pass < 2);

            // Slowly pretend to have streamed-in / downloaded more and more bytes
            avifResult parseResult = AVIF_RESULT_UNKNOWN_ERROR;
            for (io->availableBytes = 0; io->availableBytes <= io->io.sizeHint; ++io->availableBytes) {
                parseResult = avifDecoderParse(decoder);
                if (parseResult == AVIF_RESULT_WAITING_ON_IO) {
                    continue;
                }
                if (parseResult != AVIF_RESULT_OK) {
                    retCode = 1;
                }

                printf("File: [%s @ %zu / %" PRIu64 " bytes, %s, %s] parse returned: %s\n",
                       filename,
                       io->availableBytes,
                       io->io.sizeHint,
                       io->io.persistent ? "Persistent" : "NonPersistent",
                       decoder->ignoreExif ? "IgnoreMetadata" : "Metadata",
                       avifResultToString(parseResult));
                break;
            }

            if (parseResult == AVIF_RESULT_OK) {
                for (; io->availableBytes <= io->io.sizeHint; ++io->availableBytes) {
                    avifExtent extent;
                    avifResult extentResult = avifDecoderNthImageMaxExtent(decoder, 0, &extent);
                    if (extentResult != AVIF_RESULT_OK) {
                        retCode = 1;

                        printf("File: [%s @ %zu / %" PRIu64 " bytes, %s, %s] maxExtent returned: %s\n",
                               filename,
                               io->availableBytes,
                               io->io.sizeHint,
                               io->io.persistent ? "Persistent" : "NonPersistent",
                               decoder->ignoreExif ? "IgnoreMetadata" : "Metadata",
                               avifResultToString(extentResult));
                    } else {
                        avifResult nextImageResult = avifDecoderNextImage(decoder);
                        if (nextImageResult == AVIF_RESULT_WAITING_ON_IO) {
                            continue;
                        }
                        if (nextImageResult != AVIF_RESULT_OK) {
                            retCode = 1;
                        }

                        printf("File: [%s @ %zu / %" PRIu64 " bytes, %s, %s] nextImage [MaxExtent off %" PRIu64 ", size %zu] returned: %s\n",
                               filename,
                               io->availableBytes,
                               io->io.sizeHint,
                               io->io.persistent ? "Persistent" : "NonPersistent",
                               decoder->ignoreExif ? "IgnoreMetadata" : "Metadata",
                               extent.offset,
                               extent.size,
                               avifResultToString(nextImageResult));
                    }
                    break;
                }
            }
        }

        avifDecoderDestroy(decoder);
    }

    avifRWDataFree(&fileBuffer);
    return retCode;
}

static void syntax(void)
{
    fprintf(stderr, "Syntax: aviftest dataDir\n");
}

int main(int argc, char * argv[])
{
    const char * dataDir = NULL;

#ifdef WIN32_MEMORY_LEAK_DETECTION
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    // _CrtSetBreakAlloc(2906);
#endif

    // Parse cmdline
    for (int i = 1; i < argc; ++i) {
        char * arg = argv[i];
        if (!strcmp(arg, "--io-only")) {
            fprintf(stderr, "WARNING: --io-only is deprecated; ignoring.\n");
        } else if (dataDir == NULL) {
            dataDir = arg;
        } else {
            fprintf(stderr, "Too many positional arguments: %s\n", arg);
            syntax();
            return 1;
        }
    }

    // Verify all required args were set
    if (dataDir == NULL) {
        fprintf(stderr, "dataDir is required, bailing out.\n");
        syntax();
        return 1;
    }

    setbuf(stdout, NULL);

    char codecVersions[256];
    avifCodecVersions(codecVersions);
    printf("Codec Versions: %s\n", codecVersions);
    printf("Test Data Dir : %s\n", dataDir);

    int retCode = runIOTests(dataDir);
    if (retCode == 0) {
        printf("AVIF Test Suite: Complete.\n");
    } else {
        printf("AVIF Test Suite: Failed.\n");
    }
    return retCode;
}
