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

#include "SourceBufferParser.h"
#include <CoreAudio/CoreAudioTypes.h>
#include <CoreMedia/CMTime.h>
#include <pal/spi/cf/CoreMediaSPI.h>
#include <webm/callback.h>
#include <webm/status.h>
#include <webm/vp9_header_parser.h>
#include <wtf/Box.h>
#include <wtf/Function.h>
#include <wtf/MediaTime.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/UniqueRef.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/WTFString.h>

typedef const struct opaqueCMFormatDescription* CMFormatDescriptionRef;
typedef struct OpaqueCMBlockBuffer *CMBlockBufferRef;

namespace webm {
class WebmParser;
}

namespace WebCore {

class MediaSampleAVFObjC;

class SourceBufferParserWebM : public SourceBufferParser, private webm::Callback {
    WTF_MAKE_FAST_ALLOCATED;
public:
    class StreamingVectorReader;

    static bool isWebMFormatReaderAvailable();
    static MediaPlayerEnums::SupportsType isContentTypeSupported(const ContentType&);
    static const HashSet<String, ASCIICaseInsensitiveHash>& webmMIMETypes();
    WEBCORE_EXPORT static RefPtr<SourceBufferParserWebM> create(const ContentType&);

    SourceBufferParserWebM();
    ~SourceBufferParserWebM();

    static bool isAvailable();

    const webm::Status& status() const { return m_status; }

    Type type() const { return Type::WebM; }
    WEBCORE_EXPORT void appendData(Segment&&, CompletionHandler<void()>&& = [] { }, AppendFlags = AppendFlags::None) final;
    void flushPendingMediaData() final;
    void setShouldProvideMediaDataForTrackID(bool, uint64_t) final;
    bool shouldProvideMediadataForTrackID(uint64_t) final;
    void resetParserState() final;
    void invalidate() final;

    void flushPendingAudioBuffers();
    void setMinimumAudioSampleDuration(float);
    
    WEBCORE_EXPORT void setLogger(const WTF::Logger&, const void* identifier) final;

    void provideMediaData(RetainPtr<CMSampleBufferRef>, uint64_t, std::optional<size_t> byteRangeOffset);
    using DidParseTrimmingDataCallback = WTF::Function<void(uint64_t trackID, const MediaTime& discardPadding)>;
    void setDidParseTrimmingDataCallback(DidParseTrimmingDataCallback&& callback)
    {
        m_didParseTrimmingDataCallback = WTFMove(callback);
    }

    enum class ErrorCode : int32_t {
        SourceBufferParserWebMErrorCodeStart = 2000,
        InvalidDocType,
        InvalidInitSegment,
        ReceivedEbmlInsideSegment,
        UnsupportedVideoCodec,
        UnsupportedAudioCodec,
        ContentEncrypted,
        VariableFrameDuration,
    };

    enum class State : uint8_t {
        None,
        ReadingEbml,
        ReadEbml,
        ReadingSegment,
        ReadingInfo,
        ReadInfo,
        ReadingTracks,
        ReadingTrack,
        ReadTrack,
        ReadingCluster,
    };

    enum class CodecType : uint8_t {
        Unsupported,
        VP8,
        VP9,
        Vorbis,
        Opus,
    };

    class TrackData {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        static auto create(CodecType codecType, const webm::TrackEntry& trackEntry, SourceBufferParserWebM& parser) -> UniqueRef<TrackData>
        {
            return makeUniqueRef<TrackData>(codecType, trackEntry, Type::Unknown, parser);
        }

        enum class Type {
            Unknown,
            Audio,
            Video,
        };

        TrackData(CodecType codecType, const webm::TrackEntry& trackEntry, Type trackType, SourceBufferParserWebM& parser)
            : m_codec { codecType }
            , m_track { webm::TrackEntry { trackEntry } }
            , m_trackType { trackType }
            , m_parser { parser }
        {
        }
        virtual ~TrackData() = default;

        CodecType codec() const { return m_codec; }
        webm::TrackEntry& track() { return m_track; }
        Type trackType() const { return m_trackType; }

        RetainPtr<CMFormatDescriptionRef> formatDescription() { return m_formatDescription; }
        void setFormatDescription(RetainPtr<CMFormatDescriptionRef>&& description) { m_formatDescription = WTFMove(description); }

        SourceBufferParserWebM& parser() const { return m_parser; }
        
        virtual webm::Status consumeFrameData(webm::Reader&, const webm::FrameMetadata&, uint64_t*, const CMTime&, int)
        {
            ASSERT_NOT_REACHED();
            return webm::Status(webm::Status::kInvalidElementId);
        }

        virtual void reset()
        {
            m_currentPacketSize = std::nullopt;
            m_partialBytesRead = 0;
        }

    protected:
        std::optional<size_t> m_currentPacketSize;
        // Size of the currently parsed packet, possibly incomplete.
        size_t m_partialBytesRead { 0 };

    private:
        CodecType m_codec;
        webm::TrackEntry m_track;
        Type m_trackType;
        RetainPtr<CMFormatDescriptionRef> m_formatDescription;
        SourceBufferParserWebM& m_parser;
    };

    class VideoTrackData : public TrackData {
    public:
        static auto create(CodecType codecType, const webm::TrackEntry& trackEntry, SourceBufferParserWebM& parser) -> UniqueRef<VideoTrackData>
        {
            return makeUniqueRef<VideoTrackData>(codecType, trackEntry, parser);
        }

        VideoTrackData(CodecType codecType, const webm::TrackEntry& trackEntry, SourceBufferParserWebM& parser)
            : TrackData(codecType, trackEntry, Type::Video, parser)
        {
        }

#if ENABLE(VP9)
        void reset() final;
#endif
        webm::Status consumeFrameData(webm::Reader&, const webm::FrameMetadata&, uint64_t*, const CMTime&, int) final;

    private:
        void createSampleBuffer(const CMTime&, int, const webm::FrameMetadata&);
        const char* logClassName() const { return "VideoTrackData"; }

#if ENABLE(VP9)
        vp9_parser::Vp9HeaderParser m_headerParser;
        RetainPtr<CMBlockBufferRef> m_currentBlockBuffer;
#endif
    };

    class AudioTrackData : public TrackData {
    public:
        static auto create(CodecType codecType, const webm::TrackEntry& trackEntry, SourceBufferParserWebM& parser, float minimumSampleDuration) -> UniqueRef<AudioTrackData>
        {
            return makeUniqueRef<AudioTrackData>(codecType, trackEntry, parser, minimumSampleDuration);
        }

        AudioTrackData(CodecType codecType, const webm::TrackEntry& trackEntry, SourceBufferParserWebM& parser, float minimumSampleDuration)
            : TrackData { codecType, trackEntry, Type::Audio, parser }
            , m_minimumSampleDuration { minimumSampleDuration }
        {
        }

        webm::Status consumeFrameData(webm::Reader&, const webm::FrameMetadata&, uint64_t*, const CMTime&, int) final;
        void reset() final;
        void createSampleBuffer(std::optional<size_t> latestByteRangeOffset = std::nullopt);

    private:
        const char* logClassName() const { return "AudioTrackData"; }

        CMTime m_samplePresentationTime;
        CMTime m_packetDuration;
        Vector<uint8_t> m_packetsData;
        std::optional<size_t> m_currentPacketByteOffset;
        // Size of the complete packets parsed so far.
        size_t m_packetsBytesRead { 0 };
        size_t m_byteOffset { 0 };
        uint8_t m_framesPerPacket { 0 };
        Seconds m_frameDuration { 0_s };
        Vector<AudioStreamPacketDescription> m_packetDescriptions;

        // FIXME: 0.5 - 1.0 seconds is a better duration per sample buffer, but use 2 seconds so at least the first
        // sample buffer will play until we fix MediaSampleCursor::createSampleBuffer to deal with `endCursor`.
        float m_minimumSampleDuration { 2 };
    };

    const WTF::Logger* loggerPtr() const { return m_logger.get(); }
    const void* logIdentifier() const { return m_logIdentifier; }

private:

    TrackData* trackDataForTrackNumber(uint64_t);

    static const MemoryCompactLookupOnlyRobinHoodHashSet<String>& supportedVideoCodecs();
    static const MemoryCompactLookupOnlyRobinHoodHashSet<String>& supportedAudioCodecs();

    // webm::Callback
    webm::Status OnElementBegin(const webm::ElementMetadata&, webm::Action*) final;
    webm::Status OnElementEnd(const webm::ElementMetadata&) final;
    webm::Status OnEbml(const webm::ElementMetadata&, const webm::Ebml&) final;
    webm::Status OnSegmentBegin(const webm::ElementMetadata&, webm::Action*) final;
    webm::Status OnInfo(const webm::ElementMetadata&, const webm::Info&) final;
    webm::Status OnClusterBegin(const webm::ElementMetadata&, const webm::Cluster&, webm::Action*) final;
    webm::Status OnTrackEntry(const webm::ElementMetadata&, const webm::TrackEntry&) final;
    webm::Status OnBlockBegin(const webm::ElementMetadata&, const webm::Block&, webm::Action*) final;
    webm::Status OnBlockEnd(const webm::ElementMetadata&, const webm::Block&) final;
    webm::Status OnSimpleBlockBegin(const webm::ElementMetadata&, const webm::SimpleBlock&, webm::Action*) final;
    webm::Status OnSimpleBlockEnd(const webm::ElementMetadata&, const webm::SimpleBlock&) final;
    webm::Status OnBlockGroupBegin(const webm::ElementMetadata& , webm::Action*);
    webm::Status OnBlockGroupEnd(const webm::ElementMetadata&, const webm::BlockGroup&);
    webm::Status OnFrame(const webm::FrameMetadata&, webm::Reader*, uint64_t* bytesRemaining) final;

    std::unique_ptr<InitializationSegment> m_initializationSegment;
    Vector<std::pair<uint64_t, Ref<Uint8Array>>> m_keyIds;
    webm::Status m_status;
    std::unique_ptr<webm::WebmParser> m_parser;
    bool m_initializationSegmentEncountered { false };
    bool m_initializationSegmentProcessed { false };
    uint32_t m_timescale { 1000 };
    uint64_t m_currentTimecode { 0 };

    State m_state { State::None };

    UniqueRef<StreamingVectorReader> m_reader;

    Vector<UniqueRef<TrackData>> m_tracks;
    using BlockVariant = Variant<webm::Block, webm::SimpleBlock>;
    std::optional<BlockVariant> m_currentBlock;
    std::optional<uint64_t> m_rewindToPosition;
    float m_minimumAudioSampleDuration { 2 };

    RefPtr<const WTF::Logger> m_logger;
    const void* m_logIdentifier { nullptr };
    uint64_t m_nextChildIdentifier { 0 };
    DidParseTrimmingDataCallback m_didParseTrimmingDataCallback;
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SourceBufferParserWebM)
    static bool isType(const WebCore::SourceBufferParser& parser) { return parser.type() == WebCore::SourceBufferParser::Type::WebM; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SourceBufferParserWebM::VideoTrackData)
    static bool isType(const WebCore::SourceBufferParserWebM::TrackData& trackData) { return trackData.trackType() == WebCore::SourceBufferParserWebM::TrackData::Type::Video; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SourceBufferParserWebM::AudioTrackData)
    static bool isType(const WebCore::SourceBufferParserWebM::TrackData& trackData) { return trackData.trackType() == WebCore::SourceBufferParserWebM::TrackData::Type::Audio; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(MEDIA_SOURCE)
