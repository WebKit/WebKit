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
#include <wtf/Box.h>
#include <wtf/TypeCasts.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS AVAsset;
OBJC_CLASS AVStreamDataParser;
OBJC_CLASS NSData;
OBJC_CLASS NSError;
OBJC_CLASS WebAVStreamDataParserListener;

typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

namespace WebCore {

class SourceBufferParserAVFObjC final : public SourceBufferParser, public CanMakeWeakPtr<SourceBufferParserAVFObjC> {
public:
    static MediaPlayerEnums::SupportsType isContentTypeSupported(const ContentType&);

    SourceBufferParserAVFObjC();
    virtual ~SourceBufferParserAVFObjC();

    AVStreamDataParser* streamDataParser() const { return m_parser.get(); }

    Type type() const { return Type::AVFObjC; }
    void appendData(Segment&&, CompletionHandler<void()>&&, AppendFlags = AppendFlags::None) final;
    void flushPendingMediaData() final;
    void setShouldProvideMediaDataForTrackID(bool, uint64_t) final;
    bool shouldProvideMediadataForTrackID(uint64_t) final;
    void resetParserState() final;
    void invalidate() final;
#if !RELEASE_LOG_DISABLED
    void setLogger(const WTF::Logger&, const void* identifier) final;
#endif

    void didParseStreamDataAsAsset(AVAsset*);
    void didFailToParseStreamDataWithError(NSError*);
    void didProvideMediaDataForTrackID(uint64_t trackID, CMSampleBufferRef, const String& mediaType, unsigned flags);
    void willProvideContentKeyRequestInitializationDataForTrackID(uint64_t trackID);
    void didProvideContentKeyRequestInitializationDataForTrackID(NSData*, uint64_t trackID);

private:
#if !RELEASE_LOG_DISABLED
    const WTF::Logger* loggerPtr() const { return m_logger.get(); }
    const void* logIdentifier() const { return m_logIdentifier; }
#endif

    RetainPtr<AVStreamDataParser> m_parser;
    RetainPtr<WebAVStreamDataParserListener> m_delegate;
    bool m_parserStateWasReset { false };

#if !RELEASE_LOG_DISABLED
    RefPtr<const WTF::Logger> m_logger;
    const void* m_logIdentifier { nullptr };
#endif
};
}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SourceBufferParserAVFObjC)
    static bool isType(const WebCore::SourceBufferParser& parser) { return parser.type() == WebCore::SourceBufferParser::Type::AVFObjC; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(MEDIA_SOURCE)
