/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(MEDIATOOLBOX)

#include <pal/spi/cf/CoreMediaSPI.h>

#if HAVE(MT_PLUGIN_FORMAT_READER)

#if USE(APPLE_INTERNAL_SDK)

#include <MediaToolbox/MTPluginFormatReader.h>

#else

#include <CoreMedia/CMBase.h>

enum {
    kMTPluginFormatReaderError_AllocationFailure = -16501,
    kMTPluginFormatReaderError_ParsingFailure = -16503,
    kMTPluginSampleCursorError_NoSamples = -16507,
    kMTPluginSampleCursorError_LocationNotAvailable = -16508,
};

enum {
    kMTPluginFormatReader_ClassVersion_1 = 1,
};

enum {
    kMTPluginSampleCursor_ClassVersion_3 = 3,
    kMTPluginSampleCursor_ClassVersion_4 = 4,
};

enum {
    kMTPluginTrackReader_ClassVersion_1 = 1,
};

typedef struct {
    int64_t chunkSampleCount;
    Boolean chunkHasUniformSampleSizes;
    Boolean chunkHasUniformSampleDurations;
    Boolean chunkHasUniformFormatDescriptions;
    char pad[5];
} MTPluginSampleCursorChunkInfo;

typedef struct MTPluginSampleCursorDependencyInfo {
    Boolean sampleIndicatesWhetherItHasDependentSamples;
    Boolean sampleHasDependentSamples;
    Boolean sampleIndicatesWhetherItDependsOnOthers;
    Boolean sampleDependsOnOthers;
    Boolean sampleIndicatesWhetherItHasRedundantCoding;
    Boolean sampleHasRedundantCoding;
} MTPluginSampleCursorDependencyInfo;

typedef struct {
    int64_t offset;
    int64_t length;
} MTPluginSampleCursorStorageRange;

typedef struct MTPluginSampleCursorSyncInfo {
    Boolean fullSync;
    Boolean partialSync;
    Boolean droppable;
} MTPluginSampleCursorSyncInfo;

#endif // !USE(APPLE_INTERNAL_SDK)

typedef CMPersistentTrackID MTPersistentTrackID;
typedef int32_t MTPluginSampleCursorReorderingBoundary;
typedef struct OpaqueMTPluginByteSource* MTPluginByteSourceRef;
typedef struct OpaqueMTPluginFormatReader* MTPluginFormatReaderRef;
typedef struct OpaqueMTPluginSampleCursor *MTPluginSampleCursorRef;
typedef struct OpaqueMTPluginTrackReader *MTPluginTrackReaderRef;

typedef OSStatus (*MTPluginFormatReaderFunction_CopyTrackArray)(MTPluginFormatReaderRef, CFArrayRef*);
typedef OSStatus (*MTPluginFormatReaderFunction_ParseAdditionalFragments)(MTPluginFormatReaderRef, uint32_t, uint32_t*);
typedef OSStatus (*MTPluginSampleCursorFunction_Copy)(MTPluginSampleCursorRef, MTPluginSampleCursorRef*);
typedef OSStatus (*MTPluginSampleCursorFunction_StepInDecodeOrderAndReportStepsTaken)(MTPluginSampleCursorRef, int64_t, int64_t*);
typedef OSStatus (*MTPluginSampleCursorFunction_StepInPresentationOrderAndReportStepsTaken)(MTPluginSampleCursorRef, int64_t, int64_t*);
typedef OSStatus (*MTPluginSampleCursorFunction_StepByDecodeTime)(MTPluginSampleCursorRef, CMTime, Boolean*);
typedef OSStatus (*MTPluginSampleCursorFunction_StepByPresentationTime)(MTPluginSampleCursorRef, CMTime, Boolean*);
typedef CFComparisonResult (*MTPluginSampleCursorFunction_CompareInDecodeOrder)(MTPluginSampleCursorRef, MTPluginSampleCursorRef);
typedef OSStatus (*MTPluginSampleCursorFunction_GetSampleTiming)(MTPluginSampleCursorRef, CMSampleTimingInfo*);
typedef OSStatus (*MTPluginSampleCursorFunction_GetSyncInfo)(MTPluginSampleCursorRef, MTPluginSampleCursorSyncInfo*);
typedef OSStatus (*MTPluginSampleCursorFunction_GetDependencyInfo)(MTPluginSampleCursorRef, MTPluginSampleCursorDependencyInfo*);
typedef Boolean (*MTPluginSampleCursorFunction_TestReorderingBoundary)(MTPluginSampleCursorRef, MTPluginSampleCursorRef, MTPluginSampleCursorReorderingBoundary);
typedef OSStatus (*MTPluginSampleCursorFunction_CopySampleLocation)(MTPluginSampleCursorRef, MTPluginSampleCursorStorageRange*, MTPluginByteSourceRef*);
typedef OSStatus (*MTPluginSampleCursorFunction_CopyChunkDetails)(MTPluginSampleCursorRef, MTPluginByteSourceRef*, MTPluginSampleCursorStorageRange*, MTPluginSampleCursorChunkInfo*, CMItemCount*);
typedef OSStatus (*MTPluginSampleCursorFunction_CopyFormatDescription)(MTPluginSampleCursorRef, CMFormatDescriptionRef*);
typedef OSStatus (*MTPluginSampleCursorFunction_CreateSampleBuffer)(MTPluginSampleCursorRef, MTPluginSampleCursorRef, CMSampleBufferRef*);
typedef OSStatus (*MTPluginSampleCursorFunction_CopyUnrefinedSampleLocation)(MTPluginSampleCursorRef, MTPluginSampleCursorStorageRange*, MTPluginSampleCursorStorageRange*, MTPluginByteSourceRef*);
typedef OSStatus (*MTPluginSampleCursorFunction_RefineSampleLocation)(MTPluginSampleCursorRef, MTPluginSampleCursorStorageRange, const uint8_t*, size_t, MTPluginSampleCursorStorageRange*);
typedef OSStatus (*MTPluginSampleCursorFunction_CopyExtendedSampleDependencyAttributes)(MTPluginSampleCursorRef, CFDictionaryRef*);
typedef OSStatus (*MTPluginSampleCursorFunction_GetPlayableHorizon)(MTPluginSampleCursorRef, CMTime*);
typedef OSStatus (*MTPluginTrackReaderFunction_GetTrackInfo)(MTPluginTrackReaderRef, MTPersistentTrackID*, CMMediaType*);
typedef CMItemCount (*MTPluginTrackReaderFunction_GetTrackEditCount)(MTPluginTrackReaderRef);
typedef OSStatus (*MTPluginTrackReaderFunction_GetTrackEditWithIndex)(MTPluginTrackReaderRef, CMItemCount, CMTimeMapping*);
typedef OSStatus (*MTPluginTrackReaderFunction_CreateCursorAtPresentationTimeStamp)(MTPluginTrackReaderRef, CMTime, MTPluginSampleCursorRef*);
typedef OSStatus (*MTPluginTrackReaderFunction_CreateCursorAtFirstSampleInDecodeOrder)(MTPluginTrackReaderRef, MTPluginSampleCursorRef*);
typedef OSStatus (*MTPluginTrackReaderFunction_CreateCursorAtLastSampleInDecodeOrder)(MTPluginTrackReaderRef, MTPluginSampleCursorRef*);

WTF_EXTERN_C_BEGIN

extern const CFStringRef kMTPluginFormatReader_SupportsPlayableHorizonQueries;
extern const CFStringRef kMTPluginFormatReaderProperty_Duration;
extern const CFStringRef kMTPluginTrackReaderProperty_Enabled;
extern const CFStringRef kMTPluginTrackReaderProperty_FormatDescriptionArray;
extern const CFStringRef kMTPluginTrackReaderProperty_NominalFrameRate;

int64_t MTPluginByteSourceGetLength(MTPluginByteSourceRef);
OSStatus MTPluginByteSourceRead(MTPluginByteSourceRef, size_t, int64_t, void*, size_t*);

CMBaseClassID MTPluginFormatReaderGetClassID();
CMBaseClassID MTPluginSampleCursorGetClassID();
CMBaseClassID MTPluginTrackReaderGetClassID();

CFTypeID MTPluginFormatReaderGetTypeID();
CFTypeID MTPluginSampleCursorGetTypeID();
CFTypeID MTPluginTrackReaderGetTypeID();

WTF_EXTERN_C_END

#if !USE(APPLE_INTERNAL_SDK)

typedef struct {
    unsigned long version;
    MTPluginFormatReaderFunction_CopyTrackArray copyTrackArray;
    MTPluginFormatReaderFunction_ParseAdditionalFragments parseAdditionalFragments;
} MTPluginFormatReaderClass;

typedef struct {
    CMBaseVTable base;
    const MTPluginFormatReaderClass* pluginFormatReaderClass;
} MTPluginFormatReaderVTable;

typedef struct {
    unsigned long version;
    MTPluginSampleCursorFunction_Copy copy;
    MTPluginSampleCursorFunction_StepInDecodeOrderAndReportStepsTaken stepInDecodeOrderAndReportStepsTaken;
    MTPluginSampleCursorFunction_StepInPresentationOrderAndReportStepsTaken stepInPresentationOrderAndReportStepsTaken;
    MTPluginSampleCursorFunction_StepByDecodeTime stepByDecodeTime;
    MTPluginSampleCursorFunction_StepByPresentationTime stepByPresentationTime;
    MTPluginSampleCursorFunction_CompareInDecodeOrder compareInDecodeOrder;
    MTPluginSampleCursorFunction_GetSampleTiming getSampleTiming;
    MTPluginSampleCursorFunction_GetSyncInfo getSyncInfo;
    MTPluginSampleCursorFunction_GetDependencyInfo getDependencyInfo;
    MTPluginSampleCursorFunction_TestReorderingBoundary testReorderingBoundary;
    MTPluginSampleCursorFunction_CopySampleLocation copySampleLocation;
    MTPluginSampleCursorFunction_CopyChunkDetails copyChunkDetails;
    MTPluginSampleCursorFunction_CopyFormatDescription copyFormatDescription;
    MTPluginSampleCursorFunction_CreateSampleBuffer createSampleBuffer;
    MTPluginSampleCursorFunction_CopyUnrefinedSampleLocation copyUnrefinedSampleLocation;
    MTPluginSampleCursorFunction_RefineSampleLocation refineSampleLocation;
    MTPluginSampleCursorFunction_CopyExtendedSampleDependencyAttributes copyExtendedSampleDependencyAttributes;
    MTPluginSampleCursorFunction_GetPlayableHorizon getPlayableHorizon;
} MTPluginSampleCursorClass;

typedef struct {
    CMBaseVTable base;
    const MTPluginSampleCursorClass* pluginSampleCursorClass;
    const struct MTPluginSampleCursorReservedForOfficeUseOnly* reservedSetToNULL;
} MTPluginSampleCursorVTable;

typedef struct {
    unsigned long version;
    MTPluginTrackReaderFunction_GetTrackInfo getTrackInfo;
    MTPluginTrackReaderFunction_GetTrackEditCount getTrackEditCount;
    MTPluginTrackReaderFunction_GetTrackEditWithIndex getTrackEditWithIndex;
    MTPluginTrackReaderFunction_CreateCursorAtPresentationTimeStamp createCursorAtPresentationTimeStamp;
    MTPluginTrackReaderFunction_CreateCursorAtFirstSampleInDecodeOrder createCursorAtFirstSampleInDecodeOrder;
    MTPluginTrackReaderFunction_CreateCursorAtLastSampleInDecodeOrder createCursorAtLastSampleInDecodeOrder;
} MTPluginTrackReaderClass;

typedef struct {
    CMBaseVTable base;
    const MTPluginTrackReaderClass* pluginTrackReaderClass;
} MTPluginTrackReaderVTable;

#endif // !USE(APPLE_INTERNAL_SDK)

#endif // HAVE(MT_PLUGIN_FORMAT_READER)

// FIXME (68673547): Use actual <MediaToolbox/FigPhoto.h> and FigPhotoContainerFormat enum when we weak-link instead of soft-link MediaToolbox and CoreMedia.
#define kPALFigPhotoContainerFormat_HEIF 0
#define kPALFigPhotoContainerFormat_JFIF 1

#endif // USE(MEDIATOOLBOX)
