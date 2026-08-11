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

#include <wtf/Compiler.h>
#include <wtf/Platform.h>

DECLARE_SYSTEM_HEADER

#if USE(APPLE_INTERNAL_SDK)

#if ENABLE(MULTI_REPRESENTATION_HEIC)
#import <UIFoundation/NSAdaptiveImageGlyph_Private.h>
#import <UIFoundation/NSEmojiImageAsset.h>
#endif

#else

#if ENABLE(MULTI_REPRESENTATION_HEIC)

#if USE(APPKIT)
#import <AppKit/NSAdaptiveImageGlyph.h>
#else
#import <UIKit/NSAdaptiveImageGlyph.h>
#endif

@interface NSEmojiImageStrike : CTEmojiImageStrike
@end

@interface NSAdaptiveImageGlyph ()
@property (readonly) NSArray<NSEmojiImageStrike *> *strikes;
@end

#endif // ENABLE(MULTI_REPRESENTATION_HEIC)

#endif // USE(APPLE_INTERNAL_SDK)

#if PLATFORM(MAC) && !__has_include(<UIFoundation/NSTextTable.h>)

// Map new names to deprecated names so that we can build against older SDKs,
// while referencing only the new enum value names.
#define NSTextBlockValueTypeAbsolute                    NSTextBlockAbsoluteValueType
#define NSTextBlockValueTypePercentage                  NSTextBlockPercentageValueType
#define NSTextBlockDimensionWidth                       NSTextBlockWidth
#define NSTextBlockDimensionMinimumWidth                NSTextBlockMinimumWidth
#define NSTextBlockDimensionMaximumWidth                NSTextBlockMaximumWidth
#define NSTextBlockDimensionHeight                      NSTextBlockHeight
#define NSTextBlockDimensionMinimumHeight               NSTextBlockMinimumHeight
#define NSTextBlockDimensionMaximumHeight               NSTextBlockMaximumHeight
#define NSTextBlockLayerPadding                         NSTextBlockPadding
#define NSTextBlockLayerBorder                          NSTextBlockBorder
#define NSTextBlockLayerMargin                          NSTextBlockMargin
#define NSTextBlockVerticalAlignmentTop                 NSTextBlockTopAlignment
#define NSTextBlockVerticalAlignmentMiddle              NSTextBlockMiddleAlignment
#define NSTextBlockVerticalAlignmentBottom              NSTextBlockBottomAlignment
#define NSTextBlockVerticalAlignmentBaseline            NSTextBlockBaselineAlignment
#define NSTextTableLayoutAlgorithmAutomatic             NSTextTableAutomaticLayoutAlgorithm
#define NSTextTableLayoutAlgorithmFixed                 NSTextTableFixedLayoutAlgorithm

#endif // PLATFORM(MAC) && !__has_include(<UIFoundation/NSTextTable.h>)
