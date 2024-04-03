// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#ifndef AVIF_INTERNAL_H
#define AVIF_INTERNAL_H

#include "avif/avif.h" // IWYU pragma: export

#ifdef __cplusplus
extern "C" {
#endif

#if defined(AVIF_DLL) && defined(AVIF_USING_STATIC_LIBS)
#error "Your target is linking against avif and avif_internal: only one should be chosen"
#endif

// Yes, clamp macros are nasty. Do not use them.
#define AVIF_CLAMP(x, low, high) (((x) < (low)) ? (low) : (((high) < (x)) ? (high) : (x)))
#define AVIF_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define AVIF_MAX(a, b) (((a) > (b)) ? (a) : (b))

// Used for debugging. Define AVIF_BREAK_ON_ERROR to catch the earliest failure during encoding or decoding.
#if defined(AVIF_BREAK_ON_ERROR)
static inline void avifBreakOnError()
{
    // Same mechanism as OpenCV's error() function, or replace by a breakpoint.
    int * p = NULL;
    *p = 0;
}
#else
#define avifBreakOnError()
#endif

// Used by stream related things.
#define AVIF_CHECK(A)           \
    do {                        \
        if (!(A)) {             \
            avifBreakOnError(); \
            return AVIF_FALSE;  \
        }                       \
    } while (0)

// Used instead of CHECK if needing to return a specific error on failure, instead of AVIF_FALSE
#define AVIF_CHECKERR(A, ERR)   \
    do {                        \
        if (!(A)) {             \
            avifBreakOnError(); \
            return ERR;         \
        }                       \
    } while (0)

// Forward any error to the caller now or continue execution.
#define AVIF_CHECKRES(A)                  \
    do {                                  \
        const avifResult result__ = (A);  \
        if (result__ != AVIF_RESULT_OK) { \
            avifBreakOnError();           \
            return result__;              \
        }                                 \
    } while (0)

// AVIF_ASSERT_OR_RETURN() can be used instead of assert() for extra security in release builds.
#ifdef NDEBUG
#define AVIF_ASSERT_OR_RETURN(A) AVIF_CHECKERR((A), AVIF_RESULT_INTERNAL_ERROR)
#else
#define AVIF_ASSERT_OR_RETURN(A) assert(A)
#endif

// ---------------------------------------------------------------------------
// URNs and Content-Types

#define AVIF_URN_ALPHA0 "urn:mpeg:mpegB:cicp:systems:auxiliary:alpha"
#define AVIF_URN_ALPHA1 "urn:mpeg:hevc:2015:auxid:1"

#define AVIF_CONTENT_TYPE_XMP "application/rdf+xml"

// ---------------------------------------------------------------------------
// Utils

float avifRoundf(float v);

// H (host) is platform-dependent. Could be little- or big-endian.
// N (network) is big-endian: most- to least-significant bytes.
// C (custom) is little-endian: least- to most-significant bytes.
// Never read N or C values; only access after casting to uint8_t*.
uint16_t avifHTONS(uint16_t s);
uint16_t avifNTOHS(uint16_t s);
uint16_t avifCTOHS(uint16_t s);
uint32_t avifHTONL(uint32_t l);
uint32_t avifNTOHL(uint32_t l);
uint32_t avifCTOHL(uint32_t l);
uint64_t avifHTON64(uint64_t l);
uint64_t avifNTOH64(uint64_t l);

void avifCalcYUVCoefficients(const avifImage * image, float * outR, float * outG, float * outB);

typedef float (*avifTransferFunction)(float);
// Returns a function to map from gamma-encoded values in the [0.0, 1.0] range to linear extended SDR values.
// Extended SDR values are in [0.0, 1.0] for SDR transfer chracteristics (all transfer characteristics except PQ and HLG)
// and can go beyond 1.0 for HDR transfer characteristics:
// - For AVIF_TRANSFER_CHARACTERISTICS_PQ, the linear range is [0.0, 10000/203]
// - For AVIF_TRANSFER_CHARACTERISTICS_HLG, the linear range is [0.0, 1000/203]
avifTransferFunction avifTransferCharacteristicsGetGammaToLinearFunction(avifTransferCharacteristics atc);
// Same as above in the opposite direction. toGamma(toLinear(v)) ~= v.
avifTransferFunction avifTransferCharacteristicsGetLinearToGammaFunction(avifTransferCharacteristics atc);

// Computes the RGB->YUV conversion coefficients kr, kg, kb, such that Y=kr*R+kg*G+kb*B.
void avifColorPrimariesComputeYCoeffs(avifColorPrimaries colorPrimaries, float coeffs[3]);

// Computes a conversion matrix from RGB to XYZ with a D50 white point.
AVIF_NODISCARD avifBool avifColorPrimariesComputeRGBToXYZD50Matrix(avifColorPrimaries colorPrimaries, double coeffs[3][3]);
// Computes a conversion matrix from XYZ with a D50 white point to RGB.
AVIF_NODISCARD avifBool avifColorPrimariesComputeXYZD50ToRGBMatrix(avifColorPrimaries colorPrimaries, double coeffs[3][3]);
// Computes the RGB->RGB conversion matrix to convert from one set of RGB primaries to another.
AVIF_NODISCARD avifBool avifColorPrimariesComputeRGBToRGBMatrix(avifColorPrimaries srcColorPrimaries,
                                                                avifColorPrimaries dstColorPrimaries,
                                                                double coeffs[3][3]);
// Converts the given linear RGB pixel from one color space to another using the provided coefficients.
// The coefficients can be obtained with avifColorPrimariesComputeRGBToRGBMatrix().
// The output values are not clamped and may be < 0 or > 1.
void avifLinearRGBConvertColorSpace(float rgb[4], double coeffs[3][3]);

#define AVIF_ARRAY_DECLARE(TYPENAME, ITEMSTYPE, ITEMSNAME) \
    typedef struct TYPENAME                                \
    {                                                      \
        ITEMSTYPE * ITEMSNAME;                             \
        uint32_t elementSize;                              \
        uint32_t count;                                    \
        uint32_t capacity;                                 \
    } TYPENAME
AVIF_NODISCARD avifBool avifArrayCreate(void * arrayStruct, uint32_t elementSize, uint32_t initialCapacity);
AVIF_NODISCARD void * avifArrayPush(void * arrayStruct);
void avifArrayPop(void * arrayStruct);
void avifArrayDestroy(void * arrayStruct);

void avifFractionSimplify(avifFraction * f);
// Makes the fractions have a common denominator.
AVIF_NODISCARD avifBool avifFractionCD(avifFraction * a, avifFraction * b);
AVIF_NODISCARD avifBool avifFractionAdd(avifFraction a, avifFraction b, avifFraction * result);
AVIF_NODISCARD avifBool avifFractionSub(avifFraction a, avifFraction b, avifFraction * result);

// Creates an int32 fraction that is approximately equal to 'v'.
// Returns AVIF_FALSE if 'v' is NaN or abs(v) is > INT32_MAX.
AVIF_NODISCARD avifBool avifDoubleToSignedFraction(double v, int32_t * numerator, uint32_t * denominator);
// Creates a uint32 fraction that is approximately equal to 'v'.
// Returns AVIF_FALSE if 'v' is < 0 or > UINT32_MAX or NaN.
AVIF_NODISCARD avifBool avifDoubleToUnsignedFraction(double v, uint32_t * numerator, uint32_t * denominator);

void avifImageSetDefaults(avifImage * image);
// Copies all fields that do not need to be freed/allocated from srcImage to dstImage.
void avifImageCopyNoAlloc(avifImage * dstImage, const avifImage * srcImage);

// Copies the samples from srcImage to dstImage. dstImage must be allocated.
// srcImage and dstImage must have the same width, height, and depth.
// If the AVIF_PLANES_YUV bit is set in planes, then srcImage and dstImage must have the same yuvFormat.
// Ignores the gainMap field (which exists only if AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP is defined).
void avifImageCopySamples(avifImage * dstImage, const avifImage * srcImage, avifPlanesFlags planes);

typedef struct avifAlphaParams
{
    uint32_t width;
    uint32_t height;

    uint32_t srcDepth;
    const uint8_t * srcPlane;
    uint32_t srcRowBytes;
    uint32_t srcOffsetBytes;
    uint32_t srcPixelBytes;

    uint32_t dstDepth;
    uint8_t * dstPlane;
    uint32_t dstRowBytes;
    uint32_t dstOffsetBytes;
    uint32_t dstPixelBytes;

} avifAlphaParams;

void avifFillAlpha(const avifAlphaParams * params);
void avifReformatAlpha(const avifAlphaParams * params);

typedef enum avifReformatMode
{
    AVIF_REFORMAT_MODE_YUV_COEFFICIENTS = 0, // Normal YUV conversion using coefficients
    AVIF_REFORMAT_MODE_IDENTITY,             // Pack GBR directly into YUV planes (AVIF_MATRIX_COEFFICIENTS_IDENTITY)
    AVIF_REFORMAT_MODE_YCGCO,                // YUV conversion using AVIF_MATRIX_COEFFICIENTS_YCGCO
#if defined(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
    AVIF_REFORMAT_MODE_YCGCO_RE, // YUV conversion using AVIF_MATRIX_COEFFICIENTS_YCGCO_RE
    AVIF_REFORMAT_MODE_YCGCO_RO, // YUV conversion using AVIF_MATRIX_COEFFICIENTS_YCGCO_RO
#endif
} avifReformatMode;

typedef enum avifAlphaMultiplyMode
{
    AVIF_ALPHA_MULTIPLY_MODE_NO_OP = 0,
    AVIF_ALPHA_MULTIPLY_MODE_MULTIPLY,
    AVIF_ALPHA_MULTIPLY_MODE_UNMULTIPLY
} avifAlphaMultiplyMode;

// Information about an RGB color space.
typedef struct avifRGBColorSpaceInfo
{
    uint32_t channelBytes; // Number of bytes per channel.
    uint32_t pixelBytes;   // Number of bytes per pixel (= channelBytes * num channels).
    uint32_t offsetBytesR; // Offset in bytes of the red channel in a pixel.
    uint32_t offsetBytesG; // Offset in bytes of the green channel in a pixel.
    uint32_t offsetBytesB; // Offset in bytes of the blue channel in a pixel.
    uint32_t offsetBytesA; // Offset in bytes of the alpha channel in a pixel.

    int maxChannel;    // Maximum value for a channel (e.g. 255 for 8 bit).
    float maxChannelF; // Same as maxChannel but as a float.
} avifRGBColorSpaceInfo;

avifBool avifGetRGBColorSpaceInfo(const avifRGBImage * rgb, avifRGBColorSpaceInfo * info);

// Information about a YUV color space.
typedef struct avifYUVColorSpaceInfo
{
    // YUV coefficients. Y = kr*R + kg*G + kb*B.
    float kr;
    float kg;
    float kb;

    uint32_t channelBytes; // Number of bytes per channel.
    uint32_t depth;        // Bit depth.
    avifRange range;       // Full or limited range.
    int maxChannel;        // Maximum value for a channel (e.g. 255 for 8 bit).
    float biasY;           // Minimum Y value.
    float biasUV;          // The value of 0.5 for the appropriate bit depth (128 for 8 bit, 512 for 10 bit, 2048 for 12 bit).
    float rangeY;          // Difference between max and min Y.
    float rangeUV;         // Difference between max and min UV.

    avifPixelFormatInfo formatInfo; // Chroma subsampling information.
    avifReformatMode mode;          // Appropriate RGB<->YUV conversion mode.
} avifYUVColorSpaceInfo;

avifBool avifGetYUVColorSpaceInfo(const avifImage * image, avifYUVColorSpaceInfo * info);

typedef struct avifReformatState
{
    avifRGBColorSpaceInfo rgb;
    avifYUVColorSpaceInfo yuv;
} avifReformatState;

// Retrieves the pixel value at position (x, y) expressed as floats in [0, 1]. If the image's format doesn't have alpha,
// rgbaPixel[3] is set to 1.0f.
void avifGetRGBAPixel(const avifRGBImage * src, uint32_t x, uint32_t y, const avifRGBColorSpaceInfo * info, float rgbaPixel[4]);
// Sets the pixel value at position (i, j) from RGBA values expressed as floats in [0, 1]. If the image's format doesn't
// support alpha, rgbaPixel[3] is ignored.
void avifSetRGBAPixel(const avifRGBImage * dst, uint32_t x, uint32_t y, const avifRGBColorSpaceInfo * info, const float rgbaPixel[4]);

// Returns:
// * AVIF_RESULT_OK              - Converted successfully with libyuv
// * AVIF_RESULT_NOT_IMPLEMENTED - The fast path for this combination is not implemented with libyuv, use built-in RGB conversion
// * [any other error]           - Return error to caller
avifResult avifImageRGBToYUVLibYUV(avifImage * image, const avifRGBImage * rgb);

// Parameters:
// * image - input YUV image
// * rgb - output RGB image
// * reformatAlpha - if set to AVIF_TRUE, the function will attempt to copy the alpha channel to the output RGB image using
// libyuv.
// * alphaReformattedWithLibYUV - Output parameter. If reformatAlpha is set to true and libyuv was able to copy over the alpha
// channel, then this will be set to AVIF_TRUE. Otherwise, this will be set to AVIF_FALSE. The value in this parameter is valid
// only if the return value of the function is AVIF_RESULT_OK or AVIF_RESULT_NOT_IMPLEMENTED.
// Returns:
// * AVIF_RESULT_OK              - Converted successfully with libyuv
// * AVIF_RESULT_NOT_IMPLEMENTED - The fast path for this combination is not implemented with libyuv, use built-in YUV conversion
// * [any other error]           - Return error to caller
avifResult avifImageYUVToRGBLibYUV(const avifImage * image, avifRGBImage * rgb, avifBool reformatAlpha, avifBool * alphaReformattedWithLibYUV);

// Returns:
// * AVIF_RESULT_OK              - Converted successfully with libsharpyuv
// * AVIF_RESULT_NOT_IMPLEMENTED - libsharpyuv is not compiled in, or doesn't support this type of input
// * [any other error]           - Return error to caller
avifResult avifImageRGBToYUVLibSharpYUV(avifImage * image, const avifRGBImage * rgb, const avifReformatState * state);

// Returns:
// * AVIF_RESULT_OK               - Converted successfully with libyuv.
// * AVIF_RESULT_NOT_IMPLEMENTED  - The fast path for this conversion is not implemented with libyuv, use built-in conversion.
// * AVIF_RESULT_INVALID_ARGUMENT - Return error to caller.
avifResult avifRGBImageToF16LibYUV(avifRGBImage * rgb);

// Returns:
// * AVIF_RESULT_OK              - (Un)Premultiply successfully with libyuv
// * AVIF_RESULT_NOT_IMPLEMENTED - The fast path for this combination is not implemented with libyuv, use built-in (Un)Premultiply
// * [any other error]           - Return error to caller
avifResult avifRGBImagePremultiplyAlphaLibYUV(avifRGBImage * rgb);
avifResult avifRGBImageUnpremultiplyAlphaLibYUV(avifRGBImage * rgb);

AVIF_NODISCARD avifBool avifDimensionsTooLarge(uint32_t width, uint32_t height, uint32_t imageSizeLimit, uint32_t imageDimensionLimit);

// Given the number of encoding threads or decoding threads available and the image dimensions,
// chooses suitable values of *tileRowsLog2 and *tileColsLog2.
//
// Note: Although avifSetTileConfiguration() is only used in src/write.c and could be a static
// function in that file, it is defined as an internal global function so that it can be tested by
// unit tests.
void avifSetTileConfiguration(int threads, uint32_t width, uint32_t height, int * tileRowsLog2, int * tileColsLog2);

// ---------------------------------------------------------------------------
// Scaling

// Scales the YUV/A planes in-place.
AVIF_NODISCARD avifResult avifImageScaleWithLimit(avifImage * image,
                                                  uint32_t dstWidth,
                                                  uint32_t dstHeight,
                                                  uint32_t imageSizeLimit,
                                                  uint32_t imageDimensionLimit,
                                                  avifDiagnostics * diag);

// ---------------------------------------------------------------------------
// AVIF item category

typedef enum avifItemCategory
{
    AVIF_ITEM_COLOR,
    AVIF_ITEM_ALPHA,
#if defined(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
    AVIF_ITEM_GAIN_MAP,
#endif
    AVIF_ITEM_CATEGORY_COUNT
} avifItemCategory;

// ---------------------------------------------------------------------------

#if defined(AVIF_ENABLE_EXPERIMENTAL_AVIR)
// AVIF color_type field meaning in CondensedImageBox
typedef enum avifMiniColorType
{
    AVIF_MINI_COLOR_TYPE_SRGB = 0,
    AVIF_MINI_COLOR_TYPE_NCLX_5BIT = 1,
    AVIF_MINI_COLOR_TYPE_NCLX_8BIT = 2,
    AVIF_MINI_COLOR_TYPE_ICC = 3
} avifMiniColorType;
#endif // AVIF_ENABLE_EXPERIMENTAL_AVIR

// ---------------------------------------------------------------------------
// Grid AVIF images

// Returns false if the tiles in a grid image violate any standards.
// The image contains imageW*imageH pixels. The tiles are of tileW*tileH pixels each.
AVIF_NODISCARD avifBool avifAreGridDimensionsValid(avifPixelFormat yuvFormat,
                                                   uint32_t imageW,
                                                   uint32_t imageH,
                                                   uint32_t tileW,
                                                   uint32_t tileH,
                                                   avifDiagnostics * diag);

// ---------------------------------------------------------------------------
// Metadata

// Attempts to parse the image->exif payload for Exif orientation and sets image->transformFlags, image->irot and
// image->imir on success. Returns AVIF_RESULT_INVALID_EXIF_PAYLOAD on failure.
avifResult avifImageExtractExifOrientationToIrotImir(avifImage * image);

// ---------------------------------------------------------------------------
// avifCodecDecodeInput

// Legal spatial_id values are [0,1,2,3], so this serves as a sentinel value for "do not filter by spatial_id"
#define AVIF_SPATIAL_ID_UNSET 0xff

typedef struct avifDecodeSample
{
    avifROData data;
    avifBool ownsData;
    avifBool partialData; // if true, data exists but doesn't have all of the sample in it

    uint32_t itemID;   // if non-zero, data comes from a mergedExtents buffer in an avifDecoderItem, not a file offset
    uint64_t offset;   // additional offset into data. Can be used to offset into an itemID's payload as well.
    size_t size;       //
    uint8_t spatialID; // If set to a value other than AVIF_SPATIAL_ID_UNSET, output frames from this sample should be
                       // skipped until the output frame's spatial_id matches this ID.
    avifBool sync;     // is sync sample (keyframe)
} avifDecodeSample;
AVIF_ARRAY_DECLARE(avifDecodeSampleArray, avifDecodeSample, sample);

typedef struct avifCodecDecodeInput
{
    avifDecodeSampleArray samples;
    avifBool allLayers;            // if true, the underlying codec must decode all layers, not just the best layer
    avifItemCategory itemCategory; // category of item being decoded
} avifCodecDecodeInput;

AVIF_NODISCARD avifCodecDecodeInput * avifCodecDecodeInputCreate(void);
void avifCodecDecodeInputDestroy(avifCodecDecodeInput * decodeInput);

// ---------------------------------------------------------------------------
// avifCodecEncodeOutput

typedef struct avifEncodeSample
{
    avifRWData data;
    avifBool sync; // is sync sample (keyframe)
} avifEncodeSample;
AVIF_ARRAY_DECLARE(avifEncodeSampleArray, avifEncodeSample, sample);

typedef struct avifCodecEncodeOutput
{
    avifEncodeSampleArray samples;
} avifCodecEncodeOutput;

AVIF_NODISCARD avifCodecEncodeOutput * avifCodecEncodeOutputCreate(void);
avifResult avifCodecEncodeOutputAddSample(avifCodecEncodeOutput * encodeOutput, const uint8_t * data, size_t len, avifBool sync);
void avifCodecEncodeOutputDestroy(avifCodecEncodeOutput * encodeOutput);

// ---------------------------------------------------------------------------
// avifCodecSpecificOptions (key/value string pairs for advanced tuning)

typedef struct avifCodecSpecificOption
{
    char * key;   // Must be a simple lowercase alphanumeric string
    char * value; // Free-form string to be interpreted by the codec
} avifCodecSpecificOption;
AVIF_ARRAY_DECLARE(avifCodecSpecificOptions, avifCodecSpecificOption, entries);

// Returns NULL if a memory allocation failed.
AVIF_NODISCARD avifCodecSpecificOptions * avifCodecSpecificOptionsCreate(void);
void avifCodecSpecificOptionsClear(avifCodecSpecificOptions * csOptions);
void avifCodecSpecificOptionsDestroy(avifCodecSpecificOptions * csOptions);
avifResult avifCodecSpecificOptionsSet(avifCodecSpecificOptions * csOptions, const char * key, const char * value); // if(value==NULL), key is deleted

// ---------------------------------------------------------------------------
// avifCodecType (underlying video format)

// Alliance for Open Media video formats that can be used in the AVIF image format.
typedef enum avifCodecType
{
    AVIF_CODEC_TYPE_UNKNOWN,
    AVIF_CODEC_TYPE_AV1,
#if defined(AVIF_CODEC_AVM)
    AVIF_CODEC_TYPE_AV2, // Experimental.
#endif
} avifCodecType;

// Returns AVIF_CODEC_TYPE_UNKNOWN unless the chosen codec is available with the requiredFlags.
avifCodecType avifCodecTypeFromChoice(avifCodecChoice choice, avifCodecFlags requiredFlags);

// ---------------------------------------------------------------------------
// avifCodec (abstraction layer to use different codec implementations)

struct avifCodec;
struct avifCodecInternal;

typedef enum avifEncoderChange
{
    AVIF_ENCODER_CHANGE_MIN_QUANTIZER = (1u << 0),
    AVIF_ENCODER_CHANGE_MAX_QUANTIZER = (1u << 1),
    AVIF_ENCODER_CHANGE_MIN_QUANTIZER_ALPHA = (1u << 2),
    AVIF_ENCODER_CHANGE_MAX_QUANTIZER_ALPHA = (1u << 3),
    AVIF_ENCODER_CHANGE_TILE_ROWS_LOG2 = (1u << 4),
    AVIF_ENCODER_CHANGE_TILE_COLS_LOG2 = (1u << 5),
    AVIF_ENCODER_CHANGE_QUANTIZER = (1u << 6),
    AVIF_ENCODER_CHANGE_QUANTIZER_ALPHA = (1u << 7),
    AVIF_ENCODER_CHANGE_SCALING_MODE = (1u << 8),

    AVIF_ENCODER_CHANGE_CODEC_SPECIFIC = (1u << 31)
} avifEncoderChange;
typedef uint32_t avifEncoderChanges;

typedef avifBool (*avifCodecGetNextImageFunc)(struct avifCodec * codec,
                                              struct avifDecoder * decoder,
                                              const avifDecodeSample * sample,
                                              avifBool alpha,
                                              avifBool * isLimitedRangeAlpha,
                                              avifImage * image);
// EncodeImage and EncodeFinish are not required to always emit a sample, but when all images are
// encoded and EncodeFinish is called, the number of samples emitted must match the number of submitted frames.
// avifCodecEncodeImageFunc may return AVIF_RESULT_UNKNOWN_ERROR to automatically emit the appropriate
// AVIF_RESULT_ENCODE_COLOR_FAILED or AVIF_RESULT_ENCODE_ALPHA_FAILED depending on the alpha argument.
// avifCodecEncodeImageFunc should use tileRowsLog2 and tileColsLog2 instead of
// encoder->tileRowsLog2, encoder->tileColsLog2, and encoder->autoTiling. The caller of
// avifCodecEncodeImageFunc is responsible for automatic tiling if encoder->autoTiling is set to
// AVIF_TRUE. The actual tiling values are passed to avifCodecEncodeImageFunc as parameters.
// Similarly, avifCodecEncodeImageFunc should use the quantizer parameter instead of
// encoder->quality and encoder->qualityAlpha. If disableLaggedOutput is AVIF_TRUE, then the encoder will emit the output frame
// without any lag (if supported). Note that disableLaggedOutput is only used by the first call to this function (which
// initializes the encoder) and is ignored by the subsequent calls.
//
// Note: The caller of avifCodecEncodeImageFunc always passes encoder->data->tileRowsLog2 and
// encoder->data->tileColsLog2 as the tileRowsLog2 and tileColsLog2 arguments. Because
// encoder->data is of a struct type defined in src/write.c, avifCodecEncodeImageFunc cannot
// dereference encoder->data and has to receive encoder->data->tileRowsLog2 and
// encoder->data->tileColsLog2 via function parameters.
typedef avifResult (*avifCodecEncodeImageFunc)(struct avifCodec * codec,
                                               avifEncoder * encoder,
                                               const avifImage * image,
                                               avifBool alpha,
                                               int tileRowsLog2,
                                               int tileColsLog2,
                                               int quantizer,
                                               avifEncoderChanges encoderChanges,
                                               avifBool disableLaggedOutput,
                                               avifAddImageFlags addImageFlags,
                                               avifCodecEncodeOutput * output);
typedef avifBool (*avifCodecEncodeFinishFunc)(struct avifCodec * codec, avifCodecEncodeOutput * output);
typedef void (*avifCodecDestroyInternalFunc)(struct avifCodec * codec);

typedef struct avifCodec
{
    avifCodecSpecificOptions * csOptions; // Contains codec-specific key/value pairs for advanced tuning.
                                          // If a codec uses a value, it must mark it as used.
                                          // This array is NOT owned by avifCodec.
    struct avifCodecInternal * internal;  // up to each codec to use how it wants
                                          //
    avifDiagnostics * diag;               // Shallow copy; owned by avifEncoder or avifDecoder
                                          //
    uint8_t operatingPoint;               // Operating point, defaults to 0.
    avifBool allLayers;                   // if true, the underlying codec must decode all layers, not just the best layer

    avifCodecGetNextImageFunc getNextImage;
    avifCodecEncodeImageFunc encodeImage;
    avifCodecEncodeFinishFunc encodeFinish;
    avifCodecDestroyInternalFunc destroyInternal;
} avifCodec;

avifResult avifCodecCreate(avifCodecChoice choice, avifCodecFlags requiredFlags, avifCodec ** codec);
void avifCodecDestroy(avifCodec * codec);

AVIF_NODISCARD avifCodec * avifCodecCreateAOM(void);   // requires AVIF_CODEC_AOM (codec_aom.c)
const char * avifCodecVersionAOM(void);                // requires AVIF_CODEC_AOM (codec_aom.c)
AVIF_NODISCARD avifCodec * avifCodecCreateDav1d(void); // requires AVIF_CODEC_DAV1D (codec_dav1d.c)
const char * avifCodecVersionDav1d(void);              // requires AVIF_CODEC_DAV1D (codec_dav1d.c)
AVIF_NODISCARD avifCodec * avifCodecCreateGav1(void);  // requires AVIF_CODEC_LIBGAV1 (codec_libgav1.c)
const char * avifCodecVersionGav1(void);               // requires AVIF_CODEC_LIBGAV1 (codec_libgav1.c)
AVIF_NODISCARD avifCodec * avifCodecCreateRav1e(void); // requires AVIF_CODEC_RAV1E (codec_rav1e.c)
const char * avifCodecVersionRav1e(void);              // requires AVIF_CODEC_RAV1E (codec_rav1e.c)
AVIF_NODISCARD avifCodec * avifCodecCreateSvt(void);   // requires AVIF_CODEC_SVT (codec_svt.c)
const char * avifCodecVersionSvt(void);                // requires AVIF_CODEC_SVT (codec_svt.c)
AVIF_NODISCARD avifCodec * avifCodecCreateAVM(void);   // requires AVIF_CODEC_AVM (codec_avm.c)
const char * avifCodecVersionAVM(void);                // requires AVIF_CODEC_AVM (codec_avm.c)

// ---------------------------------------------------------------------------
// avifDiagnostics

#ifdef __clang__
__attribute__((__format__(__printf__, 2, 3)))
#endif
void avifDiagnosticsPrintf(avifDiagnostics * diag, const char * format, ...);

#if defined(AVIF_ENABLE_COMPLIANCE_WARDEN)
avifResult avifIsCompliant(const uint8_t * data, size_t size);
#endif

// ---------------------------------------------------------------------------
// avifStream
//
// In network byte order (big-endian) unless otherwise specified.

typedef size_t avifBoxMarker;

typedef struct avifBoxHeader
{
    // Size of the box in bytes, excluding the box header.
    size_t size;

    uint8_t type[4];
} avifBoxHeader;

typedef struct avifROStream
{
    avifROData * raw;

    // Index of the next byte in the raw stream.
    size_t offset;

    // If 0, byte-aligned functions can be used (avifROStreamRead() etc.).
    // Otherwise, it represents the number of bits already used in the last byte
    // (located at offset-1).
    size_t numUsedBitsInPartialByte;

    // Error information, if any.
    avifDiagnostics * diag;
    const char * diagContext;
} avifROStream;

const uint8_t * avifROStreamCurrent(avifROStream * stream);
void avifROStreamStart(avifROStream * stream, avifROData * raw, avifDiagnostics * diag, const char * diagContext);
size_t avifROStreamOffset(const avifROStream * stream);
void avifROStreamSetOffset(avifROStream * stream, size_t offset);

AVIF_NODISCARD avifBool avifROStreamHasBytesLeft(const avifROStream * stream, size_t byteCount);
size_t avifROStreamRemainingBytes(const avifROStream * stream);
// The following functions require byte alignment.
AVIF_NODISCARD avifBool avifROStreamSkip(avifROStream * stream, size_t byteCount);
AVIF_NODISCARD avifBool avifROStreamRead(avifROStream * stream, uint8_t * data, size_t size);
AVIF_NODISCARD avifBool avifROStreamReadU16(avifROStream * stream, uint16_t * v);
AVIF_NODISCARD avifBool avifROStreamReadU16Endianness(avifROStream * stream, uint16_t * v, avifBool littleEndian);
AVIF_NODISCARD avifBool avifROStreamReadU32(avifROStream * stream, uint32_t * v);
AVIF_NODISCARD avifBool avifROStreamReadU32Endianness(avifROStream * stream, uint32_t * v, avifBool littleEndian);
AVIF_NODISCARD avifBool avifROStreamReadUX8(avifROStream * stream, uint64_t * v, uint64_t factor); // Reads a factor*8 sized uint, saves in v
AVIF_NODISCARD avifBool avifROStreamReadU64(avifROStream * stream, uint64_t * v);
AVIF_NODISCARD avifBool avifROStreamReadString(avifROStream * stream, char * output, size_t outputSize);
AVIF_NODISCARD avifBool avifROStreamReadBoxHeader(avifROStream * stream, avifBoxHeader * header); // This fails if the size reported by the header cannot fit in the stream
AVIF_NODISCARD avifBool avifROStreamReadBoxHeaderPartial(avifROStream * stream, avifBoxHeader * header); // This doesn't require that the full box can fit in the stream
AVIF_NODISCARD avifBool avifROStreamReadVersionAndFlags(avifROStream * stream, uint8_t * version, uint32_t * flags); // version and flags ptrs are both optional
AVIF_NODISCARD avifBool avifROStreamReadAndEnforceVersion(avifROStream * stream, uint8_t enforcedVersion); // currently discards flags
// The following functions can write non-aligned bits.
AVIF_NODISCARD avifBool avifROStreamReadBits8(avifROStream * stream, uint8_t * v, size_t bitCount);
AVIF_NODISCARD avifBool avifROStreamReadBits(avifROStream * stream, uint32_t * v, size_t bitCount);
AVIF_NODISCARD avifBool avifROStreamReadVarInt(avifROStream * stream, uint32_t * v);

typedef struct avifRWStream
{
    avifRWData * raw;

    // Index of the next byte in the raw stream.
    size_t offset;

    // If 0, byte-aligned functions can be used (avifRWStreamWrite() etc.).
    // Otherwise, it represents the number of bits already used in the last byte
    // (located at offset-1).
    size_t numUsedBitsInPartialByte;
} avifRWStream;

void avifRWStreamStart(avifRWStream * stream, avifRWData * raw);
size_t avifRWStreamOffset(const avifRWStream * stream);
void avifRWStreamSetOffset(avifRWStream * stream, size_t offset);

void avifRWStreamFinishWrite(avifRWStream * stream);
// The following functions require byte alignment.
avifResult avifRWStreamWrite(avifRWStream * stream, const void * data, size_t size);
avifResult avifRWStreamWriteChars(avifRWStream * stream, const char * chars, size_t size);
avifResult avifRWStreamWriteBox(avifRWStream * stream, const char * type, size_t contentSize, avifBoxMarker * marker);
avifResult avifRWStreamWriteFullBox(avifRWStream * stream, const char * type, size_t contentSize, int version, uint32_t flags, avifBoxMarker * marker);
void avifRWStreamFinishBox(avifRWStream * stream, avifBoxMarker marker);
avifResult avifRWStreamWriteU8(avifRWStream * stream, uint8_t v);
avifResult avifRWStreamWriteU16(avifRWStream * stream, uint16_t v);
avifResult avifRWStreamWriteU32(avifRWStream * stream, uint32_t v);
avifResult avifRWStreamWriteU64(avifRWStream * stream, uint64_t v);
avifResult avifRWStreamWriteZeros(avifRWStream * stream, size_t byteCount);
// The following functions can write non-aligned bits.
avifResult avifRWStreamWriteBits(avifRWStream * stream, uint32_t v, size_t bitCount);
avifResult avifRWStreamWriteVarInt(avifRWStream * stream, uint32_t v);

// This is to make it clear that the box size is currently unknown, and will be determined later (with a call to avifRWStreamFinishBox)
#define AVIF_BOX_SIZE_TBD 0

// Used for both av1C and av2C.
typedef struct avifCodecConfigurationBox
{
    // [skipped; is constant] unsigned int (1)marker = 1;
    // [skipped; is constant] unsigned int (7)version = 1;

    uint8_t seqProfile;           // unsigned int (3) seq_profile;
    uint8_t seqLevelIdx0;         // unsigned int (5) seq_level_idx_0;
    uint8_t seqTier0;             // unsigned int (1) seq_tier_0;
    uint8_t highBitdepth;         // unsigned int (1) high_bitdepth;
    uint8_t twelveBit;            // unsigned int (1) twelve_bit;
    uint8_t monochrome;           // unsigned int (1) monochrome;
    uint8_t chromaSubsamplingX;   // unsigned int (1) chroma_subsampling_x;
    uint8_t chromaSubsamplingY;   // unsigned int (1) chroma_subsampling_y;
    uint8_t chromaSamplePosition; // unsigned int (2) chroma_sample_position;

    // unsigned int (3)reserved = 0;
    // unsigned int (1)initial_presentation_delay_present;
    // if (initial_presentation_delay_present) {
    //     unsigned int (4)initial_presentation_delay_minus_one;
    // } else {
    //     unsigned int (4)reserved = 0;
    // }
} avifCodecConfigurationBox;

typedef struct avifSequenceHeader
{
    uint8_t reduced_still_picture_header;
    uint32_t maxWidth;
    uint32_t maxHeight;
    uint32_t bitDepth;
    avifPixelFormat yuvFormat;
    avifChromaSamplePosition chromaSamplePosition;
    avifColorPrimaries colorPrimaries;
    avifTransferCharacteristics transferCharacteristics;
    avifMatrixCoefficients matrixCoefficients;
    avifRange range;
    avifCodecConfigurationBox av1C; // TODO(yguyon): Rename or add av2C
} avifSequenceHeader;

AVIF_NODISCARD avifBool avifSequenceHeaderParse(avifSequenceHeader * header, const avifROData * sample, avifCodecType codecType);

// ---------------------------------------------------------------------------
// gain maps

#if defined(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)

// Finds the approximate min/max values from the given gain map values, excluding outliers.
// Uses a histogram, with outliers defined as having at least one empty bucket between them
// and the rest of the distribution. Discards at most 0.1% of values.
// Removing outliers helps with accuracy/compression.
avifResult avifFindMinMaxWithoutOutliers(const float * gainMapF, int numPixels, float * rangeMin, float * rangeMax);

#endif // AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP

#define AVIF_INDEFINITE_DURATION64 UINT64_MAX
#define AVIF_INDEFINITE_DURATION32 UINT32_MAX

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ifndef AVIF_INTERNAL_H
