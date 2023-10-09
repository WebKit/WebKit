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

#import "config.h"
#import "DocumentEditingContext.h"

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import "WebCoreArgumentCoders.h"
#import <WebCore/ElementContext.h>
#import <wtf/cocoa/VectorCocoa.h>

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
        [platformContext setContextBefore:contextBefore.nsAttributedString().get()];
        [platformContext setSelectedText:selectedText.nsAttributedString().get()];
        [platformContext setContextAfter:contextAfter.nsAttributedString().get()];
        [platformContext setMarkedText:markedText.nsAttributedString().get()];
    } else if (options.contains(DocumentEditingContextRequest::Options::Text)) {
        [platformContext setContextBefore:[contextBefore.nsAttributedString() string]];
        [platformContext setSelectedText:[selectedText.nsAttributedString() string]];
        [platformContext setContextAfter:[contextAfter.nsAttributedString() string]];
        [platformContext setMarkedText:[markedText.nsAttributedString() string]];
    }

    [platformContext setSelectedRangeInMarkedText:toNSRange(selectedRangeInMarkedText)];

    for (const auto& rect : textRects)
        [platformContext addTextRect:rect.rect forCharacterRange:toNSRange(rect.range)];

    [platformContext setAnnotatedText:annotatedText.nsAttributedString().get()];

#if HAVE(AUTOCORRECTION_ENHANCEMENTS)
    if (options.contains(DocumentEditingContextRequest::Options::AutocorrectedRanges)) {
        auto ranges = createNSArray(autocorrectedRanges, [&] (DocumentEditingContext::Range range) {
            return [NSValue valueWithRange:toNSRange(range)];
        });

        if ([platformContext respondsToSelector:@selector(setAutocorrectedRanges:)])
            [platformContext setAutocorrectedRanges:ranges.get()];
    }
#endif

    return platformContext.autorelease();
#else
    UNUSED_PARAM(options);
    return nil;
#endif
}

}

#endif
