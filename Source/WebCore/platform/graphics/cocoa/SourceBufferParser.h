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

#if ENABLE(MEDIA_SOURCE)

#include "MediaPlayerEnums.h"
#include "SourceBufferPrivateClient.h"
#include <JavaScriptCore/Forward.h>
#include <pal/spi/cocoa/MediaToolboxSPI.h>
#include <variant>
#include <wtf/CompletionHandler.h>
#include <wtf/Expected.h>
#include <wtf/RefCounted.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WTF {
class Logger;
}

namespace WebCore {

class ContentType;
class MediaSampleAVFObjC;
class SharedBuffer;

class WEBCORE_EXPORT SourceBufferParser : public ThreadSafeRefCounted<SourceBufferParser> {
public:
    static MediaPlayerEnums::SupportsType isContentTypeSupported(const ContentType&);

    static RefPtr<SourceBufferParser> create(const ContentType&, bool webMParserEnabled);
    virtual ~SourceBufferParser() = default;

    enum class Type : uint8_t {
        AVFObjC,
        WebM,
    };
    virtual Type type() const = 0;
    enum class AppendFlags : uint8_t {
        None,
        Discontinuity,
    };

    class Segment {
    public:
#if HAVE(MT_PLUGIN_FORMAT_READER)
        Segment(RetainPtr<MTPluginByteSourceRef>&&);
#endif
        Segment(Ref<SharedBuffer>&&);
        Segment(Segment&&) = default;
        Ref<SharedBuffer> takeSharedBuffer();
        // Will return nullptr if Segment's backend isn't a SharedBuffer.
        RefPtr<SharedBuffer> getSharedBuffer() const;

        size_t size() const;

        enum class ReadError { EndOfFile, FatalError };
        using ReadResult = Expected<size_t, ReadError>;

        ReadResult read(size_t position, size_t, uint8_t* destination) const;

    private:
        std::variant<
#if HAVE(MT_PLUGIN_FORMAT_READER)
            RetainPtr<MTPluginByteSourceRef>,
#endif
            Ref<SharedBuffer>
        > m_segment;
    };

    using CallOnClientThreadCallback = Function<void(Function<void()>&&)>;
    void setCallOnClientThreadCallback(CallOnClientThreadCallback&&);

    // appendData will be called on the SourceBufferPrivateAVFObjC data parser queue.
    // Other methods will be called on the main thread, but only once appendData has returned.
    virtual void appendData(Segment&&, CompletionHandler<void()>&& = [] { }, AppendFlags = AppendFlags::None) = 0;
    virtual void flushPendingMediaData() = 0;
    virtual void setShouldProvideMediaDataForTrackID(bool, uint64_t) = 0;
    virtual bool shouldProvideMediadataForTrackID(uint64_t) = 0;
    virtual void resetParserState() = 0;
    virtual void invalidate() = 0;
    virtual void setMinimumAudioSampleDuration(float);
#if !RELEASE_LOG_DISABLED
    virtual void setLogger(const Logger&, const void* logIdentifier) = 0;
#endif

    // Will be called on the main thread.
    using InitializationSegment = SourceBufferPrivateClient::InitializationSegment;
    using DidParseInitializationDataCallback = Function<void(InitializationSegment&&)>;
    void setDidParseInitializationDataCallback(DidParseInitializationDataCallback&& callback)
    {
        m_didParseInitializationDataCallback = WTFMove(callback);
    }

    // Will be called on the main thread.
    using DidEncounterErrorDuringParsingCallback = Function<void(uint64_t errorCode)>;
    void setDidEncounterErrorDuringParsingCallback(DidEncounterErrorDuringParsingCallback&& callback)
    {
        m_didEncounterErrorDuringParsingCallback = WTFMove(callback);
    }

    // Will be called on the main thread.
    using DidProvideMediaDataCallback = Function<void(Ref<MediaSampleAVFObjC>&&, uint64_t trackID, const String& mediaType)>;
    void setDidProvideMediaDataCallback(DidProvideMediaDataCallback&& callback)
    {
        m_didProvideMediaDataCallback = WTFMove(callback);
    }

    // Will be called synchronously on the parser thead.
    using WillProvideContentKeyRequestInitializationDataForTrackIDCallback = Function<void(uint64_t trackID)>;
    void setWillProvideContentKeyRequestInitializationDataForTrackIDCallback(WillProvideContentKeyRequestInitializationDataForTrackIDCallback&& callback)
    {
        m_willProvideContentKeyRequestInitializationDataForTrackIDCallback = WTFMove(callback);
    }

    // Will be called synchronously on the parser thead.
    using DidProvideContentKeyRequestInitializationDataForTrackIDCallback = Function<void(Ref<SharedBuffer>&&, uint64_t trackID)>;
    void setDidProvideContentKeyRequestInitializationDataForTrackIDCallback(DidProvideContentKeyRequestInitializationDataForTrackIDCallback&& callback)
    {
        m_didProvideContentKeyRequestInitializationDataForTrackIDCallback = WTFMove(callback);
    }

    // Will be called on the main thread.
    using DidProvideContentKeyRequestIdentifierForTrackIDCallback = Function<void(Ref<SharedBuffer>&&, uint64_t trackID)>;
    void setDidProvideContentKeyRequestIdentifierForTrackIDCallback(DidProvideContentKeyRequestIdentifierForTrackIDCallback&& callback)
    {
        m_didProvideContentKeyRequestIdentifierForTrackIDCallback = WTFMove(callback);
    }

protected:
    SourceBufferParser();

    CallOnClientThreadCallback m_callOnClientThreadCallback;
    DidParseInitializationDataCallback m_didParseInitializationDataCallback;
    DidEncounterErrorDuringParsingCallback m_didEncounterErrorDuringParsingCallback;
    DidProvideMediaDataCallback m_didProvideMediaDataCallback;
    WillProvideContentKeyRequestInitializationDataForTrackIDCallback m_willProvideContentKeyRequestInitializationDataForTrackIDCallback;
    DidProvideContentKeyRequestInitializationDataForTrackIDCallback m_didProvideContentKeyRequestInitializationDataForTrackIDCallback;
    DidProvideContentKeyRequestIdentifierForTrackIDCallback m_didProvideContentKeyRequestIdentifierForTrackIDCallback;
};

}

#endif // ENABLE(MEDIA_SOURCE)
