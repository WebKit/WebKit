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

#include <wtf/Forward.h>
#include <wtf/OptionSet.h>
#include <wtf/Optional.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

#if ENABLE(APP_HIGHLIGHTS)

class SharedBuffer;

class AppHighlightRangeData {
public:
    struct NodePathComponent {
        String identifier;
        String nodeName;
        String textData;
        unsigned pathIndex { 0 };

        NodePathComponent(String&& elementIdentifier, String&& name, String&& data, unsigned index)
            : identifier(WTFMove(elementIdentifier))
            , nodeName(WTFMove(name))
            , textData(WTFMove(data))
            , pathIndex(index)
        {
        }

        NodePathComponent(const String& elementIdentifier, const String& name, const String& data, unsigned index)
            : identifier(elementIdentifier)
            , nodeName(name)
            , textData(data)
            , pathIndex(index)
        {
        }

        bool operator==(const NodePathComponent& other) const
        {
            return identifier == other.identifier && nodeName == other.nodeName && textData == other.textData && pathIndex == other.pathIndex;
        }

        bool operator!=(const NodePathComponent& other) const
        {
            return !(*this == other);
        }

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static Optional<NodePathComponent> decode(Decoder&);
    };

    using NodePath = Vector<NodePathComponent>;

    AppHighlightRangeData(const AppHighlightRangeData&) = default;
    AppHighlightRangeData() = default;
    AppHighlightRangeData(String&& text, NodePath&& startContainer, unsigned startOffset, NodePath&& endContainer, unsigned endOffset)
        : m_text(WTFMove(text))
        , m_startContainer(WTFMove(startContainer))
        , m_startOffset(startOffset)
        , m_endContainer(WTFMove(endContainer))
        , m_endOffset(endOffset)
    {
    }

    AppHighlightRangeData(const String& text, const NodePath& startContainer, unsigned startOffset, const NodePath& endContainer, unsigned endOffset)
        : m_text(text)
        , m_startContainer(startContainer)
        , m_startOffset(startOffset)
        , m_endContainer(endContainer)
        , m_endOffset(endOffset)
    {
    }

    const String& text() const { return m_text; }
    const NodePath& startContainer() const { return m_startContainer; }
    unsigned startOffset() const { return m_startOffset; }
    const NodePath& endContainer() const { return m_endContainer; }
    unsigned endOffset() const { return m_endOffset; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<AppHighlightRangeData> decode(Decoder&);

private:
    String m_text;
    NodePath m_startContainer;
    unsigned m_startOffset { 0 };
    NodePath m_endContainer;
    unsigned m_endOffset { 0 };
};

class AppHighlightListData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT static AppHighlightListData create(const SharedBuffer&);

    AppHighlightListData(const AppHighlightListData&) = default;
    AppHighlightListData() = default;
    AppHighlightListData(Vector<AppHighlightRangeData>&& annotationRanges)
        : m_annotationRanges(WTFMove(annotationRanges))
    {
    }

    const Vector<AppHighlightRangeData>& ranges() const { return m_annotationRanges; }
    void setRanges(Vector<AppHighlightRangeData>&& ranges) { m_annotationRanges = WTFMove(ranges); }
    void addRanges(Vector<AppHighlightRangeData>&& ranges) { m_annotationRanges.appendVector(WTFMove(ranges)); }

    Ref<SharedBuffer> toSharedBuffer() const;

    size_t size() const { return m_annotationRanges.size(); }
    bool isEmpty() const { return m_annotationRanges.isEmpty(); }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<AppHighlightListData> decode(Decoder&);

private:

    Vector<AppHighlightRangeData> m_annotationRanges;
};

template<class Encoder> void AppHighlightRangeData::NodePathComponent::encode(Encoder& encoder) const
{
    encoder << identifier;
    encoder << nodeName;
    encoder << textData;
    encoder << static_cast<uint64_t>(pathIndex);
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

    Optional<uint64_t> pathIndex;
    decoder >> pathIndex;
    if (!pathIndex)
        return WTF::nullopt;

    return {{ WTFMove(*identifier), WTFMove(*nodeName), WTFMove(*textData), static_cast<unsigned>(*pathIndex) }};
}

template<class Encoder> void AppHighlightRangeData::encode(Encoder& encoder) const
{
    encoder << m_text;
    encoder << m_startContainer;
    encoder << static_cast<uint64_t>(m_startOffset);
    encoder << m_endContainer;
    encoder << static_cast<uint64_t>(m_endOffset);
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

    Optional<uint64_t> startOffset;
    decoder >> startOffset;
    if (!startOffset)
        return WTF::nullopt;

    Optional<NodePath> endContainer;
    decoder >> endContainer;
    if (!endContainer)
        return WTF::nullopt;

    Optional<uint64_t> endOffset;
    decoder >> endOffset;
    if (!endOffset)
        return WTF::nullopt;


    return {{
        WTFMove(*text),
        WTFMove(*startContainer),
        static_cast<unsigned>(*startOffset),
        WTFMove(*endContainer),
        static_cast<unsigned>(*endOffset),
    }};
}

template<class Encoder> void AppHighlightListData::encode(Encoder& encoder) const
{
    encoder << m_annotationRanges;
}

template<class Decoder> Optional<AppHighlightListData> AppHighlightListData::decode(Decoder& decoder)
{
    Optional<Vector<AppHighlightRangeData>> annotationRanges;
    decoder >> annotationRanges;
    if (!annotationRanges)
        return WTF::nullopt;

    return {{ WTFMove(*annotationRanges) }};
}

#endif

} // namespace WebCore
