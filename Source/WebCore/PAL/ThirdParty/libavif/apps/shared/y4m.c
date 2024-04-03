// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

// This is a barebones y4m reader/writer for basic libavif testing. It is NOT comprehensive!

#include "y4m.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avif/avif.h"
#include "avifutil.h"

#define Y4M_MAX_LINE_SIZE 2048 // Arbitrary limit. Y4M headers should be much smaller than this

struct y4mFrameIterator
{
    int width;
    int height;
    int depth;
    avifBool hasAlpha;
    avifPixelFormat format;
    avifRange range;
    avifChromaSamplePosition chromaSamplePosition;
    avifAppSourceTiming sourceTiming;

    FILE * inputFile;
    const char * displayFilename;
};

// Sets frame->format, frame->depth, frame->hasAlpha, and frame->chromaSamplePosition.
static avifBool y4mColorSpaceParse(const char * formatString, struct y4mFrameIterator * frame)
{
    frame->hasAlpha = AVIF_FALSE;
    frame->chromaSamplePosition = AVIF_CHROMA_SAMPLE_POSITION_UNKNOWN;

    if (!strcmp(formatString, "C420jpeg")) {
        frame->format = AVIF_PIXEL_FORMAT_YUV420;
        frame->depth = 8;
        // Chroma sample position is center.
        return AVIF_TRUE;
    }
    if (!strcmp(formatString, "C420mpeg2")) {
        frame->format = AVIF_PIXEL_FORMAT_YUV420;
        frame->depth = 8;
        frame->chromaSamplePosition = AVIF_CHROMA_SAMPLE_POSITION_VERTICAL;
        return AVIF_TRUE;
    }
    if (!strcmp(formatString, "C420paldv")) {
        frame->format = AVIF_PIXEL_FORMAT_YUV420;
        frame->depth = 8;
        frame->chromaSamplePosition = AVIF_CHROMA_SAMPLE_POSITION_COLOCATED;
        return AVIF_TRUE;
    }
    if (!strcmp(formatString, "C444p10")) {
        frame->format = AVIF_PIXEL_FORMAT_YUV444;
        frame->depth = 10;
        return AVIF_TRUE;
    }
    if (!strcmp(formatString, "C422p10")) {
        frame->format = AVIF_PIXEL_FORMAT_YUV422;
        frame->depth = 10;
        return AVIF_TRUE;
    }
    if (!strcmp(formatString, "C420p10")) {
        frame->format = AVIF_PIXEL_FORMAT_YUV420;
        frame->depth = 10;
        return AVIF_TRUE;
    }
    if (!strcmp(formatString, "C422p10")) {
        frame->format = AVIF_PIXEL_FORMAT_YUV422;
        frame->depth = 10;
        return AVIF_TRUE;
    }
    if (!strcmp(formatString, "C444p12")) {
        frame->format = AVIF_PIXEL_FORMAT_YUV444;
        frame->depth = 12;
        return AVIF_TRUE;
    }
    if (!strcmp(formatString, "C422p12")) {
        frame->format = AVIF_PIXEL_FORMAT_YUV422;
        frame->depth = 12;
        return AVIF_TRUE;
    }
    if (!strcmp(formatString, "C420p12")) {
        frame->format = AVIF_PIXEL_FORMAT_YUV420;
        frame->depth = 12;
        return AVIF_TRUE;
    }
    if (!strcmp(formatString, "C422p12")) {
        frame->format = AVIF_PIXEL_FORMAT_YUV422;
        frame->depth = 12;
        return AVIF_TRUE;
    }
    if (!strcmp(formatString, "C444")) {
        frame->format = AVIF_PIXEL_FORMAT_YUV444;
        frame->depth = 8;
        return AVIF_TRUE;
    }
    if (!strcmp(formatString, "C444alpha")) {
        frame->format = AVIF_PIXEL_FORMAT_YUV444;
        frame->depth = 8;
        frame->hasAlpha = AVIF_TRUE;
        return AVIF_TRUE;
    }
    if (!strcmp(formatString, "C422")) {
        frame->format = AVIF_PIXEL_FORMAT_YUV422;
        frame->depth = 8;
        return AVIF_TRUE;
    }
    if (!strcmp(formatString, "C420")) {
        frame->format = AVIF_PIXEL_FORMAT_YUV420;
        frame->depth = 8;
        // Chroma sample position is center.
        return AVIF_TRUE;
    }
    if (!strcmp(formatString, "Cmono")) {
        frame->format = AVIF_PIXEL_FORMAT_YUV400;
        frame->depth = 8;
        return AVIF_TRUE;
    }
    if (!strcmp(formatString, "Cmono10")) {
        frame->format = AVIF_PIXEL_FORMAT_YUV400;
        frame->depth = 10;
        return AVIF_TRUE;
    }
    if (!strcmp(formatString, "Cmono12")) {
        frame->format = AVIF_PIXEL_FORMAT_YUV400;
        frame->depth = 12;
        return AVIF_TRUE;
    }
    return AVIF_FALSE;
}

// Returns an unsigned integer value parsed from [start:end[.
// Returns -1 in case of failure.
int y4mReadUnsignedInt(const char * start, const char * end)
{
    const char * p = start;
    int64_t value = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        value = value * 10 + (*(p++) - '0');
        if (value > INT_MAX) {
            return -1;
        }
    }
    return (p == start) ? -1 : (int)value;
}

// Note: this modifies framerateString
static avifBool y4mFramerateParse(char * framerateString, avifAppSourceTiming * sourceTiming)
{
    if (framerateString[0] != 'F') {
        return AVIF_FALSE;
    }
    ++framerateString; // skip past 'F'

    char * colonLocation = strchr(framerateString, ':');
    if (!colonLocation) {
        return AVIF_FALSE;
    }
    *colonLocation = 0;
    ++colonLocation;

    int numerator = atoi(framerateString);
    int denominator = atoi(colonLocation);
    if ((numerator < 1) || (denominator < 1)) {
        return AVIF_FALSE;
    }

    sourceTiming->timescale = (uint64_t)numerator;
    sourceTiming->duration = (uint64_t)denominator;
    return AVIF_TRUE;
}

static avifBool getHeaderString(uint8_t * p, uint8_t * end, char * out, size_t maxChars)
{
    uint8_t * headerEnd = p;
    while ((*headerEnd != ' ') && (*headerEnd != '\n')) {
        if (headerEnd >= end) {
            return AVIF_FALSE;
        }
        ++headerEnd;
    }
    size_t formatLen = headerEnd - p;
    if (formatLen > maxChars) {
        return AVIF_FALSE;
    }

    strncpy(out, (const char *)p, formatLen);
    out[formatLen] = 0;
    return AVIF_TRUE;
}

static int y4mReadLine(FILE * inputFile, avifRWData * raw, const char * displayFilename)
{
    static const int maxBytes = Y4M_MAX_LINE_SIZE;
    int bytesRead = 0;
    uint8_t * front = raw->data;

    for (;;) {
        if (fread(front, 1, 1, inputFile) != 1) {
            fprintf(stderr, "Failed to read line: %s\n", displayFilename);
            break;
        }

        ++bytesRead;
        if (bytesRead >= maxBytes) {
            break;
        }

        if (*front == '\n') {
            return bytesRead;
        }
        ++front;
    }
    return -1;
}

// Limits each sample value to fit into avif->depth bits.
// Returns AVIF_TRUE if any sample was clamped this way.
static avifBool y4mClampSamples(avifImage * avif)
{
    if (!avifImageUsesU16(avif)) {
        assert(avif->depth == 8);
        return AVIF_FALSE;
    }
    assert(avif->depth < 16); // Otherwise it could be skipped too.

    // AV1 encoders and decoders do not care whether the samples are full range or limited range
    // for the internal computation: it is only passed as an informative tag, so ignore avif->yuvRange.
    const uint16_t maxSampleValue = (uint16_t)((1u << avif->depth) - 1u);

    avifBool samplesWereClamped = AVIF_FALSE;
    for (int plane = AVIF_CHAN_Y; plane <= AVIF_CHAN_A; ++plane) {
        uint32_t planeHeight = avifImagePlaneHeight(avif, plane); // 0 for UV if 4:0:0.
        uint32_t planeWidth = avifImagePlaneWidth(avif, plane);
        uint8_t * row = avifImagePlane(avif, plane);
        uint32_t rowBytes = avifImagePlaneRowBytes(avif, plane);
        for (uint32_t y = 0; y < planeHeight; ++y) {
            uint16_t * row16 = (uint16_t *)row;
            for (uint32_t x = 0; x < planeWidth; ++x) {
                if (row16[x] > maxSampleValue) {
                    row16[x] = maxSampleValue;
                    samplesWereClamped = AVIF_TRUE;
                }
            }
            row += rowBytes;
        }
    }
    return samplesWereClamped;
}

#define ADVANCE(BYTES)    \
    do {                  \
        p += BYTES;       \
        if (p >= end)     \
            goto cleanup; \
    } while (0)

avifBool y4mRead(const char * inputFilename,
                 uint32_t imageSizeLimit,
                 avifImage * avif,
                 avifAppSourceTiming * sourceTiming,
                 struct y4mFrameIterator ** iter)
{
    avifBool result = AVIF_FALSE;

    struct y4mFrameIterator frame;
    frame.width = -1;
    frame.height = -1;
    // Default to the color space "C420" to match the defaults of aomenc and ffmpeg.
    frame.depth = 8;
    frame.hasAlpha = AVIF_FALSE;
    frame.format = AVIF_PIXEL_FORMAT_YUV420;
    frame.range = AVIF_RANGE_LIMITED;
    frame.chromaSamplePosition = AVIF_CHROMA_SAMPLE_POSITION_UNKNOWN;
    memset(&frame.sourceTiming, 0, sizeof(avifAppSourceTiming));
    frame.inputFile = NULL;
    frame.displayFilename = inputFilename;

    avifRWData raw = AVIF_DATA_EMPTY;
    if (avifRWDataRealloc(&raw, Y4M_MAX_LINE_SIZE) != AVIF_RESULT_OK) {
        fprintf(stderr, "Out of memory\n");
        goto cleanup;
    }

    if (iter && *iter) {
        // Continue reading FRAMEs from this y4m stream
        frame = **iter;
    } else {
        // Open a fresh y4m and read its header

        if (inputFilename) {
            frame.inputFile = fopen(inputFilename, "rb");
            if (!frame.inputFile) {
                fprintf(stderr, "Cannot open file for read: %s\n", inputFilename);
                goto cleanup;
            }
        } else {
            frame.inputFile = stdin;
            frame.displayFilename = "(stdin)";
        }

        int headerBytes = y4mReadLine(frame.inputFile, &raw, frame.displayFilename);
        if (headerBytes < 0) {
            fprintf(stderr, "Y4M header too large: %s\n", frame.displayFilename);
            goto cleanup;
        }
        if (headerBytes < 10) {
            fprintf(stderr, "Y4M header too small: %s\n", frame.displayFilename);
            goto cleanup;
        }

        uint8_t * end = raw.data + headerBytes;
        uint8_t * p = raw.data;

        if (memcmp(p, "YUV4MPEG2 ", 10) != 0) {
            fprintf(stderr, "Not a y4m file: %s\n", frame.displayFilename);
            goto cleanup;
        }
        ADVANCE(10); // skip past header

        char tmpBuffer[32];

        while (p != end) {
            switch (*p) {
                case 'W': // width
                    frame.width = y4mReadUnsignedInt((const char *)p + 1, (const char *)end);
                    break;
                case 'H': // height
                    frame.height = y4mReadUnsignedInt((const char *)p + 1, (const char *)end);
                    break;
                case 'C': // color space
                    if (!getHeaderString(p, end, tmpBuffer, 31)) {
                        fprintf(stderr, "Bad y4m header: %s\n", frame.displayFilename);
                        goto cleanup;
                    }
                    if (!y4mColorSpaceParse(tmpBuffer, &frame)) {
                        fprintf(stderr, "Unsupported y4m pixel format: %s\n", frame.displayFilename);
                        goto cleanup;
                    }
                    break;
                case 'F': // framerate
                    if (!getHeaderString(p, end, tmpBuffer, 31)) {
                        fprintf(stderr, "Bad y4m header: %s\n", frame.displayFilename);
                        goto cleanup;
                    }
                    if (!y4mFramerateParse(tmpBuffer, &frame.sourceTiming)) {
                        fprintf(stderr, "Unsupported framerate: %s\n", frame.displayFilename);
                        goto cleanup;
                    }
                    break;
                case 'X':
                    if (!getHeaderString(p, end, tmpBuffer, 31)) {
                        fprintf(stderr, "Bad y4m header: %s\n", frame.displayFilename);
                        goto cleanup;
                    }
                    if (!strcmp(tmpBuffer, "XCOLORRANGE=FULL")) {
                        frame.range = AVIF_RANGE_FULL;
                    }
                    break;
                default:
                    break;
            }

            // Advance past header section
            while ((*p != '\n') && (*p != ' ')) {
                ADVANCE(1);
            }
            if (*p == '\n') {
                // Done with y4m header
                break;
            }

            ADVANCE(1);
        }

        if (*p != '\n') {
            fprintf(stderr, "Truncated y4m header (no newline): %s\n", frame.displayFilename);
            goto cleanup;
        }
    }

    int frameHeaderBytes = y4mReadLine(frame.inputFile, &raw, frame.displayFilename);
    if (frameHeaderBytes < 0) {
        fprintf(stderr, "Y4M frame header too large: %s\n", frame.displayFilename);
        goto cleanup;
    }
    if (frameHeaderBytes < 6) {
        fprintf(stderr, "Y4M frame header too small: %s\n", frame.displayFilename);
        goto cleanup;
    }
    if (memcmp(raw.data, "FRAME", 5) != 0) {
        fprintf(stderr, "Truncated y4m (no frame): %s\n", frame.displayFilename);
        goto cleanup;
    }

    if ((frame.width < 1) || (frame.height < 1) || ((frame.depth != 8) && (frame.depth != 10) && (frame.depth != 12))) {
        fprintf(stderr, "Failed to parse y4m header (not enough information): %s\n", frame.displayFilename);
        goto cleanup;
    }
    if ((uint32_t)frame.width > imageSizeLimit / (uint32_t)frame.height) {
        fprintf(stderr, "Too big y4m dimensions (%d x %d > %u px): %s\n", frame.width, frame.height, imageSizeLimit, frame.displayFilename);
        goto cleanup;
    }

    if (sourceTiming) {
        *sourceTiming = frame.sourceTiming;
    }

    avifImageFreePlanes(avif, AVIF_PLANES_ALL);
    avif->width = frame.width;
    avif->height = frame.height;
    avif->depth = frame.depth;
    avif->yuvFormat = frame.format;
    avif->yuvRange = frame.range;
    avif->yuvChromaSamplePosition = frame.chromaSamplePosition;
    avifResult allocationResult = avifImageAllocatePlanes(avif, frame.hasAlpha ? AVIF_PLANES_ALL : AVIF_PLANES_YUV);
    if (allocationResult != AVIF_RESULT_OK) {
        fprintf(stderr, "Failed to allocate the planes: %s\n", avifResultToString(allocationResult));
        goto cleanup;
    }

    for (int plane = AVIF_CHAN_Y; plane <= AVIF_CHAN_A; ++plane) {
        uint32_t planeHeight = avifImagePlaneHeight(avif, plane); // 0 for A if no alpha and 0 for UV if 4:0:0.
        uint32_t planeWidthBytes = avifImagePlaneWidth(avif, plane) << (avif->depth > 8);
        uint8_t * row = avifImagePlane(avif, plane);
        uint32_t rowBytes = avifImagePlaneRowBytes(avif, plane);
        for (uint32_t y = 0; y < planeHeight; ++y) {
            uint32_t bytesRead = (uint32_t)fread(row, 1, planeWidthBytes, frame.inputFile);
            if (bytesRead != planeWidthBytes) {
                fprintf(stderr,
                        "Failed to read y4m row (not enough data, wanted %" PRIu32 ", got %" PRIu32 "): %s\n",
                        planeWidthBytes,
                        bytesRead,
                        frame.displayFilename);
                goto cleanup;
            }
            row += rowBytes;
        }
    }

    // libavif API does not guarantee the absence of undefined behavior if samples exceed the specified avif->depth.
    // Avoid that by making sure input values are within the correct range.
    if (y4mClampSamples(avif)) {
        fprintf(stderr, "WARNING: some samples were clamped to fit into %u bits per sample\n", avif->depth);
    }

    result = AVIF_TRUE;
cleanup:
    if (iter) {
        if (*iter) {
            free(*iter);
            *iter = NULL;
        }

        if (result && frame.inputFile) {
            ungetc(fgetc(frame.inputFile), frame.inputFile); // Kick frame.inputFile to force EOF

            if (!feof(frame.inputFile)) {
                // Remember y4m state for next time
                *iter = malloc(sizeof(struct y4mFrameIterator));
                if (*iter == NULL) {
                    fprintf(stderr, "Inter-frame state memory allocation failure\n");
                    result = AVIF_FALSE;
                } else {
                    **iter = frame;
                }
            }
        }
    }

    if (inputFilename && frame.inputFile && (!iter || !(*iter))) {
        fclose(frame.inputFile);
    }
    avifRWDataFree(&raw);
    return result;
}

avifBool y4mWrite(const char * outputFilename, const avifImage * avif)
{
    avifBool hasAlpha = (avif->alphaPlane != NULL) && (avif->alphaRowBytes > 0);
    avifBool writeAlpha = AVIF_FALSE;
    char * y4mHeaderFormat = NULL;

    if (hasAlpha && ((avif->depth != 8) || (avif->yuvFormat != AVIF_PIXEL_FORMAT_YUV444))) {
        fprintf(stderr, "WARNING: writing alpha is currently only supported in 8bpc YUV444, ignoring alpha channel: %s\n", outputFilename);
    }

    switch (avif->depth) {
        case 8:
            switch (avif->yuvFormat) {
                case AVIF_PIXEL_FORMAT_YUV444:
                    if (hasAlpha) {
                        y4mHeaderFormat = "C444alpha XYSCSS=444";
                        writeAlpha = AVIF_TRUE;
                    } else {
                        y4mHeaderFormat = "C444 XYSCSS=444";
                    }
                    break;
                case AVIF_PIXEL_FORMAT_YUV422:
                    y4mHeaderFormat = "C422 XYSCSS=422";
                    break;
                case AVIF_PIXEL_FORMAT_YUV420:
                    y4mHeaderFormat = "C420jpeg XYSCSS=420JPEG";
                    break;
                case AVIF_PIXEL_FORMAT_YUV400:
                    y4mHeaderFormat = "Cmono XYSCSS=400";
                    break;
                case AVIF_PIXEL_FORMAT_NONE:
                case AVIF_PIXEL_FORMAT_COUNT:
                    // will error later; these cases are here for warning's sake
                    break;
            }
            break;
        case 10:
            switch (avif->yuvFormat) {
                case AVIF_PIXEL_FORMAT_YUV444:
                    y4mHeaderFormat = "C444p10 XYSCSS=444P10";
                    break;
                case AVIF_PIXEL_FORMAT_YUV422:
                    y4mHeaderFormat = "C422p10 XYSCSS=422P10";
                    break;
                case AVIF_PIXEL_FORMAT_YUV420:
                    y4mHeaderFormat = "C420p10 XYSCSS=420P10";
                    break;
                case AVIF_PIXEL_FORMAT_YUV400:
                    y4mHeaderFormat = "Cmono10 XYSCSS=400";
                    break;
                case AVIF_PIXEL_FORMAT_NONE:
                case AVIF_PIXEL_FORMAT_COUNT:
                    // will error later; these cases are here for warning's sake
                    break;
            }
            break;
        case 12:
            switch (avif->yuvFormat) {
                case AVIF_PIXEL_FORMAT_YUV444:
                    y4mHeaderFormat = "C444p12 XYSCSS=444P12";
                    break;
                case AVIF_PIXEL_FORMAT_YUV422:
                    y4mHeaderFormat = "C422p12 XYSCSS=422P12";
                    break;
                case AVIF_PIXEL_FORMAT_YUV420:
                    y4mHeaderFormat = "C420p12 XYSCSS=420P12";
                    break;
                case AVIF_PIXEL_FORMAT_YUV400:
                    y4mHeaderFormat = "Cmono12 XYSCSS=400";
                    break;
                case AVIF_PIXEL_FORMAT_NONE:
                case AVIF_PIXEL_FORMAT_COUNT:
                    // will error later; these cases are here for warning's sake
                    break;
            }
            break;
        default:
            fprintf(stderr, "ERROR: y4mWrite unsupported depth: %d\n", avif->depth);
            return AVIF_FALSE;
    }

    if (y4mHeaderFormat == NULL) {
        fprintf(stderr, "ERROR: unsupported format\n");
        return AVIF_FALSE;
    }

    const char * rangeString = "XCOLORRANGE=FULL";
    if (avif->yuvRange == AVIF_RANGE_LIMITED) {
        rangeString = "XCOLORRANGE=LIMITED";
    }

    FILE * f = fopen(outputFilename, "wb");
    if (!f) {
        fprintf(stderr, "Cannot open file for write: %s\n", outputFilename);
        return AVIF_FALSE;
    }

    avifBool success = AVIF_TRUE;
    if (fprintf(f, "YUV4MPEG2 W%d H%d F25:1 Ip A0:0 %s %s\nFRAME\n", avif->width, avif->height, y4mHeaderFormat, rangeString) < 0) {
        fprintf(stderr, "Cannot write to file: %s\n", outputFilename);
        success = AVIF_FALSE;
        goto cleanup;
    }

    const int lastPlane = writeAlpha ? AVIF_CHAN_A : AVIF_CHAN_V;
    for (int plane = AVIF_CHAN_Y; plane <= lastPlane; ++plane) {
        uint32_t planeHeight = avifImagePlaneHeight(avif, plane); // 0 for UV if 4:0:0.
        uint32_t planeWidthBytes = avifImagePlaneWidth(avif, plane) << (avif->depth > 8);
        const uint8_t * row = avifImagePlane(avif, plane);
        uint32_t rowBytes = avifImagePlaneRowBytes(avif, plane);
        for (uint32_t y = 0; y < planeHeight; ++y) {
            if (fwrite(row, 1, planeWidthBytes, f) != planeWidthBytes) {
                fprintf(stderr, "Failed to write %" PRIu32 " bytes: %s\n", planeWidthBytes, outputFilename);
                success = AVIF_FALSE;
                goto cleanup;
            }
            row += rowBytes;
        }
    }

cleanup:
    fclose(f);
    if (success) {
        printf("Wrote Y4M: %s\n", outputFilename);
    }
    return success;
}
