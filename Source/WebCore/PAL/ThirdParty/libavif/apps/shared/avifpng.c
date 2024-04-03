// Copyright 2020 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avifpng.h"
#include "avifexif.h"
#include "avifutil.h"
#include "iccmaker.h"

#include "png.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(PNG_eXIf_SUPPORTED) || !defined(PNG_iTXt_SUPPORTED)
#error "libpng 1.6.32 or above with PNG_eXIf_SUPPORTED and PNG_iTXt_SUPPORTED is required."
#endif

//------------------------------------------------------------------------------
// Reading

// Converts a hexadecimal string which contains 2-byte character representations of hexadecimal values to raw data (bytes).
// hexString may contain values consisting of [A-F][a-f][0-9] in pairs, e.g., 7af2..., separated by any number of newlines.
// On success the bytes are filled and AVIF_TRUE is returned.
// AVIF_FALSE is returned if fewer than numExpectedBytes hexadecimal pairs are converted.
static avifBool avifHexStringToBytes(const char * hexString, size_t hexStringLength, size_t numExpectedBytes, avifRWData * bytes)
{
    if (avifRWDataRealloc(bytes, numExpectedBytes) != AVIF_RESULT_OK) {
        fprintf(stderr, "Metadata extraction failed: out of memory\n");
        return AVIF_FALSE;
    }
    size_t numBytes = 0;
    for (size_t i = 0; (i + 1 < hexStringLength) && (numBytes < numExpectedBytes);) {
        if (hexString[i] == '\n') {
            ++i;
            continue;
        }
        if (!isxdigit(hexString[i]) || !isxdigit(hexString[i + 1])) {
            avifRWDataFree(bytes);
            fprintf(stderr, "Metadata extraction failed: invalid character at %" AVIF_FMT_ZU "\n", i);
            return AVIF_FALSE;
        }
        const char twoHexDigits[] = { hexString[i], hexString[i + 1], '\0' };
        bytes->data[numBytes] = (uint8_t)strtol(twoHexDigits, NULL, 16);
        ++numBytes;
        i += 2;
    }

    if (numBytes != numExpectedBytes) {
        avifRWDataFree(bytes);
        fprintf(stderr, "Metadata extraction failed: expected %" AVIF_FMT_ZU " tokens but got %" AVIF_FMT_ZU "\n", numExpectedBytes, numBytes);
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
            fprintf(stderr, "Metadata extraction failed: malformed raw profile, unexpected null character at %" AVIF_FMT_ZU "\n", i);
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
    if (!*ignoreExif) {
        png_uint_32 exifSize = 0;
        png_bytep exif = NULL;
        if (png_get_eXIf_1(png, info, &exifSize, &exif) == PNG_INFO_eXIf) {
            if ((exifSize == 0) || !exif) {
                fprintf(stderr, "Exif extraction failed: empty eXIf chunk\n");
                return AVIF_FALSE;
            }
            // Avoid avifImageSetMetadataExif() that sets irot/imir.
            if (avifRWDataSet(&avif->exif, exif, exifSize) != AVIF_RESULT_OK) {
                fprintf(stderr, "Exif extraction failed: out of memory\n");
                return AVIF_FALSE;
            }
            // According to the Extensions to the PNG 1.2 Specification, Version 1.5.0, section 3.7:
            //   "It is recommended that unless a decoder has independent knowledge of the validity of the Exif data,
            //    the data should be considered to be of historical value only."
            // Try to remove any Exif orientation data to be safe.
            // It is easier to set it to 1 (the default top-left) than actually removing the tag.
            // libheif has the same behavior, see
            // https://github.com/strukturag/libheif/blob/18291ddebc23c924440a8a3c9a7267fe3beb5901/examples/heif_enc.cc#L703
            // Ignore errors because not being able to set Exif orientation now means it cannot be parsed later either.
            (void)avifSetExifOrientation(&avif->exif, 1);
            *ignoreExif = AVIF_TRUE; // Ignore any other Exif chunk.
        }
    }

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
        if ((text->compression == PNG_ITXT_COMPRESSION_NONE) || (text->compression == PNG_ITXT_COMPRESSION_zTXt)) {
            textLength = text->itxt_length;
        }

        if (!*ignoreExif && !strcmp(text->key, "Raw profile type exif")) {
            if (!avifCopyRawProfile(text->text, textLength, &avif->exif)) {
                return AVIF_FALSE;
            }
            avifRemoveHeader(&exifApp1Header, &avif->exif); // Ignore the return value because the header is optional.
            (void)avifSetExifOrientation(&avif->exif, 1);   // See above.
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
                (void)avifSetExifOrientation(&avif->exif, 1); // See above.
                *ignoreExif = AVIF_TRUE;                      // Ignore any other Exif chunk.
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
            if (avifImageSetMetadataXMP(avif, (const uint8_t *)text->text, textLength) != AVIF_RESULT_OK) {
                fprintf(stderr, "XMP extraction failed: out of memory\n");
                return AVIF_FALSE;
            }
            *ignoreXMP = AVIF_TRUE; // Ignore any other XMP chunk.
        }
    }
    // The iTXt XMP payload may not contain a zero byte according to section 4.2.3.3 of
    // the PNG specification, version 1.2. Still remove one trailing null character if any,
    // in case libpng does not strictly enforce that at decoding.
    avifImageFixXMP(avif);
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
                     avifBool ignoreColorProfile,
                     avifBool ignoreExif,
                     avifBool ignoreXMP,
                     avifBool allowChangingCicp,
                     uint32_t imageSizeLimit,
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
#if defined(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
    if (avif->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO_RO) {
        fprintf(stderr, "AVIF_MATRIX_COEFFICIENTS_YCGCO_RO cannot be used with PNG because it has an even bit depth.\n");
        goto cleanup;
    }
    const avifBool useYCgCoR = (avif->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO_RE);
#else
    const avifBool useYCgCoR = AVIF_FALSE;
#endif
    if (avif->yuvFormat == AVIF_PIXEL_FORMAT_NONE) {
        if ((rawColorType == PNG_COLOR_TYPE_GRAY) || (rawColorType == PNG_COLOR_TYPE_GRAY_ALPHA)) {
            avif->yuvFormat = AVIF_PIXEL_FORMAT_YUV400;
        } else if (avif->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY || useYCgCoR) {
            // Identity and YCgCo-R are only valid with YUV444.
            avif->yuvFormat = AVIF_PIXEL_FORMAT_YUV444;
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
#if defined(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
    if (useYCgCoR) {
        if (imgBitDepth != 8) {
            fprintf(stderr, "AVIF_MATRIX_COEFFICIENTS_YCGCO_RE cannot be used on 16 bit input because it adds two bits.\n");
            goto cleanup;
        }
        if (requestedDepth && requestedDepth != 10) {
            fprintf(stderr, "Cannot request %u bits for YCgCo-Re as it uses 2 extra bits.\n", requestedDepth);
            goto cleanup;
        }
        avif->depth = 10;
    }
#endif

    if (!ignoreColorProfile) {
        char * iccpProfileName = NULL;
        int iccpCompression = 0;
        unsigned char * iccpData = NULL;
        png_uint_32 iccpDataLen = 0;
        int srgbIntent;

        // PNG specification 1.2 Section 4.2.2:
        // The sRGB and iCCP chunks should not both appear.
        //
        // When the sRGB / iCCP chunk is present, applications that recognize it and are capable of color management
        // must ignore the gAMA and cHRM chunks and use the sRGB / iCCP chunk instead.
        if (png_get_iCCP(png, info, &iccpProfileName, &iccpCompression, &iccpData, &iccpDataLen) == PNG_INFO_iCCP) {
            if (avifImageSetProfileICC(avif, iccpData, iccpDataLen) != AVIF_RESULT_OK) {
                fprintf(stderr, "Setting ICC profile failed: out of memory.\n");
                goto cleanup;
            }
        } else if (allowChangingCicp) {
            if (png_get_sRGB(png, info, &srgbIntent) == PNG_INFO_sRGB) {
                // srgbIntent ignored
                avif->colorPrimaries = AVIF_COLOR_PRIMARIES_SRGB;
                avif->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_SRGB;
            } else {
                avifBool needToGenerateICC = AVIF_FALSE;
                double gamma;
                double wX, wY, rX, rY, gX, gY, bX, bY;
                float primaries[8];
                if (png_get_gAMA(png, info, &gamma) == PNG_INFO_gAMA) {
                    gamma = 1.0 / gamma;
                    avif->transferCharacteristics = avifTransferCharacteristicsFindByGamma((float)gamma);
                    if (avif->transferCharacteristics == AVIF_TRANSFER_CHARACTERISTICS_UNKNOWN) {
                        needToGenerateICC = AVIF_TRUE;
                    }
                } else {
                    // No gamma information in file. Assume the default value.
                    // PNG specification 1.2 Section 10.5:
                    // Assume a CRT exponent of 2.2 unless detailed calibration measurements
                    // of this particular CRT are available.
                    gamma = 2.2;
                }

                if (png_get_cHRM(png, info, &wX, &wY, &rX, &rY, &gX, &gY, &bX, &bY) == PNG_INFO_cHRM) {
                    primaries[0] = (float)rX;
                    primaries[1] = (float)rY;
                    primaries[2] = (float)gX;
                    primaries[3] = (float)gY;
                    primaries[4] = (float)bX;
                    primaries[5] = (float)bY;
                    primaries[6] = (float)wX;
                    primaries[7] = (float)wY;
                    avif->colorPrimaries = avifColorPrimariesFind(primaries, NULL);
                    if (avif->colorPrimaries == AVIF_COLOR_PRIMARIES_UNKNOWN) {
                        needToGenerateICC = AVIF_TRUE;
                    }
                } else {
                    // No chromaticity information in file. Assume the default value.
                    // PNG specification 1.2 Section 10.6:
                    // Decoders may wish to do this for PNG files with no cHRM chunk.
                    // In that case, a reasonable default would be the CCIR 709 primaries [ITU-R-BT709].
                    avifColorPrimariesGetValues(AVIF_COLOR_PRIMARIES_BT709, primaries);
                }

                if (needToGenerateICC) {
                    avif->colorPrimaries = AVIF_COLOR_PRIMARIES_UNSPECIFIED;
                    avif->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED;
                    fprintf(stderr,
                            "INFO: legacy PNG color space information found in file %s not matching any CICP value. libavif is generating an ICC profile for it."
                            " Use --ignore-profile to ignore color space information instead (may affect the colors of the encoded AVIF image).\n",
                            inputFilename);

                    avifBool generateICCResult = AVIF_FALSE;
                    if (avif->yuvFormat == AVIF_PIXEL_FORMAT_YUV400) {
                        generateICCResult = avifGenerateGrayICC(&avif->icc, (float)gamma, &primaries[6]);
                    } else {
                        generateICCResult = avifGenerateRGBICC(&avif->icc, (float)gamma, primaries);
                    }

                    if (!generateICCResult) {
                        fprintf(stderr,
                                "WARNING: libavif could not generate an ICC profile for file %s. "
                                "It may be caused by invalid values in the color space information. "
                                "The encoded AVIF image's colors may be affected.\n",
                                inputFilename);
                    }
                }
            }
        }
        // Note: There is no support for the rare "Raw profile type icc" or "Raw profile type icm" text chunks.
        // TODO(yguyon): Also check if there is a cICp chunk (https://github.com/AOMediaCodec/libavif/pull/1065#discussion_r958534232)
    }

    const int numChannels = png_get_channels(png, info);
    if ((numChannels != 3) && (numChannels != 4)) {
        fprintf(stderr, "png_get_channels() should return 3 or 4 but returns %d.\n", numChannels);
        goto cleanup;
    }
    if ((uint32_t)avif->width > imageSizeLimit / (uint32_t)avif->height) {
        fprintf(stderr, "Too big PNG dimensions (%d x %d > %u px): %s\n", avif->width, avif->height, imageSizeLimit, inputFilename);
        goto cleanup;
    }

    avifRGBImageSetDefaults(&rgb, avif);
    rgb.chromaDownsampling = chromaDownsampling;
    rgb.depth = imgBitDepth;
    if (numChannels == 3) {
        rgb.format = AVIF_RGB_FORMAT_RGB;
    }
    if (avifRGBImageAllocatePixels(&rgb) != AVIF_RESULT_OK) {
        fprintf(stderr, "Conversion to YUV failed: %s (out of memory)\n", inputFilename);
        goto cleanup;
    }
    // png_read_image() receives the row pointers but not the row buffer size. Verify the row
    // buffer size is exactly what libpng expects. If they are different, we have a bug and should
    // not proceed.
    const size_t rowBytes = png_get_rowbytes(png, info);
    if (rgb.rowBytes != rowBytes) {
        fprintf(stderr, "avifPNGRead internal error: rowBytes mismatch libavif %u vs libpng %" AVIF_FMT_ZU "\n", rgb.rowBytes, rowBytes);
        goto cleanup;
    }
    rowPointers = (png_bytep *)malloc(sizeof(png_bytep) * rgb.height);
    if (rowPointers == NULL) {
        fprintf(stderr, "avifPNGRead internal error: memory allocation failure");
        goto cleanup;
    }
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

//------------------------------------------------------------------------------
// Writing

avifBool avifPNGWrite(const char * outputFilename, const avifImage * avif, uint32_t requestedDepth, avifChromaUpsampling chromaUpsampling, int compressionLevel)
{
    volatile avifBool writeResult = AVIF_FALSE;
    png_structp png = NULL;
    png_infop info = NULL;
    avifRWData xmp = { NULL, 0 };
    png_bytep * volatile rowPointers = NULL;
    FILE * volatile f = NULL;

    avifRGBImage rgb;
    memset(&rgb, 0, sizeof(avifRGBImage));

    volatile int rgbDepth = requestedDepth;
    if (rgbDepth == 0) {
        rgbDepth = (avif->depth > 8) ? 16 : 8;
    }
#if defined(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
    if (avif->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO_RO) {
        fprintf(stderr, "AVIF_MATRIX_COEFFICIENTS_YCGCO_RO cannot be used with PNG because it has an even bit depth.\n");
        goto cleanup;
    }
    if (avif->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO_RE) {
        if (avif->depth != 10) {
            fprintf(stderr, "avif->depth must be 10 bits and not %u.\n", avif->depth);
            goto cleanup;
        }
        if (requestedDepth && requestedDepth != 8) {
            fprintf(stderr, "Cannot request %u bits for YCgCo-Re as it only works for 8 bits.\n", requestedDepth);
            goto cleanup;
        }

        rgbDepth = 8;
    }
#endif

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
        if (avifImageIsOpaque(avif)) {
            colorType = PNG_COLOR_TYPE_RGB;
            rgb.format = AVIF_RGB_FORMAT_RGB;
        }
        if (avifRGBImageAllocatePixels(&rgb) != AVIF_RESULT_OK) {
            fprintf(stderr, "Conversion to RGB failed: %s (out of memory)\n", outputFilename);
            goto cleanup;
        }
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

    const avifBool hasIcc = avif->icc.data && (avif->icc.size > 0);
    if (hasIcc) {
        // If there is an ICC profile, the CICP values are irrelevant and only the ICC profile
        // is written. If we could extract the primaries/transfer curve from the ICC profile,
        // then they could be written in cHRM/gAMA chunks.
        png_set_iCCP(png, info, "libavif", 0, avif->icc.data, (png_uint_32)avif->icc.size);
    } else {
        const avifBool isSrgb = (avif->colorPrimaries == AVIF_COLOR_PRIMARIES_SRGB) &&
                                (avif->transferCharacteristics == AVIF_TRANSFER_CHARACTERISTICS_SRGB);
        if (isSrgb) {
            png_set_sRGB_gAMA_and_cHRM(png, info, PNG_sRGB_INTENT_PERCEPTUAL);
        } else {
            if (avif->colorPrimaries != AVIF_COLOR_PRIMARIES_UNKNOWN && avif->colorPrimaries != AVIF_COLOR_PRIMARIES_UNSPECIFIED) {
                float primariesCoords[8];
                avifColorPrimariesGetValues(avif->colorPrimaries, primariesCoords);
                png_set_cHRM(png,
                             info,
                             primariesCoords[6],
                             primariesCoords[7],
                             primariesCoords[0],
                             primariesCoords[1],
                             primariesCoords[2],
                             primariesCoords[3],
                             primariesCoords[4],
                             primariesCoords[5]);
            }
            float gamma;
            // Write the transfer characteristics IF it can be represented as a
            // simple gamma value. Most transfer characteristics cannot be
            // represented this way. Viewers that support the cICP chunk can use
            // that instead, but older viewers might show incorrect colors.
            if (avifTransferCharacteristicsGetGamma(avif->transferCharacteristics, &gamma) == AVIF_RESULT_OK) {
                png_set_gAMA(png, info, 1.0f / gamma);
            }
        }
    }

    png_text texts[2];
    int numTextMetadataChunks = 0;
    if (avif->exif.data && (avif->exif.size > 0)) {
        if (avif->exif.size > UINT32_MAX) {
            fprintf(stderr, "Error writing PNG: Exif metadata is too big\n");
            goto cleanup;
        }
        png_set_eXIf_1(png, info, (png_uint_32)avif->exif.size, avif->exif.data);
    }
    if (avif->xmp.data && (avif->xmp.size > 0)) {
        // The iTXt XMP payload may not contain a zero byte according to section 4.2.3.3 of
        // the PNG specification, version 1.2.
        // The chunk is given to libpng as is. Bytes after a zero byte may be stripped.

        // Providing the length through png_text.itxt_length does not work.
        // The given png_text.text string must end with a zero byte.
        if (avif->xmp.size >= SIZE_MAX) {
            fprintf(stderr, "Error writing PNG: XMP metadata is too big\n");
            goto cleanup;
        }
        if (avifRWDataRealloc(&xmp, avif->xmp.size + 1) != AVIF_RESULT_OK) {
            fprintf(stderr, "Error writing PNG: out of memory\n");
            goto cleanup;
        }
        memcpy(xmp.data, avif->xmp.data, avif->xmp.size);
        xmp.data[avif->xmp.size] = '\0';
        png_text * text = &texts[numTextMetadataChunks++];
        memset(text, 0, sizeof(*text));
        text->compression = PNG_ITXT_COMPRESSION_NONE;
        text->key = "XML:com.adobe.xmp";
        text->text = (char *)xmp.data;
        text->itxt_length = xmp.size;
    }
    if (numTextMetadataChunks != 0) {
        png_set_text(png, info, texts, numTextMetadataChunks);
    }

    png_write_info(png, info);

    // Custom chunk writing, must appear after png_write_info.
    // With AVIF, an ICC profile takes priority over CICP, but with PNG files, CICP takes priority over ICC.
    // Therefore CICP should only be written if there is no ICC profile.
    if (!hasIcc) {
        const png_byte cicp[5] = "cICP";
        const png_byte cicpData[4] = { (png_byte)avif->colorPrimaries,
                                       (png_byte)avif->transferCharacteristics,
                                       AVIF_MATRIX_COEFFICIENTS_IDENTITY,
                                       1 /*full range*/ };
        png_write_chunk(png, cicp, cicpData, 4);
    }

    rowPointers = (png_bytep *)malloc(sizeof(png_bytep) * avif->height);
    if (rowPointers == NULL) {
        fprintf(stderr, "Error writing PNG: memory allocation failure");
        goto cleanup;
    }
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
    if (avifImageGetExifOrientationFromIrotImir(avif) != 1) {
        // TODO(yguyon): Rotate the samples.
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
    avifRWDataFree(&xmp);
    if (rowPointers) {
        free(rowPointers);
    }
    avifRGBImageFreePixels(&rgb);
    return writeResult;
}
