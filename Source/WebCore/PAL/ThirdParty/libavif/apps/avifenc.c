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

typedef struct
{
    avifCodecChoice codecChoice;
    int jobs;
    int targetSize;
    avifBool qualityIsConstrained;      // true if quality explicitly set by the user
    avifBool qualityAlphaIsConstrained; // true if qualityAlpha explicitly set by the user
    int overrideQuality;
    int overrideQualityAlpha;
    avifBool progressive; // automatic layered encoding (progressive) with single input
    avifBool layered;     // manual layered encoding by specifying each layer
    int layers;
    int speed;
    avifHeaderFormat headerFormat;

    avifBool paspPresent;
    uint32_t paspValues[2];
    avifBool clapValid; // clapValues may contain 4 crop values and need conversion. In this case clapValid is also false.
    uint32_t clapValues[8];
    avifBool gridDimsPresent;
    uint32_t gridDims[2];
    avifBool clliPresent;
    uint32_t clliValues[2];

    int repetitionCount;
    int keyframeInterval;
    avifBool ignoreExif;
    avifBool ignoreXMP;
    avifBool ignoreColorProfile;

    // These settings are only relevant when compiled with AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION
    // (which also implies AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP).
    avifBool qualityGainMapIsConstrained; // true if qualityGainMap explicitly set by the user
    int qualityGainMap;
    avifBool ignoreGainMap; // ignore any gain map present in the input file.

    // This holds the output timing for image sequences. The timescale member in this struct will
    // become the timescale set on avifEncoder, and the duration member will be the default duration
    // for any frame that doesn't have a specific duration set on the commandline. See the
    // declaration of avifAppSourceTiming for more documentation.
    avifAppSourceTiming outputTiming;

    avifBool cicpExplicitlySet;
    avifColorPrimaries colorPrimaries;
    avifTransferCharacteristics transferCharacteristics;
    avifMatrixCoefficients matrixCoefficients;
    avifChromaDownsampling chromaDownsampling;
} avifSettings;

typedef struct
{
    char ** keys;
    char ** values;
    int count;
} avifCodecSpecificOptions;

typedef struct avifSettingsEntryInt
{
    int value;
    avifBool set;
} avifSettingsEntryInt;

static avifSettingsEntryInt intSettingsEntryOf(int value)
{
    avifSettingsEntryInt entry = { value, AVIF_TRUE };
    return entry;
}

typedef avifSettingsEntryInt avifSettingsEntryBool;

static avifSettingsEntryBool boolSettingsEntryOf(avifBool value)
{
    return intSettingsEntryOf(value);
}

typedef struct avifSettingsEntryScalingMode
{
    avifScalingMode value;
    avifBool set;
} avifSettingsEntryScalingMode;

static avifSettingsEntryScalingMode scalingModeSettingsEntryOf(uint32_t n, uint32_t d)
{
    avifFraction mode = { (int32_t)n, (int32_t)d };
    avifSettingsEntryScalingMode entry = { { mode, mode }, AVIF_TRUE };
    return entry;
}

// Each entry records an "update" action to the corresponding field in avifEncoder.
// Fields in avifEncoder are not reset after encoding an image,
// so updates naturally apply to all following inputs,
// and is only recorded once on the first applicable input.
typedef struct avifInputFileSettings
{
    avifSettingsEntryInt quality;
    avifSettingsEntryInt qualityAlpha;
    avifSettingsEntryInt minQuantizer;
    avifSettingsEntryInt maxQuantizer;
    avifSettingsEntryInt minQuantizerAlpha;
    avifSettingsEntryInt maxQuantizerAlpha;
    avifSettingsEntryInt tileRowsLog2;
    avifSettingsEntryInt tileColsLog2;
    avifSettingsEntryBool autoTiling;
    avifSettingsEntryScalingMode scalingMode;

    avifCodecSpecificOptions codecSpecificOptions;
} avifInputFileSettings;

typedef struct avifInputFile
{
    const char * filename;
    uint64_t duration; // If 0, use the default duration
    avifInputFileSettings settings;
} avifInputFile;
static avifInputFile stdinFile;

typedef struct
{
    int fileIndex;
    avifImage * image;
    const avifInputFileSettings * settings;
    uint32_t fileBitDepth;
    avifBool fileIsRGB;
    avifAppSourceTiming sourceTiming;
} avifInputCacheEntry;

typedef struct avifInput
{
    avifInputFile * files;
    int filesCount;
    int fileIndex;
    struct y4mFrameIterator * frameIter;
    avifPixelFormat requestedFormat;
    int requestedDepth;
    avifBool useStdin;

    avifBool cacheEnabled;
    avifInputCacheEntry * cache;
    int cacheCount;
} avifInput;

typedef struct avifEncodedByteSizes
{
    size_t colorSizeBytes;
    size_t alphaSizeBytes;
    size_t gainMapSizeBytes;
} avifEncodedByteSizes;

static void syntaxShort(void)
{
    printf("Syntax: avifenc [options] -q quality input.[jpg|jpeg|png|y4m] output.avif\n");
    printf("where quality is between %d (worst quality) and %d (lossless).\n", AVIF_QUALITY_WORST, AVIF_QUALITY_LOSSLESS);
    printf("Typical value is 60-80.\n\n");
    printf("Try -h for an exhaustive list of options.\n");
}

static void syntaxLong(void)
{
    printf("Syntax: avifenc [options] input.[jpg|jpeg|png|y4m] output.avif\n");
    printf("Standard options:\n");
    printf("    -h,--help                         : Show syntax help (this page)\n");
    printf("    -V,--version                      : Show the version number\n");
    printf("\n");
    printf("Basic options:\n");
    printf("    -q,--qcolor Q                     : Set quality for color (%d-%d, where %d is lossless)\n",
           AVIF_QUALITY_WORST,
           AVIF_QUALITY_BEST,
           AVIF_QUALITY_LOSSLESS);
    printf("    --qalpha Q                        : Set quality for alpha (%d-%d, where %d is lossless)\n",
           AVIF_QUALITY_WORST,
           AVIF_QUALITY_BEST,
           AVIF_QUALITY_LOSSLESS);
    printf("    -s,--speed S                      : Encoder speed (%d-%d, slowest-fastest, 'default' or 'd' for codec internal defaults. default speed: 6)\n",
           AVIF_SPEED_SLOWEST,
           AVIF_SPEED_FASTEST);
    printf("\n");
    printf("Advanced options:\n");
    printf("    -j,--jobs J                       : Number of jobs (worker threads). Use \"all\" to potentially use as many cores as possible (default: all)\n");
    printf("    --no-overwrite                    : Never overwrite existing output file\n");
    printf("    -o,--output FILENAME              : Instead of using the last filename given as output, use this filename\n");
#if defined(AVIF_ENABLE_EXPERIMENTAL_AVIR)
    printf("    --avir                            : Use reduced header if possible\n");
#endif
    printf("    -l,--lossless                     : Set all defaults to encode losslessly, and emit warnings when settings/input don't allow for it\n");
    printf("    -d,--depth D                      : Output depth [8,10,12]. (JPEG/PNG only; For y4m or stdin, depth is retained)\n");
    printf("    -y,--yuv FORMAT                   : Output format [default=auto, 444, 422, 420, 400]. Ignored for y4m or stdin (y4m format is retained)\n");
    printf("                                        For JPEG, auto honors the JPEG's internal format, if possible. For grayscale PNG, auto defaults to 400. For all other cases, auto defaults to 444\n");
    printf("    -p,--premultiply                  : Premultiply color by the alpha channel and signal this in the AVIF\n");
    printf("    --sharpyuv                        : Use sharp RGB to YUV420 conversion (if supported). Ignored for y4m or if output is not 420.\n");
    printf("    --stdin                           : Read y4m frames from stdin instead of files; no input filenames allowed, must set before offering output filename\n");
    printf("    --cicp,--nclx P/T/M               : Set CICP values (nclx colr box) (3 raw numbers, use -r to set range flag)\n");
    printf("                                        P = color primaries\n");
    printf("                                        T = transfer characteristics\n");
    printf("                                        M = matrix coefficients\n");
    printf("                                        (use 2 for any you wish to leave unspecified)\n");
    printf("    -r,--range RANGE                  : YUV range [limited or l, full or f]. (JPEG/PNG only, default: full; For y4m or stdin, range is retained)\n");
    printf("    --target-size S                   : Set target file size in bytes (up to 7 times slower)\n");
    printf("    --progressive                     : EXPERIMENTAL: Auto set parameters to encode a simple layered image supporting progressive rendering from a single input frame.\n");
    printf("    --layered                         : EXPERIMENTAL: Encode a layered AVIF. Each input is encoded as one layer and at most %d layers can be encoded.\n",
           AVIF_MAX_AV1_LAYER_COUNT);
    printf("    -g,--grid MxN                     : Encode a single-image grid AVIF with M cols & N rows. Either supply MxN identical W/H/D images, or a single\n");
    printf("                                        image that can be evenly split into the MxN grid and follow AVIF grid image restrictions. The grid will adopt\n");
    printf("                                        the color profile of the first image supplied.\n");
    printf("    -c,--codec C                      : codec to use (choose from versions list below)\n");
    printf("    --exif FILENAME                   : Provide an Exif metadata payload to be associated with the primary item (implies --ignore-exif)\n");
    printf("    --xmp FILENAME                    : Provide an XMP metadata payload to be associated with the primary item (implies --ignore-xmp)\n");
    printf("    --icc FILENAME                    : Provide an ICC profile payload to be associated with the primary item (implies --ignore-icc)\n");
    printf("    --timescale,--fps V               : Set the timescale to V. If all frames are 1 timescale in length, this is equivalent to frames per second (Default: 30)\n");
    printf("                                        If neither duration nor timescale are set, avifenc will attempt to use the framerate stored in a y4m header, if present.\n");
    printf("    -k,--keyframe INTERVAL            : Set the maximum keyframe interval (any set of INTERVAL consecutive frames will have at least one keyframe). Set to 0 to disable (default).\n");
    printf("    --ignore-exif                     : If the input file contains embedded Exif metadata, ignore it (no-op if absent)\n");
    printf("    --ignore-xmp                      : If the input file contains embedded XMP metadata, ignore it (no-op if absent)\n");
    printf("    --ignore-profile,--ignore-icc     : If the input file contains an embedded color profile, ignore it (no-op if absent)\n");
#if defined(AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION)
    printf("    --ignore-gain-map                 : If the input file contains an embedded gain map, ignore it (no-op if absent)\n");
    printf("    --qgain-map Q                      : Set quality for the gain map (%d-%d, where %d is lossless)\n",
           AVIF_QUALITY_WORST,
           AVIF_QUALITY_BEST,
           AVIF_QUALITY_LOSSLESS);
    // TODO(maryla): add quality setting for the gain map.
#endif
    printf("    --pasp H,V                        : Add pasp property (aspect ratio). H=horizontal spacing, V=vertical spacing\n");
    printf("    --crop CROPX,CROPY,CROPW,CROPH    : Add clap property (clean aperture), but calculated from a crop rectangle\n");
    printf("    --clap WN,WD,HN,HD,HON,HOD,VON,VOD: Add clap property (clean aperture). Width, Height, HOffset, VOffset (in num/denom pairs)\n");
    printf("    --irot ANGLE                      : Add irot property (rotation). [0-3], makes (90 * ANGLE) degree rotation anti-clockwise\n");
    printf("    --imir AXIS                       : Add imir property (mirroring). 0=top-to-bottom, 1=left-to-right\n");
    printf("    --clli MaxCLL,MaxPALL             : Add clli property (content light level information).\n");
    printf("    --repetition-count N or infinite  : Number of times an animated image sequence will be repeated. Use 'infinite' for infinite repetitions (Default: infinite)\n");
    printf("    --                                : Signals the end of options. Everything after this is interpreted as file names.\n");
    printf("\n");
    printf("  The following options can optionally have a :u (or :update) suffix like `-q:u Q`, to apply only to input files appearing after the option:\n");
    printf("    -q,--qcolor Q                     : Set quality for color (%d-%d, where %d is lossless)\n",
           AVIF_QUALITY_WORST,
           AVIF_QUALITY_BEST,
           AVIF_QUALITY_LOSSLESS);
    printf("    --qalpha Q                        : Set quality for alpha (%d-%d, where %d is lossless)\n",
           AVIF_QUALITY_WORST,
           AVIF_QUALITY_BEST,
           AVIF_QUALITY_LOSSLESS);
    printf("    --tilerowslog2 R                  : Set log2 of number of tile rows (0-6, default: 0)\n");
    printf("    --tilecolslog2 C                  : Set log2 of number of tile columns (0-6, default: 0)\n");
    printf("    --autotiling                      : Set --tilerowslog2 and --tilecolslog2 automatically\n");
    printf("    --min QP                          : Set min quantizer for color (%d-%d, where %d is lossless)\n",
           AVIF_QUANTIZER_BEST_QUALITY,
           AVIF_QUANTIZER_WORST_QUALITY,
           AVIF_QUANTIZER_LOSSLESS);
    printf("    --max QP                          : Set max quantizer for color (%d-%d, where %d is lossless)\n",
           AVIF_QUANTIZER_BEST_QUALITY,
           AVIF_QUANTIZER_WORST_QUALITY,
           AVIF_QUANTIZER_LOSSLESS);
    printf("    --minalpha QP                     : Set min quantizer for alpha (%d-%d, where %d is lossless)\n",
           AVIF_QUANTIZER_BEST_QUALITY,
           AVIF_QUANTIZER_WORST_QUALITY,
           AVIF_QUANTIZER_LOSSLESS);
    printf("    --maxalpha QP                     : Set max quantizer for alpha (%d-%d, where %d is lossless)\n",
           AVIF_QUANTIZER_BEST_QUALITY,
           AVIF_QUANTIZER_WORST_QUALITY,
           AVIF_QUANTIZER_LOSSLESS);
    printf("    --scaling-mode N[/D]              : EXPERIMENTAL: Set frame (layer) scaling mode as given fraction. If omitted, D default to 1. (Default: 1/1)\n");
    printf("    --duration D                      : Set frame durations (in timescales) to D; default 1. This option always apply to following inputs with or without suffix.\n");
    printf("    -a,--advanced KEY[=VALUE]         : Pass an advanced, codec-specific key/value string pair directly to the codec. avifenc will warn on any not used by the codec.\n");
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
static const char * qualityString(int quality)
{
    if (quality == AVIF_QUALITY_LOSSLESS) {
        return "Lossless";
    }
    if (quality >= 80) {
        return "High";
    }
    if (quality >= 50) {
        return "Medium";
    }
    if (quality == AVIF_QUALITY_WORST) {
        return "Worst";
    }
    return "Low";
}

// Parse exactly n uint32_t from arg with separator character delim.
// Output must be able to hold at least n elements.
// Length of arg shall not exceed 127 characters or result can be truncated.
static avifBool parseU32List(uint32_t output[], int n, const char * arg, const char delim)
{
    char buffer[128];
    strncpy(buffer, arg, 127);
    buffer[127] = 0;

    // strtok wants a string for delim, so build a single character string here.
    const char delims[2] = { delim, '\0' };

    int index = 0;
    char * token = buffer;
    if (n > 1) {
        token = strtok(buffer, delims);
    }
    while (token != NULL) {
        output[index] = (uint32_t)atoi(token);
        ++index;
        if (index >= n) {
            break;
        }

        token = strtok(NULL, delims);
    }

    // Exactly n, and no more separator character
    if (index == n && strchr(token, delim) == NULL) {
        return AVIF_TRUE;
    }
    return AVIF_FALSE;
}

typedef enum avifOptionSuffixType
{
    AVIF_OPTION_SUFFIX_NONE,
    AVIF_OPTION_SUFFIX_UPDATE,
    AVIF_OPTION_SUFFIX_INVALID,
} avifOptionSuffixType;

static avifOptionSuffixType parseOptionSuffix(const char * arg, avifBool warnNoSuffix)
{
    const char * suffix = strchr(arg, ':');

    if (suffix == NULL) {
        if (warnNoSuffix) {
            fprintf(stderr,
                    "WARNING: %s is applying to all inputs. Use %s:u to apply only to inputs after it, "
                    "or move it before first input to avoid ambiguity.\n",
                    arg,
                    arg);
        }
        return AVIF_OPTION_SUFFIX_NONE;
    }

    if (!strcmp(suffix, ":u") || !strcmp(suffix, ":update")) {
        return AVIF_OPTION_SUFFIX_UPDATE;
    }

    fprintf(stderr, "ERROR: Unknown option suffix in flag %s.\n", arg);
    return AVIF_OPTION_SUFFIX_INVALID;
}

static avifBool strpre(const char * str, const char * prefix)
{
    return strncmp(str, prefix, strlen(prefix)) == 0;
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

static avifBool avifInputAddCachedImage(avifInput * input)
{
    avifImage * newImage = avifImageCreateEmpty();
    if (!newImage) {
        return AVIF_FALSE;
    }
    avifInputCacheEntry * newCachedImages = malloc((input->cacheCount + 1) * sizeof(*input->cache));
    if (!newCachedImages) {
        avifImageDestroy(newImage);
        return AVIF_FALSE;
    }
    avifInputCacheEntry * oldCachedImages = input->cache;
    input->cache = newCachedImages;
    if (input->cacheCount) {
        memcpy(input->cache, oldCachedImages, input->cacheCount * sizeof(*input->cache));
    }
    memset(&input->cache[input->cacheCount], 0, sizeof(input->cache[input->cacheCount]));
    input->cache[input->cacheCount].fileIndex = input->fileIndex;
    input->cache[input->cacheCount].image = newImage;
    ++input->cacheCount;
    free(oldCachedImages);
    return AVIF_TRUE;
}

static avifBool fileExists(const char * filename)
{
    FILE * outfile = fopen(filename, "rb");
    if (outfile) {
        fclose(outfile);
        return AVIF_TRUE;
    }
    return AVIF_FALSE;
}

static const avifInputFile * avifInputGetFile(const avifInput * input, int imageIndex)
{
    if (imageIndex < input->cacheCount) {
        return &input->files[input->cache[imageIndex].fileIndex];
    }

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

static avifBool avifInputHasRemainingData(const avifInput * input, int imageIndex)
{
    if (imageIndex < input->cacheCount) {
        return AVIF_TRUE;
    }

    if (input->useStdin) {
        return !feof(stdin);
    }
    return (input->fileIndex < input->filesCount);
}

static avifBool avifInputReadImage(avifInput * input,
                                   int imageIndex,
                                   avifBool ignoreColorProfile,
                                   avifBool ignoreExif,
                                   avifBool ignoreXMP,
                                   avifBool allowChangingCicp,
                                   avifBool ignoreGainMap,
                                   avifImage * image,
                                   const avifInputFileSettings ** settings,
                                   uint32_t * outDepth,
                                   avifBool * sourceIsRGB,
                                   avifAppSourceTiming * sourceTiming,
                                   avifChromaDownsampling chromaDownsampling)
{
    if (imageIndex < input->cacheCount) {
        const avifInputCacheEntry * cached = &input->cache[imageIndex];
        const avifCropRect rect = { 0, 0, cached->image->width, cached->image->height };
        if (avifImageSetViewRect(image, cached->image, &rect) != AVIF_RESULT_OK) {
            assert(AVIF_FALSE);
        }
#if defined(AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION)
        if (cached->image->gainMap && cached->image->gainMap->image) {
            image->gainMap->image = avifImageCreateEmpty();
            const avifCropRect gainMapRect = { 0, 0, cached->image->gainMap->image->width, cached->image->gainMap->image->height };
            if (avifImageSetViewRect(image->gainMap->image, cached->image->gainMap->image, &gainMapRect) != AVIF_RESULT_OK) {
                assert(AVIF_FALSE);
            }
        }
#endif
        if (settings) {
            *settings = cached->settings;
        }
        if (outDepth) {
            *outDepth = cached->fileBitDepth;
        }
        if (sourceIsRGB) {
            *sourceIsRGB = cached->fileIsRGB;
        }
        if (sourceTiming) {
            *sourceTiming = cached->sourceTiming;
        }
        return AVIF_TRUE;
    }

    avifImage * dstImage = image;
    const avifInputFileSettings ** dstSettings = settings;
    uint32_t * dstDepth = outDepth;
    avifBool * dstSourceIsRGB = sourceIsRGB;
    avifAppSourceTiming * dstSourceTiming = sourceTiming;
    if (input->cacheEnabled) {
        if (!avifInputAddCachedImage(input)) {
            fprintf(stderr, "ERROR: Out of memory");
            return AVIF_FALSE;
        }
        assert(imageIndex + 1 == input->cacheCount);
        dstImage = input->cache[imageIndex].image;
        // Copy CICP, clap etc.
        if (avifImageCopy(dstImage, image, /*planes=*/0) != AVIF_RESULT_OK) {
            assert(AVIF_FALSE);
        }
        dstSettings = &input->cache[imageIndex].settings;
        dstDepth = &input->cache[imageIndex].fileBitDepth;
        dstSourceIsRGB = &input->cache[imageIndex].fileIsRGB;
        dstSourceTiming = &input->cache[imageIndex].sourceTiming;
    }

    if (dstSourceTiming) {
        // A source timing of all 0s is a sentinel value hinting that the value is unset / should be
        // ignored. This is memset here as many of the paths in avifInputReadImage() do not set these
        // values. See the declaration for avifAppSourceTiming for more information.
        memset(dstSourceTiming, 0, sizeof(avifAppSourceTiming));
    }

    if (input->useStdin) {
        if (feof(stdin)) {
            return AVIF_FALSE;
        }
        if (!y4mRead(NULL, AVIF_DEFAULT_IMAGE_SIZE_LIMIT, dstImage, dstSourceTiming, &input->frameIter)) {
            fprintf(stderr, "ERROR: Cannot read y4m through standard input");
            return AVIF_FALSE;
        }
        if (dstSettings) {
            *dstSettings = &input->files[0].settings;
        }
        if (dstDepth) {
            *dstDepth = dstImage->depth;
        }
        assert(dstImage->yuvFormat != AVIF_PIXEL_FORMAT_NONE);
        if (dstSourceIsRGB) {
            *dstSourceIsRGB = AVIF_FALSE;
        }
    } else {
        if (input->fileIndex >= input->filesCount) {
            return AVIF_FALSE;
        }

        const avifInputFile * currentFile = &input->files[input->fileIndex];
        const avifAppFileFormat inputFormat = avifReadImage(currentFile->filename,
                                                            input->requestedFormat,
                                                            input->requestedDepth,
                                                            chromaDownsampling,
                                                            ignoreColorProfile,
                                                            ignoreExif,
                                                            ignoreXMP,
                                                            allowChangingCicp,
                                                            ignoreGainMap,
                                                            AVIF_DEFAULT_IMAGE_SIZE_LIMIT,
                                                            dstImage,
                                                            dstDepth,
                                                            dstSourceTiming,
                                                            &input->frameIter);
        if (inputFormat == AVIF_APP_FILE_FORMAT_UNKNOWN) {
            fprintf(stderr, "Cannot read input file: %s\n", currentFile->filename);
            return AVIF_FALSE;
        }
        if (dstSourceIsRGB) {
            *dstSourceIsRGB = (inputFormat != AVIF_APP_FILE_FORMAT_Y4M);
        }
        if (!input->frameIter) {
            ++input->fileIndex;
        }
        if (dstSettings) {
            *dstSettings = &currentFile->settings;
        }

        assert(dstImage->yuvFormat != AVIF_PIXEL_FORMAT_NONE);
    }

    if (input->cacheEnabled) {
        // Reuse the just created cache entry.
        assert(imageIndex < input->cacheCount);
        return avifInputReadImage(input,
                                  imageIndex,
                                  ignoreColorProfile,
                                  ignoreExif,
                                  ignoreXMP,
                                  allowChangingCicp,
                                  ignoreGainMap,
                                  image,
                                  settings,
                                  outDepth,
                                  sourceIsRGB,
                                  sourceTiming,
                                  chromaDownsampling);
    }
    return AVIF_TRUE;
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

    if (avifRWDataRealloc(raw, fileSize) != AVIF_RESULT_OK) {
        fclose(f);
        return AVIF_FALSE;
    }
    size_t bytesRead = fread(raw->data, 1, fileSize, f);
    fclose(f);

    if (bytesRead != fileSize) {
        avifRWDataFree(raw);
        return AVIF_FALSE;
    }
    return AVIF_TRUE;
}

// Returns NULL if a memory allocation failed. The return value should be freed with free().
static char * avifStrdup(const char * str)
{
    size_t len = strlen(str);
    char * dup = malloc(len + 1);
    if (!dup) {
        return NULL;
    }
    memcpy(dup, str, len + 1);
    return dup;
}

static avifBool avifCodecSpecificOptionsAdd(avifCodecSpecificOptions * options, const char * keyValue)
{
    avifBool success = AVIF_FALSE;
    char ** oldKeys = options->keys;
    char ** oldValues = options->values;
    options->keys = malloc((options->count + 1) * sizeof(*options->keys));
    options->values = malloc((options->count + 1) * sizeof(*options->values));
    if (!options->keys || !options->values) {
        free(options->keys);
        free(options->values);
        options->keys = oldKeys;
        options->values = oldValues;
        return AVIF_FALSE;
    }
    if (options->count) {
        memcpy(options->keys, oldKeys, options->count * sizeof(*options->keys));
        memcpy(options->values, oldValues, options->count * sizeof(*options->values));
    }

    const char * value = strchr(keyValue, '=');
    if (value) {
        // Keep the parts on the left and on the right of the equal sign,
        // but not the equal sign itself.
        options->values[options->count] = avifStrdup(value + 1);
        const size_t keyLength = strlen(keyValue) - strlen(value);
        options->keys[options->count] = malloc(keyLength + 1);
        if (!options->values[options->count] || !options->keys[options->count]) {
            goto cleanup;
        }
        memcpy(options->keys[options->count], keyValue, keyLength);
        options->keys[options->count][keyLength] = '\0';
    } else {
        // Pass in a non-NULL, empty string. Codecs can use the mere existence of a key as a boolean value.
        options->values[options->count] = avifStrdup("");
        options->keys[options->count] = avifStrdup(keyValue);
        if (!options->values[options->count] || !options->keys[options->count]) {
            goto cleanup;
        }
    }
    success = AVIF_TRUE;
cleanup:
    ++options->count;
    free(oldKeys);
    free(oldValues);
    return success;
}

static void avifCodecSpecificOptionsFree(avifCodecSpecificOptions * options)
{
    while (options->count) {
        --options->count;
        free(options->keys[options->count]);
        free(options->values[options->count]);
    }
    free(options->keys);
    free(options->values);
    options->keys = NULL;
    options->values = NULL;
}

// Returns the best cell size for a given horizontal or vertical dimension.
static avifBool avifGetBestCellSize(const char * dimensionStr, uint32_t numPixels, uint32_t numCells, avifBool isSubsampled, uint32_t * cellSize)
{
    assert(numPixels);
    assert(numCells);

    // ISO/IEC 23008-12:2017, Section 6.6.2.3.1:
    //   The reconstructed image is formed by tiling the input images into a grid with a column width
    //   (potentially excluding the right-most column) equal to tile_width and a row height (potentially
    //   excluding the bottom-most row) equal to tile_height, without gap or overlap, and then
    //   trimming on the right and the bottom to the indicated output_width and output_height.
    // The priority could be to use a cell size that is a multiple of 64, but there is not always a valid one,
    // even though it is recommended by MIAF. Just use ceil(numPixels/numCells) for simplicity and to avoid
    // as much padding in the right-most and bottom-most cells as possible.
    // Use uint64_t computation to avoid a potential uint32_t overflow.
    *cellSize = (uint32_t)(((uint64_t)numPixels + numCells - 1) / numCells);

    // ISO/IEC 23000-22:2019, Section 7.3.11.4.2:
    //   - the tile_width shall be greater than or equal to 64, and should be a multiple of 64
    //   - the tile_height shall be greater than or equal to 64, and should be a multiple of 64
    if (*cellSize < 64) {
        *cellSize = 64;
        if ((uint64_t)(numCells - 1) * *cellSize >= (uint64_t)numPixels) {
            // Some cells would be entirely off-canvas.
            fprintf(stderr, "ERROR: There are too many cells %s (%u) to have at least 64 pixels per cell.\n", dimensionStr, numCells);
            return AVIF_FALSE;
        }
    }

    // The maximum AV1 frame size is 65536 pixels inclusive.
    if (*cellSize > 65536) {
        fprintf(stderr, "ERROR: Cell size %u is bigger %s than the maximum frame size 65536.\n", *cellSize, dimensionStr);
        return AVIF_FALSE;
    }

    // ISO/IEC 23000-22:2019, Section 7.3.11.4.2:
    //   - when the images are in the 4:2:2 chroma sampling format the horizontal tile offsets and widths,
    //     and the output width, shall be even numbers;
    //   - when the images are in the 4:2:0 chroma sampling format both the horizontal and vertical tile
    //     offsets and widths, and the output width and height, shall be even numbers.
    if (isSubsampled && (*cellSize & 1)) {
        ++*cellSize;
        if ((uint64_t)(numCells - 1) * *cellSize >= (uint64_t)numPixels) {
            // Some cells would be entirely off-canvas.
            fprintf(stderr, "ERROR: Odd cell size %u is forbidden on a %s subsampled image.\n", *cellSize - 1, dimensionStr);
            return AVIF_FALSE;
        }
    }

    // Each pixel is covered by exactly one cell, and each cell contains at least one pixel.
    assert(((uint64_t)(numCells - 1) * *cellSize < (uint64_t)numPixels) && ((uint64_t)numCells * *cellSize >= (uint64_t)numPixels));
    return AVIF_TRUE;
}

static avifBool avifImageSplitGrid(const avifImage * gridSplitImage, uint32_t gridCols, uint32_t gridRows, avifImage ** gridCells)
{
    uint32_t cellWidth, cellHeight;
    avifPixelFormatInfo formatInfo;
    avifGetPixelFormatInfo(gridSplitImage->yuvFormat, &formatInfo);
    const avifBool isSubsampledX = !formatInfo.monochrome && formatInfo.chromaShiftX;
    const avifBool isSubsampledY = !formatInfo.monochrome && formatInfo.chromaShiftY;
    if (!avifGetBestCellSize("horizontally", gridSplitImage->width, gridCols, isSubsampledX, &cellWidth) ||
        !avifGetBestCellSize("vertically", gridSplitImage->height, gridRows, isSubsampledY, &cellHeight)) {
        return AVIF_FALSE;
    }

    for (uint32_t gridY = 0; gridY < gridRows; ++gridY) {
        for (uint32_t gridX = 0; gridX < gridCols; ++gridX) {
            uint32_t gridIndex = gridX + (gridY * gridCols);
            avifImage * cellImage = avifImageCreateEmpty();
            if (!cellImage) {
                fprintf(stderr, "ERROR: Cell creation failed: out of memory\n");
                return AVIF_FALSE;
            }
            gridCells[gridIndex] = cellImage;

            avifCropRect cellRect = { gridX * cellWidth, gridY * cellHeight, cellWidth, cellHeight };
            if (cellRect.x + cellRect.width > gridSplitImage->width) {
                cellRect.width = gridSplitImage->width - cellRect.x;
            }
            if (cellRect.y + cellRect.height > gridSplitImage->height) {
                cellRect.height = gridSplitImage->height - cellRect.y;
            }
            const avifResult copyResult = avifImageSetViewRect(cellImage, gridSplitImage, &cellRect);
            if (copyResult != AVIF_RESULT_OK) {
                fprintf(stderr, "ERROR: Cell creation failed: %s\n", avifResultToString(copyResult));
                return AVIF_FALSE;
            }
        }
    }
    return AVIF_TRUE;
}

#define INVALID_QUALITY (-1)
#define DEFAULT_QUALITY 60 // Maps to a quantizer (QP) of 25.
#define DEFAULT_QUALITY_ALPHA AVIF_QUALITY_LOSSLESS
#define DEFAULT_QUALITY_GAIN_MAP DEFAULT_QUALITY
#define PROGRESSIVE_WORST_QUALITY 10 // Not doing auto automatic layered encoding below this quality
#define PROGRESSIVE_START_QUALITY 2  // First layer use this quality

static avifBool avifEncodeUpdateEncoderSettings(avifEncoder * encoder, const avifInputFileSettings * settings)
{
    if (!settings) {
        return AVIF_TRUE;
    }

    if (settings->quality.set) {
        encoder->quality = settings->quality.value;
    }
    if (settings->qualityAlpha.set) {
        encoder->qualityAlpha = settings->qualityAlpha.value;
    }
    if (settings->minQuantizer.set) {
        encoder->minQuantizer = settings->minQuantizer.value;
    }
    if (settings->maxQuantizer.set) {
        encoder->maxQuantizer = settings->maxQuantizer.value;
    }
    if (settings->minQuantizerAlpha.set) {
        encoder->minQuantizerAlpha = settings->minQuantizerAlpha.value;
    }
    if (settings->maxQuantizerAlpha.set) {
        encoder->maxQuantizerAlpha = settings->maxQuantizerAlpha.value;
    }
    if (settings->tileRowsLog2.set) {
        encoder->tileRowsLog2 = settings->tileRowsLog2.value;
    }
    if (settings->tileColsLog2.set) {
        encoder->tileColsLog2 = settings->tileColsLog2.value;
    }
    if (settings->autoTiling.set) {
        encoder->autoTiling = settings->autoTiling.value;
    }
    if (settings->scalingMode.set) {
        encoder->scalingMode = settings->scalingMode.value;
    }
    for (int i = 0; i < settings->codecSpecificOptions.count; ++i) {
        if (avifEncoderSetCodecSpecificOption(encoder, settings->codecSpecificOptions.keys[i], settings->codecSpecificOptions.values[i]) !=
            AVIF_RESULT_OK) {
            fprintf(stderr,
                    "ERROR: Failed to set codec specific option: %s = %s\n",
                    settings->codecSpecificOptions.keys[i],
                    settings->codecSpecificOptions.values[i]);
            return AVIF_FALSE;
        }
    }

    return AVIF_TRUE;
}

static avifBool avifEncoderVerifyImageCompatibility(const avifImage * refImage,
                                                    const avifImage * testImage,
                                                    const char * seriesType,
                                                    const char * filename)
{
    // Verify that this frame's properties matches the first frame's properties
    if ((refImage->width != testImage->width) || (refImage->height != testImage->height)) {
        fprintf(stderr,
                "ERROR: Image %s dimensions mismatch, [%ux%u] vs [%ux%u]: %s\n",
                seriesType,
                refImage->width,
                refImage->height,
                testImage->width,
                testImage->height,
                filename);
        return AVIF_FALSE;
    }
    if (refImage->depth != testImage->depth) {
        fprintf(stderr, "ERROR: Image %s depth mismatch, [%u] vs [%u]: %s\n", seriesType, refImage->depth, testImage->depth, filename);
        return AVIF_FALSE;
    }
    if ((refImage->colorPrimaries != testImage->colorPrimaries) ||
        (refImage->transferCharacteristics != testImage->transferCharacteristics) ||
        (refImage->matrixCoefficients != testImage->matrixCoefficients)) {
        fprintf(stderr,
                "ERROR: Image %s CICP mismatch, [%u/%u/%u] vs [%u/%u/%u]: %s\n",
                seriesType,
                refImage->colorPrimaries,
                refImage->matrixCoefficients,
                refImage->transferCharacteristics,
                testImage->colorPrimaries,
                testImage->transferCharacteristics,
                testImage->matrixCoefficients,
                filename);
        return AVIF_FALSE;
    }
    if (refImage->yuvRange != testImage->yuvRange) {
        fprintf(stderr,
                "ERROR: Image %s range mismatch, [%s] vs [%s]: %s\n",
                seriesType,
                (refImage->yuvRange == AVIF_RANGE_FULL) ? "Full" : "Limited",
                (testImage->yuvRange == AVIF_RANGE_FULL) ? "Full" : "Limited",
                filename);
        return AVIF_FALSE;
    }

    return AVIF_TRUE;
}

static avifBool avifEncodeRestOfImageSequence(avifEncoder * encoder,
                                              const avifSettings * settings,
                                              avifInput * input,
                                              int imageIndex,
                                              const avifImage * firstImage)
{
    avifBool success = AVIF_FALSE;
    avifImage * nextImage = NULL;
    const avifInputFileSettings * nextSettings = NULL;

    const avifInputFile * nextFile;
    while ((nextFile = avifInputGetFile(input, imageIndex)) != NULL) {
        uint64_t nextDurationInTimescales = nextFile->duration ? nextFile->duration : settings->outputTiming.duration;

        printf(" * Encoding frame %d [%" PRIu64 "/%" PRIu64 " ts]: %s\n",
               imageIndex,
               nextDurationInTimescales,
               settings->outputTiming.timescale,
               nextFile->filename);

        if (nextImage) {
            avifImageDestroy(nextImage);
        }
        nextImage = avifImageCreateEmpty();
        if (!nextImage) {
            fprintf(stderr, "ERROR: Out of memory\n");
            goto cleanup;
        }
        nextImage->colorPrimaries = firstImage->colorPrimaries;
        nextImage->transferCharacteristics = firstImage->transferCharacteristics;
        nextImage->matrixCoefficients = firstImage->matrixCoefficients;
        nextImage->yuvRange = firstImage->yuvRange;
        nextImage->alphaPremultiplied = firstImage->alphaPremultiplied;

        // Ignore ICC, Exif and XMP because only the metadata of the first frame is taken into
        // account by the libavif API.
        // Ignore gain map as it's not supported for sequences.
        if (!avifInputReadImage(input,
                                imageIndex,
                                /*ignoreColorProfile=*/AVIF_TRUE,
                                /*ignoreExif=*/AVIF_TRUE,
                                /*ignoreXMP=*/AVIF_TRUE,
                                /*allowChangingCicp=*/AVIF_FALSE,
                                /*ignoreGainMap=*/AVIF_TRUE,
                                nextImage,
                                &nextSettings,
                                /*outDepth=*/NULL,
                                /*sourceIsRGB=*/NULL,
                                /*sourceTiming=*/NULL,
                                settings->chromaDownsampling)) {
            goto cleanup;
        }
        if (!avifEncoderVerifyImageCompatibility(firstImage, nextImage, "sequence", nextFile->filename)) {
            goto cleanup;
        }
        if (!avifEncodeUpdateEncoderSettings(encoder, nextSettings)) {
            goto cleanup;
        }
        const avifResult nextImageResult = avifEncoderAddImage(encoder, nextImage, nextDurationInTimescales, AVIF_ADD_IMAGE_FLAG_NONE);
        if (nextImageResult != AVIF_RESULT_OK) {
            fprintf(stderr, "ERROR: Failed to encode image: %s\n", avifResultToString(nextImageResult));
            goto cleanup;
        }
        ++imageIndex;
    }
    success = AVIF_TRUE;

cleanup:
    if (nextImage) {
        avifImageDestroy(nextImage);
    }
    return success;
}

static avifBool avifEncodeRestOfLayeredImage(avifEncoder * encoder,
                                             const avifSettings * settings,
                                             avifInput * input,
                                             int layerIndex,
                                             const avifImage * firstImage)
{
    avifBool success = AVIF_FALSE;
    int layers = encoder->extraLayerCount + 1;
    // --progressive only allows one input, so directly read from it.
    int targetQuality = (settings->overrideQuality != INVALID_QUALITY) ? settings->overrideQuality
                                                                       : input->files[0].settings.quality.value;

    avifImage * nextImage = NULL;
    const avifImage * encodingImage = firstImage;
    const avifInputFileSettings * nextSettings = NULL;

    if (settings->progressive && avifInputHasRemainingData(input, layerIndex)) {
        fprintf(stderr, "ERROR: Automatic layered encoding can only have one input image.\n");
        goto cleanup;
    }

    while (layerIndex < layers) {
        if (settings->progressive) {
            // reversed lerp, so that last layer reaches exact targetQuality
            encoder->quality = targetQuality - (targetQuality - PROGRESSIVE_START_QUALITY) *
                                                   (encoder->extraLayerCount - layerIndex) / encoder->extraLayerCount;
        } else {
            const avifInputFile * nextFile = avifInputGetFile(input, layerIndex);
            // main() function should set number of layers to number of input,
            // so nextFile should not be NULL.
            assert(nextFile);

            if (nextImage) {
                avifImageDestroy(nextImage);
            }
            nextImage = avifImageCreateEmpty();
            if (!nextImage) {
                fprintf(stderr, "ERROR: Out of memory\n");
                goto cleanup;
            }
            nextImage->colorPrimaries = firstImage->colorPrimaries;
            nextImage->transferCharacteristics = firstImage->transferCharacteristics;
            nextImage->matrixCoefficients = firstImage->matrixCoefficients;
            nextImage->yuvRange = firstImage->yuvRange;
            nextImage->alphaPremultiplied = firstImage->alphaPremultiplied;

            // Ignore ICC, Exif and XMP because only the metadata of the first frame is taken into
            // account by the libavif API.
            // Ignore gain map because the two features are currently incompatible.
            if (!avifInputReadImage(input,
                                    layerIndex,
                                    /*ignoreColorProfile=*/AVIF_TRUE,
                                    /*ignoreExif=*/AVIF_TRUE,
                                    /*ignoreXMP=*/AVIF_TRUE,
                                    !settings->cicpExplicitlySet,
                                    /*ignoreGainMap=*/AVIF_TRUE,
                                    nextImage,
                                    &nextSettings,
                                    /*outDepth=*/NULL,
                                    /*sourceIsRGB=*/NULL,
                                    /*sourceTiming=*/NULL,
                                    settings->chromaDownsampling)) {
                goto cleanup;
            }
            // frameIter is NULL if y4m reached end, so single frame y4m is still supported.
            if (input->frameIter) {
                fprintf(stderr, "ERROR: Layered encoding does not support input with multiple frames: %s.\n", nextFile->filename);
                goto cleanup;
            }
            if (!avifEncoderVerifyImageCompatibility(firstImage, nextImage, "layer", nextFile->filename)) {
                goto cleanup;
            }
            if (!avifEncodeUpdateEncoderSettings(encoder, nextSettings)) {
                goto cleanup;
            }
            encodingImage = nextImage;
        }

        printf(" * Encoding layer %d: color quality [%d (%s)], alpha quality [%d (%s)]\n",
               layerIndex,
               encoder->quality,
               qualityString(encoder->quality),
               encoder->qualityAlpha,
               qualityString(encoder->qualityAlpha));

        const avifResult result = avifEncoderAddImage(encoder, encodingImage, settings->outputTiming.duration, AVIF_ADD_IMAGE_FLAG_NONE);
        if (result != AVIF_RESULT_OK) {
            fprintf(stderr, "ERROR: Failed to encode image: %s\n", avifResultToString(result));
            goto cleanup;
        }
        ++layerIndex;
    }

    // main() function should set number of layers to number of input,
    // so there should be no input left.
    assert(!avifInputHasRemainingData(input, layerIndex));
    success = AVIF_TRUE;
cleanup:
    if (nextImage) {
        avifImageDestroy(nextImage);
    }
    return success;
}

static avifBool avifEncodeImagesFixedQuality(const avifSettings * settings,
                                             avifInput * input,
                                             const avifInputFile * firstFile,
                                             const avifImage * firstImage,
                                             const avifImage * const * gridCells,
                                             avifRWData * encoded,
                                             avifEncodedByteSizes * byteSizes)
{
    avifBool success = AVIF_FALSE;
    avifRWDataFree(encoded);
    avifEncoder * encoder = avifEncoderCreate();
    if (!encoder) {
        fprintf(stderr, "ERROR: Out of memory\n");
        goto cleanup;
    }

    char manualTilingStr[128];
    snprintf(manualTilingStr,
             sizeof(manualTilingStr),
             "tileRowsLog2 [%d], tileColsLog2 [%d]",
             firstFile->settings.tileRowsLog2.value,
             firstFile->settings.tileColsLog2.value);

    encoder->maxThreads = settings->jobs;
    encoder->codecChoice = settings->codecChoice;
    encoder->speed = settings->speed;
    encoder->timescale = settings->outputTiming.timescale;
    encoder->keyframeInterval = settings->keyframeInterval;
    encoder->repetitionCount = settings->repetitionCount;
    encoder->headerFormat = settings->headerFormat;
    encoder->extraLayerCount = settings->layers - 1;
    if (!avifEncodeUpdateEncoderSettings(encoder, &firstFile->settings)) {
        goto cleanup;
    }

    if (settings->overrideQuality != INVALID_QUALITY) {
        encoder->quality = settings->overrideQuality;
    }
    if (settings->overrideQualityAlpha != INVALID_QUALITY) {
        encoder->qualityAlpha = settings->overrideQualityAlpha;
    }
#if defined(AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION)
    if (settings->qualityGainMap != INVALID_QUALITY) {
        encoder->qualityGainMap = settings->qualityGainMap;
    }
#endif

    const char * const codecName = avifCodecName(settings->codecChoice, AVIF_CODEC_FLAG_CAN_ENCODE);
    char speedStr[16];
    if (settings->speed == AVIF_SPEED_DEFAULT) {
        strcpy(speedStr, "default");
    } else {
        snprintf(speedStr, sizeof(speedStr), "%d", settings->speed);
    }
    char gainMapStr[100] = { 0 };
#if defined(AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION)
    if (firstImage->gainMap && firstImage->gainMap->image) {
        snprintf(gainMapStr, sizeof(gainMapStr), ", gain map quality [%d (%s)]", encoder->qualityGainMap, qualityString(encoder->qualityGainMap));
    }
#endif

    printf("Encoding with codec '%s' speed [%s], color quality [%d (%s)], alpha quality [%d (%s)]%s, %s, %d worker thread(s), please wait...\n",
           codecName ? codecName : "none",
           speedStr,
           encoder->quality,
           qualityString(encoder->quality),
           encoder->qualityAlpha,
           qualityString(encoder->qualityAlpha),
           gainMapStr,
           encoder->autoTiling ? "automatic tiling" : manualTilingStr,
           settings->jobs);
    if (settings->progressive) {
        // If the color quality is less than 10, the main() function overrides
        // --progressive and sets settings->autoProgressive to false.
        assert(encoder->quality >= PROGRESSIVE_WORST_QUALITY);
        // Encode the base layer with a very low quality to ensure a small encoded size.
        encoder->quality = 2;
        // Low alpha quality resulted in weird artifact, so we don't do it.
    }

    if (settings->layers > 1) {
        printf(" * Encoding layer %d: color quality [%d (%s)], alpha quality [%d (%s)]\n",
               0,
               encoder->quality,
               qualityString(encoder->quality),
               encoder->qualityAlpha,
               qualityString(encoder->qualityAlpha));
    }

    if (settings->gridDimsPresent) {
        const avifResult addImageResult =
            avifEncoderAddImageGrid(encoder, settings->gridDims[0], settings->gridDims[1], gridCells, AVIF_ADD_IMAGE_FLAG_SINGLE);
        if (addImageResult != AVIF_RESULT_OK) {
            fprintf(stderr, "ERROR: Failed to encode image grid: %s\n", avifResultToString(addImageResult));
            goto cleanup;
        }
    } else {
        int imageIndex = 1; // firstImage with imageIndex 0 is already available.

        avifAddImageFlags addImageFlags = AVIF_ADD_IMAGE_FLAG_NONE;
        if (!avifInputHasRemainingData(input, imageIndex) && (settings->layers == 1)) {
            addImageFlags |= AVIF_ADD_IMAGE_FLAG_SINGLE;
        }

        uint64_t firstDurationInTimescales = firstFile->duration ? firstFile->duration : settings->outputTiming.duration;
        if (input->useStdin || (settings->layers == 1 && input->filesCount > 1)) {
            printf(" * Encoding frame %d [%" PRIu64 "/%" PRIu64 " ts]: %s\n",
                   0,
                   firstDurationInTimescales,
                   settings->outputTiming.timescale,
                   firstFile->filename);
        }
        const avifResult addImageResult = avifEncoderAddImage(encoder, firstImage, firstDurationInTimescales, addImageFlags);
        if (addImageResult != AVIF_RESULT_OK) {
            fprintf(stderr, "ERROR: Failed to encode image: %s\n", avifResultToString(addImageResult));
            goto cleanup;
        }

        if (settings->layers > 1) {
            if (!avifEncodeRestOfLayeredImage(encoder, settings, input, imageIndex, firstImage)) {
                goto cleanup;
            }
        } else {
            // Not generating a single-image grid: Use all remaining input files as subsequent
            // frames.
            if (!avifEncodeRestOfImageSequence(encoder, settings, input, imageIndex, firstImage)) {
                goto cleanup;
            }
        }
    }

    const avifResult finishResult = avifEncoderFinish(encoder, encoded);
    if (finishResult != AVIF_RESULT_OK) {
        fprintf(stderr, "ERROR: Failed to finish encoding: %s\n", avifResultToString(finishResult));
        goto cleanup;
    }
    success = AVIF_TRUE;
    byteSizes->colorSizeBytes = encoder->ioStats.colorOBUSize;
    byteSizes->alphaSizeBytes = encoder->ioStats.alphaOBUSize;
#if defined(AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION)
    byteSizes->gainMapSizeBytes = avifEncoderGetGainMapSizeBytes(encoder);
#endif

cleanup:
    if (encoder) {
        if (!success) {
            avifDumpDiagnostics(&encoder->diag);
        }
        avifEncoderDestroy(encoder);
    }
    return success;
}

static avifBool avifEncodeImages(avifSettings * settings,
                                 avifInput * input,
                                 const avifInputFile * firstFile,
                                 const avifImage * firstImage,
                                 const avifImage * const * gridCells,
                                 avifRWData * encoded,
                                 avifEncodedByteSizes * byteSizes)
{
    if (settings->targetSize == -1) {
        return avifEncodeImagesFixedQuality(settings, input, firstFile, firstImage, gridCells, encoded, byteSizes);
    }

    avifBool hasGainMap = AVIF_FALSE;
    avifBool allQualitiesConstrained = settings->qualityIsConstrained && settings->qualityAlphaIsConstrained;
#if defined(AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION)
    hasGainMap = (firstImage->gainMap && firstImage->gainMap->image);
    if (hasGainMap) {
        allQualitiesConstrained = allQualitiesConstrained && settings->qualityGainMapIsConstrained;
    }
#endif

    if (allQualitiesConstrained) {
        fprintf(stderr, "ERROR: --target-size is used with constrained --qcolor and --qalpha %s\n", (hasGainMap ? "and --qgain-map" : ""));
        return AVIF_FALSE;
    }

    printf("Starting a binary search to find the %s%s generating the encoded image size closest to %d bytes, please wait...\n",
           settings->qualityAlphaIsConstrained ? "color quality"
                                               : (settings->qualityIsConstrained ? "alpha quality" : "color and alpha qualities"),
           (hasGainMap && !settings->qualityGainMapIsConstrained) ? " and gain map quality" : "",
           settings->targetSize);
    const size_t targetSize = (size_t)settings->targetSize;

    // TODO(yguyon): Use quantizer instead of quality because quantizer range is smaller (faster binary search).
    int closestQuality = INVALID_QUALITY;
    avifRWData closestEncoded = { NULL, 0 };
    size_t closestSizeDiff = 0;
    avifEncodedByteSizes closestByteSizes = { 0, 0, 0 };

    int minQuality = settings->progressive ? PROGRESSIVE_WORST_QUALITY : AVIF_QUALITY_WORST; // inclusive
    int maxQuality = AVIF_QUALITY_BEST;                                                      // inclusive
    while (minQuality <= maxQuality) {
        const int quality = (minQuality + maxQuality) / 2;
        if (!settings->qualityIsConstrained) {
            settings->overrideQuality = quality;
        }
        if (!settings->qualityAlphaIsConstrained) {
            settings->overrideQualityAlpha = quality;
        }
        if (!settings->qualityGainMapIsConstrained) {
            settings->qualityGainMap = quality;
        }

        if (!avifEncodeImagesFixedQuality(settings, input, firstFile, firstImage, gridCells, encoded, byteSizes)) {
            avifRWDataFree(&closestEncoded);
            return AVIF_FALSE;
        }
        printf("Encoded image of size %" AVIF_FMT_ZU " bytes.\n", encoded->size);

        if (encoded->size == targetSize) {
            return AVIF_TRUE;
        }

        size_t sizeDiff;
        if (encoded->size > targetSize) {
            sizeDiff = encoded->size - targetSize;
            maxQuality = quality - 1;
        } else {
            sizeDiff = targetSize - encoded->size;
            minQuality = quality + 1;
        }

        if ((closestQuality == INVALID_QUALITY) || (sizeDiff < closestSizeDiff)) {
            closestQuality = quality;
            avifRWDataFree(&closestEncoded);
            closestEncoded = *encoded;
            encoded->size = 0;
            encoded->data = NULL;
            closestSizeDiff = sizeDiff;
            closestByteSizes = *byteSizes;
        }
    }

    if (!settings->qualityIsConstrained) {
        settings->overrideQuality = closestQuality;
    }
    if (!settings->qualityAlphaIsConstrained) {
        settings->overrideQualityAlpha = closestQuality;
    }
    avifRWDataFree(encoded);
    *encoded = closestEncoded;
    *byteSizes = closestByteSizes;
    printf("Kept the encoded image of size %" AVIF_FMT_ZU " bytes generated with ", encoded->size);
    if (!settings->qualityIsConstrained) {
        printf("color quality %d", settings->overrideQuality);
    }
    if (!settings->qualityAlphaIsConstrained) {
        if (!settings->qualityIsConstrained) {
            printf(" and ");
        }
        printf("alpha quality %d", settings->overrideQualityAlpha);
    }
    printf(".\n");
    return AVIF_TRUE;
}

int main(int argc, char * argv[])
{
    if (argc < 2) {
        syntaxShort();
        return 1;
    }

    const char * outputFilename = NULL;

    avifInput input;
    memset(&input, 0, sizeof(input));
    input.files = malloc(sizeof(avifInputFile) * argc);
    if (input.files == NULL) {
        fprintf(stderr, "ERROR: memory allocation failure\n");
        return 1;
    }
    input.requestedFormat = AVIF_PIXEL_FORMAT_NONE; // AVIF_PIXEL_FORMAT_NONE is used as a sentinel for "auto"

    // See here for the discussion on the semi-arbitrary defaults for speed:
    //     https://github.com/AOMediaCodec/libavif/issues/440

    int returnCode = 1;
    avifBool noOverwrite = AVIF_FALSE;
    avifSettings settings;
    memset(&settings, 0, sizeof(settings));
    settings.codecChoice = AVIF_CODEC_CHOICE_AUTO;
    settings.jobs = -1;
    settings.targetSize = -1;
    settings.qualityIsConstrained = AVIF_FALSE;
    settings.qualityAlphaIsConstrained = AVIF_FALSE;
    settings.overrideQuality = INVALID_QUALITY;
    settings.overrideQualityAlpha = INVALID_QUALITY;
    settings.qualityGainMap = DEFAULT_QUALITY_GAIN_MAP;
    settings.progressive = AVIF_FALSE;
    settings.layered = AVIF_FALSE;
    settings.layers = 0;
    settings.speed = 6;
    settings.headerFormat = AVIF_HEADER_FULL;
    settings.repetitionCount = AVIF_REPETITION_COUNT_INFINITE;
    settings.keyframeInterval = 0;
    settings.ignoreExif = AVIF_FALSE;
    settings.ignoreXMP = AVIF_FALSE;
    settings.ignoreColorProfile = AVIF_FALSE;
    settings.ignoreGainMap = AVIF_FALSE;
    settings.cicpExplicitlySet = AVIF_FALSE;

    avifInputFileSettings pendingSettings;
    memset(&pendingSettings, 0, sizeof(pendingSettings));

    avifBool cropConversionRequired = AVIF_FALSE;
    uint8_t irotAngle = 0xff; // sentinel value indicating "unused"
    uint8_t imirAxis = 0xff;  // sentinel value indicating "unused"
    avifRange requestedRange = AVIF_RANGE_FULL;
    avifBool lossless = AVIF_FALSE;
    avifImage * image = NULL;
    avifRWData raw = AVIF_DATA_EMPTY;
    avifRWData exifOverride = AVIF_DATA_EMPTY;
    avifRWData xmpOverride = AVIF_DATA_EMPTY;
    avifRWData iccOverride = AVIF_DATA_EMPTY;
    avifBool premultiplyAlpha = AVIF_FALSE;
    uint32_t gridCellCount = 0;
    avifImage ** gridCells = NULL;
    avifImage * gridSplitImage = NULL; // used for cleanup tracking

    // By default, the color profile itself is unspecified, so CP/TC are set (to 2) accordingly.
    // However, if the end-user doesn't specify any CICP, we will convert to YUV using BT601
    // coefficients anyway (as MC:2 falls back to MC:5/6), so we might as well signal it explicitly.
    // See: ISO/IEC 23000-22:2019 Amendment 2, or the comment in avifCalcYUVCoefficients()
    settings.colorPrimaries = AVIF_COLOR_PRIMARIES_UNSPECIFIED;
    settings.transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED;
    settings.matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT601;
    settings.chromaDownsampling = AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC;

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
                input.files[input.filesCount].duration = settings.outputTiming.duration;
                input.files[input.filesCount].settings = pendingSettings;
                memset(&pendingSettings, 0, sizeof(pendingSettings));
                ++input.filesCount;
                ++argIndex;
            }
            break;
        } else if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            syntaxLong();
            returnCode = 0;
            goto cleanup;
        } else if (!strcmp(arg, "-V") || !strcmp(arg, "--version")) {
            avifPrintVersions();
            returnCode = 0;
            goto cleanup;
        } else if (!strcmp(arg, "--no-overwrite")) {
            noOverwrite = AVIF_TRUE;
        } else if (!strcmp(arg, "-j") || !strcmp(arg, "--jobs")) {
            NEXTARG();
            if (!strcmp(arg, "all")) {
                settings.jobs = avifQueryCPUCount();
            } else {
                settings.jobs = atoi(arg);
                if (settings.jobs < 1) {
                    settings.jobs = 1;
                }
            }
        } else if (!strcmp(arg, "--stdin")) {
            input.useStdin = AVIF_TRUE;
        } else if (!strcmp(arg, "-o") || !strcmp(arg, "--output")) {
            NEXTARG();
            outputFilename = arg;
#if defined(AVIF_ENABLE_EXPERIMENTAL_AVIR)
        } else if (!strcmp(arg, "--avir")) {
            settings.headerFormat = AVIF_HEADER_REDUCED;
#endif // AVIF_ENABLE_EXPERIMENTAL_AVIR
        } else if (!strcmp(arg, "-d") || !strcmp(arg, "--depth")) {
            NEXTARG();
            input.requestedDepth = atoi(arg);
            if ((input.requestedDepth != 8) && (input.requestedDepth != 10) && (input.requestedDepth != 12)) {
                fprintf(stderr, "ERROR: invalid depth: %s\n", arg);
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
                goto cleanup;
            }
        } else if (!strcmp(arg, "-k") || !strcmp(arg, "--keyframe")) {
            NEXTARG();
            settings.keyframeInterval = atoi(arg);
        } else if (!strcmp(arg, "-q") || !strcmp(arg, "--qcolor") || strpre(arg, "-q:") || strpre(arg, "--qcolor:")) {
            // For compatibility reason unsuffixed flags always apply to all input (as if they appear before first input).
            // Print a warning if unsuffixed flag appears after input file.
            avifOptionSuffixType type = parseOptionSuffix(arg, /*warnNoSuffix=*/input.filesCount != 0);
            if (type == AVIF_OPTION_SUFFIX_INVALID) {
                goto cleanup;
            }
            NEXTARG();
            int quality = atoi(arg);
            if (quality < AVIF_QUALITY_WORST) {
                quality = AVIF_QUALITY_WORST;
            }
            if (quality > AVIF_QUALITY_BEST) {
                quality = AVIF_QUALITY_BEST;
            }
            if (type == AVIF_OPTION_SUFFIX_UPDATE || input.filesCount == 0) {
                pendingSettings.quality = intSettingsEntryOf(quality);
            } else {
                input.files[0].settings.quality = intSettingsEntryOf(quality);
            }
        } else if (!strcmp(arg, "--qalpha") || strpre(arg, "--qalpha:")) {
            avifOptionSuffixType type = parseOptionSuffix(arg, input.filesCount != 0);
            if (type == AVIF_OPTION_SUFFIX_INVALID) {
                goto cleanup;
            }
            NEXTARG();
            int qualityAlpha = atoi(arg);
            if (qualityAlpha < AVIF_QUALITY_WORST) {
                qualityAlpha = AVIF_QUALITY_WORST;
            }
            if (qualityAlpha > AVIF_QUALITY_BEST) {
                qualityAlpha = AVIF_QUALITY_BEST;
            }
            if (type == AVIF_OPTION_SUFFIX_UPDATE || input.filesCount == 0) {
                pendingSettings.qualityAlpha = intSettingsEntryOf(qualityAlpha);
            } else {
                input.files[0].settings.qualityAlpha = intSettingsEntryOf(qualityAlpha);
            }
        } else if (!strcmp(arg, "--min") || strpre(arg, "--min:")) {
            avifOptionSuffixType type = parseOptionSuffix(arg, input.filesCount != 0);
            if (type == AVIF_OPTION_SUFFIX_INVALID) {
                goto cleanup;
            }
            NEXTARG();
            int minQuantizer = atoi(arg);
            if (minQuantizer < AVIF_QUANTIZER_BEST_QUALITY) {
                minQuantizer = AVIF_QUANTIZER_BEST_QUALITY;
            }
            if (minQuantizer > AVIF_QUANTIZER_WORST_QUALITY) {
                minQuantizer = AVIF_QUANTIZER_WORST_QUALITY;
            }
            if (type == AVIF_OPTION_SUFFIX_UPDATE || input.filesCount == 0) {
                pendingSettings.minQuantizer = intSettingsEntryOf(minQuantizer);
            } else {
                input.files[0].settings.minQuantizer = intSettingsEntryOf(minQuantizer);
            }
        } else if (!strcmp(arg, "--max") || strpre(arg, "--max:")) {
            avifOptionSuffixType type = parseOptionSuffix(arg, input.filesCount != 0);
            if (type == AVIF_OPTION_SUFFIX_INVALID) {
                goto cleanup;
            }
            NEXTARG();
            int maxQuantizer = atoi(arg);
            if (maxQuantizer < AVIF_QUANTIZER_BEST_QUALITY) {
                maxQuantizer = AVIF_QUANTIZER_BEST_QUALITY;
            }
            if (maxQuantizer > AVIF_QUANTIZER_WORST_QUALITY) {
                maxQuantizer = AVIF_QUANTIZER_WORST_QUALITY;
            }
            if (type == AVIF_OPTION_SUFFIX_UPDATE || input.filesCount == 0) {
                pendingSettings.maxQuantizer = intSettingsEntryOf(maxQuantizer);
            } else {
                input.files[0].settings.maxQuantizer = intSettingsEntryOf(maxQuantizer);
            }
        } else if (!strcmp(arg, "--minalpha") || strpre(arg, "--minalpha:")) {
            avifOptionSuffixType type = parseOptionSuffix(arg, input.filesCount != 0);
            if (type == AVIF_OPTION_SUFFIX_INVALID) {
                goto cleanup;
            }
            NEXTARG();
            int minQuantizerAlpha = atoi(arg);
            if (minQuantizerAlpha < AVIF_QUANTIZER_BEST_QUALITY) {
                minQuantizerAlpha = AVIF_QUANTIZER_BEST_QUALITY;
            }
            if (minQuantizerAlpha > AVIF_QUANTIZER_WORST_QUALITY) {
                minQuantizerAlpha = AVIF_QUANTIZER_WORST_QUALITY;
            }
            if (type == AVIF_OPTION_SUFFIX_UPDATE || input.filesCount == 0) {
                pendingSettings.minQuantizerAlpha = intSettingsEntryOf(minQuantizerAlpha);
            } else {
                input.files[0].settings.minQuantizerAlpha = intSettingsEntryOf(minQuantizerAlpha);
            }
        } else if (!strcmp(arg, "--maxalpha") || strpre(arg, "--maxalpha:")) {
            avifOptionSuffixType type = parseOptionSuffix(arg, input.filesCount != 0);
            if (type == AVIF_OPTION_SUFFIX_INVALID) {
                goto cleanup;
            }
            NEXTARG();
            int maxQuantizerAlpha = atoi(arg);
            if (maxQuantizerAlpha < AVIF_QUANTIZER_BEST_QUALITY) {
                maxQuantizerAlpha = AVIF_QUANTIZER_BEST_QUALITY;
            }
            if (maxQuantizerAlpha > AVIF_QUANTIZER_WORST_QUALITY) {
                maxQuantizerAlpha = AVIF_QUANTIZER_WORST_QUALITY;
            }
            if (type == AVIF_OPTION_SUFFIX_UPDATE || input.filesCount == 0) {
                pendingSettings.maxQuantizerAlpha = intSettingsEntryOf(maxQuantizerAlpha);
            } else {
                input.files[0].settings.maxQuantizerAlpha = intSettingsEntryOf(maxQuantizerAlpha);
            }
        } else if (!strcmp(arg, "--target-size")) {
            NEXTARG();
            settings.targetSize = atoi(arg);
            if (settings.targetSize < 0) {
                settings.targetSize = -1;
            }
        } else if (!strcmp(arg, "--tilerowslog2") || strpre(arg, "--tilerowslog2:")) {
            avifOptionSuffixType type = parseOptionSuffix(arg, input.filesCount != 0);
            if (type == AVIF_OPTION_SUFFIX_INVALID) {
                goto cleanup;
            }
            NEXTARG();
            int tileRowsLog2 = atoi(arg);
            if (tileRowsLog2 < 0) {
                tileRowsLog2 = 0;
            }
            if (tileRowsLog2 > 6) {
                tileRowsLog2 = 6;
            }
            if (type == AVIF_OPTION_SUFFIX_UPDATE || input.filesCount == 0) {
                pendingSettings.tileRowsLog2 = intSettingsEntryOf(tileRowsLog2);
            } else {
                input.files[0].settings.tileRowsLog2 = intSettingsEntryOf(tileRowsLog2);
            }
        } else if (!strcmp(arg, "--tilecolslog2") || strpre(arg, "--tilecolslog2:")) {
            avifOptionSuffixType type = parseOptionSuffix(arg, input.filesCount != 0);
            if (type == AVIF_OPTION_SUFFIX_INVALID) {
                goto cleanup;
            }
            NEXTARG();
            int tileColsLog2 = atoi(arg);
            if (tileColsLog2 < 0) {
                tileColsLog2 = 0;
            }
            if (tileColsLog2 > 6) {
                tileColsLog2 = 6;
            }
            if (type == AVIF_OPTION_SUFFIX_UPDATE || input.filesCount == 0) {
                pendingSettings.tileColsLog2 = intSettingsEntryOf(tileColsLog2);
            } else {
                input.files[0].settings.tileColsLog2 = intSettingsEntryOf(tileColsLog2);
            }
        } else if (!strcmp(arg, "--autotiling") || strpre(arg, "--autotiling:")) {
            avifOptionSuffixType type = parseOptionSuffix(arg, input.filesCount != 0);
            if (type == AVIF_OPTION_SUFFIX_INVALID) {
                goto cleanup;
            }
            if (type == AVIF_OPTION_SUFFIX_UPDATE || input.filesCount == 0) {
                pendingSettings.autoTiling = boolSettingsEntryOf(AVIF_TRUE);
            } else {
                input.files[0].settings.autoTiling = boolSettingsEntryOf(AVIF_TRUE);
            }
        } else if (!strcmp(arg, "--progressive")) {
            if (settings.layered) {
                fprintf(stderr, "ERROR: Can not use both --progressive and --layered\n");
                goto cleanup;
            }
            settings.progressive = AVIF_TRUE;
        } else if (!strcmp(arg, "--layered")) {
            if (settings.progressive) {
                fprintf(stderr, "ERROR: Can not use both --progressive and --layered\n");
                goto cleanup;
            }
            settings.layered = AVIF_TRUE;
        } else if (!strcmp(arg, "--scaling-mode") || strpre(arg, "--scaling-mode:")) {
            avifOptionSuffixType type = parseOptionSuffix(arg, input.filesCount != 0);
            if (type == AVIF_OPTION_SUFFIX_INVALID) {
                goto cleanup;
            }
            NEXTARG();
            uint32_t frac[2] = { 0, 1 };
            if (!(parseU32List(frac, 1, arg, '/') || parseU32List(frac, 2, arg, '/')) || frac[0] > INT32_MAX || frac[1] > INT32_MAX) {
                fprintf(stderr, "ERROR: Invalid scaling mode: %s\n", arg);
                goto cleanup;
            }
            if (type == AVIF_OPTION_SUFFIX_UPDATE || input.filesCount == 0) {
                pendingSettings.scalingMode = scalingModeSettingsEntryOf(frac[0], frac[1]);
            } else {
                input.files[0].settings.scalingMode = scalingModeSettingsEntryOf(frac[0], frac[1]);
            }
        } else if (!strcmp(arg, "-g") || !strcmp(arg, "--grid")) {
            NEXTARG();
            if (!parseU32List(settings.gridDims, 2, arg, 'x')) {
                fprintf(stderr, "ERROR: Invalid grid dims: %s\n", arg);
                goto cleanup;
            }
            settings.gridDimsPresent = AVIF_TRUE;
            if ((settings.gridDims[0] == 0) || (settings.gridDims[0] > 256) || (settings.gridDims[1] == 0) ||
                (settings.gridDims[1] > 256)) {
                fprintf(stderr, "ERROR: Invalid grid dims (valid dim range [1-256]): %s\n", arg);
                goto cleanup;
            }
        } else if (!strcmp(arg, "--cicp") || !strcmp(arg, "--nclx")) {
            NEXTARG();
            uint32_t cicp[3];
            if (!parseU32List(cicp, 3, arg, '/')) {
                fprintf(stderr, "ERROR: Invalid CICP value: %s\n", arg);
                goto cleanup;
            }
            settings.colorPrimaries = (avifColorPrimaries)cicp[0];
            settings.transferCharacteristics = (avifTransferCharacteristics)cicp[1];
            settings.matrixCoefficients = (avifMatrixCoefficients)cicp[2];
            settings.cicpExplicitlySet = AVIF_TRUE;
        } else if (!strcmp(arg, "-r") || !strcmp(arg, "--range")) {
            NEXTARG();
            if (!strcmp(arg, "limited") || !strcmp(arg, "l")) {
                requestedRange = AVIF_RANGE_LIMITED;
            } else if (!strcmp(arg, "full") || !strcmp(arg, "f")) {
                requestedRange = AVIF_RANGE_FULL;
            } else {
                fprintf(stderr, "ERROR: Unknown range: %s\n", arg);
                goto cleanup;
            }
        } else if (!strcmp(arg, "-s") || !strcmp(arg, "--speed")) {
            NEXTARG();
            if (!strcmp(arg, "default") || !strcmp(arg, "d")) {
                settings.speed = AVIF_SPEED_DEFAULT;
            } else {
                settings.speed = atoi(arg);
                if (settings.speed > AVIF_SPEED_FASTEST) {
                    settings.speed = AVIF_SPEED_FASTEST;
                }
                if (settings.speed < AVIF_SPEED_SLOWEST) {
                    settings.speed = AVIF_SPEED_SLOWEST;
                }
            }
        } else if (!strcmp(arg, "--exif")) {
            NEXTARG();
            if (!readEntireFile(arg, &exifOverride)) {
                fprintf(stderr, "ERROR: Unable to read Exif metadata: %s\n", arg);
                goto cleanup;
            }
            settings.ignoreExif = AVIF_TRUE;
        } else if (!strcmp(arg, "--xmp")) {
            NEXTARG();
            if (!readEntireFile(arg, &xmpOverride)) {
                fprintf(stderr, "ERROR: Unable to read XMP metadata: %s\n", arg);
                goto cleanup;
            }
            settings.ignoreXMP = AVIF_TRUE;
        } else if (!strcmp(arg, "--icc")) {
            NEXTARG();
            if (!readEntireFile(arg, &iccOverride)) {
                fprintf(stderr, "ERROR: Unable to read ICC profile: %s\n", arg);
                goto cleanup;
            }
            settings.ignoreColorProfile = AVIF_TRUE;
        } else if (!strcmp(arg, "--duration") || strpre(arg, "--duration:")) {
            // --duration is special, we always treat it as suffixed with :u, so don't print warning for it.
            avifOptionSuffixType type = parseOptionSuffix(arg, /*warnNoSuffix=*/AVIF_FALSE);
            if (type == AVIF_OPTION_SUFFIX_INVALID) {
                goto cleanup;
            }
            NEXTARG();
            int durationInt = atoi(arg);
            if (durationInt < 1) {
                fprintf(stderr, "ERROR: Invalid duration: %d\n", durationInt);
                goto cleanup;
            }
            settings.outputTiming.duration = (uint64_t)durationInt;
        } else if (!strcmp(arg, "--timescale") || !strcmp(arg, "--fps")) {
            NEXTARG();
            int timescaleInt = atoi(arg);
            if (timescaleInt < 1) {
                fprintf(stderr, "ERROR: Invalid timescale: %d\n", timescaleInt);
                goto cleanup;
            }
            settings.outputTiming.timescale = (uint64_t)timescaleInt;
        } else if (!strcmp(arg, "-c") || !strcmp(arg, "--codec")) {
            NEXTARG();
            settings.codecChoice = avifCodecChoiceFromName(arg);
            if (settings.codecChoice == AVIF_CODEC_CHOICE_AUTO) {
                fprintf(stderr, "ERROR: Unrecognized codec: %s\n", arg);
                goto cleanup;
            } else {
                const char * codecName = avifCodecName(settings.codecChoice, AVIF_CODEC_FLAG_CAN_ENCODE);
                if (codecName == NULL) {
                    fprintf(stderr, "ERROR: Codec cannot encode: %s\n", arg);
                    goto cleanup;
                }
            }
        } else if (!strcmp(arg, "-a") || !strcmp(arg, "--advanced") || strpre(arg, "-a:") || strpre(arg, "--advanced:")) {
            avifOptionSuffixType type = parseOptionSuffix(arg, input.filesCount != 0);
            if (type == AVIF_OPTION_SUFFIX_INVALID) {
                goto cleanup;
            }
            NEXTARG();
            avifInputFileSettings * targetSettings =
                (type == AVIF_OPTION_SUFFIX_UPDATE || input.filesCount == 0) ? &pendingSettings : &input.files[0].settings;
            if (!avifCodecSpecificOptionsAdd(&targetSettings->codecSpecificOptions, arg)) {
                fprintf(stderr, "ERROR: Out of memory when setting codec specific option: %s\n", arg);
                goto cleanup;
            }
        } else if (!strcmp(arg, "--ignore-exif")) {
            settings.ignoreExif = AVIF_TRUE;
        } else if (!strcmp(arg, "--ignore-xmp")) {
            settings.ignoreXMP = AVIF_TRUE;
        } else if (!strcmp(arg, "--ignore-profile") || !strcmp(arg, "--ignore-icc")) {
            settings.ignoreColorProfile = AVIF_TRUE;
#if defined(AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION)
        } else if (!strcmp(arg, "--ignore-gain-map")) {
            settings.ignoreGainMap = AVIF_TRUE;
        } else if (!strcmp(arg, "--qgain-map")) {
            NEXTARG();
            int qualityGainMap = atoi(arg);
            if (qualityGainMap < AVIF_QUALITY_WORST) {
                qualityGainMap = AVIF_QUALITY_WORST;
            }
            if (qualityGainMap > AVIF_QUALITY_BEST) {
                qualityGainMap = AVIF_QUALITY_BEST;
            }
            settings.qualityGainMap = qualityGainMap;
            settings.qualityGainMapIsConstrained = AVIF_TRUE;
#endif
        } else if (!strcmp(arg, "--pasp")) {
            NEXTARG();
            if (!parseU32List(settings.paspValues, 2, arg, ',')) {
                fprintf(stderr, "ERROR: Invalid pasp values: %s\n", arg);
                goto cleanup;
            }
            settings.paspPresent = AVIF_TRUE;
        } else if (!strcmp(arg, "--crop")) {
            NEXTARG();
            if (!parseU32List(settings.clapValues, 4, arg, ',')) {
                fprintf(stderr, "ERROR: Invalid crop values: %s\n", arg);
                goto cleanup;
            }
            cropConversionRequired = AVIF_TRUE;
        } else if (!strcmp(arg, "--clap")) {
            NEXTARG();
            if (!parseU32List(settings.clapValues, 8, arg, ',')) {
                fprintf(stderr, "ERROR: Invalid clap values: %s\n", arg);
                goto cleanup;
            }
            settings.clapValid = AVIF_TRUE;
        } else if (!strcmp(arg, "--irot")) {
            NEXTARG();
            irotAngle = (uint8_t)atoi(arg);
            if (irotAngle > 3) {
                fprintf(stderr, "ERROR: Invalid irot angle: %s\n", arg);
                goto cleanup;
            }
        } else if (!strcmp(arg, "--imir")) {
            NEXTARG();
            imirAxis = (uint8_t)atoi(arg);
            if (imirAxis > 1) {
                fprintf(stderr, "ERROR: Invalid imir axis: %s\n", arg);
                goto cleanup;
            }
        } else if (!strcmp(arg, "--clli")) {
            NEXTARG();
            if (!parseU32List(settings.clliValues, 2, arg, ',') || settings.clliValues[0] >= (1u << 16) ||
                settings.clliValues[1] >= (1u << 16)) {
                fprintf(stderr, "ERROR: Invalid clli values: %s\n", arg);
                goto cleanup;
            }
            settings.clliPresent = AVIF_TRUE;
        } else if (!strcmp(arg, "--repetition-count")) {
            NEXTARG();
            if (!strcmp(arg, "infinite")) {
                settings.repetitionCount = AVIF_REPETITION_COUNT_INFINITE;
            } else {
                settings.repetitionCount = atoi(arg);
                if (settings.repetitionCount < 0) {
                    fprintf(stderr, "ERROR: Invalid repetition count: %s\n", arg);
                    goto cleanup;
                }
            }
        } else if (!strcmp(arg, "-l") || !strcmp(arg, "--lossless")) {
            lossless = AVIF_TRUE;
        } else if (!strcmp(arg, "-p") || !strcmp(arg, "--premultiply")) {
            premultiplyAlpha = AVIF_TRUE;
        } else if (!strcmp(arg, "--sharpyuv")) {
            settings.chromaDownsampling = AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV;
        } else if (arg[0] == '-') {
            fprintf(stderr, "ERROR: unrecognized option %s\n\n", arg);
            syntaxLong();
            goto cleanup;
        } else {
            // Positional argument
            input.files[input.filesCount].filename = arg;
            input.files[input.filesCount].duration = settings.outputTiming.duration;
            input.files[input.filesCount].settings = pendingSettings;
            memset(&pendingSettings, 0, sizeof(pendingSettings));
            ++input.filesCount;
        }

        ++argIndex;
    }

    if (settings.jobs == -1) {
        settings.jobs = avifQueryCPUCount();
    }

    // Check global lossless parameters and set to default if needed.
    if (lossless) {
        // Pixel format.
        if (input.requestedFormat != AVIF_PIXEL_FORMAT_NONE && input.requestedFormat != AVIF_PIXEL_FORMAT_YUV444 &&
            input.requestedFormat != AVIF_PIXEL_FORMAT_YUV400) {
            fprintf(stderr,
                    "When set, the pixel format can only be 444 in lossless "
                    "mode. 400 also works if the input is grayscale.\n");
            goto cleanup;
        }
        // Codec.
        const char * codecName = avifCodecName(settings.codecChoice, AVIF_CODEC_FLAG_CAN_ENCODE);
        if (codecName && !strcmp(codecName, "rav1e")) {
            fprintf(stderr, "rav1e doesn't support lossless encoding yet: https://github.com/xiph/rav1e/issues/151\n");
            goto cleanup;
        } else if (codecName && !strcmp(codecName, "svt")) {
            fprintf(stderr, "SVT-AV1 doesn't support lossless encoding yet: https://gitlab.com/AOMediaCodec/SVT-AV1/-/issues/1636\n");
            goto cleanup;
        }
        // Range.
        if (requestedRange != AVIF_RANGE_FULL) {
            fprintf(stderr, "Range has to be full in lossless mode.\n");
            goto cleanup;
        }
        // Matrix coefficients.
        if (settings.cicpExplicitlySet) {
            avifBool incompatibleMC = (settings.matrixCoefficients != AVIF_MATRIX_COEFFICIENTS_IDENTITY);
#if defined(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
            incompatibleMC &= (settings.matrixCoefficients != AVIF_MATRIX_COEFFICIENTS_YCGCO_RE &&
                               settings.matrixCoefficients != AVIF_MATRIX_COEFFICIENTS_YCGCO_RO);
#endif
            if (incompatibleMC) {
#if defined(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
                fprintf(stderr, "Matrix coefficients have to be identity, YCgCo-Re, or YCgCo-Ro in lossless mode.\n");
#else
                fprintf(stderr, "Matrix coefficients have to be identity in lossless mode.\n");
#endif
                goto cleanup;
            }
        } else {
            settings.matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_IDENTITY;
        }
        if (settings.progressive) {
            fprintf(stderr, "Automatic layered encoding is unsupported in lossless mode.\n");
        }
    }

    stdinFile.filename = "(stdin)";
    stdinFile.duration = settings.outputTiming.duration;

    avifInputFileSettings emptySettingsReference;
    memset(&emptySettingsReference, 0, sizeof(emptySettingsReference));

    if (!outputFilename) {
        if (((input.useStdin && (input.filesCount == 1)) || (!input.useStdin && (input.filesCount > 1)))) {
            --input.filesCount;
            outputFilename = input.files[input.filesCount].filename;
            if (memcmp(&input.files[input.filesCount].settings, &emptySettingsReference, sizeof(avifInputFileSettings)) != 0) {
                fprintf(stderr, "WARNING: Trailing options with update suffix has no effect. Place them before the input you intend to apply to.\n");
            }
        }
    }

    if (!outputFilename || (input.useStdin && (input.filesCount > 0)) || (!input.useStdin && (input.filesCount < 1))) {
        syntaxShort();
        goto cleanup;
    }

    if (noOverwrite && fileExists(outputFilename)) {
        fprintf(stderr, "ERROR: output file %s already exists and --no-overwrite was specified\n", outputFilename);
        goto cleanup;
    }

#if defined(_WIN32)
    if (input.useStdin) {
        setmode(fileno(stdin), O_BINARY);
    }
#endif

    if (memcmp(&pendingSettings, &emptySettingsReference, sizeof(avifInputFileSettings)) != 0) {
        fprintf(stderr, "WARNING: Trailing options with update suffix has no effect. Place them before the input you intend to apply to.\n");
    }

    // Check layer config
    if (settings.progressive) {
        assert(!settings.layered);
        if (input.filesCount > 1) {
            fprintf(stderr, "ERROR: --progressive only supports one input.\n");
            goto cleanup;
        }
        // for automatic layered encoding, make a 2 layer AVIF.
        settings.layers = 2;
    } else if (settings.layered) {
        // For manual layered encoding, infer number of layers from input count.
        // For multi-frame input (Y4Ms) layered encoding only use the first frame,
        // so that we can assume number of inputs is number of layers.
        // We don't support changing config halfway encoding on input,
        // therefore encoding layered AVIF with single multi-frame input is not very meaningful.
        if (input.filesCount < 2 || input.filesCount > AVIF_MAX_AV1_LAYER_COUNT) {
            fprintf(stderr, "Encoding layered AVIF required 2 to %d inputs, but got %d.\n", AVIF_MAX_AV1_LAYER_COUNT, input.filesCount);
            goto cleanup;
        }
        settings.layers = input.filesCount;
    } else {
        settings.layers = 1;
    }
    if (settings.layers > 1 && settings.gridDimsPresent) {
        fprintf(stderr, "Layered grid image unimplemented in avifenc.\n");
        goto cleanup;
    }

    for (int i = 0; i < input.filesCount; ++i) {
        avifInputFile * file = &input.files[i];
        avifInputFileSettings * fileSettings = &file->settings;

        // Check tiling parameters.
        // Auto tiling (autoTiling) and manual tiling (tileRowsLog2, tileColsLog2) are mutually exclusive, which means:
        // - At each input, only one of the two shall be set.
        // - At some input, specify one disables the other.
        if (fileSettings->autoTiling.set) {
            if (fileSettings->tileRowsLog2.set || fileSettings->tileColsLog2.set) {
                fprintf(stderr, "ERROR: --autotiling is specified but --tilerowslog2 or --tilecolslog2 is also specified for current input.\n");
                goto cleanup;
            }
            // At this point, autoTiling of this input file can only be set by command line.
            // (auto generation of setting entries happens below)
            // Since it's a boolean flag, its value must be AVIF_TRUE.
            assert(fileSettings->autoTiling.value);
            // Therefore disables manual tiling at this input (in case it was enabled at previous input).
            fileSettings->tileRowsLog2 = intSettingsEntryOf(0);
            fileSettings->tileColsLog2 = intSettingsEntryOf(0);
        } else if (fileSettings->tileColsLog2.set || fileSettings->tileRowsLog2.set) {
            // If this file has manual tile config set, disable autotiling, for the same reason as above.
            fileSettings->autoTiling = boolSettingsEntryOf(AVIF_FALSE);
        }

        // Check per-input lossy/lossless parameters.
        if (lossless) {
            // Quality.
            if ((fileSettings->quality.set && fileSettings->quality.value != AVIF_QUALITY_LOSSLESS) ||
                (fileSettings->qualityAlpha.set && fileSettings->qualityAlpha.value != AVIF_QUALITY_LOSSLESS)) {
                fprintf(stderr, "ERROR: Quality cannot be set in lossless mode, except to %d.\n", AVIF_QUALITY_LOSSLESS);
                goto cleanup;
            }
            // Quantizers.
            if ((fileSettings->minQuantizer.set && fileSettings->minQuantizer.value != AVIF_QUANTIZER_LOSSLESS) ||
                (fileSettings->maxQuantizer.set && fileSettings->maxQuantizer.value != AVIF_QUANTIZER_LOSSLESS) ||
                (fileSettings->minQuantizerAlpha.set && fileSettings->minQuantizerAlpha.value != AVIF_QUANTIZER_LOSSLESS) ||
                (fileSettings->maxQuantizerAlpha.set && fileSettings->maxQuantizerAlpha.value != AVIF_QUANTIZER_LOSSLESS)) {
                fprintf(stderr, "ERROR: Quantizers cannot be set in lossless mode, except to %d.\n", AVIF_QUANTIZER_LOSSLESS);
                goto cleanup;
            }
        } else {
            if (settings.progressive) {
                assert(DEFAULT_QUALITY >= PROGRESSIVE_WORST_QUALITY);
                if (fileSettings->quality.set && fileSettings->quality.value < PROGRESSIVE_WORST_QUALITY) {
                    fprintf(stderr, "ERROR: --qcolor must be at least %d when using --progressive.\n", PROGRESSIVE_WORST_QUALITY);
                    goto cleanup;
                }
                // --progressive only adjust color quality
            }
        }

        // Set defaults for first input file.
        if (i == 0) {
            // This check only applies to the first input.
            // Following inputs can change only one and leave the other unchanged.
            if (fileSettings->minQuantizer.set != fileSettings->maxQuantizer.set) {
                fprintf(stderr, "ERROR: --min and --max must be either both specified or both unspecified for input %s.\n", file->filename);
                goto cleanup;
            }
            if (fileSettings->minQuantizerAlpha.set != fileSettings->maxQuantizerAlpha.set) {
                fprintf(stderr,
                        "ERROR: --minalpha and --maxalpha must be either both specified or both unspecified for input %s.\n",
                        file->filename);
                goto cleanup;
            }

            if (!fileSettings->autoTiling.set) {
                fileSettings->autoTiling = boolSettingsEntryOf(AVIF_FALSE);
            }
            if (!fileSettings->tileRowsLog2.set) {
                fileSettings->tileRowsLog2 = intSettingsEntryOf(0);
            }
            if (!fileSettings->tileColsLog2.set) {
                fileSettings->tileColsLog2 = intSettingsEntryOf(0);
            }

            // Set lossy/lossless parameters to default if needed.
            if (lossless) {
                // Add lossless settings.
                // Settings on first input will be inherited by all inputs, so this is sufficient.
                fileSettings->quality = intSettingsEntryOf(AVIF_QUALITY_LOSSLESS);
                fileSettings->qualityAlpha = intSettingsEntryOf(AVIF_QUALITY_LOSSLESS);
                fileSettings->minQuantizer = intSettingsEntryOf(AVIF_QUANTIZER_LOSSLESS);
                fileSettings->maxQuantizer = intSettingsEntryOf(AVIF_QUANTIZER_LOSSLESS);
                fileSettings->minQuantizerAlpha = intSettingsEntryOf(AVIF_QUANTIZER_LOSSLESS);
                fileSettings->maxQuantizerAlpha = intSettingsEntryOf(AVIF_QUANTIZER_LOSSLESS);
            } else {
                settings.qualityIsConstrained = fileSettings->quality.set;
                settings.qualityAlphaIsConstrained = fileSettings->qualityAlpha.set;

                if (fileSettings->minQuantizer.set) {
                    assert(fileSettings->maxQuantizer.set);
                    if (!fileSettings->quality.set) {
                        const int quantizer = (fileSettings->minQuantizer.value + fileSettings->maxQuantizer.value) / 2;
                        const int quality = ((63 - quantizer) * 100 + 31) / 63;
                        fileSettings->quality = intSettingsEntryOf(quality);
                    }
                } else {
                    assert(!fileSettings->maxQuantizer.set);
                    if (!fileSettings->quality.set) {
                        fileSettings->quality = intSettingsEntryOf(DEFAULT_QUALITY);
                    }
                    fileSettings->minQuantizer = intSettingsEntryOf(AVIF_QUANTIZER_BEST_QUALITY);
                    fileSettings->maxQuantizer = intSettingsEntryOf(AVIF_QUANTIZER_WORST_QUALITY);
                }

                if (fileSettings->minQuantizerAlpha.set) {
                    assert(fileSettings->maxQuantizerAlpha.set);
                    if (!fileSettings->qualityAlpha.set) {
                        const int quantizerAlpha = (fileSettings->minQuantizerAlpha.set + fileSettings->maxQuantizerAlpha.set) / 2;
                        const int qualityAlpha = ((63 - quantizerAlpha) * 100 + 31) / 63;
                        fileSettings->qualityAlpha = intSettingsEntryOf(qualityAlpha);
                    }
                } else {
                    assert(!fileSettings->maxQuantizerAlpha.set);
                    if (!fileSettings->qualityAlpha.set) {
                        fileSettings->qualityAlpha = intSettingsEntryOf(DEFAULT_QUALITY_ALPHA);
                    }
                    fileSettings->minQuantizerAlpha = intSettingsEntryOf(AVIF_QUANTIZER_BEST_QUALITY);
                    fileSettings->maxQuantizerAlpha = intSettingsEntryOf(AVIF_QUANTIZER_WORST_QUALITY);
                }
            }

            if (!fileSettings->scalingMode.set) {
                fileSettings->scalingMode = scalingModeSettingsEntryOf(1, 1);
            }
        }
    }

    image = avifImageCreateEmpty();
    if (!image) {
        fprintf(stderr, "ERROR: Out of memory\n");
        goto cleanup;
    }

    // Set these in advance so any upcoming RGB -> YUV use the proper coefficients
    image->colorPrimaries = settings.colorPrimaries;
    image->transferCharacteristics = settings.transferCharacteristics;
    image->matrixCoefficients = settings.matrixCoefficients;
    image->yuvRange = requestedRange;
    image->alphaPremultiplied = premultiplyAlpha;

    if ((image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY) && (input.requestedFormat != AVIF_PIXEL_FORMAT_NONE) &&
        (input.requestedFormat != AVIF_PIXEL_FORMAT_YUV444)) {
        // User explicitly asked for non YUV444 yuvFormat, while matrixCoefficients was likely
        // set to AVIF_MATRIX_COEFFICIENTS_IDENTITY as a side effect of --lossless,
        // and Identity is only valid with YUV444. Set matrixCoefficients back to the default.
        image->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT601;

        if (settings.cicpExplicitlySet) {
            // Only warn if someone explicitly asked for identity.
            printf("WARNING: matrixCoefficients may not be set to identity (0) when %s. Resetting MC to defaults (%d).\n",
                   (input.requestedFormat == AVIF_PIXEL_FORMAT_YUV400) ? "encoding 4:0:0" : "subsampling",
                   image->matrixCoefficients);
        }
    }

    // --target-size requires multiple encodings of the same files. Cache the input images.
    input.cacheEnabled = (settings.targetSize != -1);

    const avifInputFile * firstFile = avifInputGetFile(&input, /*imageIndex=*/0);
    uint32_t sourceDepth = 0;
    avifBool sourceWasRGB = AVIF_FALSE;
    avifAppSourceTiming firstSourceTiming;
    const avifBool isImageSequence = (!settings.gridDimsPresent) && (settings.layers == 1) && (input.filesCount > 1);
    // Gain maps are not supported for animations or layered images.
    const avifBool ignoreGainMap = settings.ignoreGainMap || isImageSequence || settings.progressive;
    if (!avifInputReadImage(&input,
                            /*imageIndex=*/0,
                            settings.ignoreColorProfile,
                            settings.ignoreExif,
                            settings.ignoreXMP,
                            /*allowChangingCicp=*/!settings.cicpExplicitlySet,
                            ignoreGainMap,
                            image,
                            /*settings=*/NULL, // Must use the setting for first input
                            &sourceDepth,
                            &sourceWasRGB,
                            &firstSourceTiming,
                            settings.chromaDownsampling)) {
        goto cleanup;
    }

    // Check again for -y auto or for y4m input (y4m input ignores input.requestedFormat and
    // retains the format in file).
    if ((image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY) && (image->yuvFormat == AVIF_PIXEL_FORMAT_YUV400)) {
        image->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT601;

        if (settings.cicpExplicitlySet) {
            // Only warn if someone explicitly asked for identity.
            printf("WARNING: matrixCoefficients may not be set to identity (0) when encoding 4:0:0. Resetting MC to defaults (%d).\n",
                   image->matrixCoefficients);
        }
    }
    if ((image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY) && (image->yuvFormat != AVIF_PIXEL_FORMAT_YUV444)) {
        fprintf(stderr, "matrixCoefficients may not be set to identity (0) when subsampling.\n");
        goto cleanup;
    }

    printf("Successfully loaded: %s\n", firstFile->filename);

    // Prepare image timings
    if ((settings.outputTiming.duration == 0) && (settings.outputTiming.timescale == 0) && (firstSourceTiming.duration > 0) &&
        (firstSourceTiming.timescale > 0)) {
        // Set the default duration and timescale to the first image's timing.
        settings.outputTiming = firstSourceTiming;
    } else {
        // Set output timing defaults to 30 fps
        if (settings.outputTiming.duration == 0) {
            settings.outputTiming.duration = 1;
        }
        if (settings.outputTiming.timescale == 0) {
            settings.outputTiming.timescale = 30;
        }
    }

    if ((iccOverride.size && (avifImageSetProfileICC(image, iccOverride.data, iccOverride.size) != AVIF_RESULT_OK)) ||
        (exifOverride.size && (avifImageSetMetadataExif(image, exifOverride.data, exifOverride.size) != AVIF_RESULT_OK)) ||
        (xmpOverride.size && (avifImageSetMetadataXMP(image, xmpOverride.data, xmpOverride.size) != AVIF_RESULT_OK))) {
        fprintf(stderr, "Error when setting overridden metadata: out of memory.\n");
        goto cleanup;
    }

    if (!image->icc.size && !settings.cicpExplicitlySet && (image->colorPrimaries == AVIF_COLOR_PRIMARIES_UNSPECIFIED) &&
        (image->transferCharacteristics == AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED)) {
        // The final image has no ICC profile, the user didn't specify any CICP, and the source
        // image didn't provide any CICP. Explicitly signal SRGB CP/TC here, as 2/2/x will be
        // interpreted as SRGB anyway.
        image->colorPrimaries = AVIF_COLOR_PRIMARIES_SRGB;
        image->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_SRGB;
    }

#if defined(AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION)
    if (image->gainMap && !image->gainMap->altICC.size) {
        if (image->gainMap->altColorPrimaries == AVIF_COLOR_PRIMARIES_UNSPECIFIED) {
            // Assume the alternate image has the same primaries as the base image.
            image->gainMap->altColorPrimaries = image->colorPrimaries;
        }
        if (image->gainMap->altTransferCharacteristics == AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED) {
            // Assume the alternate image is PQ HDR.
            image->gainMap->altTransferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_PQ;
        }
    }
#endif // AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION

    if (settings.paspPresent) {
        image->transformFlags |= AVIF_TRANSFORM_PASP;
        image->pasp.hSpacing = settings.paspValues[0];
        image->pasp.vSpacing = settings.paspValues[1];
    }
    if (cropConversionRequired) {
        if (!convertCropToClap(image->width, image->height, image->yuvFormat, settings.clapValues)) {
            goto cleanup;
        }
        settings.clapValid = AVIF_TRUE;
    }
    if (settings.clapValid) {
        image->transformFlags |= AVIF_TRANSFORM_CLAP;
        image->clap.widthN = settings.clapValues[0];
        image->clap.widthD = settings.clapValues[1];
        image->clap.heightN = settings.clapValues[2];
        image->clap.heightD = settings.clapValues[3];
        image->clap.horizOffN = settings.clapValues[4];
        image->clap.horizOffD = settings.clapValues[5];
        image->clap.vertOffN = settings.clapValues[6];
        image->clap.vertOffD = settings.clapValues[7];

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
            goto cleanup;
        }
    }
    if (irotAngle != 0xff) {
        image->transformFlags |= AVIF_TRANSFORM_IROT;
        image->irot.angle = irotAngle;
    }
    if (imirAxis != 0xff) {
        image->transformFlags |= AVIF_TRANSFORM_IMIR;
        image->imir.axis = imirAxis;
    }
    if (settings.clliPresent) {
        image->clli.maxCLL = (uint16_t)settings.clliValues[0];
        image->clli.maxPALL = (uint16_t)settings.clliValues[1];
    }

    avifBool hasAlpha = (image->alphaPlane && image->alphaRowBytes);
    avifBool usingLosslessColor = (firstFile->settings.quality.value == AVIF_QUALITY_LOSSLESS);
    avifBool usingLosslessAlpha = (firstFile->settings.qualityAlpha.value == AVIF_QUALITY_LOSSLESS);
    avifBool using400 = (image->yuvFormat == AVIF_PIXEL_FORMAT_YUV400);
    avifBool using444 = (image->yuvFormat == AVIF_PIXEL_FORMAT_YUV444);
    avifBool usingFullRange = (image->yuvRange == AVIF_RANGE_FULL);
    avifBool usingIdentityMatrix = (image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY);

    // Guess if the enduser is asking for lossless and enable it so that warnings can be emitted
    if (!lossless && usingLosslessColor && (!hasAlpha || usingLosslessAlpha)) {
        // The enduser is probably expecting lossless. Turn it on and emit warnings
        printf("Quality set to %d, assuming --lossless to enable warnings on potential lossless issues.\n", AVIF_QUALITY_LOSSLESS);
        lossless = AVIF_TRUE;
    }

    // Check for any reasons lossless will fail, and complain loudly
    if (lossless) {
        if (!usingLosslessColor) {
            fprintf(stderr,
                    "WARNING: [--lossless] Color quality (-q or --qcolor) not set to %d. Color output might not be lossless.\n",
                    AVIF_QUALITY_LOSSLESS);
            lossless = AVIF_FALSE;
        }

        if (hasAlpha && !usingLosslessAlpha) {
            fprintf(stderr,
                    "WARNING: [--lossless] Alpha present and alpha quality (--qalpha) not set to %d. Alpha output might not be lossless.\n",
                    AVIF_QUALITY_LOSSLESS);
            lossless = AVIF_FALSE;
        }

        if (usingIdentityMatrix && (sourceDepth != image->depth)) {
            fprintf(stderr,
                    "WARNING: [--lossless] Identity matrix is used but input depth (%d) does not match output depth (%d). Output might not be lossless.\n",
                    sourceDepth,
                    image->depth);
            lossless = AVIF_FALSE;
        }

        if (sourceWasRGB) {
            if (!using444 && !using400) {
                fprintf(stderr,
                        "WARNING: [--lossless] Input data was RGB and YUV "
                        "subsampling (-y) isn't YUV444 or YUV400. Output might "
                        "not be lossless.\n");
                lossless = AVIF_FALSE;
            }

            if (!usingFullRange) {
                fprintf(stderr, "WARNING: [--lossless] Input data was RGB and output range (-r) isn't full. Output might not be lossless.\n");
                lossless = AVIF_FALSE;
            }

            avifBool matrixCoefficientsAreLosslessCompatible = usingIdentityMatrix;
#if defined(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
            matrixCoefficientsAreLosslessCompatible |= (image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO_RE ||
                                                        image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO_RO);
#endif
            if (!matrixCoefficientsAreLosslessCompatible && !using400) {
#if defined(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
                fprintf(stderr, "WARNING: [--lossless] Input data was RGB and matrixCoefficients isn't set to identity (--cicp x/x/0) or YCgCo-Re/Ro (--cicp x/x/15 or x/x/16); Output might not be lossless.\n");
#else
                fprintf(stderr, "WARNING: [--lossless] Input data was RGB and matrixCoefficients isn't set to identity (--cicp x/x/0); Output might not be lossless.\n");
#endif
                lossless = AVIF_FALSE;
            }
        }
    }

    if (settings.gridDimsPresent) {
        // Grid image!

        gridCellCount = settings.gridDims[0] * settings.gridDims[1];
        printf("Preparing to encode a %ux%u grid (%u cells)...\n", settings.gridDims[0], settings.gridDims[1], gridCellCount);

        gridCells = calloc(gridCellCount, sizeof(avifImage *));
        if (gridCells == NULL) {
            fprintf(stderr, "ERROR: memory allocation failure\n");
            goto cleanup;
        }
        gridCells[0] = image; // take ownership of image

        int imageIndex = 1; // The first grid cell was loaded into image (imageIndex 0).
        const avifInputFile * nextFile;
        while ((nextFile = avifInputGetFile(&input, imageIndex)) != NULL) {
            if (imageIndex == 1) {
                printf("Loading additional cells for grid image (%u cells)...\n", gridCellCount);
            }
            if (imageIndex >= (int)gridCellCount) {
                // We have enough, warn and continue
                fprintf(stderr,
                        "WARNING: [--grid] More than %u images were supplied for this %ux%u grid. The rest will be ignored.\n",
                        gridCellCount,
                        settings.gridDims[0],
                        settings.gridDims[1]);
                break;
            }

            // Ensure no settings is set for other cells
            if (memcmp(&nextFile->settings, &emptySettingsReference, sizeof(avifInputFileSettings)) != 0) {
                fprintf(stderr, "ERROR: Grid image cannot use different settings for each cell.\n");
                goto cleanup;
            }

            avifImage * cellImage = avifImageCreateEmpty();
            if (!cellImage) {
                fprintf(stderr, "ERROR: Out of memory\n");
                goto cleanup;
            }
            cellImage->colorPrimaries = image->colorPrimaries;
            cellImage->transferCharacteristics = image->transferCharacteristics;
            cellImage->matrixCoefficients = image->matrixCoefficients;
            cellImage->yuvRange = image->yuvRange;
            cellImage->alphaPremultiplied = image->alphaPremultiplied;
            gridCells[imageIndex] = cellImage;

            // Ignore ICC, Exif and XMP because only the metadata of the first frame is taken into
            // account by the libavif API.
            if (!avifInputReadImage(&input,
                                    imageIndex,
                                    /*ignoreColorProfile=*/AVIF_TRUE,
                                    /*ignoreExif=*/AVIF_TRUE,
                                    /*ignoreXMP=*/AVIF_TRUE,
                                    /*allowChangingCicp=*/AVIF_FALSE,
                                    settings.ignoreGainMap,
                                    cellImage,
                                    /*settings=*/NULL,
                                    /*outDepth=*/NULL,
                                    /*sourceIsRGB=*/NULL,
                                    /*sourceTiming=*/NULL,
                                    settings.chromaDownsampling)) {
                goto cleanup;
            }
            // Let avifEncoderAddImageGrid() verify the grid integrity (valid cell sizes, depths etc.).

            ++imageIndex;
        }

        if (imageIndex == 1) {
            printf("Single image input for a grid image. Attempting to split into %u cells...\n", gridCellCount);
            gridSplitImage = image;
            gridCells[0] = NULL;

            if (!avifImageSplitGrid(gridSplitImage, settings.gridDims[0], settings.gridDims[1], gridCells)) {
                goto cleanup;
            }
        } else if (imageIndex != (int)gridCellCount) {
            fprintf(stderr, "ERROR: Not enough input files for grid image! (expecting %u, or a single image to be split)\n", gridCellCount);
            goto cleanup;
        }
        // TODO(yguyon): Check if it is possible to use frames from a single input file as grid cells. Maybe forbid it.
    }

    const char * lossyHint = " (Lossy)";
    if (lossless) {
        lossyHint = " (Lossless)";
    }
    printf("AVIF to be written:%s\n", lossyHint);
    const avifImage * avif = gridCells ? gridCells[0] : image;
    avifBool gainMapPresent = AVIF_FALSE;
#if defined(AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION)
    gainMapPresent = (avif->gainMap && avif->gainMap->image);
#endif
    avifImageDump(avif,
                  settings.gridDims[0],
                  settings.gridDims[1],
                  gainMapPresent,
                  settings.layers > 1 ? AVIF_PROGRESSIVE_STATE_AVAILABLE : AVIF_PROGRESSIVE_STATE_UNAVAILABLE);

    avifEncodedByteSizes byteSizes = { 0, 0, 0 };
    if (!avifEncodeImages(&settings, &input, firstFile, image, (const avifImage * const *)gridCells, &raw, &byteSizes)) {
        goto cleanup;
    }

    printf("Encoded successfully.\n");
    printf(" * Color total size: %" AVIF_FMT_ZU " bytes\n", byteSizes.colorSizeBytes);
    printf(" * Alpha total size: %" AVIF_FMT_ZU " bytes\n", byteSizes.alphaSizeBytes);
    if (byteSizes.gainMapSizeBytes > 0) {
        printf(" * Gain Map AV1 total size: %" AVIF_FMT_ZU " bytes\n", byteSizes.gainMapSizeBytes);
    }
    if (isImageSequence) {
        if (settings.repetitionCount == AVIF_REPETITION_COUNT_INFINITE) {
            printf(" * Repetition Count: Infinite\n");
        } else {
            printf(" * Repetition Count: %d\n", settings.repetitionCount);
        }
    }
    if (noOverwrite && fileExists(outputFilename)) {
        // check again before write
        fprintf(stderr, "ERROR: output file %s already exists and --no-overwrite was specified\n", outputFilename);
        goto cleanup;
    }
    FILE * f = fopen(outputFilename, "wb");
    if (!f) {
        fprintf(stderr, "ERROR: Failed to open file for write: %s\n", outputFilename);
        goto cleanup;
    }
    if (fwrite(raw.data, 1, raw.size, f) != raw.size) {
        fprintf(stderr, "Failed to write %" AVIF_FMT_ZU " bytes: %s\n", raw.size, outputFilename);
        goto cleanup;
    } else {
        printf("Wrote AVIF: %s\n", outputFilename);
    }
    fclose(f);
    returnCode = 0;

cleanup:
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
    avifRWDataFree(&raw);
    avifRWDataFree(&exifOverride);
    avifRWDataFree(&xmpOverride);
    avifRWDataFree(&iccOverride);
    avifCodecSpecificOptionsFree(&pendingSettings.codecSpecificOptions);
    while (input.cacheCount) {
        --input.cacheCount;
        if (input.cache[input.cacheCount].image) {
            avifImageDestroy(input.cache[input.cacheCount].image);
        }
    }
    free(input.cache);
    while (input.filesCount) {
        --input.filesCount;
        avifInputFile * file = &input.files[input.filesCount];
        avifCodecSpecificOptionsFree(&file->settings.codecSpecificOptions);
    }
    free(input.files);

    return returnCode;
}
