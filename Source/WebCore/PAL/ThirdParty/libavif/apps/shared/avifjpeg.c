// Copyright 2020 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avifjpeg.h"
#include "avifexif.h"
#include "avifutil.h"

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jpeglib.h"

#include "iccjpeg.h"

#if defined(AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION)
#include <libxml/parser.h>
#endif

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
static avifBool avifJPEGCopyPixels(avifImage * avif, uint32_t imageSizeLimit, struct jpeg_decompress_struct * cinfo)
{
    cinfo->raw_data_out = TRUE;
    jpeg_start_decompress(cinfo);

    avif->width = cinfo->image_width;
    avif->height = cinfo->image_height;
    if ((uint32_t)avif->width > imageSizeLimit / (uint32_t)avif->height) {
        return AVIF_FALSE;
    }

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

    avifImageFreePlanes(avif, AVIF_PLANES_ALL); // Free planes in case they were already allocated.
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
static avifBool avifJPEGReadCopy(avifImage * avif, uint32_t imageSizeLimit, struct jpeg_decompress_struct * cinfo)
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
                    return avifJPEGCopyPixels(avif, imageSizeLimit, cinfo);
                }
            }

            // YUV->Grayscale: subsample Y plane not allowed.
            if ((avif->yuvFormat == AVIF_PIXEL_FORMAT_YUV400) && (cinfo->comp_info[0].h_samp_factor == cinfo->max_h_samp_factor &&
                                                                  cinfo->comp_info[0].v_samp_factor == cinfo->max_v_samp_factor)) {
                cinfo->out_color_space = JCS_YCbCr;
                return avifJPEGCopyPixels(avif, imageSizeLimit, cinfo);
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
                    return avifJPEGCopyPixels(avif, imageSizeLimit, cinfo);
                }

                // Grayscale->YUV: copy Y, fill UV with monochrome value.
                if ((avif->yuvFormat == AVIF_PIXEL_FORMAT_YUV444) || (avif->yuvFormat == AVIF_PIXEL_FORMAT_YUV422) ||
                    (avif->yuvFormat == AVIF_PIXEL_FORMAT_YUV420)) {
                    cinfo->out_color_space = JCS_GRAYSCALE;
                    if (!avifJPEGCopyPixels(avif, imageSizeLimit, cinfo)) {
                        return AVIF_FALSE;
                    }

                    uint32_t uvHeight = avifImagePlaneHeight(avif, AVIF_CHAN_U);
                    memset(avif->yuvPlanes[AVIF_CHAN_U], 128, (size_t)avif->yuvRowBytes[AVIF_CHAN_U] * uvHeight);
                    memset(avif->yuvPlanes[AVIF_CHAN_V], 128, (size_t)avif->yuvRowBytes[AVIF_CHAN_V] * uvHeight);

                    return AVIF_TRUE;
                }
            }

            // Grayscale->RGB: copy Y to G, duplicate to B and R.
            if ((avif->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY) &&
                ((avif->yuvFormat == AVIF_PIXEL_FORMAT_YUV444) || (avif->yuvFormat == AVIF_PIXEL_FORMAT_NONE))) {
                avif->yuvFormat = AVIF_PIXEL_FORMAT_YUV444;
                cinfo->out_color_space = JCS_GRAYSCALE;
                if (!avifJPEGCopyPixels(avif, imageSizeLimit, cinfo)) {
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
            return avifJPEGCopyPixels(avif, imageSizeLimit, cinfo);
        }
    }

    // A typical RGB->YUV conversion is required.
    return AVIF_FALSE;
}

// Reads a 4-byte unsigned integer in big-endian format from the raw bitstream src.
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

#define AVIF_JPEG_MAX_MARKER_DATA_LENGTH 65533

// Exif tag
#define AVIF_JPEG_EXIF_HEADER "Exif\0\0"
#define AVIF_JPEG_EXIF_HEADER_LENGTH 6

// XMP tags
#define AVIF_JPEG_STANDARD_XMP_TAG "http://ns.adobe.com/xap/1.0/\0"
#define AVIF_JPEG_STANDARD_XMP_TAG_LENGTH 29
#define AVIF_JPEG_EXTENDED_XMP_TAG "http://ns.adobe.com/xmp/extension/\0"
#define AVIF_JPEG_EXTENDED_XMP_TAG_LENGTH 35

// MPF tag (Multi-Picture Format)
#define AVIF_JPEG_MPF_HEADER "MPF\0"
#define AVIF_JPEG_MPF_HEADER_LENGTH 4

// One way of storing the Extended XMP GUID (generated by a camera for example).
#define AVIF_JPEG_XMP_NOTE_TAG "xmpNote:HasExtendedXMP=\""
#define AVIF_JPEG_XMP_NOTE_TAG_LENGTH 24
// Another way of storing the Extended XMP GUID (generated by exiftool for example).
#define AVIF_JPEG_ALTERNATIVE_XMP_NOTE_TAG "<xmpNote:HasExtendedXMP>"
#define AVIF_JPEG_ALTERNATIVE_XMP_NOTE_TAG_LENGTH 24

#define AVIF_JPEG_EXTENDED_XMP_GUID_LENGTH 32

// Offset in APP1 segment (skip tag + guid + size + offset).
#define AVIF_JPEG_OFFSET_TILL_EXTENDED_XMP (AVIF_JPEG_EXTENDED_XMP_TAG_LENGTH + AVIF_JPEG_EXTENDED_XMP_GUID_LENGTH + 4 + 4)

#define AVIF_CHECK(A)          \
    do {                       \
        if (!(A))              \
            return AVIF_FALSE; \
    } while (0)

#if defined(AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION)

// Reads a 4-byte unsigned integer in little-endian format from the raw bitstream src.
static uint32_t avifJPEGReadUint32LittleEndian(const uint8_t * src)
{
    return ((uint32_t)src[0] << 0) | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
}

// Reads a 2-byte unsigned integer in big-endian format from the raw bitstream src.
static uint16_t avifJPEGReadUint16BigEndian(const uint8_t * src)
{
    return ((uint32_t)src[0] << 8) | ((uint32_t)src[1] << 0);
}

// Reads a 2-byte unsigned integer in little-endian format from the raw bitstream src.
static uint16_t avifJPEGReadUint16LittleEndian(const uint8_t * src)
{
    return ((uint32_t)src[0] << 0) | ((uint32_t)src[1] << 8);
}

// Reads 'numBytes' at 'offset', stores them in 'bytes' and increases 'offset'.
static avifBool avifJPEGReadBytes(const avifROData * data, uint8_t * bytes, uint32_t * offset, uint32_t numBytes)
{
    if (data->size < (*offset + numBytes)) {
        return AVIF_FALSE;
    }
    memcpy(bytes, &data->data[*offset], numBytes);
    *offset += numBytes;
    return AVIF_TRUE;
}

static avifBool avifJPEGReadU32(const avifROData * data, uint32_t * v, uint32_t * offset, avifBool isBigEndian)
{
    uint8_t bytes[4];
    AVIF_CHECK(avifJPEGReadBytes(data, bytes, offset, 4));
    *v = isBigEndian ? avifJPEGReadUint32BigEndian(bytes) : avifJPEGReadUint32LittleEndian(bytes);
    return AVIF_TRUE;
}

static avifBool avifJPEGReadU16(const avifROData * data, uint16_t * v, uint32_t * offset, avifBool isBigEndian)
{
    uint8_t bytes[2];
    AVIF_CHECK(avifJPEGReadBytes(data, bytes, offset, 2));
    *v = isBigEndian ? avifJPEGReadUint16BigEndian(bytes) : avifJPEGReadUint16LittleEndian(bytes);
    return AVIF_TRUE;
}

static avifBool avifJPEGReadInternal(FILE * f,
                                     const char * inputFilename,
                                     avifImage * avif,
                                     avifPixelFormat requestedFormat,
                                     uint32_t requestedDepth,
                                     avifChromaDownsampling chromaDownsampling,
                                     avifBool ignoreColorProfile,
                                     avifBool ignoreExif,
                                     avifBool ignoreXMP,
                                     avifBool ignoreGainMap,
                                     uint32_t imageSizeLimit);

// Arbitrary max number of jpeg segments to parse before giving up.
#define MAX_JPEG_SEGMENTS 100

// Finds the offset of the first MPF segment. Returns AVIF_TRUE if it was found.
static avifBool avifJPEGFindMpfSegmentOffset(FILE * f, uint32_t * mpfOffset)
{
    const long oldOffset = ftell(f);

    uint32_t offset = 2; // Skip the 2 byte SOI (Start Of Image) marker.
    if (fseek(f, offset, SEEK_SET) != 0) {
        return AVIF_FALSE;
    }

    uint8_t buffer[4];
    int numSegments = 0;
    while (numSegments < MAX_JPEG_SEGMENTS) {
        ++numSegments;
        // Read the APP<n> segment marker (2 bytes) and the segment size (2 bytes).
        if (fread(buffer, 1, 4, f) != 4) {
            fseek(f, oldOffset, SEEK_SET);
            return AVIF_FALSE; // End of the file reached.
        }
        offset += 4;

        // Total APP<n> segment byte count, including the byte count value (2 bytes), but excluding the 2 byte APP<n> marker itself.
        const uint16_t segmentLength = avifJPEGReadUint16BigEndian(&buffer[2]);
        if (segmentLength < 2) {
            fseek(f, oldOffset, SEEK_SET);
            return AVIF_FALSE; // Invalid length.
        } else if (segmentLength < 2 + AVIF_JPEG_MPF_HEADER_LENGTH) {
            // Cannot be an MPF segment, skip to the next segment.
            offset += segmentLength - 2;
            if (fseek(f, offset, SEEK_SET) != 0) {
                fseek(f, oldOffset, SEEK_SET);
                return AVIF_FALSE;
            }
            continue;
        }

        uint8_t identifier[AVIF_JPEG_MPF_HEADER_LENGTH];
        if (fread(identifier, 1, AVIF_JPEG_MPF_HEADER_LENGTH, f) != AVIF_JPEG_MPF_HEADER_LENGTH) {
            fseek(f, oldOffset, SEEK_SET);
            return AVIF_FALSE; // End of the file reached.
        }
        offset += AVIF_JPEG_MPF_HEADER_LENGTH;

        if (buffer[1] == (JPEG_APP0 + 2) && !memcmp(identifier, AVIF_JPEG_MPF_HEADER, AVIF_JPEG_MPF_HEADER_LENGTH)) {
            // MPF segment found.
            *mpfOffset = offset;
            fseek(f, oldOffset, SEEK_SET);
            return AVIF_TRUE;
        }

        // Skip to the next segment.
        offset += segmentLength - 2 - AVIF_JPEG_MPF_HEADER_LENGTH;
        if (fseek(f, offset, SEEK_SET) != 0) {
            fseek(f, oldOffset, SEEK_SET);
            return AVIF_FALSE;
        }
    }
    return AVIF_FALSE;
}

// Searches for a node called 'nameSpace:nodeName' in the children (or descendants if 'recursive' is set) of 'parentNode'.
// Returns the first such node found (in depth first search). Returns NULL if no such node is found.
static const xmlNode * avifJPEGFindXMLNodeByName(const xmlNode * parentNode, const char * nameSpace, const char * nodeName, avifBool recursive)
{
    if (parentNode == NULL) {
        return NULL;
    }
    for (const xmlNode * node = parentNode->children; node != NULL; node = node->next) {
        if (node->ns != NULL && !xmlStrcmp(node->ns->href, (const xmlChar *)nameSpace) &&
            !xmlStrcmp(node->name, (const xmlChar *)nodeName)) {
            return node;
        } else if (recursive) {
            const xmlNode * descendantNode = avifJPEGFindXMLNodeByName(node, nameSpace, nodeName, recursive);
            if (descendantNode != NULL) {
                return descendantNode;
            }
        }
    }
    return NULL;
}

#define XML_NAME_SPACE_GAIN_MAP "http://ns.adobe.com/hdr-gain-map/1.0/"
#define XML_NAME_SPACE_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"

// Finds an 'rdf:Description' node containing a gain map version attribute (hdrgm:Version="1.0").
// Returns NULL if not found.
static const xmlNode * avifJPEGFindGainMapXMPNode(const xmlNode * rootNode)
{
    // See XMP specification https://github.com/adobe/XMP-Toolkit-SDK/blob/main/docs/XMPSpecificationPart1.pdf
    // ISO 16684-1:2011 7.1 "For this serialization, a single XMP packet shall be serialized using a single rdf:RDF XML element."
    // 7.3 "Other XML elements may appear around the rdf:RDF element."
    const xmlNode * rdfNode = avifJPEGFindXMLNodeByName(rootNode, XML_NAME_SPACE_RDF, "RDF", /*recursive=*/AVIF_TRUE);
    if (rdfNode == NULL) {
        return NULL;
    }
    for (const xmlNode * node = rdfNode->children; node != NULL; node = node->next) {
        // Loop through rdf:Description children.
        // 7.4 "A single XMP packet shall be serialized using a single rdf:RDF XML element. The rdf:RDF element content
        // shall consist of only zero or more rdf:Description elements."
        if (node->ns && !xmlStrcmp(node->ns->href, (const xmlChar *)XML_NAME_SPACE_RDF) &&
            !xmlStrcmp(node->name, (const xmlChar *)"Description")) {
            // Look for the gain map version attribute: hdrgm:Version="1.0"
            for (xmlAttr * prop = node->properties; prop != NULL; prop = prop->next) {
                if (prop->ns && !xmlStrcmp(prop->ns->href, (const xmlChar *)XML_NAME_SPACE_GAIN_MAP) &&
                    !xmlStrcmp(prop->name, (const xmlChar *)"Version") && prop->children != NULL &&
                    !xmlStrcmp(prop->children->content, (const xmlChar *)"1.0")) {
                    return node;
                }
            }
        }
    }
    return NULL;
}

// Use XML_PARSE_RECOVER and XML_PARSE_NOERROR to avoid failing/printing errors for invalid XML.
// In particular, if the jpeg files contains extended XMP, avifJPEGReadInternal simply concatenates it to
// standard XMP, which is not a valid XML tree.
// TODO(maryla): better handle extended XMP. If the gain map metadata is in the extended part,
// the current code won't detect it.
#define LIBXML2_XML_PARSING_FLAGS (XML_PARSE_RECOVER | XML_PARSE_NOERROR)

// Returns true if there is an 'rdf:Description' node containing a gain map version attribute (hdrgm:Version="1.0").
// On the main image, this signals that the file also contains a gain map.
// On a subsequent image, this signals that it is a gain map.
static avifBool avifJPEGHasGainMapXMPNode(const uint8_t * xmpData, size_t xmpSize)
{
    xmlDoc * document = xmlReadMemory((const char *)xmpData, (int)xmpSize, NULL, NULL, LIBXML2_XML_PARSING_FLAGS);
    if (document == NULL) {
        return AVIF_FALSE; // Probably and out of memory error.
    }
    const xmlNode * rootNode = xmlDocGetRootElement(document);
    const xmlNode * node = avifJPEGFindGainMapXMPNode(rootNode);
    const avifBool found = (node != NULL);
    xmlFreeDoc(document);
    return found;
}

// Finds the value of a gain map metadata property, that can be either stored as an attribute of 'descriptionNode'
// (which should point to a <rdf:Description> node) or as a child node.
// 'maxValues' is the maximum number of expected values, and the size of the 'values' array. 'numValues' is set to the number
// of values actually found (which may be smaller or larger, but only up to 'maxValues' are stored in 'values').
// Returns AVIF_TRUE if the property was found.
static avifBool avifJPEGFindGainMapProperty(const xmlNode * descriptionNode,
                                            const char * propertyName,
                                            uint32_t maxValues,
                                            const char * values[],
                                            uint32_t * numValues)
{
    *numValues = 0;

    // Search attributes.
    for (xmlAttr * prop = descriptionNode->properties; prop != NULL; prop = prop->next) {
        if (prop->ns && !xmlStrcmp(prop->ns->href, (const xmlChar *)XML_NAME_SPACE_GAIN_MAP) &&
            !xmlStrcmp(prop->name, (const xmlChar *)propertyName) && prop->children != NULL && prop->children->content != NULL) {
            // Properties should have just one child containing the property's value
            // (in fact the 'children' field is documented as "the value of the property").
            values[0] = (const char *)prop->children->content;
            *numValues = 1;
            return AVIF_TRUE;
        }
    }

    // Search child nodes.
    for (const xmlNode * node = descriptionNode->children; node != NULL; node = node->next) {
        if (node->ns && !xmlStrcmp(node->ns->href, (const xmlChar *)XML_NAME_SPACE_GAIN_MAP) &&
            !xmlStrcmp(node->name, (const xmlChar *)propertyName) && node->children) {
            // Multiple values can be specified with a Seq tag: <rdf:Seq><rdf:li>value1</rdf:li><rdf:li>value2</rdf:li>...</rdf:Seq>
            const xmlNode * seq = avifJPEGFindXMLNodeByName(node, XML_NAME_SPACE_RDF, "Seq", /*recursive=*/AVIF_FALSE);
            if (seq) {
                for (xmlNode * seqChild = seq->children; seqChild; seqChild = seqChild->next) {
                    if (!xmlStrcmp(seqChild->name, (const xmlChar *)"li") && seqChild->children != NULL &&
                        seqChild->children->content != NULL) {
                        if (*numValues < maxValues) {
                            values[*numValues] = (const char *)seqChild->children->content;
                        }
                        ++(*numValues);
                    }
                }
                return *numValues > 0 ? AVIF_TRUE : AVIF_FALSE;
            } else if (node->children->next == NULL && node->children->type == XML_TEXT_NODE) { // Only one child and it's text.
                values[0] = (const char *)node->children->content;
                *numValues = 1;
                return AVIF_TRUE;
            }
            // We found a tag for this property but no valid content.
            return AVIF_FALSE;
        }
    }

    return AVIF_FALSE; // Property not found.
}

// Up to 3 values per property (one for each RGB channel).
#define GAIN_MAP_PROPERTY_MAX_VALUES 3

// Looks for a given gain map property's double value(s), and if found, stores them in 'values'.
// The 'values' array should have size at least 'numDoubles', and should be initialized with default
// values for this property, since the array will be left untouched if the property is not found.
// Returns AVIF_TRUE if the property was successfully parsed, or if it was not found, since all properties
// are optional. Returns AVIF_FALSE in case of error (invalid metadata XMP).
static avifBool avifJPEGFindGainMapPropertyDoubles(const xmlNode * descriptionNode, const char * propertyName, double * values, uint32_t numDoubles)
{
    assert(numDoubles <= GAIN_MAP_PROPERTY_MAX_VALUES);
    const char * textValues[GAIN_MAP_PROPERTY_MAX_VALUES];
    uint32_t numValues;
    if (!avifJPEGFindGainMapProperty(descriptionNode, propertyName, /*maxValues=*/numDoubles, &textValues[0], &numValues)) {
        return AVIF_TRUE; // Property was not found, but it's not an error since they're optional.
    }
    if (numValues != 1 && numValues != numDoubles) {
        return AVIF_FALSE; // Invalid, we expect either 1 or exactly numDoubles values.
    }
    for (uint32_t i = 0; i < numDoubles; ++i) {
        if (i >= numValues) {
            // If there is only 1 value, it's copied into the rest of the array.
            values[i] = values[i - 1];
        } else {
            int charsRead;
            if (sscanf(textValues[i], "%lf%n", &values[i], &charsRead) < 1) {
                return AVIF_FALSE; // Was not able to parse the full string value as a double.
            }
            // Make sure that remaining characters (if any) are only whitespace.
            const int len = (int)strlen(textValues[i]);
            while (charsRead < len) {
                if (!isspace(textValues[i][charsRead])) {
                    return AVIF_FALSE; // Invalid character.
                }
                ++charsRead;
            }
        }
    }

    return AVIF_TRUE;
}

static inline void SwapDoubles(double * x, double * y)
{
    double tmp = *x;
    *x = *y;
    *y = tmp;
}

// Parses gain map metadata from XMP.
// See https://helpx.adobe.com/camera-raw/using/gain-map.html
// Returns AVIF_TRUE if the gain map metadata was successfully read.
static avifBool avifJPEGParseGainMapXMPProperties(const xmlNode * rootNode, avifGainMapMetadata * metadata)
{
    const xmlNode * descNode = avifJPEGFindGainMapXMPNode(rootNode);
    if (descNode == NULL) {
        return AVIF_FALSE;
    }

    avifGainMapMetadataDouble metadataDouble;
    // Set default values from Adobe's spec.
    metadataDouble.backwardDirection = AVIF_FALSE;
    metadataDouble.baseHdrHeadroom = 0.0;
    metadataDouble.alternateHdrHeadroom = 1.0;
    for (int i = 0; i < 3; ++i) {
        metadataDouble.gainMapMin[i] = 0.0;
        metadataDouble.gainMapMax[i] = 1.0;
        metadataDouble.baseOffset[i] = 1.0 / 64.0;
        metadataDouble.alternateOffset[i] = 1.0 / 64.0;
        metadataDouble.gainMapGamma[i] = 1.0;
    }
    // Not in Adobe's spec but both color spaces should be the same so this value doesn't matter.
    metadataDouble.useBaseColorSpace = AVIF_TRUE;

    AVIF_CHECK(avifJPEGFindGainMapPropertyDoubles(descNode, "HDRCapacityMin", &metadataDouble.baseHdrHeadroom, /*numDoubles=*/1));
    AVIF_CHECK(avifJPEGFindGainMapPropertyDoubles(descNode, "HDRCapacityMax", &metadataDouble.alternateHdrHeadroom, /*numDoubles=*/1));
    AVIF_CHECK(avifJPEGFindGainMapPropertyDoubles(descNode, "OffsetSDR", metadataDouble.baseOffset, /*numDoubles=*/3));
    AVIF_CHECK(avifJPEGFindGainMapPropertyDoubles(descNode, "OffsetHDR", metadataDouble.alternateOffset, /*numDoubles=*/3));
    AVIF_CHECK(avifJPEGFindGainMapPropertyDoubles(descNode, "GainMapMin", metadataDouble.gainMapMin, /*numDoubles=*/3));
    AVIF_CHECK(avifJPEGFindGainMapPropertyDoubles(descNode, "GainMapMax", metadataDouble.gainMapMax, /*numDoubles=*/3));
    AVIF_CHECK(avifJPEGFindGainMapPropertyDoubles(descNode, "Gamma", metadataDouble.gainMapGamma, /*numDoubles=*/3));

    // See inequality requirements in section 'XMP Representation of Gain Map Metadata' of Adobe's gain map specification
    // https://helpx.adobe.com/camera-raw/using/gain-map.html
    AVIF_CHECK(metadataDouble.alternateHdrHeadroom > metadataDouble.baseHdrHeadroom);
    AVIF_CHECK(metadataDouble.baseHdrHeadroom >= 0);
    for (int i = 0; i < 3; ++i) {
        AVIF_CHECK(metadataDouble.gainMapMax[i] >= metadataDouble.gainMapMin[i]);
        AVIF_CHECK(metadataDouble.baseOffset[i] >= 0.0);
        AVIF_CHECK(metadataDouble.alternateOffset[i] >= 0.0);
        AVIF_CHECK(metadataDouble.gainMapGamma[i] > 0.0);
    }

    uint32_t numValues;
    const char * baseRenditionIsHDR;
    if (avifJPEGFindGainMapProperty(descNode, "BaseRenditionIsHDR", /*maxValues=*/1, &baseRenditionIsHDR, &numValues)) {
        if (!strcmp(baseRenditionIsHDR, "True")) {
            metadataDouble.backwardDirection = AVIF_TRUE;
            SwapDoubles(&metadataDouble.baseHdrHeadroom, &metadataDouble.alternateHdrHeadroom);
            for (int c = 0; c < 3; ++c) {
                SwapDoubles(&metadataDouble.baseOffset[c], &metadataDouble.alternateOffset[c]);
            }
        } else if (!strcmp(baseRenditionIsHDR, "False")) {
            metadataDouble.backwardDirection = AVIF_FALSE;
        } else {
            return AVIF_FALSE; // Unexpected value.
        }
    }

    AVIF_CHECK(avifGainMapMetadataDoubleToFractions(metadata, &metadataDouble));

    return AVIF_TRUE;
}

// Parses gain map metadata from an XMP payload.
// Returns AVIF_TRUE if the gain map metadata was successfully read.
avifBool avifJPEGParseGainMapXMP(const uint8_t * xmpData, size_t xmpSize, avifGainMapMetadata * metadata)
{
    xmlDoc * document = xmlReadMemory((const char *)xmpData, (int)xmpSize, NULL, NULL, LIBXML2_XML_PARSING_FLAGS);
    if (document == NULL) {
        return AVIF_FALSE; // Probably an out of memory error.
    }
    xmlNode * rootNode = xmlDocGetRootElement(document);
    const avifBool res = avifJPEGParseGainMapXMPProperties(rootNode, metadata);
    xmlFreeDoc(document);
    return res;
}

// Parses an MPF (Multi-Picture File) JPEG metadata segment to find the location of other
// images, and decodes the gain map image (as determined by having gain map XMP metadata) into 'avif'.
// See CIPA DC-007-Translation-2021 Multi-Picture Format at https://www.cipa.jp/e/std/std-sec.html
// and https://helpx.adobe.com/camera-raw/using/gain-map.html in particular Figures 1 to 6.
// Returns AVIF_FALSE if no gain map was found.
static avifBool avifJPEGExtractGainMapImageFromMpf(FILE * f,
                                                   uint32_t imageSizeLimit,
                                                   const avifROData * segmentData,
                                                   avifImage * avif,
                                                   avifChromaDownsampling chromaDownsampling)
{
    uint32_t offset = 0;

    const uint8_t littleEndian[4] = { 0x49, 0x49, 0x2A, 0x00 }; // "II*\0"
    const uint8_t bigEndian[4] = { 0x4D, 0x4D, 0x00, 0x2A };    // "MM\0*"

    uint8_t endiannessTag[4];
    AVIF_CHECK(avifJPEGReadBytes(segmentData, endiannessTag, &offset, 4));

    avifBool isBigEndian;
    if (!memcmp(endiannessTag, bigEndian, 4)) {
        isBigEndian = AVIF_TRUE;
    } else if (!memcmp(endiannessTag, littleEndian, 4)) {
        isBigEndian = AVIF_FALSE;
    } else {
        return AVIF_FALSE; // Invalid endianness tag.
    }

    uint32_t offsetToFirstIfd;
    AVIF_CHECK(avifJPEGReadU32(segmentData, &offsetToFirstIfd, &offset, isBigEndian));
    if (offsetToFirstIfd < offset) {
        return AVIF_FALSE;
    }
    offset = offsetToFirstIfd;

    // Read MP (Multi-Picture) tags.
    uint16_t mpTagCount;
    AVIF_CHECK(avifJPEGReadU16(segmentData, &mpTagCount, &offset, isBigEndian));

    // See also https://www.media.mit.edu/pia/Research/deepview/exif.html
    uint32_t numImages = 0;
    uint32_t mpEntryOffset = 0;
    for (int mpTagIdx = 0; mpTagIdx < mpTagCount; ++mpTagIdx) {
        uint16_t tagId;
        AVIF_CHECK(avifJPEGReadU16(segmentData, &tagId, &offset, isBigEndian));
        offset += 2; // Skip data format.
        offset += 4; // Skip num components.
        uint8_t valueBytes[4];
        AVIF_CHECK(avifJPEGReadBytes(segmentData, valueBytes, &offset, 4));
        const uint32_t value = isBigEndian ? avifJPEGReadUint32BigEndian(valueBytes) : avifJPEGReadUint32LittleEndian(valueBytes);

        switch (tagId) { // MPFVersion
            case 45056:  // MPFVersion
                if (memcmp(valueBytes, "0100", 4)) {
                    // Unexpected version.
                    return AVIF_FALSE;
                }
                break;
            case 45057: // NumberOfImages
                numImages = value;
                break;
            case 45058: // MPEntry
                mpEntryOffset = value;
                break;
            case 45059: // ImageUIDList, unused
            case 45060: // TotalFrames, unused
            default:
                break;
        }
    }
    if (numImages < 2 || mpEntryOffset < offset) {
        return AVIF_FALSE;
    }
    offset = mpEntryOffset;

    uint32_t mpfSegmentOffset;
    AVIF_CHECK(avifJPEGFindMpfSegmentOffset(f, &mpfSegmentOffset));

    for (uint32_t imageIdx = 0; imageIdx < numImages; ++imageIdx) {
        offset += 4; // Skip "Individual Image Attribute"
        uint32_t imageSize;
        AVIF_CHECK(avifJPEGReadU32(segmentData, &imageSize, &offset, isBigEndian));
        uint32_t imageDataOffset;
        AVIF_CHECK(avifJPEGReadU32(segmentData, &imageDataOffset, &offset, isBigEndian));

        offset += 4; // Skip "Dependent image Entry Number" (2 + 2 bytes)
        if (imageDataOffset == 0) {
            // 0 is a special value which indicates the first image.
            // Assume the first image cannot be the gain map and skip it.
            continue;
        }

        // Offsets are relative to the start of the MPF segment. Make them absolute.
        imageDataOffset += mpfSegmentOffset;
        if (fseek(f, imageDataOffset, SEEK_SET) != 0) {
            return AVIF_FALSE;
        }
        // Read the image and check its XMP to see if it's a gain map.
        // NOTE we decode all additional images until a gain map is found, even if some might not
        // be gain maps. This could be fixed by having a helper function to get just the XMP without
        // decoding the whole image.
        if (!avifJPEGReadInternal(f,
                                  "gain map",
                                  avif,
                                  /*requestedFormat=*/AVIF_PIXEL_FORMAT_NONE, // automatic
                                  /*requestedDepth=*/0,                       // automatic
                                  chromaDownsampling,
                                  /*ignoreColorProfile=*/AVIF_TRUE,
                                  /*ignoreExif=*/AVIF_TRUE,
                                  /*ignoreXMP=*/AVIF_FALSE,
                                  /*ignoreGainMap=*/AVIF_TRUE,
                                  imageSizeLimit)) {
            continue;
        }
        if (avifJPEGHasGainMapXMPNode(avif->xmp.data, avif->xmp.size)) {
            return AVIF_TRUE;
        }
    }

    return AVIF_FALSE;
}

// Tries to find and decode a gain map image and its metadata.
// Looks for an MPF (Multi-Picture Format) segment then loops through the linked images to see
// if one of them has gain map XMP metadata.
// See CIPA DC-007-Translation-2021 Multi-Picture Format at https://www.cipa.jp/e/std/std-sec.html
// and https://helpx.adobe.com/camera-raw/using/gain-map.html
// Returns AVIF_TRUE if a gain map was found.
static avifBool avifJPEGExtractGainMapImage(FILE * f,
                                            uint32_t imageSizeLimit,
                                            struct jpeg_decompress_struct * cinfo,
                                            avifGainMap * gainMap,
                                            avifChromaDownsampling chromaDownsampling)
{
    const avifROData tagMpf = { (const uint8_t *)AVIF_JPEG_MPF_HEADER, AVIF_JPEG_MPF_HEADER_LENGTH };
    for (jpeg_saved_marker_ptr marker = cinfo->marker_list; marker != NULL; marker = marker->next) {
        // Note we assume there is only one MPF segment and only look at the first one.
        // Otherwise avifJPEGFindMpfSegmentOffset() would have to be modified to take the index of the
        // MPF segment whose offset to return.
        if ((marker->marker == (JPEG_APP0 + 2)) && (marker->data_length > tagMpf.size) &&
            !memcmp(marker->data, tagMpf.data, tagMpf.size)) {
            avifImage * image = avifImageCreateEmpty();
            // Set jpeg native matrix coefficients to allow copying YUV values directly.
            image->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT601;
            assert(avifJPEGHasCompatibleMatrixCoefficients(image->matrixCoefficients));

            const avifROData mpfData = { (const uint8_t *)marker->data + tagMpf.size, marker->data_length - tagMpf.size };
            if (!avifJPEGExtractGainMapImageFromMpf(f, imageSizeLimit, &mpfData, image, chromaDownsampling)) {
                fprintf(stderr, "Note: XMP metadata indicated the presence of a gain map, but it could not be found or decoded\n");
                avifImageDestroy(image);
                return AVIF_FALSE;
            }
            if (!avifJPEGParseGainMapXMP(image->xmp.data, image->xmp.size, &gainMap->metadata)) {
                fprintf(stderr, "Warning: failed to parse gain map metadata\n");
                avifImageDestroy(image);
                return AVIF_FALSE;
            }

            gainMap->image = image;
            return AVIF_TRUE;
        }
    }
    return AVIF_FALSE;
}
#endif // AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION

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

static avifBool avifJPEGReadInternal(FILE * f,
                                     const char * inputFilename,
                                     avifImage * avif,
                                     avifPixelFormat requestedFormat,
                                     uint32_t requestedDepth,
                                     avifChromaDownsampling chromaDownsampling,
                                     avifBool ignoreColorProfile,
                                     avifBool ignoreExif,
                                     avifBool ignoreXMP,
                                     avifBool ignoreGainMap,
                                     uint32_t imageSizeLimit)
{
    volatile avifBool ret = AVIF_FALSE;
    uint8_t * volatile iccData = NULL;

    avifRGBImage rgb;
    memset(&rgb, 0, sizeof(avifRGBImage));

    // Standard XMP segment followed by all extended XMP segments.
    avifRWData totalXMP = { NULL, 0 };
    // Each byte set to 0 is a missing byte. Each byte set to 1 was read and copied to totalXMP.
    avifRWData extendedXMPReadBytes = { NULL, 0 };

    struct my_error_mgr jerr;
    struct jpeg_decompress_struct cinfo;
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;
    if (setjmp(jerr.setjmp_buffer)) {
        goto cleanup;
    }

    jpeg_create_decompress(&cinfo);

    // See also https://exiftool.org/TagNames/JPEG.html for the meaning of various APP<n> segments.
    if (!ignoreExif || !ignoreXMP || !ignoreGainMap) {
        // Keep APP1 blocks, for Exif and XMP.
        jpeg_save_markers(&cinfo, JPEG_APP0 + 1, /*length_limit=*/0xFFFF);
    }
    if (!ignoreGainMap) {
        // Keep APP2 blocks, for obtaining ICC and MPF data.
        jpeg_save_markers(&cinfo, JPEG_APP0 + 2, /*length_limit=*/0xFFFF);
    }

    if (!ignoreColorProfile) {
        setup_read_icc_profile(&cinfo);
    }
    jpeg_stdio_src(&cinfo, f);
    jpeg_read_header(&cinfo, TRUE);

    if (!ignoreColorProfile) {
        uint8_t * iccDataTmp;
        unsigned int iccDataLen;
        if (read_icc_profile(&cinfo, &iccDataTmp, &iccDataLen)) {
            iccData = iccDataTmp;
            if (avifImageSetProfileICC(avif, iccDataTmp, (size_t)iccDataLen) != AVIF_RESULT_OK) {
                fprintf(stderr, "Setting ICC profile failed: %s (out of memory)\n", inputFilename);
                goto cleanup;
            }
        }
    }

    avif->yuvFormat = requestedFormat; // This may be AVIF_PIXEL_FORMAT_NONE, which is "auto" to avifJPEGReadCopy()
    avif->depth = requestedDepth ? requestedDepth : 8;
    // JPEG doesn't have alpha. Prevent confusion.
    avif->alphaPremultiplied = AVIF_FALSE;

    if (avifJPEGReadCopy(avif, imageSizeLimit, &cinfo)) {
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
        if ((uint32_t)avif->width > imageSizeLimit / (uint32_t)avif->height) {
            fprintf(stderr, "Too big JPEG dimensions (%d x %d > %u px): %s\n", avif->width, avif->height, imageSizeLimit, inputFilename);
            goto cleanup;
        }
#if defined(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
        const avifBool useYCgCoR = (avif->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO_RE ||
                                    avif->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO_RO);
#else
        const avifBool useYCgCoR = AVIF_FALSE;
#endif
        if (avif->yuvFormat == AVIF_PIXEL_FORMAT_NONE) {
            // Identity and YCgCo-R are only valid with YUV444.
            avif->yuvFormat = (avif->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY || useYCgCoR)
                                  ? AVIF_PIXEL_FORMAT_YUV444
                                  : AVIF_APP_DEFAULT_PIXEL_FORMAT;
        }
        avif->depth = requestedDepth ? requestedDepth : 8;
#if defined(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
        if (useYCgCoR) {
            if (avif->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO_RO) {
                fprintf(stderr, "AVIF_MATRIX_COEFFICIENTS_YCGCO_RO cannot be used with JPEG because it has an even bit depth.\n");
                goto cleanup;
            }
            if (requestedDepth && requestedDepth != 10) {
                fprintf(stderr, "Cannot request %u bits for YCgCo-Re as it uses 2 extra bits.\n", requestedDepth);
                goto cleanup;
            }
            avif->depth = 10;
        }
#endif
        avifRGBImageSetDefaults(&rgb, avif);
        rgb.format = AVIF_RGB_FORMAT_RGB;
        rgb.chromaDownsampling = chromaDownsampling;
        rgb.depth = 8;
        if (avifRGBImageAllocatePixels(&rgb) != AVIF_RESULT_OK) {
            fprintf(stderr, "Conversion to YUV failed: %s (out of memory)\n", inputFilename);
            goto cleanup;
        }

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
        const avifROData tagExif = { (const uint8_t *)AVIF_JPEG_EXIF_HEADER, AVIF_JPEG_EXIF_HEADER_LENGTH };
        avifBool found = AVIF_FALSE;
        for (jpeg_saved_marker_ptr marker = cinfo.marker_list; marker != NULL; marker = marker->next) {
            if ((marker->marker == (JPEG_APP0 + 1)) && (marker->data_length > tagExif.size) &&
                !memcmp(marker->data, tagExif.data, tagExif.size)) {
                if (found) {
                    // TODO(yguyon): Implement instead of outputting an error.
                    fprintf(stderr, "Exif extraction failed: unsupported Exif split into multiple segments or invalid multiple Exif segments\n");
                    goto cleanup;
                }
                // Exif orientation, if any, is imported to avif->irot/imir and kept in avif->exif.
                // libheif has the same behavior, see
                // https://github.com/strukturag/libheif/blob/ea78603d8e47096606813d221725621306789ff2/examples/heif_enc.cc#L403
                if (avifImageSetMetadataExif(avif, marker->data + tagExif.size, marker->data_length - tagExif.size) != AVIF_RESULT_OK) {
                    fprintf(stderr, "Setting Exif metadata failed: %s (out of memory)\n", inputFilename);
                    goto cleanup;
                }
                found = AVIF_TRUE;
            }
        }
    }

    avifBool readXMP = !ignoreXMP;
#if defined(AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION)
    readXMP = readXMP || !ignoreGainMap; // Gain map metadata is in XMP.
#endif
    if (readXMP) {
        const uint8_t * standardXMPData = NULL;
        uint32_t standardXMPSize = 0; // At most 64kB as defined by Adobe XMP Specification Part 3.
        for (jpeg_saved_marker_ptr marker = cinfo.marker_list; marker != NULL; marker = marker->next) {
            if ((marker->marker == (JPEG_APP0 + 1)) && (marker->data_length > AVIF_JPEG_STANDARD_XMP_TAG_LENGTH) &&
                !memcmp(marker->data, AVIF_JPEG_STANDARD_XMP_TAG, AVIF_JPEG_STANDARD_XMP_TAG_LENGTH)) {
                if (standardXMPData) {
                    fprintf(stderr, "XMP extraction failed: invalid multiple standard XMP segments\n");
                    goto cleanup;
                }
                standardXMPData = marker->data + AVIF_JPEG_STANDARD_XMP_TAG_LENGTH;
                standardXMPSize = (uint32_t)(marker->data_length - AVIF_JPEG_STANDARD_XMP_TAG_LENGTH);
            }
        }

        avifBool foundExtendedXMP = AVIF_FALSE;
        uint8_t extendedXMPGUID[AVIF_JPEG_EXTENDED_XMP_GUID_LENGTH]; // The value is common to all extended XMP segments.
        for (jpeg_saved_marker_ptr marker = cinfo.marker_list; marker != NULL; marker = marker->next) {
            if ((marker->marker == (JPEG_APP0 + 1)) && (marker->data_length > AVIF_JPEG_EXTENDED_XMP_TAG_LENGTH) &&
                !memcmp(marker->data, AVIF_JPEG_EXTENDED_XMP_TAG, AVIF_JPEG_EXTENDED_XMP_TAG_LENGTH)) {
                if (!standardXMPData) {
                    fprintf(stderr, "XMP extraction failed: extended XMP segment found, missing standard XMP segment\n");
                    goto cleanup;
                }

                if (marker->data_length < AVIF_JPEG_OFFSET_TILL_EXTENDED_XMP) {
                    fprintf(stderr, "XMP extraction failed: truncated extended XMP segment\n");
                    goto cleanup;
                }
                const uint8_t * guid = &marker->data[AVIF_JPEG_EXTENDED_XMP_TAG_LENGTH];
                for (size_t c = 0; c < AVIF_JPEG_EXTENDED_XMP_GUID_LENGTH; ++c) {
                    // According to Adobe XMP Specification Part 3 section 1.1.3.1:
                    //   "128-bit GUID stored as a 32-byte ASCII hex string, capital A-F, no null termination"
                    if (((guid[c] < '0') || (guid[c] > '9')) && ((guid[c] < 'A') || (guid[c] > 'F'))) {
                        fprintf(stderr, "XMP extraction failed: invalid XMP segment GUID\n");
                        goto cleanup;
                    }
                }
                // Size of the current extended segment.
                const size_t extendedXMPSize = marker->data_length - AVIF_JPEG_OFFSET_TILL_EXTENDED_XMP;
                // Expected size of the sum of all extended segments.
                // According to Adobe XMP Specification Part 3 section 1.1.3.1:
                //   "full length of the ExtendedXMP serialization as a 32-bit unsigned integer"
                const uint32_t totalExtendedXMPSize =
                    avifJPEGReadUint32BigEndian(&marker->data[AVIF_JPEG_EXTENDED_XMP_TAG_LENGTH + AVIF_JPEG_EXTENDED_XMP_GUID_LENGTH]);
                // Offset in totalXMP after standardXMP.
                // According to Adobe XMP Specification Part 3 section 1.1.3.1:
                //   "offset of this portion as a 32-bit unsigned integer"
                const uint32_t extendedXMPOffset = avifJPEGReadUint32BigEndian(
                    &marker->data[AVIF_JPEG_EXTENDED_XMP_TAG_LENGTH + AVIF_JPEG_EXTENDED_XMP_GUID_LENGTH + 4]);
                if (((uint64_t)standardXMPSize + totalExtendedXMPSize) > SIZE_MAX) {
                    fprintf(stderr, "XMP extraction failed: total XMP size is too large\n");
                    goto cleanup;
                }
                if ((extendedXMPSize == 0) || (((uint64_t)extendedXMPOffset + extendedXMPSize) > totalExtendedXMPSize)) {
                    fprintf(stderr, "XMP extraction failed: invalid extended XMP segment size or offset\n");
                    goto cleanup;
                }
                if (foundExtendedXMP) {
                    if (memcmp(guid, extendedXMPGUID, AVIF_JPEG_EXTENDED_XMP_GUID_LENGTH)) {
                        fprintf(stderr, "XMP extraction failed: extended XMP segment GUID mismatch\n");
                        goto cleanup;
                    }
                    if (totalExtendedXMPSize != (totalXMP.size - standardXMPSize)) {
                        fprintf(stderr, "XMP extraction failed: extended XMP total size mismatch\n");
                        goto cleanup;
                    }
                } else {
                    memcpy(extendedXMPGUID, guid, AVIF_JPEG_EXTENDED_XMP_GUID_LENGTH);

                    if (avifRWDataRealloc(&totalXMP, (size_t)standardXMPSize + totalExtendedXMPSize) != AVIF_RESULT_OK) {
                        fprintf(stderr, "XMP extraction failed: out of memory\n");
                        goto cleanup;
                    }
                    memcpy(totalXMP.data, standardXMPData, standardXMPSize);

                    // Keep track of the bytes that were set.
                    if (avifRWDataRealloc(&extendedXMPReadBytes, totalExtendedXMPSize) != AVIF_RESULT_OK) {
                        fprintf(stderr, "XMP extraction failed: out of memory\n");
                        goto cleanup;
                    }
                    memset(extendedXMPReadBytes.data, 0, extendedXMPReadBytes.size);

                    foundExtendedXMP = AVIF_TRUE;
                }
                // According to Adobe XMP Specification Part 3 section 1.1.3.1:
                //   "A robust JPEG reader should tolerate the marker segments in any order."
                memcpy(&totalXMP.data[standardXMPSize + extendedXMPOffset], &marker->data[AVIF_JPEG_OFFSET_TILL_EXTENDED_XMP], extendedXMPSize);

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
            uint8_t xmpNote[AVIF_JPEG_XMP_NOTE_TAG_LENGTH + AVIF_JPEG_EXTENDED_XMP_GUID_LENGTH];
            memcpy(xmpNote, AVIF_JPEG_XMP_NOTE_TAG, AVIF_JPEG_XMP_NOTE_TAG_LENGTH);
            memcpy(xmpNote + AVIF_JPEG_XMP_NOTE_TAG_LENGTH, extendedXMPGUID, AVIF_JPEG_EXTENDED_XMP_GUID_LENGTH);
            if (!avifJPEGFindSubstr(standardXMPData, standardXMPSize, xmpNote, sizeof(xmpNote))) {
                // Try the alternative before returning an error.
                uint8_t alternativeXmpNote[AVIF_JPEG_ALTERNATIVE_XMP_NOTE_TAG_LENGTH + AVIF_JPEG_EXTENDED_XMP_GUID_LENGTH];
                memcpy(alternativeXmpNote, AVIF_JPEG_ALTERNATIVE_XMP_NOTE_TAG, AVIF_JPEG_ALTERNATIVE_XMP_NOTE_TAG_LENGTH);
                memcpy(alternativeXmpNote + AVIF_JPEG_ALTERNATIVE_XMP_NOTE_TAG_LENGTH, extendedXMPGUID, AVIF_JPEG_EXTENDED_XMP_GUID_LENGTH);
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
            if (avifImageSetMetadataXMP(avif, standardXMPData, standardXMPSize) != AVIF_RESULT_OK) {
                fprintf(stderr, "XMP extraction failed: out of memory\n");
                goto cleanup;
            }
        }
        avifImageFixXMP(avif); // Remove one trailing null character if any.
    }

#if defined(AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION)
    // The primary XMP block (for the main image) must contain a node with an hdrgm:Version field if and only if a gain map is present.
    if (!ignoreGainMap && avifJPEGHasGainMapXMPNode(avif->xmp.data, avif->xmp.size)) {
        avifGainMap * gainMap = avifGainMapCreate();
        if (gainMap == NULL) {
            fprintf(stderr, "Creating gain map failed: out of memory\n");
            goto cleanup;
        }
        // Ignore the return value: continue even if we fail to find/parse/decode the gain map.
        if (avifJPEGExtractGainMapImage(f, imageSizeLimit, &cinfo, gainMap, chromaDownsampling)) {
            // Since jpeg doesn't provide this metadata, assume the values are the same as the base image
            // with a PQ transfer curve.
            gainMap->altColorPrimaries = avif->colorPrimaries;
            gainMap->altTransferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_PQ;
            gainMap->altMatrixCoefficients = avif->matrixCoefficients;
            gainMap->altDepth = 8;
            gainMap->altPlaneCount =
                (avif->yuvFormat == AVIF_PIXEL_FORMAT_YUV400 && gainMap->image->yuvFormat == AVIF_PIXEL_FORMAT_YUV400) ? 1 : 3;
            if (avif->icc.size > 0) {
                // The base image's ICC should also apply to the alternage image.
                if (avifRWDataSet(&gainMap->altICC, avif->icc.data, avif->icc.size) != AVIF_RESULT_OK) {
                    fprintf(stderr, "Setting gain map ICC profile failed: out of memory\n");
                    goto cleanup;
                }
            }
            avif->gainMap = gainMap;
        } else {
            avifGainMapDestroy(gainMap);
        }
    }

    if (avif->xmp.size > 0 && ignoreXMP) {
        // Clear XMP in case we read it for something else (like gain map).
        if (avifImageSetMetadataXMP(avif, NULL, 0) != AVIF_RESULT_OK) {
            assert(AVIF_FALSE);
        }
    }
#endif // AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION
    jpeg_finish_decompress(&cinfo);
    ret = AVIF_TRUE;
cleanup:
    jpeg_destroy_decompress(&cinfo);
    free(iccData);
    avifRGBImageFreePixels(&rgb);
    avifRWDataFree(&totalXMP);
    avifRWDataFree(&extendedXMPReadBytes);
    return ret;
}

avifBool avifJPEGRead(const char * inputFilename,
                      avifImage * avif,
                      avifPixelFormat requestedFormat,
                      uint32_t requestedDepth,
                      avifChromaDownsampling chromaDownsampling,
                      avifBool ignoreColorProfile,
                      avifBool ignoreExif,
                      avifBool ignoreXMP,
                      avifBool ignoreGainMap,
                      uint32_t imageSizeLimit)
{
    FILE * f = fopen(inputFilename, "rb");
    if (!f) {
        fprintf(stderr, "Can't open JPEG file for read: %s\n", inputFilename);
        return AVIF_FALSE;
    }
    const avifBool res = avifJPEGReadInternal(f,
                                              inputFilename,
                                              avif,
                                              requestedFormat,
                                              requestedDepth,
                                              chromaDownsampling,
                                              ignoreColorProfile,
                                              ignoreExif,
                                              ignoreXMP,
                                              ignoreGainMap,
                                              imageSizeLimit);
    fclose(f);
    return res;
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
    if (avifRGBImageAllocatePixels(&rgb) != AVIF_RESULT_OK) {
        fprintf(stderr, "Conversion to RGB failed: %s (out of memory)\n", outputFilename);
        goto cleanup;
    }
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
        // TODO(yguyon): Use jpeg_write_icc_profile() instead?
        write_icc_profile(&cinfo, avif->icc.data, (unsigned int)avif->icc.size);
    }

    if (avif->exif.data && (avif->exif.size > 0)) {
        size_t exifTiffHeaderOffset;
        avifResult result = avifGetExifTiffHeaderOffset(avif->exif.data, avif->exif.size, &exifTiffHeaderOffset);
        if (result != AVIF_RESULT_OK) {
            fprintf(stderr, "Error writing JPEG metadata: %s\n", avifResultToString(result));
            goto cleanup;
        }

        avifRWData exif = { NULL, 0 };
        if (avifRWDataRealloc(&exif, AVIF_JPEG_EXIF_HEADER_LENGTH + avif->exif.size - exifTiffHeaderOffset) != AVIF_RESULT_OK) {
            fprintf(stderr, "Error writing JPEG metadata: out of memory\n");
            goto cleanup;
        }
        memcpy(exif.data, AVIF_JPEG_EXIF_HEADER, AVIF_JPEG_EXIF_HEADER_LENGTH);
        memcpy(exif.data + AVIF_JPEG_EXIF_HEADER_LENGTH, avif->exif.data + exifTiffHeaderOffset, avif->exif.size - exifTiffHeaderOffset);
        // Make sure the Exif orientation matches the irot/imir values.
        // libheif does not have the same behavior. The orientation is applied to samples and orientation data is discarded there,
        // see https://github.com/strukturag/libheif/blob/ea78603d8e47096606813d221725621306789ff2/examples/encoder_jpeg.cc#L187
        const uint8_t orientation = avifImageGetExifOrientationFromIrotImir(avif);
        result = avifSetExifOrientation(&exif, orientation);
        if (result != AVIF_RESULT_OK) {
            // Ignore errors if the orientation is the default one because not being able to set Exif orientation now
            // means a reader will not be able to parse it later either.
            if (orientation != 1) {
                fprintf(stderr, "Error writing JPEG metadata: %s\n", avifResultToString(result));
                avifRWDataFree(&exif);
                goto cleanup;
            }
        }

        avifROData remainingExif = { exif.data, exif.size };
        while (remainingExif.size > AVIF_JPEG_MAX_MARKER_DATA_LENGTH) {
            jpeg_write_marker(&cinfo, JPEG_APP0 + 1, remainingExif.data, AVIF_JPEG_MAX_MARKER_DATA_LENGTH);
            remainingExif.data += AVIF_JPEG_MAX_MARKER_DATA_LENGTH;
            remainingExif.size -= AVIF_JPEG_MAX_MARKER_DATA_LENGTH;
        }
        jpeg_write_marker(&cinfo, JPEG_APP0 + 1, remainingExif.data, (unsigned int)remainingExif.size);
        avifRWDataFree(&exif);
    } else if (avifImageGetExifOrientationFromIrotImir(avif) != 1) {
        // There is no Exif yet, but we need to store the orientation.
        // TODO(yguyon): Add a valid Exif payload or rotate the samples.
    }

    if (avif->xmp.data && (avif->xmp.size > 0)) {
        // See XMP specification part 3.
        if (avif->xmp.size > 65502) {
            // libheif just refuses to export JPEG with long XMP, see
            // https://github.com/strukturag/libheif/blob/18291ddebc23c924440a8a3c9a7267fe3beb5901/examples/encoder_jpeg.cc#L227
            // But libheif also ignores extended XMP at reading, so converting a JPEG with extended XMP to HEIC and back to JPEG
            // works, with the extended XMP part dropped, even if it had fit into a single JPEG marker.

            // In libavif the whole XMP payload is dropped if it exceeds a single JPEG marker size limit, with a warning.
            // The advantage is that it keeps the whole XMP payload, including the extended part, if it fits into a single JPEG
            // marker. This is acceptable because section 1.1.3.1 of XMP specification part 3 says
            //   "It is unusual for XMP to exceed 65502 bytes; typically, it is around 2 KB."
            fprintf(stderr, "Warning writing JPEG metadata: XMP payload is too big and was dropped\n");
        } else {
            avifRWData xmp = { NULL, 0 };
            if (avifRWDataRealloc(&xmp, AVIF_JPEG_STANDARD_XMP_TAG_LENGTH + avif->xmp.size) != AVIF_RESULT_OK) {
                fprintf(stderr, "Error writing JPEG metadata: out of memory\n");
                goto cleanup;
            }
            memcpy(xmp.data, AVIF_JPEG_STANDARD_XMP_TAG, AVIF_JPEG_STANDARD_XMP_TAG_LENGTH);
            memcpy(xmp.data + AVIF_JPEG_STANDARD_XMP_TAG_LENGTH, avif->xmp.data, avif->xmp.size);
            jpeg_write_marker(&cinfo, JPEG_APP0 + 1, xmp.data, (unsigned int)xmp.size);
            avifRWDataFree(&xmp);
        }
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
