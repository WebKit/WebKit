// Copyright 2020 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char * argv[])
{
    if (argc != 3) {
        fprintf(stderr, "avif_example_encode [encodeYUVDirectly:0/1] [output.avif]\n");
        return 1;
    }
    avifBool encodeYUVDirectly = AVIF_FALSE;
    if (argv[1][0] == '1') {
        encodeYUVDirectly = AVIF_TRUE;
    }
    const char * outputFilename = argv[2];

    int returnCode = 1;
    avifEncoder * encoder = NULL;
    avifRWData avifOutput = AVIF_DATA_EMPTY;
    avifRGBImage rgb;
    memset(&rgb, 0, sizeof(rgb));

    avifImage * image = avifImageCreate(128, 128, 8, AVIF_PIXEL_FORMAT_YUV444); // these values dictate what goes into the final AVIF
    // Configure image here: (see avif/avif.h)
    // * colorPrimaries
    // * transferCharacteristics
    // * matrixCoefficients
    // * avifImageSetProfileICC()
    // * avifImageSetMetadataExif()
    // * avifImageSetMetadataXMP()
    // * yuvRange
    // * alphaPremultiplied
    // * transforms (transformFlags, pasp, clap, irot, imir)

    if (encodeYUVDirectly) {
        // If you have YUV(A) data you want to encode, use this path
        printf("Encoding raw YUVA data\n");

        const avifResult allocateResult = avifImageAllocatePlanes(image, AVIF_PLANES_ALL);
        if (allocateResult != AVIF_RESULT_OK) {
            fprintf(stderr, "Failed to allocate the planes: %s\n", avifResultToString(allocateResult));
            goto cleanup;
        }

        // Fill your YUV(A) data here
        memset(image->yuvPlanes[AVIF_CHAN_Y], 255, image->yuvRowBytes[AVIF_CHAN_Y] * image->height);
        memset(image->yuvPlanes[AVIF_CHAN_U], 128, image->yuvRowBytes[AVIF_CHAN_U] * image->height);
        memset(image->yuvPlanes[AVIF_CHAN_V], 128, image->yuvRowBytes[AVIF_CHAN_V] * image->height);
        memset(image->alphaPlane, 255, image->alphaRowBytes * image->height);
    } else {
        // If you have RGB(A) data you want to encode, use this path
        printf("Encoding from converted RGBA\n");

        avifRGBImageSetDefaults(&rgb, image);
        // Override RGB(A)->YUV(A) defaults here:
        //   depth, format, chromaDownsampling, avoidLibYUV, ignoreAlpha, alphaPremultiplied, etc.

        // Alternative: set rgb.pixels and rgb.rowBytes yourself, which should match your chosen rgb.format
        // Be sure to use uint16_t* instead of uint8_t* for rgb.pixels/rgb.rowBytes if (rgb.depth > 8)
        avifRGBImageAllocatePixels(&rgb);

        // Fill your RGB(A) data here
        memset(rgb.pixels, 255, rgb.rowBytes * image->height);

        avifResult convertResult = avifImageRGBToYUV(image, &rgb);
        if (convertResult != AVIF_RESULT_OK) {
            fprintf(stderr, "Failed to convert to YUV(A): %s\n", avifResultToString(convertResult));
            goto cleanup;
        }
    }

    encoder = avifEncoderCreate();
    // Configure your encoder here (see avif/avif.h):
    // * maxThreads
    // * minQuantizer
    // * maxQuantizer
    // * minQuantizerAlpha
    // * maxQuantizerAlpha
    // * tileRowsLog2
    // * tileColsLog2
    // * speed
    // * keyframeInterval
    // * timescale

    // Call avifEncoderAddImage() for each image in your sequence
    // Only set AVIF_ADD_IMAGE_FLAG_SINGLE if you're not encoding a sequence
    // Use avifEncoderAddImageGrid() instead with an array of avifImage* to make a grid image
    avifResult addImageResult = avifEncoderAddImage(encoder, image, 1, AVIF_ADD_IMAGE_FLAG_SINGLE);
    if (addImageResult != AVIF_RESULT_OK) {
        fprintf(stderr, "Failed to add image to encoder: %s\n", avifResultToString(addImageResult));
        goto cleanup;
    }

    avifResult finishResult = avifEncoderFinish(encoder, &avifOutput);
    if (finishResult != AVIF_RESULT_OK) {
        fprintf(stderr, "Failed to finish encode: %s\n", avifResultToString(finishResult));
        goto cleanup;
    }

    printf("Encode success: %zu total bytes\n", avifOutput.size);

    FILE * f = fopen(outputFilename, "wb");
    size_t bytesWritten = fwrite(avifOutput.data, 1, avifOutput.size, f);
    fclose(f);
    if (bytesWritten != avifOutput.size) {
        fprintf(stderr, "Failed to write %zu bytes\n", avifOutput.size);
        goto cleanup;
    }
    printf("Wrote: %s\n", outputFilename);

    returnCode = 0;
cleanup:
    if (image) {
        avifImageDestroy(image);
    }
    if (encoder) {
        avifEncoderDestroy(encoder);
    }
    avifRWDataFree(&avifOutput);
    avifRGBImageFreePixels(&rgb); // Only use in conjunction with avifRGBImageAllocatePixels()
    return returnCode;
}
