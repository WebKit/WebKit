/*
 * Copyright (C) 2009   Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>
#import <WebKitLegacy/DOMUIKitExtensions.h>

#import <WebKitLegacy/WAKAppKitStubs.h>

typedef struct WebObjectInternal WebObjectInternal;

typedef enum {
    WebTextGranularityCharacter,
    WebTextGranularityWord,
    WebTextGranularitySentence,
    WebTextGranularityParagraph,
    WebTextGranularityLine,
    WebTextGranularityAll,
} WebTextGranularity;

@interface WebVisiblePosition : NSObject {
@private
    WebObjectInternal *_internal;
}

@property (nonatomic) NSSelectionAffinity affinity;

- (NSComparisonResult)compare:(WebVisiblePosition *)other;
- (int)distanceFromPosition:(WebVisiblePosition *)other;
- (WebVisiblePosition *)positionByMovingInDirection:(WebTextAdjustmentDirection)direction amount:(UInt32)amount;
- (WebVisiblePosition *)positionByMovingInDirection:(WebTextAdjustmentDirection)direction amount:(UInt32)amount withAffinityDownstream:(BOOL)affinityDownstream;

// Returnes YES only if a position is at a boundary of a text unit of the specified granularity in the particular direction.
- (BOOL)atBoundaryOfGranularity:(WebTextGranularity)granularity inDirection:(WebTextAdjustmentDirection)direction;

// Returns the next boundary position of a text unit of the given granularity in the given direction, or nil if there is no such position.
- (WebVisiblePosition *)positionOfNextBoundaryOfGranularity:(WebTextGranularity)granularity inDirection:(WebTextAdjustmentDirection)direction;

// Returns YES if position is within a text unit of the given granularity. If the position is at a boundary, returns YES only if
// if the boundary is part of the text unit in the given direction.
- (BOOL)withinTextUnitOfGranularity:(WebTextGranularity)granularity inDirectionIfAtBoundary:(WebTextAdjustmentDirection)direction;

// Returns range of the enclosing text unit of the given granularity, or nil if there is no such enclosing unit. Whether a boundary position
// is enclosed depends on the given direction, using the same rule as -[WebVisiblePosition withinTextUnitOfGranularity:inDirectionAtBoundary:].
- (DOMRange *)enclosingTextUnitOfGranularity:(WebTextGranularity)granularity inDirectionIfAtBoundary:(WebTextAdjustmentDirection)direction;

// Uses fine-tuned logic originally from WebCore::Frame::moveSelectionToStartOrEndOfCurrentWord
- (WebVisiblePosition *)positionAtStartOrEndOfWord;

- (BOOL)isEditable;
- (BOOL)requiresContextForWordBoundary;
- (BOOL)atAlphaNumericBoundaryInDirection:(WebTextAdjustmentDirection)direction;

- (DOMRange *)enclosingRangeWithDictationPhraseAlternatives:(NSArray **)alternatives;
- (DOMRange *)enclosingRangeWithCorrectionIndicator;

@end

@interface DOMRange (VisiblePositionExtensions)

- (WebVisiblePosition *)startPosition;
- (WebVisiblePosition *)endPosition;
+ (DOMRange *)rangeForFirstPosition:(WebVisiblePosition *)first second:(WebVisiblePosition *)second;

// Uses fine-tuned logic from SelectionController::wordSelectionContainingCaretSelection
- (DOMRange *)enclosingWordRange;

@end

@interface DOMNode (VisiblePositionExtensions)

- (WebVisiblePosition *)startPosition;
- (WebVisiblePosition *)endPosition;

@end
