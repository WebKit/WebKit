/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "DocumentEditingContext.h"

#if PLATFORM(IOS_FAMILY)

#include "TextInputContext.h"
#include "UIKitSPI.h"
#include "WebCoreArgumentCoders.h"

namespace WebKit {

static inline NSRange toNSRange(DocumentEditingContext::Range range)
{
    return NSMakeRange(range.location, range.length);
}

UIWKDocumentContext *DocumentEditingContext::toPlatformContext(OptionSet<DocumentEditingContextRequest::Options> options)
{
#if HAVE(UI_WK_DOCUMENT_CONTEXT)
    auto platformContext = adoptNS([[UIWKDocumentContext alloc] init]);

    if (options.contains(DocumentEditingContextRequest::Options::AttributedText)) {
        [platformContext setContextBefore:contextBefore.string.get()];
        [platformContext setSelectedText:selectedText.string.get()];
        [platformContext setContextAfter:contextAfter.string.get()];
        [platformContext setMarkedText:markedText.string.get()];
    } else if (options.contains(DocumentEditingContextRequest::Options::Text)) {
        [platformContext setContextBefore:[contextBefore.string string]];
        [platformContext setSelectedText:[selectedText.string string]];
        [platformContext setContextAfter:[contextAfter.string string]];
        [platformContext setMarkedText:[markedText.string string]];
    }

    [platformContext setSelectedRangeInMarkedText:toNSRange(selectedRangeInMarkedText)];

    for (const auto& rect : textRects)
        [platformContext addTextRect:rect.rect forCharacterRange:toNSRange(rect.range)];

    [platformContext setAnnotatedText:annotatedText.string.get()];

    return platformContext.autorelease();
#else
    UNUSED_PARAM(options);
    return nil;
#endif
}

}

namespace IPC {

void ArgumentCoder<WebKit::DocumentEditingContext::Range>::encode(Encoder& encoder, const WebKit::DocumentEditingContext::Range& range)
{
    encoder << range.location;
    encoder << range.length;
}

Optional<WebKit::DocumentEditingContext::Range> ArgumentCoder<WebKit::DocumentEditingContext::Range>::decode(Decoder& decoder)
{
    WebKit::DocumentEditingContext::Range range;

    if (!decoder.decode(range.location))
        return WTF::nullopt;
    if (!decoder.decode(range.length))
        return WTF::nullopt;

    return range;
}

void ArgumentCoder<WebKit::DocumentEditingContext::TextRectAndRange>::encode(Encoder& encoder, const WebKit::DocumentEditingContext::TextRectAndRange& rect)
{
    encoder << rect.rect;
    encoder << rect.range;
}

Optional<WebKit::DocumentEditingContext::TextRectAndRange> ArgumentCoder<WebKit::DocumentEditingContext::TextRectAndRange>::decode(Decoder& decoder)
{
    WebKit::DocumentEditingContext::TextRectAndRange rect;

    if (!decoder.decode(rect.rect))
        return WTF::nullopt;

    Optional<WebKit::DocumentEditingContext::Range> range;
    decoder >> range;
    if (!range)
        return WTF::nullopt;
    rect.range = *range;

    return rect;
}

void ArgumentCoder<WebKit::DocumentEditingContext>::encode(Encoder& encoder, const WebKit::DocumentEditingContext& context)
{
    encoder << context.contextBefore;
    encoder << context.selectedText;
    encoder << context.contextAfter;
    encoder << context.markedText;
    encoder << context.annotatedText;

    encoder << context.selectedRangeInMarkedText;

    encoder << context.textRects;
}

Optional<WebKit::DocumentEditingContext> ArgumentCoder<WebKit::DocumentEditingContext>::decode(Decoder& decoder)
{
    WebKit::DocumentEditingContext context;

    Optional<WebKit::AttributedString> contextBefore;
    decoder >> contextBefore;
    if (!contextBefore)
        return WTF::nullopt;
    context.contextBefore = *contextBefore;

    Optional<WebKit::AttributedString> selectedText;
    decoder >> selectedText;
    if (!selectedText)
        return WTF::nullopt;
    context.selectedText = *selectedText;

    Optional<WebKit::AttributedString> contextAfter;
    decoder >> contextAfter;
    if (!contextAfter)
        return WTF::nullopt;
    context.contextAfter = *contextAfter;

    Optional<WebKit::AttributedString> markedText;
    decoder >> markedText;
    if (!markedText)
        return WTF::nullopt;
    context.markedText = *markedText;

    Optional<WebKit::AttributedString> annotatedText;
    decoder >> annotatedText;
    if (!annotatedText)
        return WTF::nullopt;
    context.annotatedText = *annotatedText;

    Optional<WebKit::DocumentEditingContext::Range> selectedRangeInMarkedText;
    decoder >> selectedRangeInMarkedText;
    if (!selectedRangeInMarkedText)
        return WTF::nullopt;
    context.selectedRangeInMarkedText = *selectedRangeInMarkedText;

    if (!decoder.decode(context.textRects))
        return WTF::nullopt;

    return context;
}

void ArgumentCoder<WebKit::DocumentEditingContextRequest>::encode(Encoder& encoder, const WebKit::DocumentEditingContextRequest& request)
{
    encoder << request.options;
    encoder << request.surroundingGranularity;
    encoder << request.granularityCount;
    encoder << request.rect;
    encoder << request.textInputContext;
}

Optional<WebKit::DocumentEditingContextRequest> ArgumentCoder<WebKit::DocumentEditingContextRequest>::decode(Decoder& decoder)
{
    WebKit::DocumentEditingContextRequest request;

    if (!decoder.decode(request.options))
        return WTF::nullopt;

    if (!decoder.decode(request.surroundingGranularity))
        return WTF::nullopt;

    if (!decoder.decode(request.granularityCount))
        return WTF::nullopt;

    if (!decoder.decode(request.rect))
        return WTF::nullopt;

    Optional<Optional<WebKit::TextInputContext>> optionalTextInputContext;
    decoder >> optionalTextInputContext;
    if (!optionalTextInputContext)
        return WTF::nullopt;

    request.textInputContext = optionalTextInputContext.value();

    return request;
}

}

#endif
