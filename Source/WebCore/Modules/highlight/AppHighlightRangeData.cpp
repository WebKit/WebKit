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
#include "Logging.h"
#include "Node.h"
#include "RenderedDocumentMarker.h"
#include "SharedBuffer.h"
#include "SimpleRange.h"
#include "StaticRange.h"
#include "TextIterator.h"
#include <wtf/persistence/PersistentCoders.h>

namespace WebCore {

constexpr uint64_t highlightFileSignature = 0x4141504832303231; // File Signature  (A)pple(AP)plication(H)ighlights(2021)

std::optional<AppHighlightRangeData> AppHighlightRangeData::create(const FragmentedSharedBuffer& buffer)
{
    auto contiguousBuffer = buffer.makeContiguous();
    auto decoder = contiguousBuffer->decoder();
    std::optional<AppHighlightRangeData> data;
    decoder >> data;
    return data;
}

Ref<FragmentedSharedBuffer> AppHighlightRangeData::toSharedBuffer() const
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

template<class Decoder> std::optional<AppHighlightRangeData::NodePathComponent> AppHighlightRangeData::NodePathComponent::decode(Decoder& decoder)
{
    std::optional<String> identifier;
    decoder >> identifier;
    if (!identifier)
        return std::nullopt;

    std::optional<String> nodeName;
    decoder >> nodeName;
    if (!nodeName)
        return std::nullopt;

    std::optional<String> textData;
    decoder >> textData;
    if (!textData)
        return std::nullopt;

    std::optional<uint32_t> pathIndex;
    decoder >> pathIndex;
    if (!pathIndex)
        return std::nullopt;

    return {{ WTFMove(*identifier), WTFMove(*nodeName), WTFMove(*textData), *pathIndex }};
}

template<class Encoder> void AppHighlightRangeData::encode(Encoder& encoder) const
{
    static_assert(!Encoder::isIPCEncoder, "AppHighlightRangeData should not be used by IPC::Encoder");
    constexpr uint64_t currentAppHighlightVersion = 1;
    
    encoder << highlightFileSignature;
    encoder << currentAppHighlightVersion;
    encoder << m_identifier;
    encoder << m_text;
    encoder << m_startContainer;
    encoder << m_startOffset;
    encoder << m_endContainer;
    encoder << m_endOffset;
}

template<class Decoder> std::optional<AppHighlightRangeData> AppHighlightRangeData::decode(Decoder& decoder)
{
    static_assert(!Decoder::isIPCDecoder, "AppHighlightRangeData should not be used by IPC::Decoder");
    
    std::optional<uint64_t> version;
    
    std::optional<uint64_t> decodedHighlightFileSignature;
    decoder >> decodedHighlightFileSignature;
    if (!decodedHighlightFileSignature)
        return std::nullopt;
    if (decodedHighlightFileSignature != highlightFileSignature) {
        if (!decoder.rewind(sizeof(highlightFileSignature)))
            return std::nullopt;
        version = 0;
        RELEASE_LOG(AppHighlights, "Decoded legacy (v0) highlight.");
    }
    
    std::optional<String> identifier;
    if (version)
        identifier = nullString();
    else {
        decoder >> version;
        if (!version)
            return std::nullopt;
        
        decoder >> identifier;
        if (!identifier)
            return std::nullopt;
    }

    std::optional<String> text;
    decoder >> text;
    if (!text)
        return std::nullopt;

    std::optional<NodePath> startContainer;
    decoder >> startContainer;
    if (!startContainer)
        return std::nullopt;

    std::optional<uint32_t> startOffset;
    decoder >> startOffset;
    if (!startOffset)
        return std::nullopt;

    std::optional<NodePath> endContainer;
    decoder >> endContainer;
    if (!endContainer)
        return std::nullopt;

    std::optional<uint32_t> endOffset;
    decoder >> endOffset;
    if (!endOffset)
        return std::nullopt;

    return {{ WTFMove(*identifier), WTFMove(*text), WTFMove(*startContainer), *startOffset, WTFMove(*endContainer), *endOffset }};
}

} // namespace WebCore

#endif
