// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"

#include "avifjpeg.h"
#include "avifpng.h"
#include "avifutil.h"
#include "y4m.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
// for setmode()
#include <fcntl.h>
#include <io.h>
#endif

#define NEXTARG()                                                     \
    if (((argIndex + 1) == argc) || (argv[argIndex + 1][0] == '-')) { \
        fprintf(stderr, "%s requires an argument.", arg);             \
        goto cleanup;                                                 \
    }                                                                 \
    arg = argv[++argIndex]

typedef struct avifInputFile
{
    const char * filename;
    uint64_t duration; // If 0, use the default duration
} avifInputFile;
static avifInputFile stdinFile;

typedef struct avifInput
{
    avifInputFile * files;
    int filesCount;
    int fileIndex;
    struct y4mFrameIterator * frameIter;
    avifPixelFormat requestedFormat;
    int requestedDepth;
    avifBool useStdin;
} avifInput;

static void syntax(void)
{
    printf("Syntax: avifenc [options] input.[jpg|jpeg|png|y4m] output.avif\n");
    printf("Options:\n");
    printf("    -h,--help                         : Show syntax help\n");
    printf("    -V,--version                      : Show the version number\n");
    printf("    -j,--jobs J                       : Number of jobs (worker threads, default: 1. Use \"all\" to use all available cores)\n");
    printf("    -o,--output FILENAME              : Instead of using the last filename given as output, use this filename\n");
    printf("    -l,--lossless                     : Set all defaults to encode losslessly, and emit warnings when settings/input don't allow for it\n");
    printf("    -d,--depth D                      : Output depth [8,10,12]. (JPEG/PNG only; For y4m or stdin, depth is retained)\n");
    printf("    -y,--yuv FORMAT                   : Output format [default=auto, 444, 422, 420, 400]. Ignored for y4m or stdin (y4m format is retained)\n");
    printf("                                        For JPEG, auto honors the JPEG's internal format, if possible. For all other cases, auto defaults to 444\n");
    printf("    -p,--premultiply                  : Premultiply color by the alpha channel and signal this in the AVIF\n");
    printf("    --sharpyuv                        : Use sharp RGB to YUV420 conversion (if supported). Ignored for y4m or if output is not 420.\n");
    printf("    --stdin                           : Read y4m frames from stdin instead of files; no input filenames allowed, must set before offering output filename\n");
    printf("    --cicp,--nclx P/T/M               : Set CICP values (nclx colr box) (3 raw numbers, use -r to set range flag)\n");
    printf("                                        P = color primaries\n");
    printf("                                        T = transfer characteristics\n");
    printf("                                        M = matrix coefficients\n");
    printf("                                        (use 2 for any you wish to leave unspecified)\n");
    printf("    -r,--range RANGE                  : YUV range [limited or l, full or f]. (JPEG/PNG only, default: full; For y4m or stdin, range is retained)\n");
    printf("    --min Q                           : Set min quantizer for color (%d-%d, where %d is lossless)\n",
           AVIF_QUANTIZER_BEST_QUALITY,
           AVIF_QUANTIZER_WORST_QUALITY,
           AVIF_QUANTIZER_LOSSLESS);
    printf("    --max Q                           : Set max quantizer for color (%d-%d, where %d is lossless)\n",
           AVIF_QUANTIZER_BEST_QUALITY,
           AVIF_QUANTIZER_WORST_QUALITY,
           AVIF_QUANTIZER_LOSSLESS);
    printf("    --minalpha Q                      : Set min quantizer for alpha (%d-%d, where %d is lossless)\n",
           AVIF_QUANTIZER_BEST_QUALITY,
           AVIF_QUANTIZER_WORST_QUALITY,
           AVIF_QUANTIZER_LOSSLESS);
    printf("    --maxalpha Q                      : Set max quantizer for alpha (%d-%d, where %d is lossless)\n",
           AVIF_QUANTIZER_BEST_QUALITY,
           AVIF_QUANTIZER_WORST_QUALITY,
           AVIF_QUANTIZER_LOSSLESS);
    printf("    --tilerowslog2 R                  : Set log2 of number of tile rows (0-6, default: 0)\n");
    printf("    --tilecolslog2 C                  : Set log2 of number of tile columns (0-6, default: 0)\n");
    printf("    --autotiling                      : Set --tilerowslog2 and --tilecolslog2 automatically\n");
    printf("    -g,--grid MxN                     : Encode a single-image grid AVIF with M cols & N rows. Either supply MxN identical W/H/D images, or a single\n");
    printf("                                        image that can be evenly split into the MxN grid and follow AVIF grid image restrictions. The grid will adopt\n");
    printf("                                        the color profile of the first image supplied.\n");
    printf("    -s,--speed S                      : Encoder speed (%d-%d, slowest-fastest, 'default' or 'd' for codec internal defaults. default speed: 6)\n",
           AVIF_SPEED_SLOWEST,
           AVIF_SPEED_FASTEST);
    printf("    -c,--codec C                      : AV1 codec to use (choose from versions list below)\n");
    printf("    --exif FILENAME                   : Provide an Exif metadata payload to be associated with the primary item (implies --ignore-exif)\n");
    printf("    --xmp FILENAME                    : Provide an XMP metadata payload to be associated with the primary item (implies --ignore-xmp)\n");
    printf("    --icc FILENAME                    : Provide an ICC profile payload to be associated with the primary item (implies --ignore-icc)\n");
    printf("    -a,--advanced KEY[=VALUE]         : Pass an advanced, codec-specific key/value string pair directly to the codec. avifenc will warn on any not used by the codec.\n");
    printf("    --duration D                      : Set all following frame durations (in timescales) to D; default 1. Can be set multiple times (before supplying each filename)\n");
    printf("    --timescale,--fps V               : Set the timescale to V. If all frames are 1 timescale in length, this is equivalent to frames per second (Default: 30)\n");
    printf("                                        If neither duration nor timescale are set, avifenc will attempt to use the framerate stored in a y4m header, if present.\n");
    printf("    -k,--keyframe INTERVAL            : Set the forced keyframe interval (maximum frames between keyframes). Set to 0 to disable (default).\n");
    printf("    --ignore-exif                     : If the input file contains embedded Exif metadata, ignore it (no-op if absent)\n");
    printf("    --ignore-xmp                      : If the input file contains embedded XMP metadata, ignore it (no-op if absent)\n");
    printf("    --ignore-icc                      : If the input file contains an embedded ICC profile, ignore it (no-op if absent)\n");
    printf("    --pasp H,V                        : Add pasp property (aspect ratio). H=horizontal spacing, V=vertical spacing\n");
    printf("    --crop CROPX,CROPY,CROPW,CROPH    : Add clap property (clean aperture), but calculated from a crop rectangle\n");
    printf("    --clap WN,WD,HN,HD,HON,HOD,VON,VOD: Add clap property (clean aperture). Width, Height, HOffset, VOffset (in num/denom pairs)\n");
    printf("    --irot ANGLE                      : Add irot property (rotation). [0-3], makes (90 * ANGLE) degree rotation anti-clockwise\n");
    printf("    --imir MODE                       : Add imir property (mirroring). 0=top-to-bottom, 1=left-to-right\n");
    printf("    --                                : Signals the end of options. Everything after this is interpreted as file names.\n");
    printf("\n");
    if (avifCodecName(AVIF_CODEC_CHOICE_AOM, 0)) {
        printf("aom-specific advanced options:\n");
        printf("    1. <key>=<value> applies to both the color (YUV) planes and the alpha plane (if present).\n");
        printf("    2. color:<key>=<value> or c:<key>=<value> applies only to the color (YUV) planes.\n");
        printf("    3. alpha:<key>=<value> or a:<key>=<value> applies only to the alpha plane (if present).\n");
        printf("       Since the alpha plane is encoded as a monochrome image, the options that refer to the chroma planes,\n");
        printf("       such as enable-chroma-deltaq=B, should not be used with the alpha plane. In addition, the film grain\n");
        printf("       options are unlikely to make sense for the alpha plane.\n");
        printf("\n");
        printf("    When used with libaom 3.0.0 or later, any key-value pairs supported by the aom_codec_set_option() function\n");
        printf("    can be used. When used with libaom 2.0.x or older, the following key-value pairs can be used:\n");
        printf("\n");
        printf("    aq-mode=M                         : Adaptive quantization mode (0: off (default), 1: variance, 2: complexity, 3: cyclic refresh)\n");
        printf("    cq-level=Q                        : Constant/Constrained Quality level (0-63, end-usage must be set to cq or q)\n");
        printf("    enable-chroma-deltaq=B            : Enable delta quantization in chroma planes (0: disable (default), 1: enable)\n");
        printf("    end-usage=MODE                    : Rate control mode (vbr, cbr, cq, or q)\n");
        printf("    sharpness=S                       : Bias towards block sharpness in rate-distortion optimization of transform coefficients (0-7, default: 0)\n");
        printf("    tune=METRIC                       : Tune the encoder for distortion metric (psnr or ssim, default: psnr)\n");
        printf("    film-grain-test=TEST              : Film grain test vectors (0: none (default), 1: test-1  2: test-2, ... 16: test-16)\n");
        printf("    film-grain-table=FILENAME         : Path to file containing film grain parameters\n");
        printf("\n");
    }
    avifPrintVersions();
}

// This is *very* arbitrary, I just want to set people's expectations a bit
static const char * quantizerString(int quantizer)
{
    if (quantizer == 0) {
        return "Lossless";
    }
    if (quantizer <= 12) {
        return "High";
    }
    if (quantizer <= 32) {
        return "Medium";
    }
    if (quantizer == AVIF_QUANTIZER_WORST_QUALITY) {
        return "Worst";
    }
    return "Low";
}

static avifBool parseCICP(int cicp[3], const char * arg)
{
    char buffer[128];
    strncpy(buffer, arg, 127);
    buffer[127] = 0;

    int index = 0;
    char * token = strtok(buffer, "/");
    while (token != NULL) {
        cicp[index] = atoi(token);
        ++index;
        if (index >= 3) {
            break;
        }

        token = strtok(NULL, "/");
    }

    if (index == 3) {
        return AVIF_TRUE;
    }
    return AVIF_FALSE;
}

// Returns the count of uint32_t (up to 8)
static int parseU32List(uint32_t output[8], const char * arg)
{
    char buffer[128];
    strncpy(buffer, arg, 127);
    buffer[127] = 0;

    int index = 0;
    char * token = strtok(buffer, ",x");
    while (token != NULL) {
        output[index] = (uint32_t)atoi(token);
        ++index;
        if (index >= 8) {
            break;
        }

        token = strtok(NULL, ",x");
    }
    return index;
}

static avifBool convertCropToClap(uint32_t srcW, uint32_t srcH, avifPixelFormat yuvFormat, uint32_t clapValues[8])
{
    avifCleanApertureBox clap;
    avifCropRect cropRect;
    cropRect.x = clapValues[0];
    cropRect.y = clapValues[1];
    cropRect.width = clapValues[2];
    cropRect.height = clapValues[3];

    avifDiagnostics diag;
    avifDiagnosticsClearError(&diag);
    avifBool convertResult = avifCleanApertureBoxConvertCropRect(&clap, &cropRect, srcW, srcH, yuvFormat, &diag);
    if (!convertResult) {
        fprintf(stderr,
                "ERROR: Impossible crop rect: imageSize:[%ux%u], pixelFormat:%s, cropRect:[%u,%u, %ux%u] - %s\n",
                srcW,
                srcH,
                avifPixelFormatToString(yuvFormat),
                cropRect.x,
                cropRect.y,
                cropRect.width,
                cropRect.height,
                diag.error);
        return convertResult;
    }

    clapValues[0] = clap.widthN;
    clapValues[1] = clap.widthD;
    clapValues[2] = clap.heightN;
    clapValues[3] = clap.heightD;
    clapValues[4] = clap.horizOffN;
    clapValues[5] = clap.horizOffD;
    clapValues[6] = clap.vertOffN;
    clapValues[7] = clap.vertOffD;
    return AVIF_TRUE;
}

static avifInputFile * avifInputGetNextFile(avifInput * input)
{
    if (input->useStdin) {
        ungetc(fgetc(stdin), stdin); // Kick stdin to force EOF

        if (feof(stdin)) {
            return NULL;
        }
        return &stdinFile;
    }

    if (input->fileIndex >= input->filesCount) {
        return NULL;
    }
    return &input->files[input->fileIndex];
}
static avifBool avifInputHasRemainingData(avifInput * input)
{
    if (input->useStdin) {
        return !feof(stdin);
    }
    return (input->fileIndex < input->filesCount);
}

static avifAppFileFormat avifInputReadImage(avifInput * input,
                                            avifBool ignoreICC,
                                            avifBool ignoreExif,
                                            avifBool ignoreXMP,
                                            avifImage * image,
                                            uint32_t * outDepth,
                                            avifAppSourceTiming * sourceTiming,
                                            avifChromaDownsampling chromaDownsampling)
{
    if (sourceTiming) {
        // A source timing of all 0s is a sentinel value hinting that the value is unset / should be
        // ignored. This is memset here as many of the paths in avifInputReadImage() do not set these
        // values. See the declaration for avifAppSourceTiming for more information.
        memset(sourceTiming, 0, sizeof(avifAppSourceTiming));
    }

    if (input->useStdin) {
        if (feof(stdin)) {
            return AVIF_APP_FILE_FORMAT_UNKNOWN;
        }
        if (!y4mRead(NULL, image, sourceTiming, &input->frameIter)) {
            return AVIF_APP_FILE_FORMAT_UNKNOWN;
        }
        assert(image->yuvFormat != AVIF_PIXEL_FORMAT_NONE);
        return AVIF_APP_FILE_FORMAT_Y4M;
    }

    if (input->fileIndex >= input->filesCount) {
        return AVIF_APP_FILE_FORMAT_UNKNOWN;
    }

    const avifAppFileFormat nextInputFormat = avifReadImage(input->files[input->fileIndex].filename,
                                                            input->requestedFormat,
                                                            input->requestedDepth,
                                                            chromaDownsampling,
                                                            ignoreICC,
                                                            ignoreExif,
                                                            ignoreXMP,
                                                            image,
                                                            outDepth,
                                                            sourceTiming,
                                                            &input->frameIter);
    if (nextInputFormat == AVIF_APP_FILE_FORMAT_UNKNOWN) {
        return AVIF_APP_FILE_FORMAT_UNKNOWN;
    }

    if (!input->frameIter) {
        ++input->fileIndex;
    }

    assert(image->yuvFormat != AVIF_PIXEL_FORMAT_NONE);
    return nextInputFormat;
}

static avifBool readEntireFile(const char * filename, avifRWData * raw)
{
    FILE * f = fopen(filename, "rb");
    if (!f) {
        return AVIF_FALSE;
    }

    fseek(f, 0, SEEK_END);
    long pos = ftell(f);
    if (pos <= 0) {
        fclose(f);
        return AVIF_FALSE;
    }
    size_t fileSize = (size_t)pos;
    fseek(f, 0, SEEK_SET);

    avifRWDataRealloc(raw, fileSize);
    size_t bytesRead = fread(raw->data, 1, fileSize, f);
    fclose(f);

    if (bytesRead != fileSize) {
        avifRWDataFree(raw);
        return AVIF_FALSE;
    }
    return AVIF_TRUE;
}

static avifBool avifImageSplitGrid(const avifImage * gridSplitImage, uint32_t gridCols, uint32_t gridRows, avifImage ** gridCells)
{
    if ((gridSplitImage->width % gridCols) != 0) {
        fprintf(stderr, "ERROR: Can't split image width (%u) evenly into %u columns.\n", gridSplitImage->width, gridCols);
        return AVIF_FALSE;
    }
    if ((gridSplitImage->height % gridRows) != 0) {
        fprintf(stderr, "ERROR: Can't split image height (%u) evenly into %u rows.\n", gridSplitImage->height, gridRows);
        return AVIF_FALSE;
    }

    uint32_t cellWidth = gridSplitImage->width / gridCols;
    uint32_t cellHeight = gridSplitImage->height / gridRows;
    if ((cellWidth < 64) || (cellHeight < 64)) {
        fprintf(stderr, "ERROR: Split cell dimensions are too small (must be at least 64x64, and were %ux%u)\n", cellWidth, cellHeight);
        return AVIF_FALSE;
    }
    if (((cellWidth % 2) != 0) || ((cellHeight % 2) != 0)) {
        fprintf(stderr, "ERROR: Odd split cell dimensions are unsupported (%ux%u)\n", cellWidth, cellHeight);
        return AVIF_FALSE;
    }

    for (uint32_t gridY = 0; gridY < gridRows; ++gridY) {
        for (uint32_t gridX = 0; gridX < gridCols; ++gridX) {
            uint32_t gridIndex = gridX + (gridY * gridCols);
            avifImage * cellImage = avifImageCreateEmpty();
            gridCells[gridIndex] = cellImage;

            const avifResult copyResult = avifImageCopy(cellImage, gridSplitImage, 0);
            if (copyResult != AVIF_RESULT_OK) {
                fprintf(stderr, "ERROR: Image copy failed: %s\n", avifResultToString(copyResult));
                return AVIF_FALSE;
            }
            cellImage->width = cellWidth;
            cellImage->height = cellHeight;

            const uint32_t bytesPerPixel = avifImageUsesU16(cellImage) ? 2 : 1;

            const uint32_t bytesPerRowY = bytesPerPixel * cellWidth;
            const uint32_t srcRowBytesY = gridSplitImage->yuvRowBytes[AVIF_CHAN_Y];
            cellImage->yuvPlanes[AVIF_CHAN_Y] =
                &gridSplitImage->yuvPlanes[AVIF_CHAN_Y][(gridX * bytesPerRowY) + (gridY * cellHeight) * srcRowBytesY];
            cellImage->yuvRowBytes[AVIF_CHAN_Y] = srcRowBytesY;

            if (gridSplitImage->yuvFormat != AVIF_PIXEL_FORMAT_YUV400) {
                avifPixelFormatInfo info;
                avifGetPixelFormatInfo(gridSplitImage->yuvFormat, &info);

                const uint32_t uvWidth = (cellWidth + info.chromaShiftX) >> info.chromaShiftX;
                const uint32_t uvHeight = (cellHeight + info.chromaShiftY) >> info.chromaShiftY;
                const uint32_t bytesPerRowUV = bytesPerPixel * uvWidth;

                const uint32_t srcRowBytesU = gridSplitImage->yuvRowBytes[AVIF_CHAN_U];
                cellImage->yuvPlanes[AVIF_CHAN_U] =
                    &gridSplitImage->yuvPlanes[AVIF_CHAN_U][(gridX * bytesPerRowUV) + (gridY * uvHeight) * srcRowBytesU];
                cellImage->yuvRowBytes[AVIF_CHAN_U] = srcRowBytesU;

                const uint32_t srcRowBytesV = gridSplitImage->yuvRowBytes[AVIF_CHAN_V];
                cellImage->yuvPlanes[AVIF_CHAN_V] =
                    &gridSplitImage->yuvPlanes[AVIF_CHAN_V][(gridX * bytesPerRowUV) + (gridY * uvHeight) * srcRowBytesV];
                cellImage->yuvRowBytes[AVIF_CHAN_V] = srcRowBytesV;
            }

            if (gridSplitImage->alphaPlane) {
                const uint32_t bytesPerRowA = bytesPerPixel * cellWidth;
                const uint32_t srcRowBytesA = gridSplitImage->alphaRowBytes;
                cellImage->alphaPlane = &gridSplitImage->alphaPlane[(gridX * bytesPerRowA) + (gridY * cellHeight) * srcRowBytesA];
                cellImage->alphaRowBytes = srcRowBytesA;
            }
        }
    }
    return AVIF_TRUE;
}

int main(int argc, char * argv[])
{
    if (argc < 2) {
        syntax();
        return 1;
    }

    const char * outputFilename = NULL;

    avifInput input;
    memset(&input, 0, sizeof(input));
    input.files = malloc(sizeof(avifInputFile) * argc);
    input.requestedFormat = AVIF_PIXEL_FORMAT_NONE; // AVIF_PIXEL_FORMAT_NONE is used as a sentinel for "auto"

    // See here for the discussion on the semi-arbitrary defaults for speed/min/max:
    //     https://github.com/AOMediaCodec/libavif/issues/440

    int returnCode = 0;
    int jobs = 1;
    int minQuantizer = 24;
    int maxQuantizer = 26;
    int minQuantizerAlpha = AVIF_QUANTIZER_LOSSLESS;
    int maxQuantizerAlpha = AVIF_QUANTIZER_LOSSLESS;
    int tileRowsLog2 = -1;
    int tileColsLog2 = -1;
    avifBool autoTiling = AVIF_FALSE;
    int speed = 6;
    int paspCount = 0;
    uint32_t paspValues[8]; // only the first two are used
    int clapCount = 0;
    uint32_t clapValues[8];
    avifBool cropConversionRequired = AVIF_FALSE;
    uint8_t irotAngle = 0xff; // sentinel value indicating "unused"
    uint8_t imirMode = 0xff;  // sentinel value indicating "unused"
    avifCodecChoice codecChoice = AVIF_CODEC_CHOICE_AUTO;
    avifRange requestedRange = AVIF_RANGE_FULL;
    avifBool lossless = AVIF_FALSE;
    avifBool ignoreExif = AVIF_FALSE;
    avifBool ignoreXMP = AVIF_FALSE;
    avifBool ignoreICC = AVIF_FALSE;
    avifEncoder * encoder = avifEncoderCreate();
    avifImage * image = NULL;
    avifImage * nextImage = NULL;
    avifRWData raw = AVIF_DATA_EMPTY;
    avifRWData exifOverride = AVIF_DATA_EMPTY;
    avifRWData xmpOverride = AVIF_DATA_EMPTY;
    avifRWData iccOverride = AVIF_DATA_EMPTY;
    int keyframeInterval = 0;
    avifBool cicpExplicitlySet = AVIF_FALSE;
    avifBool premultiplyAlpha = AVIF_FALSE;
    int gridDimsCount = 0;
    uint32_t gridDims[8]; // only the first two are used
    uint32_t gridCellCount = 0;
    avifImage ** gridCells = NULL;
    avifImage * gridSplitImage = NULL; // used for cleanup tracking
    memset(gridDims, 0, sizeof(gridDims));

    // This holds the output timing for image sequences. The timescale member in this struct will
    // become the timescale set on avifEncoder, and the duration member will be the default duration
    // for any frame that doesn't have a specific duration set on the commandline. See the
    // declaration of avifAppSourceTiming for more documentation.
    avifAppSourceTiming outputTiming = { 0, 0 };

    // By default, the color profile itself is unspecified, so CP/TC are set (to 2) accordingly.
    // However, if the end-user doesn't specify any CICP, we will convert to YUV using BT601
    // coefficients anyway (as MC:2 falls back to MC:5/6), so we might as well signal it explicitly.
    // See: ISO/IEC 23000-22:2019 Amendment 2, or the comment in avifCalcYUVCoefficients()
    avifColorPrimaries colorPrimaries = AVIF_COLOR_PRIMARIES_UNSPECIFIED;
    avifTransferCharacteristics transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED;
    avifMatrixCoefficients matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT601;
    avifChromaDownsampling chromaDownsampling = AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC;

    int argIndex = 1;
    while (argIndex < argc) {
        const char * arg = argv[argIndex];

        if (!strcmp(arg, "--")) {
            // Stop parsing flags, everything after this is positional arguments
            ++argIndex;
            // Parse additional positional arguments if any
            while (argIndex < argc) {
                arg = argv[argIndex];
                input.files[input.filesCount].filename = arg;
                input.files[input.filesCount].duration = outputTiming.duration;
                ++input.filesCount;
                ++argIndex;
            }
            break;
        } else if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            syntax();
            goto cleanup;
        } else if (!strcmp(arg, "-V") || !strcmp(arg, "--version")) {
            avifPrintVersions();
            goto cleanup;
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
        } else if (!strcmp(arg, "--stdin")) {
            input.useStdin = AVIF_TRUE;
        } else if (!strcmp(arg, "-o") || !strcmp(arg, "--output")) {
            NEXTARG();
            outputFilename = arg;
        } else if (!strcmp(arg, "-d") || !strcmp(arg, "--depth")) {
            NEXTARG();
            input.requestedDepth = atoi(arg);
            if ((input.requestedDepth != 8) && (input.requestedDepth != 10) && (input.requestedDepth != 12)) {
                fprintf(stderr, "ERROR: invalid depth: %s\n", arg);
                returnCode = 1;
                goto cleanup;
            }
        } else if (!strcmp(arg, "-y") || !strcmp(arg, "--yuv")) {
            NEXTARG();
            if (!strcmp(arg, "444")) {
                input.requestedFormat = AVIF_PIXEL_FORMAT_YUV444;
            } else if (!strcmp(arg, "422")) {
                input.requestedFormat = AVIF_PIXEL_FORMAT_YUV422;
            } else if (!strcmp(arg, "420")) {
                input.requestedFormat = AVIF_PIXEL_FORMAT_YUV420;
            } else if (!strcmp(arg, "400")) {
                input.requestedFormat = AVIF_PIXEL_FORMAT_YUV400;
            } else {
                fprintf(stderr, "ERROR: invalid format: %s\n", arg);
                returnCode = 1;
                goto cleanup;
            }
        } else if (!strcmp(arg, "-k") || !strcmp(arg, "--keyframe")) {
            NEXTARG();
            keyframeInterval = atoi(arg);
        } else if (!strcmp(arg, "--min")) {
            NEXTARG();
            minQuantizer = atoi(arg);
            if (minQuantizer < AVIF_QUANTIZER_BEST_QUALITY) {
                minQuantizer = AVIF_QUANTIZER_BEST_QUALITY;
            }
            if (minQuantizer > AVIF_QUANTIZER_WORST_QUALITY) {
                minQuantizer = AVIF_QUANTIZER_WORST_QUALITY;
            }
        } else if (!strcmp(arg, "--max")) {
            NEXTARG();
            maxQuantizer = atoi(arg);
            if (maxQuantizer < AVIF_QUANTIZER_BEST_QUALITY) {
                maxQuantizer = AVIF_QUANTIZER_BEST_QUALITY;
            }
            if (maxQuantizer > AVIF_QUANTIZER_WORST_QUALITY) {
                maxQuantizer = AVIF_QUANTIZER_WORST_QUALITY;
            }
        } else if (!strcmp(arg, "--minalpha")) {
            NEXTARG();
            minQuantizerAlpha = atoi(arg);
            if (minQuantizerAlpha < AVIF_QUANTIZER_BEST_QUALITY) {
                minQuantizerAlpha = AVIF_QUANTIZER_BEST_QUALITY;
            }
            if (minQuantizerAlpha > AVIF_QUANTIZER_WORST_QUALITY) {
                minQuantizerAlpha = AVIF_QUANTIZER_WORST_QUALITY;
            }
        } else if (!strcmp(arg, "--maxalpha")) {
            NEXTARG();
            maxQuantizerAlpha = atoi(arg);
            if (maxQuantizerAlpha < AVIF_QUANTIZER_BEST_QUALITY) {
                maxQuantizerAlpha = AVIF_QUANTIZER_BEST_QUALITY;
            }
            if (maxQuantizerAlpha > AVIF_QUANTIZER_WORST_QUALITY) {
                maxQuantizerAlpha = AVIF_QUANTIZER_WORST_QUALITY;
            }
        } else if (!strcmp(arg, "--tilerowslog2")) {
            NEXTARG();
            tileRowsLog2 = atoi(arg);
            if (tileRowsLog2 < 0) {
                tileRowsLog2 = 0;
            }
            if (tileRowsLog2 > 6) {
                tileRowsLog2 = 6;
            }
        } else if (!strcmp(arg, "--tilecolslog2")) {
            NEXTARG();
            tileColsLog2 = atoi(arg);
            if (tileColsLog2 < 0) {
                tileColsLog2 = 0;
            }
            if (tileColsLog2 > 6) {
                tileColsLog2 = 6;
            }
        } else if (!strcmp(arg, "--autotiling")) {
            autoTiling = AVIF_TRUE;
        } else if (!strcmp(arg, "-g") || !strcmp(arg, "--grid")) {
            NEXTARG();
            gridDimsCount = parseU32List(gridDims, arg);
            if (gridDimsCount != 2) {
                fprintf(stderr, "ERROR: Invalid grid dims: %s\n", arg);
                returnCode = 1;
                goto cleanup;
            }
            if ((gridDims[0] == 0) || (gridDims[0] > 256) || (gridDims[1] == 0) || (gridDims[1] > 256)) {
                fprintf(stderr, "ERROR: Invalid grid dims (valid dim range [1-256]): %s\n", arg);
                returnCode = 1;
                goto cleanup;
            }
        } else if (!strcmp(arg, "--cicp") || !strcmp(arg, "--nclx")) {
            NEXTARG();
            int cicp[3];
            if (!parseCICP(cicp, arg)) {
                returnCode = 1;
                goto cleanup;
            }
            colorPrimaries = (avifColorPrimaries)cicp[0];
            transferCharacteristics = (avifTransferCharacteristics)cicp[1];
            matrixCoefficients = (avifMatrixCoefficients)cicp[2];
            cicpExplicitlySet = AVIF_TRUE;
        } else if (!strcmp(arg, "-r") || !strcmp(arg, "--range")) {
            NEXTARG();
            if (!strcmp(arg, "limited") || !strcmp(arg, "l")) {
                requestedRange = AVIF_RANGE_LIMITED;
            } else if (!strcmp(arg, "full") || !strcmp(arg, "f")) {
                requestedRange = AVIF_RANGE_FULL;
            } else {
                fprintf(stderr, "ERROR: Unknown range: %s\n", arg);
                returnCode = 1;
                goto cleanup;
            }
        } else if (!strcmp(arg, "-s") || !strcmp(arg, "--speed")) {
            NEXTARG();
            if (!strcmp(arg, "default") || !strcmp(arg, "d")) {
                speed = AVIF_SPEED_DEFAULT;
            } else {
                speed = atoi(arg);
                if (speed > AVIF_SPEED_FASTEST) {
                    speed = AVIF_SPEED_FASTEST;
                }
                if (speed < AVIF_SPEED_SLOWEST) {
                    speed = AVIF_SPEED_SLOWEST;
                }
            }
        } else if (!strcmp(arg, "--exif")) {
            NEXTARG();
            if (!readEntireFile(arg, &exifOverride)) {
                fprintf(stderr, "ERROR: Unable to read Exif metadata: %s\n", arg);
                returnCode = 1;
                goto cleanup;
            }
            ignoreExif = AVIF_TRUE;
        } else if (!strcmp(arg, "--xmp")) {
            NEXTARG();
            if (!readEntireFile(arg, &xmpOverride)) {
                fprintf(stderr, "ERROR: Unable to read XMP metadata: %s\n", arg);
                returnCode = 1;
                goto cleanup;
            }
            ignoreXMP = AVIF_TRUE;
        } else if (!strcmp(arg, "--icc")) {
            NEXTARG();
            if (!readEntireFile(arg, &iccOverride)) {
                fprintf(stderr, "ERROR: Unable to read ICC profile: %s\n", arg);
                returnCode = 1;
                goto cleanup;
            }
            ignoreICC = AVIF_TRUE;
        } else if (!strcmp(arg, "--duration")) {
            NEXTARG();
            int durationInt = atoi(arg);
            if (durationInt < 1) {
                fprintf(stderr, "ERROR: Invalid duration: %d\n", durationInt);
                returnCode = 1;
                goto cleanup;
            }
            outputTiming.duration = (uint64_t)durationInt;
        } else if (!strcmp(arg, "--timescale") || !strcmp(arg, "--fps")) {
            NEXTARG();
            int timescaleInt = atoi(arg);
            if (timescaleInt < 1) {
                fprintf(stderr, "ERROR: Invalid timescale: %d\n", timescaleInt);
                returnCode = 1;
                goto cleanup;
            }
            outputTiming.timescale = (uint64_t)timescaleInt;
        } else if (!strcmp(arg, "-c") || !strcmp(arg, "--codec")) {
            NEXTARG();
            codecChoice = avifCodecChoiceFromName(arg);
            if (codecChoice == AVIF_CODEC_CHOICE_AUTO) {
                fprintf(stderr, "ERROR: Unrecognized codec: %s\n", arg);
                returnCode = 1;
                goto cleanup;
            } else {
                const char * codecName = avifCodecName(codecChoice, AVIF_CODEC_FLAG_CAN_ENCODE);
                if (codecName == NULL) {
                    fprintf(stderr, "ERROR: AV1 Codec cannot encode: %s\n", arg);
                    returnCode = 1;
                    goto cleanup;
                }
            }
        } else if (!strcmp(arg, "-a") || !strcmp(arg, "--advanced")) {
            NEXTARG();
            char * tempBuffer = strdup(arg);
            char * value = strchr(tempBuffer, '=');
            if (value) {
                *value = 0; // remove equals sign,
                ++value;    // and move past it

            } else {
                value = ""; // Pass in a non-NULL, empty string. Codecs can use the
                            // mere existence of a key as a boolean value.
            }
            avifEncoderSetCodecSpecificOption(encoder, tempBuffer, value);
            free(tempBuffer);
        } else if (!strcmp(arg, "--ignore-exif")) {
            ignoreExif = AVIF_TRUE;
        } else if (!strcmp(arg, "--ignore-xmp")) {
            ignoreXMP = AVIF_TRUE;
        } else if (!strcmp(arg, "--ignore-icc")) {
            ignoreICC = AVIF_TRUE;
        } else if (!strcmp(arg, "--pasp")) {
            NEXTARG();
            paspCount = parseU32List(paspValues, arg);
            if (paspCount != 2) {
                fprintf(stderr, "ERROR: Invalid pasp values: %s\n", arg);
                returnCode = 1;
                goto cleanup;
            }
        } else if (!strcmp(arg, "--crop")) {
            NEXTARG();
            clapCount = parseU32List(clapValues, arg);
            if (clapCount != 4) {
                fprintf(stderr, "ERROR: Invalid crop values: %s\n", arg);
                returnCode = 1;
                goto cleanup;
            }
            cropConversionRequired = AVIF_TRUE;
        } else if (!strcmp(arg, "--clap")) {
            NEXTARG();
            clapCount = parseU32List(clapValues, arg);
            if (clapCount != 8) {
                fprintf(stderr, "ERROR: Invalid clap values: %s\n", arg);
                returnCode = 1;
                goto cleanup;
            }
        } else if (!strcmp(arg, "--irot")) {
            NEXTARG();
            irotAngle = (uint8_t)atoi(arg);
            if (irotAngle > 3) {
                fprintf(stderr, "ERROR: Invalid irot angle: %s\n", arg);
                returnCode = 1;
                goto cleanup;
            }
        } else if (!strcmp(arg, "--imir")) {
            NEXTARG();
            imirMode = (uint8_t)atoi(arg);
            if (imirMode > 1) {
                fprintf(stderr, "ERROR: Invalid imir mode: %s\n", arg);
                returnCode = 1;
                goto cleanup;
            }
        } else if (!strcmp(arg, "-l") || !strcmp(arg, "--lossless")) {
            lossless = AVIF_TRUE;

            // Set defaults, and warn later on if anything looks incorrect
            input.requestedFormat = AVIF_PIXEL_FORMAT_YUV444; // don't subsample when using AVIF_MATRIX_COEFFICIENTS_IDENTITY
            minQuantizer = AVIF_QUANTIZER_LOSSLESS;           // lossless
            maxQuantizer = AVIF_QUANTIZER_LOSSLESS;           // lossless
            minQuantizerAlpha = AVIF_QUANTIZER_LOSSLESS;      // lossless
            maxQuantizerAlpha = AVIF_QUANTIZER_LOSSLESS;      // lossless
            codecChoice = AVIF_CODEC_CHOICE_AOM;              // rav1e doesn't support lossless transform yet:
                                                              // https://github.com/xiph/rav1e/issues/151
                                                              // SVT-AV1 doesn't support lossless encoding yet:
                                                              // https://gitlab.com/AOMediaCodec/SVT-AV1/-/issues/1636
            requestedRange = AVIF_RANGE_FULL;                 // avoid limited range
            matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_IDENTITY; // this is key for lossless
        } else if (!strcmp(arg, "-p") || !strcmp(arg, "--premultiply")) {
            premultiplyAlpha = AVIF_TRUE;
        } else if (!strcmp(arg, "--sharpyuv")) {
            chromaDownsampling = AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV;
        } else if (arg[0] == '-') {
            fprintf(stderr, "ERROR: unrecognized option %s\n\n", arg);
            syntax();
            returnCode = 1;
            goto cleanup;
        } else {
            // Positional argument
            input.files[input.filesCount].filename = arg;
            input.files[input.filesCount].duration = outputTiming.duration;
            ++input.filesCount;
        }

        ++argIndex;
    }

    stdinFile.filename = "(stdin)";
    stdinFile.duration = outputTiming.duration;

    if (!outputFilename) {
        if (((input.useStdin && (input.filesCount == 1)) || (!input.useStdin && (input.filesCount > 1)))) {
            --input.filesCount;
            outputFilename = input.files[input.filesCount].filename;
        }
    }

    if (!outputFilename || (input.useStdin && (input.filesCount > 0)) || (!input.useStdin && (input.filesCount < 1))) {
        syntax();
        returnCode = 1;
        goto cleanup;
    }

#if defined(_WIN32)
    if (input.useStdin) {
        setmode(fileno(stdin), O_BINARY);
    }
#endif

    image = avifImageCreateEmpty();

    // Set these in advance so any upcoming RGB -> YUV use the proper coefficients
    image->colorPrimaries = colorPrimaries;
    image->transferCharacteristics = transferCharacteristics;
    image->matrixCoefficients = matrixCoefficients;
    image->yuvRange = requestedRange;
    image->alphaPremultiplied = premultiplyAlpha;

    if ((image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY) && (input.requestedFormat != AVIF_PIXEL_FORMAT_NONE) &&
        (input.requestedFormat != AVIF_PIXEL_FORMAT_YUV444)) {
        // User explicitly asked for non YUV444 yuvFormat, while matrixCoefficients was likely
        // set to AVIF_MATRIX_COEFFICIENTS_IDENTITY as a side effect of --lossless,
        // and Identity is only valid with YUV444. Set matrixCoefficients back to the default.
        image->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT601;

        if (cicpExplicitlySet) {
            // Only warn if someone explicitly asked for identity.
            printf("WARNING: matrixCoefficients may not be set to identity (0) when subsampling. Resetting MC to defaults (%d).\n",
                   image->matrixCoefficients);
        }
    }

    avifInputFile * firstFile = avifInputGetNextFile(&input);
    uint32_t sourceDepth = 0;
    avifAppSourceTiming firstSourceTiming;
    avifAppFileFormat inputFormat =
        avifInputReadImage(&input, ignoreICC, ignoreExif, ignoreXMP, image, &sourceDepth, &firstSourceTiming, chromaDownsampling);
    if (inputFormat == AVIF_APP_FILE_FORMAT_UNKNOWN) {
        fprintf(stderr, "Cannot determine input file format: %s\n", firstFile->filename);
        returnCode = 1;
        goto cleanup;
    }
    avifBool sourceWasRGB = (inputFormat != AVIF_APP_FILE_FORMAT_Y4M);

    // Check again for y4m input (y4m input ignores input.requestedFormat and retains the format in file).
    if ((image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY) && (image->yuvFormat != AVIF_PIXEL_FORMAT_YUV444)) {
        fprintf(stderr, "matrixCoefficients may not be set to identity (0) when subsampling.\n");
        returnCode = 1;
        goto cleanup;
    }

    printf("Successfully loaded: %s\n", firstFile->filename);

    // Prepare image timings
    if ((outputTiming.duration == 0) && (outputTiming.timescale == 0) && (firstSourceTiming.duration > 0) &&
        (firstSourceTiming.timescale > 0)) {
        // Set the default duration and timescale to the first image's timing.
        outputTiming = firstSourceTiming;
    } else {
        // Set output timing defaults to 30 fps
        if (outputTiming.duration == 0) {
            outputTiming.duration = 1;
        }
        if (outputTiming.timescale == 0) {
            outputTiming.timescale = 30;
        }
    }

    if (iccOverride.size) {
        avifImageSetProfileICC(image, iccOverride.data, iccOverride.size);
    }
    if (exifOverride.size) {
        avifImageSetMetadataExif(image, exifOverride.data, exifOverride.size);
    }
    if (xmpOverride.size) {
        avifImageSetMetadataXMP(image, xmpOverride.data, xmpOverride.size);
    }

    if (!image->icc.size && !cicpExplicitlySet && (image->colorPrimaries == AVIF_COLOR_PRIMARIES_UNSPECIFIED) &&
        (image->transferCharacteristics == AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED)) {
        // The final image has no ICC profile, the user didn't specify any CICP, and the source
        // image didn't provide any CICP. Explicitly signal SRGB CP/TC here, as 2/2/x will be
        // interpreted as SRGB anyway.
        image->colorPrimaries = AVIF_COLOR_PRIMARIES_BT709;
        image->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_SRGB;
    }

    if (paspCount == 2) {
        image->transformFlags |= AVIF_TRANSFORM_PASP;
        image->pasp.hSpacing = paspValues[0];
        image->pasp.vSpacing = paspValues[1];
    }
    if (cropConversionRequired) {
        if (!convertCropToClap(image->width, image->height, image->yuvFormat, clapValues)) {
            returnCode = 1;
            goto cleanup;
        }
        clapCount = 8;
    }
    if (clapCount == 8) {
        image->transformFlags |= AVIF_TRANSFORM_CLAP;
        image->clap.widthN = clapValues[0];
        image->clap.widthD = clapValues[1];
        image->clap.heightN = clapValues[2];
        image->clap.heightD = clapValues[3];
        image->clap.horizOffN = clapValues[4];
        image->clap.horizOffD = clapValues[5];
        image->clap.vertOffN = clapValues[6];
        image->clap.vertOffD = clapValues[7];

        // Validate clap
        avifCropRect cropRect;
        avifDiagnostics diag;
        avifDiagnosticsClearError(&diag);
        if (!avifCropRectConvertCleanApertureBox(&cropRect, &image->clap, image->width, image->height, image->yuvFormat, &diag)) {
            fprintf(stderr,
                    "ERROR: Invalid clap: width:[%d / %d], height:[%d / %d], horizOff:[%d / %d], vertOff:[%d / %d] - %s\n",
                    (int32_t)image->clap.widthN,
                    (int32_t)image->clap.widthD,
                    (int32_t)image->clap.heightN,
                    (int32_t)image->clap.heightD,
                    (int32_t)image->clap.horizOffN,
                    (int32_t)image->clap.horizOffD,
                    (int32_t)image->clap.vertOffN,
                    (int32_t)image->clap.vertOffD,
                    diag.error);
            returnCode = 1;
            goto cleanup;
        }
    }
    if (irotAngle != 0xff) {
        image->transformFlags |= AVIF_TRANSFORM_IROT;
        image->irot.angle = irotAngle;
    }
    if (imirMode != 0xff) {
        image->transformFlags |= AVIF_TRANSFORM_IMIR;
        image->imir.mode = imirMode;
    }

    avifBool usingAOM = AVIF_FALSE;
    const char * codecName = avifCodecName(codecChoice, AVIF_CODEC_FLAG_CAN_ENCODE);
    if (codecName && !strcmp(codecName, "aom")) {
        usingAOM = AVIF_TRUE;
    }
    avifBool hasAlpha = (image->alphaPlane && image->alphaRowBytes);
    avifBool losslessColorQP = (minQuantizer == AVIF_QUANTIZER_LOSSLESS) && (maxQuantizer == AVIF_QUANTIZER_LOSSLESS);
    avifBool losslessAlphaQP = (minQuantizerAlpha == AVIF_QUANTIZER_LOSSLESS) && (maxQuantizerAlpha == AVIF_QUANTIZER_LOSSLESS);
    avifBool depthMatches = (sourceDepth == image->depth);
    avifBool using400 = (image->yuvFormat == AVIF_PIXEL_FORMAT_YUV400);
    avifBool using444 = (image->yuvFormat == AVIF_PIXEL_FORMAT_YUV444);
    avifBool usingFullRange = (image->yuvRange == AVIF_RANGE_FULL);
    avifBool usingIdentityMatrix = (image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY);

    // Guess if the enduser is asking for lossless and enable it so that warnings can be emitted
    if (!lossless && losslessColorQP && (!hasAlpha || losslessAlphaQP)) {
        // The enduser is probably expecting lossless. Turn it on and emit warnings
        printf("Min/max QPs set to %d, assuming --lossless to enable warnings on potential lossless issues.\n", AVIF_QUANTIZER_LOSSLESS);
        lossless = AVIF_TRUE;
    }

    // Check for any reasons lossless will fail, and complain loudly
    if (lossless) {
        if (!usingAOM) {
            fprintf(stderr, "WARNING: [--lossless] Only aom (-c) supports lossless transforms. Output might not be lossless.\n");
            lossless = AVIF_FALSE;
        }

        if (!losslessColorQP) {
            fprintf(stderr,
                    "WARNING: [--lossless] Color quantizer range (--min, --max) not set to %d. Color output might not be lossless.\n",
                    AVIF_QUANTIZER_LOSSLESS);
            lossless = AVIF_FALSE;
        }

        if (hasAlpha && !losslessAlphaQP) {
            fprintf(stderr,
                    "WARNING: [--lossless] Alpha present and alpha quantizer range (--minalpha, --maxalpha) not set to %d. Alpha output might not be lossless.\n",
                    AVIF_QUANTIZER_LOSSLESS);
            lossless = AVIF_FALSE;
        }

        if (!depthMatches) {
            fprintf(stderr,
                    "WARNING: [--lossless] Input depth (%d) does not match output depth (%d). Output might not be lossless.\n",
                    sourceDepth,
                    image->depth);
            lossless = AVIF_FALSE;
        }

        if (sourceWasRGB) {
            if (!using444 && !using400) {
                fprintf(stderr, "WARNING: [--lossless] Input data was RGB and YUV subsampling (-y) isn't YUV444. Output might not be lossless.\n");
                lossless = AVIF_FALSE;
            }

            if (!usingFullRange) {
                fprintf(stderr, "WARNING: [--lossless] Input data was RGB and output range (-r) isn't full. Output might not be lossless.\n");
                lossless = AVIF_FALSE;
            }

            if (!usingIdentityMatrix && !using400) {
                fprintf(stderr, "WARNING: [--lossless] Input data was RGB and matrixCoefficients isn't set to identity (--cicp x/x/0); Output might not be lossless.\n");
                lossless = AVIF_FALSE;
            }
        }
    }

    if (gridDimsCount > 0) {
        // Grid image!

        gridCellCount = gridDims[0] * gridDims[1];
        printf("Preparing to encode a %ux%u grid (%u cells)...\n", gridDims[0], gridDims[1], gridCellCount);

        gridCells = calloc(gridCellCount, sizeof(avifImage *));
        gridCells[0] = image; // take ownership of image

        uint32_t gridCellIndex = 0;
        avifInputFile * nextFile;
        while ((nextFile = avifInputGetNextFile(&input)) != NULL) {
            if (!gridCellIndex) {
                printf("Loading additional cells for grid image (%u cells)...\n", gridCellCount);
            }
            ++gridCellIndex;
            if (gridCellIndex >= gridCellCount) {
                // We have enough, warn and continue
                fprintf(stderr,
                        "WARNING: [--grid] More than %u images were supplied for this %ux%u grid. The rest will be ignored.\n",
                        gridCellCount,
                        gridDims[0],
                        gridDims[1]);
                break;
            }

            avifImage * cellImage = avifImageCreateEmpty();
            cellImage->colorPrimaries = image->colorPrimaries;
            cellImage->transferCharacteristics = image->transferCharacteristics;
            cellImage->matrixCoefficients = image->matrixCoefficients;
            cellImage->yuvRange = image->yuvRange;
            cellImage->alphaPremultiplied = image->alphaPremultiplied;
            gridCells[gridCellIndex] = cellImage;

            avifAppFileFormat nextInputFormat =
                avifInputReadImage(&input, ignoreICC, ignoreExif, ignoreXMP, cellImage, NULL, NULL, chromaDownsampling);
            if (nextInputFormat == AVIF_APP_FILE_FORMAT_UNKNOWN) {
                returnCode = 1;
                goto cleanup;
            }

            // Verify that this cell's properties matches the first cell's properties
            if ((image->width != cellImage->width) || (image->height != cellImage->height)) {
                fprintf(stderr,
                        "ERROR: Image grid dimensions mismatch, [%ux%u] vs [%ux%u]: %s\n",
                        image->width,
                        image->height,
                        cellImage->width,
                        cellImage->height,
                        nextFile->filename);
                returnCode = 1;
                goto cleanup;
            }
            if (image->depth != cellImage->depth) {
                fprintf(stderr, "ERROR: Image grid depth mismatch, [%u] vs [%u]: %s\n", image->depth, cellImage->depth, nextFile->filename);
                returnCode = 1;
                goto cleanup;
            }
            if (image->yuvRange != cellImage->yuvRange) {
                fprintf(stderr,
                        "ERROR: Image grid range mismatch, [%s] vs [%s]: %s\n",
                        (image->yuvRange == AVIF_RANGE_FULL) ? "Full" : "Limited",
                        (nextImage->yuvRange == AVIF_RANGE_FULL) ? "Full" : "Limited",
                        nextFile->filename);
                returnCode = 1;
                goto cleanup;
            }
        }

        if (gridCellIndex == 0) {
            printf("Single image input for a grid image. Attempting to split into %u cells...\n", gridCellCount);
            gridSplitImage = image;
            gridCells[0] = NULL;

            if (!avifImageSplitGrid(gridSplitImage, gridDims[0], gridDims[1], gridCells)) {
                returnCode = 1;
                goto cleanup;
            }
            gridCellIndex = gridCellCount - 1;
        }

        if (gridCellIndex != gridCellCount - 1) {
            fprintf(stderr, "ERROR: Not enough input files for grid image! (expecting %u, or a single image to be split)\n", gridCellCount);
            returnCode = 1;
            goto cleanup;
        }
    }

    const char * lossyHint = " (Lossy)";
    if (lossless) {
        lossyHint = " (Lossless)";
    }
    printf("AVIF to be written:%s\n", lossyHint);
    const avifImage * avif = gridCells ? gridCells[0] : image;
    avifImageDump(avif, gridDims[0], gridDims[1], AVIF_PROGRESSIVE_STATE_UNAVAILABLE);

    if (autoTiling) {
        if ((tileRowsLog2 >= 0) || (tileColsLog2 >= 0)) {
            fprintf(stderr, "ERROR: --autotiling is specified but --tilerowslog2 or --tilecolslog2 is also specified\n");
            returnCode = 1;
            goto cleanup;
        }
    } else {
        if (tileRowsLog2 < 0) {
            tileRowsLog2 = 0;
        }
        if (tileColsLog2 < 0) {
            tileColsLog2 = 0;
        }
    }

    char manualTilingStr[128];
    snprintf(manualTilingStr, sizeof(manualTilingStr), "tileRowsLog2 [%d], tileColsLog2 [%d]", tileRowsLog2, tileColsLog2);

    printf("Encoding with AV1 codec '%s' speed [%d], color QP [%d (%s) <-> %d (%s)], alpha QP [%d (%s) <-> %d (%s)], %s, %d worker thread(s), please wait...\n",
           avifCodecName(codecChoice, AVIF_CODEC_FLAG_CAN_ENCODE),
           speed,
           minQuantizer,
           quantizerString(minQuantizer),
           maxQuantizer,
           quantizerString(maxQuantizer),
           minQuantizerAlpha,
           quantizerString(minQuantizerAlpha),
           maxQuantizerAlpha,
           quantizerString(maxQuantizerAlpha),
           autoTiling ? "automatic tiling" : manualTilingStr,
           jobs);
    encoder->maxThreads = jobs;
    encoder->minQuantizer = minQuantizer;
    encoder->maxQuantizer = maxQuantizer;
    encoder->minQuantizerAlpha = minQuantizerAlpha;
    encoder->maxQuantizerAlpha = maxQuantizerAlpha;
    encoder->tileRowsLog2 = tileRowsLog2;
    encoder->tileColsLog2 = tileColsLog2;
    encoder->autoTiling = autoTiling;
    encoder->codecChoice = codecChoice;
    encoder->speed = speed;
    encoder->timescale = outputTiming.timescale;
    encoder->keyframeInterval = keyframeInterval;

    if (gridDimsCount > 0) {
        avifResult addImageResult =
            avifEncoderAddImageGrid(encoder, gridDims[0], gridDims[1], (const avifImage * const *)gridCells, AVIF_ADD_IMAGE_FLAG_SINGLE);
        if (addImageResult != AVIF_RESULT_OK) {
            fprintf(stderr, "ERROR: Failed to encode image grid: %s\n", avifResultToString(addImageResult));
            returnCode = 1;
            goto cleanup;
        }
    } else {
        avifAddImageFlags addImageFlags = AVIF_ADD_IMAGE_FLAG_NONE;
        if (!avifInputHasRemainingData(&input)) {
            addImageFlags |= AVIF_ADD_IMAGE_FLAG_SINGLE;
        }

        uint64_t firstDurationInTimescales = firstFile->duration ? firstFile->duration : outputTiming.duration;
        if (input.useStdin || (input.filesCount > 1)) {
            printf(" * Encoding frame 1 [%" PRIu64 "/%" PRIu64 " ts]: %s\n",
                   firstDurationInTimescales,
                   outputTiming.timescale,
                   firstFile->filename);
        }
        avifResult addImageResult = avifEncoderAddImage(encoder, image, firstDurationInTimescales, addImageFlags);
        if (addImageResult != AVIF_RESULT_OK) {
            fprintf(stderr, "ERROR: Failed to encode image: %s\n", avifResultToString(addImageResult));
            returnCode = 1;
            goto cleanup;
        }

        // Not generating a single-image grid: Use all remaining input files as subsequent frames.

        avifInputFile * nextFile;
        int nextImageIndex = -1;
        while ((nextFile = avifInputGetNextFile(&input)) != NULL) {
            ++nextImageIndex;

            uint64_t nextDurationInTimescales = nextFile->duration ? nextFile->duration : outputTiming.duration;

            printf(" * Encoding frame %d [%" PRIu64 "/%" PRIu64 " ts]: %s\n",
                   nextImageIndex + 1,
                   nextDurationInTimescales,
                   outputTiming.timescale,
                   nextFile->filename);

            if (nextImage) {
                avifImageDestroy(nextImage);
            }
            nextImage = avifImageCreateEmpty();
            nextImage->colorPrimaries = image->colorPrimaries;
            nextImage->transferCharacteristics = image->transferCharacteristics;
            nextImage->matrixCoefficients = image->matrixCoefficients;
            nextImage->yuvRange = image->yuvRange;
            nextImage->alphaPremultiplied = image->alphaPremultiplied;

            avifAppFileFormat nextInputFormat =
                avifInputReadImage(&input, ignoreICC, ignoreExif, ignoreXMP, nextImage, NULL, NULL, chromaDownsampling);
            if (nextInputFormat == AVIF_APP_FILE_FORMAT_UNKNOWN) {
                returnCode = 1;
                goto cleanup;
            }

            // Verify that this frame's properties matches the first frame's properties
            if ((image->width != nextImage->width) || (image->height != nextImage->height)) {
                fprintf(stderr,
                        "ERROR: Image sequence dimensions mismatch, [%ux%u] vs [%ux%u]: %s\n",
                        image->width,
                        image->height,
                        nextImage->width,
                        nextImage->height,
                        nextFile->filename);
                returnCode = 1;
                goto cleanup;
            }
            if (image->depth != nextImage->depth) {
                fprintf(stderr,
                        "ERROR: Image sequence depth mismatch, [%u] vs [%u]: %s\n",
                        image->depth,
                        nextImage->depth,
                        nextFile->filename);
                returnCode = 1;
                goto cleanup;
            }
            if ((image->colorPrimaries != nextImage->colorPrimaries) ||
                (image->transferCharacteristics != nextImage->transferCharacteristics) ||
                (image->matrixCoefficients != nextImage->matrixCoefficients)) {
                fprintf(stderr,
                        "ERROR: Image sequence CICP mismatch, [%u/%u/%u] vs [%u/%u/%u]: %s\n",
                        image->colorPrimaries,
                        image->matrixCoefficients,
                        image->transferCharacteristics,
                        nextImage->colorPrimaries,
                        nextImage->transferCharacteristics,
                        nextImage->matrixCoefficients,
                        nextFile->filename);
                returnCode = 1;
                goto cleanup;
            }
            if (image->yuvRange != nextImage->yuvRange) {
                fprintf(stderr,
                        "ERROR: Image sequence range mismatch, [%s] vs [%s]: %s\n",
                        (image->yuvRange == AVIF_RANGE_FULL) ? "Full" : "Limited",
                        (nextImage->yuvRange == AVIF_RANGE_FULL) ? "Full" : "Limited",
                        nextFile->filename);
                returnCode = 1;
                goto cleanup;
            }

            avifResult nextImageResult = avifEncoderAddImage(encoder, nextImage, nextDurationInTimescales, AVIF_ADD_IMAGE_FLAG_NONE);
            if (nextImageResult != AVIF_RESULT_OK) {
                fprintf(stderr, "ERROR: Failed to encode image: %s\n", avifResultToString(nextImageResult));
                returnCode = 1;
                goto cleanup;
            }
        }
    }

    avifResult finishResult = avifEncoderFinish(encoder, &raw);
    if (finishResult != AVIF_RESULT_OK) {
        fprintf(stderr, "ERROR: Failed to finish encoding: %s\n", avifResultToString(finishResult));
        returnCode = 1;
        goto cleanup;
    }

    printf("Encoded successfully.\n");
    printf(" * Color AV1 total size: " AVIF_FMT_ZU " bytes\n", encoder->ioStats.colorOBUSize);
    printf(" * Alpha AV1 total size: " AVIF_FMT_ZU " bytes\n", encoder->ioStats.alphaOBUSize);
    FILE * f = fopen(outputFilename, "wb");
    if (!f) {
        fprintf(stderr, "ERROR: Failed to open file for write: %s\n", outputFilename);
        returnCode = 1;
        goto cleanup;
    }
    if (fwrite(raw.data, 1, raw.size, f) != raw.size) {
        fprintf(stderr, "Failed to write " AVIF_FMT_ZU " bytes: %s\n", raw.size, outputFilename);
        returnCode = 1;
    } else {
        printf("Wrote AVIF: %s\n", outputFilename);
    }
    fclose(f);

cleanup:
    if (encoder) {
        if (returnCode != 0) {
            avifDumpDiagnostics(&encoder->diag);
        }
        avifEncoderDestroy(encoder);
    }
    if (gridCells) {
        for (uint32_t i = 0; i < gridCellCount; ++i) {
            if (gridCells[i]) {
                avifImageDestroy(gridCells[i]);
            }
        }
        free(gridCells);
    } else if (image) { // image is owned/cleaned up by gridCells if it exists
        avifImageDestroy(image);
    }
    if (gridSplitImage) {
        avifImageDestroy(gridSplitImage);
    }
    if (nextImage) {
        avifImageDestroy(nextImage);
    }
    avifRWDataFree(&raw);
    avifRWDataFree(&exifOverride);
    avifRWDataFree(&xmpOverride);
    avifRWDataFree(&iccOverride);
    free((void *)input.files);
    return returnCode;
}
