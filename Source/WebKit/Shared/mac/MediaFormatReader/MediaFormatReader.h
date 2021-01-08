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
#include <WebCore/SourceBufferPrivateClient.h>
#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/Logger.h>

DECLARE_CORE_MEDIA_TRAITS(FormatReader);

namespace WebCore {
class SourceBufferParser;
}

namespace WebKit {

class MediaTrackReader;

class MediaFormatReader final : public CoreMediaWrapped<MediaFormatReader> {
public:
    using CoreMediaWrapped<MediaFormatReader>::unwrap;

    static constexpr WrapperClass wrapperClass();
    static CMBaseClassID wrapperClassID();
    static CoreMediaWrapped<MediaFormatReader>* unwrap(CMBaseObjectRef);

    static RefPtr<MediaFormatReader> create(Allocator&&);

    void startOnMainThread(MTPluginByteSourceRef);
    const MediaTime& duration() const { return m_duration; }

    const Logger& logger() const { ASSERT(m_logger); return *m_logger.get(); }
    const void* nextTrackReaderLogIdentifier(uint64_t) const;

private:
    explicit MediaFormatReader(Allocator&&);

    void parseByteSource(RetainPtr<MTPluginByteSourceRef>&&);
    void didParseTracks(WebCore::SourceBufferPrivateClient::InitializationSegment&&, uint64_t errorCode);
    void didSelectVideoTrack(WebCore::VideoTrackPrivate&, bool) { }
    void didEnableAudioTrack(WebCore::AudioTrackPrivate&, bool) { }
    void didProvideMediaData(Ref<WebCore::MediaSample>&&, uint64_t, const String&);
    void finishParsing(Ref<WebCore::SourceBufferParser>&&);

    // CMBaseClass
    String debugDescription() const final { return "WebKit::MediaFormatReader"_s; }
    OSStatus copyProperty(CFStringRef, CFAllocatorRef, void* copiedValue) final;

    // WrapperClass
    OSStatus copyTrackArray(CFArrayRef*);
    
    const void* logIdentifier() const { return m_logIdentifier; }

    RetainPtr<MTPluginByteSourceRef> m_byteSource;
    Condition m_parseTracksCondition;
    Lock m_parseTracksLock;
    MediaTime m_duration;
    Optional<OSStatus> m_parseTracksStatus;
    Vector<Ref<MediaTrackReader>> m_trackReaders;
    RefPtr<const Logger> m_logger;
    const void* m_logIdentifier;
};

constexpr MediaFormatReader::WrapperClass MediaFormatReader::wrapperClass()
{
    return {
        .version = kMTPluginFormatReader_ClassVersion_1,
        .copyTrackArray = [](WrapperRef reader, CFArrayRef* trackArrayCopy) {
            return unwrap(reader)->copyTrackArray(trackArrayCopy);
        },
    };
}

} // namespace WebKit

#endif // ENABLE(WEBM_FORMAT_READER)
