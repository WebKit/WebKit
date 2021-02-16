/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "AppHighlightRangeData.h"

#if ENABLE(APP_HIGHLIGHTS)

#include "Document.h"
#include "DocumentMarkerController.h"
#include "HTMLBodyElement.h"
#include "Node.h"
#include "RenderedDocumentMarker.h"
#include "SharedBuffer.h"
#include "SimpleRange.h"
#include "StaticRange.h"
#include "TextIterator.h"
#include <wtf/persistence/PersistentCoders.h>

namespace WebCore {

Optional<AppHighlightRangeData> AppHighlightRangeData::create(const SharedBuffer& buffer)
{
    auto decoder = buffer.decoder();
    Optional<AppHighlightRangeData> data;
    decoder >> data;
    return data;
}

Ref<SharedBuffer> AppHighlightRangeData::toSharedBuffer() const
{
    WTF::Persistence::Encoder encoder;
    encoder << *this;
    return SharedBuffer::create(encoder.buffer(), encoder.bufferSize());
}

template<class Encoder> void AppHighlightRangeData::NodePathComponent::encode(Encoder& encoder) const
{
    encoder << identifier;
    encoder << nodeName;
    encoder << textData;
    encoder << pathIndex;
}

template<class Decoder> Optional<AppHighlightRangeData::NodePathComponent> AppHighlightRangeData::NodePathComponent::decode(Decoder& decoder)
{
    Optional<String> identifier;
    decoder >> identifier;
    if (!identifier)
        return WTF::nullopt;

    Optional<String> nodeName;
    decoder >> nodeName;
    if (!nodeName)
        return WTF::nullopt;

    Optional<String> textData;
    decoder >> textData;
    if (!textData)
        return WTF::nullopt;

    Optional<unsigned> pathIndex;
    decoder >> pathIndex;
    if (!pathIndex)
        return WTF::nullopt;

    return {{ WTFMove(*identifier), WTFMove(*nodeName), WTFMove(*textData), *pathIndex }};
}

template<class Encoder> void AppHighlightRangeData::encode(Encoder& encoder) const
{
    encoder << m_text;
    encoder << m_startContainer;
    encoder << m_startOffset;
    encoder << m_endContainer;
    encoder << m_endOffset;
}

template<class Decoder> Optional<AppHighlightRangeData> AppHighlightRangeData::decode(Decoder& decoder)
{
    Optional<String> text;
    decoder >> text;
    if (!text)
        return WTF::nullopt;

    Optional<NodePath> startContainer;
    decoder >> startContainer;
    if (!startContainer)
        return WTF::nullopt;

    Optional<unsigned> startOffset;
    decoder >> startOffset;
    if (!startOffset)
        return WTF::nullopt;

    Optional<NodePath> endContainer;
    decoder >> endContainer;
    if (!endContainer)
        return WTF::nullopt;

    Optional<unsigned> endOffset;
    decoder >> endOffset;
    if (!endOffset)
        return WTF::nullopt;


    return {{ WTFMove(*text), WTFMove(*startContainer), *startOffset, WTFMove(*endContainer), *endOffset }};
}

} // namespace WebCore

#endif
