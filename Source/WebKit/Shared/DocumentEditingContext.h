/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#include "ArgumentCoders.h"
#include <WebCore/AttributedString.h>
#include <WebCore/ElementContext.h>
#include <WebCore/FloatRect.h>
#include <WebCore/TextGranularity.h>
#include <wtf/OptionSet.h>
#include <wtf/Optional.h>
#include <wtf/Vector.h>

OBJC_CLASS UIWKDocumentContext;

namespace WebKit {

struct DocumentEditingContextRequest {
    enum class Options : uint8_t {
        Text = 1 << 0,
        AttributedText = 1 << 1,
        Rects = 1 << 2,
        Spatial = 1 << 3,
        Annotation = 1 << 4,
        MarkedTextRects = 1 << 5,
        SpatialAndCurrentSelection = 1 << 6,
    };

    OptionSet<Options> options;

    WebCore::TextGranularity surroundingGranularity { WebCore::TextGranularity::CharacterGranularity };
    int64_t granularityCount { 0 };

    WebCore::FloatRect rect;

    Optional<WebCore::ElementContext> textInputContext;
};

struct DocumentEditingContext {
    UIWKDocumentContext *toPlatformContext(OptionSet<WebKit::DocumentEditingContextRequest::Options>);

    WebCore::AttributedString contextBefore;
    WebCore::AttributedString selectedText;
    WebCore::AttributedString contextAfter;
    WebCore::AttributedString markedText;
    WebCore::AttributedString annotatedText;

    struct Range {
        uint64_t location { 0 };
        uint64_t length { 0 };
    };

    Range selectedRangeInMarkedText;

    struct TextRectAndRange {
        WebCore::FloatRect rect;
        Range range;
    };

    Vector<TextRectAndRange> textRects;
};

}

namespace IPC {
template<> struct ArgumentCoder<WebKit::DocumentEditingContext::Range> {
    static void encode(Encoder&, const WebKit::DocumentEditingContext::Range&);
    static Optional<WebKit::DocumentEditingContext::Range> decode(Decoder&);
};

template<> struct ArgumentCoder<WebKit::DocumentEditingContext::TextRectAndRange> {
    static void encode(Encoder&, const WebKit::DocumentEditingContext::TextRectAndRange&);
    static Optional<WebKit::DocumentEditingContext::TextRectAndRange> decode(Decoder&);
};

template<> struct ArgumentCoder<WebKit::DocumentEditingContext> {
    static void encode(Encoder&, const WebKit::DocumentEditingContext&);
    static Optional<WebKit::DocumentEditingContext> decode(Decoder&);
};

template<> struct ArgumentCoder<WebKit::DocumentEditingContextRequest> {
    static void encode(Encoder&, const WebKit::DocumentEditingContextRequest&);
    static Optional<WebKit::DocumentEditingContextRequest> decode(Decoder&);
};
}

namespace WTF {

template<> struct EnumTraits<WebKit::DocumentEditingContextRequest::Options> {
    using values = EnumValues<
        WebKit::DocumentEditingContextRequest::Options,
        WebKit::DocumentEditingContextRequest::Options::Text,
        WebKit::DocumentEditingContextRequest::Options::AttributedText,
        WebKit::DocumentEditingContextRequest::Options::Rects,
        WebKit::DocumentEditingContextRequest::Options::Spatial,
        WebKit::DocumentEditingContextRequest::Options::Annotation,
        WebKit::DocumentEditingContextRequest::Options::MarkedTextRects,
        WebKit::DocumentEditingContextRequest::Options::SpatialAndCurrentSelection
    >;
};

} // namespace WTF

#endif // PLATFORM(IOS_FAMILY)
