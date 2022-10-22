// Copyright 2020 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avifjpeg.h"
#include "avifutil.h"

#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jpeglib.h"

#include "iccjpeg.h"

#define AVIF_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define AVIF_MAX(a, b) (((a) > (b)) ? (a) : (b))

struct my_error_mgr
{
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};
typedef struct my_error_mgr * my_error_ptr;
static void my_error_exit(j_common_ptr cinfo)
{
    my_error_ptr myerr = (my_error_ptr)cinfo->err;
    (*cinfo->err->output_message)(cinfo);
    longjmp(myerr->setjmp_buffer, 1);
}

#if JPEG_LIB_VERSION >= 70
#define AVIF_LIBJPEG_DCT_v_scaled_size DCT_v_scaled_size
#define AVIF_LIBJPEG_DCT_h_scaled_size DCT_h_scaled_size
#else
#define AVIF_LIBJPEG_DCT_h_scaled_size DCT_scaled_size
#define AVIF_LIBJPEG_DCT_v_scaled_size DCT_scaled_size
#endif

// An internal function used by avifJPEGReadCopy(), this is the shared libjpeg decompression code
// for all paths avifJPEGReadCopy() takes.
static avifBool avifJPEGCopyPixels(avifImage * avif, struct jpeg_decompress_struct * cinfo)
{
    cinfo->raw_data_out = TRUE;
    jpeg_start_decompress(cinfo);

    avif->width = cinfo->image_width;
    avif->height = cinfo->image_height;

    JSAMPIMAGE buffer = (*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_IMAGE, sizeof(JSAMPARRAY) * cinfo->num_components);

    // lines of output image to be read per jpeg_read_raw_data call
    int readLines = 0;
    // lines of samples to be read per call (for each channel)
    int linesPerCall[3] = { 0, 0, 0 };
    // expected count of sample lines (for each channel)
    int targetRead[3] = { 0, 0, 0 };
    for (int i = 0; i < cinfo->num_components; ++i) {
        jpeg_component_info * comp = &cinfo->comp_info[i];

        linesPerCall[i] = comp->v_samp_factor * comp->AVIF_LIBJPEG_DCT_v_scaled_size;
        targetRead[i] = comp->downsampled_height;
        buffer[i] = (*cinfo->mem->alloc_sarray)((j_common_ptr)cinfo,
                                                JPOOL_IMAGE,
                                                comp->width_in_blocks * comp->AVIF_LIBJPEG_DCT_h_scaled_size,
                                                linesPerCall[i]);
        readLines = AVIF_MAX(readLines, linesPerCall[i]);
    }

    if (avifImageAllocatePlanes(avif, AVIF_PLANES_YUV) != AVIF_RESULT_OK) {
        return AVIF_FALSE;
    }

    // destination avif channel for each jpeg channel
    avifChannelIndex targetChannel[3] = { AVIF_CHAN_Y, AVIF_CHAN_Y, AVIF_CHAN_Y };
    if (cinfo->jpeg_color_space == JCS_YCbCr) {
        targetChannel[0] = AVIF_CHAN_Y;
        targetChannel[1] = AVIF_CHAN_U;
        targetChannel[2] = AVIF_CHAN_V;
    } else if (cinfo->jpeg_color_space == JCS_GRAYSCALE) {
        targetChannel[0] = AVIF_CHAN_Y;
    } else {
        // cinfo->jpeg_color_space == JCS_RGB
        targetChannel[0] = AVIF_CHAN_V;
        targetChannel[1] = AVIF_CHAN_Y;
        targetChannel[2] = AVIF_CHAN_U;
    }

    int workComponents = avif->yuvFormat == AVIF_PIXEL_FORMAT_YUV400 ? 1 : cinfo->num_components;

    // count of already-read lines (for each channel)
    int alreadyRead[3] = { 0, 0, 0 };
    while (cinfo->output_scanline < cinfo->output_height) {
        jpeg_read_raw_data(cinfo, buffer, readLines);

        for (int i = 0; i < workComponents; ++i) {
            int linesRead = AVIF_MIN(targetRead[i] - alreadyRead[i], linesPerCall[i]);
            for (int j = 0; j < linesRead; ++j) {
                memcpy(&avif->yuvPlanes[targetChannel[i]][avif->yuvRowBytes[targetChannel[i]] * (alreadyRead[i] + j)],
                       buffer[i][j],
                       avif->yuvRowBytes[targetChannel[i]]);
            }
            alreadyRead[i] += linesPerCall[i];
        }
    }
    return AVIF_TRUE;
}

static avifBool avifJPEGHasCompatibleMatrixCoefficients(avifMatrixCoefficients matrixCoefficients)
{
    switch (matrixCoefficients) {
        case AVIF_MATRIX_COEFFICIENTS_BT470BG:
        case AVIF_MATRIX_COEFFICIENTS_BT601:
            // JPEG always uses [Kr:0.299, Kb:0.114], which matches these MCs.
            return AVIF_TRUE;
    }
    return AVIF_FALSE;
}

// This attempts to copy the internal representation of the JPEG directly into avifImage without
// YUV->RGB conversion. If it returns AVIF_FALSE, a typical RGB->YUV conversion is required.
static avifBool avifJPEGReadCopy(avifImage * avif, struct jpeg_decompress_struct * cinfo)
{
    if ((avif->depth != 8) || (avif->yuvRange != AVIF_RANGE_FULL)) {
        return AVIF_FALSE;
    }

    if (cinfo->jpeg_color_space == JCS_YCbCr) {
        // Import from YUV: must use compatible matrixCoefficients.
        if (avifJPEGHasCompatibleMatrixCoefficients(avif->matrixCoefficients)) {
            // YUV->YUV: require precise match for pixel format.
            avifPixelFormat jpegFormat = AVIF_PIXEL_FORMAT_NONE;
            if (cinfo->comp_info[0].h_samp_factor == 1 && cinfo->comp_info[0].v_samp_factor == 1 &&
                cinfo->comp_info[1].h_samp_factor == 1 && cinfo->comp_info[1].v_samp_factor == 1 &&
                cinfo->comp_info[2].h_samp_factor == 1 && cinfo->comp_info[2].v_samp_factor == 1) {
                jpegFormat = AVIF_PIXEL_FORMAT_YUV444;
            } else if (cinfo->comp_info[0].h_samp_factor == 2 && cinfo->comp_info[0].v_samp_factor == 1 &&
                       cinfo->comp_info[1].h_samp_factor == 1 && cinfo->comp_info[1].v_samp_factor == 1 &&
                       cinfo->comp_info[2].h_samp_factor == 1 && cinfo->comp_info[2].v_samp_factor == 1) {
                jpegFormat = AVIF_PIXEL_FORMAT_YUV422;
            } else if (cinfo->comp_info[0].h_samp_factor == 2 && cinfo->comp_info[0].v_samp_factor == 2 &&
                       cinfo->comp_info[1].h_samp_factor == 1 && cinfo->comp_info[1].v_samp_factor == 1 &&
                       cinfo->comp_info[2].h_samp_factor == 1 && cinfo->comp_info[2].v_samp_factor == 1) {
                jpegFormat = AVIF_PIXEL_FORMAT_YUV420;
            }
            if (jpegFormat != AVIF_PIXEL_FORMAT_NONE) {
                if (avif->yuvFormat == AVIF_PIXEL_FORMAT_NONE) {
                    // The requested format is "auto": Adopt JPEG's internal format.
                    avif->yuvFormat = jpegFormat;
                }
                if (avif->yuvFormat == jpegFormat) {
                    cinfo->out_color_space = JCS_YCbCr;
                    return avifJPEGCopyPixels(avif, cinfo);
                }
            }

            // YUV->Grayscale: subsample Y plane not allowed.
            if ((avif->yuvFormat == AVIF_PIXEL_FORMAT_YUV400) && (cinfo->comp_info[0].h_samp_factor == cinfo->max_h_samp_factor &&
                                                                  cinfo->comp_info[0].v_samp_factor == cinfo->max_v_samp_factor)) {
                cinfo->out_color_space = JCS_YCbCr;
                return avifJPEGCopyPixels(avif, cinfo);
            }
        }
    } else if (cinfo->jpeg_color_space == JCS_GRAYSCALE) {
        // Import from Grayscale: subsample not allowed.
        if ((cinfo->comp_info[0].h_samp_factor == cinfo->max_h_samp_factor &&
             cinfo->comp_info[0].v_samp_factor == cinfo->max_v_samp_factor)) {
            // Import to YUV/Grayscale: must use compatible matrixCoefficients.
            if (avifJPEGHasCompatibleMatrixCoefficients(avif->matrixCoefficients)) {
                // Grayscale->Grayscale: direct copy.
                if ((avif->yuvFormat == AVIF_PIXEL_FORMAT_YUV400) || (avif->yuvFormat == AVIF_PIXEL_FORMAT_NONE)) {
                    avif->yuvFormat = AVIF_PIXEL_FORMAT_YUV400;
                    cinfo->out_color_space = JCS_GRAYSCALE;
                    return avifJPEGCopyPixels(avif, cinfo);
                }

                // Grayscale->YUV: copy Y, fill UV with monochrome value.
                if ((avif->yuvFormat == AVIF_PIXEL_FORMAT_YUV444) || (avif->yuvFormat == AVIF_PIXEL_FORMAT_YUV422) ||
                    (avif->yuvFormat == AVIF_PIXEL_FORMAT_YUV420)) {
                    cinfo->out_color_space = JCS_GRAYSCALE;
                    if (!avifJPEGCopyPixels(avif, cinfo)) {
                        return AVIF_FALSE;
                    }

                    avifPixelFormatInfo info;
                    avifGetPixelFormatInfo(avif->yuvFormat, &info);
                    uint32_t uvHeight = (avif->height + info.chromaShiftY) >> info.chromaShiftY;
                    memset(avif->yuvPlanes[AVIF_CHAN_U], 128, avif->yuvRowBytes[AVIF_CHAN_U] * uvHeight);
                    memset(avif->yuvPlanes[AVIF_CHAN_V], 128, avif->yuvRowBytes[AVIF_CHAN_V] * uvHeight);

                    return AVIF_TRUE;
                }
            }

            // Grayscale->RGB: copy Y to G, duplicate to B and R.
            if ((avif->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY) &&
                ((avif->yuvFormat == AVIF_PIXEL_FORMAT_YUV444) || (avif->yuvFormat == AVIF_PIXEL_FORMAT_NONE))) {
                avif->yuvFormat = AVIF_PIXEL_FORMAT_YUV444;
                cinfo->out_color_space = JCS_GRAYSCALE;
                if (!avifJPEGCopyPixels(avif, cinfo)) {
                    return AVIF_FALSE;
                }

                memcpy(avif->yuvPlanes[AVIF_CHAN_U], avif->yuvPlanes[AVIF_CHAN_Y], (size_t)avif->yuvRowBytes[AVIF_CHAN_U] * avif->height);
                memcpy(avif->yuvPlanes[AVIF_CHAN_V], avif->yuvPlanes[AVIF_CHAN_Y], (size_t)avif->yuvRowBytes[AVIF_CHAN_V] * avif->height);

                return AVIF_TRUE;
            }
        }
    } else if (cinfo->jpeg_color_space == JCS_RGB) {
        // RGB->RGB: subsample not allowed.
        if ((avif->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY) &&
            ((avif->yuvFormat == AVIF_PIXEL_FORMAT_YUV444) || (avif->yuvFormat == AVIF_PIXEL_FORMAT_NONE)) &&
            (cinfo->comp_info[0].h_samp_factor == 1 && cinfo->comp_info[0].v_samp_factor == 1 &&
             cinfo->comp_info[1].h_samp_factor == 1 && cinfo->comp_info[1].v_samp_factor == 1 &&
             cinfo->comp_info[2].h_samp_factor == 1 && cinfo->comp_info[2].v_samp_factor == 1)) {
            avif->yuvFormat = AVIF_PIXEL_FORMAT_YUV444;
            cinfo->out_color_space = JCS_RGB;
            return avifJPEGCopyPixels(avif, cinfo);
        }
    }

    // A typical RGB->YUV conversion is required.
    return AVIF_FALSE;
}

// Reads 4-byte unsigned integer in big-endian format from the raw bitstream src.
static uint32_t avifJPEGReadUint32BigEndian(const uint8_t * src)
{
    return ((uint32_t)src[0] << 24) | ((uint32_t)src[1] << 16) | ((uint32_t)src[2] << 8) | ((uint32_t)src[3] << 0);
}

// Returns the pointer in str to the first occurrence of substr. Returns NULL if substr cannot be found in str.
static const uint8_t * avifJPEGFindSubstr(const uint8_t * str, size_t strLength, const uint8_t * substr, size_t substrLength)
{
    for (size_t index = 0; index + substrLength <= strLength; ++index) {
        if (!memcmp(&str[index], substr, substrLength)) {
            return &str[index];
        }
    }
    return NULL;
}

// XMP tags
#define AVIF_STANDARD_XMP_TAG "http://ns.adobe.com/xap/1.0/\0"
#define AVIF_STANDARD_XMP_TAG_LENGTH 29
#define AVIF_EXTENDED_XMP_TAG "http://ns.adobe.com/xmp/extension/\0"
#define AVIF_EXTENDED_XMP_TAG_LENGTH 35

// One way of storing the Extended XMP GUID (generated by a camera for example).
#define AVIF_XMP_NOTE_TAG "xmpNote:HasExtendedXMP=\""
#define AVIF_XMP_NOTE_TAG_LENGTH 24
// Another way of storing the Extended XMP GUID (generated by exiftool for example).
#define AVIF_ALTERNATIVE_XMP_NOTE_TAG "<xmpNote:HasExtendedXMP>"
#define AVIF_ALTERNATIVE_XMP_NOTE_TAG_LENGTH 24

#define AVIF_EXTENDED_XMP_GUID_LENGTH 32

// Offset in APP1 segment (skip tag + guid + size + offset).
#define AVIF_OFFSET_TILL_EXTENDED_XMP (AVIF_EXTENDED_XMP_TAG_LENGTH + AVIF_EXTENDED_XMP_GUID_LENGTH + 4 + 4)

// Note on setjmp() and volatile variables:
//
// K & R, The C Programming Language 2nd Ed, p. 254 says:
//   ... Accessible objects have the values they had when longjmp was called,
//   except that non-volatile automatic variables in the function calling setjmp
//   become undefined if they were changed after the setjmp call.
//
// Therefore, 'iccData' is declared as volatile. 'rgb' should be declared as
// volatile, but doing so would be inconvenient (try it) and since it is a
// struct, the compiler is unlikely to put it in a register. 'ret' does not need
// to be declared as volatile because it is not modified between setjmp and
// longjmp. But GCC's -Wclobbered warning may have trouble figuring that out, so
// we preemptively declare it as volatile.

avifBool avifJPEGRead(const char * inputFilename,
                      avifImage * avif,
                      avifPixelFormat requestedFormat,
                      uint32_t requestedDepth,
                      avifChromaDownsampling chromaDownsampling,
                      avifBool ignoreICC,
                      avifBool ignoreExif,
                      avifBool ignoreXMP)
{
    volatile avifBool ret = AVIF_FALSE;
    uint8_t * volatile iccData = NULL;

    avifRGBImage rgb;
    memset(&rgb, 0, sizeof(avifRGBImage));

    // Standard XMP segment followed by all extended XMP segments.
    avifRWData totalXMP = { NULL, 0 };
    // Each byte set to 0 is a missing byte. Each byte set to 1 was read and copied to totalXMP.
    avifRWData extendedXMPReadBytes = { NULL, 0 };

    FILE * f = fopen(inputFilename, "rb");
    if (!f) {
        fprintf(stderr, "Can't open JPEG file for read: %s\n", inputFilename);
        return ret;
    }

    struct my_error_mgr jerr;
    struct jpeg_decompress_struct cinfo;
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;
    if (setjmp(jerr.setjmp_buffer)) {
        goto cleanup;
    }

    jpeg_create_decompress(&cinfo);

    if (!ignoreExif || !ignoreXMP) {
        jpeg_save_markers(&cinfo, JPEG_APP0 + 1, /*length_limit=*/0xFFFF); // Exif/XMP
    }
    if (!ignoreICC) {
        setup_read_icc_profile(&cinfo);
    }
    jpeg_stdio_src(&cinfo, f);
    jpeg_read_header(&cinfo, TRUE);

    if (!ignoreICC) {
        uint8_t * iccDataTmp;
        unsigned int iccDataLen;
        if (read_icc_profile(&cinfo, &iccDataTmp, &iccDataLen)) {
            iccData = iccDataTmp;
            avifImageSetProfileICC(avif, iccDataTmp, (size_t)iccDataLen);
        }
    }

    avif->yuvFormat = requestedFormat; // This may be AVIF_PIXEL_FORMAT_NONE, which is "auto" to avifJPEGReadCopy()
    avif->depth = requestedDepth ? requestedDepth : 8;
    // JPEG doesn't have alpha. Prevent confusion.
    avif->alphaPremultiplied = AVIF_FALSE;

    if (avifJPEGReadCopy(avif, &cinfo)) {
        // JPEG pixels were successfully copied without conversion. Notify the enduser.

        assert(inputFilename); // JPEG read doesn't support stdin
        printf("Directly copied JPEG pixel data (no YUV conversion): %s\n", inputFilename);
    } else {
        // JPEG pixels could not be copied without conversion. Request (converted) RGB pixels from
        // libjpeg and convert to YUV with libavif instead.

        cinfo.out_color_space = JCS_RGB;
        jpeg_start_decompress(&cinfo);

        int row_stride = cinfo.output_width * cinfo.output_components;
        JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

        avif->width = cinfo.output_width;
        avif->height = cinfo.output_height;
        if (avif->yuvFormat == AVIF_PIXEL_FORMAT_NONE) {
            // Identity is only valid with YUV444.
            avif->yuvFormat = (avif->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY) ? AVIF_PIXEL_FORMAT_YUV444
                                                                                              : AVIF_APP_DEFAULT_PIXEL_FORMAT;
        }
        avif->depth = requestedDepth ? requestedDepth : 8;
        avifRGBImageSetDefaults(&rgb, avif);
        rgb.format = AVIF_RGB_FORMAT_RGB;
        rgb.chromaDownsampling = chromaDownsampling;
        rgb.depth = 8;
        avifRGBImageAllocatePixels(&rgb);

        int row = 0;
        while (cinfo.output_scanline < cinfo.output_height) {
            jpeg_read_scanlines(&cinfo, buffer, 1);
            uint8_t * pixelRow = &rgb.pixels[row * rgb.rowBytes];
            memcpy(pixelRow, buffer[0], rgb.rowBytes);
            ++row;
        }
        if (avifImageRGBToYUV(avif, &rgb) != AVIF_RESULT_OK) {
            fprintf(stderr, "Conversion to YUV failed: %s\n", inputFilename);
            goto cleanup;
        }
    }

    if (!ignoreExif) {
        const avifROData tagExif = { (const uint8_t *)"Exif\0\0", 6 };
        avifBool found = AVIF_FALSE;
        for (jpeg_saved_marker_ptr marker = cinfo.marker_list; marker != NULL; marker = marker->next) {
            if ((marker->marker == (JPEG_APP0 + 1)) && (marker->data_length > tagExif.size) &&
                !memcmp(marker->data, tagExif.data, tagExif.size)) {
                if (found) {
                    // TODO(yguyon): Implement instead of outputting an error.
                    fprintf(stderr, "Exif extraction failed: unsupported Exif split into multiple segments or invalid multiple Exif segments\n");
                    goto cleanup;
                }
                avifImageSetMetadataExif(avif, marker->data + tagExif.size, marker->data_length - tagExif.size);
                found = AVIF_TRUE;
            }
        }
    }
    if (!ignoreXMP) {
        const uint8_t * standardXMPData = NULL;
        uint32_t standardXMPSize = 0; // At most 64kB as defined by Adobe XMP Specification Part 3.
        for (jpeg_saved_marker_ptr marker = cinfo.marker_list; marker != NULL; marker = marker->next) {
            if ((marker->marker == (JPEG_APP0 + 1)) && (marker->data_length > AVIF_STANDARD_XMP_TAG_LENGTH) &&
                !memcmp(marker->data, AVIF_STANDARD_XMP_TAG, AVIF_STANDARD_XMP_TAG_LENGTH)) {
                if (standardXMPData) {
                    fprintf(stderr, "XMP extraction failed: invalid multiple standard XMP segments\n");
                    goto cleanup;
                }
                standardXMPData = marker->data + AVIF_STANDARD_XMP_TAG_LENGTH;
                standardXMPSize = (uint32_t)(marker->data_length - AVIF_STANDARD_XMP_TAG_LENGTH);
            }
        }

        avifBool foundExtendedXMP = AVIF_FALSE;
        uint8_t extendedXMPGUID[AVIF_EXTENDED_XMP_GUID_LENGTH]; // The value is common to all extended XMP segments.
        for (jpeg_saved_marker_ptr marker = cinfo.marker_list; marker != NULL; marker = marker->next) {
            if ((marker->marker == (JPEG_APP0 + 1)) && (marker->data_length > AVIF_EXTENDED_XMP_TAG_LENGTH) &&
                !memcmp(marker->data, AVIF_EXTENDED_XMP_TAG, AVIF_EXTENDED_XMP_TAG_LENGTH)) {
                if (!standardXMPData) {
                    fprintf(stderr, "XMP extraction failed: extended XMP segment found, missing standard XMP segment\n");
                    goto cleanup;
                }

                if (marker->data_length < AVIF_OFFSET_TILL_EXTENDED_XMP) {
                    fprintf(stderr, "XMP extraction failed: truncated extended XMP segment\n");
                    goto cleanup;
                }
                const uint8_t * guid = &marker->data[AVIF_EXTENDED_XMP_TAG_LENGTH];
                for (size_t c = 0; c < AVIF_EXTENDED_XMP_GUID_LENGTH; ++c) {
                    // According to Adobe XMP Specification Part 3 section 1.1.3.1:
                    //   "128-bit GUID stored as a 32-byte ASCII hex string, capital A-F, no null termination"
                    if (((guid[c] < '0') || (guid[c] > '9')) && ((guid[c] < 'A') || (guid[c] > 'F'))) {
                        fprintf(stderr, "XMP extraction failed: invalid XMP segment GUID\n");
                        goto cleanup;
                    }
                }
                // Size of the current extended segment.
                const size_t extendedXMPSize = marker->data_length - AVIF_OFFSET_TILL_EXTENDED_XMP;
                // Expected size of the sum of all extended segments.
                // According to Adobe XMP Specification Part 3 section 1.1.3.1:
                //   "full length of the ExtendedXMP serialization as a 32-bit unsigned integer"
                const uint32_t totalExtendedXMPSize =
                    avifJPEGReadUint32BigEndian(&marker->data[AVIF_EXTENDED_XMP_TAG_LENGTH + AVIF_EXTENDED_XMP_GUID_LENGTH]);
                // Offset in totalXMP after standardXMP.
                // According to Adobe XMP Specification Part 3 section 1.1.3.1:
                //   "offset of this portion as a 32-bit unsigned integer"
                const uint32_t extendedXMPOffset =
                    avifJPEGReadUint32BigEndian(&marker->data[AVIF_EXTENDED_XMP_TAG_LENGTH + AVIF_EXTENDED_XMP_GUID_LENGTH + 4]);
                if (((uint64_t)standardXMPSize + totalExtendedXMPSize) > SIZE_MAX) {
                    fprintf(stderr, "XMP extraction failed: total XMP size is too large\n");
                    goto cleanup;
                }
                if ((extendedXMPSize == 0) || (((uint64_t)extendedXMPOffset + extendedXMPSize) > totalExtendedXMPSize)) {
                    fprintf(stderr, "XMP extraction failed: invalid extended XMP segment size or offset\n");
                    goto cleanup;
                }
                if (foundExtendedXMP) {
                    if (memcmp(guid, extendedXMPGUID, AVIF_EXTENDED_XMP_GUID_LENGTH)) {
                        fprintf(stderr, "XMP extraction failed: extended XMP segment GUID mismatch\n");
                        goto cleanup;
                    }
                    if (totalExtendedXMPSize != (totalXMP.size - standardXMPSize)) {
                        fprintf(stderr, "XMP extraction failed: extended XMP total size mismatch\n");
                        goto cleanup;
                    }
                } else {
                    memcpy(extendedXMPGUID, guid, AVIF_EXTENDED_XMP_GUID_LENGTH);

                    avifRWDataRealloc(&totalXMP, (size_t)standardXMPSize + totalExtendedXMPSize);
                    memcpy(totalXMP.data, standardXMPData, standardXMPSize);

                    // Keep track of the bytes that were set.
                    avifRWDataRealloc(&extendedXMPReadBytes, totalExtendedXMPSize);
                    memset(extendedXMPReadBytes.data, 0, extendedXMPReadBytes.size);

                    foundExtendedXMP = AVIF_TRUE;
                }
                // According to Adobe XMP Specification Part 3 section 1.1.3.1:
                //   "A robust JPEG reader should tolerate the marker segments in any order."
                memcpy(&totalXMP.data[standardXMPSize + extendedXMPOffset], &marker->data[AVIF_OFFSET_TILL_EXTENDED_XMP], extendedXMPSize);

                // Make sure no previously read data was overwritten by the current segment.
                if (memchr(&extendedXMPReadBytes.data[extendedXMPOffset], 1, extendedXMPSize)) {
                    fprintf(stderr, "XMP extraction failed: overlapping extended XMP segments\n");
                    goto cleanup;
                }
                // Keep track of the bytes that were set.
                memset(&extendedXMPReadBytes.data[extendedXMPOffset], 1, extendedXMPSize);
            }
        }

        if (foundExtendedXMP) {
            // Make sure there is no missing byte.
            if (memchr(extendedXMPReadBytes.data, 0, extendedXMPReadBytes.size)) {
                fprintf(stderr, "XMP extraction failed: missing extended XMP segments\n");
                goto cleanup;
            }

            // According to Adobe XMP Specification Part 3 section 1.1.3.1:
            //   "A reader must incorporate only ExtendedXMP blocks whose GUID matches the value of xmpNote:HasExtendedXMP."
            uint8_t xmpNote[AVIF_XMP_NOTE_TAG_LENGTH + AVIF_EXTENDED_XMP_GUID_LENGTH];
            memcpy(xmpNote, AVIF_XMP_NOTE_TAG, AVIF_XMP_NOTE_TAG_LENGTH);
            memcpy(xmpNote + AVIF_XMP_NOTE_TAG_LENGTH, extendedXMPGUID, AVIF_EXTENDED_XMP_GUID_LENGTH);
            if (!avifJPEGFindSubstr(standardXMPData, standardXMPSize, xmpNote, sizeof(xmpNote))) {
                // Try the alternative before returning an error.
                uint8_t alternativeXmpNote[AVIF_ALTERNATIVE_XMP_NOTE_TAG_LENGTH + AVIF_EXTENDED_XMP_GUID_LENGTH];
                memcpy(alternativeXmpNote, AVIF_ALTERNATIVE_XMP_NOTE_TAG, AVIF_ALTERNATIVE_XMP_NOTE_TAG_LENGTH);
                memcpy(alternativeXmpNote + AVIF_ALTERNATIVE_XMP_NOTE_TAG_LENGTH, extendedXMPGUID, AVIF_EXTENDED_XMP_GUID_LENGTH);
                if (!avifJPEGFindSubstr(standardXMPData, standardXMPSize, alternativeXmpNote, sizeof(alternativeXmpNote))) {
                    fprintf(stderr, "XMP extraction failed: standard and extended XMP GUID mismatch\n");
                    goto cleanup;
                }
            }

            // According to Adobe XMP Specification Part 3 section 1.1.3.1:
            //   "A JPEG reader must [...] remove the xmpNote:HasExtendedXMP property."
            // This constraint is ignored here because leaving the xmpNote:HasExtendedXMP property is rather harmless
            // and editing XMP metadata is quite involved.

            avifRWDataFree(&avif->xmp);
            avif->xmp = totalXMP;
            totalXMP.data = NULL;
            totalXMP.size = 0;
        } else if (standardXMPData) {
            avifImageSetMetadataXMP(avif, standardXMPData, standardXMPSize);
        }
    }
    jpeg_finish_decompress(&cinfo);
    ret = AVIF_TRUE;
cleanup:
    jpeg_destroy_decompress(&cinfo);
    fclose(f);
    free(iccData);
    avifRGBImageFreePixels(&rgb);
    avifRWDataFree(&totalXMP);
    avifRWDataFree(&extendedXMPReadBytes);
    return ret;
}

avifBool avifJPEGWrite(const char * outputFilename, const avifImage * avif, int jpegQuality, avifChromaUpsampling chromaUpsampling)
{
    avifBool ret = AVIF_FALSE;
    FILE * f = NULL;

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    avifRGBImage rgb;
    avifRGBImageSetDefaults(&rgb, avif);
    rgb.format = AVIF_RGB_FORMAT_RGB;
    rgb.chromaUpsampling = chromaUpsampling;
    rgb.depth = 8;
    avifRGBImageAllocatePixels(&rgb);
    if (avifImageYUVToRGB(avif, &rgb) != AVIF_RESULT_OK) {
        fprintf(stderr, "Conversion to RGB failed: %s\n", outputFilename);
        goto cleanup;
    }

    f = fopen(outputFilename, "wb");
    if (!f) {
        fprintf(stderr, "Can't open JPEG file for write: %s\n", outputFilename);
        goto cleanup;
    }

    jpeg_stdio_dest(&cinfo, f);
    cinfo.image_width = avif->width;
    cinfo.image_height = avif->height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, jpegQuality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    if (avif->icc.data && (avif->icc.size > 0)) {
        write_icc_profile(&cinfo, avif->icc.data, (unsigned int)avif->icc.size);
    }

    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &rgb.pixels[cinfo.next_scanline * rgb.rowBytes];
        (void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    ret = AVIF_TRUE;
    printf("Wrote JPEG: %s\n", outputFilename);
cleanup:
    if (f) {
        fclose(f);
    }
    jpeg_destroy_compress(&cinfo);
    avifRGBImageFreePixels(&rgb);
    return ret;
}
