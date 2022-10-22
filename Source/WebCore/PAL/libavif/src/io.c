// Copyright 2020 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/internal.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

void avifIODestroy(avifIO * io)
{
    if (io && io->destroy) {
        io->destroy(io);
    }
}

// --------------------------------------------------------------------------------------
// avifIOMemoryReader

typedef struct avifIOMemoryReader
{
    avifIO io; // this must be the first member for easy casting to avifIO*
    avifROData rodata;
} avifIOMemoryReader;

static avifResult avifIOMemoryReaderRead(struct avifIO * io, uint32_t readFlags, uint64_t offset, size_t size, avifROData * out)
{
    // printf("avifIOMemoryReaderRead offset %" PRIu64 " size %zu\n", offset, size);

    if (readFlags != 0) {
        // Unsupported readFlags
        return AVIF_RESULT_IO_ERROR;
    }

    avifIOMemoryReader * reader = (avifIOMemoryReader *)io;

    // Sanitize/clamp incoming request
    if (offset > reader->rodata.size) {
        // The offset is past the end of the buffer.
        return AVIF_RESULT_IO_ERROR;
    }
    uint64_t availableSize = reader->rodata.size - offset;
    if (size > availableSize) {
        size = (size_t)availableSize;
    }

    out->data = reader->rodata.data + offset;
    out->size = size;
    return AVIF_RESULT_OK;
}

static void avifIOMemoryReaderDestroy(struct avifIO * io)
{
    avifFree(io);
}

avifIO * avifIOCreateMemoryReader(const uint8_t * data, size_t size)
{
    avifIOMemoryReader * reader = avifAlloc(sizeof(avifIOMemoryReader));
    memset(reader, 0, sizeof(avifIOMemoryReader));
    reader->io.destroy = avifIOMemoryReaderDestroy;
    reader->io.read = avifIOMemoryReaderRead;
    reader->io.sizeHint = size;
    reader->io.persistent = AVIF_TRUE;
    reader->rodata.data = data;
    reader->rodata.size = size;
    return (avifIO *)reader;
}

// --------------------------------------------------------------------------------------
// avifIOFileReader

typedef struct avifIOFileReader
{
    avifIO io; // this must be the first member for easy casting to avifIO*
    avifRWData buffer;
    FILE * f;
} avifIOFileReader;

static avifResult avifIOFileReaderRead(struct avifIO * io, uint32_t readFlags, uint64_t offset, size_t size, avifROData * out)
{
    // printf("avifIOFileReaderRead offset %" PRIu64 " size %zu\n", offset, size);

    if (readFlags != 0) {
        // Unsupported readFlags
        return AVIF_RESULT_IO_ERROR;
    }

    avifIOFileReader * reader = (avifIOFileReader *)io;

    // Sanitize/clamp incoming request
    if (offset > reader->io.sizeHint) {
        // The offset is past the EOF.
        return AVIF_RESULT_IO_ERROR;
    }
    uint64_t availableSize = reader->io.sizeHint - offset;
    if (size > availableSize) {
        size = (size_t)availableSize;
    }

    if (size > 0) {
        if (offset > LONG_MAX) {
            return AVIF_RESULT_IO_ERROR;
        }
        if (reader->buffer.size < size) {
            avifRWDataRealloc(&reader->buffer, size);
        }
        if (fseek(reader->f, (long)offset, SEEK_SET) != 0) {
            return AVIF_RESULT_IO_ERROR;
        }
        size_t bytesRead = fread(reader->buffer.data, 1, size, reader->f);
        if (size != bytesRead) {
            if (ferror(reader->f)) {
                return AVIF_RESULT_IO_ERROR;
            }
            size = bytesRead;
        }
    }

    out->data = reader->buffer.data;
    out->size = size;
    return AVIF_RESULT_OK;
}

static void avifIOFileReaderDestroy(struct avifIO * io)
{
    avifIOFileReader * reader = (avifIOFileReader *)io;
    fclose(reader->f);
    avifRWDataFree(&reader->buffer);
    avifFree(io);
}

avifIO * avifIOCreateFileReader(const char * filename)
{
    FILE * f = fopen(filename, "rb");
    if (!f) {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    if (fileSize < 0) {
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);

    avifIOFileReader * reader = avifAlloc(sizeof(avifIOFileReader));
    memset(reader, 0, sizeof(avifIOFileReader));
    reader->f = f;
    reader->io.destroy = avifIOFileReaderDestroy;
    reader->io.read = avifIOFileReaderRead;
    reader->io.sizeHint = (uint64_t)fileSize;
    reader->io.persistent = AVIF_FALSE;
    avifRWDataRealloc(&reader->buffer, 1024);
    return (avifIO *)reader;
}
