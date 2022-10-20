// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"

#include "avifjpeg.h"
#include "avifpng.h"
#include "avifutil.h"
#include "y4m.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_JPEG_QUALITY 90

#define NEXTARG()                                                     \
    if (((argIndex + 1) == argc) || (argv[argIndex + 1][0] == '-')) { \
        fprintf(stderr, "%s requires an argument.", arg);             \
        return 1;                                                     \
    }                                                                 \
    arg = argv[++argIndex]

static void syntax(void)
{
    printf("Syntax: avifdec [options] input.avif output.[jpg|jpeg|png|y4m]\n");
    printf("        avifdec --info    input.avif\n");
    printf("Options:\n");
    printf("    -h,--help         : Show syntax help\n");
    printf("    -V,--version      : Show the version number\n");
    printf("    -j,--jobs J       : Number of jobs (worker threads, default: 1. Use \"all\" to use all available cores)\n");
    printf("    -c,--codec C      : AV1 codec to use (choose from versions list below)\n");
    printf("    -d,--depth D      : Output depth [8,16]. (PNG only; For y4m, depth is retained, and JPEG is always 8bpc)\n");
    printf("    -q,--quality Q    : Output quality [0-100]. (JPEG only, default: %d)\n", DEFAULT_JPEG_QUALITY);
    printf("    --png-compress L  : Set PNG compression level (PNG only; 0-9, 0=none, 9=max). Defaults to libpng's builtin default.\n");
    printf("    -u,--upsampling U : Chroma upsampling (for 420/422). automatic (default), fastest, best, nearest, or bilinear\n");
    printf("    -r,--raw-color    : Output raw RGB values instead of multiplying by alpha when saving to opaque formats\n");
    printf("                        (JPEG only; not applicable to y4m)\n");
    printf("    --index I         : When decoding an image sequence or progressive image, specify which frame index to decode (Default: 0)\n");
    printf("    --progressive     : Enable progressive AVIF processing. If a progressive image is encountered and --progressive is passed,\n");
    printf("                        avifdec will use --index to choose which layer to decode (in progressive order).\n");
    printf("    --no-strict       : Disable strict decoding, which disables strict validation checks and errors\n");
    printf("    -i,--info         : Decode all frames and display all image information instead of saving to disk\n");
    printf("    --ignore-icc      : If the input file contains an embedded ICC profile, ignore it (no-op if absent)\n");
    printf("    --size-limit C    : Specifies the image size limit (in total pixels) that should be tolerated.\n");
    printf("                        Default: %u, set to a smaller value to further restrict.\n", AVIF_DEFAULT_IMAGE_SIZE_LIMIT);
    printf("  --dimension-limit C : Specifies the image dimension limit (width or height) that should be tolerated.\n");
    printf("                        Default: %u, set to 0 to ignore.\n", AVIF_DEFAULT_IMAGE_DIMENSION_LIMIT);
    printf("    --                : Signals the end of options. Everything after this is interpreted as file names.\n");
    printf("\n");
    avifPrintVersions();
}

int main(int argc, char * argv[])
{
    const char * inputFilename = NULL;
    const char * outputFilename = NULL;
    int requestedDepth = 0;
    int jobs = 1;
    int jpegQuality = DEFAULT_JPEG_QUALITY;
    int pngCompressionLevel = -1; // -1 is a sentinel to avifPNGWrite() to skip calling png_set_compression_level()
    avifCodecChoice codecChoice = AVIF_CODEC_CHOICE_AUTO;
    avifBool infoOnly = AVIF_FALSE;
    avifChromaUpsampling chromaUpsampling = AVIF_CHROMA_UPSAMPLING_AUTOMATIC;
    avifBool ignoreICC = AVIF_FALSE;
    avifBool rawColor = AVIF_FALSE;
    avifBool allowProgressive = AVIF_FALSE;
    avifStrictFlags strictFlags = AVIF_STRICT_ENABLED;
    uint32_t frameIndex = 0;
    uint32_t imageSizeLimit = AVIF_DEFAULT_IMAGE_SIZE_LIMIT;
    uint32_t imageDimensionLimit = AVIF_DEFAULT_IMAGE_DIMENSION_LIMIT;

    if (argc < 2) {
        syntax();
        return 1;
    }

    int argIndex = 1;
    while (argIndex < argc) {
        const char * arg = argv[argIndex];

        if (!strcmp(arg, "--")) {
            // Stop parsing flags, everything after this is positional arguments
            ++argIndex;
            // Parse additional positional arguments if any.
            while (argIndex < argc) {
                arg = argv[argIndex];
                if (!inputFilename) {
                    inputFilename = arg;
                } else if (!outputFilename) {
                    outputFilename = arg;
                } else {
                    fprintf(stderr, "Too many positional arguments: %s\n\n", arg);
                    syntax();
                    return 1;
                }
                ++argIndex;
            }
            break;
        } else if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            syntax();
            return 0;
        } else if (!strcmp(arg, "-V") || !strcmp(arg, "--version")) {
            avifPrintVersions();
            return 0;
        } else if (!strcmp(arg, "-j") || !strcmp(arg, "--jobs")) {
            NEXTARG();
            if (!strcmp(arg, "all")) {
                jobs = avifQueryCPUCount();
            } else {
                jobs = atoi(arg);
                if (jobs < 1) {
                    jobs = 1;
                }
            }
        } else if (!strcmp(arg, "-c") || !strcmp(arg, "--codec")) {
            NEXTARG();
            codecChoice = avifCodecChoiceFromName(arg);
            if (codecChoice == AVIF_CODEC_CHOICE_AUTO) {
                fprintf(stderr, "ERROR: Unrecognized codec: %s\n", arg);
                return 1;
            } else {
                const char * codecName = avifCodecName(codecChoice, AVIF_CODEC_FLAG_CAN_DECODE);
                if (codecName == NULL) {
                    fprintf(stderr, "ERROR: AV1 Codec cannot decode: %s\n", arg);
                    return 1;
                }
            }
        } else if (!strcmp(arg, "-d") || !strcmp(arg, "--depth")) {
            NEXTARG();
            requestedDepth = atoi(arg);
            if ((requestedDepth != 8) && (requestedDepth != 16)) {
                fprintf(stderr, "ERROR: invalid depth: %s\n", arg);
                return 1;
            }
        } else if (!strcmp(arg, "-q") || !strcmp(arg, "--quality")) {
            NEXTARG();
            jpegQuality = atoi(arg);
            if (jpegQuality < 0) {
                jpegQuality = 0;
            } else if (jpegQuality > 100) {
                jpegQuality = 100;
            }
        } else if (!strcmp(arg, "--png-compress")) {
            NEXTARG();
            pngCompressionLevel = atoi(arg);
            if (pngCompressionLevel < 0) {
                pngCompressionLevel = 0;
            } else if (pngCompressionLevel > 9) {
                pngCompressionLevel = 9;
            }
        } else if (!strcmp(arg, "-u") || !strcmp(arg, "--upsampling")) {
            NEXTARG();
            if (!strcmp(arg, "automatic")) {
                chromaUpsampling = AVIF_CHROMA_UPSAMPLING_AUTOMATIC;
            } else if (!strcmp(arg, "fastest")) {
                chromaUpsampling = AVIF_CHROMA_UPSAMPLING_FASTEST;
            } else if (!strcmp(arg, "best")) {
                chromaUpsampling = AVIF_CHROMA_UPSAMPLING_BEST_QUALITY;
            } else if (!strcmp(arg, "nearest")) {
                chromaUpsampling = AVIF_CHROMA_UPSAMPLING_NEAREST;
            } else if (!strcmp(arg, "bilinear")) {
                chromaUpsampling = AVIF_CHROMA_UPSAMPLING_BILINEAR;
            } else {
                fprintf(stderr, "ERROR: invalid upsampling: %s\n", arg);
                return 1;
            }
        } else if (!strcmp(arg, "-r") || !strcmp(arg, "--raw-color")) {
            rawColor = AVIF_TRUE;
        } else if (!strcmp(arg, "--progressive")) {
            allowProgressive = AVIF_TRUE;
        } else if (!strcmp(arg, "--index")) {
            NEXTARG();
            frameIndex = (uint32_t)atoi(arg);
        } else if (!strcmp(arg, "--no-strict")) {
            strictFlags = AVIF_STRICT_DISABLED;
        } else if (!strcmp(arg, "-i") || !strcmp(arg, "--info")) {
            infoOnly = AVIF_TRUE;
        } else if (!strcmp(arg, "--ignore-icc")) {
            ignoreICC = AVIF_TRUE;
        } else if (!strcmp(arg, "--size-limit")) {
            NEXTARG();
            unsigned long value = strtoul(arg, NULL, 10);
            if ((value > AVIF_DEFAULT_IMAGE_SIZE_LIMIT) || (value == 0)) {
                fprintf(stderr, "ERROR: invalid image size limit: %s\n", arg);
                return 1;
            }
            imageSizeLimit = (uint32_t)value;
        } else if (!strcmp(arg, "--dimension-limit")) {
            NEXTARG();
            unsigned long value = strtoul(arg, NULL, 10);
            if (value > UINT32_MAX) {
                fprintf(stderr, "ERROR: invalid image dimension limit: %s\n", arg);
                return 1;
            }
            imageDimensionLimit = (uint32_t)value;
        } else if (arg[0] == '-') {
            fprintf(stderr, "ERROR: unrecognized option %s\n\n", arg);
            syntax();
            return 1;
        } else {
            // Positional argument
            if (!inputFilename) {
                inputFilename = arg;
            } else if (!outputFilename) {
                outputFilename = arg;
            } else {
                fprintf(stderr, "Too many positional arguments: %s\n\n", arg);
                syntax();
                return 1;
            }
        }

        ++argIndex;
    }

    if (!inputFilename) {
        syntax();
        return 1;
    }

    if (infoOnly) {
        if (!inputFilename || outputFilename) {
            syntax();
            return 1;
        }

        avifDecoder * decoder = avifDecoderCreate();
        decoder->maxThreads = jobs;
        decoder->codecChoice = codecChoice;
        decoder->imageSizeLimit = imageSizeLimit;
        decoder->imageDimensionLimit = imageDimensionLimit;
        decoder->strictFlags = strictFlags;
        decoder->allowProgressive = allowProgressive;
        avifResult result = avifDecoderSetIOFile(decoder, inputFilename);
        if (result != AVIF_RESULT_OK) {
            fprintf(stderr, "Cannot open file for read: %s\n", inputFilename);
            avifDecoderDestroy(decoder);
            return 1;
        }
        result = avifDecoderParse(decoder);
        if (result == AVIF_RESULT_OK) {
            printf("Image decoded: %s\n", inputFilename);
            avifContainerDump(decoder);

            printf(" * %" PRIu64 " timescales per second, %2.2f seconds (%" PRIu64 " timescales), %d frame%s\n",
                   decoder->timescale,
                   decoder->duration,
                   decoder->durationInTimescales,
                   decoder->imageCount,
                   (decoder->imageCount == 1) ? "" : "s");
            if (decoder->imageCount > 1) {
                printf(" * %s Frames: (%u expected frames)\n",
                       (decoder->progressiveState != AVIF_PROGRESSIVE_STATE_UNAVAILABLE) ? "Progressive Image" : "Image Sequence",
                       decoder->imageCount);
            } else {
                printf(" * Frame:\n");
            }

            int currIndex = 0;
            while ((result = avifDecoderNextImage(decoder)) == AVIF_RESULT_OK) {
                printf("   * Decoded frame [%d] [pts %2.2f (%" PRIu64 " timescales)] [duration %2.2f (%" PRIu64 " timescales)] [%ux%u]\n",
                       currIndex,
                       decoder->imageTiming.pts,
                       decoder->imageTiming.ptsInTimescales,
                       decoder->imageTiming.duration,
                       decoder->imageTiming.durationInTimescales,
                       decoder->image->width,
                       decoder->image->height);
                ++currIndex;
            }
            if (result == AVIF_RESULT_NO_IMAGES_REMAINING) {
                result = AVIF_RESULT_OK;
            } else {
                fprintf(stderr, "ERROR: Failed to decode frame: %s\n", avifResultToString(result));
                avifDumpDiagnostics(&decoder->diag);
            }
        } else {
            fprintf(stderr, "ERROR: Failed to parse image: %s\n", avifResultToString(result));
            avifDumpDiagnostics(&decoder->diag);
        }

        avifDecoderDestroy(decoder);
        return result != AVIF_RESULT_OK;
    } else {
        if (!inputFilename || !outputFilename) {
            syntax();
            return 1;
        }
    }

    printf("Decoding with AV1 codec '%s' (%d worker thread%s), please wait...\n",
           avifCodecName(codecChoice, AVIF_CODEC_FLAG_CAN_DECODE),
           jobs,
           (jobs == 1) ? "" : "s");

    int returnCode = 0;
    avifDecoder * decoder = avifDecoderCreate();
    decoder->maxThreads = jobs;
    decoder->codecChoice = codecChoice;
    decoder->imageSizeLimit = imageSizeLimit;
    decoder->imageDimensionLimit = imageDimensionLimit;
    decoder->strictFlags = strictFlags;
    decoder->allowProgressive = allowProgressive;

    avifResult result = avifDecoderSetIOFile(decoder, inputFilename);
    if (result != AVIF_RESULT_OK) {
        fprintf(stderr, "Cannot open file for read: %s\n", inputFilename);
        returnCode = 1;
        goto cleanup;
    }

    result = avifDecoderParse(decoder);
    if (result != AVIF_RESULT_OK) {
        fprintf(stderr, "ERROR: Failed to parse image: %s\n", avifResultToString(result));
        returnCode = 1;
        goto cleanup;
    }

    result = avifDecoderNthImage(decoder, frameIndex);
    if (result != AVIF_RESULT_OK) {
        fprintf(stderr, "ERROR: Failed to decode image: %s\n", avifResultToString(result));
        returnCode = 1;
        goto cleanup;
    }

    printf("Image decoded: %s\n", inputFilename);
    printf("Image details:\n");
    avifImageDump(decoder->image, 0, 0, decoder->progressiveState);

    if (ignoreICC && (decoder->image->icc.size > 0)) {
        printf("[--ignore-icc] Discarding ICC profile.\n");
        avifImageSetProfileICC(decoder->image, NULL, 0);
    }

    avifAppFileFormat outputFormat = avifGuessFileFormat(outputFilename);
    if (outputFormat == AVIF_APP_FILE_FORMAT_UNKNOWN) {
        fprintf(stderr, "Cannot determine output file extension: %s\n", outputFilename);
        returnCode = 1;
    } else if (outputFormat == AVIF_APP_FILE_FORMAT_Y4M) {
        if (!y4mWrite(outputFilename, decoder->image)) {
            returnCode = 1;
        }
    } else if (outputFormat == AVIF_APP_FILE_FORMAT_JPEG) {
        // Bypass alpha multiply step during conversion
        if (rawColor) {
            decoder->image->alphaPremultiplied = AVIF_TRUE;
        }
        if (!avifJPEGWrite(outputFilename, decoder->image, jpegQuality, chromaUpsampling)) {
            returnCode = 1;
        }
    } else if (outputFormat == AVIF_APP_FILE_FORMAT_PNG) {
        if (!avifPNGWrite(outputFilename, decoder->image, requestedDepth, chromaUpsampling, pngCompressionLevel)) {
            returnCode = 1;
        }
    } else {
        fprintf(stderr, "Unsupported output file extension: %s\n", outputFilename);
        returnCode = 1;
    }

cleanup:
    if (returnCode != 0) {
        avifDumpDiagnostics(&decoder->diag);
    }
    avifDecoderDestroy(decoder);
    return returnCode;
}
