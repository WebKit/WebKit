// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

// This is a barebones y4m reader/writer for basic libavif testing. It is NOT comprehensive!

#include "y4m.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define ADVANCE(BYTES)    \
    do {                  \
        p += BYTES;       \
        if (p >= end)     \
            goto cleanup; \
    } while (0)

avifBool y4mRead(const char * inputFilename, avifImage * avif, avifAppSourceTiming * sourceTiming, struct y4mFrameIterator ** iter)
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
    avifRWDataRealloc(&raw, Y4M_MAX_LINE_SIZE);

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
                    frame.width = atoi((const char *)p + 1);
                    break;
                case 'H': // height
                    frame.height = atoi((const char *)p + 1);
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

    avifPixelFormatInfo formatInfo;
    avifGetPixelFormatInfo(avif->yuvFormat, &formatInfo);

    const int planeCount = formatInfo.monochrome ? 1 : AVIF_PLANE_COUNT_YUV;
    for (int plane = 0; plane < planeCount; ++plane) {
        uint32_t planeWidth = (plane > 0) ? ((avif->width + formatInfo.chromaShiftX) >> formatInfo.chromaShiftX) : avif->width;
        uint32_t planeHeight = (plane > 0) ? ((avif->height + formatInfo.chromaShiftY) >> formatInfo.chromaShiftY) : avif->height;
        uint32_t rowBytes = planeWidth << (avif->depth > 8);
        uint8_t * row = avif->yuvPlanes[plane];
        for (uint32_t y = 0; y < planeHeight; ++y) {
            uint32_t bytesRead = (uint32_t)fread(row, 1, rowBytes, frame.inputFile);
            if (bytesRead != rowBytes) {
                fprintf(stderr,
                        "Failed to read y4m row (not enough data, wanted %" PRIu32 ", got %" PRIu32 "): %s\n",
                        rowBytes,
                        bytesRead,
                        frame.displayFilename);
                goto cleanup;
            }
            row += avif->yuvRowBytes[plane];
        }
    }
    if (frame.hasAlpha) {
        uint32_t rowBytes = avif->width << (avif->depth > 8);
        uint8_t * row = avif->alphaPlane;
        for (uint32_t y = 0; y < avif->height; ++y) {
            uint32_t bytesRead = (uint32_t)fread(row, 1, rowBytes, frame.inputFile);
            if (bytesRead != rowBytes) {
                fprintf(stderr,
                        "Failed to read y4m row (not enough data, wanted %" PRIu32 ", got %" PRIu32 "): %s\n",
                        rowBytes,
                        bytesRead,
                        frame.displayFilename);
                goto cleanup;
            }
            row += avif->alphaRowBytes;
        }
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
                **iter = frame;
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

    avifPixelFormatInfo formatInfo;
    avifGetPixelFormatInfo(avif->yuvFormat, &formatInfo);

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

    const int planeCount = formatInfo.monochrome ? 1 : AVIF_PLANE_COUNT_YUV;
    for (int plane = 0; plane < planeCount; ++plane) {
        uint32_t planeWidth = (plane > 0) ? ((avif->width + formatInfo.chromaShiftX) >> formatInfo.chromaShiftX) : avif->width;
        uint32_t planeHeight = (plane > 0) ? ((avif->height + formatInfo.chromaShiftY) >> formatInfo.chromaShiftY) : avif->height;
        uint32_t rowBytes = planeWidth << (avif->depth > 8);
        const uint8_t * row = avif->yuvPlanes[plane];
        for (uint32_t y = 0; y < planeHeight; ++y) {
            if (fwrite(row, 1, rowBytes, f) != rowBytes) {
                fprintf(stderr, "Failed to write %" PRIu32 " bytes: %s\n", rowBytes, outputFilename);
                success = AVIF_FALSE;
                goto cleanup;
            }
            row += avif->yuvRowBytes[plane];
        }
    }
    if (writeAlpha) {
        uint32_t rowBytes = avif->width << (avif->depth > 8);
        const uint8_t * row = avif->alphaPlane;
        for (uint32_t y = 0; y < avif->height; ++y) {
            if (fwrite(row, 1, rowBytes, f) != rowBytes) {
                fprintf(stderr, "Failed to write %" PRIu32 " bytes: %s\n", rowBytes, outputFilename);
                success = AVIF_FALSE;
                goto cleanup;
            }
            row += avif->alphaRowBytes;
        }
    }

cleanup:
    fclose(f);
    if (success) {
        printf("Wrote Y4M: %s\n", outputFilename);
    }
    return success;
}
