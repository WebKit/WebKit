// Copyright 2020 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char * argv[])
{
    if (argc != 2) {
        fprintf(stderr, "avif_example_decode_memory [filename.avif]\n");
        return 1;
    }
    const char * inputFilename = argv[1];

    int returnCode = 1;
    avifRGBImage rgb;
    memset(&rgb, 0, sizeof(rgb));

    avifDecoder * decoder = avifDecoderCreate();
    // Override decoder defaults here (codecChoice, requestedSource, ignoreExif, ignoreXMP, etc)

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
    }
    fseek(f, 0, SEEK_SET);
    fileBuffer = malloc(fileSize);
    long bytesRead = (long)fread(fileBuffer, 1, fileSize, f);
    if (bytesRead != fileSize) {
        fprintf(stderr, "Cannot read file: %s\n", inputFilename);
        goto cleanup;
    }

    avifResult result = avifDecoderSetIOMemory(decoder, fileBuffer, fileSize);
    if (result != AVIF_RESULT_OK) {
        fprintf(stderr, "Cannot set IO on avifDecoder: %s\n", inputFilename);
        goto cleanup;
    }

    result = avifDecoderParse(decoder);
    if (result != AVIF_RESULT_OK) {
        fprintf(stderr, "Failed to decode image: %s\n", avifResultToString(result));
        goto cleanup;
    }

    // Now available:
    // * All decoder->image information other than pixel data:
    //   * width, height, depth
    //   * transformations (pasp, clap, irot, imir)
    //   * color profile (icc, CICP)
    //   * metadata (Exif, XMP)
    // * decoder->alphaPresent
    // * number of total images in the AVIF (decoder->imageCount)
    // * overall image sequence timing (including per-frame timing with avifDecoderNthImageTiming())

    printf("Parsed AVIF: %ux%u (%ubpc)\n", decoder->image->width, decoder->image->height, decoder->image->depth);

    while (avifDecoderNextImage(decoder) == AVIF_RESULT_OK) {
        // Now available (for this frame):
        // * All decoder->image YUV pixel data (yuvFormat, yuvPlanes, yuvRange, yuvChromaSamplePosition, yuvRowBytes)
        // * decoder->image alpha data (alphaPlane, alphaRowBytes)
        // * this frame's sequence timing

        avifRGBImageSetDefaults(&rgb, decoder->image);
        // Override YUV(A)->RGB(A) defaults here:
        //   depth, format, chromaUpsampling, avoidLibYUV, ignoreAlpha, alphaPremultiplied, etc.

        // Alternative: set rgb.pixels and rgb.rowBytes yourself, which should match your chosen rgb.format
        // Be sure to use uint16_t* instead of uint8_t* for rgb.pixels/rgb.rowBytes if (rgb.depth > 8)
        avifRGBImageAllocatePixels(&rgb);

        if (avifImageYUVToRGB(decoder->image, &rgb) != AVIF_RESULT_OK) {
            fprintf(stderr, "Conversion from YUV failed: %s\n", inputFilename);
            goto cleanup;
        }

        // Now available:
        // * RGB(A) pixel data (rgb.pixels, rgb.rowBytes)

        if (rgb.depth > 8) {
            uint16_t * firstPixel = (uint16_t *)rgb.pixels;
            printf(" * First pixel: RGBA(%u,%u,%u,%u)\n", firstPixel[0], firstPixel[1], firstPixel[2], firstPixel[3]);
        } else {
            uint8_t * firstPixel = rgb.pixels;
            printf(" * First pixel: RGBA(%u,%u,%u,%u)\n", firstPixel[0], firstPixel[1], firstPixel[2], firstPixel[3]);
        }
    }

    returnCode = 0;
cleanup:
    avifRGBImageFreePixels(&rgb); // Only use in conjunction with avifRGBImageAllocatePixels()
    avifDecoderDestroy(decoder);
    if (f) {
        fclose(f);
    }
    free(fileBuffer);
    return returnCode;
}
