// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#ifndef AVIF_AVIF_H
#define AVIF_AVIF_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Export macros

// AVIF_BUILDING_SHARED_LIBS should only be defined when libavif is being built
// as a shared library.
// AVIF_DLL should be defined if libavif is a shared library. If you are using
// libavif as a CMake dependency, through a CMake package config file or through
// pkg-config, this is defined automatically.
//
// Here's what AVIF_API will be defined as in shared build:
// |       |        Windows        |                  Unix                  |
// | Build | __declspec(dllexport) | __attribute__((visibility("default"))) |
// |  Use  | __declspec(dllimport) |                                        |
//
// For static build, AVIF_API is always defined as nothing.

#if defined(_WIN32)
#define AVIF_HELPER_EXPORT __declspec(dllexport)
#define AVIF_HELPER_IMPORT __declspec(dllimport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#define AVIF_HELPER_EXPORT __attribute__((visibility("default")))
#define AVIF_HELPER_IMPORT
#else
#define AVIF_HELPER_EXPORT
#define AVIF_HELPER_IMPORT
#endif

#if defined(AVIF_DLL)
#if defined(AVIF_BUILDING_SHARED_LIBS)
#define AVIF_API AVIF_HELPER_EXPORT
#else
#define AVIF_API AVIF_HELPER_IMPORT
#endif // defined(AVIF_BUILDING_SHARED_LIBS)
#else
#define AVIF_API
#endif // defined(AVIF_DLL)

#if defined(AVIF_ENABLE_NODISCARD) || (defined(__cplusplus) && __cplusplus >= 201700L) || \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L)
#define AVIF_NODISCARD [[nodiscard]]
#else
// Starting with 3.9, clang allows defining the warn_unused_result attribute for enums.
#if defined(__clang__) && defined(__has_attribute) && ((__clang_major__ << 8) | __clang_minor__) >= ((3 << 8) | 9)
#if __has_attribute(warn_unused_result)
#define AVIF_NODISCARD __attribute__((warn_unused_result))
#else
#define AVIF_NODISCARD
#endif
#else
#define AVIF_NODISCARD
#endif
#endif

// ---------------------------------------------------------------------------
// Constants

// AVIF_VERSION_DEVEL should always be 0 for official releases / version tags,
// and non-zero during development of the next release. This should allow for
// downstream projects to do greater-than preprocessor checks on AVIF_VERSION
// to leverage in-development code without breaking their stable builds.
#define AVIF_VERSION_MAJOR 1
#define AVIF_VERSION_MINOR 0
#define AVIF_VERSION_PATCH 3
#define AVIF_VERSION_DEVEL 1
#define AVIF_VERSION \
    ((AVIF_VERSION_MAJOR * 1000000) + (AVIF_VERSION_MINOR * 10000) + (AVIF_VERSION_PATCH * 100) + AVIF_VERSION_DEVEL)

typedef int avifBool;
#define AVIF_TRUE 1
#define AVIF_FALSE 0

#define AVIF_DIAGNOSTICS_ERROR_BUFFER_SIZE 256

// A reasonable default for maximum image size (in pixel count) to avoid out-of-memory errors or
// integer overflow in (32-bit) int or unsigned int arithmetic operations.
#define AVIF_DEFAULT_IMAGE_SIZE_LIMIT (16384 * 16384)

// A reasonable default for maximum image dimension (width or height).
#define AVIF_DEFAULT_IMAGE_DIMENSION_LIMIT 32768

// a 12 hour AVIF image sequence, running at 60 fps (a basic sanity check as this is quite ridiculous)
#define AVIF_DEFAULT_IMAGE_COUNT_LIMIT (12 * 3600 * 60)

#define AVIF_QUALITY_DEFAULT -1
#define AVIF_QUALITY_LOSSLESS 100
#define AVIF_QUALITY_WORST 0
#define AVIF_QUALITY_BEST 100

#define AVIF_QUANTIZER_LOSSLESS 0
#define AVIF_QUANTIZER_BEST_QUALITY 0
#define AVIF_QUANTIZER_WORST_QUALITY 63

#define AVIF_PLANE_COUNT_YUV 3

#define AVIF_SPEED_DEFAULT -1
#define AVIF_SPEED_SLOWEST 0
#define AVIF_SPEED_FASTEST 10

// This value is used to indicate that an animated AVIF file has to be repeated infinitely.
#define AVIF_REPETITION_COUNT_INFINITE -1
// This value is used if an animated AVIF file does not have repetitions specified using an EditList box. Applications can choose
// to handle this case however they want.
#define AVIF_REPETITION_COUNT_UNKNOWN -2

// The number of spatial layers in AV1, with spatial_id = 0..3.
#define AVIF_MAX_AV1_LAYER_COUNT 4

typedef enum avifPlanesFlag
{
    AVIF_PLANES_YUV = (1 << 0),
    AVIF_PLANES_A = (1 << 1),

    AVIF_PLANES_ALL = 0xff
} avifPlanesFlag;
typedef uint32_t avifPlanesFlags;

typedef enum avifChannelIndex
{
    // These can be used as the index for the yuvPlanes and yuvRowBytes arrays in avifImage.
    AVIF_CHAN_Y = 0,
    AVIF_CHAN_U = 1,
    AVIF_CHAN_V = 2,

    // This may not be used in yuvPlanes and yuvRowBytes, but is available for use with avifImagePlane().
    AVIF_CHAN_A = 3
} avifChannelIndex;

// ---------------------------------------------------------------------------
// Version

AVIF_API const char * avifVersion(void);
AVIF_API void avifCodecVersions(char outBuffer[256]);
AVIF_API unsigned int avifLibYUVVersion(void); // returns 0 if libavif wasn't compiled with libyuv support

// ---------------------------------------------------------------------------
// Memory management

// Returns NULL on memory allocation failure.
AVIF_API void * avifAlloc(size_t size);
AVIF_API void avifFree(void * p);

// ---------------------------------------------------------------------------
// avifResult

typedef enum AVIF_NODISCARD avifResult
{
    AVIF_RESULT_OK = 0,
    AVIF_RESULT_UNKNOWN_ERROR = 1,
    AVIF_RESULT_INVALID_FTYP = 2,
    AVIF_RESULT_NO_CONTENT = 3,
    AVIF_RESULT_NO_YUV_FORMAT_SELECTED = 4,
    AVIF_RESULT_REFORMAT_FAILED = 5,
    AVIF_RESULT_UNSUPPORTED_DEPTH = 6,
    AVIF_RESULT_ENCODE_COLOR_FAILED = 7,
    AVIF_RESULT_ENCODE_ALPHA_FAILED = 8,
    AVIF_RESULT_BMFF_PARSE_FAILED = 9,
    AVIF_RESULT_MISSING_IMAGE_ITEM = 10,
    AVIF_RESULT_DECODE_COLOR_FAILED = 11,
    AVIF_RESULT_DECODE_ALPHA_FAILED = 12,
    AVIF_RESULT_COLOR_ALPHA_SIZE_MISMATCH = 13,
    AVIF_RESULT_ISPE_SIZE_MISMATCH = 14,
    AVIF_RESULT_NO_CODEC_AVAILABLE = 15,
    AVIF_RESULT_NO_IMAGES_REMAINING = 16,
    AVIF_RESULT_INVALID_EXIF_PAYLOAD = 17,
    AVIF_RESULT_INVALID_IMAGE_GRID = 18,
    AVIF_RESULT_INVALID_CODEC_SPECIFIC_OPTION = 19,
    AVIF_RESULT_TRUNCATED_DATA = 20,
    AVIF_RESULT_IO_NOT_SET = 21, // the avifIO field of avifDecoder is not set
    AVIF_RESULT_IO_ERROR = 22,
    AVIF_RESULT_WAITING_ON_IO = 23, // similar to EAGAIN/EWOULDBLOCK, this means the avifIO doesn't have necessary data available yet
    AVIF_RESULT_INVALID_ARGUMENT = 24, // an argument passed into this function is invalid
    AVIF_RESULT_NOT_IMPLEMENTED = 25,  // a requested code path is not (yet) implemented
    AVIF_RESULT_OUT_OF_MEMORY = 26,
    AVIF_RESULT_CANNOT_CHANGE_SETTING = 27, // a setting that can't change is changed during encoding
    AVIF_RESULT_INCOMPATIBLE_IMAGE = 28,    // the image is incompatible with already encoded images
    AVIF_RESULT_INTERNAL_ERROR = 29,        // some invariants have not been satisfied (likely a bug in libavif)
#if defined(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
    AVIF_RESULT_ENCODE_GAIN_MAP_FAILED = 30,
    AVIF_RESULT_DECODE_GAIN_MAP_FAILED = 31,
    AVIF_RESULT_INVALID_TONE_MAPPED_IMAGE = 32,
#endif

    // Kept for backward compatibility; please use the symbols above instead.
    AVIF_RESULT_NO_AV1_ITEMS_FOUND = AVIF_RESULT_MISSING_IMAGE_ITEM
} avifResult;

AVIF_API const char * avifResultToString(avifResult result);

// ---------------------------------------------------------------------------
// avifHeaderFormat

typedef enum avifHeaderFormat
{
    // AVIF file with an "avif" brand, a MetaBox and all its required boxes for maximum compatibility.
    AVIF_HEADER_FULL,
#if defined(AVIF_ENABLE_EXPERIMENTAL_AVIR)
    // AVIF file with an "avir" brand and a CondensedImageBox to reduce the encoded file size.
    // This is based on the m64572 "Condensed image item" MPEG proposal for HEIF.
    // WARNING: Experimental feature. Produces files that are incompatible with older decoders.
    AVIF_HEADER_REDUCED,
#endif
} avifHeaderFormat;

// ---------------------------------------------------------------------------
// avifROData/avifRWData: Generic raw memory storage

typedef struct avifROData
{
    const uint8_t * data;
    size_t size;
} avifROData;

// Note: Use avifRWDataFree() if any avif*() function populates one of these.

typedef struct avifRWData
{
    uint8_t * data;
    size_t size;
} avifRWData;

// clang-format off
// Initialize avifROData/avifRWData on the stack with this
#define AVIF_DATA_EMPTY { NULL, 0 }
// clang-format on

// The avifRWData input must be zero-initialized before being manipulated with these functions.
// If AVIF_RESULT_OUT_OF_MEMORY is returned, raw is left unchanged.
AVIF_API avifResult avifRWDataRealloc(avifRWData * raw, size_t newSize);
AVIF_API avifResult avifRWDataSet(avifRWData * raw, const uint8_t * data, size_t len);
AVIF_API void avifRWDataFree(avifRWData * raw);

// ---------------------------------------------------------------------------
// Metadata

// Validates the first bytes of the Exif payload and finds the TIFF header offset (up to UINT32_MAX).
AVIF_API avifResult avifGetExifTiffHeaderOffset(const uint8_t * exif, size_t exifSize, size_t * offset);
// Returns the offset to the Exif 8-bit orientation value and AVIF_RESULT_OK, or an error.
// If the offset is set to exifSize, there was no parsing error but no orientation tag was found.
AVIF_API avifResult avifGetExifOrientationOffset(const uint8_t * exif, size_t exifSize, size_t * offset);

// ---------------------------------------------------------------------------
// avifPixelFormat
//
// Note to libavif maintainers: The lookup tables in avifImageYUVToRGBLibYUV
// rely on the ordering of this enum values for their correctness. So changing
// the values in this enum will require auditing avifImageYUVToRGBLibYUV for
// correctness.
typedef enum avifPixelFormat
{
    // No YUV pixels are present. Alpha plane can still be present.
    AVIF_PIXEL_FORMAT_NONE = 0,

    AVIF_PIXEL_FORMAT_YUV444,
    AVIF_PIXEL_FORMAT_YUV422,
    AVIF_PIXEL_FORMAT_YUV420,
    AVIF_PIXEL_FORMAT_YUV400,
    AVIF_PIXEL_FORMAT_COUNT
} avifPixelFormat;
AVIF_API const char * avifPixelFormatToString(avifPixelFormat format);

typedef struct avifPixelFormatInfo
{
    avifBool monochrome;
    int chromaShiftX;
    int chromaShiftY;
} avifPixelFormatInfo;

// Returns the avifPixelFormatInfo depending on the avifPixelFormat.
// When monochrome is AVIF_TRUE, chromaShiftX and chromaShiftY are set to 1 according to the AV1 specification but they should be ignored.
//
// Note: This function implements the second table on page 119 of the AV1 specification version 1.0.0 with Errata 1.
// For monochrome 4:0:0, subsampling_x and subsampling are specified as 1 to allow
// an AV1 implementation that only supports profile 0 to hardcode subsampling_x and subsampling_y to 1.
AVIF_API void avifGetPixelFormatInfo(avifPixelFormat format, avifPixelFormatInfo * info);

// ---------------------------------------------------------------------------
// avifChromaSamplePosition

typedef enum avifChromaSamplePosition
{
    AVIF_CHROMA_SAMPLE_POSITION_UNKNOWN = 0,
    AVIF_CHROMA_SAMPLE_POSITION_VERTICAL = 1,
    AVIF_CHROMA_SAMPLE_POSITION_COLOCATED = 2
} avifChromaSamplePosition;

// ---------------------------------------------------------------------------
// avifRange

typedef enum avifRange
{
    AVIF_RANGE_LIMITED = 0,
    AVIF_RANGE_FULL = 1
} avifRange;

// ---------------------------------------------------------------------------
// CICP enums - https://www.itu.int/rec/T-REC-H.273-201612-S/en

enum
{
    // This is actually reserved, but libavif uses it as a sentinel value.
    AVIF_COLOR_PRIMARIES_UNKNOWN = 0,

    AVIF_COLOR_PRIMARIES_BT709 = 1,
    AVIF_COLOR_PRIMARIES_SRGB = 1,
    AVIF_COLOR_PRIMARIES_IEC61966_2_4 = 1,
    AVIF_COLOR_PRIMARIES_UNSPECIFIED = 2,
    AVIF_COLOR_PRIMARIES_BT470M = 4,
    AVIF_COLOR_PRIMARIES_BT470BG = 5,
    AVIF_COLOR_PRIMARIES_BT601 = 6,
    AVIF_COLOR_PRIMARIES_SMPTE240 = 7,
    AVIF_COLOR_PRIMARIES_GENERIC_FILM = 8,
    AVIF_COLOR_PRIMARIES_BT2020 = 9,
    AVIF_COLOR_PRIMARIES_BT2100 = 9,
    AVIF_COLOR_PRIMARIES_XYZ = 10,
    AVIF_COLOR_PRIMARIES_SMPTE431 = 11,
    AVIF_COLOR_PRIMARIES_SMPTE432 = 12,
    AVIF_COLOR_PRIMARIES_DCI_P3 = 12,
    AVIF_COLOR_PRIMARIES_EBU3213 = 22
};
typedef uint16_t avifColorPrimaries; // AVIF_COLOR_PRIMARIES_*

// outPrimaries: rX, rY, gX, gY, bX, bY, wX, wY
AVIF_API void avifColorPrimariesGetValues(avifColorPrimaries acp, float outPrimaries[8]);
AVIF_API avifColorPrimaries avifColorPrimariesFind(const float inPrimaries[8], const char ** outName);

enum
{
    // This is actually reserved, but libavif uses it as a sentinel value.
    AVIF_TRANSFER_CHARACTERISTICS_UNKNOWN = 0,

    AVIF_TRANSFER_CHARACTERISTICS_BT709 = 1,
    AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED = 2,
    AVIF_TRANSFER_CHARACTERISTICS_BT470M = 4,  // 2.2 gamma
    AVIF_TRANSFER_CHARACTERISTICS_BT470BG = 5, // 2.8 gamma
    AVIF_TRANSFER_CHARACTERISTICS_BT601 = 6,
    AVIF_TRANSFER_CHARACTERISTICS_SMPTE240 = 7,
    AVIF_TRANSFER_CHARACTERISTICS_LINEAR = 8,
    AVIF_TRANSFER_CHARACTERISTICS_LOG100 = 9,
    AVIF_TRANSFER_CHARACTERISTICS_LOG100_SQRT10 = 10,
    AVIF_TRANSFER_CHARACTERISTICS_IEC61966 = 11,
    AVIF_TRANSFER_CHARACTERISTICS_BT1361 = 12,
    AVIF_TRANSFER_CHARACTERISTICS_SRGB = 13,
    AVIF_TRANSFER_CHARACTERISTICS_BT2020_10BIT = 14,
    AVIF_TRANSFER_CHARACTERISTICS_BT2020_12BIT = 15,
    AVIF_TRANSFER_CHARACTERISTICS_PQ = 16, // Perceptual Quantizer (HDR); BT.2100 PQ
    AVIF_TRANSFER_CHARACTERISTICS_SMPTE2084 = 16,
    AVIF_TRANSFER_CHARACTERISTICS_SMPTE428 = 17,
    AVIF_TRANSFER_CHARACTERISTICS_HLG = 18 // Hybrid Log-Gamma (HDR); ARIB STD-B67; BT.2100 HLG
};
typedef uint16_t avifTransferCharacteristics; // AVIF_TRANSFER_CHARACTERISTICS_*

// If the given transfer characteristics can be expressed with a simple gamma value, sets 'gamma'
// to that value and returns AVIF_RESULT_OK. Returns an error otherwise.
AVIF_API avifResult avifTransferCharacteristicsGetGamma(avifTransferCharacteristics atc, float * gamma);
AVIF_API avifTransferCharacteristics avifTransferCharacteristicsFindByGamma(float gamma);

enum
{
    AVIF_MATRIX_COEFFICIENTS_IDENTITY = 0,
    AVIF_MATRIX_COEFFICIENTS_BT709 = 1,
    AVIF_MATRIX_COEFFICIENTS_UNSPECIFIED = 2,
    AVIF_MATRIX_COEFFICIENTS_FCC = 4,
    AVIF_MATRIX_COEFFICIENTS_BT470BG = 5,
    AVIF_MATRIX_COEFFICIENTS_BT601 = 6,
    AVIF_MATRIX_COEFFICIENTS_SMPTE240 = 7,
    AVIF_MATRIX_COEFFICIENTS_YCGCO = 8,
    AVIF_MATRIX_COEFFICIENTS_BT2020_NCL = 9,
    AVIF_MATRIX_COEFFICIENTS_BT2020_CL = 10,
    AVIF_MATRIX_COEFFICIENTS_SMPTE2085 = 11,
    AVIF_MATRIX_COEFFICIENTS_CHROMA_DERIVED_NCL = 12,
    AVIF_MATRIX_COEFFICIENTS_CHROMA_DERIVED_CL = 13,
    AVIF_MATRIX_COEFFICIENTS_ICTCP = 14,
#if defined(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
    AVIF_MATRIX_COEFFICIENTS_YCGCO_RE = 15,
    AVIF_MATRIX_COEFFICIENTS_YCGCO_RO = 16,
#endif
    AVIF_MATRIX_COEFFICIENTS_LAST
};
typedef uint16_t avifMatrixCoefficients; // AVIF_MATRIX_COEFFICIENTS_*

// ---------------------------------------------------------------------------
// avifDiagnostics

typedef struct avifDiagnostics
{
    // Upon receiving an error from any non-const libavif API call, if the toplevel structure used
    // in the API call (avifDecoder, avifEncoder) contains a diag member, this buffer may be
    // populated with a NULL-terminated, freeform error string explaining the first encountered error in
    // more detail. It will be cleared at the beginning of every non-const API call.
    //
    // Note: If an error string contains the "[Strict]" prefix, it means that you encountered an
    // error that only occurs during strict decoding. If you disable strict mode, you will no
    // longer encounter this error.
    char error[AVIF_DIAGNOSTICS_ERROR_BUFFER_SIZE];
} avifDiagnostics;

AVIF_API void avifDiagnosticsClearError(avifDiagnostics * diag);

// ---------------------------------------------------------------------------
// Fraction utility

typedef struct avifFraction
{
    int32_t n;
    int32_t d;
} avifFraction;

// ---------------------------------------------------------------------------
// Optional transformation structs

typedef enum avifTransformFlag
{
    AVIF_TRANSFORM_NONE = 0,

    AVIF_TRANSFORM_PASP = (1 << 0),
    AVIF_TRANSFORM_CLAP = (1 << 1),
    AVIF_TRANSFORM_IROT = (1 << 2),
    AVIF_TRANSFORM_IMIR = (1 << 3)
} avifTransformFlag;
typedef uint32_t avifTransformFlags;

typedef struct avifPixelAspectRatioBox
{
    // 'pasp' from ISO/IEC 14496-12:2015 12.1.4.3

    // define the relative width and height of a pixel
    uint32_t hSpacing;
    uint32_t vSpacing;
} avifPixelAspectRatioBox;

typedef struct avifCleanApertureBox
{
    // 'clap' from ISO/IEC 14496-12:2015 12.1.4.3

    // a fractional number which defines the exact clean aperture width, in counted pixels, of the video image
    uint32_t widthN;
    uint32_t widthD;

    // a fractional number which defines the exact clean aperture height, in counted pixels, of the video image
    uint32_t heightN;
    uint32_t heightD;

    // a fractional number which defines the horizontal offset of clean aperture centre minus (width-1)/2. Typically 0.
    uint32_t horizOffN;
    uint32_t horizOffD;

    // a fractional number which defines the vertical offset of clean aperture centre minus (height-1)/2. Typically 0.
    uint32_t vertOffN;
    uint32_t vertOffD;
} avifCleanApertureBox;

typedef struct avifImageRotation
{
    // 'irot' from ISO/IEC 23008-12:2017 6.5.10

    // angle * 90 specifies the angle (in anti-clockwise direction) in units of degrees.
    uint8_t angle; // legal values: [0-3]
} avifImageRotation;

typedef struct avifImageMirror
{
    // 'imir' from ISO/IEC 23008-12:2022 6.5.12:
    //
    //     'axis' specifies how the mirroring is performed:
    //
    //     0 indicates that the top and bottom parts of the image are exchanged;
    //     1 specifies that the left and right parts are exchanged.
    //
    //     NOTE In Exif, orientation tag can be used to signal mirroring operations. Exif
    //     orientation tag 4 corresponds to axis = 0 of ImageMirror, and Exif orientation tag 2
    //     corresponds to axis = 1 accordingly.
    //
    // Legal values: [0, 1]
    uint8_t axis;
} avifImageMirror;

// ---------------------------------------------------------------------------
// avifCropRect - Helper struct/functions to work with avifCleanApertureBox

typedef struct avifCropRect
{
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} avifCropRect;

// These will return AVIF_FALSE if the resultant values violate any standards, and if so, the output
// values are not guaranteed to be complete or correct and should not be used.
AVIF_NODISCARD AVIF_API avifBool avifCropRectConvertCleanApertureBox(avifCropRect * cropRect,
                                                                     const avifCleanApertureBox * clap,
                                                                     uint32_t imageW,
                                                                     uint32_t imageH,
                                                                     avifPixelFormat yuvFormat,
                                                                     avifDiagnostics * diag);
AVIF_NODISCARD AVIF_API avifBool avifCleanApertureBoxConvertCropRect(avifCleanApertureBox * clap,
                                                                     const avifCropRect * cropRect,
                                                                     uint32_t imageW,
                                                                     uint32_t imageH,
                                                                     avifPixelFormat yuvFormat,
                                                                     avifDiagnostics * diag);

// ---------------------------------------------------------------------------
// avifContentLightLevelInformationBox

typedef struct avifContentLightLevelInformationBox
{
    // 'clli' from ISO/IEC 23000-22:2019 (MIAF) 7.4.4.2.2. The SEI message semantics written above
    // each entry were originally described in ISO/IEC 23008-2:2020 (HEVC) section D.3.35,
    // available at https://standards.iso.org/ittf/PubliclyAvailableStandards/

    // Given the red, green, and blue colour primary intensities in the linear light domain for the
    // location of a luma sample in a corresponding 4:4:4 representation, denoted as E_R, E_G, and E_B,
    // the maximum component intensity is defined as E_Max = Max(E_R, Max(E_G, E_B)).
    // The light level corresponding to the stimulus is then defined as the CIE 1931 luminance
    // corresponding to equal amplitudes of E_Max for all three colour primary intensities for red,
    // green, and blue (with appropriate scaling to reflect the nominal luminance level associated
    // with peak white, e.g. ordinarily scaling to associate peak white with 10 000 candelas per
    // square metre when transfer_characteristics is equal to 16).

    // max_content_light_level, when not equal to 0, indicates an upper bound on the maximum light
    // level among all individual samples in a 4:4:4 representation of red, green, and blue colour
    // primary intensities (in the linear light domain) for the pictures of the CLVS, in units of
    // candelas per square metre. When equal to 0, no such upper bound is indicated by
    // max_content_light_level.
    uint16_t maxCLL;

    // max_pic_average_light_level, when not equal to 0, indicates an upper bound on the maximum
    // average light level among the samples in a 4:4:4 representation of red, green, and blue
    // colour primary intensities (in the linear light domain) for any individual picture of the
    // CLVS, in units of candelas per square metre. When equal to 0, no such upper bound is
    // indicated by max_pic_average_light_level.
    uint16_t maxPALL;
} avifContentLightLevelInformationBox;

#if defined(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
// ---------------------------------------------------------------------------
// avifGainMap
// Gain Maps are a solution for a consistent and adaptive display of HDR images.
// Gain Maps are a HIGHLY EXPERIMENTAL FEATURE. The format might still change and
// images containing a gain map encoded with the current version of libavif might
// not decode with a future version of libavif. The API is not guaranteed
// to be stable, and might even be removed in the future. Use are your own risk.
// This is based on ISO/IEC JTC 1/SC 29/WG 3 m64379
// This product includes Gain Map technology under license by Adobe.
//
// Terms:
// base image: main image stored in the file, shown by viewers that do not support
//     gain maps
// alternate image: image  obtained by combining the base image and the gain map
// gain map: data structure that contains pixels and metadata used for conversion
//     between the base image and the alternate image

struct avifImage;

// Gain map metadata, to apply the gain map. Fully applying the gain map to the base
// image results in the alternate image.
// All field pairs ending with 'N' and 'D' are fractional values (numerator and denominator).
typedef struct avifGainMapMetadata
{
    // Parameters for converting the gain map from its image encoding to log2 space.
    // gainMapLog2 = lerp(gainMapMin, gainMapMax, pow(gainMapEncoded, gainMapGamma));
    // where 'lerp' is a linear interpolation function.

    // Minimum value in the gain map, log2-encoded, per RGB channel.
    int32_t gainMapMinN[3];
    uint32_t gainMapMinD[3];
    // Maximum value in the gain map, log2-encoded, per RGB channel.
    int32_t gainMapMaxN[3];
    uint32_t gainMapMaxD[3];
    // Gain map gamma value with which the gain map was encoded, per RGB channel.
    // For decoding, the inverse value (1/gamma) should be used.
    uint32_t gainMapGammaN[3];
    uint32_t gainMapGammaD[3];

    // Parameters used in gain map computation/tone mapping to avoid numerical
    // instability.
    // toneMappedLinear = ((baseImageLinear + baseOffset) * exp(gainMapLog * w)) - alternateOffset;
    // Where 'w' is a weight parameter based on the display's HDR capacity
    // (see below).

    // Offset constants for the base image, per RGB channel.
    int32_t baseOffsetN[3];
    uint32_t baseOffsetD[3];
    // Offset constants for the alternate image, per RGB channel.
    int32_t alternateOffsetN[3];
    uint32_t alternateOffsetD[3];

    // -----------------------------------------------------------------------

    // Parameters below can be manually tuned after the gain map has been
    // created.

    // Log2-encoded HDR headroom of the base and alternate images respectively.
    // If baseHdrHeadroom is < alternateHdrHeadroom, the result of tone mapping
    // for a display with an HDR headroom that is <= baseHdrHeadroom is the base
    // image, and the result of tone mapping for a display with an HDR headroom >=
    // alternateHdrHeadroom is the alternate image.
    // Conversely, if baseHdrHeadroom is > alternateHdrHeadroom, the result of
    // tone mapping for a display with an HDR headroom that is >= baseHdrHeadroom
    // is the base image, and the result of tone mapping for a display with an HDR
    // headroom <= alternateHdrHeadroom is the alternate image.
    // For a display with a capacity between baseHdrHeadroom and alternateHdrHeadroom,
    // tone mapping results in an interpolation between the base and alternate
    // versions. baseHdrHeadroom and alternateHdrHeadroom can be tuned to change how
    // the gain map should be applied.
    //
    // If 'H' is the display's current log2-encoded HDR capacity (HDR to SDR ratio),
    // then the weight 'w' to apply the gain map is computed as follows:
    // f = clamp((H - hdrCapacityMin) /
    //           (hdrCapacityMax - hdrCapacityMin), 0, 1);
    // w = backwardDirection ? f * -1 : f;
    uint32_t baseHdrHeadroomN;
    uint32_t baseHdrHeadroomD;
    uint32_t alternateHdrHeadroomN;
    uint32_t alternateHdrHeadroomD;

    // True if the gain map should be applied in reverse, see weight formula above.
    avifBool backwardDirection;

    // True if tone mapping should be performed in the color space of the
    // base image. If false, the color space of the alternate image should
    // be used.
    avifBool useBaseColorSpace;
} avifGainMapMetadata;

// Gain map image and associated metadata.
// Must be allocated by calling avifGainMapCreate().
typedef struct avifGainMap
{
    // Gain map pixels.
    // Owned by the avifGainMap and gets freed when calling avifGainMapDestroy().
    // Used fields: width, height, depth, yuvFormat, yuvRange,
    // yuvChromaSamplePosition, yuvPlanes, yuvRowBytes, imageOwnsYUVPlanes,
    // matrixCoefficients. The colorPrimaries and transferCharacteristics fields
    // shall be 2. Other fields are ignored.
    struct avifImage * image;

    // When encoding an image grid, all metadata below shall be identical for all
    // cells.

    // Gain map metadata used to interpret and apply the gain map pixel data.
    avifGainMapMetadata metadata;

    // Colorimetry of the alternate image (ICC profile and/or CICP information
    // of the alternate image that the gain map was created from).
    avifRWData altICC;
    avifColorPrimaries altColorPrimaries;
    avifTransferCharacteristics altTransferCharacteristics;
    avifMatrixCoefficients altMatrixCoefficients;
    avifRange altYUVRange;

    // Hint on the approximate amount of colour resolution available after fully
    // applying the gain map ('pixi' box content of the alternate image that the
    // gain map was created from).
    uint32_t altDepth;
    uint32_t altPlaneCount;

    // Optimal viewing conditions of the alternate image ('clli' box content
    // of the alternate image that the gain map was created from).
    avifContentLightLevelInformationBox altCLLI;
} avifGainMap;

// Allocates a gain map. Returns NULL if a memory allocation failed.
// The 'image' field is NULL by default and must be allocated separately.
AVIF_API avifGainMap * avifGainMapCreate();
// Frees a gain map, including the 'image' field if non NULL.
AVIF_API void avifGainMapDestroy(avifGainMap * gainMap);

// Same as avifGainMapMetadata, but with fields of type double instead of uint32_t fractions.
// Use avifGainMapMetadataDoubleToFractions() to convert this to a avifGainMapMetadata.
// See avifGainMapMetadata for detailed descriptions of fields.
typedef struct avifGainMapMetadataDouble
{
    double gainMapMin[3];
    double gainMapMax[3];
    double gainMapGamma[3];
    double baseOffset[3];
    double alternateOffset[3];
    double baseHdrHeadroom;
    double alternateHdrHeadroom;
    avifBool backwardDirection;
    avifBool useBaseColorSpace;
} avifGainMapMetadataDouble;

// Converts a avifGainMapMetadataDouble to avifGainMapMetadata by converting double values
// to the closest uint32_t fractions.
// Returns AVIF_FALSE if some field values are < 0 or > UINT32_MAX.
AVIF_NODISCARD AVIF_API avifBool avifGainMapMetadataDoubleToFractions(avifGainMapMetadata * dst, const avifGainMapMetadataDouble * src);
// Converts a avifGainMapMetadata to avifGainMapMetadataDouble by converting fractions to double values.
// Returns AVIF_FALSE if some denominators are zero.
AVIF_NODISCARD AVIF_API avifBool avifGainMapMetadataFractionsToDouble(avifGainMapMetadataDouble * dst, const avifGainMapMetadata * src);

#endif // AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP

// ---------------------------------------------------------------------------
// avifImage

// NOTE: The avifImage struct may be extended in a future release. Code outside the libavif library
// must allocate avifImage by calling the avifImageCreate() or avifImageCreateEmpty() function.
typedef struct avifImage
{
    // Image information
    uint32_t width;
    uint32_t height;
    uint32_t depth; // all planes must share this depth; if depth>8, all planes are uint16_t internally

    avifPixelFormat yuvFormat;
    avifRange yuvRange;
    avifChromaSamplePosition yuvChromaSamplePosition;
    uint8_t * yuvPlanes[AVIF_PLANE_COUNT_YUV];
    uint32_t yuvRowBytes[AVIF_PLANE_COUNT_YUV];
    avifBool imageOwnsYUVPlanes;

    uint8_t * alphaPlane;
    uint32_t alphaRowBytes;
    avifBool imageOwnsAlphaPlane;
    avifBool alphaPremultiplied;

    // ICC Profile
    avifRWData icc;

    // CICP information:
    // These are stored in the AV1 payload and used to signal YUV conversion. Additionally, if an
    // ICC profile is not specified, these will be stored in the AVIF container's `colr` box with
    // a type of `nclx`. If your system supports ICC profiles, be sure to check for the existence
    // of one (avifImage.icc) before relying on the values listed here!
    avifColorPrimaries colorPrimaries;
    avifTransferCharacteristics transferCharacteristics;
    avifMatrixCoefficients matrixCoefficients;

    // CLLI information:
    // Content Light Level Information. Used to represent maximum and average light level of an
    // image. Useful for tone mapping HDR images, especially when using transfer characteristics
    // SMPTE2084 (PQ). The default value of (0, 0) means the content light level information is
    // unknown or unavailable, and will cause libavif to avoid writing a clli box for it.
    avifContentLightLevelInformationBox clli;

    // Transformations - These metadata values are encoded/decoded when transformFlags are set
    // appropriately, but do not impact/adjust the actual pixel buffers used (images won't be
    // pre-cropped or mirrored upon decode). Basic explanations from the standards are offered in
    // comments above, but for detailed explanations, please refer to the HEIF standard (ISO/IEC
    // 23008-12:2017) and the BMFF standard (ISO/IEC 14496-12:2015).
    //
    // To encode any of these boxes, set the values in the associated box, then enable the flag in
    // transformFlags. On decode, only honor the values in boxes with the associated transform flag set.
    avifTransformFlags transformFlags;
    avifPixelAspectRatioBox pasp;
    avifCleanApertureBox clap;
    avifImageRotation irot;
    avifImageMirror imir;

    // Metadata - set with avifImageSetMetadata*() before write, check .size>0 for existence after read
    avifRWData exif; // exif_payload chunk from the ExifDataBlock specified in ISO/IEC 23008-12:2022 Section A.2.1.
                     // The value of the 4-byte exif_tiff_header_offset field, which is not part of this avifRWData
                     // byte sequence, can be retrieved by calling avifGetExifTiffHeaderOffset(avifImage.exif).
    avifRWData xmp;

    // Version 1.0.0 ends here. Add any new members after this line.

#if defined(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
    // Gain map image and metadata. NULL if no gain map is present.
    // Owned by the avifImage and gets freed when calling avifImageDestroy().
    avifGainMap * gainMap;
#endif
} avifImage;

// avifImageCreate() and avifImageCreateEmpty() return NULL if arguments are invalid or if a memory allocation failed.
AVIF_NODISCARD AVIF_API avifImage * avifImageCreate(uint32_t width, uint32_t height, uint32_t depth, avifPixelFormat yuvFormat);
AVIF_NODISCARD AVIF_API avifImage * avifImageCreateEmpty(void); // helper for making an image to decode into
// Performs a deep copy of an image, including all metadata and planes, and the gain map metadata/planes if present
// and if AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP is defined.
AVIF_API avifResult avifImageCopy(avifImage * dstImage, const avifImage * srcImage, avifPlanesFlags planes);
// Performs a shallow copy of a rectangular area of an image. 'dstImage' does not own the planes.
// Ignores the gainMap field (which exists only if AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP is defined).
AVIF_API avifResult avifImageSetViewRect(avifImage * dstImage, const avifImage * srcImage, const avifCropRect * rect);
AVIF_API void avifImageDestroy(avifImage * image);

AVIF_API avifResult avifImageSetProfileICC(avifImage * image, const uint8_t * icc, size_t iccSize);
// Sets Exif metadata. Attempts to parse the Exif metadata for Exif orientation. Sets
// image->transformFlags, image->irot and image->imir if the Exif metadata is parsed successfully,
// otherwise leaves image->transformFlags, image->irot and image->imir unchanged.
// Warning: If the Exif payload is set and invalid, avifEncoderWrite() may return AVIF_RESULT_INVALID_EXIF_PAYLOAD.
AVIF_API avifResult avifImageSetMetadataExif(avifImage * image, const uint8_t * exif, size_t exifSize);
// Sets XMP metadata.
AVIF_API avifResult avifImageSetMetadataXMP(avifImage * image, const uint8_t * xmp, size_t xmpSize);

// Allocate/free/steal planes. These functions ignore the gainMap field (which exists only if
// AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP is defined).
AVIF_API avifResult avifImageAllocatePlanes(avifImage * image, avifPlanesFlags planes); // Ignores any pre-existing planes
AVIF_API void avifImageFreePlanes(avifImage * image, avifPlanesFlags planes);           // Ignores already-freed planes
AVIF_API void avifImageStealPlanes(avifImage * dstImage, avifImage * srcImage, avifPlanesFlags planes);

// ---------------------------------------------------------------------------
// Understanding maxThreads
//
// libavif's structures and API use the setting 'maxThreads' in a few places. The intent of this
// setting is to limit concurrent thread activity/usage, not necessarily to put a hard ceiling on
// how many sleeping threads happen to exist behind the scenes. The goal of this setting is to
// ensure that at any given point during libavif's encoding or decoding, no more than *maxThreads*
// threads are simultaneously **active and taking CPU time**.
//
// As an important example, when encoding an image sequence that has an alpha channel, two
// long-lived underlying AV1 encoders must simultaneously exist (one for color, one for alpha). For
// each additional frame fed into libavif, its YUV planes are fed into one instance of the AV1
// encoder, and its alpha plane is fed into another. These operations happen serially, so only one
// of these AV1 encoders is ever active at a time. However, the AV1 encoders might pre-create a
// pool of worker threads upon initialization, so during this process, twice the amount of worker
// threads actually simultaneously exist on the machine, but half of them are guaranteed to be
// sleeping.
//
// This design ensures that AV1 implementations are given as many threads as possible to ensure a
// speedy encode or decode, despite the complexities of occasionally needing two AV1 codec instances
// (due to alpha payloads being separate from color payloads). If your system has a hard ceiling on
// the number of threads that can ever be in flight at a given time, please account for this
// accordingly.

// ---------------------------------------------------------------------------
// Scaling

// Scales the YUV/A planes in-place. dstWidth and dstHeight must both be <= AVIF_DEFAULT_IMAGE_DIMENSION_LIMIT and
// dstWidth*dstHeight should be <= AVIF_DEFAULT_IMAGE_SIZE_LIMIT.
AVIF_API avifResult avifImageScale(avifImage * image, uint32_t dstWidth, uint32_t dstHeight, avifDiagnostics * diag);

// ---------------------------------------------------------------------------
// Optional YUV<->RGB support

// To convert to/from RGB, create an avifRGBImage on the stack, call avifRGBImageSetDefaults() on
// it, and then tweak the values inside of it accordingly. At a minimum, you should populate
// ->pixels and ->rowBytes with an appropriately sized pixel buffer, which should be at least
// (->rowBytes * ->height) bytes, where ->rowBytes is at least (->width * avifRGBImagePixelSize()).
// If you don't want to supply your own pixel buffer, you can use the
// avifRGBImageAllocatePixels()/avifRGBImageFreePixels() convenience functions.

// avifImageRGBToYUV() and avifImageYUVToRGB() will perform depth rescaling and limited<->full range
// conversion, if necessary. Pixels in an avifRGBImage buffer are always full range, and conversion
// routines will fail if the width and height don't match the associated avifImage.

// If libavif is built with a version of libyuv offering a fast conversion between RGB and YUV for
// the given inputs, libavif will use it. See reformat_libyuv.c for the details.
// libyuv is faster but may have slightly less precision than built-in conversion, so avoidLibYUV
// can be set to AVIF_TRUE when AVIF_CHROMA_UPSAMPLING_BEST_QUALITY or
// AVIF_CHROMA_DOWNSAMPLING_BEST_QUALITY is used, to get the most precise but slowest results.

// Note to libavif maintainers: The lookup tables in avifImageYUVToRGBLibYUV
// rely on the ordering of this enum values for their correctness. So changing
// the values in this enum will require auditing avifImageYUVToRGBLibYUV for
// correctness.
typedef enum avifRGBFormat
{
    AVIF_RGB_FORMAT_RGB = 0,
    AVIF_RGB_FORMAT_RGBA, // This is the default format set in avifRGBImageSetDefaults().
    AVIF_RGB_FORMAT_ARGB,
    AVIF_RGB_FORMAT_BGR,
    AVIF_RGB_FORMAT_BGRA,
    AVIF_RGB_FORMAT_ABGR,
    // RGB_565 format uses five bits for the red and blue components and six
    // bits for the green component. Each RGB pixel is 16 bits (2 bytes), which
    // is packed as follows:
    //   uint16_t: [r4 r3 r2 r1 r0 g5 g4 g3 g2 g1 g0 b4 b3 b2 b1 b0]
    //   r4 and r0 are the MSB and LSB of the red component respectively.
    //   g5 and g0 are the MSB and LSB of the green component respectively.
    //   b4 and b0 are the MSB and LSB of the blue component respectively.
    // This format is only supported for YUV -> RGB conversion and when
    // avifRGBImage.depth is set to 8.
    AVIF_RGB_FORMAT_RGB_565,
    AVIF_RGB_FORMAT_COUNT
} avifRGBFormat;
AVIF_API uint32_t avifRGBFormatChannelCount(avifRGBFormat format);
AVIF_API avifBool avifRGBFormatHasAlpha(avifRGBFormat format);

typedef enum avifChromaUpsampling
{
    AVIF_CHROMA_UPSAMPLING_AUTOMATIC = 0,    // Chooses best trade off of speed/quality (uses BILINEAR libyuv if available,
                                             // or falls back to NEAREST libyuv if available, or falls back to BILINEAR built-in)
    AVIF_CHROMA_UPSAMPLING_FASTEST = 1,      // Chooses speed over quality (same as NEAREST)
    AVIF_CHROMA_UPSAMPLING_BEST_QUALITY = 2, // Chooses the best quality upsampling, given settings (same as BILINEAR)
    AVIF_CHROMA_UPSAMPLING_NEAREST = 3,      // Uses nearest-neighbor filter
    AVIF_CHROMA_UPSAMPLING_BILINEAR = 4      // Uses bilinear filter
} avifChromaUpsampling;

typedef enum avifChromaDownsampling
{
    AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC = 0,    // Chooses best trade off of speed/quality (same as AVERAGE)
    AVIF_CHROMA_DOWNSAMPLING_FASTEST = 1,      // Chooses speed over quality (same as AVERAGE)
    AVIF_CHROMA_DOWNSAMPLING_BEST_QUALITY = 2, // Chooses the best quality upsampling (same as AVERAGE)
    AVIF_CHROMA_DOWNSAMPLING_AVERAGE = 3,      // Uses averaging filter
    AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV = 4     // Uses sharp yuv filter (libsharpyuv), available for 4:2:0 only, ignored for 4:2:2
} avifChromaDownsampling;

// NOTE: avifRGBImage must be initialized with avifRGBImageSetDefaults() (preferred) or memset()
// before use.
typedef struct avifRGBImage
{
    uint32_t width;                        // must match associated avifImage
    uint32_t height;                       // must match associated avifImage
    uint32_t depth;                        // legal depths [8, 10, 12, 16]. if depth>8, pixels must be uint16_t internally
    avifRGBFormat format;                  // all channels are always full range
    avifChromaUpsampling chromaUpsampling; // How to upsample from 4:2:0 or 4:2:2 UV when converting to RGB (ignored for 4:4:4 and 4:0:0).
                                           // Ignored when converting to YUV. Defaults to AVIF_CHROMA_UPSAMPLING_AUTOMATIC.
    avifChromaDownsampling chromaDownsampling; // How to downsample to 4:2:0 or 4:2:2 UV when converting from RGB (ignored for 4:4:4 and 4:0:0).
                                               // Ignored when converting to RGB. Defaults to AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC.
    avifBool avoidLibYUV; // If AVIF_FALSE and libyuv conversion between RGB and YUV (including upsampling or downsampling if any)
                          // is available for the avifImage/avifRGBImage combination, then libyuv is used. Default is AVIF_FALSE.
    avifBool ignoreAlpha; // Used for XRGB formats, treats formats containing alpha (such as ARGB) as if they were RGB, treating
                          // the alpha bits as if they were all 1.
    avifBool alphaPremultiplied; // indicates if RGB value is pre-multiplied by alpha. Default: false
    avifBool isFloat; // indicates if RGBA values are in half float (f16) format. Valid only when depth == 16. Default: false
    int maxThreads; // Number of threads to be used for the YUV to RGB conversion. Note that this value is ignored for RGB to YUV
                    // conversion. Setting this to zero has the same effect as setting it to one. Negative values are invalid.
                    // Default: 1.

    uint8_t * pixels;
    uint32_t rowBytes;
} avifRGBImage;

// Sets rgb->width, rgb->height, and rgb->depth to image->width, image->height, and image->depth.
// Sets rgb->pixels to NULL and rgb->rowBytes to 0. Sets the other fields of 'rgb' to default
// values.
AVIF_API void avifRGBImageSetDefaults(avifRGBImage * rgb, const avifImage * image);
AVIF_API uint32_t avifRGBImagePixelSize(const avifRGBImage * rgb);

// Convenience functions. If you supply your own pixels/rowBytes, you do not need to use these.
AVIF_API avifResult avifRGBImageAllocatePixels(avifRGBImage * rgb);
AVIF_API void avifRGBImageFreePixels(avifRGBImage * rgb);

// The main conversion functions
AVIF_API avifResult avifImageRGBToYUV(avifImage * image, const avifRGBImage * rgb);
AVIF_API avifResult avifImageYUVToRGB(const avifImage * image, avifRGBImage * rgb);

// Premultiply handling functions.
// (Un)premultiply is automatically done by the main conversion functions above,
// so usually you don't need to call these. They are there for convenience.
AVIF_API avifResult avifRGBImagePremultiplyAlpha(avifRGBImage * rgb);
AVIF_API avifResult avifRGBImageUnpremultiplyAlpha(avifRGBImage * rgb);

// ---------------------------------------------------------------------------
// YUV Utils

AVIF_API int avifFullToLimitedY(uint32_t depth, int v);
AVIF_API int avifFullToLimitedUV(uint32_t depth, int v);
AVIF_API int avifLimitedToFullY(uint32_t depth, int v);
AVIF_API int avifLimitedToFullUV(uint32_t depth, int v);

// ---------------------------------------------------------------------------
// Codec selection

typedef enum avifCodecChoice
{
    AVIF_CODEC_CHOICE_AUTO = 0,
    AVIF_CODEC_CHOICE_AOM,
    AVIF_CODEC_CHOICE_DAV1D,   // Decode only
    AVIF_CODEC_CHOICE_LIBGAV1, // Decode only
    AVIF_CODEC_CHOICE_RAV1E,   // Encode only
    AVIF_CODEC_CHOICE_SVT,     // Encode only
    AVIF_CODEC_CHOICE_AVM      // Experimental (AV2)
} avifCodecChoice;

typedef enum avifCodecFlag
{
    AVIF_CODEC_FLAG_CAN_DECODE = (1 << 0),
    AVIF_CODEC_FLAG_CAN_ENCODE = (1 << 1)
} avifCodecFlag;
typedef uint32_t avifCodecFlags;

// If this returns NULL, the codec choice/flag combination is unavailable
AVIF_API const char * avifCodecName(avifCodecChoice choice, avifCodecFlags requiredFlags);
AVIF_API avifCodecChoice avifCodecChoiceFromName(const char * name);

// ---------------------------------------------------------------------------
// avifIO

struct avifIO;

// Destroy must completely destroy all child structures *and* free the avifIO object itself.
// This function pointer is optional, however, if the avifIO object isn't intended to be owned by
// a libavif encoder/decoder.
typedef void (*avifIODestroyFunc)(struct avifIO * io);

// This function should return a block of memory that *must* remain valid until another read call to
// this avifIO struct is made (reusing a read buffer is acceptable/expected).
//
// * If offset exceeds the size of the content (past EOF), return AVIF_RESULT_IO_ERROR.
// * If offset is *exactly* at EOF, provide a 0-byte buffer and return AVIF_RESULT_OK.
// * If (offset+size) exceeds the contents' size, it must truncate the range to provide all
//   bytes from the offset to EOF.
// * If the range is unavailable yet (due to network conditions or any other reason),
//   return AVIF_RESULT_WAITING_ON_IO.
// * Otherwise, provide the range and return AVIF_RESULT_OK.
typedef avifResult (*avifIOReadFunc)(struct avifIO * io, uint32_t readFlags, uint64_t offset, size_t size, avifROData * out);

typedef avifResult (*avifIOWriteFunc)(struct avifIO * io, uint32_t writeFlags, uint64_t offset, const uint8_t * data, size_t size);

typedef struct avifIO
{
    avifIODestroyFunc destroy;
    avifIOReadFunc read;

    // This is reserved for future use - but currently ignored. Set it to a null pointer.
    avifIOWriteFunc write;

    // If non-zero, this is a hint to internal structures of the max size offered by the content
    // this avifIO structure is reading. If it is a static memory source, it should be the size of
    // the memory buffer; if it is a file, it should be the file's size. If this information cannot
    // be known (as it is streamed-in), set a reasonable upper boundary here (larger than the file
    // can possibly be for your environment, but within your environment's memory constraints). This
    // is used for sanity checks when allocating internal buffers to protect against
    // malformed/malicious files.
    uint64_t sizeHint;

    // If true, *all* memory regions returned from *all* calls to read are guaranteed to be
    // persistent and exist for the lifetime of the avifIO object. If false, libavif will make
    // in-memory copies of samples and metadata content, and a memory region returned from read must
    // only persist until the next call to read.
    avifBool persistent;

    // The contents of this are defined by the avifIO implementation, and should be fully destroyed
    // by the implementation of the associated destroy function, unless it isn't owned by the avifIO
    // struct. It is not necessary to use this pointer in your implementation.
    void * data;
} avifIO;

// Returns NULL if the reader cannot be allocated.
AVIF_API avifIO * avifIOCreateMemoryReader(const uint8_t * data, size_t size);
// Returns NULL if the file cannot be opened or if the reader cannot be allocated.
AVIF_API avifIO * avifIOCreateFileReader(const char * filename);
AVIF_API void avifIODestroy(avifIO * io);

// ---------------------------------------------------------------------------
// avifDecoder

// Some encoders (including very old versions of avifenc) do not implement the AVIF standard
// perfectly, and thus create invalid files. However, these files are likely still recoverable /
// decodable, if it wasn't for the strict requirements imposed by libavif's decoder. These flags
// allow a user of avifDecoder to decide what level of strictness they want in their project.
typedef enum avifStrictFlag
{
    // Disables all strict checks.
    AVIF_STRICT_DISABLED = 0,

    // Requires the PixelInformationProperty ('pixi') be present in AV1 image items. libheif v1.11.0
    // or older does not add the 'pixi' item property to AV1 image items. If you need to decode AVIF
    // images encoded by libheif v1.11.0 or older, be sure to disable this bit. (This issue has been
    // corrected in libheif v1.12.0.)
    AVIF_STRICT_PIXI_REQUIRED = (1 << 0),

    // This demands that the values surfaced in the clap box are valid, determined by attempting to
    // convert the clap box to a crop rect using avifCropRectConvertCleanApertureBox(). If this
    // function returns AVIF_FALSE and this strict flag is set, the decode will fail.
    AVIF_STRICT_CLAP_VALID = (1 << 1),

    // Requires the ImageSpatialExtentsProperty ('ispe') be present in alpha auxiliary image items.
    // avif-serialize 0.7.3 or older does not add the 'ispe' item property to alpha auxiliary image
    // items. If you need to decode AVIF images encoded by the cavif encoder with avif-serialize
    // 0.7.3 or older, be sure to disable this bit. (This issue has been corrected in avif-serialize
    // 0.7.4.) See https://github.com/kornelski/avif-serialize/issues/3 and
    // https://crbug.com/1246678.
    AVIF_STRICT_ALPHA_ISPE_REQUIRED = (1 << 2),

    // Maximum strictness; enables all bits above. This is avifDecoder's default.
    AVIF_STRICT_ENABLED = AVIF_STRICT_PIXI_REQUIRED | AVIF_STRICT_CLAP_VALID | AVIF_STRICT_ALPHA_ISPE_REQUIRED
} avifStrictFlag;
typedef uint32_t avifStrictFlags;

// Useful stats related to a read/write
typedef struct avifIOStats
{
    // Size in bytes of the AV1 image item or track data containing color samples.
    size_t colorOBUSize;
    // Size in bytes of the AV1 image item or track data containing alpha samples.
    size_t alphaOBUSize;
} avifIOStats;

struct avifDecoderData;

typedef enum avifDecoderSource
{
    // Honor the major brand signaled in the beginning of the file to pick between an AVIF sequence
    // ('avis', tracks-based) or a single image ('avif', item-based). If the major brand is neither
    // of these, prefer the AVIF sequence ('avis', tracks-based), if present.
    AVIF_DECODER_SOURCE_AUTO = 0,

    // Use the primary item and the aux (alpha) item in the avif(s).
    // This is where single-image avifs store their image.
    AVIF_DECODER_SOURCE_PRIMARY_ITEM,

    // Use the chunks inside primary/aux tracks in the moov block.
    // This is where avifs image sequences store their images.
    AVIF_DECODER_SOURCE_TRACKS

    // Decode the thumbnail item. Currently unimplemented.
    // AVIF_DECODER_SOURCE_THUMBNAIL_ITEM
} avifDecoderSource;

// Information about the timing of a single image in an image sequence
typedef struct avifImageTiming
{
    uint64_t timescale;            // timescale of the media (Hz)
    double pts;                    // presentation timestamp in seconds (ptsInTimescales / timescale)
    uint64_t ptsInTimescales;      // presentation timestamp in "timescales"
    double duration;               // in seconds (durationInTimescales / timescale)
    uint64_t durationInTimescales; // duration in "timescales"
} avifImageTiming;

typedef enum avifProgressiveState
{
    // The current AVIF/Source does not offer a progressive image. This will always be the state
    // for an image sequence.
    AVIF_PROGRESSIVE_STATE_UNAVAILABLE = 0,

    // The current AVIF/Source offers a progressive image, but avifDecoder.allowProgressive is not
    // enabled, so it will behave as if the image was not progressive and will simply decode the
    // best version of this item.
    AVIF_PROGRESSIVE_STATE_AVAILABLE,

    // The current AVIF/Source offers a progressive image, and avifDecoder.allowProgressive is true.
    // In this state, avifDecoder.imageCount will be the count of all of the available progressive
    // layers, and any specific layer can be decoded using avifDecoderNthImage() as if it was an
    // image sequence, or simply using repeated calls to avifDecoderNextImage() to decode better and
    // better versions of this image.
    AVIF_PROGRESSIVE_STATE_ACTIVE
} avifProgressiveState;
AVIF_API const char * avifProgressiveStateToString(avifProgressiveState progressiveState);

// NOTE: The avifDecoder struct may be extended in a future release. Code outside the libavif
// library must allocate avifDecoder by calling the avifDecoderCreate() function.
typedef struct avifDecoder
{
    // --------------------------------------------------------------------------------------------
    // Inputs

    // Defaults to AVIF_CODEC_CHOICE_AUTO: Preference determined by order in availableCodecs table (avif.c)
    avifCodecChoice codecChoice;

    // Defaults to 1. -- NOTE: Please see the "Understanding maxThreads" comment block above
    int maxThreads;

    // avifs can have multiple sets of images in them. This specifies which to decode.
    // Set this via avifDecoderSetSource().
    avifDecoderSource requestedSource;

    // If this is true and a progressive AVIF is decoded, avifDecoder will behave as if the AVIF is
    // an image sequence, in that it will set imageCount to the number of progressive frames
    // available, and avifDecoderNextImage()/avifDecoderNthImage() will allow for specific layers
    // of a progressive image to be decoded. To distinguish between a progressive AVIF and an AVIF
    // image sequence, inspect avifDecoder.progressiveState.
    avifBool allowProgressive;

    // If this is false, avifDecoderNextImage() will start decoding a frame only after there are
    // enough input bytes to decode all of that frame. If this is true, avifDecoder will decode each
    // subimage or grid cell as soon as possible. The benefits are: grid images may be partially
    // displayed before being entirely available, and the overall decoding may finish earlier.
    // Must be set before calling avifDecoderNextImage() or avifDecoderNthImage().
    // WARNING: Experimental feature.
    avifBool allowIncremental;

    // Enable any of these to avoid reading and surfacing specific data to the decoded avifImage.
    // These can be useful if your avifIO implementation heavily uses AVIF_RESULT_WAITING_ON_IO for
    // streaming data, as some of these payloads are (unfortunately) packed at the end of the file,
    // which will cause avifDecoderParse() to return AVIF_RESULT_WAITING_ON_IO until it finds them.
    // If you don't actually leverage this data, it is best to ignore it here.
    avifBool ignoreExif;
    avifBool ignoreXMP;

    // This represents the maximum size of an image (in pixel count) that libavif and the underlying
    // AV1 decoder should attempt to decode. It defaults to AVIF_DEFAULT_IMAGE_SIZE_LIMIT, and can
    // be set to a smaller value. The value 0 is reserved.
    // Note: Only some underlying AV1 codecs support a configurable size limit (such as dav1d).
    uint32_t imageSizeLimit;

    // This represents the maximum dimension of an image (width or height) that libavif should
    // attempt to decode. It defaults to AVIF_DEFAULT_IMAGE_DIMENSION_LIMIT. Set it to 0 to ignore
    // the limit.
    uint32_t imageDimensionLimit;

    // This provides an upper bound on how many images the decoder is willing to attempt to decode,
    // to provide a bit of protection from malicious or malformed AVIFs citing millions upon
    // millions of frames, only to be invalid later. The default is AVIF_DEFAULT_IMAGE_COUNT_LIMIT
    // (see comment above), and setting this to 0 disables the limit.
    uint32_t imageCountLimit;

    // Strict flags. Defaults to AVIF_STRICT_ENABLED. See avifStrictFlag definitions above.
    avifStrictFlags strictFlags;

    // --------------------------------------------------------------------------------------------
    // Outputs

    // All decoded image data; owned by the decoder. All information in this image is incrementally
    // added and updated as avifDecoder*() functions are called. After a successful call to
    // avifDecoderParse(), all values in decoder->image (other than the planes/rowBytes themselves)
    // will be pre-populated with all information found in the outer AVIF container, prior to any
    // AV1 decoding. If the contents of the inner AV1 payload disagree with the outer container,
    // these values may change after calls to avifDecoderRead*(),avifDecoderNextImage(), or
    // avifDecoderNthImage().
    //
    // The YUV and A contents of this image are likely owned by the decoder, so be sure to copy any
    // data inside of this image before advancing to the next image or reusing the decoder. It is
    // legal to call avifImageYUVToRGB() on this in between calls to avifDecoderNextImage(), but use
    // avifImageCopy() if you want to make a complete, permanent copy of this image's YUV content or
    // metadata.
    avifImage * image;

    // Counts and timing for the current image in an image sequence. Uninteresting for single image files.
    int imageIndex;                        // 0-based
    int imageCount;                        // Always 1 for non-progressive, non-sequence AVIFs.
    avifProgressiveState progressiveState; // See avifProgressiveState declaration
    avifImageTiming imageTiming;           //
    uint64_t timescale;                    // timescale of the media (Hz)
    double duration;                       // duration of a single playback of the image sequence in seconds
                                           // (durationInTimescales / timescale)
    uint64_t durationInTimescales;         // duration of a single playback of the image sequence in "timescales"
    int repetitionCount;                   // number of times the sequence has to be repeated. This can also be one of
                                           // AVIF_REPETITION_COUNT_INFINITE or AVIF_REPETITION_COUNT_UNKNOWN. Essentially, if
                                           // repetitionCount is a non-negative integer `n`, then the image sequence should be
                                           // played back `n + 1` times.

    // This is true when avifDecoderParse() detects an alpha plane. Use this to find out if alpha is
    // present after a successful call to avifDecoderParse(), but prior to any call to
    // avifDecoderNextImage() or avifDecoderNthImage(), as decoder->image->alphaPlane won't exist yet.
    avifBool alphaPresent;

    // stats from the most recent read, possibly 0s if reading an image sequence
    avifIOStats ioStats;

    // Additional diagnostics (such as detailed error state)
    avifDiagnostics diag;

    // --------------------------------------------------------------------------------------------
    // Internals

    // Use one of the avifDecoderSetIO*() functions to set this
    avifIO * io;

    // Internals used by the decoder
    struct avifDecoderData * data;

    // Version 1.0.0 ends here. Add any new members after this line.

    // This is true when avifDecoderParse() detects an image sequence track in the image. If this is true, the image can be
    // decoded either as an animated image sequence or as a still image (the primary image item) by setting avifDecoderSetSource
    // to the appropriate source.
    avifBool imageSequenceTrackPresent;

#if defined(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
    // This is true when avifDecoderParse() detects a gain map.
    avifBool gainMapPresent;
    // Enable decoding the gain map image if present (defaults to AVIF_FALSE).
    // (see also enableParsingGainMapMetadata below).
    // gainMapPresent is still set if the presence of a gain map is detected, regardless
    // of this setting.
    avifBool enableDecodingGainMap;
    // Enable parsing the gain map metadata if present (defaults to AVIF_FALSE).
    // gainMapPresent is still set if the presence of a gain map is detected, regardless
    // of this setting.
    // Gain map metadata is read during avifDecoderParse(). Like Exif and XMP, this data
    // can be (unfortunately) packed at the end of the file, which will cause
    // avifDecoderParse() to return AVIF_RESULT_WAITING_ON_IO until it finds it.
    // If you don't actually use this data, it's best to leave this to AVIF_FALSE (default).
    avifBool enableParsingGainMapMetadata;
    // Do not decode the color/alpha planes of the main image.
    // Can be useful to decode the gain map image only.
    avifBool ignoreColorAndAlpha;
#endif
} avifDecoder;

// Returns NULL in case of memory allocation failure.
AVIF_API avifDecoder * avifDecoderCreate(void);
AVIF_API void avifDecoderDestroy(avifDecoder * decoder);

// Simple interfaces to decode a single image, independent of the decoder afterwards (decoder may be destroyed).
AVIF_API avifResult avifDecoderRead(avifDecoder * decoder, avifImage * image); // call avifDecoderSetIO*() first
AVIF_API avifResult avifDecoderReadMemory(avifDecoder * decoder, avifImage * image, const uint8_t * data, size_t size);
AVIF_API avifResult avifDecoderReadFile(avifDecoder * decoder, avifImage * image, const char * filename);

// Multi-function alternative to avifDecoderRead() for image sequences and gaining direct access
// to the decoder's YUV buffers (for performance's sake). Data passed into avifDecoderParse() is NOT
// copied, so it must continue to exist until the decoder is destroyed.
//
// Usage / function call order is:
// * avifDecoderCreate()
// * avifDecoderSetSource() - optional, the default (AVIF_DECODER_SOURCE_AUTO) is usually sufficient
// * avifDecoderSetIO*()
// * avifDecoderParse()
// * avifDecoderNextImage() - in a loop, using decoder->image after each successful call
// * avifDecoderDestroy()
//
// NOTE: Until avifDecoderParse() returns AVIF_RESULT_OK, no data in avifDecoder should
//       be considered valid, and no queries (such as Keyframe/Timing/MaxExtent) should be made.
//
// You can use avifDecoderReset() any time after a successful call to avifDecoderParse()
// to reset the internal decoder back to before the first frame. Calling either
// avifDecoderSetSource() or avifDecoderParse() will automatically Reset the decoder.
//
// avifDecoderSetSource() allows you not only to choose whether to parse tracks or
// items in a file containing both, but switch between sources without having to
// Parse again. Normally AVIF_DECODER_SOURCE_AUTO is enough for the common path.
AVIF_API avifResult avifDecoderSetSource(avifDecoder * decoder, avifDecoderSource source);
// Note: When avifDecoderSetIO() is called, whether 'decoder' takes ownership of 'io' depends on
// whether io->destroy is set. avifDecoderDestroy(decoder) calls avifIODestroy(io), which calls
// io->destroy(io) if io->destroy is set. Therefore, if io->destroy is not set, then
// avifDecoderDestroy(decoder) has no effects on 'io'.
AVIF_API void avifDecoderSetIO(avifDecoder * decoder, avifIO * io);
AVIF_API avifResult avifDecoderSetIOMemory(avifDecoder * decoder, const uint8_t * data, size_t size);
AVIF_API avifResult avifDecoderSetIOFile(avifDecoder * decoder, const char * filename);
AVIF_API avifResult avifDecoderParse(avifDecoder * decoder);
AVIF_API avifResult avifDecoderNextImage(avifDecoder * decoder);
AVIF_API avifResult avifDecoderNthImage(avifDecoder * decoder, uint32_t frameIndex);
AVIF_API avifResult avifDecoderReset(avifDecoder * decoder);

// Keyframe information
// frameIndex - 0-based, matching avifDecoder->imageIndex, bound by avifDecoder->imageCount
// "nearest" keyframe means the keyframe prior to this frame index (returns frameIndex if it is a keyframe)
// These functions may be used after a successful call (AVIF_RESULT_OK) to avifDecoderParse().
AVIF_NODISCARD AVIF_API avifBool avifDecoderIsKeyframe(const avifDecoder * decoder, uint32_t frameIndex);
AVIF_API uint32_t avifDecoderNearestKeyframe(const avifDecoder * decoder, uint32_t frameIndex);

// Timing helper - This does not change the current image or invoke the codec (safe to call repeatedly)
// This function may be used after a successful call (AVIF_RESULT_OK) to avifDecoderParse().
AVIF_API avifResult avifDecoderNthImageTiming(const avifDecoder * decoder, uint32_t frameIndex, avifImageTiming * outTiming);

// When avifDecoderNextImage() or avifDecoderNthImage() returns AVIF_RESULT_WAITING_ON_IO, this
// function can be called next to retrieve the number of top rows that can be immediately accessed
// from the luma plane of decoder->image, and alpha if any. The corresponding rows from the chroma planes,
// if any, can also be accessed (half rounded up if subsampled, same number of rows otherwise).
// If a gain map is present and AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP is on and enableDecodingGainMap is also on,
// the gain map's planes can also be accessed in the same way. If the gain map's height is different from
// the main image, then the number of available gain map rows is at least:
// roundf((float)decoded_row_count / decoder->image->height * decoder->image->gainMap.image->height)
// When gain map scaling is needed, callers might choose to use a few less rows depending on how many rows
// are needed by the scaling algorithm, to avoid the last row(s) changing when more data becomes available.
// decoder->allowIncremental must be set to true before calling avifDecoderNextImage() or
// avifDecoderNthImage(). Returns decoder->image->height when the last call to avifDecoderNextImage() or
// avifDecoderNthImage() returned AVIF_RESULT_OK. Returns 0 in all other cases.
// WARNING: Experimental feature.
AVIF_API uint32_t avifDecoderDecodedRowCount(const avifDecoder * decoder);

// ---------------------------------------------------------------------------
// avifExtent

typedef struct avifExtent
{
    uint64_t offset;
    size_t size;
} avifExtent;

// Streaming data helper - Use this to calculate the maximal AVIF data extent encompassing all AV1
// sample data needed to decode the Nth image. The offset will be the earliest offset of all
// required AV1 extents for this frame, and the size will create a range including the last byte of
// the last AV1 sample needed. Note that this extent may include non-sample data, as a frame's
// sample data may be broken into multiple extents and interleaved with other data, or in
// non-sequential order. This extent will also encompass all AV1 samples that this frame's sample
// depends on to decode (such as samples for reference frames), from the nearest keyframe up to this
// Nth frame.
//
// If avifDecoderNthImageMaxExtent() returns AVIF_RESULT_OK and the extent's size is 0 bytes, this
// signals that libavif doesn't expect to call avifIO's Read for this frame's decode. This happens if
// data for this frame was read as a part of avifDecoderParse() (typically in an idat box inside of
// a meta box).
//
// This function may be used after a successful call (AVIF_RESULT_OK) to avifDecoderParse().
AVIF_API avifResult avifDecoderNthImageMaxExtent(const avifDecoder * decoder, uint32_t frameIndex, avifExtent * outExtent);

// ---------------------------------------------------------------------------
// avifEncoder

struct avifEncoderData;
struct avifCodecSpecificOptions;

typedef struct avifScalingMode
{
    avifFraction horizontal;
    avifFraction vertical;
} avifScalingMode;

// Notes:
// * The avifEncoder struct may be extended in a future release. Code outside the libavif library
//   must allocate avifEncoder by calling the avifEncoderCreate() function.
// * If avifEncoderWrite() returns AVIF_RESULT_OK, output must be freed with avifRWDataFree()
// * If (maxThreads < 2), multithreading is disabled
//   * NOTE: Please see the "Understanding maxThreads" comment block above
// * Quality range: [AVIF_QUALITY_WORST - AVIF_QUALITY_BEST]
// * Quantizer range: [AVIF_QUANTIZER_BEST_QUALITY - AVIF_QUANTIZER_WORST_QUALITY]
// * In older versions of libavif, the avifEncoder struct doesn't have the quality and qualityAlpha
//   fields. For backward compatibility, if the quality field is not set, the default value of
//   quality is based on the average of minQuantizer and maxQuantizer. Similarly the default value
//   of qualityAlpha is based on the average of minQuantizerAlpha and maxQuantizerAlpha. New code
//   should set quality and qualityAlpha and leave minQuantizer, maxQuantizer, minQuantizerAlpha,
//   and maxQuantizerAlpha initialized to their default values.
// * To enable tiling, set tileRowsLog2 > 0 and/or tileColsLog2 > 0.
//   Tiling values range [0-6], where the value indicates a request for 2^n tiles in that dimension.
//   If autoTiling is set to AVIF_TRUE, libavif ignores tileRowsLog2 and tileColsLog2 and
//   automatically chooses suitable tiling values.
// * Speed range: [AVIF_SPEED_SLOWEST - AVIF_SPEED_FASTEST]. Slower should make for a better quality
//   image in less bytes. AVIF_SPEED_DEFAULT means "Leave the AV1 codec to its default speed settings"./
//   If avifEncoder uses rav1e, the speed value is directly passed through (0-10). If libaom is used,
//   a combination of settings are tweaked to simulate this speed range.
// * Extra layer count: [0 - (AVIF_MAX_AV1_LAYER_COUNT-1)]. Non-zero value indicates a layered
//   (progressive) image.
// * Some encoder settings can be changed after encoding starts. Changes will take effect in the next
//   call to avifEncoderAddImage().
typedef struct avifEncoder
{
    // Defaults to AVIF_CODEC_CHOICE_AUTO: Preference determined by order in availableCodecs table (avif.c)
    avifCodecChoice codecChoice;

    // settings (see Notes above)
    int maxThreads;
    int speed;
    int keyframeInterval;     // Any set of |keyframeInterval| consecutive frames will have at least one keyframe. When it is 0,
                              // there is no such restriction.
    uint64_t timescale;       // timescale of the media (Hz)
    int repetitionCount;      // Number of times the image sequence should be repeated. This can also be set to
                              // AVIF_REPETITION_COUNT_INFINITE for infinite repetitions.  Only applicable for image sequences.
                              // Essentially, if repetitionCount is a non-negative integer `n`, then the image sequence should be
                              // played back `n + 1` times. Defaults to AVIF_REPETITION_COUNT_INFINITE.
    uint32_t extraLayerCount; // EXPERIMENTAL: Non-zero value encodes layered image.

    // changeable encoder settings
    int quality;
    int qualityAlpha;
    int minQuantizer;
    int maxQuantizer;
    int minQuantizerAlpha;
    int maxQuantizerAlpha;
    int tileRowsLog2;
    int tileColsLog2;
    avifBool autoTiling;
    avifScalingMode scalingMode;

    // stats from the most recent write
    avifIOStats ioStats;

    // Additional diagnostics (such as detailed error state)
    avifDiagnostics diag;

    // Internals used by the encoder
    struct avifEncoderData * data;
    struct avifCodecSpecificOptions * csOptions;

    // Version 1.0.0 ends here. Add any new members after this line.

    // Defaults to AVIF_HEADER_FULL
    avifHeaderFormat headerFormat;

#if defined(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
    int qualityGainMap; // changeable encoder setting
#endif
} avifEncoder;

// avifEncoderCreate() returns NULL if a memory allocation failed.
AVIF_NODISCARD AVIF_API avifEncoder * avifEncoderCreate(void);
AVIF_API avifResult avifEncoderWrite(avifEncoder * encoder, const avifImage * image, avifRWData * output);
AVIF_API void avifEncoderDestroy(avifEncoder * encoder);

typedef enum avifAddImageFlag
{
    AVIF_ADD_IMAGE_FLAG_NONE = 0,

    // Force this frame to be a keyframe (sync frame).
    AVIF_ADD_IMAGE_FLAG_FORCE_KEYFRAME = (1 << 0),

    // Use this flag when encoding a single frame, single layer image.
    // Signals "still_picture" to AV1 encoders, which tweaks various compression rules.
    // This is enabled automatically when using the avifEncoderWrite() single-image encode path.
    AVIF_ADD_IMAGE_FLAG_SINGLE = (1 << 1)
} avifAddImageFlag;
typedef uint32_t avifAddImageFlags;

// Multi-function alternative to avifEncoderWrite() for advanced features.
//
// Usage / function call order is:
// * avifEncoderCreate()
// - Still image:
//   * avifEncoderAddImage() [exactly once]
// - Still image grid:
//   * avifEncoderAddImageGrid() [exactly once, AVIF_ADD_IMAGE_FLAG_SINGLE is assumed]
// - Image sequence:
//   * Set encoder->timescale (Hz) correctly
//   * avifEncoderAddImage() ... [repeatedly; at least once]
// - Still layered image:
//   * Set encoder->extraLayerCount correctly
//   * avifEncoderAddImage() ... [exactly encoder->extraLayerCount+1 times]
// - Still layered grid:
//   * Set encoder->extraLayerCount correctly
//   * avifEncoderAddImageGrid() ... [exactly encoder->extraLayerCount+1 times]
// * avifEncoderFinish()
// * avifEncoderDestroy()
//
// The image passed to avifEncoderAddImage() or avifEncoderAddImageGrid() is encoded during the
// call (which may be slow) and can be freed after the function returns.

// durationInTimescales is ignored if AVIF_ADD_IMAGE_FLAG_SINGLE is set in addImageFlags,
// or if we are encoding a layered image.
AVIF_API avifResult avifEncoderAddImage(avifEncoder * encoder, const avifImage * image, uint64_t durationInTimescales, avifAddImageFlags addImageFlags);
AVIF_API avifResult avifEncoderAddImageGrid(avifEncoder * encoder,
                                            uint32_t gridCols,
                                            uint32_t gridRows,
                                            const avifImage * const * cellImages,
                                            avifAddImageFlags addImageFlags);
AVIF_API avifResult avifEncoderFinish(avifEncoder * encoder, avifRWData * output);

// Codec-specific, optional "advanced" tuning settings, in the form of string key/value pairs,
// to be consumed by the codec in the next avifEncoderAddImage() call.
// See the codec documentation to know if a setting is persistent or applied only to the next frame.
// key must be non-NULL, but passing a NULL value will delete the pending key, if it exists.
// Setting an incorrect or unknown option for the current codec will cause errors of type
// AVIF_RESULT_INVALID_CODEC_SPECIFIC_OPTION from avifEncoderWrite() or avifEncoderAddImage().
AVIF_API avifResult avifEncoderSetCodecSpecificOption(avifEncoder * encoder, const char * key, const char * value);

#if defined(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
// Returns the size in bytes of the AV1 image item containing gain map samples, or 0 if no gain map was encoded.
AVIF_API size_t avifEncoderGetGainMapSizeBytes(avifEncoder * encoder);
#endif

// Helpers
AVIF_NODISCARD AVIF_API avifBool avifImageUsesU16(const avifImage * image);
AVIF_NODISCARD AVIF_API avifBool avifImageIsOpaque(const avifImage * image);
// channel can be an avifChannelIndex.
AVIF_API uint8_t * avifImagePlane(const avifImage * image, int channel);
AVIF_API uint32_t avifImagePlaneRowBytes(const avifImage * image, int channel);
AVIF_API uint32_t avifImagePlaneWidth(const avifImage * image, int channel);
AVIF_API uint32_t avifImagePlaneHeight(const avifImage * image, int channel);

// Returns AVIF_TRUE if input begins with a valid FileTypeBox (ftyp) that supports
// either the brand 'avif' or 'avis' (or both), without performing any allocations.
AVIF_NODISCARD AVIF_API avifBool avifPeekCompatibleFileType(const avifROData * input);

#if defined(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
// ---------------------------------------------------------------------------
// Gain Map utilities.
// Gain Maps are a HIGHLY EXPERIMENTAL FEATURE, see comments in the avifGainMap
// section above.

// Performs tone mapping on a base image using the provided gain map.
// The HDR headroom is log2 of the ratio of HDR to SDR white brightness of the display to tone map for.
// 'toneMappedImage' should have the 'format', 'depth', and 'isFloat' fields set to the desired values.
// If non NULL, 'clli' will be filled with the light level information of the tone mapped image.
// NOTE: only used in tests for now, might be added to the public API at some point.
AVIF_API avifResult avifImageApplyGainMap(const avifImage * baseImage,
                                          const avifGainMap * gainMap,
                                          float hdrHeadroom,
                                          avifColorPrimaries outputColorPrimaries,
                                          avifTransferCharacteristics outputTransferCharacteristics,
                                          avifRGBImage * toneMappedImage,
                                          avifContentLightLevelInformationBox * clli,
                                          avifDiagnostics * diag);
// Same as above but takes an avifRGBImage as input instead of avifImage.
AVIF_API avifResult avifRGBImageApplyGainMap(const avifRGBImage * baseImage,
                                             avifColorPrimaries baseColorPrimaries,
                                             avifTransferCharacteristics baseTransferCharacteristics,
                                             const avifGainMap * gainMap,
                                             float hdrHeadroom,
                                             avifColorPrimaries outputColorPrimaries,
                                             avifTransferCharacteristics outputTransferCharacteristics,
                                             avifRGBImage * toneMappedImage,
                                             avifContentLightLevelInformationBox * clli,
                                             avifDiagnostics * diag);

// Computes a gain map between two images: a base image and an alternate image.
// Both images should have the same width and height, and use the same color
// primaries. TODO(maryla): allow different primaries.
// gainMap->image should be initialized with avifImageCreate(), with the width,
// height, depth and yuvFormat fields set to the desired output values for the
// gain map. All of these fields may differ from the source images.
AVIF_API avifResult avifRGBImageComputeGainMap(const avifRGBImage * baseRgbImage,
                                               avifColorPrimaries baseColorPrimaries,
                                               avifTransferCharacteristics baseTransferCharacteristics,
                                               const avifRGBImage * altRgbImage,
                                               avifColorPrimaries altColorPrimaries,
                                               avifTransferCharacteristics altTransferCharacteristics,
                                               avifGainMap * gainMap,
                                               avifDiagnostics * diag);
// Convenience function. Same as above but takes avifImage images as input
// instead of avifRGBImage. Gain map computation is performed in RGB space so
// the images are converted to RGB first.
AVIF_API avifResult avifImageComputeGainMap(const avifImage * baseImage,
                                            const avifImage * altImage,
                                            avifGainMap * gainMap,
                                            avifDiagnostics * diag);

#endif // AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ifndef AVIF_AVIF_H
