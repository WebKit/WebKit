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
#include <common/vp9_header_parser.h>
#include <webm/callback.h>
#include <webm/status.h>
#include <wtf/Deque.h>
#include <wtf/Function.h>
#include <wtf/MediaTime.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/WTFString.h>

typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;
typedef const struct opaqueCMFormatDescription* CMFormatDescriptionRef;
typedef struct OpaqueCMBlockBuffer *CMBlockBufferRef;

namespace webm {
class WebmParser;
}

namespace WebCore {

class SourceBufferParserWebM : public SourceBufferParser, private webm::Callback {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static MediaPlayerEnums::SupportsType isContentTypeSupported(const ContentType&);

    SourceBufferParserWebM();
    ~SourceBufferParserWebM();

    static bool isAvailable();

    const webm::Status& status() const { return m_status; }

    Type type() const { return Type::WebM; }
    void appendData(Vector<unsigned char>&&, AppendFlags = AppendFlags::None) final;
    void flushPendingMediaData() final;
    void setShouldProvideMediaDataForTrackID(bool, uint64_t) final;
    bool shouldProvideMediadataForTrackID(uint64_t) final;
    void resetParserState() final;
    void invalidate() final;

    enum class ErrorCode : int32_t {
        SourceBufferParserWebMErrorCodeStart = 2000,
        InvalidDocType,
        InvalidInitSegment,
    };

private:
    struct TrackData {
        webm::TrackEntry track;
        vp9_parser::Vp9HeaderParser headerParser;
        RetainPtr<CMFormatDescriptionRef> formatDescription;
        RetainPtr<CMBlockBufferRef> currentBlockBuffer;
        uint64_t currentBlockBufferPosition { 0 };
    };
    TrackData* trackDataForTrackNumber(uint64_t);

    // webm::Callback
    webm::Status OnElementBegin(const webm::ElementMetadata&, webm::Action*) final;
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
    webm::Status m_status;
    std::unique_ptr<webm::WebmParser> m_parser;
    bool m_initializationSegmentEncountered { false };
    uint32_t m_timescale { 1000 };
    uint64_t m_currentTimecode { 0 };

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
    State m_state { State::None };

    class StreamingVectorReader;
    UniqueRef<StreamingVectorReader> m_reader;

    Vector<TrackData> m_tracks;
    using BlockVariant = Variant<webm::Block, webm::SimpleBlock>;
    Optional<BlockVariant> m_currentBlock;
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SourceBufferParserWebM)
    static bool isType(const WebCore::SourceBufferParser& parser) { return parser.type() == WebCore::SourceBufferParser::Type::WebM; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(MEDIA_SOURCE)
