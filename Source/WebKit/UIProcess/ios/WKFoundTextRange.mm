/*
 * Copyright (C) 2012-2026 Apple Inc. All rights reserved.
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
#import "WKFoundTextRange.h"

#if HAVE(UIFINDINTERACTION)

#import "WebFoundTextRange.h"
#import <wtf/cocoa/VectorCocoa.h>

@implementation WKFoundTextRange

+ (WKFoundTextRange *)foundTextRangeWithWebFoundTextRange:(WebKit::WebFoundTextRange)webRange
{
    RetainPtr range = WTF::switchOn(webRange.data,
        [](const WebKit::WebFoundTextRange::DOMData& domData) -> RetainPtr<WKFoundTextRange> {
            RetainPtr foundDOMTextRange = adoptNS([[WKFoundDOMTextRange alloc] init]);
            [foundDOMTextRange setLocation:domData.location];
            [foundDOMTextRange setLength:domData.length];
            return foundDOMTextRange;
        },
        [](const WebKit::WebFoundTextRange::PDFData& pdfData) -> RetainPtr<WKFoundTextRange> {
            RetainPtr foundPDFTextRange = adoptNS([[WKFoundPDFTextRange alloc] init]);
            [foundPDFTextRange setStartPage:pdfData.startPage];
            [foundPDFTextRange setStartPageOffset:pdfData.startOffset];
            [foundPDFTextRange setEndPage:pdfData.endPage];
            [foundPDFTextRange setEndPageOffset:pdfData.endOffset];
            return foundPDFTextRange;
        },
        [](const WebKit::WebFoundTextRange::CueData& cueData) -> RetainPtr<WKFoundTextRange> {
            RetainPtr foundCueTextRange = adoptNS([[WKFoundCueTextRange alloc] init]);
            [foundCueTextRange setMediaElementIdentifier:cueData.mediaElementIdentifier.toUInt64()];
            [foundCueTextRange setDocumentOffset:cueData.documentOffset];
            [foundCueTextRange setSeekTimeMilliseconds:cueData.seekTimeMilliseconds];
            return foundCueTextRange;
        }
    );

    [range setPathToFrame:createNSArray(webRange.pathToFrame, [](uint64_t index) {
        return @(index);
    }).get()];
    [range setOrder:webRange.order];
    return range.autorelease();
}

- (void)dealloc
{
    [super dealloc];
}

- (BOOL)isEmpty
{
    return NO;
}

- (WebKit::WebFoundTextRange)webFoundTextRange
{
    return { };
}

@end

@implementation WKFoundTextPosition
@end

@implementation WKFoundDOMTextRange

- (WKFoundDOMTextPosition *)start
{
    WKFoundDOMTextPosition *position = [WKFoundDOMTextPosition textPositionWithOffset:self.location order:self.order];
    return position;
}

- (WKFoundDOMTextPosition *)end
{
    WKFoundDOMTextPosition *position = [WKFoundDOMTextPosition textPositionWithOffset:(self.location + self.length) order:self.order];
    return position;
}

- (WebKit::WebFoundTextRange)webFoundTextRange
{
    WebKit::WebFoundTextRange::DOMData data { self.location, self.length };
    auto pathToFrameVector = makeVector(self.pathToFrame, [](id number) -> std::optional<uint64_t> {
        return [number unsignedLongValue];
    });
    return { data, WTF::move(pathToFrameVector), self.order };
}

@end

@implementation WKFoundDOMTextPosition

+ (WKFoundDOMTextPosition *)textPositionWithOffset:(NSUInteger)offset order:(NSUInteger)order
{
    RetainPtr pos = adoptNS([[WKFoundDOMTextPosition alloc] init]);
    [pos setOffset:offset];
    [pos setOrder:order];
    return pos.autorelease();
}

@end

@implementation WKFoundPDFTextRange

- (WKFoundPDFTextPosition *)start
{
    WKFoundPDFTextPosition *position = [WKFoundPDFTextPosition textPositionWithPage:self.startPage offset:self.startPageOffset];
    return position;
}

- (WKFoundPDFTextPosition *)end
{
    WKFoundPDFTextPosition *position = [WKFoundPDFTextPosition textPositionWithPage:self.endPage offset:self.endPageOffset];
    return position;
}

- (WebKit::WebFoundTextRange)webFoundTextRange
{
    WebKit::WebFoundTextRange::PDFData data { self.startPage, self.startPageOffset, self.endPage, self.endPageOffset };
    auto pathToFrameVector = makeVector(self.pathToFrame, [](id number) -> std::optional<uint64_t> {
        return [number unsignedLongValue];
    });
    return { data, WTF::move(pathToFrameVector), self.order };
}

@end

@implementation WKFoundPDFTextPosition

+ (WKFoundPDFTextPosition *)textPositionWithPage:(NSUInteger)page offset:(NSUInteger)offset
{
    RetainPtr pos = adoptNS([[WKFoundPDFTextPosition alloc] init]);
    [pos setPage:page];
    [pos setOffset:offset];
    return pos.autorelease();
}

@end

@implementation WKFoundCueTextRange

- (WKFoundCueTextPosition *)start
{
    return [WKFoundCueTextPosition textPositionWithDocumentOffset:self.documentOffset seekTimeMilliseconds:self.seekTimeMilliseconds order:self.order];
}

- (WKFoundCueTextPosition *)end
{
    return [WKFoundCueTextPosition textPositionWithDocumentOffset:self.documentOffset seekTimeMilliseconds:self.seekTimeMilliseconds order:self.order];
}

- (WebKit::WebFoundTextRange)webFoundTextRange
{
    WebKit::WebFoundTextRange::CueData data { WebCore::MediaPlayerClientIdentifier(self.mediaElementIdentifier), self.documentOffset, self.seekTimeMilliseconds };
    auto pathToFrameVector = makeVector(self.pathToFrame, [](id number) -> std::optional<uint64_t> {
        return [number unsignedLongValue];
    });
    return { data, WTF::move(pathToFrameVector), self.order };
}

@end

@implementation WKFoundCueTextPosition

+ (WKFoundCueTextPosition *)textPositionWithDocumentOffset:(uint64_t)documentOffset seekTimeMilliseconds:(uint64_t)seekTimeMilliseconds order:(NSUInteger)order
{
    RetainPtr pos = adoptNS([[WKFoundCueTextPosition alloc] init]);
    [pos setDocumentOffset:documentOffset];
    [pos setSeekTimeMilliseconds:seekTimeMilliseconds];
    [pos setOrder:order];
    return pos.autorelease();
}

@end

#endif // HAVE(UIFINDINTERACTION)
