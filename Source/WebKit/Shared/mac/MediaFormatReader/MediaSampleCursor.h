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

#if ENABLE(WEBM_FORMAT_READER)

#include "CoreMediaWrapped.h"
#include <WebCore/SampleMap.h>
#include <wtf/MediaTime.h>
#include <wtf/Variant.h>

DECLARE_CORE_MEDIA_TRAITS(SampleCursor);

namespace WTF {
class Logger;
}

namespace WebCore {
class MediaSample;
};

namespace WebKit {

class MediaTrackReader;

class MediaSampleCursor : public CoreMediaWrapped<MediaSampleCursor>, ThreadSafeIdentified<MediaSampleCursor> {
public:
    using DecodeOrderIterator = DecodeOrderSampleMap::iterator;
    using PresentationOrderIterator = PresentationOrderSampleMap::iterator;
    using Locator = Variant<MediaTime, DecodeOrderIterator, PresentationOrderIterator>;

    struct Timing {
        MediaTime decodeTime;
        MediaTime presentationTime;
        MediaTime duration;
    };

    static constexpr WrapperClass wrapperClass();
    static CMBaseClassID wrapperClassID();
    static CoreMediaWrapped<MediaSampleCursor>* unwrap(CMBaseObjectRef);

    static RefPtr<MediaSampleCursor> copy(Allocator&&, const MediaSampleCursor&);
    static RefPtr<MediaSampleCursor> createAtPresentationTime(Allocator&&, MediaTrackReader&, MediaTime);

private:
    using CoreMediaWrapped<MediaSampleCursor>::unwrap;

    MediaSampleCursor(Allocator&&, MediaTrackReader&, Locator);
    MediaSampleCursor(Allocator&&, const MediaSampleCursor&);

    template<typename OrderedMap> Optional<typename OrderedMap::iterator> locateIterator(OrderedMap&, bool hasAllSamples) const;
    WebCore::MediaSample* locateMediaSample(WebCore::SampleMap&, bool hasAllSamples) const;
    Timing locateTiming(WebCore::SampleMap&, bool hasAllSamples) const;

    template<typename Function> OSStatus getSampleMap(Function&&) const;
    template<typename Function> OSStatus getMediaSample(Function&&) const;
    template<typename Function> OSStatus getTiming(Function&&) const;

    template<typename OrderedMap> OSStatus stepInOrderedMap(int64_t, int64_t&);
    OSStatus stepInPresentationTime(const MediaTime&, Boolean& wasPinned);

    // CoreMediaWrapped
    String debugDescription() const final { return "WebKit::MediaSampleCursor"_s; }
    OSStatus copyProperty(CFStringRef, CFAllocatorRef, void* copiedValue) final;

    // WrapperClass
    OSStatus copy(MTPluginSampleCursorRef*) const;
    OSStatus stepInDecodeOrderAndReportStepsTaken(int64_t, int64_t*);
    OSStatus stepInPresentationOrderAndReportStepsTaken(int64_t, int64_t*);
    OSStatus stepByDecodeTime(CMTime, Boolean*);
    OSStatus stepByPresentationTime(CMTime, Boolean*);
    CFComparisonResult compareInDecodeOrder(MTPluginSampleCursorRef) const;
    OSStatus getSampleTiming(CMSampleTimingInfo*) const;
    OSStatus getSyncInfo(MTPluginSampleCursorSyncInfo*) const;
    OSStatus copyFormatDescription(CMFormatDescriptionRef*) const;
    OSStatus copySampleLocation(MTPluginSampleCursorStorageRange*, MTPluginByteSourceRef*) const;
    OSStatus getPlayableHorizon(CMTime*) const;

    const WTF::Logger& logger() const { return m_logger; }
    const char* logClassName() const { return "MediaSampleCursor"; }
    const void* logIdentifier() const { return m_logIdentifier; }
    WTFLogChannel& logChannel() const;

    Ref<MediaTrackReader> m_trackReader;
    mutable Locator m_locator;
    mutable Lock m_locatorLock;
    Ref<const WTF::Logger> m_logger;
    const void* m_logIdentifier;
};

constexpr MediaSampleCursor::WrapperClass MediaSampleCursor::wrapperClass()
{
    return {
#if HAVE(MT_PLUGIN_SAMPLE_CURSOR_PLAYABLE_HORIZON)
        .version = kMTPluginSampleCursor_ClassVersion_4,
#else
        .version = kMTPluginSampleCursor_ClassVersion_3,
#endif
        .copy = [](MTPluginSampleCursorRef sampleCursor, MTPluginSampleCursorRef* sampleCursorCopy) {
            return unwrap(sampleCursor)->copy(sampleCursorCopy);
        },
        .stepInDecodeOrderAndReportStepsTaken = [](MTPluginSampleCursorRef sampleCursor, int64_t stepCount, int64_t* stepsTaken) {
            return unwrap(sampleCursor)->stepInDecodeOrderAndReportStepsTaken(stepCount, stepsTaken);
        },
        .stepInPresentationOrderAndReportStepsTaken = [](MTPluginSampleCursorRef sampleCursor, int64_t stepCount, int64_t* stepsTaken) {
            return unwrap(sampleCursor)->stepInPresentationOrderAndReportStepsTaken(stepCount, stepsTaken);
        },
        .stepByDecodeTime = [](MTPluginSampleCursorRef sampleCursor, CMTime deltaDecodeTime, Boolean* positionWasPinned) {
            return unwrap(sampleCursor)->stepByDecodeTime(deltaDecodeTime, positionWasPinned);
        },
        .stepByPresentationTime = [](MTPluginSampleCursorRef sampleCursor, CMTime deltaPresentationTime, Boolean* positionWasPinned) {
            return unwrap(sampleCursor)->stepByPresentationTime(deltaPresentationTime, positionWasPinned);
        },
        .compareInDecodeOrder = [](MTPluginSampleCursorRef sampleCursor, MTPluginSampleCursorRef otherSampleCursor) {
            return unwrap(sampleCursor)->compareInDecodeOrder(otherSampleCursor);
        },
        .getSampleTiming = [](MTPluginSampleCursorRef sampleCursor, CMSampleTimingInfo* sampleTimingInfo) {
            return unwrap(sampleCursor)->getSampleTiming(sampleTimingInfo);
        },
        .getSyncInfo = [](MTPluginSampleCursorRef sampleCursor, MTPluginSampleCursorSyncInfo* syncInfo) -> OSStatus {
            return unwrap(sampleCursor)->getSyncInfo(syncInfo);
        },
        .copyFormatDescription = [](MTPluginSampleCursorRef sampleCursor, CMFormatDescriptionRef* formatDescription) {
            return unwrap(sampleCursor)->copyFormatDescription(formatDescription);
        },
        .copySampleLocation = [](MTPluginSampleCursorRef sampleCursor, MTPluginSampleCursorStorageRange* storageRange, MTPluginByteSourceRef* byteSource) {
            return unwrap(sampleCursor)->copySampleLocation(storageRange, byteSource);
        },
#if HAVE(MT_PLUGIN_SAMPLE_CURSOR_PLAYABLE_HORIZON)
        .getPlayableHorizon = [](MTPluginSampleCursorRef sampleCursor, CMTime* playableHorizon) {
            return unwrap(sampleCursor)->getPlayableHorizon(playableHorizon);
        },
#endif
    };
}

} // namespace WebKit

#endif // ENABLE(WEBM_FORMAT_READER)
