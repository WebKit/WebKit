/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#if TARGET_OS_IPHONE

#import <CoreGraphics/CoreGraphics.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebVisiblePosition.h>

@class DOMRange;
@class DOMVisiblePosition;

typedef enum {
    WebTextSelectionStateNone,
    WebTextSelectionStateCaret,
    WebTextSelectionStateRange,
} WebTextSelectionState;

typedef enum {
    WebTextSmartExtendDirectionNone,
    WebTextSmartExtendDirectionLeft,
    WebTextSmartExtendDirectionRight,
} WebTextSmartExtendDirection;

@interface WebFrame (WebFrameIOS)

- (void)moveSelectionToPoint:(CGPoint)point;

- (void)clearSelection;
- (BOOL)hasSelection;
- (WebTextSelectionState)selectionState;
- (CGRect)caretRectForPosition:(WebVisiblePosition *)position;
- (CGRect)closestCaretRectInMarkedTextRangeForPoint:(CGPoint)point;
- (void)collapseSelection;
- (NSArray *)selectionRects;
- (NSArray *)selectionRectsForRange:(DOMRange *)domRange;
- (DOMRange *)wordAtPoint:(CGPoint)point;
- (WebVisiblePosition *)webVisiblePositionForPoint:(CGPoint)point;
- (void)setRangedSelectionBaseToCurrentSelection;
- (void)setRangedSelectionBaseToCurrentSelectionStart;
- (void)setRangedSelectionBaseToCurrentSelectionEnd;
- (void)clearRangedSelectionInitialExtent;
- (void)setRangedSelectionInitialExtentToCurrentSelectionStart;
- (void)setRangedSelectionInitialExtentToCurrentSelectionEnd;
- (BOOL)setRangedSelectionExtentPoint:(CGPoint)extentPoint baseIsStart:(BOOL)baseIsStart allowFlipping:(BOOL)allowFlipping;
- (BOOL)setSelectionWithBasePoint:(CGPoint)basePoint extentPoint:(CGPoint)extentPoint baseIsStart:(BOOL)baseIsStart;
- (BOOL)setSelectionWithBasePoint:(CGPoint)basePoint extentPoint:(CGPoint)extentPoint baseIsStart:(BOOL)baseIsStart allowFlipping:(BOOL)allowFlipping;
- (void)setSelectionWithFirstPoint:(CGPoint)firstPoint secondPoint:(CGPoint)secondPoint;
- (void)ensureRangedSelectionContainsInitialStartPoint:(CGPoint)initialStartPoint initialEndPoint:(CGPoint)initialEndPoint;

- (void)smartExtendRangedSelection:(WebTextSmartExtendDirection)direction;
- (void)aggressivelyExpandSelectionToWordContainingCaretSelection; // Doesn't accept no for an answer; expands past white space.

- (WKWritingDirection)selectionBaseWritingDirection;
- (void)toggleBaseWritingDirection;
- (void)setBaseWritingDirection:(WKWritingDirection)direction;

- (void)moveSelectionToStart;
- (void)moveSelectionToEnd;

- (void)setSelectionGranularity:(WebTextGranularity)granularity;
- (void)setRangedSelectionWithExtentPoint:(CGPoint)point;

- (WebVisiblePosition *)startPosition;
- (WebVisiblePosition *)endPosition;

- (BOOL)renderedCharactersExceed:(NSUInteger)threshold;

- (CGImageRef)imageForNode:(DOMNode *)node allowDownsampling:(BOOL)allowDownsampling drawContentBehindTransparentNodes:(BOOL)drawContentBehindTransparentNodes;

- (WebVisiblePosition *)previousUnperturbedDictationResultBoundaryFromPosition:(WebVisiblePosition *)position;
- (WebVisiblePosition *)nextUnperturbedDictationResultBoundaryFromPosition:(WebVisiblePosition *)position;

@end

#endif // TARGET_OS_IPHONE
