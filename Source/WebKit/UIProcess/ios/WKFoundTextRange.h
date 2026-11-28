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

#pragma once

#if HAVE(UIFINDINTERACTION)

#import <UIKit/UIKit.h>

namespace WebKit {
struct WebFoundTextRange;
}

@interface WKFoundTextRange : UITextRange

@property (nonatomic) NSUInteger order;
@property (nonatomic, copy) NSArray *pathToFrame;

+ (WKFoundTextRange *)foundTextRangeWithWebFoundTextRange:(WebKit::WebFoundTextRange)range;

- (WebKit::WebFoundTextRange)webFoundTextRange;

@end

@interface WKFoundTextPosition : UITextPosition

@property (nonatomic) NSUInteger order;

@end

@interface WKFoundDOMTextRange : WKFoundTextRange

@property (nonatomic) NSUInteger location;
@property (nonatomic) NSUInteger length;

@end

@interface WKFoundDOMTextPosition : WKFoundTextPosition

@property (nonatomic) NSUInteger offset;

+ (WKFoundDOMTextPosition *)textPositionWithOffset:(NSUInteger)offset order:(NSUInteger)order;

@end

@interface WKFoundPDFTextRange : WKFoundTextRange

@property (nonatomic) NSUInteger startPage;
@property (nonatomic) NSUInteger startPageOffset;
@property (nonatomic) NSUInteger endPage;
@property (nonatomic) NSUInteger endPageOffset;

@end

@interface WKFoundCueTextRange : WKFoundTextRange

@property (nonatomic) uint64_t mediaElementIdentifier;
@property (nonatomic) uint64_t documentOffset;
@property (nonatomic) uint64_t seekTimeMilliseconds;

@end

@interface WKFoundPDFTextPosition : WKFoundTextPosition

@property (nonatomic) NSUInteger page;
@property (nonatomic) NSUInteger offset;

+ (WKFoundPDFTextPosition *)textPositionWithPage:(NSUInteger)page offset:(NSUInteger)offset;

@end

@interface WKFoundCueTextPosition : WKFoundTextPosition

@property (nonatomic) uint64_t documentOffset;
@property (nonatomic) uint64_t seekTimeMilliseconds;

+ (WKFoundCueTextPosition *)textPositionWithDocumentOffset:(uint64_t)documentOffset seekTimeMilliseconds:(uint64_t)seekTimeMilliseconds order:(NSUInteger)order;

@end

#endif // HAVE(UIFINDINTERACTION)
