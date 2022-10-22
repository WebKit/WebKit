// Copyright 2020 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avifpng.h"
#include "avifutil.h"

#include "png.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// See libpng-manual.txt, section XI.
#if PNG_LIBPNG_VER_MAJOR > 1 || (PNG_LIBPNG_VER_MAJOR == 1 && PNG_LIBPNG_VER_MINOR >= 5)
typedef png_bytep png_iccp_datap;
#else
typedef png_charp png_iccp_datap;
#endif

// Converts a hexadecimal string which contains 2-byte character representations of hexadecimal values to raw data (bytes).
// hexString may contain values consisting of [A-F][a-f][0-9] in pairs, e.g., 7af2..., separated by any number of newlines.
// On success the bytes are filled and AVIF_TRUE is returned.
// AVIF_FALSE is returned if fewer than numExpectedBytes hexadecimal pairs are converted.
static avifBool avifHexStringToBytes(const char * hexString, size_t hexStringLength, size_t numExpectedBytes, avifRWData * bytes)
{
    avifRWDataRealloc(bytes, numExpectedBytes);
    size_t numBytes = 0;
    for (size_t i = 0; (i + 1 < hexStringLength) && (numBytes < numExpectedBytes);) {
        if (hexString[i] == '\n') {
            ++i;
            continue;
        }
        if (!isxdigit(hexString[i]) || !isxdigit(hexString[i + 1])) {
            avifRWDataFree(bytes);
            fprintf(stderr, "Metadata extraction failed: invalid character at " AVIF_FMT_ZU "\n", i);
            return AVIF_FALSE;
        }
        const char twoHexDigits[] = { hexString[i], hexString[i + 1], '\0' };
        bytes->data[numBytes] = (uint8_t)strtol(twoHexDigits, NULL, 16);
        ++numBytes;
        i += 2;
    }

    if (numBytes != numExpectedBytes) {
        avifRWDataFree(bytes);
        fprintf(stderr, "Metadata extraction failed: expected " AVIF_FMT_ZU " tokens but got " AVIF_FMT_ZU "\n", numExpectedBytes, numBytes);
        return AVIF_FALSE;
    }
    return AVIF_TRUE;
}

// Parses the raw profile string of profileLength characters and extracts the payload.
static avifBool avifCopyRawProfile(const char * profile, size_t profileLength, avifRWData * payload)
{
    // ImageMagick formats 'raw profiles' as "\n<name>\n<length>(%8lu)\n<hex payload>\n".
    if (!profile || (profileLength == 0) || (profile[0] != '\n')) {
        fprintf(stderr, "Metadata extraction failed: truncated or malformed raw profile\n");
        return AVIF_FALSE;
    }

    const char * lengthStart = NULL;
    for (size_t i = 1; i < profileLength; ++i) { // i starts at 1 because the first '\n' was already checked above.
        if (profile[i] == '\0') {
            // This should not happen as libpng provides this guarantee but extra safety does not hurt.
            fprintf(stderr, "Metadata extraction failed: malformed raw profile, unexpected null character at " AVIF_FMT_ZU "\n", i);
            return AVIF_FALSE;
        }
        if (profile[i] == '\n') {
            if (!lengthStart) {
                // Skip the name and store the beginning of the string containing the length of the payload.
                lengthStart = &profile[i + 1];
            } else {
                const char * hexPayloadStart = &profile[i + 1];
                const size_t hexPayloadMaxLength = profileLength - (i + 1);
                // Parse the length, now that we are sure that it is surrounded by '\n' within the profileLength characters.
                char * lengthEnd;
                const long expectedLength = strtol(lengthStart, &lengthEnd, 10);
                if (lengthEnd != &profile[i]) {
                    fprintf(stderr, "Metadata extraction failed: malformed raw profile, expected '\\n' but got '\\x%.2X'\n", *lengthEnd);
                    return AVIF_FALSE;
                }
                // No need to check for errno. Just make sure expectedLength is not LONG_MIN and not LONG_MAX.
                if ((expectedLength <= 0) || (expectedLength == LONG_MAX) ||
                    ((unsigned long)expectedLength > (hexPayloadMaxLength / 2))) {
                    fprintf(stderr, "Metadata extraction failed: invalid length %ld\n", expectedLength);
                    return AVIF_FALSE;
                }
                // Note: The profile may be malformed by containing more data than the extracted expectedLength bytes.
                //       Be lenient about it and consider it as a valid payload.
                return avifHexStringToBytes(hexPayloadStart, hexPayloadMaxLength, (size_t)expectedLength, payload);
            }
        }
    }
    fprintf(stderr, "Metadata extraction failed: malformed or truncated raw profile\n");
    return AVIF_FALSE;
}

static avifBool avifRemoveHeader(const avifROData * header, avifRWData * payload)
{
    if (payload->size > header->size && !memcmp(payload->data, header->data, header->size)) {
        memmove(payload->data, payload->data + header->size, payload->size - header->size);
        payload->size -= header->size;
        return AVIF_TRUE;
    }
    return AVIF_FALSE;
}

// Extracts metadata to avif->exif and avif->xmp unless the corresponding *ignoreExif or *ignoreXMP is set to AVIF_TRUE.
// *ignoreExif and *ignoreXMP may be set to AVIF_TRUE if the corresponding Exif or XMP metadata was extracted.
// Returns AVIF_FALSE in case of a parsing error.
static avifBool avifExtractExifAndXMP(png_structp png, png_infop info, avifBool * ignoreExif, avifBool * ignoreXMP, avifImage * avif)
{
#ifdef PNG_eXIf_SUPPORTED
    if (!*ignoreExif) {
        png_uint_32 exifSize = 0;
        png_bytep exif = NULL;
        if (png_get_eXIf_1(png, info, &exifSize, &exif) == PNG_INFO_eXIf) {
            if ((exifSize == 0) || !exif) {
                fprintf(stderr, "Exif extraction failed: empty eXIf chunk\n");
                return AVIF_FALSE;
            }
            avifImageSetMetadataExif(avif, exif, exifSize);
            *ignoreExif = AVIF_TRUE; // Ignore any other Exif chunk.
        }
    }
#endif // PNG_eXIf_SUPPORTED

    // HEIF specification ISO-23008 section A.2.1 allows including and excluding the Exif\0\0 header from AVIF files.
    // The PNG 1.5 extension mentions the omission of this header for the modern standard eXIf chunk.
    const avifROData exifApp1Header = { (const uint8_t *)"Exif\0\0", 6 };
    const avifROData xmpApp1Header = { (const uint8_t *)"http://ns.adobe.com/xap/1.0/\0", 29 };

    // tXMP could be retrieved using the png_get_unknown_chunks() API but tXMP is deprecated
    // and there is no PNG file example with a tXMP chunk lying around, so it is not worth the hassle.

    png_textp text = NULL;
    const png_uint_32 numTextChunks = png_get_text(png, info, &text, NULL);
    for (png_uint_32 i = 0; (!*ignoreExif || !*ignoreXMP) && (i < numTextChunks); ++i, ++text) {
        png_size_t textLength = text->text_length;
#ifdef PNG_iTXt_SUPPORTED
        if ((text->compression == PNG_ITXT_COMPRESSION_NONE) || (text->compression == PNG_ITXT_COMPRESSION_zTXt)) {
            textLength = text->itxt_length;
        }
#endif

        if (!*ignoreExif && !strcmp(text->key, "Raw profile type exif")) {
            if (!avifCopyRawProfile(text->text, textLength, &avif->exif)) {
                return AVIF_FALSE;
            }
            avifRemoveHeader(&exifApp1Header, &avif->exif); // Ignore the return value because the header is optional.
            *ignoreExif = AVIF_TRUE;                        // Ignore any other Exif chunk.
        } else if (!*ignoreXMP && !strcmp(text->key, "Raw profile type xmp")) {
            if (!avifCopyRawProfile(text->text, textLength, &avif->xmp)) {
                return AVIF_FALSE;
            }
            avifRemoveHeader(&xmpApp1Header, &avif->xmp); // Ignore the return value because the header is optional.
            *ignoreXMP = AVIF_TRUE;                       // Ignore any other XMP chunk.
        } else if (!strcmp(text->key, "Raw profile type APP1")) {
            // This can be either Exif, XMP or something else.
            avifRWData metadata = { NULL, 0 };
            if (!avifCopyRawProfile(text->text, textLength, &metadata)) {
                return AVIF_FALSE;
            }
            if (!*ignoreExif && avifRemoveHeader(&exifApp1Header, &metadata)) {
                avifRWDataFree(&avif->exif);
                avif->exif = metadata;
                *ignoreExif = AVIF_TRUE; // Ignore any other Exif chunk.
            } else if (!*ignoreXMP && avifRemoveHeader(&xmpApp1Header, &metadata)) {
                avifRWDataFree(&avif->xmp);
                avif->xmp = metadata;
                *ignoreXMP = AVIF_TRUE; // Ignore any other XMP chunk.
            } else {
                avifRWDataFree(&metadata); // Discard chunk.
            }
        } else if (!*ignoreXMP && !strcmp(text->key, "XML:com.adobe.xmp")) {
            if (textLength == 0) {
                fprintf(stderr, "XMP extraction failed: empty XML:com.adobe.xmp payload\n");
                return AVIF_FALSE;
            }
            avifImageSetMetadataXMP(avif, (const uint8_t *)text->text, textLength);
            *ignoreXMP = AVIF_TRUE; // Ignore any other XMP chunk.
        }
    }
    return AVIF_TRUE;
}

// Note on setjmp() and volatile variables:
//
// K & R, The C Programming Language 2nd Ed, p. 254 says:
//   ... Accessible objects have the values they had when longjmp was called,
//   except that non-volatile automatic variables in the function calling setjmp
//   become undefined if they were changed after the setjmp call.
//
// Therefore, 'rowPointers' is declared as volatile. 'rgb' should be declared as
// volatile, but doing so would be inconvenient (try it) and since it is a
// struct, the compiler is unlikely to put it in a register. 'readResult' and
// 'writeResult' do not need to be declared as volatile because they are not
// modified between setjmp and longjmp. But GCC's -Wclobbered warning may have
// trouble figuring that out, so we preemptively declare them as volatile.

avifBool avifPNGRead(const char * inputFilename,
                     avifImage * avif,
                     avifPixelFormat requestedFormat,
                     uint32_t requestedDepth,
                     avifChromaDownsampling chromaDownsampling,
                     avifBool ignoreICC,
                     avifBool ignoreExif,
                     avifBool ignoreXMP,
                     uint32_t * outPNGDepth)
{
    volatile avifBool readResult = AVIF_FALSE;
    png_structp png = NULL;
    png_infop info = NULL;
    png_bytep * volatile rowPointers = NULL;

    avifRGBImage rgb;
    memset(&rgb, 0, sizeof(avifRGBImage));

    FILE * f = fopen(inputFilename, "rb");
    if (!f) {
        fprintf(stderr, "Can't open PNG file for read: %s\n", inputFilename);
        goto cleanup;
    }

    uint8_t header[8];
    size_t bytesRead = fread(header, 1, 8, f);
    if (bytesRead != 8) {
        fprintf(stderr, "Can't read PNG header: %s\n", inputFilename);
        goto cleanup;
    }
    if (png_sig_cmp(header, 0, 8)) {
        fprintf(stderr, "Not a PNG: %s\n", inputFilename);
        goto cleanup;
    }

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fprintf(stderr, "Cannot init libpng (png): %s\n", inputFilename);
        goto cleanup;
    }
    info = png_create_info_struct(png);
    if (!info) {
        fprintf(stderr, "Cannot init libpng (info): %s\n", inputFilename);
        goto cleanup;
    }

    if (setjmp(png_jmpbuf(png))) {
        fprintf(stderr, "Error reading PNG: %s\n", inputFilename);
        goto cleanup;
    }

    png_init_io(png, f);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    if (!ignoreICC) {
        char * iccpProfileName = NULL;
        int iccpCompression = 0;
        unsigned char * iccpData = NULL;
        png_uint_32 iccpDataLen = 0;
        if (png_get_iCCP(png, info, &iccpProfileName, &iccpCompression, (png_iccp_datap *)&iccpData, &iccpDataLen) == PNG_INFO_iCCP) {
            avifImageSetProfileICC(avif, iccpData, iccpDataLen);
        }
        // Note: There is no support for the rare "Raw profile type icc" or "Raw profile type icm" text chunks.
        // TODO(yguyon): Also check if there is a cICp chunk (https://github.com/AOMediaCodec/libavif/pull/1065#discussion_r958534232)
    }

    int rawWidth = png_get_image_width(png, info);
    int rawHeight = png_get_image_height(png, info);
    png_byte rawColorType = png_get_color_type(png, info);
    png_byte rawBitDepth = png_get_bit_depth(png, info);

    if (rawColorType == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }

    if ((rawColorType == PNG_COLOR_TYPE_GRAY) && (rawBitDepth < 8)) {
        png_set_expand_gray_1_2_4_to_8(png);
    }

    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
    }

    if ((rawColorType == PNG_COLOR_TYPE_RGB) || (rawColorType == PNG_COLOR_TYPE_GRAY) || (rawColorType == PNG_COLOR_TYPE_PALETTE)) {
        png_set_filler(png, 0xFFFF, PNG_FILLER_AFTER);
    }

    if ((rawColorType == PNG_COLOR_TYPE_GRAY) || (rawColorType == PNG_COLOR_TYPE_GRAY_ALPHA)) {
        png_set_gray_to_rgb(png);
    }

    int imgBitDepth = 8;
    if (rawBitDepth == 16) {
        png_set_swap(png);
        imgBitDepth = 16;
    }

    if (outPNGDepth) {
        *outPNGDepth = imgBitDepth;
    }

    png_read_update_info(png, info);

    avif->width = rawWidth;
    avif->height = rawHeight;
    avif->yuvFormat = requestedFormat;
    if (avif->yuvFormat == AVIF_PIXEL_FORMAT_NONE) {
        if (avif->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY) {
            // Identity is only valid with YUV444.
            avif->yuvFormat = AVIF_PIXEL_FORMAT_YUV444;
        } else if ((rawColorType == PNG_COLOR_TYPE_GRAY) || (rawColorType == PNG_COLOR_TYPE_GRAY_ALPHA)) {
            avif->yuvFormat = AVIF_PIXEL_FORMAT_YUV400;
        } else {
            avif->yuvFormat = AVIF_APP_DEFAULT_PIXEL_FORMAT;
        }
    }
    avif->depth = requestedDepth;
    if (avif->depth == 0) {
        if (imgBitDepth == 8) {
            avif->depth = 8;
        } else {
            avif->depth = 12;
        }
    }

    avifRGBImageSetDefaults(&rgb, avif);
    rgb.chromaDownsampling = chromaDownsampling;
    rgb.depth = imgBitDepth;
    avifRGBImageAllocatePixels(&rgb);
    rowPointers = (png_bytep *)malloc(sizeof(png_bytep) * rgb.height);
    for (uint32_t y = 0; y < rgb.height; ++y) {
        rowPointers[y] = &rgb.pixels[y * rgb.rowBytes];
    }
    png_read_image(png, rowPointers);
    if (avifImageRGBToYUV(avif, &rgb) != AVIF_RESULT_OK) {
        fprintf(stderr, "Conversion to YUV failed: %s\n", inputFilename);
        goto cleanup;
    }

    // Read Exif metadata at the beginning of the file.
    if (!avifExtractExifAndXMP(png, info, &ignoreExif, &ignoreXMP, avif)) {
        goto cleanup;
    }
    // Read Exif or XMP metadata at the end of the file if there was none at the beginning.
    if (!ignoreExif || !ignoreXMP) {
        png_read_end(png, info);
        if (!avifExtractExifAndXMP(png, info, &ignoreExif, &ignoreXMP, avif)) {
            goto cleanup;
        }
    }
    readResult = AVIF_TRUE;

cleanup:
    if (f) {
        fclose(f);
    }
    if (png) {
        png_destroy_read_struct(&png, &info, NULL);
    }
    if (rowPointers) {
        free(rowPointers);
    }
    avifRGBImageFreePixels(&rgb);
    return readResult;
}

avifBool avifPNGWrite(const char * outputFilename, const avifImage * avif, uint32_t requestedDepth, avifChromaUpsampling chromaUpsampling, int compressionLevel)
{
    volatile avifBool writeResult = AVIF_FALSE;
    png_structp png = NULL;
    png_infop info = NULL;
    png_bytep * volatile rowPointers = NULL;
    FILE * volatile f = NULL;

    avifRGBImage rgb;
    memset(&rgb, 0, sizeof(avifRGBImage));

    volatile int rgbDepth = requestedDepth;
    if (rgbDepth == 0) {
        if (avif->depth > 8) {
            rgbDepth = 16;
        } else {
            rgbDepth = 8;
        }
    }

    volatile avifBool monochrome8bit = (avif->yuvFormat == AVIF_PIXEL_FORMAT_YUV400) && !avif->alphaPlane && (avif->depth == 8) &&
                                       (rgbDepth == 8);

    volatile int colorType;
    if (monochrome8bit) {
        colorType = PNG_COLOR_TYPE_GRAY;
    } else {
        avifRGBImageSetDefaults(&rgb, avif);
        rgb.chromaUpsampling = chromaUpsampling;
        rgb.depth = rgbDepth;
        colorType = PNG_COLOR_TYPE_RGBA;
        if (!avif->alphaPlane) {
            colorType = PNG_COLOR_TYPE_RGB;
            rgb.format = AVIF_RGB_FORMAT_RGB;
        }
        avifRGBImageAllocatePixels(&rgb);
        if (avifImageYUVToRGB(avif, &rgb) != AVIF_RESULT_OK) {
            fprintf(stderr, "Conversion to RGB failed: %s\n", outputFilename);
            goto cleanup;
        }
    }

    f = fopen(outputFilename, "wb");
    if (!f) {
        fprintf(stderr, "Can't open PNG file for write: %s\n", outputFilename);
        goto cleanup;
    }

    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fprintf(stderr, "Cannot init libpng (png): %s\n", outputFilename);
        goto cleanup;
    }
    info = png_create_info_struct(png);
    if (!info) {
        fprintf(stderr, "Cannot init libpng (info): %s\n", outputFilename);
        goto cleanup;
    }

    if (setjmp(png_jmpbuf(png))) {
        fprintf(stderr, "Error writing PNG: %s\n", outputFilename);
        goto cleanup;
    }

    png_init_io(png, f);

    // Don't bother complaining about ICC profile's contents when transferring from AVIF to PNG.
    // It is up to the enduser to decide if they want to keep their ICC profiles or not.
#if defined(PNG_SKIP_sRGB_CHECK_PROFILE) && defined(PNG_SET_OPTION_SUPPORTED) // See libpng-manual.txt, section XII.
    png_set_option(png, PNG_SKIP_sRGB_CHECK_PROFILE, PNG_OPTION_ON);
#endif

    if (compressionLevel >= 0) {
        png_set_compression_level(png, compressionLevel);
    }

    png_set_IHDR(png, info, avif->width, avif->height, rgbDepth, colorType, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    if (avif->icc.data && (avif->icc.size > 0)) {
        png_set_iCCP(png, info, "libavif", 0, (png_iccp_datap)avif->icc.data, (png_uint_32)avif->icc.size);
    }
    png_write_info(png, info);

    rowPointers = (png_bytep *)malloc(sizeof(png_bytep) * avif->height);
    if (monochrome8bit) {
        uint8_t * yPlane = avif->yuvPlanes[AVIF_CHAN_Y];
        uint32_t yRowBytes = avif->yuvRowBytes[AVIF_CHAN_Y];
        for (uint32_t y = 0; y < avif->height; ++y) {
            rowPointers[y] = &yPlane[y * yRowBytes];
        }
    } else {
        for (uint32_t y = 0; y < avif->height; ++y) {
            rowPointers[y] = &rgb.pixels[y * rgb.rowBytes];
        }
    }

    if (rgbDepth > 8) {
        png_set_swap(png);
    }

    png_write_image(png, rowPointers);
    png_write_end(png, NULL);

    writeResult = AVIF_TRUE;
    printf("Wrote PNG: %s\n", outputFilename);
cleanup:
    if (f) {
        fclose(f);
    }
    if (png) {
        png_destroy_write_struct(&png, &info);
    }
    if (rowPointers) {
        free(rowPointers);
    }
    avifRGBImageFreePixels(&rgb);
    return writeResult;
}
