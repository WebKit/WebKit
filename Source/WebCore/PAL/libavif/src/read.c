// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/internal.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

#define AUXTYPE_SIZE 64
#define CONTENTTYPE_SIZE 64

// class VisualSampleEntry(codingname) extends SampleEntry(codingname) {
//     unsigned int(16) pre_defined = 0;
//     const unsigned int(16) reserved = 0;
//     unsigned int(32)[3] pre_defined = 0;
//     unsigned int(16) width;
//     unsigned int(16) height;
//     template unsigned int(32) horizresolution = 0x00480000; // 72 dpi
//     template unsigned int(32) vertresolution = 0x00480000;  // 72 dpi
//     const unsigned int(32) reserved = 0;
//     template unsigned int(16) frame_count = 1;
//     string[32] compressorname;
//     template unsigned int(16) depth = 0x0018;
//     int(16) pre_defined = -1;
//     // other boxes from derived specifications
//     CleanApertureBox clap;    // optional
//     PixelAspectRatioBox pasp; // optional
// }
static const size_t VISUALSAMPLEENTRY_SIZE = 78;

static const char xmpContentType[] = AVIF_CONTENT_TYPE_XMP;
static const size_t xmpContentTypeSize = sizeof(xmpContentType);

// The only supported ipma box values for both version and flags are [0,1], so there technically
// can't be more than 4 unique tuples right now.
#define MAX_IPMA_VERSION_AND_FLAGS_SEEN 4

#define MAX_AV1_LAYER_COUNT 4

// ---------------------------------------------------------------------------
// Box data structures

// ftyp
typedef struct avifFileType
{
    uint8_t majorBrand[4];
    uint32_t minorVersion;
    // If not null, points to a memory block of 4 * compatibleBrandsCount bytes.
    const uint8_t * compatibleBrands;
    int compatibleBrandsCount;
} avifFileType;

// ispe
typedef struct avifImageSpatialExtents
{
    uint32_t width;
    uint32_t height;
} avifImageSpatialExtents;

// auxC
typedef struct avifAuxiliaryType
{
    char auxType[AUXTYPE_SIZE];
} avifAuxiliaryType;

// infe mime content_type
typedef struct avifContentType
{
    char contentType[CONTENTTYPE_SIZE];
} avifContentType;

// colr
typedef struct avifColourInformationBox
{
    avifBool hasICC;
    uint64_t iccOffset;
    size_t iccSize;

    avifBool hasNCLX;
    avifColorPrimaries colorPrimaries;
    avifTransferCharacteristics transferCharacteristics;
    avifMatrixCoefficients matrixCoefficients;
    avifRange range;
} avifColourInformationBox;

#define MAX_PIXI_PLANE_DEPTHS 4
typedef struct avifPixelInformationProperty
{
    uint8_t planeDepths[MAX_PIXI_PLANE_DEPTHS];
    uint8_t planeCount;
} avifPixelInformationProperty;

typedef struct avifOperatingPointSelectorProperty
{
    uint8_t opIndex;
} avifOperatingPointSelectorProperty;

typedef struct avifLayerSelectorProperty
{
    uint16_t layerID;
} avifLayerSelectorProperty;

typedef struct avifAV1LayeredImageIndexingProperty
{
    uint32_t layerSize[3];
} avifAV1LayeredImageIndexingProperty;

// ---------------------------------------------------------------------------
// Top-level structures

struct avifMeta;

// Temporary storage for ipco/stsd contents until they can be associated and memcpy'd to an avifDecoderItem
typedef struct avifProperty
{
    uint8_t type[4];
    union
    {
        avifImageSpatialExtents ispe;
        avifAuxiliaryType auxC;
        avifColourInformationBox colr;
        avifCodecConfigurationBox av1C;
        avifPixelAspectRatioBox pasp;
        avifCleanApertureBox clap;
        avifImageRotation irot;
        avifImageMirror imir;
        avifPixelInformationProperty pixi;
        avifOperatingPointSelectorProperty a1op;
        avifLayerSelectorProperty lsel;
        avifAV1LayeredImageIndexingProperty a1lx;
    } u;
} avifProperty;
AVIF_ARRAY_DECLARE(avifPropertyArray, avifProperty, prop);

static const avifProperty * avifPropertyArrayFind(const avifPropertyArray * properties, const char * type)
{
    for (uint32_t propertyIndex = 0; propertyIndex < properties->count; ++propertyIndex) {
        avifProperty * prop = &properties->prop[propertyIndex];
        if (!memcmp(prop->type, type, 4)) {
            return prop;
        }
    }
    return NULL;
}

AVIF_ARRAY_DECLARE(avifExtentArray, avifExtent, extent);

// one "item" worth for decoding (all iref, iloc, iprp, etc refer to one of these)
typedef struct avifDecoderItem
{
    uint32_t id;
    struct avifMeta * meta; // Unowned; A back-pointer for convenience
    uint8_t type[4];
    size_t size;
    avifBool idatStored; // If true, offset is relative to the associated meta box's idat box (iloc construction_method==1)
    uint32_t width;      // Set from this item's ispe property, if present
    uint32_t height;     // Set from this item's ispe property, if present
    avifContentType contentType;
    avifPropertyArray properties;
    avifExtentArray extents;       // All extent offsets/sizes
    avifRWData mergedExtents;      // if set, is a single contiguous block of this item's extents (unused when extents.count == 1)
    avifBool ownsMergedExtents;    // if true, mergedExtents must be freed when this item is destroyed
    avifBool partialMergedExtents; // If true, mergedExtents doesn't have all of the item data yet
    uint32_t thumbnailForID;       // if non-zero, this item is a thumbnail for Item #{thumbnailForID}
    uint32_t auxForID;             // if non-zero, this item is an auxC plane for Item #{auxForID}
    uint32_t descForID;            // if non-zero, this item is a content description for Item #{descForID}
    uint32_t dimgForID;            // if non-zero, this item is a derived image for Item #{dimgForID}
    uint32_t premByID;             // if non-zero, this item is premultiplied by Item #{premByID}
    avifBool hasUnsupportedEssentialProperty; // If true, this item cites a property flagged as 'essential' that libavif doesn't support (yet). Ignore the item, if so.
    avifBool ipmaSeen;    // if true, this item already received a property association
    avifBool progressive; // if true, this item has progressive layers (a1lx), but does not select a specific layer (the layer_id value in lsel is set to 0xFFFF)
} avifDecoderItem;
AVIF_ARRAY_DECLARE(avifDecoderItemArray, avifDecoderItem, item);

// grid storage
typedef struct avifImageGrid
{
    uint32_t rows;    // Legal range: [1-256]
    uint32_t columns; // Legal range: [1-256]
    uint32_t outputWidth;
    uint32_t outputHeight;
} avifImageGrid;

// ---------------------------------------------------------------------------
// avifTrack

typedef struct avifSampleTableChunk
{
    uint64_t offset;
} avifSampleTableChunk;
AVIF_ARRAY_DECLARE(avifSampleTableChunkArray, avifSampleTableChunk, chunk);

typedef struct avifSampleTableSampleToChunk
{
    uint32_t firstChunk;
    uint32_t samplesPerChunk;
    uint32_t sampleDescriptionIndex;
} avifSampleTableSampleToChunk;
AVIF_ARRAY_DECLARE(avifSampleTableSampleToChunkArray, avifSampleTableSampleToChunk, sampleToChunk);

typedef struct avifSampleTableSampleSize
{
    uint32_t size;
} avifSampleTableSampleSize;
AVIF_ARRAY_DECLARE(avifSampleTableSampleSizeArray, avifSampleTableSampleSize, sampleSize);

typedef struct avifSampleTableTimeToSample
{
    uint32_t sampleCount;
    uint32_t sampleDelta;
} avifSampleTableTimeToSample;
AVIF_ARRAY_DECLARE(avifSampleTableTimeToSampleArray, avifSampleTableTimeToSample, timeToSample);

typedef struct avifSyncSample
{
    uint32_t sampleNumber;
} avifSyncSample;
AVIF_ARRAY_DECLARE(avifSyncSampleArray, avifSyncSample, syncSample);

typedef struct avifSampleDescription
{
    uint8_t format[4];
    avifPropertyArray properties;
} avifSampleDescription;
AVIF_ARRAY_DECLARE(avifSampleDescriptionArray, avifSampleDescription, description);

typedef struct avifSampleTable
{
    avifSampleTableChunkArray chunks;
    avifSampleDescriptionArray sampleDescriptions;
    avifSampleTableSampleToChunkArray sampleToChunks;
    avifSampleTableSampleSizeArray sampleSizes;
    avifSampleTableTimeToSampleArray timeToSamples;
    avifSyncSampleArray syncSamples;
    uint32_t allSamplesSize; // If this is non-zero, sampleSizes will be empty and all samples will be this size
} avifSampleTable;

static void avifSampleTableDestroy(avifSampleTable * sampleTable);

static avifSampleTable * avifSampleTableCreate()
{
    avifSampleTable * sampleTable = (avifSampleTable *)avifAlloc(sizeof(avifSampleTable));
    memset(sampleTable, 0, sizeof(avifSampleTable));
    if (!avifArrayCreate(&sampleTable->chunks, sizeof(avifSampleTableChunk), 16)) {
        goto error;
    }
    if (!avifArrayCreate(&sampleTable->sampleDescriptions, sizeof(avifSampleDescription), 2)) {
        goto error;
    }
    if (!avifArrayCreate(&sampleTable->sampleToChunks, sizeof(avifSampleTableSampleToChunk), 16)) {
        goto error;
    }
    if (!avifArrayCreate(&sampleTable->sampleSizes, sizeof(avifSampleTableSampleSize), 16)) {
        goto error;
    }
    if (!avifArrayCreate(&sampleTable->timeToSamples, sizeof(avifSampleTableTimeToSample), 16)) {
        goto error;
    }
    if (!avifArrayCreate(&sampleTable->syncSamples, sizeof(avifSyncSample), 16)) {
        goto error;
    }
    return sampleTable;

error:
    avifSampleTableDestroy(sampleTable);
    return NULL;
}

static void avifSampleTableDestroy(avifSampleTable * sampleTable)
{
    avifArrayDestroy(&sampleTable->chunks);
    for (uint32_t i = 0; i < sampleTable->sampleDescriptions.count; ++i) {
        avifSampleDescription * description = &sampleTable->sampleDescriptions.description[i];
        avifArrayDestroy(&description->properties);
    }
    avifArrayDestroy(&sampleTable->sampleDescriptions);
    avifArrayDestroy(&sampleTable->sampleToChunks);
    avifArrayDestroy(&sampleTable->sampleSizes);
    avifArrayDestroy(&sampleTable->timeToSamples);
    avifArrayDestroy(&sampleTable->syncSamples);
    avifFree(sampleTable);
}

static uint32_t avifSampleTableGetImageDelta(const avifSampleTable * sampleTable, int imageIndex)
{
    int maxSampleIndex = 0;
    for (uint32_t i = 0; i < sampleTable->timeToSamples.count; ++i) {
        const avifSampleTableTimeToSample * timeToSample = &sampleTable->timeToSamples.timeToSample[i];
        maxSampleIndex += timeToSample->sampleCount;
        if ((imageIndex < maxSampleIndex) || (i == (sampleTable->timeToSamples.count - 1))) {
            return timeToSample->sampleDelta;
        }
    }

    // TODO: fail here?
    return 1;
}

static avifBool avifSampleTableHasFormat(const avifSampleTable * sampleTable, const char * format)
{
    for (uint32_t i = 0; i < sampleTable->sampleDescriptions.count; ++i) {
        if (!memcmp(sampleTable->sampleDescriptions.description[i].format, format, 4)) {
            return AVIF_TRUE;
        }
    }
    return AVIF_FALSE;
}

static uint32_t avifCodecConfigurationBoxGetDepth(const avifCodecConfigurationBox * av1C)
{
    if (av1C->twelveBit) {
        return 12;
    } else if (av1C->highBitdepth) {
        return 10;
    }
    return 8;
}

// This is used as a hint to validating the clap box in avifDecoderItemValidateAV1.
static avifPixelFormat avifCodecConfigurationBoxGetFormat(const avifCodecConfigurationBox * av1C)
{
    if (av1C->monochrome) {
        return AVIF_PIXEL_FORMAT_YUV400;
    } else if (av1C->chromaSubsamplingY == 1) {
        return AVIF_PIXEL_FORMAT_YUV420;
    } else if (av1C->chromaSubsamplingX == 1) {
        return AVIF_PIXEL_FORMAT_YUV422;
    }
    return AVIF_PIXEL_FORMAT_YUV444;
}

static const avifPropertyArray * avifSampleTableGetProperties(const avifSampleTable * sampleTable)
{
    for (uint32_t i = 0; i < sampleTable->sampleDescriptions.count; ++i) {
        const avifSampleDescription * description = &sampleTable->sampleDescriptions.description[i];
        if (!memcmp(description->format, "av01", 4)) {
            return &description->properties;
        }
    }
    return NULL;
}

// one video track ("trak" contents)
typedef struct avifTrack
{
    uint32_t id;
    uint32_t auxForID; // if non-zero, this track is an auxC plane for Track #{auxForID}
    uint32_t premByID; // if non-zero, this track is premultiplied by Track #{premByID}
    uint32_t mediaTimescale;
    uint64_t mediaDuration;
    uint32_t width;
    uint32_t height;
    avifSampleTable * sampleTable;
    struct avifMeta * meta;
} avifTrack;
AVIF_ARRAY_DECLARE(avifTrackArray, avifTrack, track);

// ---------------------------------------------------------------------------
// avifCodecDecodeInput

avifCodecDecodeInput * avifCodecDecodeInputCreate(void)
{
    avifCodecDecodeInput * decodeInput = (avifCodecDecodeInput *)avifAlloc(sizeof(avifCodecDecodeInput));
    memset(decodeInput, 0, sizeof(avifCodecDecodeInput));
    if (!avifArrayCreate(&decodeInput->samples, sizeof(avifDecodeSample), 1)) {
        goto error;
    }
    return decodeInput;

error:
    avifFree(decodeInput);
    return NULL;
}

void avifCodecDecodeInputDestroy(avifCodecDecodeInput * decodeInput)
{
    for (uint32_t sampleIndex = 0; sampleIndex < decodeInput->samples.count; ++sampleIndex) {
        avifDecodeSample * sample = &decodeInput->samples.sample[sampleIndex];
        if (sample->ownsData) {
            avifRWDataFree((avifRWData *)&sample->data);
        }
    }
    avifArrayDestroy(&decodeInput->samples);
    avifFree(decodeInput);
}

// Returns how many samples are in the chunk.
static uint32_t avifGetSampleCountOfChunk(const avifSampleTableSampleToChunkArray * sampleToChunks, uint32_t chunkIndex)
{
    uint32_t sampleCount = 0;
    for (int sampleToChunkIndex = sampleToChunks->count - 1; sampleToChunkIndex >= 0; --sampleToChunkIndex) {
        const avifSampleTableSampleToChunk * sampleToChunk = &sampleToChunks->sampleToChunk[sampleToChunkIndex];
        if (sampleToChunk->firstChunk <= (chunkIndex + 1)) {
            sampleCount = sampleToChunk->samplesPerChunk;
            break;
        }
    }
    return sampleCount;
}

static avifBool avifCodecDecodeInputFillFromSampleTable(avifCodecDecodeInput * decodeInput,
                                                        avifSampleTable * sampleTable,
                                                        const uint32_t imageCountLimit,
                                                        const uint64_t sizeHint,
                                                        avifDiagnostics * diag)
{
    if (imageCountLimit) {
        // Verify that the we're not about to exceed the frame count limit.

        uint32_t imageCountLeft = imageCountLimit;
        for (uint32_t chunkIndex = 0; chunkIndex < sampleTable->chunks.count; ++chunkIndex) {
            // First, figure out how many samples are in this chunk
            uint32_t sampleCount = avifGetSampleCountOfChunk(&sampleTable->sampleToChunks, chunkIndex);
            if (sampleCount == 0) {
                // chunks with 0 samples are invalid
                avifDiagnosticsPrintf(diag, "Sample table contains a chunk with 0 samples");
                return AVIF_FALSE;
            }

            if (sampleCount > imageCountLeft) {
                // This file exceeds the imageCountLimit, bail out
                avifDiagnosticsPrintf(diag, "Exceeded avifDecoder's imageCountLimit");
                return AVIF_FALSE;
            }
            imageCountLeft -= sampleCount;
        }
    }

    uint32_t sampleSizeIndex = 0;
    for (uint32_t chunkIndex = 0; chunkIndex < sampleTable->chunks.count; ++chunkIndex) {
        avifSampleTableChunk * chunk = &sampleTable->chunks.chunk[chunkIndex];

        // First, figure out how many samples are in this chunk
        uint32_t sampleCount = avifGetSampleCountOfChunk(&sampleTable->sampleToChunks, chunkIndex);
        if (sampleCount == 0) {
            // chunks with 0 samples are invalid
            avifDiagnosticsPrintf(diag, "Sample table contains a chunk with 0 samples");
            return AVIF_FALSE;
        }

        uint64_t sampleOffset = chunk->offset;
        for (uint32_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
            uint32_t sampleSize = sampleTable->allSamplesSize;
            if (sampleSize == 0) {
                if (sampleSizeIndex >= sampleTable->sampleSizes.count) {
                    // We've run out of samples to sum
                    avifDiagnosticsPrintf(diag, "Truncated sample table");
                    return AVIF_FALSE;
                }
                avifSampleTableSampleSize * sampleSizePtr = &sampleTable->sampleSizes.sampleSize[sampleSizeIndex];
                sampleSize = sampleSizePtr->size;
            }

            avifDecodeSample * sample = (avifDecodeSample *)avifArrayPushPtr(&decodeInput->samples);
            sample->offset = sampleOffset;
            sample->size = sampleSize;
            sample->spatialID = AVIF_SPATIAL_ID_UNSET; // Not filtering by spatial_id
            sample->sync = AVIF_FALSE;                 // to potentially be set to true following the outer loop

            if (sampleSize > UINT64_MAX - sampleOffset) {
                avifDiagnosticsPrintf(diag,
                                      "Sample table contains an offset/size pair which overflows: [%" PRIu64 " / %u]",
                                      sampleOffset,
                                      sampleSize);
                return AVIF_FALSE;
            }
            if (sizeHint && ((sampleOffset + sampleSize) > sizeHint)) {
                avifDiagnosticsPrintf(diag, "Exceeded avifIO's sizeHint, possibly truncated data");
                return AVIF_FALSE;
            }

            sampleOffset += sampleSize;
            ++sampleSizeIndex;
        }
    }

    // Mark appropriate samples as sync
    for (uint32_t syncSampleIndex = 0; syncSampleIndex < sampleTable->syncSamples.count; ++syncSampleIndex) {
        uint32_t frameIndex = sampleTable->syncSamples.syncSample[syncSampleIndex].sampleNumber - 1; // sampleNumber is 1-based
        if (frameIndex < decodeInput->samples.count) {
            decodeInput->samples.sample[frameIndex].sync = AVIF_TRUE;
        }
    }

    // Assume frame 0 is sync, just in case the stss box is absent in the BMFF. (Unnecessary?)
    if (decodeInput->samples.count > 0) {
        decodeInput->samples.sample[0].sync = AVIF_TRUE;
    }
    return AVIF_TRUE;
}

static avifBool avifCodecDecodeInputFillFromDecoderItem(avifCodecDecodeInput * decodeInput,
                                                        avifDecoderItem * item,
                                                        avifBool allowProgressive,
                                                        const uint32_t imageCountLimit,
                                                        const uint64_t sizeHint,
                                                        avifDiagnostics * diag)
{
    if (sizeHint && (item->size > sizeHint)) {
        avifDiagnosticsPrintf(diag, "Exceeded avifIO's sizeHint, possibly truncated data");
        return AVIF_FALSE;
    }

    uint8_t layerCount = 0;
    size_t layerSizes[4] = { 0 };
    const avifProperty * a1lxProp = avifPropertyArrayFind(&item->properties, "a1lx");
    if (a1lxProp) {
        // Calculate layer count and all layer sizes from the a1lx box, and then validate

        size_t remainingSize = item->size;
        for (int i = 0; i < 3; ++i) {
            ++layerCount;

            const size_t layerSize = (size_t)a1lxProp->u.a1lx.layerSize[i];
            if (layerSize) {
                if (layerSize >= remainingSize) { // >= instead of > because there must be room for the last layer
                    avifDiagnosticsPrintf(diag, "a1lx layer index [%d] does not fit in item size", i);
                    return AVIF_FALSE;
                }
                layerSizes[i] = layerSize;
                remainingSize -= layerSize;
            } else {
                layerSizes[i] = remainingSize;
                remainingSize = 0;
                break;
            }
        }
        if (remainingSize > 0) {
            assert(layerCount == 3);
            ++layerCount;
            layerSizes[3] = remainingSize;
        }
    }

    const avifProperty * lselProp = avifPropertyArrayFind(&item->properties, "lsel");
    // Progressive images offer layers via the a1lxProp, but don't specify a layer selection with lsel.
    //
    // For backward compatibility with earlier drafts of AVIF spec v1.1.0, treat an absent lsel as
    // equivalent to layer_id == 0xFFFF during the transitional period. Remove !lselProp when the test
    // images have been updated to the v1.1.0 spec.
    item->progressive = (a1lxProp && (!lselProp || (lselProp->u.lsel.layerID == 0xFFFF)));
    if (lselProp && (lselProp->u.lsel.layerID != 0xFFFF)) {
        // Layer selection. This requires that the underlying AV1 codec decodes all layers,
        // and then only returns the requested layer as a single frame. To the user of libavif,
        // this appears to be a single frame.

        decodeInput->allLayers = AVIF_TRUE;

        size_t sampleSize = 0;
        if (layerCount > 0) {
            // Optimization: If we're selecting a layer that doesn't require the entire image's payload (hinted via the a1lx box)

            if (lselProp->u.lsel.layerID >= layerCount) {
                avifDiagnosticsPrintf(diag,
                                      "lsel property requests layer index [%u] which isn't present in a1lx property ([%u] layers)",
                                      lselProp->u.lsel.layerID,
                                      layerCount);
                return AVIF_FALSE;
            }

            for (uint8_t i = 0; i <= lselProp->u.lsel.layerID; ++i) {
                sampleSize += layerSizes[i];
            }
        } else {
            // This layer's payload subsection is unknown, just use the whole payload
            sampleSize = item->size;
        }

        avifDecodeSample * sample = (avifDecodeSample *)avifArrayPushPtr(&decodeInput->samples);
        sample->itemID = item->id;
        sample->offset = 0;
        sample->size = sampleSize;
        assert(lselProp->u.lsel.layerID < MAX_AV1_LAYER_COUNT);
        sample->spatialID = (uint8_t)lselProp->u.lsel.layerID;
        sample->sync = AVIF_TRUE;
    } else if (allowProgressive && item->progressive) {
        // Progressive image. Decode all layers and expose them all to the user.

        if (imageCountLimit && (layerCount > imageCountLimit)) {
            avifDiagnosticsPrintf(diag, "Exceeded avifDecoder's imageCountLimit (progressive)");
            return AVIF_FALSE;
        }

        decodeInput->allLayers = AVIF_TRUE;

        size_t offset = 0;
        for (int i = 0; i < layerCount; ++i) {
            avifDecodeSample * sample = (avifDecodeSample *)avifArrayPushPtr(&decodeInput->samples);
            sample->itemID = item->id;
            sample->offset = offset;
            sample->size = layerSizes[i];
            sample->spatialID = AVIF_SPATIAL_ID_UNSET;
            sample->sync = (i == 0); // Assume all layers depend on the first layer

            offset += layerSizes[i];
        }
    } else {
        // Typical case: Use the entire item's payload for a single frame output

        avifDecodeSample * sample = (avifDecodeSample *)avifArrayPushPtr(&decodeInput->samples);
        sample->itemID = item->id;
        sample->offset = 0;
        sample->size = item->size;
        sample->spatialID = AVIF_SPATIAL_ID_UNSET;
        sample->sync = AVIF_TRUE;
    }
    return AVIF_TRUE;
}

// ---------------------------------------------------------------------------
// Helper macros / functions

#define BEGIN_STREAM(VARNAME, PTR, SIZE, DIAG, CONTEXT) \
    avifROStream VARNAME;                               \
    avifROData VARNAME##_roData;                        \
    VARNAME##_roData.data = PTR;                        \
    VARNAME##_roData.size = SIZE;                       \
    avifROStreamStart(&VARNAME, &VARNAME##_roData, DIAG, CONTEXT)

// Use this to keep track of whether or not a child box that must be unique (0 or 1 present) has
// been seen yet, when parsing a parent box. If the "seen" bit is already set for a given box when
// it is encountered during parse, an error is thrown. Which bit corresponds to which box is
// dictated entirely by the calling function.
static avifBool uniqueBoxSeen(uint32_t * uniqueBoxFlags, uint32_t whichFlag, const char * parentBoxType, const char * boxType, avifDiagnostics * diagnostics)
{
    const uint32_t flag = 1 << whichFlag;
    if (*uniqueBoxFlags & flag) {
        // This box has already been seen. Error!
        avifDiagnosticsPrintf(diagnostics, "Box[%s] contains a duplicate unique box of type '%s'", parentBoxType, boxType);
        return AVIF_FALSE;
    }

    // Mark this box as seen.
    *uniqueBoxFlags |= flag;
    return AVIF_TRUE;
}

// ---------------------------------------------------------------------------
// avifDecoderData

typedef struct avifTile
{
    avifCodecDecodeInput * input;
    struct avifCodec * codec;
    avifImage * image;
    uint32_t width;  // Either avifTrack.width or avifDecoderItem.width
    uint32_t height; // Either avifTrack.height or avifDecoderItem.height
    uint8_t operatingPoint;
} avifTile;
AVIF_ARRAY_DECLARE(avifTileArray, avifTile, tile);

// This holds one "meta" box (from the BMFF and HEIF standards) worth of relevant-to-AVIF information.
// * If a meta box is parsed from the root level of the BMFF, it can contain the information about
//   "items" which might be color planes, alpha planes, or EXIF or XMP metadata.
// * If a meta box is parsed from inside of a track ("trak") box, any metadata (EXIF/XMP) items inside
//   of that box are implicitly associated with that track.
typedef struct avifMeta
{
    // Items (from HEIF) are the generic storage for any data that does not require timed processing
    // (single image color planes, alpha planes, EXIF, XMP, etc). Each item has a unique integer ID >1,
    // and is defined by a series of child boxes in a meta box:
    //  * iloc - location:     byte offset to item data, item size in bytes
    //  * iinf - information:  type of item (color planes, alpha plane, EXIF, XMP)
    //  * ipco - properties:   dimensions, aspect ratio, image transformations, references to other items
    //  * ipma - associations: Attaches an item in the properties list to a given item
    //
    // Items are lazily created in this array when any of the above boxes refer to one by a new (unseen) ID,
    // and are then further modified/updated as new information for an item's ID is parsed.
    avifDecoderItemArray items;

    // Any ipco boxes explained above are populated into this array as a staging area, which are
    // then duplicated into the appropriate items upon encountering an item property association
    // (ipma) box.
    avifPropertyArray properties;

    // Filled with the contents of this meta box's "idat" box, which is raw data that an item can
    // directly refer to in its item location box (iloc) instead of just giving an offset into the
    // overall file. If all items' iloc boxes simply point at an offset/length in the file itself,
    // this buffer will likely be empty.
    avifRWData idat;

    // Ever-incrementing ID for uniquely identifying which 'meta' box contains an idat (when
    // multiple meta boxes exist as BMFF siblings). Each time avifParseMetaBox() is called on an
    // avifMeta struct, this value is incremented. Any time an additional meta box is detected at
    // the same "level" (root level, trak level, etc), this ID helps distinguish which meta box's
    // "idat" is which, as items implicitly reference idat boxes that exist in the same meta
    // box.
    uint32_t idatID;

    // Contents of a pitm box, which signal which of the items in this file is the main image. For
    // AVIF, this should point at an av01 type item containing color planes, and all other items
    // are ignored unless they refer to this item in some way (alpha plane, EXIF/XMP metadata).
    uint32_t primaryItemID;
} avifMeta;

static void avifMetaDestroy(avifMeta * meta);

static avifMeta * avifMetaCreate()
{
    avifMeta * meta = (avifMeta *)avifAlloc(sizeof(avifMeta));
    memset(meta, 0, sizeof(avifMeta));
    if (!avifArrayCreate(&meta->items, sizeof(avifDecoderItem), 8)) {
        goto error;
    }
    if (!avifArrayCreate(&meta->properties, sizeof(avifProperty), 16)) {
        goto error;
    }
    return meta;

error:
    avifMetaDestroy(meta);
    return NULL;
}

static void avifMetaDestroy(avifMeta * meta)
{
    for (uint32_t i = 0; i < meta->items.count; ++i) {
        avifDecoderItem * item = &meta->items.item[i];
        avifArrayDestroy(&item->properties);
        avifArrayDestroy(&item->extents);
        if (item->ownsMergedExtents) {
            avifRWDataFree(&item->mergedExtents);
        }
    }
    avifArrayDestroy(&meta->items);
    avifArrayDestroy(&meta->properties);
    avifRWDataFree(&meta->idat);
    avifFree(meta);
}

static avifDecoderItem * avifMetaFindItem(avifMeta * meta, uint32_t itemID)
{
    if (itemID == 0) {
        return NULL;
    }

    for (uint32_t i = 0; i < meta->items.count; ++i) {
        if (meta->items.item[i].id == itemID) {
            return &meta->items.item[i];
        }
    }

    avifDecoderItem * item = (avifDecoderItem *)avifArrayPushPtr(&meta->items);
    if (!avifArrayCreate(&item->properties, sizeof(avifProperty), 16)) {
        goto error;
    }
    if (!avifArrayCreate(&item->extents, sizeof(avifExtent), 1)) {
        goto error;
    }
    item->id = itemID;
    item->meta = meta;
    return item;

error:
    avifArrayDestroy(&item->extents);
    avifArrayDestroy(&item->properties);
    avifArrayPop(&meta->items);
    return NULL;
}

typedef struct avifDecoderData
{
    avifMeta * meta; // The root-level meta box
    avifTrackArray tracks;
    avifTileArray tiles;
    unsigned int colorTileCount;
    unsigned int alphaTileCount;
    unsigned int decodedColorTileCount;
    unsigned int decodedAlphaTileCount;
    avifImageGrid colorGrid;
    avifImageGrid alphaGrid;
    avifDecoderSource source;
    uint8_t majorBrand[4];                     // From the file's ftyp, used by AVIF_DECODER_SOURCE_AUTO
    avifDiagnostics * diag;                    // Shallow copy; owned by avifDecoder
    const avifSampleTable * sourceSampleTable; // NULL unless (source == AVIF_DECODER_SOURCE_TRACKS), owned by an avifTrack
    avifBool cicpSet;                          // True if avifDecoder's image has had its CICP set correctly yet.
                                               // This allows nclx colr boxes to override AV1 CICP, as specified in the MIAF
                                               // standard (ISO/IEC 23000-22:2019), section 7.3.6.4:
                                               //
    // "The colour information property takes precedence over any colour information in the image
    // bitstream, i.e. if the property is present, colour information in the bitstream shall be ignored."
} avifDecoderData;

static void avifDecoderDataDestroy(avifDecoderData * data);

static avifDecoderData * avifDecoderDataCreate()
{
    avifDecoderData * data = (avifDecoderData *)avifAlloc(sizeof(avifDecoderData));
    memset(data, 0, sizeof(avifDecoderData));
    data->meta = avifMetaCreate();
    if (!avifArrayCreate(&data->tracks, sizeof(avifTrack), 2)) {
        goto error;
    }
    if (!avifArrayCreate(&data->tiles, sizeof(avifTile), 8)) {
        goto error;
    }
    return data;

error:
    avifDecoderDataDestroy(data);
    return NULL;
}

static void avifDecoderDataResetCodec(avifDecoderData * data)
{
    for (unsigned int i = 0; i < data->tiles.count; ++i) {
        avifTile * tile = &data->tiles.tile[i];
        if (tile->image) {
            avifImageFreePlanes(tile->image, AVIF_PLANES_ALL); // forget any pointers into codec image buffers
        }
        if (tile->codec) {
            avifCodecDestroy(tile->codec);
            tile->codec = NULL;
        }
    }
    data->decodedColorTileCount = 0;
    data->decodedAlphaTileCount = 0;
}

static avifTile * avifDecoderDataCreateTile(avifDecoderData * data, uint32_t width, uint32_t height, uint8_t operatingPoint)
{
    avifTile * tile = (avifTile *)avifArrayPushPtr(&data->tiles);
    tile->image = avifImageCreateEmpty();
    if (!tile->image) {
        goto error;
    }
    tile->input = avifCodecDecodeInputCreate();
    if (!tile->input) {
        goto error;
    }
    tile->width = width;
    tile->height = height;
    tile->operatingPoint = operatingPoint;
    return tile;

error:
    if (tile->input) {
        avifCodecDecodeInputDestroy(tile->input);
    }
    if (tile->image) {
        avifImageDestroy(tile->image);
    }
    avifArrayPop(&data->tiles);
    return NULL;
}

static avifTrack * avifDecoderDataCreateTrack(avifDecoderData * data)
{
    avifTrack * track = (avifTrack *)avifArrayPushPtr(&data->tracks);
    track->meta = avifMetaCreate();
    return track;
}

static void avifDecoderDataClearTiles(avifDecoderData * data)
{
    for (unsigned int i = 0; i < data->tiles.count; ++i) {
        avifTile * tile = &data->tiles.tile[i];
        if (tile->input) {
            avifCodecDecodeInputDestroy(tile->input);
            tile->input = NULL;
        }
        if (tile->codec) {
            avifCodecDestroy(tile->codec);
            tile->codec = NULL;
        }
        if (tile->image) {
            avifImageDestroy(tile->image);
            tile->image = NULL;
        }
    }
    data->tiles.count = 0;
    data->colorTileCount = 0;
    data->alphaTileCount = 0;
    data->decodedColorTileCount = 0;
    data->decodedAlphaTileCount = 0;
}

static void avifDecoderDataDestroy(avifDecoderData * data)
{
    avifMetaDestroy(data->meta);
    for (uint32_t i = 0; i < data->tracks.count; ++i) {
        avifTrack * track = &data->tracks.track[i];
        if (track->sampleTable) {
            avifSampleTableDestroy(track->sampleTable);
        }
        if (track->meta) {
            avifMetaDestroy(track->meta);
        }
    }
    avifArrayDestroy(&data->tracks);
    avifDecoderDataClearTiles(data);
    avifArrayDestroy(&data->tiles);
    avifFree(data);
}

// This returns the max extent that has to be read in order to decode this item. If
// the item is stored in an idat, the data has already been read during Parse() and
// this function will return AVIF_RESULT_OK with a 0-byte extent.
static avifResult avifDecoderItemMaxExtent(const avifDecoderItem * item, const avifDecodeSample * sample, avifExtent * outExtent)
{
    if (item->extents.count == 0) {
        return AVIF_RESULT_TRUNCATED_DATA;
    }

    if (item->idatStored) {
        // construction_method: idat(1)

        if (item->meta->idat.size > 0) {
            // Already read from a meta box during Parse()
            memset(outExtent, 0, sizeof(avifExtent));
            return AVIF_RESULT_OK;
        }

        // no associated idat box was found in the meta box, bail out
        return AVIF_RESULT_NO_CONTENT;
    }

    // construction_method: file(0)

    if (sample->size == 0) {
        return AVIF_RESULT_TRUNCATED_DATA;
    }
    uint64_t remainingOffset = sample->offset;
    size_t remainingBytes = sample->size; // This may be smaller than item->size if the item is progressive

    // Assert that the for loop below will execute at least one iteration.
    assert(item->extents.count != 0);
    uint64_t minOffset = UINT64_MAX;
    uint64_t maxOffset = 0;
    for (uint32_t extentIter = 0; extentIter < item->extents.count; ++extentIter) {
        avifExtent * extent = &item->extents.extent[extentIter];

        // Make local copies of extent->offset and extent->size as they might need to be adjusted
        // due to the sample's offset.
        uint64_t startOffset = extent->offset;
        size_t extentSize = extent->size;
        if (remainingOffset) {
            if (remainingOffset >= extentSize) {
                remainingOffset -= extentSize;
                continue;
            } else {
                if (remainingOffset > UINT64_MAX - startOffset) {
                    return AVIF_RESULT_BMFF_PARSE_FAILED;
                }
                startOffset += remainingOffset;
                extentSize -= remainingOffset;
                remainingOffset = 0;
            }
        }

        const size_t usedExtentSize = (extentSize < remainingBytes) ? extentSize : remainingBytes;

        if (usedExtentSize > UINT64_MAX - startOffset) {
            return AVIF_RESULT_BMFF_PARSE_FAILED;
        }
        const uint64_t endOffset = startOffset + usedExtentSize;

        if (minOffset > startOffset) {
            minOffset = startOffset;
        }
        if (maxOffset < endOffset) {
            maxOffset = endOffset;
        }

        remainingBytes -= usedExtentSize;
        if (remainingBytes == 0) {
            // We've got enough bytes for this sample.
            break;
        }
    }

    if (remainingBytes != 0) {
        return AVIF_RESULT_TRUNCATED_DATA;
    }

    outExtent->offset = minOffset;
    const uint64_t extentLength = maxOffset - minOffset;
    if (extentLength > SIZE_MAX) {
        return AVIF_RESULT_BMFF_PARSE_FAILED;
    }
    outExtent->size = (size_t)extentLength;
    return AVIF_RESULT_OK;
}

static uint8_t avifDecoderItemOperatingPoint(const avifDecoderItem * item)
{
    const avifProperty * a1opProp = avifPropertyArrayFind(&item->properties, "a1op");
    if (a1opProp) {
        return a1opProp->u.a1op.opIndex;
    }
    return 0; // default
}

static avifResult avifDecoderItemValidateAV1(const avifDecoderItem * item, avifDiagnostics * diag, const avifStrictFlags strictFlags)
{
    const avifProperty * av1CProp = avifPropertyArrayFind(&item->properties, "av1C");
    if (!av1CProp) {
        // An av1C box is mandatory in all valid AVIF configurations. Bail out.
        avifDiagnosticsPrintf(diag, "Item ID %u of type '%.4s' is missing mandatory av1C property", item->id, (const char *)item->type);
        return AVIF_RESULT_BMFF_PARSE_FAILED;
    }

    const avifProperty * pixiProp = avifPropertyArrayFind(&item->properties, "pixi");
    if (!pixiProp && (strictFlags & AVIF_STRICT_PIXI_REQUIRED)) {
        // A pixi box is mandatory in all valid AVIF configurations. Bail out.
        avifDiagnosticsPrintf(diag,
                              "[Strict] Item ID %u of type '%.4s' is missing mandatory pixi property",
                              item->id,
                              (const char *)item->type);
        return AVIF_RESULT_BMFF_PARSE_FAILED;
    }

    if (pixiProp) {
        const uint32_t av1CDepth = avifCodecConfigurationBoxGetDepth(&av1CProp->u.av1C);
        for (uint8_t i = 0; i < pixiProp->u.pixi.planeCount; ++i) {
            if (pixiProp->u.pixi.planeDepths[i] != av1CDepth) {
                // pixi depth must match av1C depth
                avifDiagnosticsPrintf(diag,
                                      "Item ID %u depth specified by pixi property [%u] does not match av1C property depth [%u]",
                                      item->id,
                                      pixiProp->u.pixi.planeDepths[i],
                                      av1CDepth);
                return AVIF_RESULT_BMFF_PARSE_FAILED;
            }
        }
    }

    if (strictFlags & AVIF_STRICT_CLAP_VALID) {
        const avifProperty * clapProp = avifPropertyArrayFind(&item->properties, "clap");
        if (clapProp) {
            const avifProperty * ispeProp = avifPropertyArrayFind(&item->properties, "ispe");
            if (!ispeProp) {
                avifDiagnosticsPrintf(diag,
                                      "[Strict] Item ID %u is missing an ispe property, so its clap property cannot be validated",
                                      item->id);
                return AVIF_RESULT_BMFF_PARSE_FAILED;
            }

            avifCropRect cropRect;
            const uint32_t imageW = ispeProp->u.ispe.width;
            const uint32_t imageH = ispeProp->u.ispe.height;
            const avifPixelFormat av1CFormat = avifCodecConfigurationBoxGetFormat(&av1CProp->u.av1C);
            avifBool validClap = avifCropRectConvertCleanApertureBox(&cropRect, &clapProp->u.clap, imageW, imageH, av1CFormat, diag);
            if (!validClap) {
                return AVIF_RESULT_BMFF_PARSE_FAILED;
            }
        }
    }
    return AVIF_RESULT_OK;
}

static avifResult avifDecoderItemRead(avifDecoderItem * item,
                                      avifIO * io,
                                      avifROData * outData,
                                      size_t offset,
                                      size_t partialByteCount,
                                      avifDiagnostics * diag)
{
    if (item->mergedExtents.data && !item->partialMergedExtents) {
        // Multiple extents have already been concatenated for this item, just return it
        if (offset >= item->mergedExtents.size) {
            avifDiagnosticsPrintf(diag, "Item ID %u read has overflowing offset", item->id);
            return AVIF_RESULT_TRUNCATED_DATA;
        }
        outData->data = item->mergedExtents.data + offset;
        outData->size = item->mergedExtents.size - offset;
        return AVIF_RESULT_OK;
    }

    if (item->extents.count == 0) {
        avifDiagnosticsPrintf(diag, "Item ID %u has zero extents", item->id);
        return AVIF_RESULT_TRUNCATED_DATA;
    }

    // Find this item's source of all extents' data, based on the construction method
    const avifRWData * idatBuffer = NULL;
    if (item->idatStored) {
        // construction_method: idat(1)

        if (item->meta->idat.size > 0) {
            idatBuffer = &item->meta->idat;
        } else {
            // no associated idat box was found in the meta box, bail out
            avifDiagnosticsPrintf(diag, "Item ID %u is stored in an idat, but no associated idat box was found", item->id);
            return AVIF_RESULT_NO_CONTENT;
        }
    }

    // Merge extents into a single contiguous buffer
    if ((io->sizeHint > 0) && (item->size > io->sizeHint)) {
        // Sanity check: somehow the sum of extents exceeds the entire file or idat size!
        avifDiagnosticsPrintf(diag, "Item ID %u reported size failed size hint sanity check. Truncated data?", item->id);
        return AVIF_RESULT_TRUNCATED_DATA;
    }

    if (offset >= item->size) {
        avifDiagnosticsPrintf(diag, "Item ID %u read has overflowing offset", item->id);
        return AVIF_RESULT_TRUNCATED_DATA;
    }
    const size_t maxOutputSize = item->size - offset;
    const size_t readOutputSize = (partialByteCount && (partialByteCount < maxOutputSize)) ? partialByteCount : maxOutputSize;
    const size_t totalBytesToRead = offset + readOutputSize;

    // If there is a single extent for this item and the source of the read buffer is going to be
    // persistent for the lifetime of the avifDecoder (whether it comes from its own internal
    // idatBuffer or from a known-persistent IO), we can avoid buffer duplication and just use the
    // preexisting buffer.
    avifBool singlePersistentBuffer = ((item->extents.count == 1) && (idatBuffer || io->persistent));
    if (!singlePersistentBuffer) {
        // Always allocate the item's full size here, as progressive image decodes will do partial
        // reads into this buffer and begin feeding the buffer to the underlying AV1 decoder, but
        // will then write more into this buffer without flushing the AV1 decoder (which is still
        // holding the address of the previous allocation of this buffer). This strategy avoids
        // use-after-free issues in the AV1 decoder and unnecessary reallocs as a typical
        // progressive decode use case will eventually decode the final layer anyway.
        avifRWDataRealloc(&item->mergedExtents, item->size);
        item->ownsMergedExtents = AVIF_TRUE;
    }

    // Set this until we manage to fill the entire mergedExtents buffer
    item->partialMergedExtents = AVIF_TRUE;

    uint8_t * front = item->mergedExtents.data;
    size_t remainingBytes = totalBytesToRead;
    for (uint32_t extentIter = 0; extentIter < item->extents.count; ++extentIter) {
        avifExtent * extent = &item->extents.extent[extentIter];

        size_t bytesToRead = extent->size;
        if (bytesToRead > remainingBytes) {
            bytesToRead = remainingBytes;
        }

        avifROData offsetBuffer;
        if (idatBuffer) {
            if (extent->offset > idatBuffer->size) {
                avifDiagnosticsPrintf(diag, "Item ID %u has impossible extent offset in idat buffer", item->id);
                return AVIF_RESULT_BMFF_PARSE_FAILED;
            }
            // Since extent->offset (a uint64_t) is not bigger than idatBuffer->size (a size_t),
            // it is safe to cast extent->offset to size_t.
            const size_t extentOffset = (size_t)extent->offset;
            if (extent->size > idatBuffer->size - extentOffset) {
                avifDiagnosticsPrintf(diag, "Item ID %u has impossible extent size in idat buffer", item->id);
                return AVIF_RESULT_BMFF_PARSE_FAILED;
            }
            offsetBuffer.data = idatBuffer->data + extentOffset;
            offsetBuffer.size = idatBuffer->size - extentOffset;
        } else {
            // construction_method: file(0)

            if ((io->sizeHint > 0) && (extent->offset > io->sizeHint)) {
                avifDiagnosticsPrintf(diag, "Item ID %u extent offset failed size hint sanity check. Truncated data?", item->id);
                return AVIF_RESULT_BMFF_PARSE_FAILED;
            }
            avifResult readResult = io->read(io, 0, extent->offset, bytesToRead, &offsetBuffer);
            if (readResult != AVIF_RESULT_OK) {
                return readResult;
            }
            if (bytesToRead != offsetBuffer.size) {
                avifDiagnosticsPrintf(diag,
                                      "Item ID %u tried to read %zu bytes, but only received %zu bytes",
                                      item->id,
                                      bytesToRead,
                                      offsetBuffer.size);
                return AVIF_RESULT_TRUNCATED_DATA;
            }
        }

        if (singlePersistentBuffer) {
            memcpy(&item->mergedExtents, &offsetBuffer, sizeof(avifRWData));
            item->mergedExtents.size = bytesToRead;
        } else {
            assert(item->ownsMergedExtents);
            assert(front);
            memcpy(front, offsetBuffer.data, bytesToRead);
            front += bytesToRead;
        }

        remainingBytes -= bytesToRead;
        if (remainingBytes == 0) {
            // This happens when partialByteCount is set
            break;
        }
    }
    if (remainingBytes != 0) {
        // This should be impossible?
        avifDiagnosticsPrintf(diag, "Item ID %u has %zu unexpected trailing bytes", item->id, remainingBytes);
        return AVIF_RESULT_TRUNCATED_DATA;
    }

    outData->data = item->mergedExtents.data + offset;
    outData->size = readOutputSize;
    item->partialMergedExtents = (item->size != totalBytesToRead);
    return AVIF_RESULT_OK;
}

static avifBool avifDecoderGenerateImageGridTiles(avifDecoder * decoder, avifImageGrid * grid, avifDecoderItem * gridItem, avifBool alpha)
{
    unsigned int tilesRequested = grid->rows * grid->columns;

    // Count number of dimg for this item, bail out if it doesn't match perfectly
    unsigned int tilesAvailable = 0;
    for (uint32_t i = 0; i < gridItem->meta->items.count; ++i) {
        avifDecoderItem * item = &gridItem->meta->items.item[i];
        if (item->dimgForID == gridItem->id) {
            if (memcmp(item->type, "av01", 4)) {
                continue;
            }
            if (item->hasUnsupportedEssentialProperty) {
                // An essential property isn't supported by libavif; can't
                // decode a grid image if any tile in the grid isn't supported.
                avifDiagnosticsPrintf(&decoder->diag, "Grid image contains tile with an unsupported property marked as essential");
                return AVIF_FALSE;
            }

            ++tilesAvailable;
        }
    }

    if (tilesRequested != tilesAvailable) {
        avifDiagnosticsPrintf(&decoder->diag,
                              "Grid image of dimensions %ux%u requires %u tiles, and only %u were found",
                              grid->columns,
                              grid->rows,
                              tilesRequested,
                              tilesAvailable);
        return AVIF_FALSE;
    }

    avifBool firstTile = AVIF_TRUE;
    for (uint32_t i = 0; i < gridItem->meta->items.count; ++i) {
        avifDecoderItem * item = &gridItem->meta->items.item[i];
        if (item->dimgForID == gridItem->id) {
            if (memcmp(item->type, "av01", 4)) {
                continue;
            }

            avifTile * tile = avifDecoderDataCreateTile(decoder->data, item->width, item->height, avifDecoderItemOperatingPoint(item));
            if (!tile) {
                return AVIF_FALSE;
            }
            if (!avifCodecDecodeInputFillFromDecoderItem(tile->input,
                                                         item,
                                                         decoder->allowProgressive,
                                                         decoder->imageCountLimit,
                                                         decoder->io->sizeHint,
                                                         &decoder->diag)) {
                return AVIF_FALSE;
            }
            tile->input->alpha = alpha;

            if (firstTile) {
                firstTile = AVIF_FALSE;

                // Adopt the av1C property of the first av01 tile, so that it can be queried from
                // the top-level color/alpha item during avifDecoderReset().
                const avifProperty * srcProp = avifPropertyArrayFind(&item->properties, "av1C");
                if (!srcProp) {
                    avifDiagnosticsPrintf(&decoder->diag, "Grid image's first tile is missing an av1C property");
                    return AVIF_FALSE;
                }
                avifProperty * dstProp = (avifProperty *)avifArrayPushPtr(&gridItem->properties);
                *dstProp = *srcProp;

                if (!alpha && item->progressive) {
                    decoder->progressiveState = AVIF_PROGRESSIVE_STATE_AVAILABLE;
                    if (tile->input->samples.count > 1) {
                        decoder->progressiveState = AVIF_PROGRESSIVE_STATE_ACTIVE;
                        decoder->imageCount = tile->input->samples.count;
                    }
                }
            }
        }
    }
    return AVIF_TRUE;
}

// Checks the grid consistency and copies the pixels from the tiles to the
// dstImage. Only the freshly decoded tiles are considered, skipping the already
// copied or not-yet-decoded tiles.
static avifBool avifDecoderDataFillImageGrid(avifDecoderData * data,
                                             avifImageGrid * grid,
                                             avifImage * dstImage,
                                             unsigned int firstTileIndex,
                                             unsigned int oldDecodedTileCount,
                                             unsigned int decodedTileCount,
                                             avifBool alpha)
{
    assert(decodedTileCount > oldDecodedTileCount);

    avifTile * firstTile = &data->tiles.tile[firstTileIndex];
    avifBool firstTileUVPresent = (firstTile->image->yuvPlanes[AVIF_CHAN_U] && firstTile->image->yuvPlanes[AVIF_CHAN_V]);

    // Check for tile consistency: All tiles in a grid image should match in the properties checked below.
    for (unsigned int i = AVIF_MAX(1, oldDecodedTileCount); i < decodedTileCount; ++i) {
        avifTile * tile = &data->tiles.tile[firstTileIndex + i];
        avifBool uvPresent = (tile->image->yuvPlanes[AVIF_CHAN_U] && tile->image->yuvPlanes[AVIF_CHAN_V]);
        if ((tile->image->width != firstTile->image->width) || (tile->image->height != firstTile->image->height) ||
            (tile->image->depth != firstTile->image->depth) || (tile->image->yuvFormat != firstTile->image->yuvFormat) ||
            (tile->image->yuvRange != firstTile->image->yuvRange) || (uvPresent != firstTileUVPresent) ||
            (tile->image->colorPrimaries != firstTile->image->colorPrimaries) ||
            (tile->image->transferCharacteristics != firstTile->image->transferCharacteristics) ||
            (tile->image->matrixCoefficients != firstTile->image->matrixCoefficients)) {
            avifDiagnosticsPrintf(data->diag, "Grid image contains mismatched tiles");
            return AVIF_FALSE;
        }
    }

    // Validate grid image size and tile size.
    //
    // HEIF (ISO/IEC 23008-12:2017), Section 6.6.2.3.1:
    //   The tiled input images shall completely "cover" the reconstructed image grid canvas, ...
    if (((firstTile->image->width * grid->columns) < grid->outputWidth) ||
        ((firstTile->image->height * grid->rows) < grid->outputHeight)) {
        avifDiagnosticsPrintf(data->diag,
                              "Grid image tiles do not completely cover the image (HEIF (ISO/IEC 23008-12:2017), Section 6.6.2.3.1)");
        return AVIF_FALSE;
    }
    // Tiles in the rightmost column and bottommost row must overlap the reconstructed image grid canvas. See MIAF (ISO/IEC 23000-22:2019), Section 7.3.11.4.2, Figure 2.
    if (((firstTile->image->width * (grid->columns - 1)) >= grid->outputWidth) ||
        ((firstTile->image->height * (grid->rows - 1)) >= grid->outputHeight)) {
        avifDiagnosticsPrintf(data->diag,
                              "Grid image tiles in the rightmost column and bottommost row do not overlap the reconstructed image grid canvas. See MIAF (ISO/IEC 23000-22:2019), Section 7.3.11.4.2, Figure 2");
        return AVIF_FALSE;
    }

    if (alpha) {
        // An alpha tile does not contain any YUV pixels.
        assert(firstTile->image->yuvFormat == AVIF_PIXEL_FORMAT_NONE);
    }
    if (!avifAreGridDimensionsValid(firstTile->image->yuvFormat,
                                    grid->outputWidth,
                                    grid->outputHeight,
                                    firstTile->image->width,
                                    firstTile->image->height,
                                    data->diag)) {
        return AVIF_FALSE;
    }

    // Lazily populate dstImage with the new frame's properties. If we're decoding alpha,
    // these values must already match.
    if ((dstImage->width != grid->outputWidth) || (dstImage->height != grid->outputHeight) ||
        (dstImage->depth != firstTile->image->depth) || (!alpha && (dstImage->yuvFormat != firstTile->image->yuvFormat))) {
        if (alpha) {
            // Alpha doesn't match size, just bail out
            avifDiagnosticsPrintf(data->diag, "Alpha plane dimensions do not match color plane dimensions");
            return AVIF_FALSE;
        }

        avifImageFreePlanes(dstImage, AVIF_PLANES_ALL);
        dstImage->width = grid->outputWidth;
        dstImage->height = grid->outputHeight;
        dstImage->depth = firstTile->image->depth;
        dstImage->yuvFormat = firstTile->image->yuvFormat;
        dstImage->yuvRange = firstTile->image->yuvRange;
        if (!data->cicpSet) {
            data->cicpSet = AVIF_TRUE;
            dstImage->colorPrimaries = firstTile->image->colorPrimaries;
            dstImage->transferCharacteristics = firstTile->image->transferCharacteristics;
            dstImage->matrixCoefficients = firstTile->image->matrixCoefficients;
        }
    }

    if (avifImageAllocatePlanes(dstImage, alpha ? AVIF_PLANES_A : AVIF_PLANES_YUV) != AVIF_RESULT_OK) {
        avifDiagnosticsPrintf(data->diag, "Image allocation failure");
        return AVIF_FALSE;
    }

    avifPixelFormatInfo formatInfo;
    avifGetPixelFormatInfo(firstTile->image->yuvFormat, &formatInfo);

    unsigned int tileIndex = oldDecodedTileCount;
    size_t pixelBytes = avifImageUsesU16(dstImage) ? 2 : 1;
    unsigned int rowIndex = oldDecodedTileCount / grid->columns;
    unsigned int colIndex = oldDecodedTileCount % grid->columns;
    // Only the first iteration of the outer for loop uses this initial value of colIndex.
    // Subsequent iterations of the outer for loop initializes colIndex to 0.
    for (; rowIndex < grid->rows; ++rowIndex, colIndex = 0) {
        for (; colIndex < grid->columns; ++colIndex, ++tileIndex) {
            if (tileIndex >= decodedTileCount) {
                // Tile is not ready yet.
                return AVIF_TRUE;
            }
            avifTile * tile = &data->tiles.tile[firstTileIndex + tileIndex];

            unsigned int widthToCopy = firstTile->image->width;
            unsigned int maxX = firstTile->image->width * (colIndex + 1);
            if (maxX > grid->outputWidth) {
                widthToCopy -= maxX - grid->outputWidth;
            }

            unsigned int heightToCopy = firstTile->image->height;
            unsigned int maxY = firstTile->image->height * (rowIndex + 1);
            if (maxY > grid->outputHeight) {
                heightToCopy -= maxY - grid->outputHeight;
            }

            // Y and A channels
            size_t yaColOffset = (size_t)colIndex * firstTile->image->width;
            size_t yaRowOffset = (size_t)rowIndex * firstTile->image->height;
            size_t yaRowBytes = widthToCopy * pixelBytes;

            if (alpha) {
                // A
                for (unsigned int j = 0; j < heightToCopy; ++j) {
                    uint8_t * src = &tile->image->alphaPlane[j * tile->image->alphaRowBytes];
                    uint8_t * dst = &dstImage->alphaPlane[(yaColOffset * pixelBytes) + ((yaRowOffset + j) * dstImage->alphaRowBytes)];
                    memcpy(dst, src, yaRowBytes);
                }
            } else {
                // Y
                for (unsigned int j = 0; j < heightToCopy; ++j) {
                    uint8_t * src = &tile->image->yuvPlanes[AVIF_CHAN_Y][j * tile->image->yuvRowBytes[AVIF_CHAN_Y]];
                    uint8_t * dst =
                        &dstImage->yuvPlanes[AVIF_CHAN_Y][(yaColOffset * pixelBytes) + ((yaRowOffset + j) * dstImage->yuvRowBytes[AVIF_CHAN_Y])];
                    memcpy(dst, src, yaRowBytes);
                }

                if (!firstTileUVPresent) {
                    continue;
                }

                // UV
                heightToCopy >>= formatInfo.chromaShiftY;
                size_t uvColOffset = yaColOffset >> formatInfo.chromaShiftX;
                size_t uvRowOffset = yaRowOffset >> formatInfo.chromaShiftY;
                size_t uvRowBytes = yaRowBytes >> formatInfo.chromaShiftX;
                for (unsigned int j = 0; j < heightToCopy; ++j) {
                    uint8_t * srcU = &tile->image->yuvPlanes[AVIF_CHAN_U][j * tile->image->yuvRowBytes[AVIF_CHAN_U]];
                    uint8_t * dstU =
                        &dstImage->yuvPlanes[AVIF_CHAN_U][(uvColOffset * pixelBytes) + ((uvRowOffset + j) * dstImage->yuvRowBytes[AVIF_CHAN_U])];
                    memcpy(dstU, srcU, uvRowBytes);

                    uint8_t * srcV = &tile->image->yuvPlanes[AVIF_CHAN_V][j * tile->image->yuvRowBytes[AVIF_CHAN_V]];
                    uint8_t * dstV =
                        &dstImage->yuvPlanes[AVIF_CHAN_V][(uvColOffset * pixelBytes) + ((uvRowOffset + j) * dstImage->yuvRowBytes[AVIF_CHAN_V])];
                    memcpy(dstV, srcV, uvRowBytes);
                }
            }
        }
    }

    return AVIF_TRUE;
}

// If colorId == 0 (a sentinel value as item IDs must be nonzero), accept any found EXIF/XMP metadata. Passing in 0
// is used when finding metadata in a meta box embedded in a trak box, as any items inside of a meta box that is
// inside of a trak box are implicitly associated to the track.
static avifResult avifDecoderFindMetadata(avifDecoder * decoder, avifMeta * meta, avifImage * image, uint32_t colorId)
{
    if (decoder->ignoreExif && decoder->ignoreXMP) {
        // Nothing to do!
        return AVIF_RESULT_OK;
    }

    for (uint32_t itemIndex = 0; itemIndex < meta->items.count; ++itemIndex) {
        avifDecoderItem * item = &meta->items.item[itemIndex];
        if (!item->size) {
            continue;
        }
        if (item->hasUnsupportedEssentialProperty) {
            // An essential property isn't supported by libavif; ignore the item.
            continue;
        }

        if ((colorId > 0) && (item->descForID != colorId)) {
            // Not a content description (metadata) for the colorOBU, skip it
            continue;
        }

        if (!decoder->ignoreExif && !memcmp(item->type, "Exif", 4)) {
            avifROData exifContents;
            avifResult readResult = avifDecoderItemRead(item, decoder->io, &exifContents, 0, 0, &decoder->diag);
            if (readResult != AVIF_RESULT_OK) {
                return readResult;
            }

            // Advance past Annex A.2.1's header
            BEGIN_STREAM(exifBoxStream, exifContents.data, exifContents.size, &decoder->diag, "Exif header");
            uint32_t exifTiffHeaderOffset;
            AVIF_CHECKERR(avifROStreamReadU32(&exifBoxStream, &exifTiffHeaderOffset),
                          AVIF_RESULT_BMFF_PARSE_FAILED); // unsigned int(32) exif_tiff_header_offset;

            avifRWDataSet(&image->exif, avifROStreamCurrent(&exifBoxStream), avifROStreamRemainingBytes(&exifBoxStream));
        } else if (!decoder->ignoreXMP && !memcmp(item->type, "mime", 4) &&
                   !memcmp(item->contentType.contentType, xmpContentType, xmpContentTypeSize)) {
            avifROData xmpContents;
            avifResult readResult = avifDecoderItemRead(item, decoder->io, &xmpContents, 0, 0, &decoder->diag);
            if (readResult != AVIF_RESULT_OK) {
                return readResult;
            }

            avifImageSetMetadataXMP(image, xmpContents.data, xmpContents.size);
        }
    }
    return AVIF_RESULT_OK;
}

// ---------------------------------------------------------------------------
// URN

static avifBool isAlphaURN(const char * urn)
{
    return !strcmp(urn, AVIF_URN_ALPHA0) || !strcmp(urn, AVIF_URN_ALPHA1);
}

// ---------------------------------------------------------------------------
// BMFF Parsing

static avifBool avifParseHandlerBox(const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[hdlr]");

    AVIF_CHECK(avifROStreamReadAndEnforceVersion(&s, 0));

    uint32_t predefined;
    AVIF_CHECK(avifROStreamReadU32(&s, &predefined)); // unsigned int(32) pre_defined = 0;
    if (predefined != 0) {
        avifDiagnosticsPrintf(diag, "Box[hdlr] contains a pre_defined value that is nonzero");
        return AVIF_FALSE;
    }

    uint8_t handlerType[4];
    AVIF_CHECK(avifROStreamRead(&s, handlerType, 4)); // unsigned int(32) handler_type;
    if (memcmp(handlerType, "pict", 4) != 0) {
        avifDiagnosticsPrintf(diag, "Box[hdlr] handler_type is not 'pict'");
        return AVIF_FALSE;
    }

    for (int i = 0; i < 3; ++i) {
        uint32_t reserved;
        AVIF_CHECK(avifROStreamReadU32(&s, &reserved)); // const unsigned int(32)[3] reserved = 0;
    }

    // Verify that a valid string is here, but don't bother to store it
    AVIF_CHECK(avifROStreamReadString(&s, NULL, 0)); // string name;
    return AVIF_TRUE;
}

static avifBool avifParseItemLocationBox(avifMeta * meta, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[iloc]");

    uint8_t version;
    AVIF_CHECK(avifROStreamReadVersionAndFlags(&s, &version, NULL));
    if (version > 2) {
        avifDiagnosticsPrintf(diag, "Box[iloc] has an unsupported version [%u]", version);
        return AVIF_FALSE;
    }

    uint8_t offsetSizeAndLengthSize;
    AVIF_CHECK(avifROStreamRead(&s, &offsetSizeAndLengthSize, 1));
    uint8_t offsetSize = (offsetSizeAndLengthSize >> 4) & 0xf; // unsigned int(4) offset_size;
    uint8_t lengthSize = (offsetSizeAndLengthSize >> 0) & 0xf; // unsigned int(4) length_size;

    uint8_t baseOffsetSizeAndIndexSize;
    AVIF_CHECK(avifROStreamRead(&s, &baseOffsetSizeAndIndexSize, 1));
    uint8_t baseOffsetSize = (baseOffsetSizeAndIndexSize >> 4) & 0xf; // unsigned int(4) base_offset_size;
    uint8_t indexSize = 0;
    if ((version == 1) || (version == 2)) {
        indexSize = baseOffsetSizeAndIndexSize & 0xf; // unsigned int(4) index_size;
        if (indexSize != 0) {
            // extent_index unsupported
            avifDiagnosticsPrintf(diag, "Box[iloc] has an unsupported extent_index");
            return AVIF_FALSE;
        }
    }

    uint16_t tmp16;
    uint32_t itemCount;
    if (version < 2) {
        AVIF_CHECK(avifROStreamReadU16(&s, &tmp16)); // unsigned int(16) item_count;
        itemCount = tmp16;
    } else {
        AVIF_CHECK(avifROStreamReadU32(&s, &itemCount)); // unsigned int(32) item_count;
    }
    for (uint32_t i = 0; i < itemCount; ++i) {
        uint32_t itemID;
        if (version < 2) {
            AVIF_CHECK(avifROStreamReadU16(&s, &tmp16)); // unsigned int(16) item_ID;
            itemID = tmp16;
        } else {
            AVIF_CHECK(avifROStreamReadU32(&s, &itemID)); // unsigned int(32) item_ID;
        }

        avifDecoderItem * item = avifMetaFindItem(meta, itemID);
        if (!item) {
            avifDiagnosticsPrintf(diag, "Box[iloc] has an invalid item ID [%u]", itemID);
            return AVIF_FALSE;
        }
        if (item->extents.count > 0) {
            // This item has already been given extents via this iloc box. This is invalid.
            avifDiagnosticsPrintf(diag, "Item ID [%u] contains duplicate sets of extents", itemID);
            return AVIF_FALSE;
        }

        if ((version == 1) || (version == 2)) {
            uint8_t ignored;
            uint8_t constructionMethod;
            AVIF_CHECK(avifROStreamRead(&s, &ignored, 1));            // unsigned int(12) reserved = 0;
            AVIF_CHECK(avifROStreamRead(&s, &constructionMethod, 1)); // unsigned int(4) construction_method;
            constructionMethod = constructionMethod & 0xf;
            if ((constructionMethod != 0 /* file */) && (constructionMethod != 1 /* idat */)) {
                // construction method item(2) unsupported
                avifDiagnosticsPrintf(diag, "Box[iloc] has an unsupported construction method [%u]", constructionMethod);
                return AVIF_FALSE;
            }
            if (constructionMethod == 1) {
                item->idatStored = AVIF_TRUE;
            }
        }

        uint16_t dataReferenceIndex;                                      // unsigned int(16) data_ref rence_index;
        AVIF_CHECK(avifROStreamReadU16(&s, &dataReferenceIndex));         //
        uint64_t baseOffset;                                              // unsigned int(base_offset_size*8) base_offset;
        AVIF_CHECK(avifROStreamReadUX8(&s, &baseOffset, baseOffsetSize)); //
        uint16_t extentCount;                                             // unsigned int(16) extent_count;
        AVIF_CHECK(avifROStreamReadU16(&s, &extentCount));                //
        for (int extentIter = 0; extentIter < extentCount; ++extentIter) {
            // If extent_index is ever supported, this spec must be implemented here:
            // ::  if (((version == 1) || (version == 2)) && (index_size > 0)) {
            // ::      unsigned int(index_size*8) extent_index;
            // ::  }

            uint64_t extentOffset; // unsigned int(offset_size*8) extent_offset;
            AVIF_CHECK(avifROStreamReadUX8(&s, &extentOffset, offsetSize));
            uint64_t extentLength; // unsigned int(offset_size*8) extent_length;
            AVIF_CHECK(avifROStreamReadUX8(&s, &extentLength, lengthSize));

            avifExtent * extent = (avifExtent *)avifArrayPushPtr(&item->extents);
            if (extentOffset > UINT64_MAX - baseOffset) {
                avifDiagnosticsPrintf(diag,
                                      "Item ID [%u] contains an extent offset which overflows: [base: %" PRIu64 " offset:%" PRIu64 "]",
                                      itemID,
                                      baseOffset,
                                      extentOffset);
                return AVIF_FALSE;
            }
            uint64_t offset = baseOffset + extentOffset;
            extent->offset = offset;
            if (extentLength > SIZE_MAX) {
                avifDiagnosticsPrintf(diag, "Item ID [%u] contains an extent length which overflows: [%" PRIu64 "]", itemID, extentLength);
                return AVIF_FALSE;
            }
            extent->size = (size_t)extentLength;
            if (extent->size > SIZE_MAX - item->size) {
                avifDiagnosticsPrintf(diag,
                                      "Item ID [%u] contains an extent length which overflows the item size: [%zu, %zu]",
                                      itemID,
                                      extent->size,
                                      item->size);
                return AVIF_FALSE;
            }
            item->size += extent->size;
        }
    }
    return AVIF_TRUE;
}

static avifBool avifParseImageGridBox(avifImageGrid * grid,
                                      const uint8_t * raw,
                                      size_t rawLen,
                                      uint32_t imageSizeLimit,
                                      uint32_t imageDimensionLimit,
                                      avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[grid]");

    uint8_t version, flags;
    AVIF_CHECK(avifROStreamRead(&s, &version, 1)); // unsigned int(8) version = 0;
    if (version != 0) {
        avifDiagnosticsPrintf(diag, "Box[grid] has unsupported version [%u]", version);
        return AVIF_FALSE;
    }
    uint8_t rowsMinusOne, columnsMinusOne;
    AVIF_CHECK(avifROStreamRead(&s, &flags, 1));           // unsigned int(8) flags;
    AVIF_CHECK(avifROStreamRead(&s, &rowsMinusOne, 1));    // unsigned int(8) rows_minus_one;
    AVIF_CHECK(avifROStreamRead(&s, &columnsMinusOne, 1)); // unsigned int(8) columns_minus_one;
    grid->rows = (uint32_t)rowsMinusOne + 1;
    grid->columns = (uint32_t)columnsMinusOne + 1;

    uint32_t fieldLength = ((flags & 1) + 1) * 16;
    if (fieldLength == 16) {
        uint16_t outputWidth16, outputHeight16;
        AVIF_CHECK(avifROStreamReadU16(&s, &outputWidth16));  // unsigned int(FieldLength) output_width;
        AVIF_CHECK(avifROStreamReadU16(&s, &outputHeight16)); // unsigned int(FieldLength) output_height;
        grid->outputWidth = outputWidth16;
        grid->outputHeight = outputHeight16;
    } else {
        if (fieldLength != 32) {
            // This should be impossible
            avifDiagnosticsPrintf(diag, "Grid box contains illegal field length: [%u]", fieldLength);
            return AVIF_FALSE;
        }
        AVIF_CHECK(avifROStreamReadU32(&s, &grid->outputWidth));  // unsigned int(FieldLength) output_width;
        AVIF_CHECK(avifROStreamReadU32(&s, &grid->outputHeight)); // unsigned int(FieldLength) output_height;
    }
    if ((grid->outputWidth == 0) || (grid->outputHeight == 0)) {
        avifDiagnosticsPrintf(diag, "Grid box contains illegal dimensions: [%u x %u]", grid->outputWidth, grid->outputHeight);
        return AVIF_FALSE;
    }
    if (avifDimensionsTooLarge(grid->outputWidth, grid->outputHeight, imageSizeLimit, imageDimensionLimit)) {
        avifDiagnosticsPrintf(diag, "Grid box dimensions are too large: [%u x %u]", grid->outputWidth, grid->outputHeight);
        return AVIF_FALSE;
    }
    return avifROStreamRemainingBytes(&s) == 0;
}

static avifBool avifParseImageSpatialExtentsProperty(avifProperty * prop, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[ispe]");
    AVIF_CHECK(avifROStreamReadAndEnforceVersion(&s, 0));

    avifImageSpatialExtents * ispe = &prop->u.ispe;
    AVIF_CHECK(avifROStreamReadU32(&s, &ispe->width));
    AVIF_CHECK(avifROStreamReadU32(&s, &ispe->height));
    return AVIF_TRUE;
}

static avifBool avifParseAuxiliaryTypeProperty(avifProperty * prop, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[auxC]");
    AVIF_CHECK(avifROStreamReadAndEnforceVersion(&s, 0));

    AVIF_CHECK(avifROStreamReadString(&s, prop->u.auxC.auxType, AUXTYPE_SIZE));
    return AVIF_TRUE;
}

static avifBool avifParseColourInformationBox(avifProperty * prop, uint64_t rawOffset, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[colr]");

    avifColourInformationBox * colr = &prop->u.colr;
    colr->hasICC = AVIF_FALSE;
    colr->hasNCLX = AVIF_FALSE;

    uint8_t colorType[4]; // unsigned int(32) colour_type;
    AVIF_CHECK(avifROStreamRead(&s, colorType, 4));
    if (!memcmp(colorType, "rICC", 4) || !memcmp(colorType, "prof", 4)) {
        colr->hasICC = AVIF_TRUE;
        // Remember the offset of the ICC payload relative to the beginning of the stream. A direct pointer cannot be stored
        // because decoder->io->persistent could have been AVIF_FALSE when obtaining raw through decoder->io->read().
        // The bytes could be copied now instead of remembering the offset, but it is as invasive as passing rawOffset everywhere.
        colr->iccOffset = rawOffset + avifROStreamOffset(&s);
        colr->iccSize = avifROStreamRemainingBytes(&s);
    } else if (!memcmp(colorType, "nclx", 4)) {
        AVIF_CHECK(avifROStreamReadU16(&s, &colr->colorPrimaries));          // unsigned int(16) colour_primaries;
        AVIF_CHECK(avifROStreamReadU16(&s, &colr->transferCharacteristics)); // unsigned int(16) transfer_characteristics;
        AVIF_CHECK(avifROStreamReadU16(&s, &colr->matrixCoefficients));      // unsigned int(16) matrix_coefficients;
        // unsigned int(1) full_range_flag;
        // unsigned int(7) reserved = 0;
        uint8_t tmp8;
        AVIF_CHECK(avifROStreamRead(&s, &tmp8, 1));
        colr->range = (tmp8 & 0x80) ? AVIF_RANGE_FULL : AVIF_RANGE_LIMITED;
        colr->hasNCLX = AVIF_TRUE;
    }
    return AVIF_TRUE;
}

static avifBool avifParseAV1CodecConfigurationBox(const uint8_t * raw, size_t rawLen, avifCodecConfigurationBox * av1C, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[av1C]");

    uint8_t markerAndVersion = 0;
    AVIF_CHECK(avifROStreamRead(&s, &markerAndVersion, 1));
    uint8_t seqProfileAndIndex = 0;
    AVIF_CHECK(avifROStreamRead(&s, &seqProfileAndIndex, 1));
    uint8_t rawFlags = 0;
    AVIF_CHECK(avifROStreamRead(&s, &rawFlags, 1));

    if (markerAndVersion != 0x81) {
        // Marker and version must both == 1
        avifDiagnosticsPrintf(diag, "av1C contains illegal marker and version pair: [%u]", markerAndVersion);
        return AVIF_FALSE;
    }

    av1C->seqProfile = (seqProfileAndIndex >> 5) & 0x7;    // unsigned int (3) seq_profile;
    av1C->seqLevelIdx0 = (seqProfileAndIndex >> 0) & 0x1f; // unsigned int (5) seq_level_idx_0;
    av1C->seqTier0 = (rawFlags >> 7) & 0x1;                // unsigned int (1) seq_tier_0;
    av1C->highBitdepth = (rawFlags >> 6) & 0x1;            // unsigned int (1) high_bitdepth;
    av1C->twelveBit = (rawFlags >> 5) & 0x1;               // unsigned int (1) twelve_bit;
    av1C->monochrome = (rawFlags >> 4) & 0x1;              // unsigned int (1) monochrome;
    av1C->chromaSubsamplingX = (rawFlags >> 3) & 0x1;      // unsigned int (1) chroma_subsampling_x;
    av1C->chromaSubsamplingY = (rawFlags >> 2) & 0x1;      // unsigned int (1) chroma_subsampling_y;
    av1C->chromaSamplePosition = (rawFlags >> 0) & 0x3;    // unsigned int (2) chroma_sample_position;
    return AVIF_TRUE;
}

static avifBool avifParseAV1CodecConfigurationBoxProperty(avifProperty * prop, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    return avifParseAV1CodecConfigurationBox(raw, rawLen, &prop->u.av1C, diag);
}

static avifBool avifParsePixelAspectRatioBoxProperty(avifProperty * prop, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[pasp]");

    avifPixelAspectRatioBox * pasp = &prop->u.pasp;
    AVIF_CHECK(avifROStreamReadU32(&s, &pasp->hSpacing)); // unsigned int(32) hSpacing;
    AVIF_CHECK(avifROStreamReadU32(&s, &pasp->vSpacing)); // unsigned int(32) vSpacing;
    return AVIF_TRUE;
}

static avifBool avifParseCleanApertureBoxProperty(avifProperty * prop, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[clap]");

    avifCleanApertureBox * clap = &prop->u.clap;
    AVIF_CHECK(avifROStreamReadU32(&s, &clap->widthN));    // unsigned int(32) cleanApertureWidthN;
    AVIF_CHECK(avifROStreamReadU32(&s, &clap->widthD));    // unsigned int(32) cleanApertureWidthD;
    AVIF_CHECK(avifROStreamReadU32(&s, &clap->heightN));   // unsigned int(32) cleanApertureHeightN;
    AVIF_CHECK(avifROStreamReadU32(&s, &clap->heightD));   // unsigned int(32) cleanApertureHeightD;
    AVIF_CHECK(avifROStreamReadU32(&s, &clap->horizOffN)); // unsigned int(32) horizOffN;
    AVIF_CHECK(avifROStreamReadU32(&s, &clap->horizOffD)); // unsigned int(32) horizOffD;
    AVIF_CHECK(avifROStreamReadU32(&s, &clap->vertOffN));  // unsigned int(32) vertOffN;
    AVIF_CHECK(avifROStreamReadU32(&s, &clap->vertOffD));  // unsigned int(32) vertOffD;
    return AVIF_TRUE;
}

static avifBool avifParseImageRotationProperty(avifProperty * prop, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[irot]");

    avifImageRotation * irot = &prop->u.irot;
    AVIF_CHECK(avifROStreamRead(&s, &irot->angle, 1)); // unsigned int (6) reserved = 0; unsigned int (2) angle;
    if ((irot->angle & 0xfc) != 0) {
        // reserved bits must be 0
        avifDiagnosticsPrintf(diag, "Box[irot] contains nonzero reserved bits [%u]", irot->angle);
        return AVIF_FALSE;
    }
    return AVIF_TRUE;
}

static avifBool avifParseImageMirrorProperty(avifProperty * prop, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[imir]");

    avifImageMirror * imir = &prop->u.imir;
    AVIF_CHECK(avifROStreamRead(&s, &imir->mode, 1)); // unsigned int (7) reserved = 0; unsigned int (1) mode;
    if ((imir->mode & 0xfe) != 0) {
        // reserved bits must be 0
        avifDiagnosticsPrintf(diag, "Box[imir] contains nonzero reserved bits [%u]", imir->mode);
        return AVIF_FALSE;
    }
    return AVIF_TRUE;
}

static avifBool avifParsePixelInformationProperty(avifProperty * prop, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[pixi]");
    AVIF_CHECK(avifROStreamReadAndEnforceVersion(&s, 0));

    avifPixelInformationProperty * pixi = &prop->u.pixi;
    AVIF_CHECK(avifROStreamRead(&s, &pixi->planeCount, 1)); // unsigned int (8) num_channels;
    if (pixi->planeCount > MAX_PIXI_PLANE_DEPTHS) {
        avifDiagnosticsPrintf(diag, "Box[pixi] contains unsupported plane count [%u]", pixi->planeCount);
        return AVIF_FALSE;
    }
    for (uint8_t i = 0; i < pixi->planeCount; ++i) {
        AVIF_CHECK(avifROStreamRead(&s, &pixi->planeDepths[i], 1)); // unsigned int (8) bits_per_channel;
    }
    return AVIF_TRUE;
}

static avifBool avifParseOperatingPointSelectorProperty(avifProperty * prop, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[a1op]");

    avifOperatingPointSelectorProperty * a1op = &prop->u.a1op;
    AVIF_CHECK(avifROStreamRead(&s, &a1op->opIndex, 1));
    if (a1op->opIndex > 31) { // 31 is AV1's max operating point value
        avifDiagnosticsPrintf(diag, "Box[a1op] contains an unsupported operating point [%u]", a1op->opIndex);
        return AVIF_FALSE;
    }
    return AVIF_TRUE;
}

static avifBool avifParseLayerSelectorProperty(avifProperty * prop, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[lsel]");

    avifLayerSelectorProperty * lsel = &prop->u.lsel;
    AVIF_CHECK(avifROStreamReadU16(&s, &lsel->layerID));
    if ((lsel->layerID != 0xFFFF) && (lsel->layerID >= MAX_AV1_LAYER_COUNT)) {
        avifDiagnosticsPrintf(diag, "Box[lsel] contains an unsupported layer [%u]", lsel->layerID);
        return AVIF_FALSE;
    }
    return AVIF_TRUE;
}

static avifBool avifParseAV1LayeredImageIndexingProperty(avifProperty * prop, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[a1lx]");

    avifAV1LayeredImageIndexingProperty * a1lx = &prop->u.a1lx;

    uint8_t largeSize = 0;
    AVIF_CHECK(avifROStreamRead(&s, &largeSize, 1));
    if (largeSize & 0xFE) {
        avifDiagnosticsPrintf(diag, "Box[a1lx] has bits set in the reserved section [%u]", largeSize);
        return AVIF_FALSE;
    }

    for (int i = 0; i < 3; ++i) {
        if (largeSize) {
            AVIF_CHECK(avifROStreamReadU32(&s, &a1lx->layerSize[i]));
        } else {
            uint16_t layerSize16;
            AVIF_CHECK(avifROStreamReadU16(&s, &layerSize16));
            a1lx->layerSize[i] = (uint32_t)layerSize16;
        }
    }

    // Layer sizes will be validated layer (when the item's size is known)
    return AVIF_TRUE;
}

static avifBool avifParseItemPropertyContainerBox(avifPropertyArray * properties,
                                                  uint64_t rawOffset,
                                                  const uint8_t * raw,
                                                  size_t rawLen,
                                                  avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[ipco]");

    while (avifROStreamHasBytesLeft(&s, 1)) {
        avifBoxHeader header;
        AVIF_CHECK(avifROStreamReadBoxHeader(&s, &header));

        int propertyIndex = avifArrayPushIndex(properties);
        avifProperty * prop = &properties->prop[propertyIndex];
        memcpy(prop->type, header.type, 4);
        if (!memcmp(header.type, "ispe", 4)) {
            AVIF_CHECK(avifParseImageSpatialExtentsProperty(prop, avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "auxC", 4)) {
            AVIF_CHECK(avifParseAuxiliaryTypeProperty(prop, avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "colr", 4)) {
            AVIF_CHECK(avifParseColourInformationBox(prop, rawOffset + avifROStreamOffset(&s), avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "av1C", 4)) {
            AVIF_CHECK(avifParseAV1CodecConfigurationBoxProperty(prop, avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "pasp", 4)) {
            AVIF_CHECK(avifParsePixelAspectRatioBoxProperty(prop, avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "clap", 4)) {
            AVIF_CHECK(avifParseCleanApertureBoxProperty(prop, avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "irot", 4)) {
            AVIF_CHECK(avifParseImageRotationProperty(prop, avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "imir", 4)) {
            AVIF_CHECK(avifParseImageMirrorProperty(prop, avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "pixi", 4)) {
            AVIF_CHECK(avifParsePixelInformationProperty(prop, avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "a1op", 4)) {
            AVIF_CHECK(avifParseOperatingPointSelectorProperty(prop, avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "lsel", 4)) {
            AVIF_CHECK(avifParseLayerSelectorProperty(prop, avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "a1lx", 4)) {
            AVIF_CHECK(avifParseAV1LayeredImageIndexingProperty(prop, avifROStreamCurrent(&s), header.size, diag));
        }

        AVIF_CHECK(avifROStreamSkip(&s, header.size));
    }
    return AVIF_TRUE;
}

static avifBool avifParseItemPropertyAssociation(avifMeta * meta, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag, uint32_t * outVersionAndFlags)
{
    // NOTE: If this function ever adds support for versions other than [0,1] or flags other than
    //       [0,1], please increase the value of MAX_IPMA_VERSION_AND_FLAGS_SEEN accordingly.

    BEGIN_STREAM(s, raw, rawLen, diag, "Box[ipma]");

    uint8_t version;
    uint32_t flags;
    AVIF_CHECK(avifROStreamReadVersionAndFlags(&s, &version, &flags));
    avifBool propertyIndexIsU16 = ((flags & 0x1) != 0);
    *outVersionAndFlags = ((uint32_t)version << 24) | flags;

    uint32_t entryCount;
    AVIF_CHECK(avifROStreamReadU32(&s, &entryCount));
    unsigned int prevItemID = 0;
    for (uint32_t entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
        // ISO/IEC 23008-12, First edition, 2017-12, Section 9.3.1:
        //   Each ItemPropertyAssociation box shall be ordered by increasing item_ID, and there shall
        //   be at most one association box for each item_ID, in any ItemPropertyAssociation box.
        unsigned int itemID;
        if (version < 1) {
            uint16_t tmp;
            AVIF_CHECK(avifROStreamReadU16(&s, &tmp));
            itemID = tmp;
        } else {
            AVIF_CHECK(avifROStreamReadU32(&s, &itemID));
        }
        if (itemID <= prevItemID) {
            avifDiagnosticsPrintf(diag, "Box[ipma] item IDs are not ordered by increasing ID");
            return AVIF_FALSE;
        }
        prevItemID = itemID;

        avifDecoderItem * item = avifMetaFindItem(meta, itemID);
        if (!item) {
            avifDiagnosticsPrintf(diag, "Box[ipma] has an invalid item ID [%u]", itemID);
            return AVIF_FALSE;
        }
        if (item->ipmaSeen) {
            avifDiagnosticsPrintf(diag, "Duplicate Box[ipma] for item ID [%u]", itemID);
            return AVIF_FALSE;
        }
        item->ipmaSeen = AVIF_TRUE;

        uint8_t associationCount;
        AVIF_CHECK(avifROStreamRead(&s, &associationCount, 1));
        for (uint8_t associationIndex = 0; associationIndex < associationCount; ++associationIndex) {
            avifBool essential = AVIF_FALSE;
            uint16_t propertyIndex = 0;
            if (propertyIndexIsU16) {
                AVIF_CHECK(avifROStreamReadU16(&s, &propertyIndex));
                essential = ((propertyIndex & 0x8000) != 0);
                propertyIndex &= 0x7fff;
            } else {
                uint8_t tmp;
                AVIF_CHECK(avifROStreamRead(&s, &tmp, 1));
                essential = ((tmp & 0x80) != 0);
                propertyIndex = tmp & 0x7f;
            }

            if (propertyIndex == 0) {
                // Not associated with any item
                continue;
            }
            --propertyIndex; // 1-indexed

            if (propertyIndex >= meta->properties.count) {
                avifDiagnosticsPrintf(diag,
                                      "Box[ipma] for item ID [%u] contains an illegal property index [%u] (out of [%u] properties)",
                                      itemID,
                                      propertyIndex,
                                      meta->properties.count);
                return AVIF_FALSE;
            }

            // Copy property to item
            const avifProperty * srcProp = &meta->properties.prop[propertyIndex];

            static const char * supportedTypes[] = { "ispe", "auxC", "colr", "av1C", "pasp", "clap",
                                                     "irot", "imir", "pixi", "a1op", "lsel", "a1lx" };
            size_t supportedTypesCount = sizeof(supportedTypes) / sizeof(supportedTypes[0]);
            avifBool supportedType = AVIF_FALSE;
            for (size_t i = 0; i < supportedTypesCount; ++i) {
                if (!memcmp(srcProp->type, supportedTypes[i], 4)) {
                    supportedType = AVIF_TRUE;
                    break;
                }
            }
            if (supportedType) {
                if (essential) {
                    // Verify that it is legal for this property to be flagged as essential. Any
                    // types in this list are *required* in the spec to not be flagged as essential
                    // when associated with an item.
                    static const char * const nonessentialTypes[] = {

                        // AVIF: Section 2.3.2.3.2: "If associated, it shall not be marked as essential."
                        "a1lx"

                    };
                    size_t nonessentialTypesCount = sizeof(nonessentialTypes) / sizeof(nonessentialTypes[0]);
                    for (size_t i = 0; i < nonessentialTypesCount; ++i) {
                        if (!memcmp(srcProp->type, nonessentialTypes[i], 4)) {
                            avifDiagnosticsPrintf(diag,
                                                  "Item ID [%u] has a %s property association which must not be marked essential, but is",
                                                  itemID,
                                                  nonessentialTypes[i]);
                            return AVIF_FALSE;
                        }
                    }
                } else {
                    // Verify that it is legal for this property to not be flagged as essential. Any
                    // types in this list are *required* in the spec to be flagged as essential when
                    // associated with an item.
                    static const char * const essentialTypes[] = {

                        // AVIF: Section 2.3.2.1.1: "If associated, it shall be marked as essential."
                        "a1op",

                        // HEIF: Section 6.5.11.1: "essential shall be equal to 1 for an 'lsel' item property."
                        "lsel"

                    };
                    size_t essentialTypesCount = sizeof(essentialTypes) / sizeof(essentialTypes[0]);
                    for (size_t i = 0; i < essentialTypesCount; ++i) {
                        if (!memcmp(srcProp->type, essentialTypes[i], 4)) {
                            avifDiagnosticsPrintf(diag,
                                                  "Item ID [%u] has a %s property association which must be marked essential, but is not",
                                                  itemID,
                                                  essentialTypes[i]);
                            return AVIF_FALSE;
                        }
                    }
                }

                // Supported and valid; associate it with this item.
                avifProperty * dstProp = (avifProperty *)avifArrayPushPtr(&item->properties);
                *dstProp = *srcProp;
            } else {
                if (essential) {
                    // Discovered an essential item property that libavif doesn't support!
                    // Make a note to ignore this item later.
                    item->hasUnsupportedEssentialProperty = AVIF_TRUE;
                }
            }
        }
    }
    return AVIF_TRUE;
}

static avifBool avifParsePrimaryItemBox(avifMeta * meta, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    if (meta->primaryItemID > 0) {
        // Illegal to have multiple pitm boxes, bail out
        avifDiagnosticsPrintf(diag, "Multiple boxes of unique Box[pitm] found");
        return AVIF_FALSE;
    }

    BEGIN_STREAM(s, raw, rawLen, diag, "Box[pitm]");

    uint8_t version;
    AVIF_CHECK(avifROStreamReadVersionAndFlags(&s, &version, NULL));

    if (version == 0) {
        uint16_t tmp16;
        AVIF_CHECK(avifROStreamReadU16(&s, &tmp16)); // unsigned int(16) item_ID;
        meta->primaryItemID = tmp16;
    } else {
        AVIF_CHECK(avifROStreamReadU32(&s, &meta->primaryItemID)); // unsigned int(32) item_ID;
    }
    return AVIF_TRUE;
}

static avifBool avifParseItemDataBox(avifMeta * meta, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    // Check to see if we've already seen an idat box for this meta box. If so, bail out
    if (meta->idat.size > 0) {
        avifDiagnosticsPrintf(diag, "Meta box contains multiple idat boxes");
        return AVIF_FALSE;
    }
    if (rawLen == 0) {
        avifDiagnosticsPrintf(diag, "idat box has a length of 0");
        return AVIF_FALSE;
    }

    avifRWDataSet(&meta->idat, raw, rawLen);
    return AVIF_TRUE;
}

static avifBool avifParseItemPropertiesBox(avifMeta * meta, uint64_t rawOffset, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[iprp]");

    avifBoxHeader ipcoHeader;
    AVIF_CHECK(avifROStreamReadBoxHeader(&s, &ipcoHeader));
    if (memcmp(ipcoHeader.type, "ipco", 4)) {
        avifDiagnosticsPrintf(diag, "Failed to find Box[ipco] as the first box in Box[iprp]");
        return AVIF_FALSE;
    }

    // Read all item properties inside of ItemPropertyContainerBox
    AVIF_CHECK(avifParseItemPropertyContainerBox(&meta->properties,
                                                 rawOffset + avifROStreamOffset(&s),
                                                 avifROStreamCurrent(&s),
                                                 ipcoHeader.size,
                                                 diag));
    AVIF_CHECK(avifROStreamSkip(&s, ipcoHeader.size));

    uint32_t versionAndFlagsSeen[MAX_IPMA_VERSION_AND_FLAGS_SEEN];
    uint32_t versionAndFlagsSeenCount = 0;

    // Now read all ItemPropertyAssociation until the end of the box, and make associations
    while (avifROStreamHasBytesLeft(&s, 1)) {
        avifBoxHeader ipmaHeader;
        AVIF_CHECK(avifROStreamReadBoxHeader(&s, &ipmaHeader));

        if (!memcmp(ipmaHeader.type, "ipma", 4)) {
            uint32_t versionAndFlags;
            AVIF_CHECK(avifParseItemPropertyAssociation(meta, avifROStreamCurrent(&s), ipmaHeader.size, diag, &versionAndFlags));
            for (uint32_t i = 0; i < versionAndFlagsSeenCount; ++i) {
                if (versionAndFlagsSeen[i] == versionAndFlags) {
                    // HEIF (ISO 23008-12:2017) 9.3.1 - There shall be at most one
                    // ItemPropertyAssociation box with a given pair of values of version and
                    // flags.
                    avifDiagnosticsPrintf(diag, "Multiple Box[ipma] with a given pair of values of version and flags. See HEIF (ISO 23008-12:2017) 9.3.1");
                    return AVIF_FALSE;
                }
            }
            if (versionAndFlagsSeenCount == MAX_IPMA_VERSION_AND_FLAGS_SEEN) {
                avifDiagnosticsPrintf(diag, "Exceeded possible count of unique ipma version and flags tuples");
                return AVIF_FALSE;
            }
            versionAndFlagsSeen[versionAndFlagsSeenCount] = versionAndFlags;
            ++versionAndFlagsSeenCount;
        } else {
            // These must all be type ipma
            avifDiagnosticsPrintf(diag, "Box[iprp] contains a box that isn't type 'ipma'");
            return AVIF_FALSE;
        }

        AVIF_CHECK(avifROStreamSkip(&s, ipmaHeader.size));
    }
    return AVIF_TRUE;
}

static avifBool avifParseItemInfoEntry(avifMeta * meta, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[infe]");

    uint8_t version;
    uint32_t flags;
    AVIF_CHECK(avifROStreamReadVersionAndFlags(&s, &version, &flags));
    // Version 2+ is required for item_type
    if (version != 2 && version != 3) {
        avifDiagnosticsPrintf(s.diag, "%s: Expecting box version 2 or 3, got version %u", s.diagContext, version);
        return AVIF_FALSE;
    }
    // TODO: check flags. ISO/IEC 23008-12:2017, Section 9.2 says:
    //   The flags field of ItemInfoEntry with version greater than or equal to 2 is specified as
    //   follows:
    //
    //   (flags & 1) equal to 1 indicates that the item is not intended to be a part of the
    //   presentation. For example, when (flags & 1) is equal to 1 for an image item, the image
    //   item should not be displayed.
    //   (flags & 1) equal to 0 indicates that the item is intended to be a part of the
    //   presentation.
    //
    // See also Section 6.4.2.

    uint32_t itemID;
    if (version == 2) {
        uint16_t tmp;
        AVIF_CHECK(avifROStreamReadU16(&s, &tmp)); // unsigned int(16) item_ID;
        itemID = tmp;
    } else {
        assert(version == 3);
        AVIF_CHECK(avifROStreamReadU32(&s, &itemID)); // unsigned int(32) item_ID;
    }
    uint16_t itemProtectionIndex;                              // unsigned int(16) item_protection_index;
    AVIF_CHECK(avifROStreamReadU16(&s, &itemProtectionIndex)); //
    uint8_t itemType[4];                                       // unsigned int(32) item_type;
    AVIF_CHECK(avifROStreamRead(&s, itemType, 4));             //

    avifContentType contentType;
    if (!memcmp(itemType, "mime", 4)) {
        AVIF_CHECK(avifROStreamReadString(&s, NULL, 0));                                   // string item_name; (skipped)
        AVIF_CHECK(avifROStreamReadString(&s, contentType.contentType, CONTENTTYPE_SIZE)); // string content_type;
    } else {
        memset(&contentType, 0, sizeof(contentType));
    }

    avifDecoderItem * item = avifMetaFindItem(meta, itemID);
    if (!item) {
        avifDiagnosticsPrintf(diag, "Box[infe] has an invalid item ID [%u]", itemID);
        return AVIF_FALSE;
    }

    memcpy(item->type, itemType, sizeof(itemType));
    item->contentType = contentType;
    return AVIF_TRUE;
}

static avifBool avifParseItemInfoBox(avifMeta * meta, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[iinf]");

    uint8_t version;
    AVIF_CHECK(avifROStreamReadVersionAndFlags(&s, &version, NULL));
    uint32_t entryCount;
    if (version == 0) {
        uint16_t tmp;
        AVIF_CHECK(avifROStreamReadU16(&s, &tmp)); // unsigned int(16) entry_count;
        entryCount = tmp;
    } else if (version == 1) {
        AVIF_CHECK(avifROStreamReadU32(&s, &entryCount)); // unsigned int(32) entry_count;
    } else {
        avifDiagnosticsPrintf(diag, "Box[iinf] has an unsupported version %u", version);
        return AVIF_FALSE;
    }

    for (uint32_t entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
        avifBoxHeader infeHeader;
        AVIF_CHECK(avifROStreamReadBoxHeader(&s, &infeHeader));

        if (!memcmp(infeHeader.type, "infe", 4)) {
            AVIF_CHECK(avifParseItemInfoEntry(meta, avifROStreamCurrent(&s), infeHeader.size, diag));
        } else {
            // These must all be type infe
            avifDiagnosticsPrintf(diag, "Box[iinf] contains a box that isn't type 'infe'");
            return AVIF_FALSE;
        }

        AVIF_CHECK(avifROStreamSkip(&s, infeHeader.size));
    }

    return AVIF_TRUE;
}

static avifBool avifParseItemReferenceBox(avifMeta * meta, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[iref]");

    uint8_t version;
    AVIF_CHECK(avifROStreamReadVersionAndFlags(&s, &version, NULL));

    while (avifROStreamHasBytesLeft(&s, 1)) {
        avifBoxHeader irefHeader;
        AVIF_CHECK(avifROStreamReadBoxHeader(&s, &irefHeader));

        uint32_t fromID = 0;
        if (version == 0) {
            uint16_t tmp;
            AVIF_CHECK(avifROStreamReadU16(&s, &tmp)); // unsigned int(16) from_item_ID;
            fromID = tmp;
        } else if (version == 1) {
            AVIF_CHECK(avifROStreamReadU32(&s, &fromID)); // unsigned int(32) from_item_ID;
        } else {
            // unsupported iref version, skip it
            break;
        }

        uint16_t referenceCount = 0;
        AVIF_CHECK(avifROStreamReadU16(&s, &referenceCount)); // unsigned int(16) reference_count;

        for (uint16_t refIndex = 0; refIndex < referenceCount; ++refIndex) {
            uint32_t toID = 0;
            if (version == 0) {
                uint16_t tmp;
                AVIF_CHECK(avifROStreamReadU16(&s, &tmp)); // unsigned int(16) to_item_ID;
                toID = tmp;
            } else if (version == 1) {
                AVIF_CHECK(avifROStreamReadU32(&s, &toID)); // unsigned int(32) to_item_ID;
            } else {
                // unsupported iref version, skip it
                break;
            }

            // Read this reference as "{fromID} is a {irefType} for {toID}"
            if (fromID && toID) {
                avifDecoderItem * item = avifMetaFindItem(meta, fromID);
                if (!item) {
                    avifDiagnosticsPrintf(diag, "Box[iref] has an invalid item ID [%u]", fromID);
                    return AVIF_FALSE;
                }

                if (!memcmp(irefHeader.type, "thmb", 4)) {
                    item->thumbnailForID = toID;
                } else if (!memcmp(irefHeader.type, "auxl", 4)) {
                    item->auxForID = toID;
                } else if (!memcmp(irefHeader.type, "cdsc", 4)) {
                    item->descForID = toID;
                } else if (!memcmp(irefHeader.type, "dimg", 4)) {
                    // derived images refer in the opposite direction
                    avifDecoderItem * dimg = avifMetaFindItem(meta, toID);
                    if (!dimg) {
                        avifDiagnosticsPrintf(diag, "Box[iref] has an invalid item ID dimg ref [%u]", toID);
                        return AVIF_FALSE;
                    }

                    dimg->dimgForID = fromID;
                } else if (!memcmp(irefHeader.type, "prem", 4)) {
                    item->premByID = toID;
                }
            }
        }
    }

    return AVIF_TRUE;
}

static avifBool avifParseMetaBox(avifMeta * meta, uint64_t rawOffset, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[meta]");

    AVIF_CHECK(avifROStreamReadAndEnforceVersion(&s, 0));

    ++meta->idatID; // for tracking idat

    avifBool firstBox = AVIF_TRUE;
    uint32_t uniqueBoxFlags = 0;
    while (avifROStreamHasBytesLeft(&s, 1)) {
        avifBoxHeader header;
        AVIF_CHECK(avifROStreamReadBoxHeader(&s, &header));

        if (firstBox) {
            if (!memcmp(header.type, "hdlr", 4)) {
                AVIF_CHECK(uniqueBoxSeen(&uniqueBoxFlags, 0, "meta", "hdlr", diag));
                AVIF_CHECK(avifParseHandlerBox(avifROStreamCurrent(&s), header.size, diag));
                firstBox = AVIF_FALSE;
            } else {
                // hdlr must be the first box!
                avifDiagnosticsPrintf(diag, "Box[meta] does not have a Box[hdlr] as its first child box");
                return AVIF_FALSE;
            }
        } else if (!memcmp(header.type, "iloc", 4)) {
            AVIF_CHECK(uniqueBoxSeen(&uniqueBoxFlags, 1, "meta", "iloc", diag));
            AVIF_CHECK(avifParseItemLocationBox(meta, avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "pitm", 4)) {
            AVIF_CHECK(uniqueBoxSeen(&uniqueBoxFlags, 2, "meta", "pitm", diag));
            AVIF_CHECK(avifParsePrimaryItemBox(meta, avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "idat", 4)) {
            AVIF_CHECK(uniqueBoxSeen(&uniqueBoxFlags, 3, "meta", "idat", diag));
            AVIF_CHECK(avifParseItemDataBox(meta, avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "iprp", 4)) {
            AVIF_CHECK(uniqueBoxSeen(&uniqueBoxFlags, 4, "meta", "iprp", diag));
            AVIF_CHECK(avifParseItemPropertiesBox(meta, rawOffset + avifROStreamOffset(&s), avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "iinf", 4)) {
            AVIF_CHECK(uniqueBoxSeen(&uniqueBoxFlags, 5, "meta", "iinf", diag));
            AVIF_CHECK(avifParseItemInfoBox(meta, avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "iref", 4)) {
            AVIF_CHECK(uniqueBoxSeen(&uniqueBoxFlags, 6, "meta", "iref", diag));
            AVIF_CHECK(avifParseItemReferenceBox(meta, avifROStreamCurrent(&s), header.size, diag));
        }

        AVIF_CHECK(avifROStreamSkip(&s, header.size));
    }
    if (firstBox) {
        // The meta box must not be empty (it must contain at least a hdlr box)
        avifDiagnosticsPrintf(diag, "Box[meta] has no child boxes");
        return AVIF_FALSE;
    }
    return AVIF_TRUE;
}

static avifBool avifParseTrackHeaderBox(avifTrack * track,
                                        const uint8_t * raw,
                                        size_t rawLen,
                                        uint32_t imageSizeLimit,
                                        uint32_t imageDimensionLimit,
                                        avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[tkhd]");

    uint8_t version;
    AVIF_CHECK(avifROStreamReadVersionAndFlags(&s, &version, NULL));

    uint32_t ignored32, trackID;
    uint64_t ignored64;
    if (version == 1) {
        AVIF_CHECK(avifROStreamReadU64(&s, &ignored64)); // unsigned int(64) creation_time;
        AVIF_CHECK(avifROStreamReadU64(&s, &ignored64)); // unsigned int(64) modification_time;
        AVIF_CHECK(avifROStreamReadU32(&s, &trackID));   // unsigned int(32) track_ID;
        AVIF_CHECK(avifROStreamReadU32(&s, &ignored32)); // const unsigned int(32) reserved = 0;
        AVIF_CHECK(avifROStreamReadU64(&s, &ignored64)); // unsigned int(64) duration;
    } else if (version == 0) {
        AVIF_CHECK(avifROStreamReadU32(&s, &ignored32)); // unsigned int(32) creation_time;
        AVIF_CHECK(avifROStreamReadU32(&s, &ignored32)); // unsigned int(32) modification_time;
        AVIF_CHECK(avifROStreamReadU32(&s, &trackID));   // unsigned int(32) track_ID;
        AVIF_CHECK(avifROStreamReadU32(&s, &ignored32)); // const unsigned int(32) reserved = 0;
        AVIF_CHECK(avifROStreamReadU32(&s, &ignored32)); // unsigned int(32) duration;
    } else {
        // Unsupported version
        avifDiagnosticsPrintf(diag, "Box[tkhd] has an unsupported version [%u]", version);
        return AVIF_FALSE;
    }

    // Skipping the following 52 bytes here:
    // ------------------------------------
    // const unsigned int(32)[2] reserved = 0;
    // template int(16) layer = 0;
    // template int(16) alternate_group = 0;
    // template int(16) volume = {if track_is_audio 0x0100 else 0};
    // const unsigned int(16) reserved = 0;
    // template int(32)[9] matrix= { 0x00010000,0,0,0,0x00010000,0,0,0,0x40000000 }; // unity matrix
    AVIF_CHECK(avifROStreamSkip(&s, 52));

    uint32_t width, height;
    AVIF_CHECK(avifROStreamReadU32(&s, &width));  // unsigned int(32) width;
    AVIF_CHECK(avifROStreamReadU32(&s, &height)); // unsigned int(32) height;
    track->width = width >> 16;
    track->height = height >> 16;

    if ((track->width == 0) || (track->height == 0)) {
        avifDiagnosticsPrintf(diag, "Track ID [%u] has an invalid size [%ux%u]", track->id, track->width, track->height);
        return AVIF_FALSE;
    }
    if (avifDimensionsTooLarge(track->width, track->height, imageSizeLimit, imageDimensionLimit)) {
        avifDiagnosticsPrintf(diag, "Track ID [%u] dimensions are too large [%ux%u]", track->id, track->width, track->height);
        return AVIF_FALSE;
    }

    // TODO: support scaling based on width/height track header info?

    track->id = trackID;
    return AVIF_TRUE;
}

static avifBool avifParseMediaHeaderBox(avifTrack * track, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[mdhd]");

    uint8_t version;
    AVIF_CHECK(avifROStreamReadVersionAndFlags(&s, &version, NULL));

    uint32_t ignored32, mediaTimescale, mediaDuration32;
    uint64_t ignored64, mediaDuration64;
    if (version == 1) {
        AVIF_CHECK(avifROStreamReadU64(&s, &ignored64));       // unsigned int(64) creation_time;
        AVIF_CHECK(avifROStreamReadU64(&s, &ignored64));       // unsigned int(64) modification_time;
        AVIF_CHECK(avifROStreamReadU32(&s, &mediaTimescale));  // unsigned int(32) timescale;
        AVIF_CHECK(avifROStreamReadU64(&s, &mediaDuration64)); // unsigned int(64) duration;
        track->mediaDuration = mediaDuration64;
    } else if (version == 0) {
        AVIF_CHECK(avifROStreamReadU32(&s, &ignored32));       // unsigned int(32) creation_time;
        AVIF_CHECK(avifROStreamReadU32(&s, &ignored32));       // unsigned int(32) modification_time;
        AVIF_CHECK(avifROStreamReadU32(&s, &mediaTimescale));  // unsigned int(32) timescale;
        AVIF_CHECK(avifROStreamReadU32(&s, &mediaDuration32)); // unsigned int(32) duration;
        track->mediaDuration = (uint64_t)mediaDuration32;
    } else {
        // Unsupported version
        avifDiagnosticsPrintf(diag, "Box[mdhd] has an unsupported version [%u]", version);
        return AVIF_FALSE;
    }

    track->mediaTimescale = mediaTimescale;
    return AVIF_TRUE;
}

static avifBool avifParseChunkOffsetBox(avifSampleTable * sampleTable, avifBool largeOffsets, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, largeOffsets ? "Box[co64]" : "Box[stco]");

    AVIF_CHECK(avifROStreamReadAndEnforceVersion(&s, 0));

    uint32_t entryCount;
    AVIF_CHECK(avifROStreamReadU32(&s, &entryCount)); // unsigned int(32) entry_count;
    for (uint32_t i = 0; i < entryCount; ++i) {
        uint64_t offset;
        if (largeOffsets) {
            AVIF_CHECK(avifROStreamReadU64(&s, &offset)); // unsigned int(32) chunk_offset;
        } else {
            uint32_t offset32;
            AVIF_CHECK(avifROStreamReadU32(&s, &offset32)); // unsigned int(32) chunk_offset;
            offset = (uint64_t)offset32;
        }

        avifSampleTableChunk * chunk = (avifSampleTableChunk *)avifArrayPushPtr(&sampleTable->chunks);
        chunk->offset = offset;
    }
    return AVIF_TRUE;
}

static avifBool avifParseSampleToChunkBox(avifSampleTable * sampleTable, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[stsc]");

    AVIF_CHECK(avifROStreamReadAndEnforceVersion(&s, 0));

    uint32_t entryCount;
    AVIF_CHECK(avifROStreamReadU32(&s, &entryCount)); // unsigned int(32) entry_count;
    uint32_t prevFirstChunk = 0;
    for (uint32_t i = 0; i < entryCount; ++i) {
        avifSampleTableSampleToChunk * sampleToChunk = (avifSampleTableSampleToChunk *)avifArrayPushPtr(&sampleTable->sampleToChunks);
        AVIF_CHECK(avifROStreamReadU32(&s, &sampleToChunk->firstChunk));             // unsigned int(32) first_chunk;
        AVIF_CHECK(avifROStreamReadU32(&s, &sampleToChunk->samplesPerChunk));        // unsigned int(32) samples_per_chunk;
        AVIF_CHECK(avifROStreamReadU32(&s, &sampleToChunk->sampleDescriptionIndex)); // unsigned int(32) sample_description_index;
        // The first_chunk fields should start with 1 and be strictly increasing.
        if (i == 0) {
            if (sampleToChunk->firstChunk != 1) {
                avifDiagnosticsPrintf(diag, "Box[stsc] does not begin with chunk 1 [%u]", sampleToChunk->firstChunk);
                return AVIF_FALSE;
            }
        } else {
            if (sampleToChunk->firstChunk <= prevFirstChunk) {
                avifDiagnosticsPrintf(diag, "Box[stsc] chunks are not strictly increasing");
                return AVIF_FALSE;
            }
        }
        prevFirstChunk = sampleToChunk->firstChunk;
    }
    return AVIF_TRUE;
}

static avifBool avifParseSampleSizeBox(avifSampleTable * sampleTable, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[stsz]");

    AVIF_CHECK(avifROStreamReadAndEnforceVersion(&s, 0));

    uint32_t allSamplesSize, sampleCount;
    AVIF_CHECK(avifROStreamReadU32(&s, &allSamplesSize)); // unsigned int(32) sample_size;
    AVIF_CHECK(avifROStreamReadU32(&s, &sampleCount));    // unsigned int(32) sample_count;

    if (allSamplesSize > 0) {
        sampleTable->allSamplesSize = allSamplesSize;
    } else {
        for (uint32_t i = 0; i < sampleCount; ++i) {
            avifSampleTableSampleSize * sampleSize = (avifSampleTableSampleSize *)avifArrayPushPtr(&sampleTable->sampleSizes);
            AVIF_CHECK(avifROStreamReadU32(&s, &sampleSize->size)); // unsigned int(32) entry_size;
        }
    }
    return AVIF_TRUE;
}

static avifBool avifParseSyncSampleBox(avifSampleTable * sampleTable, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[stss]");

    AVIF_CHECK(avifROStreamReadAndEnforceVersion(&s, 0));

    uint32_t entryCount;
    AVIF_CHECK(avifROStreamReadU32(&s, &entryCount)); // unsigned int(32) entry_count;

    for (uint32_t i = 0; i < entryCount; ++i) {
        uint32_t sampleNumber = 0;
        AVIF_CHECK(avifROStreamReadU32(&s, &sampleNumber)); // unsigned int(32) sample_number;
        avifSyncSample * syncSample = (avifSyncSample *)avifArrayPushPtr(&sampleTable->syncSamples);
        syncSample->sampleNumber = sampleNumber;
    }
    return AVIF_TRUE;
}

static avifBool avifParseTimeToSampleBox(avifSampleTable * sampleTable, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[stts]");

    AVIF_CHECK(avifROStreamReadAndEnforceVersion(&s, 0));

    uint32_t entryCount;
    AVIF_CHECK(avifROStreamReadU32(&s, &entryCount)); // unsigned int(32) entry_count;

    for (uint32_t i = 0; i < entryCount; ++i) {
        avifSampleTableTimeToSample * timeToSample = (avifSampleTableTimeToSample *)avifArrayPushPtr(&sampleTable->timeToSamples);
        AVIF_CHECK(avifROStreamReadU32(&s, &timeToSample->sampleCount)); // unsigned int(32) sample_count;
        AVIF_CHECK(avifROStreamReadU32(&s, &timeToSample->sampleDelta)); // unsigned int(32) sample_delta;
    }
    return AVIF_TRUE;
}

static avifBool avifParseSampleDescriptionBox(avifSampleTable * sampleTable,
                                              uint64_t rawOffset,
                                              const uint8_t * raw,
                                              size_t rawLen,
                                              avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[stsd]");

    AVIF_CHECK(avifROStreamReadAndEnforceVersion(&s, 0));

    uint32_t entryCount;
    AVIF_CHECK(avifROStreamReadU32(&s, &entryCount)); // unsigned int(32) entry_count;

    for (uint32_t i = 0; i < entryCount; ++i) {
        avifBoxHeader sampleEntryHeader;
        AVIF_CHECK(avifROStreamReadBoxHeader(&s, &sampleEntryHeader));

        avifSampleDescription * description = (avifSampleDescription *)avifArrayPushPtr(&sampleTable->sampleDescriptions);
        if (!avifArrayCreate(&description->properties, sizeof(avifProperty), 16)) {
            avifArrayPop(&sampleTable->sampleDescriptions);
            return AVIF_FALSE;
        }
        memcpy(description->format, sampleEntryHeader.type, sizeof(description->format));
        size_t remainingBytes = avifROStreamRemainingBytes(&s);
        if (!memcmp(description->format, "av01", 4) && (remainingBytes > VISUALSAMPLEENTRY_SIZE)) {
            AVIF_CHECK(avifParseItemPropertyContainerBox(&description->properties,
                                                         rawOffset + avifROStreamOffset(&s) + VISUALSAMPLEENTRY_SIZE,
                                                         avifROStreamCurrent(&s) + VISUALSAMPLEENTRY_SIZE,
                                                         remainingBytes - VISUALSAMPLEENTRY_SIZE,
                                                         diag));
        }

        AVIF_CHECK(avifROStreamSkip(&s, sampleEntryHeader.size));
    }
    return AVIF_TRUE;
}

static avifBool avifParseSampleTableBox(avifTrack * track, uint64_t rawOffset, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    if (track->sampleTable) {
        // A TrackBox may only have one SampleTable
        avifDiagnosticsPrintf(diag, "Duplicate Box[stbl] for a single track detected");
        return AVIF_FALSE;
    }
    track->sampleTable = avifSampleTableCreate();

    BEGIN_STREAM(s, raw, rawLen, diag, "Box[stbl]");

    while (avifROStreamHasBytesLeft(&s, 1)) {
        avifBoxHeader header;
        AVIF_CHECK(avifROStreamReadBoxHeader(&s, &header));

        if (!memcmp(header.type, "stco", 4)) {
            AVIF_CHECK(avifParseChunkOffsetBox(track->sampleTable, AVIF_FALSE, avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "co64", 4)) {
            AVIF_CHECK(avifParseChunkOffsetBox(track->sampleTable, AVIF_TRUE, avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "stsc", 4)) {
            AVIF_CHECK(avifParseSampleToChunkBox(track->sampleTable, avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "stsz", 4)) {
            AVIF_CHECK(avifParseSampleSizeBox(track->sampleTable, avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "stss", 4)) {
            AVIF_CHECK(avifParseSyncSampleBox(track->sampleTable, avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "stts", 4)) {
            AVIF_CHECK(avifParseTimeToSampleBox(track->sampleTable, avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "stsd", 4)) {
            AVIF_CHECK(avifParseSampleDescriptionBox(track->sampleTable,
                                                     rawOffset + avifROStreamOffset(&s),
                                                     avifROStreamCurrent(&s),
                                                     header.size,
                                                     diag));
        }

        AVIF_CHECK(avifROStreamSkip(&s, header.size));
    }
    return AVIF_TRUE;
}

static avifBool avifParseMediaInformationBox(avifTrack * track, uint64_t rawOffset, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[minf]");

    while (avifROStreamHasBytesLeft(&s, 1)) {
        avifBoxHeader header;
        AVIF_CHECK(avifROStreamReadBoxHeader(&s, &header));

        if (!memcmp(header.type, "stbl", 4)) {
            AVIF_CHECK(avifParseSampleTableBox(track, rawOffset + avifROStreamOffset(&s), avifROStreamCurrent(&s), header.size, diag));
        }

        AVIF_CHECK(avifROStreamSkip(&s, header.size));
    }
    return AVIF_TRUE;
}

static avifBool avifParseMediaBox(avifTrack * track, uint64_t rawOffset, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[mdia]");

    while (avifROStreamHasBytesLeft(&s, 1)) {
        avifBoxHeader header;
        AVIF_CHECK(avifROStreamReadBoxHeader(&s, &header));

        if (!memcmp(header.type, "mdhd", 4)) {
            AVIF_CHECK(avifParseMediaHeaderBox(track, avifROStreamCurrent(&s), header.size, diag));
        } else if (!memcmp(header.type, "minf", 4)) {
            AVIF_CHECK(avifParseMediaInformationBox(track, rawOffset + avifROStreamOffset(&s), avifROStreamCurrent(&s), header.size, diag));
        }

        AVIF_CHECK(avifROStreamSkip(&s, header.size));
    }
    return AVIF_TRUE;
}

static avifBool avifTrackReferenceBox(avifTrack * track, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[tref]");

    while (avifROStreamHasBytesLeft(&s, 1)) {
        avifBoxHeader header;
        AVIF_CHECK(avifROStreamReadBoxHeader(&s, &header));

        if (!memcmp(header.type, "auxl", 4)) {
            uint32_t toID;
            AVIF_CHECK(avifROStreamReadU32(&s, &toID));                       // unsigned int(32) track_IDs[];
            AVIF_CHECK(avifROStreamSkip(&s, header.size - sizeof(uint32_t))); // just take the first one
            track->auxForID = toID;
        } else if (!memcmp(header.type, "prem", 4)) {
            uint32_t byID;
            AVIF_CHECK(avifROStreamReadU32(&s, &byID));                       // unsigned int(32) track_IDs[];
            AVIF_CHECK(avifROStreamSkip(&s, header.size - sizeof(uint32_t))); // just take the first one
            track->premByID = byID;
        } else {
            AVIF_CHECK(avifROStreamSkip(&s, header.size));
        }
    }
    return AVIF_TRUE;
}

static avifBool avifParseTrackBox(avifDecoderData * data, uint64_t rawOffset, const uint8_t * raw, size_t rawLen, uint32_t imageSizeLimit, uint32_t imageDimensionLimit)
{
    BEGIN_STREAM(s, raw, rawLen, data->diag, "Box[trak]");

    avifTrack * track = avifDecoderDataCreateTrack(data);

    while (avifROStreamHasBytesLeft(&s, 1)) {
        avifBoxHeader header;
        AVIF_CHECK(avifROStreamReadBoxHeader(&s, &header));

        if (!memcmp(header.type, "tkhd", 4)) {
            AVIF_CHECK(avifParseTrackHeaderBox(track, avifROStreamCurrent(&s), header.size, imageSizeLimit, imageDimensionLimit, data->diag));
        } else if (!memcmp(header.type, "meta", 4)) {
            AVIF_CHECK(avifParseMetaBox(track->meta, rawOffset + avifROStreamOffset(&s), avifROStreamCurrent(&s), header.size, data->diag));
        } else if (!memcmp(header.type, "mdia", 4)) {
            AVIF_CHECK(avifParseMediaBox(track, rawOffset + avifROStreamOffset(&s), avifROStreamCurrent(&s), header.size, data->diag));
        } else if (!memcmp(header.type, "tref", 4)) {
            AVIF_CHECK(avifTrackReferenceBox(track, avifROStreamCurrent(&s), header.size, data->diag));
        }

        AVIF_CHECK(avifROStreamSkip(&s, header.size));
    }
    return AVIF_TRUE;
}

static avifBool avifParseMovieBox(avifDecoderData * data, uint64_t rawOffset, const uint8_t * raw, size_t rawLen, uint32_t imageSizeLimit, uint32_t imageDimensionLimit)
{
    BEGIN_STREAM(s, raw, rawLen, data->diag, "Box[moov]");

    while (avifROStreamHasBytesLeft(&s, 1)) {
        avifBoxHeader header;
        AVIF_CHECK(avifROStreamReadBoxHeader(&s, &header));

        if (!memcmp(header.type, "trak", 4)) {
            AVIF_CHECK(
                avifParseTrackBox(data, rawOffset + avifROStreamOffset(&s), avifROStreamCurrent(&s), header.size, imageSizeLimit, imageDimensionLimit));
        }

        AVIF_CHECK(avifROStreamSkip(&s, header.size));
    }
    return AVIF_TRUE;
}

static avifBool avifParseFileTypeBox(avifFileType * ftyp, const uint8_t * raw, size_t rawLen, avifDiagnostics * diag)
{
    BEGIN_STREAM(s, raw, rawLen, diag, "Box[ftyp]");

    AVIF_CHECK(avifROStreamRead(&s, ftyp->majorBrand, 4));
    AVIF_CHECK(avifROStreamReadU32(&s, &ftyp->minorVersion));

    size_t compatibleBrandsBytes = avifROStreamRemainingBytes(&s);
    if ((compatibleBrandsBytes % 4) != 0) {
        avifDiagnosticsPrintf(diag, "Box[ftyp] contains a compatible brands section that isn't divisible by 4 [%zu]", compatibleBrandsBytes);
        return AVIF_FALSE;
    }
    ftyp->compatibleBrands = avifROStreamCurrent(&s);
    AVIF_CHECK(avifROStreamSkip(&s, compatibleBrandsBytes));
    ftyp->compatibleBrandsCount = (int)compatibleBrandsBytes / 4;

    return AVIF_TRUE;
}

static avifBool avifFileTypeHasBrand(avifFileType * ftyp, const char * brand);
static avifBool avifFileTypeIsCompatible(avifFileType * ftyp);

static avifResult avifParse(avifDecoder * decoder)
{
    // Note: this top-level function is the only avifParse*() function that returns avifResult instead of avifBool.
    // Be sure to use AVIF_CHECKERR() in this function with an explicit error result instead of simply using AVIF_CHECK().

    avifResult readResult;
    uint64_t parseOffset = 0;
    avifDecoderData * data = decoder->data;
    avifBool ftypSeen = AVIF_FALSE;
    avifBool metaSeen = AVIF_FALSE;
    avifBool moovSeen = AVIF_FALSE;
    avifBool needsMeta = AVIF_FALSE;
    avifBool needsMoov = AVIF_FALSE;

    for (;;) {
        // Read just enough to get the next box header (a max of 32 bytes)
        avifROData headerContents;
        if ((decoder->io->sizeHint > 0) && (parseOffset > decoder->io->sizeHint)) {
            return AVIF_RESULT_BMFF_PARSE_FAILED;
        }
        readResult = decoder->io->read(decoder->io, 0, parseOffset, 32, &headerContents);
        if (readResult != AVIF_RESULT_OK) {
            return readResult;
        }
        if (!headerContents.size) {
            // If we got AVIF_RESULT_OK from the reader but received 0 bytes,
            // we've reached the end of the file with no errors. Hooray!
            break;
        }

        // Parse the header, and find out how many bytes it actually was
        BEGIN_STREAM(headerStream, headerContents.data, headerContents.size, &decoder->diag, "File-level box header");
        avifBoxHeader header;
        AVIF_CHECKERR(avifROStreamReadBoxHeaderPartial(&headerStream, &header), AVIF_RESULT_BMFF_PARSE_FAILED);
        parseOffset += headerStream.offset;
        assert((decoder->io->sizeHint == 0) || (parseOffset <= decoder->io->sizeHint));

        // Try to get the remainder of the box, if necessary
        uint64_t boxOffset = 0;
        avifROData boxContents = AVIF_DATA_EMPTY;

        // TODO: reorg this code to only do these memcmps once each
        if (!memcmp(header.type, "ftyp", 4) || !memcmp(header.type, "meta", 4) || !memcmp(header.type, "moov", 4)) {
            boxOffset = parseOffset;
            readResult = decoder->io->read(decoder->io, 0, parseOffset, header.size, &boxContents);
            if (readResult != AVIF_RESULT_OK) {
                return readResult;
            }
            if (boxContents.size != header.size) {
                // A truncated box, bail out
                return AVIF_RESULT_TRUNCATED_DATA;
            }
        } else if (header.size > (UINT64_MAX - parseOffset)) {
            return AVIF_RESULT_BMFF_PARSE_FAILED;
        }
        parseOffset += header.size;

        if (!memcmp(header.type, "ftyp", 4)) {
            AVIF_CHECKERR(!ftypSeen, AVIF_RESULT_BMFF_PARSE_FAILED);
            avifFileType ftyp;
            AVIF_CHECKERR(avifParseFileTypeBox(&ftyp, boxContents.data, boxContents.size, data->diag), AVIF_RESULT_BMFF_PARSE_FAILED);
            if (!avifFileTypeIsCompatible(&ftyp)) {
                return AVIF_RESULT_INVALID_FTYP;
            }
            ftypSeen = AVIF_TRUE;
            memcpy(data->majorBrand, ftyp.majorBrand, 4); // Remember the major brand for future AVIF_DECODER_SOURCE_AUTO decisions
            needsMeta = avifFileTypeHasBrand(&ftyp, "avif");
            needsMoov = avifFileTypeHasBrand(&ftyp, "avis");
        } else if (!memcmp(header.type, "meta", 4)) {
            AVIF_CHECKERR(!metaSeen, AVIF_RESULT_BMFF_PARSE_FAILED);
            AVIF_CHECKERR(avifParseMetaBox(data->meta, boxOffset, boxContents.data, boxContents.size, data->diag),
                          AVIF_RESULT_BMFF_PARSE_FAILED);
            metaSeen = AVIF_TRUE;
        } else if (!memcmp(header.type, "moov", 4)) {
            AVIF_CHECKERR(!moovSeen, AVIF_RESULT_BMFF_PARSE_FAILED);
            AVIF_CHECKERR(avifParseMovieBox(data, boxOffset, boxContents.data, boxContents.size, decoder->imageSizeLimit, decoder->imageDimensionLimit),
                          AVIF_RESULT_BMFF_PARSE_FAILED);
            moovSeen = AVIF_TRUE;
        }

        // See if there is enough information to consider Parse() a success and early-out:
        // * If the brand 'avif' is present, require a meta box
        // * If the brand 'avis' is present, require a moov box
        if (ftypSeen && (!needsMeta || metaSeen) && (!needsMoov || moovSeen)) {
            return AVIF_RESULT_OK;
        }
    }
    if (!ftypSeen) {
        return AVIF_RESULT_INVALID_FTYP;
    }
    if ((needsMeta && !metaSeen) || (needsMoov && !moovSeen)) {
        return AVIF_RESULT_TRUNCATED_DATA;
    }
    return AVIF_RESULT_OK;
}

// ---------------------------------------------------------------------------

static avifBool avifFileTypeHasBrand(avifFileType * ftyp, const char * brand)
{
    if (!memcmp(ftyp->majorBrand, brand, 4)) {
        return AVIF_TRUE;
    }

    for (int compatibleBrandIndex = 0; compatibleBrandIndex < ftyp->compatibleBrandsCount; ++compatibleBrandIndex) {
        const uint8_t * compatibleBrand = &ftyp->compatibleBrands[4 * compatibleBrandIndex];
        if (!memcmp(compatibleBrand, brand, 4)) {
            return AVIF_TRUE;
        }
    }
    return AVIF_FALSE;
}

static avifBool avifFileTypeIsCompatible(avifFileType * ftyp)
{
    return avifFileTypeHasBrand(ftyp, "avif") || avifFileTypeHasBrand(ftyp, "avis");
}

avifBool avifPeekCompatibleFileType(const avifROData * input)
{
    BEGIN_STREAM(s, input->data, input->size, NULL, NULL);

    avifBoxHeader header;
    AVIF_CHECK(avifROStreamReadBoxHeader(&s, &header));
    if (memcmp(header.type, "ftyp", 4)) {
        return AVIF_FALSE;
    }

    avifFileType ftyp;
    memset(&ftyp, 0, sizeof(avifFileType));
    avifBool parsed = avifParseFileTypeBox(&ftyp, avifROStreamCurrent(&s), header.size, NULL);
    if (!parsed) {
        return AVIF_FALSE;
    }
    return avifFileTypeIsCompatible(&ftyp);
}

// ---------------------------------------------------------------------------

avifDecoder * avifDecoderCreate(void)
{
    avifDecoder * decoder = (avifDecoder *)avifAlloc(sizeof(avifDecoder));
    memset(decoder, 0, sizeof(avifDecoder));
    decoder->maxThreads = 1;
    decoder->imageSizeLimit = AVIF_DEFAULT_IMAGE_SIZE_LIMIT;
    decoder->imageDimensionLimit = AVIF_DEFAULT_IMAGE_DIMENSION_LIMIT;
    decoder->imageCountLimit = AVIF_DEFAULT_IMAGE_COUNT_LIMIT;
    decoder->strictFlags = AVIF_STRICT_ENABLED;
    return decoder;
}

static void avifDecoderCleanup(avifDecoder * decoder)
{
    if (decoder->data) {
        avifDecoderDataDestroy(decoder->data);
        decoder->data = NULL;
    }

    if (decoder->image) {
        avifImageDestroy(decoder->image);
        decoder->image = NULL;
    }
    avifDiagnosticsClearError(&decoder->diag);
}

void avifDecoderDestroy(avifDecoder * decoder)
{
    avifDecoderCleanup(decoder);
    avifIODestroy(decoder->io);
    avifFree(decoder);
}

avifResult avifDecoderSetSource(avifDecoder * decoder, avifDecoderSource source)
{
    decoder->requestedSource = source;
    return avifDecoderReset(decoder);
}

void avifDecoderSetIO(avifDecoder * decoder, avifIO * io)
{
    avifIODestroy(decoder->io);
    decoder->io = io;
}

avifResult avifDecoderSetIOMemory(avifDecoder * decoder, const uint8_t * data, size_t size)
{
    avifIO * io = avifIOCreateMemoryReader(data, size);
    assert(io);
    avifDecoderSetIO(decoder, io);
    return AVIF_RESULT_OK;
}

avifResult avifDecoderSetIOFile(avifDecoder * decoder, const char * filename)
{
    avifIO * io = avifIOCreateFileReader(filename);
    if (!io) {
        return AVIF_RESULT_IO_ERROR;
    }
    avifDecoderSetIO(decoder, io);
    return AVIF_RESULT_OK;
}

// 0-byte extents are ignored/overwritten during the merge, as they are the signal from helper
// functions that no extent was necessary for this given sample. If both provided extents are
// >0 bytes, this will set dst to be an extent that bounds both supplied extents.
static avifResult avifExtentMerge(avifExtent * dst, const avifExtent * src)
{
    if (!dst->size) {
        *dst = *src;
        return AVIF_RESULT_OK;
    }
    if (!src->size) {
        return AVIF_RESULT_OK;
    }

    const uint64_t minExtent1 = dst->offset;
    const uint64_t maxExtent1 = dst->offset + dst->size;
    const uint64_t minExtent2 = src->offset;
    const uint64_t maxExtent2 = src->offset + src->size;
    dst->offset = AVIF_MIN(minExtent1, minExtent2);
    const uint64_t extentLength = AVIF_MAX(maxExtent1, maxExtent2) - dst->offset;
    if (extentLength > SIZE_MAX) {
        return AVIF_RESULT_BMFF_PARSE_FAILED;
    }
    dst->size = (size_t)extentLength;
    return AVIF_RESULT_OK;
}

avifResult avifDecoderNthImageMaxExtent(const avifDecoder * decoder, uint32_t frameIndex, avifExtent * outExtent)
{
    if (!decoder->data) {
        // Nothing has been parsed yet
        return AVIF_RESULT_NO_CONTENT;
    }

    memset(outExtent, 0, sizeof(avifExtent));

    uint32_t startFrameIndex = avifDecoderNearestKeyframe(decoder, frameIndex);
    uint32_t endFrameIndex = frameIndex;
    for (uint32_t currentFrameIndex = startFrameIndex; currentFrameIndex <= endFrameIndex; ++currentFrameIndex) {
        for (unsigned int tileIndex = 0; tileIndex < decoder->data->tiles.count; ++tileIndex) {
            avifTile * tile = &decoder->data->tiles.tile[tileIndex];
            if (currentFrameIndex >= tile->input->samples.count) {
                return AVIF_RESULT_NO_IMAGES_REMAINING;
            }

            avifDecodeSample * sample = &tile->input->samples.sample[currentFrameIndex];
            avifExtent sampleExtent;
            if (sample->itemID) {
                // The data comes from an item. Let avifDecoderItemMaxExtent() do the heavy lifting.

                avifDecoderItem * item = avifMetaFindItem(decoder->data->meta, sample->itemID);
                avifResult maxExtentResult = avifDecoderItemMaxExtent(item, sample, &sampleExtent);
                if (maxExtentResult != AVIF_RESULT_OK) {
                    return maxExtentResult;
                }
            } else {
                // The data likely comes from a sample table. Use the sample position directly.

                sampleExtent.offset = sample->offset;
                sampleExtent.size = sample->size;
            }

            if (sampleExtent.size > UINT64_MAX - sampleExtent.offset) {
                return AVIF_RESULT_BMFF_PARSE_FAILED;
            }

            avifResult extentMergeResult = avifExtentMerge(outExtent, &sampleExtent);
            if (extentMergeResult != AVIF_RESULT_OK) {
                return extentMergeResult;
            }
        }
    }
    return AVIF_RESULT_OK;
}

static avifResult avifDecoderPrepareSample(avifDecoder * decoder, avifDecodeSample * sample, size_t partialByteCount)
{
    if (!sample->data.size || sample->partialData) {
        // This sample hasn't been read from IO or had its extents fully merged yet.

        size_t bytesToRead = sample->size;
        if (partialByteCount && (bytesToRead > partialByteCount)) {
            bytesToRead = partialByteCount;
        }

        if (sample->itemID) {
            // The data comes from an item. Let avifDecoderItemRead() do the heavy lifting.

            avifDecoderItem * item = avifMetaFindItem(decoder->data->meta, sample->itemID);
            avifROData itemContents;
            if (sample->offset > SIZE_MAX) {
                return AVIF_RESULT_BMFF_PARSE_FAILED;
            }
            size_t offset = (size_t)sample->offset;
            avifResult readResult = avifDecoderItemRead(item, decoder->io, &itemContents, offset, bytesToRead, &decoder->diag);
            if (readResult != AVIF_RESULT_OK) {
                return readResult;
            }

            // avifDecoderItemRead is guaranteed to already be persisted by either the underlying IO
            // or by mergedExtents; just reuse the buffer here.
            sample->data = itemContents;
            sample->ownsData = AVIF_FALSE;
            sample->partialData = item->partialMergedExtents;
        } else {
            // The data likely comes from a sample table. Pull the sample and make a copy if necessary.

            avifROData sampleContents;
            if ((decoder->io->sizeHint > 0) && (sample->offset > decoder->io->sizeHint)) {
                return AVIF_RESULT_BMFF_PARSE_FAILED;
            }
            avifResult readResult = decoder->io->read(decoder->io, 0, sample->offset, bytesToRead, &sampleContents);
            if (readResult != AVIF_RESULT_OK) {
                return readResult;
            }
            if (sampleContents.size != bytesToRead) {
                return AVIF_RESULT_TRUNCATED_DATA;
            }

            sample->ownsData = !decoder->io->persistent;
            sample->partialData = (bytesToRead != sample->size);
            if (decoder->io->persistent) {
                sample->data = sampleContents;
            } else {
                avifRWDataSet((avifRWData *)&sample->data, sampleContents.data, sampleContents.size);
            }
        }
    }
    return AVIF_RESULT_OK;
}

avifResult avifDecoderParse(avifDecoder * decoder)
{
    avifDiagnosticsClearError(&decoder->diag);

    // An imageSizeLimit greater than AVIF_DEFAULT_IMAGE_SIZE_LIMIT and the special value of 0 to
    // disable the limit are not yet implemented.
    if ((decoder->imageSizeLimit > AVIF_DEFAULT_IMAGE_SIZE_LIMIT) || (decoder->imageSizeLimit == 0)) {
        return AVIF_RESULT_NOT_IMPLEMENTED;
    }
    if (!decoder->io || !decoder->io->read) {
        return AVIF_RESULT_IO_NOT_SET;
    }

    // Cleanup anything lingering in the decoder
    avifDecoderCleanup(decoder);

    // -----------------------------------------------------------------------
    // Parse BMFF boxes

    decoder->data = avifDecoderDataCreate();
    decoder->data->diag = &decoder->diag;

    avifResult parseResult = avifParse(decoder);
    if (parseResult != AVIF_RESULT_OK) {
        return parseResult;
    }

    // Walk the decoded items (if any) and harvest ispe
    avifDecoderData * data = decoder->data;
    for (uint32_t itemIndex = 0; itemIndex < data->meta->items.count; ++itemIndex) {
        avifDecoderItem * item = &data->meta->items.item[itemIndex];
        if (!item->size) {
            continue;
        }
        if (item->hasUnsupportedEssentialProperty) {
            // An essential property isn't supported by libavif; ignore the item.
            continue;
        }
        avifBool isGrid = (memcmp(item->type, "grid", 4) == 0);
        if (memcmp(item->type, "av01", 4) && !isGrid) {
            // probably exif or some other data
            continue;
        }

        const avifProperty * ispeProp = avifPropertyArrayFind(&item->properties, "ispe");
        if (ispeProp) {
            item->width = ispeProp->u.ispe.width;
            item->height = ispeProp->u.ispe.height;

            if ((item->width == 0) || (item->height == 0)) {
                avifDiagnosticsPrintf(data->diag, "Item ID [%u] has an invalid size [%ux%u]", item->id, item->width, item->height);
                return AVIF_RESULT_BMFF_PARSE_FAILED;
            }
            if (avifDimensionsTooLarge(item->width, item->height, decoder->imageSizeLimit, decoder->imageDimensionLimit)) {
                avifDiagnosticsPrintf(data->diag, "Item ID [%u] dimensions are too large [%ux%u]", item->id, item->width, item->height);
                return AVIF_RESULT_BMFF_PARSE_FAILED;
            }
        } else {
            const avifProperty * auxCProp = avifPropertyArrayFind(&item->properties, "auxC");
            if (auxCProp && isAlphaURN(auxCProp->u.auxC.auxType)) {
                if (decoder->strictFlags & AVIF_STRICT_ALPHA_ISPE_REQUIRED) {
                    avifDiagnosticsPrintf(data->diag,
                                          "[Strict] Alpha auxiliary image item ID [%u] is missing a mandatory ispe property",
                                          item->id);
                    return AVIF_RESULT_BMFF_PARSE_FAILED;
                }
            } else {
                avifDiagnosticsPrintf(data->diag, "Item ID [%u] is missing a mandatory ispe property", item->id);
                return AVIF_RESULT_BMFF_PARSE_FAILED;
            }
        }
    }
    return avifDecoderReset(decoder);
}

static avifCodec * avifCodecCreateInternal(avifCodecChoice choice)
{
    return avifCodecCreate(choice, AVIF_CODEC_FLAG_CAN_DECODE);
}

static avifResult avifDecoderFlush(avifDecoder * decoder)
{
    avifDecoderDataResetCodec(decoder->data);

    for (unsigned int i = 0; i < decoder->data->tiles.count; ++i) {
        avifTile * tile = &decoder->data->tiles.tile[i];
        tile->codec = avifCodecCreateInternal(decoder->codecChoice);
        if (tile->codec) {
            tile->codec->diag = &decoder->diag;
            tile->codec->operatingPoint = tile->operatingPoint;
            tile->codec->allLayers = tile->input->allLayers;
        }
    }
    return AVIF_RESULT_OK;
}

avifResult avifDecoderReset(avifDecoder * decoder)
{
    avifDiagnosticsClearError(&decoder->diag);

    avifDecoderData * data = decoder->data;
    if (!data) {
        // Nothing to reset.
        return AVIF_RESULT_OK;
    }

    memset(&data->colorGrid, 0, sizeof(data->colorGrid));
    memset(&data->alphaGrid, 0, sizeof(data->alphaGrid));
    avifDecoderDataClearTiles(data);

    // Prepare / cleanup decoded image state
    if (decoder->image) {
        avifImageDestroy(decoder->image);
    }
    decoder->image = avifImageCreateEmpty();
    decoder->progressiveState = AVIF_PROGRESSIVE_STATE_UNAVAILABLE;
    data->cicpSet = AVIF_FALSE;

    memset(&decoder->ioStats, 0, sizeof(decoder->ioStats));

    // -----------------------------------------------------------------------
    // Build decode input

    data->sourceSampleTable = NULL; // Reset
    if (decoder->requestedSource == AVIF_DECODER_SOURCE_AUTO) {
        // Honor the major brand (avif or avis) if present, otherwise prefer avis (tracks) if possible.
        if (!memcmp(data->majorBrand, "avis", 4)) {
            data->source = AVIF_DECODER_SOURCE_TRACKS;
        } else if (!memcmp(data->majorBrand, "avif", 4)) {
            data->source = AVIF_DECODER_SOURCE_PRIMARY_ITEM;
        } else if (data->tracks.count > 0) {
            data->source = AVIF_DECODER_SOURCE_TRACKS;
        } else {
            data->source = AVIF_DECODER_SOURCE_PRIMARY_ITEM;
        }
    } else {
        data->source = decoder->requestedSource;
    }

    const avifPropertyArray * colorProperties = NULL;
    if (data->source == AVIF_DECODER_SOURCE_TRACKS) {
        avifTrack * colorTrack = NULL;
        avifTrack * alphaTrack = NULL;

        // Find primary track - this probably needs some better detection
        uint32_t colorTrackIndex = 0;
        for (; colorTrackIndex < data->tracks.count; ++colorTrackIndex) {
            avifTrack * track = &data->tracks.track[colorTrackIndex];
            if (!track->sampleTable) {
                continue;
            }
            if (!track->id) { // trak box might be missing a tkhd box inside, skip it
                continue;
            }
            if (!track->sampleTable->chunks.count) {
                continue;
            }
            if (!avifSampleTableHasFormat(track->sampleTable, "av01")) {
                continue;
            }
            if (track->auxForID != 0) {
                continue;
            }

            // Found one!
            break;
        }
        if (colorTrackIndex == data->tracks.count) {
            avifDiagnosticsPrintf(&decoder->diag, "Failed to find AV1 color track");
            return AVIF_RESULT_NO_CONTENT;
        }
        colorTrack = &data->tracks.track[colorTrackIndex];

        colorProperties = avifSampleTableGetProperties(colorTrack->sampleTable);
        if (!colorProperties) {
            avifDiagnosticsPrintf(&decoder->diag, "Failed to find AV1 color track's color properties");
            return AVIF_RESULT_BMFF_PARSE_FAILED;
        }

        // Find Exif and/or XMP metadata, if any
        if (colorTrack->meta) {
            // See the comment above avifDecoderFindMetadata() for the explanation of using 0 here
            avifResult findResult = avifDecoderFindMetadata(decoder, colorTrack->meta, decoder->image, 0);
            if (findResult != AVIF_RESULT_OK) {
                return findResult;
            }
        }

        uint32_t alphaTrackIndex = 0;
        for (; alphaTrackIndex < data->tracks.count; ++alphaTrackIndex) {
            avifTrack * track = &data->tracks.track[alphaTrackIndex];
            if (!track->sampleTable) {
                continue;
            }
            if (!track->id) {
                continue;
            }
            if (!track->sampleTable->chunks.count) {
                continue;
            }
            if (!avifSampleTableHasFormat(track->sampleTable, "av01")) {
                continue;
            }
            if (track->auxForID == colorTrack->id) {
                // Found it!
                break;
            }
        }
        if (alphaTrackIndex != data->tracks.count) {
            alphaTrack = &data->tracks.track[alphaTrackIndex];
        }

        avifTile * colorTile = avifDecoderDataCreateTile(data, colorTrack->width, colorTrack->height, 0); // No way to set operating point via tracks
        if (!colorTile) {
            return AVIF_RESULT_OUT_OF_MEMORY;
        }
        if (!avifCodecDecodeInputFillFromSampleTable(colorTile->input,
                                                     colorTrack->sampleTable,
                                                     decoder->imageCountLimit,
                                                     decoder->io->sizeHint,
                                                     data->diag)) {
            return AVIF_RESULT_BMFF_PARSE_FAILED;
        }
        data->colorTileCount = 1;

        if (alphaTrack) {
            avifTile * alphaTile = avifDecoderDataCreateTile(data, alphaTrack->width, alphaTrack->height, 0); // No way to set operating point via tracks
            if (!alphaTile) {
                return AVIF_RESULT_OUT_OF_MEMORY;
            }
            if (!avifCodecDecodeInputFillFromSampleTable(alphaTile->input,
                                                         alphaTrack->sampleTable,
                                                         decoder->imageCountLimit,
                                                         decoder->io->sizeHint,
                                                         data->diag)) {
                return AVIF_RESULT_BMFF_PARSE_FAILED;
            }
            alphaTile->input->alpha = AVIF_TRUE;
            data->alphaTileCount = 1;
        }

        // Stash off sample table for future timing information
        data->sourceSampleTable = colorTrack->sampleTable;

        // Image sequence timing
        decoder->imageIndex = -1;
        decoder->imageCount = colorTile->input->samples.count;
        decoder->timescale = colorTrack->mediaTimescale;
        decoder->durationInTimescales = colorTrack->mediaDuration;
        if (colorTrack->mediaTimescale) {
            decoder->duration = (double)decoder->durationInTimescales / (double)colorTrack->mediaTimescale;
        } else {
            decoder->duration = 0;
        }
        memset(&decoder->imageTiming, 0, sizeof(decoder->imageTiming)); // to be set in avifDecoderNextImage()

        decoder->image->width = colorTrack->width;
        decoder->image->height = colorTrack->height;
        decoder->alphaPresent = (alphaTrack != NULL);
        decoder->image->alphaPremultiplied = decoder->alphaPresent && (colorTrack->premByID == alphaTrack->id);
    } else {
        // Create from items

        avifDecoderItem * colorItem = NULL;
        avifDecoderItem * alphaItem = NULL;

        if (data->meta->primaryItemID == 0) {
            // A primary item is required
            avifDiagnosticsPrintf(&decoder->diag, "Primary item not specified");
            return AVIF_RESULT_NO_AV1_ITEMS_FOUND;
        }

        // Find the colorOBU (primary) item
        for (uint32_t itemIndex = 0; itemIndex < data->meta->items.count; ++itemIndex) {
            avifDecoderItem * item = &data->meta->items.item[itemIndex];
            if (!item->size) {
                continue;
            }
            if (item->hasUnsupportedEssentialProperty) {
                // An essential property isn't supported by libavif; ignore the item.
                continue;
            }
            avifBool isGrid = (memcmp(item->type, "grid", 4) == 0);
            if (memcmp(item->type, "av01", 4) && !isGrid) {
                // probably exif or some other data
                continue;
            }
            if (item->thumbnailForID != 0) {
                // It's a thumbnail, skip it
                continue;
            }
            if (item->id != data->meta->primaryItemID) {
                // This is not the primary item, skip it
                continue;
            }

            if (isGrid) {
                avifROData readData;
                avifResult readResult = avifDecoderItemRead(item, decoder->io, &readData, 0, 0, data->diag);
                if (readResult != AVIF_RESULT_OK) {
                    return readResult;
                }
                if (!avifParseImageGridBox(&data->colorGrid,
                                           readData.data,
                                           readData.size,
                                           decoder->imageSizeLimit,
                                           decoder->imageDimensionLimit,
                                           data->diag)) {
                    return AVIF_RESULT_INVALID_IMAGE_GRID;
                }
            }

            colorItem = item;
            break;
        }

        if (!colorItem) {
            avifDiagnosticsPrintf(&decoder->diag, "Primary item not found");
            return AVIF_RESULT_NO_AV1_ITEMS_FOUND;
        }
        colorProperties = &colorItem->properties;

        // Find the alphaOBU item, if any
        for (uint32_t itemIndex = 0; itemIndex < data->meta->items.count; ++itemIndex) {
            avifDecoderItem * item = &data->meta->items.item[itemIndex];
            if (!item->size) {
                continue;
            }
            if (item->hasUnsupportedEssentialProperty) {
                // An essential property isn't supported by libavif; ignore the item.
                continue;
            }
            avifBool isGrid = (memcmp(item->type, "grid", 4) == 0);
            if (memcmp(item->type, "av01", 4) && !isGrid) {
                // probably exif or some other data
                continue;
            }

            // Is this an alpha auxiliary item of whatever we chose for colorItem?
            const avifProperty * auxCProp = avifPropertyArrayFind(&item->properties, "auxC");
            if (auxCProp && isAlphaURN(auxCProp->u.auxC.auxType) && (item->auxForID == colorItem->id)) {
                if (isGrid) {
                    avifROData readData;
                    avifResult readResult = avifDecoderItemRead(item, decoder->io, &readData, 0, 0, data->diag);
                    if (readResult != AVIF_RESULT_OK) {
                        return readResult;
                    }
                    if (!avifParseImageGridBox(&data->alphaGrid,
                                               readData.data,
                                               readData.size,
                                               decoder->imageSizeLimit,
                                               decoder->imageDimensionLimit,
                                               data->diag)) {
                        return AVIF_RESULT_INVALID_IMAGE_GRID;
                    }
                }

                alphaItem = item;
                break;
            }
        }

        // Find Exif and/or XMP metadata, if any
        avifResult findResult = avifDecoderFindMetadata(decoder, data->meta, decoder->image, colorItem->id);
        if (findResult != AVIF_RESULT_OK) {
            return findResult;
        }

        // Set all counts and timing to safe-but-uninteresting values
        decoder->imageIndex = -1;
        decoder->imageCount = 1;
        decoder->imageTiming.timescale = 1;
        decoder->imageTiming.pts = 0;
        decoder->imageTiming.ptsInTimescales = 0;
        decoder->imageTiming.duration = 1;
        decoder->imageTiming.durationInTimescales = 1;
        decoder->timescale = 1;
        decoder->duration = 1;
        decoder->durationInTimescales = 1;

        if ((data->colorGrid.rows > 0) && (data->colorGrid.columns > 0)) {
            if (!avifDecoderGenerateImageGridTiles(decoder, &data->colorGrid, colorItem, AVIF_FALSE)) {
                return AVIF_RESULT_INVALID_IMAGE_GRID;
            }
            data->colorTileCount = data->tiles.count;
        } else {
            if (colorItem->size == 0) {
                return AVIF_RESULT_NO_AV1_ITEMS_FOUND;
            }

            avifTile * colorTile =
                avifDecoderDataCreateTile(data, colorItem->width, colorItem->height, avifDecoderItemOperatingPoint(colorItem));
            if (!colorTile) {
                return AVIF_RESULT_OUT_OF_MEMORY;
            }
            if (!avifCodecDecodeInputFillFromDecoderItem(colorTile->input,
                                                         colorItem,
                                                         decoder->allowProgressive,
                                                         decoder->imageCountLimit,
                                                         decoder->io->sizeHint,
                                                         &decoder->diag)) {
                return AVIF_RESULT_BMFF_PARSE_FAILED;
            }
            data->colorTileCount = 1;

            if (colorItem->progressive) {
                decoder->progressiveState = AVIF_PROGRESSIVE_STATE_AVAILABLE;
                if (colorTile->input->samples.count > 1) {
                    decoder->progressiveState = AVIF_PROGRESSIVE_STATE_ACTIVE;
                    decoder->imageCount = colorTile->input->samples.count;
                }
            }
        }

        if (alphaItem) {
            if (!alphaItem->width && !alphaItem->height) {
                // NON-STANDARD: Alpha subimage does not have an ispe property; adopt width/height from color item
                assert(!(decoder->strictFlags & AVIF_STRICT_ALPHA_ISPE_REQUIRED));
                alphaItem->width = colorItem->width;
                alphaItem->height = colorItem->height;
            }

            if ((data->alphaGrid.rows > 0) && (data->alphaGrid.columns > 0)) {
                if (!avifDecoderGenerateImageGridTiles(decoder, &data->alphaGrid, alphaItem, AVIF_TRUE)) {
                    return AVIF_RESULT_INVALID_IMAGE_GRID;
                }
                data->alphaTileCount = data->tiles.count - data->colorTileCount;
            } else {
                if (alphaItem->size == 0) {
                    return AVIF_RESULT_NO_AV1_ITEMS_FOUND;
                }

                avifTile * alphaTile =
                    avifDecoderDataCreateTile(data, alphaItem->width, alphaItem->height, avifDecoderItemOperatingPoint(alphaItem));
                if (!alphaTile) {
                    return AVIF_RESULT_OUT_OF_MEMORY;
                }
                if (!avifCodecDecodeInputFillFromDecoderItem(alphaTile->input,
                                                             alphaItem,
                                                             decoder->allowProgressive,
                                                             decoder->imageCountLimit,
                                                             decoder->io->sizeHint,
                                                             &decoder->diag)) {
                    return AVIF_RESULT_BMFF_PARSE_FAILED;
                }
                alphaTile->input->alpha = AVIF_TRUE;
                data->alphaTileCount = 1;
            }
        }

        decoder->ioStats.colorOBUSize = colorItem->size;
        decoder->ioStats.alphaOBUSize = alphaItem ? alphaItem->size : 0;

        decoder->image->width = colorItem->width;
        decoder->image->height = colorItem->height;
        decoder->alphaPresent = (alphaItem != NULL);
        decoder->image->alphaPremultiplied = decoder->alphaPresent && (colorItem->premByID == alphaItem->id);

        avifResult colorItemValidationResult = avifDecoderItemValidateAV1(colorItem, &decoder->diag, decoder->strictFlags);
        if (colorItemValidationResult != AVIF_RESULT_OK) {
            return colorItemValidationResult;
        }
        if (alphaItem) {
            avifResult alphaItemValidationResult = avifDecoderItemValidateAV1(alphaItem, &decoder->diag, decoder->strictFlags);
            if (alphaItemValidationResult != AVIF_RESULT_OK) {
                return alphaItemValidationResult;
            }
        }
    }

    // Sanity check tiles
    for (uint32_t tileIndex = 0; tileIndex < data->tiles.count; ++tileIndex) {
        avifTile * tile = &data->tiles.tile[tileIndex];
        for (uint32_t sampleIndex = 0; sampleIndex < tile->input->samples.count; ++sampleIndex) {
            avifDecodeSample * sample = &tile->input->samples.sample[sampleIndex];
            if (!sample->size) {
                // Every sample must have some data
                return AVIF_RESULT_BMFF_PARSE_FAILED;
            }
        }
    }

    // Find and adopt all colr boxes "at most one for a given value of colour type" (HEIF 6.5.5.1, from Amendment 3)
    // Accept one of each type, and bail out if more than one of a given type is provided.
    avifBool colrICCSeen = AVIF_FALSE;
    avifBool colrNCLXSeen = AVIF_FALSE;
    for (uint32_t propertyIndex = 0; propertyIndex < colorProperties->count; ++propertyIndex) {
        avifProperty * prop = &colorProperties->prop[propertyIndex];

        if (!memcmp(prop->type, "colr", 4)) {
            if (prop->u.colr.hasICC) {
                if (colrICCSeen) {
                    return AVIF_RESULT_BMFF_PARSE_FAILED;
                }
                avifROData icc;
                const avifResult readResult = decoder->io->read(decoder->io, 0, prop->u.colr.iccOffset, prop->u.colr.iccSize, &icc);
                if (readResult != AVIF_RESULT_OK) {
                    return readResult;
                }
                colrICCSeen = AVIF_TRUE;
                avifImageSetProfileICC(decoder->image, icc.data, icc.size);
            }
            if (prop->u.colr.hasNCLX) {
                if (colrNCLXSeen) {
                    return AVIF_RESULT_BMFF_PARSE_FAILED;
                }
                colrNCLXSeen = AVIF_TRUE;
                data->cicpSet = AVIF_TRUE;
                decoder->image->colorPrimaries = prop->u.colr.colorPrimaries;
                decoder->image->transferCharacteristics = prop->u.colr.transferCharacteristics;
                decoder->image->matrixCoefficients = prop->u.colr.matrixCoefficients;
                decoder->image->yuvRange = prop->u.colr.range;
            }
        }
    }

    // Transformations
    const avifProperty * paspProp = avifPropertyArrayFind(colorProperties, "pasp");
    if (paspProp) {
        decoder->image->transformFlags |= AVIF_TRANSFORM_PASP;
        decoder->image->pasp = paspProp->u.pasp;
    }
    const avifProperty * clapProp = avifPropertyArrayFind(colorProperties, "clap");
    if (clapProp) {
        decoder->image->transformFlags |= AVIF_TRANSFORM_CLAP;
        decoder->image->clap = clapProp->u.clap;
    }
    const avifProperty * irotProp = avifPropertyArrayFind(colorProperties, "irot");
    if (irotProp) {
        decoder->image->transformFlags |= AVIF_TRANSFORM_IROT;
        decoder->image->irot = irotProp->u.irot;
    }
    const avifProperty * imirProp = avifPropertyArrayFind(colorProperties, "imir");
    if (imirProp) {
        decoder->image->transformFlags |= AVIF_TRANSFORM_IMIR;
        decoder->image->imir = imirProp->u.imir;
    }

    if (!data->cicpSet && (data->tiles.count > 0)) {
        avifTile * firstTile = &data->tiles.tile[0];
        if (firstTile->input->samples.count > 0) {
            avifDecodeSample * sample = &firstTile->input->samples.sample[0];

            // Harvest CICP from the AV1's sequence header, which should be very close to the front
            // of the first sample. Read in successively larger chunks until we successfully parse the sequence.
            static const size_t searchSampleChunkIncrement = 64;
            static const size_t searchSampleSizeMax = 4096;
            size_t searchSampleSize = 0;
            do {
                searchSampleSize += searchSampleChunkIncrement;
                if (searchSampleSize > sample->size) {
                    searchSampleSize = sample->size;
                }

                avifResult prepareResult = avifDecoderPrepareSample(decoder, sample, searchSampleSize);
                if (prepareResult != AVIF_RESULT_OK) {
                    return prepareResult;
                }

                avifSequenceHeader sequenceHeader;
                if (avifSequenceHeaderParse(&sequenceHeader, &sample->data)) {
                    data->cicpSet = AVIF_TRUE;
                    decoder->image->colorPrimaries = sequenceHeader.colorPrimaries;
                    decoder->image->transferCharacteristics = sequenceHeader.transferCharacteristics;
                    decoder->image->matrixCoefficients = sequenceHeader.matrixCoefficients;
                    decoder->image->yuvRange = sequenceHeader.range;
                    break;
                }
            } while (searchSampleSize != sample->size && searchSampleSize < searchSampleSizeMax);
        }
    }

    const avifProperty * av1CProp = avifPropertyArrayFind(colorProperties, "av1C");
    if (av1CProp) {
        decoder->image->depth = avifCodecConfigurationBoxGetDepth(&av1CProp->u.av1C);
        if (av1CProp->u.av1C.monochrome) {
            decoder->image->yuvFormat = AVIF_PIXEL_FORMAT_YUV400;
        } else {
            if (av1CProp->u.av1C.chromaSubsamplingX && av1CProp->u.av1C.chromaSubsamplingY) {
                decoder->image->yuvFormat = AVIF_PIXEL_FORMAT_YUV420;
            } else if (av1CProp->u.av1C.chromaSubsamplingX) {
                decoder->image->yuvFormat = AVIF_PIXEL_FORMAT_YUV422;

            } else {
                decoder->image->yuvFormat = AVIF_PIXEL_FORMAT_YUV444;
            }
        }
        decoder->image->yuvChromaSamplePosition = (avifChromaSamplePosition)av1CProp->u.av1C.chromaSamplePosition;
    } else {
        // An av1C box is mandatory in all valid AVIF configurations. Bail out.
        return AVIF_RESULT_BMFF_PARSE_FAILED;
    }

    return avifDecoderFlush(decoder);
}

static avifResult avifDecoderPrepareTiles(avifDecoder * decoder,
                                          uint32_t nextImageIndex,
                                          unsigned int firstTileIndex,
                                          unsigned int tileCount,
                                          unsigned int decodedTileCount)
{
    for (unsigned int tileIndex = decodedTileCount; tileIndex < tileCount; ++tileIndex) {
        avifTile * tile = &decoder->data->tiles.tile[firstTileIndex + tileIndex];

        // Ensure there's an AV1 codec available before doing anything else
        if (!tile->codec) {
            return AVIF_RESULT_NO_CODEC_AVAILABLE;
        }

        if (nextImageIndex >= tile->input->samples.count) {
            return AVIF_RESULT_NO_IMAGES_REMAINING;
        }

        avifDecodeSample * sample = &tile->input->samples.sample[nextImageIndex];
        avifResult prepareResult = avifDecoderPrepareSample(decoder, sample, 0);
        if (prepareResult != AVIF_RESULT_OK) {
            return prepareResult;
        }
    }
    return AVIF_RESULT_OK;
}

static avifResult avifImageLimitedToFullAlpha(avifImage * image)
{
    if (image->imageOwnsAlphaPlane) {
        return AVIF_RESULT_NOT_IMPLEMENTED;
    }

    uint8_t * alphaPlane = image->alphaPlane;
    const uint32_t alphaRowBytes = image->alphaRowBytes;

    // We cannot do the range conversion in place since it will modify the
    // codec's internal frame buffers. Allocate memory for the conversion.
    image->alphaPlane = NULL;
    image->alphaRowBytes = 0;
    const avifResult allocationResult = avifImageAllocatePlanes(image, AVIF_PLANES_A);
    if (allocationResult != AVIF_RESULT_OK) {
        return allocationResult;
    }

    if (image->depth > 8) {
        for (uint32_t j = 0; j < image->height; ++j) {
            uint8_t * srcRow = &alphaPlane[j * alphaRowBytes];
            uint8_t * dstRow = &image->alphaPlane[j * image->alphaRowBytes];
            for (uint32_t i = 0; i < image->width; ++i) {
                int srcAlpha = *((uint16_t *)&srcRow[i * 2]);
                int dstAlpha = avifLimitedToFullY(image->depth, srcAlpha);
                *((uint16_t *)&dstRow[i * 2]) = (uint16_t)dstAlpha;
            }
        }
    } else {
        for (uint32_t j = 0; j < image->height; ++j) {
            uint8_t * srcRow = &alphaPlane[j * alphaRowBytes];
            uint8_t * dstRow = &image->alphaPlane[j * image->alphaRowBytes];
            for (uint32_t i = 0; i < image->width; ++i) {
                int srcAlpha = srcRow[i];
                int dstAlpha = avifLimitedToFullY(image->depth, srcAlpha);
                dstRow[i] = (uint8_t)dstAlpha;
            }
        }
    }
    return AVIF_RESULT_OK;
}

static avifResult avifDecoderDecodeTiles(avifDecoder * decoder,
                                         uint32_t nextImageIndex,
                                         unsigned int firstTileIndex,
                                         unsigned int tileCount,
                                         unsigned int * decodedTileCount)
{
    const unsigned int oldDecodedTileCount = *decodedTileCount;
    for (unsigned int tileIndex = oldDecodedTileCount; tileIndex < tileCount; ++tileIndex) {
        avifTile * tile = &decoder->data->tiles.tile[firstTileIndex + tileIndex];

        const avifDecodeSample * sample = &tile->input->samples.sample[nextImageIndex];
        if (sample->data.size < sample->size) {
            assert(decoder->allowIncremental);
            // Data is missing but there is no error yet. Output available pixel rows.
            return AVIF_RESULT_OK;
        }

        avifBool isLimitedRangeAlpha = AVIF_FALSE;
        if (!tile->codec->getNextImage(tile->codec, decoder, sample, tile->input->alpha, &isLimitedRangeAlpha, tile->image)) {
            avifDiagnosticsPrintf(&decoder->diag, "tile->codec->getNextImage() failed");
            return tile->input->alpha ? AVIF_RESULT_DECODE_ALPHA_FAILED : AVIF_RESULT_DECODE_COLOR_FAILED;
        }

        // Alpha plane with limited range is not allowed by the latest revision
        // of the specification. However, it was allowed in version 1.0.0 of the
        // specification. To allow such files, simply convert the alpha plane to
        // full range.
        if (tile->input->alpha && isLimitedRangeAlpha) {
            avifResult result = avifImageLimitedToFullAlpha(tile->image);
            if (result != AVIF_RESULT_OK) {
                avifDiagnosticsPrintf(&decoder->diag, "avifImageLimitedToFullAlpha failed");
                return result;
            }
        }

        // Scale the decoded image so that it corresponds to this tile's output dimensions
        if ((tile->width != tile->image->width) || (tile->height != tile->image->height)) {
            if (!avifImageScale(tile->image,
                                tile->width,
                                tile->height,
                                decoder->imageSizeLimit,
                                decoder->imageDimensionLimit,
                                &decoder->diag)) {
                avifDiagnosticsPrintf(&decoder->diag, "avifImageScale() failed");
                return tile->input->alpha ? AVIF_RESULT_DECODE_ALPHA_FAILED : AVIF_RESULT_DECODE_COLOR_FAILED;
            }
        }

        ++*decodedTileCount;
    }
    return AVIF_RESULT_OK;
}

avifResult avifDecoderNextImage(avifDecoder * decoder)
{
    avifDiagnosticsClearError(&decoder->diag);

    if (!decoder->data) {
        // Nothing has been parsed yet
        return AVIF_RESULT_NO_CONTENT;
    }

    if (!decoder->io || !decoder->io->read) {
        return AVIF_RESULT_IO_NOT_SET;
    }

    if ((decoder->data->decodedColorTileCount == decoder->data->colorTileCount) &&
        (decoder->data->decodedAlphaTileCount == decoder->data->alphaTileCount)) {
        // A frame was decoded during the last avifDecoderNextImage() call.
        decoder->data->decodedColorTileCount = 0;
        decoder->data->decodedAlphaTileCount = 0;
    }

    assert(decoder->data->tiles.count == (decoder->data->colorTileCount + decoder->data->alphaTileCount));
    const uint32_t nextImageIndex = (uint32_t)(decoder->imageIndex + 1);
    const unsigned int firstColorTileIndex = 0;
    const unsigned int firstAlphaTileIndex = decoder->data->colorTileCount;

    // Acquire all sample data for the current image first, allowing for any read call to bail out
    // with AVIF_RESULT_WAITING_ON_IO harmlessly / idempotently, unless decoder->allowIncremental.
    // Start with color tiles.
    const avifResult prepareColorTileResult =
        avifDecoderPrepareTiles(decoder, nextImageIndex, firstColorTileIndex, decoder->data->colorTileCount, decoder->data->decodedColorTileCount);
    if ((prepareColorTileResult != AVIF_RESULT_OK) &&
        (!decoder->allowIncremental || (prepareColorTileResult != AVIF_RESULT_WAITING_ON_IO))) {
        return prepareColorTileResult;
    }
    // Do the same with alpha tiles. They are handled separately because their
    // order of appearance relative to the color tiles in the bitstream is left
    // to the encoder's choice, and decoding as many as possible of each
    // category in parallel is beneficial for incremental decoding, as pixel
    // rows need all channels to be decoded before being accessible to the user.
    const avifResult prepareAlphaTileResult =
        avifDecoderPrepareTiles(decoder, nextImageIndex, firstAlphaTileIndex, decoder->data->alphaTileCount, decoder->data->decodedAlphaTileCount);
    if ((prepareAlphaTileResult != AVIF_RESULT_OK) &&
        (!decoder->allowIncremental || (prepareAlphaTileResult != AVIF_RESULT_WAITING_ON_IO))) {
        return prepareAlphaTileResult;
    }

    // Decode all available color tiles now, then all available alpha tiles.
    const unsigned int oldDecodedColorTileCount = decoder->data->decodedColorTileCount;
    const avifResult decodeColorTileResult =
        avifDecoderDecodeTiles(decoder, nextImageIndex, firstColorTileIndex, decoder->data->colorTileCount, &decoder->data->decodedColorTileCount);
    if (decodeColorTileResult != AVIF_RESULT_OK) {
        return decodeColorTileResult;
    }
    const unsigned int oldDecodedAlphaTileCount = decoder->data->decodedAlphaTileCount;
    const avifResult decodeAlphaTileResult =
        avifDecoderDecodeTiles(decoder, nextImageIndex, firstAlphaTileIndex, decoder->data->alphaTileCount, &decoder->data->decodedAlphaTileCount);
    if (decodeAlphaTileResult != AVIF_RESULT_OK) {
        return decodeAlphaTileResult;
    }

    if (decoder->data->decodedColorTileCount > oldDecodedColorTileCount) {
        // There is at least one newly decoded color tile.
        if ((decoder->data->colorGrid.rows > 0) && (decoder->data->colorGrid.columns > 0)) {
            assert(decoder->data->colorTileCount == (decoder->data->colorGrid.rows * decoder->data->colorGrid.columns));
            if (!avifDecoderDataFillImageGrid(decoder->data,
                                              &decoder->data->colorGrid,
                                              decoder->image,
                                              firstColorTileIndex,
                                              oldDecodedColorTileCount,
                                              decoder->data->decodedColorTileCount,
                                              AVIF_FALSE)) {
                return AVIF_RESULT_INVALID_IMAGE_GRID;
            }
        } else {
            // Normal (most common) non-grid path. Just steal the planes from the only "tile".
            assert(decoder->data->colorTileCount == 1);
            avifImage * srcColor = decoder->data->tiles.tile[0].image;
            if ((decoder->image->width != srcColor->width) || (decoder->image->height != srcColor->height) ||
                (decoder->image->depth != srcColor->depth)) {
                avifImageFreePlanes(decoder->image, AVIF_PLANES_ALL);

                decoder->image->width = srcColor->width;
                decoder->image->height = srcColor->height;
                decoder->image->depth = srcColor->depth;
            }

#if 0
            // This code is currently unnecessary as the CICP is always set by the end of avifDecoderParse().
            if (!decoder->data->cicpSet) {
                decoder->data->cicpSet = AVIF_TRUE;
                decoder->image->colorPrimaries = srcColor->colorPrimaries;
                decoder->image->transferCharacteristics = srcColor->transferCharacteristics;
                decoder->image->matrixCoefficients = srcColor->matrixCoefficients;
            }
#endif

            avifImageStealPlanes(decoder->image, srcColor, AVIF_PLANES_YUV);
        }
    }

    if (decoder->data->decodedAlphaTileCount > oldDecodedAlphaTileCount) {
        // There is at least one newly decoded alpha tile.
        if ((decoder->data->alphaGrid.rows > 0) && (decoder->data->alphaGrid.columns > 0)) {
            assert(decoder->data->alphaTileCount == (decoder->data->alphaGrid.rows * decoder->data->alphaGrid.columns));
            if (!avifDecoderDataFillImageGrid(decoder->data,
                                              &decoder->data->alphaGrid,
                                              decoder->image,
                                              firstAlphaTileIndex,
                                              oldDecodedAlphaTileCount,
                                              decoder->data->decodedAlphaTileCount,
                                              AVIF_TRUE)) {
                return AVIF_RESULT_INVALID_IMAGE_GRID;
            }
        } else {
            // Normal (most common) non-grid path. Just steal the planes from the only "tile".
            assert(decoder->data->alphaTileCount == 1);
            avifImage * srcAlpha = decoder->data->tiles.tile[decoder->data->colorTileCount].image;
            if ((decoder->image->width != srcAlpha->width) || (decoder->image->height != srcAlpha->height) ||
                (decoder->image->depth != srcAlpha->depth)) {
                avifDiagnosticsPrintf(&decoder->diag, "decoder->image does not match srcAlpha in width, height, or bit depth");
                return AVIF_RESULT_DECODE_ALPHA_FAILED;
            }

            avifImageStealPlanes(decoder->image, srcAlpha, AVIF_PLANES_A);
        }
    }

    if ((decoder->data->decodedColorTileCount != decoder->data->colorTileCount) ||
        (decoder->data->decodedAlphaTileCount != decoder->data->alphaTileCount)) {
        assert(decoder->allowIncremental);
        // The image is not completely decoded. There should be no error unrelated to missing bytes,
        // and at least some missing bytes.
        assert((prepareColorTileResult == AVIF_RESULT_OK) || (prepareColorTileResult == AVIF_RESULT_WAITING_ON_IO));
        assert((prepareAlphaTileResult == AVIF_RESULT_OK) || (prepareAlphaTileResult == AVIF_RESULT_WAITING_ON_IO));
        assert((prepareColorTileResult != AVIF_RESULT_OK) || (prepareAlphaTileResult != AVIF_RESULT_OK));
        // Return the "not enough bytes" status now instead of moving on to the next frame.
        return AVIF_RESULT_WAITING_ON_IO;
    }
    assert((prepareColorTileResult == AVIF_RESULT_OK) && (prepareAlphaTileResult == AVIF_RESULT_OK));

    // Only advance decoder->imageIndex once the image is completely decoded, so that
    // avifDecoderNthImage(decoder, decoder->imageIndex + 1) is equivalent to avifDecoderNextImage(decoder)
    // if the previous call to avifDecoderNextImage() returned AVIF_RESULT_WAITING_ON_IO.
    decoder->imageIndex = nextImageIndex;
    // The decoded tile counts will be reset to 0 the next time avifDecoderNextImage() is called,
    // for avifDecoderDecodedRowCount() to work until then.
    if (decoder->data->sourceSampleTable) {
        // Decoding from a track! Provide timing information.

        avifResult timingResult = avifDecoderNthImageTiming(decoder, decoder->imageIndex, &decoder->imageTiming);
        if (timingResult != AVIF_RESULT_OK) {
            return timingResult;
        }
    }
    return AVIF_RESULT_OK;
}

avifResult avifDecoderNthImageTiming(const avifDecoder * decoder, uint32_t frameIndex, avifImageTiming * outTiming)
{
    if (!decoder->data) {
        // Nothing has been parsed yet
        return AVIF_RESULT_NO_CONTENT;
    }

    if ((frameIndex > INT_MAX) || ((int)frameIndex >= decoder->imageCount)) {
        // Impossible index
        return AVIF_RESULT_NO_IMAGES_REMAINING;
    }

    if (!decoder->data->sourceSampleTable) {
        // There isn't any real timing associated with this decode, so
        // just hand back the defaults chosen in avifDecoderReset().
        *outTiming = decoder->imageTiming;
        return AVIF_RESULT_OK;
    }

    outTiming->timescale = decoder->timescale;
    outTiming->ptsInTimescales = 0;
    for (int imageIndex = 0; imageIndex < (int)frameIndex; ++imageIndex) {
        outTiming->ptsInTimescales += avifSampleTableGetImageDelta(decoder->data->sourceSampleTable, imageIndex);
    }
    outTiming->durationInTimescales = avifSampleTableGetImageDelta(decoder->data->sourceSampleTable, frameIndex);

    if (outTiming->timescale > 0) {
        outTiming->pts = (double)outTiming->ptsInTimescales / (double)outTiming->timescale;
        outTiming->duration = (double)outTiming->durationInTimescales / (double)outTiming->timescale;
    } else {
        outTiming->pts = 0.0;
        outTiming->duration = 0.0;
    }
    return AVIF_RESULT_OK;
}

avifResult avifDecoderNthImage(avifDecoder * decoder, uint32_t frameIndex)
{
    avifDiagnosticsClearError(&decoder->diag);

    if (!decoder->data) {
        // Nothing has been parsed yet
        return AVIF_RESULT_NO_CONTENT;
    }

    if ((frameIndex > INT_MAX) || ((int)frameIndex >= decoder->imageCount)) {
        // Impossible index
        return AVIF_RESULT_NO_IMAGES_REMAINING;
    }

    int requestedIndex = (int)frameIndex;
    if (requestedIndex == (decoder->imageIndex + 1)) {
        // It's just the next image (already partially decoded or not at all), nothing special here
        return avifDecoderNextImage(decoder);
    }

    if (requestedIndex == decoder->imageIndex) {
        if ((decoder->data->decodedColorTileCount == decoder->data->colorTileCount) &&
            (decoder->data->decodedAlphaTileCount == decoder->data->alphaTileCount)) {
            // The current fully decoded image (decoder->imageIndex) is requested, nothing to do
            return AVIF_RESULT_OK;
        }
        // The next image (decoder->imageIndex + 1) is partially decoded but
        // the previous image (decoder->imageIndex) is requested.
        // Fall through to flush and start decoding from the nearest key frame.
    }

    int nearestKeyFrame = (int)avifDecoderNearestKeyframe(decoder, frameIndex);
    if ((nearestKeyFrame > (decoder->imageIndex + 1)) || (requestedIndex <= decoder->imageIndex)) {
        // If we get here, a decoder flush is necessary
        decoder->imageIndex = nearestKeyFrame - 1; // prepare to read nearest keyframe
        avifDecoderFlush(decoder);
    }
    for (;;) {
        avifResult result = avifDecoderNextImage(decoder);
        if (result != AVIF_RESULT_OK) {
            return result;
        }

        if (requestedIndex == decoder->imageIndex) {
            break;
        }
    }
    return AVIF_RESULT_OK;
}

avifBool avifDecoderIsKeyframe(const avifDecoder * decoder, uint32_t frameIndex)
{
    if (!decoder->data || (decoder->data->tiles.count == 0)) {
        // Nothing has been parsed yet
        return AVIF_FALSE;
    }

    // *All* tiles for the requested frameIndex must be keyframes in order for
    //  avifDecoderIsKeyframe() to return true, otherwise we may seek to a frame in which the color
    //  planes are a keyframe but the alpha plane isn't a keyframe, which will cause an alpha plane
    //  decode failure.
    for (unsigned int i = 0; i < decoder->data->tiles.count; ++i) {
        const avifTile * tile = &decoder->data->tiles.tile[i];
        if ((frameIndex >= tile->input->samples.count) || !tile->input->samples.sample[frameIndex].sync) {
            return AVIF_FALSE;
        }
    }
    return AVIF_TRUE;
}

uint32_t avifDecoderNearestKeyframe(const avifDecoder * decoder, uint32_t frameIndex)
{
    if (!decoder->data) {
        // Nothing has been parsed yet
        return 0;
    }

    for (; frameIndex != 0; --frameIndex) {
        if (avifDecoderIsKeyframe(decoder, frameIndex)) {
            break;
        }
    }
    return frameIndex;
}

// Returns the number of available rows in decoder->image given a color or alpha subimage.
static uint32_t avifGetDecodedRowCount(const avifDecoder * decoder,
                                       const avifImageGrid * grid,
                                       unsigned int firstTileIndex,
                                       unsigned int tileCount,
                                       unsigned int decodedTileCount)
{
    if (decodedTileCount == tileCount) {
        return decoder->image->height;
    }
    if (decodedTileCount == 0) {
        return 0;
    }

    if ((grid->rows > 0) && (grid->columns > 0)) {
        // Grid of AVIF tiles (not to be confused with AV1 tiles).
        const uint32_t tileHeight = decoder->data->tiles.tile[firstTileIndex].height;
        return AVIF_MIN((decodedTileCount / grid->columns) * tileHeight, decoder->image->height);
    } else {
        // Non-grid image.
        return decoder->image->height;
    }
}

uint32_t avifDecoderDecodedRowCount(const avifDecoder * decoder)
{
    const uint32_t colorRowCount = avifGetDecodedRowCount(decoder,
                                                          &decoder->data->colorGrid,
                                                          /*firstTileIndex=*/0,
                                                          decoder->data->colorTileCount,
                                                          decoder->data->decodedColorTileCount);
    const uint32_t alphaRowCount = avifGetDecodedRowCount(decoder,
                                                          &decoder->data->alphaGrid,
                                                          /*firstTileIndex=*/decoder->data->colorTileCount,
                                                          decoder->data->alphaTileCount,
                                                          decoder->data->decodedAlphaTileCount);
    return AVIF_MIN(colorRowCount, alphaRowCount);
}

avifResult avifDecoderRead(avifDecoder * decoder, avifImage * image)
{
    avifResult result = avifDecoderParse(decoder);
    if (result != AVIF_RESULT_OK) {
        return result;
    }
    result = avifDecoderNextImage(decoder);
    if (result != AVIF_RESULT_OK) {
        return result;
    }
    return avifImageCopy(image, decoder->image, AVIF_PLANES_ALL);
}

avifResult avifDecoderReadMemory(avifDecoder * decoder, avifImage * image, const uint8_t * data, size_t size)
{
    avifDiagnosticsClearError(&decoder->diag);
    avifResult result = avifDecoderSetIOMemory(decoder, data, size);
    if (result != AVIF_RESULT_OK) {
        return result;
    }
    return avifDecoderRead(decoder, image);
}

avifResult avifDecoderReadFile(avifDecoder * decoder, avifImage * image, const char * filename)
{
    avifDiagnosticsClearError(&decoder->diag);
    avifResult result = avifDecoderSetIOFile(decoder, filename);
    if (result != AVIF_RESULT_OK) {
        return result;
    }
    return avifDecoderRead(decoder, image);
}
