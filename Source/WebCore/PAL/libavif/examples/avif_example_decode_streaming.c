// Copyright 2020 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// This example intends to show how a custom avifIO implementation can be used to decode
// partially-downloaded AVIFs. Read either the avif_example_decode_file or
// avif_example_decode_memory examples for something more basic.
//
// This test will emit something like this:
//
//     File: [file.avif @ 4823 / 125867 bytes, Metadata] parse returned: OK
//     File: [file.avif @ 125867 / 125867 bytes, Metadata] nextImage returned: OK
//     File: [file.avif @ 390 / 125867 bytes, IgnoreMetadata] parse returned: OK
//     File: [file.avif @ 125867 / 125867 bytes, IgnoreMetadata] nextImage returned: OK
//
// In the above output, file.avif is 125867 bytes. If parsing Exif/XMP metadata is enabled,
// avifDecoderParse() finally returns AVIF_RESULT_OK once 4823 bytes are "downloaded".
// and requires the entire file to decode the first image (this example is a single image AVIF).
// If Exif/XMP metadata is ignored, avifDecoderParse() only needs the first 390 bytes to return OK.
//
// How much of an AVIF is required to be downloaded in order to return OK from avifDecoderParse()
// or avifDecoderNextImage() varies wildly due to the packing of the file. Ideally, the end of the
// AVIF is simply a large mdat or moov box full of AV1 payloads, and all metadata (meta boxes,
// Exif/XMP payloads, etc) are as close to the front as possible. Any trailing MP4 boxes (free, etc)
// will cause avifDecoderParse() to have to wait to download those, as it can't ensure a succesful
// parse without knowing what boxes are remaining.

typedef struct avifIOStreamingReader
{
    avifIO io;              // This must be first if you plan to cast this struct to an (avifIO *).
    avifROData rodata;      // The actual data.
    size_t downloadedBytes; // How many bytes have been "downloaded" so far. This is what will
                            // dictate when we return AVIF_RESULT_WAITING_ON_IO in this example.
                            // The example will slowly increment this value (from 0) until
                            // avifDecoderParse() returns something other than AVIF_RESULT_WAITING_ON_IO,
                            // and then it will continue to incremement it until avifDecoderNextImage()
                            // returns something other than AVIF_RESULT_WAITING_ON_IO.
} avifIOStreamingReader;

// This example has interleaved the documentation above avifIOReadFunc in avif/avif.h to help
// explain why these checks are here.
static avifResult avifIOStreamingReaderRead(struct avifIO * io, uint32_t readFlags, uint64_t offset, size_t size, avifROData * out)
{
    avifIOStreamingReader * reader = (avifIOStreamingReader *)io;

    if (readFlags != 0) {
        // Unsupported readFlags
        return AVIF_RESULT_IO_ERROR;
    }

    // * If offset exceeds the size of the content (past EOF), return AVIF_RESULT_IO_ERROR.
    if (offset > reader->rodata.size) {
        return AVIF_RESULT_IO_ERROR;
    }

    // * If offset is *exactly* at EOF, provide any 0-byte buffer and return AVIF_RESULT_OK.
    if (offset == reader->rodata.size) {
        out->data = reader->rodata.data;
        out->size = 0;
        return AVIF_RESULT_OK;
    }

    // * If (offset+size) exceeds the contents' size, it must provide a truncated buffer that provides
    //   all bytes from the offset to EOF, and return AVIF_RESULT_OK.
    uint64_t availableSize = reader->rodata.size - offset;
    if (size > availableSize) {
        size = (size_t)availableSize;
    }

    // * If (offset+size) does not exceed the contents' size but the *entire range* is unavailable yet
    //   (due to network conditions or any other reason), return AVIF_RESULT_WAITING_ON_IO.
    if (offset > reader->downloadedBytes) {
        return AVIF_RESULT_WAITING_ON_IO;
    }
    if (size > (reader->downloadedBytes - offset)) {
        return AVIF_RESULT_WAITING_ON_IO;
    }

    // * If (offset+size) does not exceed the contents' size, it must provide the *entire range* and
    //   return AVIF_RESULT_OK.
    out->data = reader->rodata.data + offset;
    out->size = size;
    return AVIF_RESULT_OK;
}

static void avifIOStreamingReaderDestroy(struct avifIO * io)
{
    avifFree(io);
}

// Returns null in case of memory allocation failure.
static avifIOStreamingReader * avifIOCreateStreamingReader(const uint8_t * data, size_t size)
{
    avifIOStreamingReader * reader = avifAlloc(sizeof(avifIOStreamingReader));
    if (!reader)
        return NULL;
    memset(reader, 0, sizeof(avifIOStreamingReader));

    // It is legal for io.destroy to be NULL, in which you are responsible for cleaning up
    // your own reader. This allows for a pre-existing, on-the-stack, or member variable to be
    // used as an avifIO*.
    reader->io.destroy = avifIOStreamingReaderDestroy;

    // The heart of the reader is this function. See the implementation and comments above.
    reader->io.read = avifIOStreamingReaderRead;

    // See the documentation for sizeHint in avif/avif.h. It is not required to be set, but it is recommended.
    reader->io.sizeHint = size;

    // See the documentation for persistent in avif/avif.h. Enabling this adds heavy restrictions to
    // the lifetime of the buffers you return from io.read, but cuts down on memory overhead and memcpys.
    reader->io.persistent = AVIF_TRUE;

    reader->rodata.data = data;
    reader->rodata.size = size;
    return reader;
}

int main(int argc, char * argv[])
{
    if (argc != 2) {
        fprintf(stderr, "avif_example_decode_streaming [filename.avif]\n");
        return 1;
    }
    const char * inputFilename = argv[1];

    int returnCode = 1;
    avifDecoder * decoder = NULL;

    // Read entire file into fileBuffer
    FILE * f = NULL;
    uint8_t * fileBuffer = NULL;
    f = fopen(inputFilename, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open file for read: %s\n", inputFilename);
        goto cleanup;
    }
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    if (fileSize < 0) {
        fprintf(stderr, "Truncated file: %s\n", inputFilename);
        goto cleanup;
    }
    fseek(f, 0, SEEK_SET);
    fileBuffer = malloc(fileSize);
    long bytesRead = (long)fread(fileBuffer, 1, fileSize, f);
    if (bytesRead != fileSize) {
        fprintf(stderr, "Cannot read file: %s\n", inputFilename);
        goto cleanup;
    }

    decoder = avifDecoderCreate();
    if (!decoder) {
        fprintf(stderr, "Memory allocation failure\n");
        goto cleanup;
    }
    // Override decoder defaults here (codecChoice, requestedSource, ignoreExif, ignoreXMP, etc)

    avifIOStreamingReader * io = avifIOCreateStreamingReader(fileBuffer, fileSize);
    if (!io) {
        fprintf(stderr, "Memory allocation failure\n");
        goto cleanup;
    }
    avifDecoderSetIO(decoder, (avifIO *)io);

    for (int pass = 0; pass < 2; ++pass) {
        // This shows the difference in how much data avifDecoderParse() needs from the file
        // depending on whether or not Exif/XMP metadata is necessary. If the caller plans to
        // interpret this metadata, avifDecoderParse() will continue to return
        // AVIF_RESULT_WAITING_ON_IO until it has those payloads in their entirety (if they exist).
        decoder->ignoreExif = decoder->ignoreXMP = (pass > 0);

        // Slowly pretend to have streamed-in / downloaded more and more bytes by incrementing io->downloadedBytes
        avifResult parseResult = AVIF_RESULT_UNKNOWN_ERROR;
        for (io->downloadedBytes = 0; io->downloadedBytes <= io->io.sizeHint; ++io->downloadedBytes) {
            parseResult = avifDecoderParse(decoder);
            if (parseResult == AVIF_RESULT_WAITING_ON_IO) {
                continue;
            }
            if (parseResult != AVIF_RESULT_OK) {
                returnCode = 1;
            }

            // See other examples on how to access the parsed information.

            printf("File: [%s @ %zu / %" PRIu64 " bytes, %s] parse returned: %s\n",
                   inputFilename,
                   io->downloadedBytes,
                   io->io.sizeHint,
                   decoder->ignoreExif ? "IgnoreMetadata" : "Metadata",
                   avifResultToString(parseResult));
            break;
        }

        if (parseResult == AVIF_RESULT_OK) {
            for (; io->downloadedBytes <= io->io.sizeHint; ++io->downloadedBytes) {
                avifResult nextImageResult = avifDecoderNextImage(decoder);
                if (nextImageResult == AVIF_RESULT_WAITING_ON_IO) {
                    continue;
                }
                if (nextImageResult != AVIF_RESULT_OK) {
                    returnCode = 1;
                }

                // See other examples on how to access the pixel content of the images.

                printf("File: [%s @ %zu / %" PRIu64 " bytes, %s] nextImage returned: %s\n",
                       inputFilename,
                       io->downloadedBytes,
                       io->io.sizeHint,
                       decoder->ignoreExif ? "IgnoreMetadata" : "Metadata",
                       avifResultToString(nextImageResult));
                break;
            }
        }
    }

    returnCode = 0;
cleanup:
    if (decoder) {
        avifDecoderDestroy(decoder); // this calls avifIOStreamingReaderDestroy for us
    }
    if (f) {
        fclose(f);
    }
    free(fileBuffer);
    return returnCode;
}
