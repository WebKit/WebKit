// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avifutil.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "avifjpeg.h"
#include "avifpng.h"
#include "y4m.h"

// |a| and |b| hold int32_t values. The int64_t type is used so that we can negate INT32_MIN without
// overflowing int32_t.
static int64_t calcGCD(int64_t a, int64_t b)
{
    if (a < 0) {
        a *= -1;
    }
    if (b < 0) {
        b *= -1;
    }
    while (b != 0) {
        int64_t r = a % b;
        a = b;
        b = r;
    }
    return a;
}

static void printClapFraction(const char * name, int32_t n, int32_t d)
{
    printf("%s: %d/%d", name, n, d);
    if (d != 0) {
        int64_t gcd = calcGCD(n, d);
        if (gcd > 1) {
            int32_t rn = (int32_t)(n / gcd);
            int32_t rd = (int32_t)(d / gcd);
            printf(" (%d/%d)", rn, rd);
        }
    }
}

static void avifImageDumpInternal(const avifImage * avif,
                                  uint32_t gridCols,
                                  uint32_t gridRows,
                                  avifBool alphaPresent,
                                  avifBool gainMapPresent,
                                  avifProgressiveState progressiveState)
{
    uint32_t width = avif->width;
    uint32_t height = avif->height;
    if (gridCols && gridRows) {
        width *= gridCols;
        height *= gridRows;
    }
    printf(" * Resolution     : %ux%u\n", width, height);
    printf(" * Bit Depth      : %u\n", avif->depth);
    printf(" * Format         : %s\n", avifPixelFormatToString(avif->yuvFormat));
    if (avif->yuvFormat == AVIF_PIXEL_FORMAT_YUV420) {
        printf(" * Chroma Sam. Pos: %u\n", avif->yuvChromaSamplePosition);
    }
    printf(" * Alpha          : %s\n", alphaPresent ? (avif->alphaPremultiplied ? "Premultiplied" : "Not premultiplied") : "Absent");
    printf(" * Range          : %s\n", (avif->yuvRange == AVIF_RANGE_FULL) ? "Full" : "Limited");

    printf(" * Color Primaries: %u\n", avif->colorPrimaries);
    printf(" * Transfer Char. : %u\n", avif->transferCharacteristics);
    printf(" * Matrix Coeffs. : %u\n", avif->matrixCoefficients);

    if (avif->icc.size != 0) {
        printf(" * ICC Profile    : Present (%" AVIF_FMT_ZU " bytes)\n", avif->icc.size);
    } else {
        printf(" * ICC Profile    : Absent\n");
    }
    if (avif->xmp.size != 0) {
        printf(" * XMP Metadata   : Present (%" AVIF_FMT_ZU " bytes)\n", avif->xmp.size);
    } else {
        printf(" * XMP Metadata   : Absent\n");
    }
    if (avif->exif.size != 0) {
        printf(" * Exif Metadata  : Present (%" AVIF_FMT_ZU " bytes)\n", avif->exif.size);
    } else {
        printf(" * Exif Metadata  : Absent\n");
    }

    if (avif->transformFlags == AVIF_TRANSFORM_NONE) {
        printf(" * Transformations: None\n");
    } else {
        printf(" * Transformations:\n");

        if (avif->transformFlags & AVIF_TRANSFORM_PASP) {
            printf("    * pasp (Aspect Ratio)  : %d/%d\n", (int)avif->pasp.hSpacing, (int)avif->pasp.vSpacing);
        }
        if (avif->transformFlags & AVIF_TRANSFORM_CLAP) {
            printf("    * clap (Clean Aperture): ");
            printClapFraction("W", (int32_t)avif->clap.widthN, (int32_t)avif->clap.widthD);
            printf(", ");
            printClapFraction("H", (int32_t)avif->clap.heightN, (int32_t)avif->clap.heightD);
            printf(", ");
            printClapFraction("hOff", (int32_t)avif->clap.horizOffN, (int32_t)avif->clap.horizOffD);
            printf(", ");
            printClapFraction("vOff", (int32_t)avif->clap.vertOffN, (int32_t)avif->clap.vertOffD);
            printf("\n");

            avifCropRect cropRect;
            avifDiagnostics diag;
            avifDiagnosticsClearError(&diag);
            avifBool validClap =
                avifCropRectConvertCleanApertureBox(&cropRect, &avif->clap, avif->width, avif->height, avif->yuvFormat, &diag);
            if (validClap) {
                printf("      * Valid, derived crop rect: X: %d, Y: %d, W: %d, H: %d\n",
                       cropRect.x,
                       cropRect.y,
                       cropRect.width,
                       cropRect.height);
            } else {
                printf("      * Invalid: %s\n", diag.error);
            }
        }
        if (avif->transformFlags & AVIF_TRANSFORM_IROT) {
            printf("    * irot (Rotation)      : %u\n", avif->irot.angle);
        }
        if (avif->transformFlags & AVIF_TRANSFORM_IMIR) {
            printf("    * imir (Mirror)        : %u (%s)\n", avif->imir.axis, (avif->imir.axis == 0) ? "top-to-bottom" : "left-to-right");
        }
    }
    printf(" * Progressive    : %s\n", avifProgressiveStateToString(progressiveState));
    if (avif->clli.maxCLL > 0 || avif->clli.maxPALL > 0) {
        printf(" * CLLI           : %hu, %hu\n", avif->clli.maxCLL, avif->clli.maxPALL);
    }

#if defined(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
    printf(" * Gain map       : ");
    avifImage * gainMapImage = avif->gainMap ? avif->gainMap->image : NULL;
    if (gainMapImage != NULL) {
        printf("%ux%u pixels, %u bit, %s, %s Range, Matrix Coeffs. %u, Base Image is %s\n",
               gainMapImage->width,
               gainMapImage->height,
               gainMapImage->depth,
               avifPixelFormatToString(gainMapImage->yuvFormat),
               (gainMapImage->yuvRange == AVIF_RANGE_FULL) ? "Full" : "Limited",
               gainMapImage->matrixCoefficients,
               (avif->gainMap->metadata.baseHdrHeadroomN == 0) ? "SDR" : "HDR");
        printf(" * Alternate image:\n");
        printf("    * Color Primaries: %u\n", avif->gainMap->altColorPrimaries);
        printf("    * Transfer Char. : %u\n", avif->gainMap->altTransferCharacteristics);
        printf("    * Matrix Coeffs. : %u\n", avif->gainMap->altMatrixCoefficients);
        if (avif->gainMap->altICC.size != 0) {
            printf("    * ICC Profile    : Present (%" AVIF_FMT_ZU " bytes)\n", avif->gainMap->altICC.size);
        } else {
            printf("    * ICC Profile    : Absent\n");
        }
        if (avif->gainMap->altDepth) {
            printf("    * Bit Depth      : %u\n", avif->gainMap->altDepth);
        }
        if (avif->gainMap->altPlaneCount) {
            printf("    * Planes         : %u\n", avif->gainMap->altPlaneCount);
        }
        if (gainMapImage->clli.maxCLL > 0 || gainMapImage->clli.maxPALL > 0) {
            printf("    * CLLI           : %hu, %hu\n", gainMapImage->clli.maxCLL, gainMapImage->clli.maxPALL);
        }
        printf("\n");
    } else if (gainMapPresent) {
        printf("Present (but ignored)\n");
    } else {
        printf("Absent\n");
    }
#else
    (void)gainMapPresent;
#endif // AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP
}

void avifImageDump(const avifImage * avif, uint32_t gridCols, uint32_t gridRows, avifBool gainMapPresent, avifProgressiveState progressiveState)
{
    const avifBool alphaPresent = avif->alphaPlane && (avif->alphaRowBytes > 0);
    avifImageDumpInternal(avif, gridCols, gridRows, alphaPresent, gainMapPresent, progressiveState);
}

void avifContainerDump(const avifDecoder * decoder)
{
    avifBool gainMapPresent = AVIF_FALSE;
#if defined(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
    gainMapPresent = decoder->gainMapPresent;
#endif
    avifImageDumpInternal(decoder->image, 0, 0, decoder->alphaPresent, gainMapPresent, decoder->progressiveState);
    if ((decoder->progressiveState == AVIF_PROGRESSIVE_STATE_UNAVAILABLE) && (decoder->imageCount > 1)) {
        if (decoder->repetitionCount == AVIF_REPETITION_COUNT_INFINITE) {
            printf(" * Repeat Count   : Infinite\n");
        } else if (decoder->repetitionCount == AVIF_REPETITION_COUNT_UNKNOWN) {
            printf(" * Repeat Count   : Unknown\n");
        } else {
            printf(" * Repeat Count   : %d\n", decoder->repetitionCount);
        }
    }
}

void avifPrintVersions(void)
{
    char codecVersions[256];
    avifCodecVersions(codecVersions);
    printf("Version: %s (%s)\n", avifVersion(), codecVersions);

    unsigned int libyuvVersion = avifLibYUVVersion();
    if (libyuvVersion == 0) {
        printf("libyuv : unavailable\n");
    } else {
        printf("libyuv : available (%u)\n", libyuvVersion);
    }

    printf("\n");
}

avifAppFileFormat avifGuessFileFormat(const char * filename)
{
    // Guess from the file header
    FILE * f = fopen(filename, "rb");
    if (f) {
        uint8_t headerBuffer[144];
        size_t bytesRead = fread(headerBuffer, 1, sizeof(headerBuffer), f);
        fclose(f);

        if (bytesRead > 0) {
            // If the file could be read, use the first bytes to guess the file format.
            return avifGuessBufferFileFormat(headerBuffer, bytesRead);
        }
    }

    // If we get here, the file header couldn't be read for some reason. Guess from the extension.

    const char * fileExt = strrchr(filename, '.');
    if (!fileExt) {
        return AVIF_APP_FILE_FORMAT_UNKNOWN;
    }
    ++fileExt; // skip past the dot

    char lowercaseExt[8]; // This only needs to fit up to "jpeg", so this is plenty
    const size_t fileExtLen = strlen(fileExt);
    if (fileExtLen >= sizeof(lowercaseExt)) { // >= accounts for NULL terminator
        return AVIF_APP_FILE_FORMAT_UNKNOWN;
    }

    for (size_t i = 0; i < fileExtLen; ++i) {
        lowercaseExt[i] = (char)tolower((unsigned char)fileExt[i]);
    }
    lowercaseExt[fileExtLen] = 0;

    if (!strcmp(lowercaseExt, "avif")) {
        return AVIF_APP_FILE_FORMAT_AVIF;
    } else if (!strcmp(lowercaseExt, "y4m")) {
        return AVIF_APP_FILE_FORMAT_Y4M;
    } else if (!strcmp(lowercaseExt, "jpg") || !strcmp(lowercaseExt, "jpeg")) {
        return AVIF_APP_FILE_FORMAT_JPEG;
    } else if (!strcmp(lowercaseExt, "png")) {
        return AVIF_APP_FILE_FORMAT_PNG;
    }
    return AVIF_APP_FILE_FORMAT_UNKNOWN;
}

avifAppFileFormat avifGuessBufferFileFormat(const uint8_t * data, size_t size)
{
    if (size == 0) {
        return AVIF_APP_FILE_FORMAT_UNKNOWN;
    }

    avifROData header;
    header.data = data;
    header.size = size;

    if (avifPeekCompatibleFileType(&header)) {
        return AVIF_APP_FILE_FORMAT_AVIF;
    }

    static const uint8_t signatureJPEG[2] = { 0xFF, 0xD8 };
    static const uint8_t signaturePNG[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    static const uint8_t signatureY4M[9] = { 0x59, 0x55, 0x56, 0x34, 0x4D, 0x50, 0x45, 0x47, 0x32 }; // "YUV4MPEG2"
    struct avifHeaderSignature
    {
        avifAppFileFormat format;
        const uint8_t * magic;
        size_t magicSize;
    } signatures[] = { { AVIF_APP_FILE_FORMAT_JPEG, signatureJPEG, sizeof(signatureJPEG) },
                       { AVIF_APP_FILE_FORMAT_PNG, signaturePNG, sizeof(signaturePNG) },
                       { AVIF_APP_FILE_FORMAT_Y4M, signatureY4M, sizeof(signatureY4M) } };
    const size_t signaturesCount = sizeof(signatures) / sizeof(signatures[0]);

    for (size_t signatureIndex = 0; signatureIndex < signaturesCount; ++signatureIndex) {
        const struct avifHeaderSignature * const signature = &signatures[signatureIndex];
        if (header.size < signature->magicSize) {
            continue;
        }
        if (!memcmp(header.data, signature->magic, signature->magicSize)) {
            return signature->format;
        }
    }

    return AVIF_APP_FILE_FORMAT_UNKNOWN;
}

avifAppFileFormat avifReadImage(const char * filename,
                                avifPixelFormat requestedFormat,
                                int requestedDepth,
                                avifChromaDownsampling chromaDownsampling,
                                avifBool ignoreColorProfile,
                                avifBool ignoreExif,
                                avifBool ignoreXMP,
                                avifBool allowChangingCicp,
                                avifBool ignoreGainMap,
                                uint32_t imageSizeLimit,
                                avifImage * image,
                                uint32_t * outDepth,
                                avifAppSourceTiming * sourceTiming,
                                struct y4mFrameIterator ** frameIter)
{
    const avifAppFileFormat format = avifGuessFileFormat(filename);
    if (format == AVIF_APP_FILE_FORMAT_Y4M) {
        if (!y4mRead(filename, imageSizeLimit, image, sourceTiming, frameIter)) {
            return AVIF_APP_FILE_FORMAT_UNKNOWN;
        }
        if (outDepth) {
            *outDepth = image->depth;
        }
    } else if (format == AVIF_APP_FILE_FORMAT_JPEG) {
        if (!avifJPEGRead(filename, image, requestedFormat, requestedDepth, chromaDownsampling, ignoreColorProfile, ignoreExif, ignoreXMP, ignoreGainMap, imageSizeLimit)) {
            return AVIF_APP_FILE_FORMAT_UNKNOWN;
        }
        if (outDepth) {
            *outDepth = 8;
        }
    } else if (format == AVIF_APP_FILE_FORMAT_PNG) {
        if (!avifPNGRead(filename,
                         image,
                         requestedFormat,
                         requestedDepth,
                         chromaDownsampling,
                         ignoreColorProfile,
                         ignoreExif,
                         ignoreXMP,
                         allowChangingCicp,
                         imageSizeLimit,
                         outDepth)) {
            return AVIF_APP_FILE_FORMAT_UNKNOWN;
        }
    } else {
        fprintf(stderr, "Unrecognized file format for input file: %s\n", filename);
        return AVIF_APP_FILE_FORMAT_UNKNOWN;
    }
    return format;
}

void avifImageFixXMP(avifImage * image)
{
    // Zero bytes are forbidden in UTF-8 XML: https://en.wikipedia.org/wiki/Valid_characters_in_XML
    // Keeping zero bytes in XMP may lead to issues at encoding or decoding.
    // For example, the PNG specification forbids null characters in XMP. See avifPNGWrite().
    // The XMP Specification Part 3 says "When XMP is encoded as UTF-8,
    // there are no zero bytes in the XMP packet" for GIF.

    // Consider a single trailing null character following a non-null character
    // as a programming error. Leave other null characters as is.
    // See the discussion at https://github.com/AOMediaCodec/libavif/issues/1333.
    if (image->xmp.size >= 2 && image->xmp.data[image->xmp.size - 1] == '\0' && image->xmp.data[image->xmp.size - 2] != '\0') {
        --image->xmp.size;
    }
}

void avifDumpDiagnostics(const avifDiagnostics * diag)
{
    if (!*diag->error) {
        return;
    }

    printf("Diagnostics:\n");
    printf(" * %s\n", diag->error);
}

// ---------------------------------------------------------------------------
// avifQueryCPUCount (separated into OS implementations)

#if defined(_WIN32)

// Windows

#include <windows.h>

int avifQueryCPUCount(void)
{
    int numCPU;
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    numCPU = sysinfo.dwNumberOfProcessors;
    return numCPU;
}

#elif defined(__APPLE__)

// Apple

#include <sys/sysctl.h>

int avifQueryCPUCount()
{
    int mib[4];
    int numCPU;
    size_t len = sizeof(numCPU);

    /* set the mib for hw.ncpu */
    mib[0] = CTL_HW;
    mib[1] = HW_AVAILCPU; // alternatively, try HW_NCPU;

    /* get the number of CPUs from the system */
    sysctl(mib, 2, &numCPU, &len, NULL, 0);

    if (numCPU < 1) {
        mib[1] = HW_NCPU;
        sysctl(mib, 2, &numCPU, &len, NULL, 0);
        if (numCPU < 1)
            numCPU = 1;
    }
    return numCPU;
}

#elif defined(__EMSCRIPTEN__)

// Emscripten

int avifQueryCPUCount()
{
    return 1;
}

#else

// POSIX

#include <unistd.h>

int avifQueryCPUCount()
{
    int numCPU = (int)sysconf(_SC_NPROCESSORS_ONLN);
    return (numCPU > 0) ? numCPU : 1;
}

#endif
