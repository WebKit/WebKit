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
#include <WebCore/MediaSample.h>
#include <WebCore/SampleMap.h>
#include <wtf/Condition.h>
#include <wtf/Lock.h>

DECLARE_CORE_MEDIA_TRAITS(TrackReader);

namespace WTF {
class WorkQueue;
}

namespace WebCore {
class AudioTrackPrivate;
class InbandTextTrackPrivate;
class MediaSamplesBlock;
class TrackPrivateBase;
class VideoTrackPrivate;
}

namespace WebKit {

class MediaFormatReader;

class MediaTrackReader final : public CoreMediaWrapped<MediaTrackReader> {
public:
    static constexpr WrapperClass wrapperClass();
    static CMBaseClassID wrapperClassID();
    static CoreMediaWrapped<MediaTrackReader>* unwrap(CMBaseObjectRef);
    static WTF::WorkQueue& storageQueue();

    static RefPtr<MediaTrackReader> create(Allocator&&, const MediaFormatReader&, CMMediaType, uint64_t, std::optional<bool>);

    uint64_t trackID() const { return m_trackID; }
    CMMediaType mediaType() const { return m_mediaType; }
    const MediaTime& duration() const { return m_duration; }
    MediaTime greatestPresentationTime() const;

    void setEnabled(bool enabled) { m_isEnabled = enabled ? Enabled::True : Enabled::False; }
    void addSample(WebCore::MediaSamplesBlock&&, MTPluginByteSourceRef);
    void waitForSample(Function<bool(WebCore::SampleMap&, bool)>&&) const;
    void finishParsing();

    const WTF::Logger& logger() const { return m_logger; }
    const void* nextSampleCursorLogIdentifier(uint64_t cursorID) const;

private:
    using CoreMediaWrapped<MediaTrackReader>::unwrap;

    MediaTrackReader(Allocator&&, const MediaFormatReader&, CMMediaType, uint64_t, std::optional<bool>);

    // CMBaseClass
    String debugDescription() const final { return "WebKit::MediaTrackReader"_s; }
    OSStatus copyProperty(CFStringRef, CFAllocatorRef, void* copiedValue) final;
    void finalize() final;

    // WrapperClass
    OSStatus getTrackInfo(MTPersistentTrackID*, CMMediaType*);
    OSStatus createCursorAtPresentationTimeStamp(CMTime, MTPluginSampleCursorRef*);
    OSStatus createCursorAtFirstSampleInDecodeOrder(MTPluginSampleCursorRef*);
    OSStatus createCursorAtLastSampleInDecodeOrder(MTPluginSampleCursorRef*);

    struct SampleStorage {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        ~SampleStorage() { ASSERT(!isMainRunLoop()); }
        WebCore::SampleMap sampleMap;
        bool hasAllSamples { false };
    };

    const char* mediaTypeString() const;
    const char* logClassName() const { return "MediaTrackReader"; }
    const void* logIdentifier() const { return m_logIdentifier; }
    WTFLogChannel& logChannel() const;

    enum Enabled : uint8_t { Unknown, False, True };

    const uint64_t m_trackID;
    const CMMediaType m_mediaType;
    const MediaTime m_duration;
    std::atomic<Enabled> m_isEnabled { Enabled::Unknown };
    mutable Condition m_sampleStorageCondition;
    mutable Lock m_sampleStorageLock;
    mutable std::unique_ptr<SampleStorage> m_sampleStorage WTF_GUARDED_BY_LOCK(m_sampleStorageLock);
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
    RetainPtr<CMFormatDescriptionRef> m_formatDescription;
    RefPtr<const WebCore::TrackInfo> m_trackInfo;
};

constexpr MediaTrackReader::WrapperClass MediaTrackReader::wrapperClass()
{
    return {
        .version = kMTPluginTrackReader_ClassVersion_1,
        .getTrackInfo = [](MTPluginTrackReaderRef trackReader, MTPersistentTrackID* trackID, CMMediaType* mediaType) {
            return unwrap(trackReader)->getTrackInfo(trackID, mediaType);
        },
        .createCursorAtPresentationTimeStamp = [](MTPluginTrackReaderRef trackReader, CMTime timestamp, MTPluginSampleCursorRef* cursor) {
            return unwrap(trackReader)->createCursorAtPresentationTimeStamp(timestamp, cursor);
        },
        .createCursorAtFirstSampleInDecodeOrder = [](MTPluginTrackReaderRef trackReader, MTPluginSampleCursorRef* cursor) {
            return unwrap(trackReader)->createCursorAtFirstSampleInDecodeOrder(cursor);
        },
        .createCursorAtLastSampleInDecodeOrder = [](MTPluginTrackReaderRef trackReader, MTPluginSampleCursorRef* cursor) {
            return unwrap(trackReader)->createCursorAtLastSampleInDecodeOrder(cursor);
        },
    };
}

} // namespace WebKit

#endif // ENABLE(WEBM_FORMAT_READER)
