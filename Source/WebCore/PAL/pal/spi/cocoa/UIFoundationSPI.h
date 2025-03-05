/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

DECLARE_SYSTEM_HEADER

#if USE(APPLE_INTERNAL_SDK)

#if ENABLE(ADAPTIVE_IMAGE_GLYPH)
#import <UIFoundation/NSAdaptiveImageGlyph_Private.h>
#import <UIFoundation/NSEmojiImageAsset.h>
#endif

#else

#if ENABLE(ADAPTIVE_IMAGE_GLYPH)

#if USE(APPKIT)
#import <AppKit/NSAdaptiveImageGlyph.h>
#else
#import <UIKit/NSAdaptiveImageGlyph.h>
#endif

@interface CTEmojiImageStrike : NSObject

- (instancetype)initWithImage:(CGImageRef)image alignmentInset:(CGSize)inset;

#if HAVE(NS_EMOJI_IMAGE_STRIKE_PROVENANCE)

- (instancetype)initWithImage:(CGImageRef)image alignmentInset:(CGSize)inset provenanceInfo:(NSDictionary*)provenanceInfo;

@property (atomic, readonly) CGImageRef cgImage;
@property (atomic, readonly) CGSize alignmentInset;
@property (atomic, readonly, copy) NSDictionary *provenance;

#endif

@end

@interface CTEmojiImageAsset : NSObject <NSCopying>

- (instancetype)initWithContentIdentifier:(NSString*)identifier shortDescription:(NSString*)description strikeImages:(NSArray <CTEmojiImageStrike*> *)images;

- (NSData *)imageData;

@end

@interface NSEmojiImageStrike : CTEmojiImageStrike
@end

@interface NSAdaptiveImageGlyph ()
@property (readonly) NSArray<NSEmojiImageStrike *> *strikes;
@end

@interface CTAdaptiveImageGlyph : NSObject
+ (void)flushInstanceCache;
@end

#endif // ENABLE(ADAPTIVE_IMAGE_GLYPH)

#endif // USE(APPLE_INTERNAL_SDK)
