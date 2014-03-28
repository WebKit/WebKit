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

#if PLATFORM(IOS)

#import "WebFrameIOS.h"

#import <WebCore/Editor.h>
#import <WebCore/Element.h>
#import <WebCore/DocumentMarkerController.h>
#import <WebCore/EventHandler.h>
#import <WebCore/FloatRect.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameSelection.h>
#import <WebCore/FrameSnapshotting.h>
#import <WebCore/FrameView.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/htmlediting.h>
#import <WebCore/Position.h>
#import <WebCore/Range.h>
#import <WebCore/RenderObject.h>
#import <WebCore/RenderText.h>
#import <WebCore/SelectionRect.h>
#import <WebCore/TextBoundaries.h>
#import <WebCore/TextDirection.h>
#import <WebCore/VisiblePosition.h>
#import <WebCore/VisibleUnits.h>
#import <WebKit/DOM.h>
#import <WebKit/DOMRange.h>
#import <WebKit/DOMUIKitExtensions.h>
#import <WebKit/WebSelectionRect.h>
#import <WebKit/WebVisiblePosition.h>
#import <unicode/uchar.h>

#import "DOMNodeInternal.h"
#import "DOMRangeInternal.h"
#import "WebFrameInternal.h"
#import "WebUIKitDelegate.h"
#import "WebViewPrivate.h"
#import "WebVisiblePosition.h"
#import "WebVisiblePositionInternal.h"

using namespace WebCore;

//-------------------

@interface WebFrame (WebSecretsIKnowAbout)
- (VisiblePosition)_visiblePositionForPoint:(CGPoint)point;
@end

@implementation WebFrame (WebFrameIOS)

//-------------------

- (WebCore::Frame *)coreFrame
{
    return _private->coreFrame;
}

- (VisiblePosition)visiblePositionForPoint:(CGPoint)point
{
    return [self _visiblePositionForPoint:point];
}

- (VisiblePosition)closestWordBoundary:(VisiblePosition)position
{
    VisiblePosition start = startOfWord(position);
    VisiblePosition end = endOfWord(position);
    int startDistance = position.deepEquivalent().deprecatedEditingOffset() - start.deepEquivalent().deprecatedEditingOffset();
    int endDistance = end.deepEquivalent().deprecatedEditingOffset() - position.deepEquivalent().deprecatedEditingOffset() ;
    return (startDistance < endDistance) ? start : end;    
}

//-------------------

- (void)clearSelection
{
    Frame *frame = [self coreFrame];
    if (frame)
        frame->selection().clearCurrentSelection();
    
}

- (WebTextSelectionState)selectionState
{
    WebTextSelectionState state = WebTextSelectionStateNone;

    Frame *frame = [self coreFrame];
    FrameSelection& frameSelection = frame->selection();

    if (frameSelection.isCaret())
        state = WebTextSelectionStateCaret;
    else if (frameSelection.isRange())
        state = WebTextSelectionStateRange;

    return state;
}

- (BOOL)hasSelection
{
    WebTextSelectionState state = [self selectionState];
    return state == WebTextSelectionStateCaret || state == WebTextSelectionStateRange;
}

- (CGRect)caretRectForPosition:(WebVisiblePosition *)position
{
    return position ? [position _visiblePosition].absoluteCaretBounds() : CGRectZero;
}

- (CGRect)closestCaretRectInMarkedTextRangeForPoint:(CGPoint)point
{
    Frame *frame = [self coreFrame];
    Range *markedTextRange = frame->editor().compositionRange().get();
    VisibleSelection markedTextRangeSelection = VisibleSelection(markedTextRange);

    IntRect result;

    if (markedTextRangeSelection.isRange()) {
        VisiblePosition start(markedTextRangeSelection.start());
        VisiblePosition end(markedTextRangeSelection.end());

        // Adjust pos and give it an appropriate affinity.
        VisiblePosition pos;
        Vector<IntRect> intRects;
        markedTextRange->textRects(intRects, NO);
        unsigned size = intRects.size();
        CGRect firstRect = intRects[0];
        CGRect lastRect  = intRects[size-1];
        if (point.y < firstRect.origin.y) {
            point.y = firstRect.origin.y;
            pos = [self visiblePositionForPoint:point];
            pos.setAffinity(UPSTREAM);
        }
        else if (point.y >= lastRect.origin.y) {
            point.y = lastRect.origin.y;
            pos = [self visiblePositionForPoint:point];
            pos.setAffinity(DOWNSTREAM);
        }
        else {
            pos = [self visiblePositionForPoint:point];
        }
        
        if (pos == start || pos < start) {
            start.setAffinity(UPSTREAM);
            //result = start.next().absoluteCaretBounds();
            result = start.absoluteCaretBounds(); //<rdar://problem/6716185>: Allow placing the caaret to the left of the first character.
        } else if (pos > end) {
            end.setAffinity(DOWNSTREAM);
            result = end.absoluteCaretBounds();
        } else {
            result = pos.absoluteCaretBounds();
        }
    } else {
        VisiblePosition pos = [self visiblePositionForPoint:point];
        result = pos.absoluteCaretBounds();
    }
    
    return (CGRect) result;    
}


- (void)collapseSelection
{
    if ([self selectionState] == WebTextSelectionStateRange) {
        Frame *frame = [self coreFrame];
        FrameSelection& frameSelection = frame->selection();
        VisiblePosition end(frameSelection.selection().end());
        frameSelection.moveTo(end);
    }
}

- (void)extendSelection:(BOOL)start
{
    if ([self selectionState] == WebTextSelectionStateRange) {
        Frame *frame = [self coreFrame];
        const VisibleSelection& originalSelection = frame->selection().selection();
        if (start) {
            VisiblePosition start = startOfWord(originalSelection.start());
            frame->selection().moveTo(start, originalSelection.end());
        } else {
            VisiblePosition end = endOfWord(originalSelection.end());
            frame->selection().moveTo(originalSelection.start(), end);
        }
    }    
}

- (NSArray *)selectionRectsForCoreRange:(Range *)range
{
    if (!range)
        return nil;
    
    Vector<SelectionRect> rects;
    range->collectSelectionRects(rects);
    unsigned size = rects.size();
    
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:size];
    for (unsigned i = 0; i < size; i++) {
        SelectionRect &coreRect = rects[i];
        WebSelectionRect *webRect = [WebSelectionRect selectionRect];
        webRect.rect = static_cast<CGRect>(coreRect.rect());
        webRect.writingDirection = coreRect.direction() == LTR ? WKWritingDirectionLeftToRight : WKWritingDirectionRightToLeft;
        webRect.isLineBreak = coreRect.isLineBreak();
        webRect.isFirstOnLine = coreRect.isFirstOnLine();
        webRect.isLastOnLine = coreRect.isLastOnLine();
        webRect.containsStart = coreRect.containsStart();
        webRect.containsEnd = coreRect.containsEnd();
        webRect.isInFixedPosition = coreRect.isInFixedPosition();
        webRect.isHorizontal = coreRect.isHorizontal();
        [result addObject:webRect];
    }
    
    return result;        
}

- (NSArray *)selectionRectsForRange:(DOMRange *)domRange
{
    return [self selectionRectsForCoreRange:core(domRange)];
}

- (NSArray *)selectionRects
{
    if (![self hasSelection])
        return nil;

    Frame *frame = [self coreFrame];
    return [self selectionRectsForCoreRange:frame->selection().toNormalizedRange().get()];
}

- (DOMRange *)wordAtPoint:(CGPoint)point
{
    VisiblePosition pos = [self visiblePositionForPoint:point];
    VisiblePosition start = startOfWord(pos);
    VisiblePosition end = endOfWord(pos);
    DOMRange *wordRange = kit(makeRange(start, end).get());
    return wordRange;   
}

- (WebVisiblePosition *)webVisiblePositionForPoint:(CGPoint)point
{
    return [WebVisiblePosition _wrapVisiblePosition:[self visiblePositionForPoint:point]];
}

- (void)setRangedSelectionBaseToCurrentSelection
{
    Frame *frame = [self coreFrame];
    frame->setRangedSelectionBaseToCurrentSelection();
}

- (void)setRangedSelectionBaseToCurrentSelectionStart
{
    Frame *frame = [self coreFrame];
    frame->setRangedSelectionBaseToCurrentSelectionStart();
}

- (void)setRangedSelectionBaseToCurrentSelectionEnd
{
    Frame *frame = [self coreFrame];
    frame->setRangedSelectionBaseToCurrentSelectionEnd();
}

- (void)clearRangedSelectionInitialExtent
{
    Frame *frame = [self coreFrame];
    frame->clearRangedSelectionInitialExtent();
}

- (void)setRangedSelectionInitialExtentToCurrentSelectionStart
{
    Frame *frame = [self coreFrame];
    frame->setRangedSelectionInitialExtentToCurrentSelectionStart();
}

- (void)setRangedSelectionInitialExtentToCurrentSelectionEnd
{
    Frame *frame = [self coreFrame];
    frame->setRangedSelectionInitialExtentToCurrentSelectionEnd();
}

- (void)setRangedSelectionWithExtentPoint:(CGPoint)point
{    
    Frame *frame = [self coreFrame];
    FrameSelection& frameSelection = frame->selection();
    VisiblePosition pos = [self visiblePositionForPoint:point];
    VisibleSelection base = frame->rangedSelectionBase();
    
    if (pos.isNull() || !base.isRange())
        return;
    
    VisiblePosition start(base.start());
    VisiblePosition end(base.end());    
    
    if (pos < start) {        
        frameSelection.moveTo(pos, end);
    } 
    else if (pos > end) {
        frameSelection.moveTo(start, pos);
    } 
    else {
        frameSelection.moveTo(start, end);
    }
}

- (BOOL)setRangedSelectionExtentPoint:(CGPoint)extentPoint baseIsStart:(BOOL)baseIsStart allowFlipping:(BOOL)allowFlipping
{
    Frame *frame = [self coreFrame];
    VisibleSelection rangedSelectionBase(frame->rangedSelectionBase());
    VisiblePosition baseStart(rangedSelectionBase.start(), rangedSelectionBase.affinity());
    VisiblePosition baseEnd;
    if (rangedSelectionBase.isNone()) {
        return baseIsStart;
    }
    else if (rangedSelectionBase.isCaret()) {
        baseEnd = baseStart;
    }
    else {
        baseEnd = VisiblePosition(rangedSelectionBase.end(), rangedSelectionBase.affinity());
    }

    VisiblePosition extent([self visiblePositionForPoint:extentPoint]);
    
    if (rangedSelectionBase.isRange() && baseStart < extent && extent < baseEnd) {
        frame->selection().moveTo(baseStart, baseEnd);
        return NO;
    }    
    
    CGRect caretRect = baseIsStart ? baseStart.absoluteCaretBounds() : baseEnd.absoluteCaretBounds();
    CGPoint basePoint = CGPointMake(caretRect.origin.x, caretRect.origin.y);

    static const CGFloat FlipMargin = 30;
    bool didFlipStartEnd = false;
    bool canFlipStartEnd = allowFlipping && 
        (fabs(basePoint.x - extentPoint.x) > FlipMargin || fabs(basePoint.y - extentPoint.y) > FlipMargin);

    VisiblePosition base;
    if (baseIsStart) {                
        base = baseStart;
        BOOL wouldFlip = (extent < base);
        if (wouldFlip) {
            if (!canFlipStartEnd) {
                // We're going to prevent flipping.  First try for a position on the same line.
                // If that fails, just choose something after the start.
                CGRect baseCaret = base.absoluteCaretBounds();
                bool baseIsHorizontal = baseCaret.size.height > baseCaret.size.width;
                CGPoint pointInLine = baseIsHorizontal ? CGPointMake(extentPoint.x, CGRectGetMidY(baseCaret)) :
                                                         CGPointMake(CGRectGetMidX(baseCaret), extentPoint.y);
                VisiblePosition positionInLine = [self visiblePositionForPoint:pointInLine];
                if (positionInLine.isNotNull() && positionInLine > base) {
                    extent = positionInLine;
                } else {
                    extent = base.next();
                }
            } else {
                didFlipStartEnd = YES;
            }
        }
        
        if (base == extent)
            extent = base.next();
    }
    else {
        base = baseEnd;
        BOOL wouldFlip = (extent > base);
        if (wouldFlip) {
            if (!canFlipStartEnd) {
                // We're going to prevent flipping.  First try for a position on the same line.
                // If that fails, just choose something before the end.
                CGRect baseCaret = base.absoluteCaretBounds();
                bool baseIsHorizontal = baseCaret.size.height > baseCaret.size.width;
                CGPoint pointInLine = baseIsHorizontal ? CGPointMake(extentPoint.x, CGRectGetMidY(baseCaret)) :
                                                         CGPointMake(CGRectGetMidX(baseCaret), extentPoint.y);
                VisiblePosition positionInLine = [self visiblePositionForPoint:pointInLine];
                if (positionInLine.isNotNull() && positionInLine < base) {
                    extent = positionInLine;
                } else {
                    extent = base.previous();
                }
            } else {
                didFlipStartEnd = YES;
            }
        }
        
        if (base == extent)
            extent = base.previous();
    }
    
    frame->selection().moveTo(base, extent);

    return didFlipStartEnd ? !baseIsStart : baseIsStart;
}

- (BOOL)setSelectionWithBasePoint:(CGPoint)basePoint extentPoint:(CGPoint)extentPoint baseIsStart:(BOOL)baseIsStart allowFlipping:(BOOL)allowFlipping
{
    // This function updates the selection using two points and an existing notion of
    // which is the base and which is the extent. However, if the allowFlipping argument
    // is YES, it will allow the base and extent positions to flip if the extent moves a
    // certain amount to the "other side" of the base. When a flip of start/end occurs
    // relative to base/extent,  this is reported back to the calling code, which is then
    // expected to take the flip into account in subsequent calls to this function (for at
    // least as long as a single, logical selection session continues).

    Frame *frame = [self coreFrame];
    FrameSelection& frameSelection = frame->selection();
    VisiblePosition base([self visiblePositionForPoint:basePoint]);
    VisiblePosition extent([self visiblePositionForPoint:extentPoint]);

    static const CGFloat FlipMargin = 30;
    bool didFlipStartEnd = false;
    bool canFlipStartEnd = allowFlipping &&
                        ((baseIsStart &&  (extentPoint.x <= basePoint.x - FlipMargin || extentPoint.y <= basePoint.y - FlipMargin)) ||
                         (!baseIsStart && (extentPoint.x >= basePoint.x + FlipMargin || extentPoint.y >= basePoint.y + FlipMargin)));
    
                        
    if (extent == base) {
        extent = baseIsStart ? base.next() : base.previous();
    }
    else if (baseIsStart && extent < base) {
        if (canFlipStartEnd)
            didFlipStartEnd = true;
        else
            extent = base.next();
    }
    else if (!baseIsStart && extent > base) {
        if (canFlipStartEnd)
            didFlipStartEnd = true;
        else
            extent = base.previous();
    }

    frameSelection.moveTo(base, extent);

    return didFlipStartEnd ? !baseIsStart : baseIsStart;
}

- (BOOL)setSelectionWithBasePoint:(CGPoint)basePoint extentPoint:(CGPoint)extentPoint baseIsStart:(BOOL)baseIsStart
{
    return [self setSelectionWithBasePoint:basePoint extentPoint:extentPoint baseIsStart:baseIsStart allowFlipping:YES];
}

- (void)setSelectionWithFirstPoint:(CGPoint)firstPoint secondPoint:(CGPoint)secondPoint
{
    // We still support two-finger taps to make a selection, and these taps
    // don't care about base/extent.
    VisiblePosition first([self visiblePositionForPoint:firstPoint]);
    VisiblePosition second([self visiblePositionForPoint:secondPoint]);
    Frame *frame = [self coreFrame];
    FrameSelection& frameSelection = frame->selection();
    frameSelection.moveTo(first, second);
}

- (void)ensureRangedSelectionContainsInitialStartPoint:(CGPoint)initialStartPoint initialEndPoint:(CGPoint)initialEndPoint
{
    // This method ensures that selection ends doesn't contract such that it no
    // longer contains these points. This is the desirable behavior when the
    // user does the tap-and-a-half + drag operation.
    Frame *frame = [self coreFrame];
    const VisibleSelection& originalSelection = frame->selection().selection();
    Position ensureStart([self visiblePositionForPoint:initialStartPoint].deepEquivalent());
    Position ensureEnd([self visiblePositionForPoint:initialEndPoint].deepEquivalent());
    if (originalSelection.start() > ensureStart)
        frame->selection().moveTo(ensureStart, originalSelection.end());
    else if (originalSelection.end() < ensureEnd)
        frame->selection().moveTo(originalSelection.start(), ensureEnd);
}

- (void)aggressivelyExpandSelectionToWordContainingCaretSelection
{
    Frame *frame = [self coreFrame];
    FrameSelection& frameSelection = frame->selection();
    VisiblePosition end = frameSelection.selection().visibleEnd();
    if (end == endOfDocument(end) && end != startOfDocument(end) && end == startOfLine(end))
        frameSelection.moveTo(end.previous(), end);

    [self expandSelectionToWordContainingCaretSelection];

    // This is a temporary hack until we get the improvements
    // I'm working on for RTL selection.
    if (frameSelection.granularity() == WordGranularity)
        frameSelection.moveTo(frameSelection.selection().start(), frameSelection.selection().end());
    
    if (frameSelection.selection().isCaret()) {
        VisiblePosition pos(frameSelection.selection().end());
        if (isStartOfLine(pos) && isEndOfLine(pos)) {
            VisiblePosition next(pos.next());
            if (next.isNotNull())
                frameSelection.moveTo(end, next);
        }
        else {
            while (pos.isNotNull()) {
                VisiblePosition wordStart(startOfWord(pos));
                if (wordStart != pos) {
                    frameSelection.moveTo(wordStart, frameSelection.selection().end());
                    break;
                }
                pos = pos.previous();
            }
        }
    }
}

- (void)expandSelectionToSentence
{
    Frame *frame = [self coreFrame];
    FrameSelection& frameSelection = frame->selection();
    VisiblePosition pos = frameSelection.selection().start();
    VisiblePosition start = startOfSentence(pos);
    VisiblePosition end = endOfSentence(pos);
    frameSelection.moveTo(start, end);
}

- (WKWritingDirection)selectionBaseWritingDirection
{
    Frame *frame = [self coreFrame];
    switch (frame->editor().baseWritingDirectionForSelectionStart()) {
    case LeftToRightWritingDirection:
        return WKWritingDirectionLeftToRight;

    case RightToLeftWritingDirection:
        return WKWritingDirectionRightToLeft;

    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return WKWritingDirectionLeftToRight;
}

- (void)toggleBaseWritingDirection
{
    WKWritingDirection updated = WKWritingDirectionRightToLeft;
    switch ([self selectionBaseWritingDirection]) {
        case WKWritingDirectionLeftToRight:
            updated = WKWritingDirectionRightToLeft;
            break;
        case WKWritingDirectionRightToLeft:
            updated = WKWritingDirectionLeftToRight;
            break;
        default:
            // WebCore should never return anything else, including WKWritingDirectionNatural
            ASSERT_NOT_REACHED();
            break;
    }
    [self setBaseWritingDirection:updated];
}

- (void)setBaseWritingDirection:(WKWritingDirection)direction
{
    WKWritingDirection originalDirection = [self selectionBaseWritingDirection];

    Frame *frame = [self coreFrame];
    if (!frame->selection().selection().isContentEditable())
        return;
    
    WritingDirection wcDirection = LeftToRightWritingDirection;
    switch (direction) {
        case WKWritingDirectionNatural:
            wcDirection = NaturalWritingDirection;
            break;
        case WKWritingDirectionLeftToRight:
            wcDirection = LeftToRightWritingDirection;
            break;
        case WKWritingDirectionRightToLeft:
            wcDirection = RightToLeftWritingDirection;
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
    }
    frame->editor().setBaseWritingDirection(wcDirection);
    
    if (originalDirection != [self selectionBaseWritingDirection])
        frame->editor().setTextAlignmentForChangedBaseWritingDirection(wcDirection);
}

- (void)moveSelectionToStart
{
    Frame *frame = [self coreFrame];
    FrameSelection& frameSelection = frame->selection();
    VisiblePosition start = startOfDocument(frameSelection.selection().start());
    frameSelection.moveTo(start);
}

- (void)moveSelectionToEnd
{
    Frame *frame = [self coreFrame];
    FrameSelection& frameSelection = frame->selection();
    VisiblePosition end =  endOfDocument(frameSelection.selection().end());
    frameSelection.moveTo(end);
}

- (void)moveSelectionToPoint:(CGPoint)point
{
    Frame *frame = [self coreFrame];
    FrameSelection& frameSelection = frame->selection();
    VisiblePosition pos = [self _visiblePositionForPoint:point];
    frameSelection.moveTo(pos);
}

- (void)setSelectionGranularity:(WebTextGranularity)granularity
{
    TextGranularity wcGranularity = CharacterGranularity;
    switch (granularity) {
        case WebTextGranularityCharacter:
            wcGranularity = CharacterGranularity;
            break;
        case WebTextGranularityWord:
            wcGranularity = WordGranularity;
            break;
        case WebTextGranularitySentence:
            wcGranularity = SentenceGranularity;
            break;
        case WebTextGranularityParagraph:
            wcGranularity = ParagraphGranularity;
            break;
        case WebTextGranularityAll:
            // FIXME: Add DocumentGranularity.
            wcGranularity = ParagraphGranularity;
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
    }
    FrameSelection& frameSelection = _private->coreFrame->selection();
    frameSelection.setSelection(frameSelection.selection(), wcGranularity);
}

static inline bool isAlphaNumericCharacter(UChar32 c)
{
    static CFCharacterSetRef set = CFCharacterSetGetPredefined(kCFCharacterSetAlphaNumeric);
    return CFCharacterSetIsCharacterMember(set, c);
}

static VisiblePosition SimpleSmartExtendStart(const VisiblePosition& start, const VisiblePosition& end, const VisibleSelection& initialExtent)
{
    VisiblePosition pos(start);
    VisiblePosition initialStart;
    if (initialExtent.isCaretOrRange())
        initialStart = VisiblePosition(initialExtent.start(), initialExtent.affinity());

    if (initialStart == start) {
        // No smarts needed. Leave selection where it is.
        return pos;
    }

    UChar32 charBefore = pos.characterBefore();
    UChar32 charAfter = pos.characterAfter();
    if (isAlphaNumericCharacter(charAfter) && !isAlphaNumericCharacter(charBefore)) {
        // This is a word start. Leave selection where it is.
        return pos;
    }
    
    if (isAlphaNumericCharacter(charBefore) && !isAlphaNumericCharacter(charAfter)) {
        // This is a word end. Nudge the selection to the next character before proceeding.
        pos = pos.next();
    }

    // Extend to the start of the word.
    // If this isn't where the start was initially, use this position.
    VisiblePosition wordStart(startOfWord(pos));
    if (wordStart != initialStart) {
        return wordStart;
    }
    // Conversely, if the initial start equals the current word start, then
    // run the rest of this function to see if the selection should extend
    // back to the next word.

    // Passed-in end must be at least three characters from initialStart or
    // must cross word boundary.
    // If this is where the start was initially, skip to the end of the word,
    // then iterate forward in the document until we hit an alphanumeric.
    VisiblePosition wordEnd(endOfWord(pos));
    pos = wordEnd;
    while (pos.isNotNull() && !isStartOfLine(pos) && !isEndOfLine(pos) && pos != end) {
        UChar32 c = pos.characterAfter();
        if (isAlphaNumericCharacter(c))
            break;
        pos = pos.next();
    }
    
    // Don't let the smart extension make the start equal the end.
    // Expand out to word boundary.
    if (pos == end)
        pos = wordStart;
    return pos;
}

static VisiblePosition SimpleSmartExtendEnd(const VisiblePosition& start, const VisiblePosition& end, const VisibleSelection& initialExtent)
{
    VisiblePosition pos(end);

    VisiblePosition initialEnd;
    if (initialExtent.isCaretOrRange())
        initialEnd = VisiblePosition(initialExtent.end(), initialExtent.affinity());
    
    if (initialEnd == end) {
        // No smarts needed. Leave selection where it is.
        return pos;
    }
    
    UChar32 charBefore = pos.characterBefore();
    UChar32 charAfter = pos.characterAfter();
    if (isAlphaNumericCharacter(charBefore) && !isAlphaNumericCharacter(charAfter)) {
        // This is a word end. Leave selection where it is.
        return pos;
    }

    if (!isAlphaNumericCharacter(charBefore) && isAlphaNumericCharacter(charAfter)) {
        // This is a word start. Nudge the selection to the previous character before proceeding.
        pos = pos.previous();
    }

    // Extend to the end of the word.
    // If this isn't where the end was initially, use this position.
    VisiblePosition wordEnd(endOfWord(pos));
    if (wordEnd != initialEnd && isAlphaNumericCharacter(wordEnd.characterBefore())) {
        return wordEnd;
    }
    // Conversely, if the initial end equals the current word end, then
    // run the rest of this function to see if the selection should extend
    // back to the previous word.

    // If this is where the end was initially, skip to the start of the word,
    // then iterate backward in the document until we hit an alphanumeric.
    VisiblePosition wordStart(startOfWord(pos));
    pos = wordStart;
    while (pos.isNotNull() && !isStartOfLine(pos) && !isEndOfLine(pos) && pos != start) {
        UChar32 c = pos.characterBefore();
        if (isAlphaNumericCharacter(c))
            break;
        pos = pos.previous();
    }

    // Don't let the smart extension make the end equal the start.
    // Expand out to word boundary.
    if (pos == start)
        pos = wordEnd;
    
    return pos;
}

- (void)smartExtendRangedSelection:(WebTextSmartExtendDirection)direction
{
    if ([self selectionState] != WebTextSelectionStateRange)
        return;
    
    Frame *frame = [self coreFrame];
    FrameSelection& frameSelection = frame->selection();
    EAffinity affinity = frameSelection.selection().affinity();
    VisiblePosition start(frameSelection.selection().start(), affinity);
    VisiblePosition end(frameSelection.selection().end(), affinity);
    VisiblePosition base(frame->rangedSelectionBase().base());  // should equal start or end

    // Base must equal start or end
    if (base != start && base != end)
        return;

    VisiblePosition extent(frameSelection.selection().extent(), affinity);
    
    // We don't yet support smart extension for languages which
    // require context for word boundary.
    if (requiresContextForWordBoundary(extent.characterAfter()) || 
        requiresContextForWordBoundary(extent.characterBefore()))
        return;

    // If the smart-extend direction is neither left nor right, do
    // not pass rangedSelectionInitialExtent to the smart extend functions.
    // This will have the effect of always extending out to include the
    // word which contains the extent.
    VisibleSelection initialExtent;
    if (direction != WebTextSmartExtendDirectionNone)
        initialExtent = frame->rangedSelectionInitialExtent();

    VisiblePosition smartExtent;
    if (base == end) {  // extend start
        smartExtent = SimpleSmartExtendStart(start, end, initialExtent);
    }
    else {  // base == start / extend end
        smartExtent = SimpleSmartExtendEnd(start, end, initialExtent);
    }

    if (smartExtent.isNotNull() && smartExtent != extent)
        frameSelection.moveTo(base, smartExtent);

}

- (WebVisiblePosition *)startPosition
{
    Frame *frame = [self coreFrame];
    Element *rootElement = frame->document()->documentElement();
    return [WebVisiblePosition _wrapVisiblePosition:startOfDocument(static_cast<Node*>(rootElement))];
}

- (WebVisiblePosition *)endPosition
{
    Frame *frame = [self coreFrame];
    Element *rootElement = frame->document()->documentElement();
    return [WebVisiblePosition _wrapVisiblePosition:endOfDocument(static_cast<Node*>(rootElement))];
}

- (BOOL)renderedCharactersExceed:(NSUInteger)threshold
{
    Frame *frame = [self coreFrame];
    return frame->view()->renderedCharactersExceed(threshold);
}

- (CGImageRef)imageForNode:(DOMNode *)node allowDownsampling:(BOOL)allowDownsampling drawContentBehindTransparentNodes:(BOOL)drawContentBehindTransparentNodes
{
    // FIXME: implement: <rdar://problem/15808709>
    return nullptr;
}

// Iterates backward through the document and returns the point at which untouched dictation results end.
- (WebVisiblePosition *)previousUnperturbedDictationResultBoundaryFromPosition:(WebVisiblePosition *)position
{
    VisiblePosition currentVisiblePosition = [position _visiblePosition];
    if (currentVisiblePosition.isNull())
        return position;
    
    Document& document = currentVisiblePosition.deepEquivalent().anchorNode()->document();

    id uikitDelegate = [[self webView] _UIKitDelegate];
    if (![uikitDelegate respondsToSelector:@selector(isUnperturbedDictationResultMarker:)])
        return position;
    
    while (currentVisiblePosition.isNotNull()) {
        WebVisiblePosition *currentWebVisiblePosition = [WebVisiblePosition _wrapVisiblePosition:currentVisiblePosition];
        
        Node *currentNode = currentVisiblePosition.deepEquivalent().anchorNode();
        int lastOffset = lastOffsetForEditing(currentNode);
        ASSERT(lastOffset >= 0);
        if (lastOffset < 0)
            return currentWebVisiblePosition;
        
        VisiblePosition previousVisiblePosition = currentVisiblePosition.previous();
        if (previousVisiblePosition.isNull())
            return currentWebVisiblePosition;
        
        RefPtr<Range> graphemeRange = Range::create(document, previousVisiblePosition.deepEquivalent(), currentVisiblePosition.deepEquivalent());
        
        Vector<DocumentMarker*> markers = document.markers().markersInRange(graphemeRange.get(), DocumentMarker::DictationResult);
        if (markers.isEmpty())
            return currentWebVisiblePosition;
        
        // FIXME: Result markers should not overlap, so there should only ever be one for a single grapheme.
        // <rdar://problem/9810617> Too much document context is omitted when sending dictation hints because of problems with WebCore DocumentMarkers
        // ASSERT(markers.size() == 1);
        if (markers.size() > 1)
            return currentWebVisiblePosition;
        DocumentMarker* resultMarker = markers.at(0);
        
        // FIXME: WebCore doesn't always update markers correctly during editing. Bail if resultMarker extends off the edge of 
        // this node, because that means it's invalid.
        if (resultMarker->endOffset() > (unsigned)lastOffset)
            return currentWebVisiblePosition;
        
        if (![uikitDelegate isUnperturbedDictationResultMarker:resultMarker->metadata()])
            return currentWebVisiblePosition;
        
        if (resultMarker->startOffset() > 0)
            return [WebVisiblePosition _wrapVisiblePosition:VisiblePosition(createLegacyEditingPosition(currentNode, resultMarker->startOffset()))];
        
        currentVisiblePosition = VisiblePosition(createLegacyEditingPosition(currentNode, 0));
    }
    
    return position;
}

// Iterates forward through the document and returns the point at which untouched dictation results end.
- (WebVisiblePosition *)nextUnperturbedDictationResultBoundaryFromPosition:(WebVisiblePosition *)position
{
    VisiblePosition currentVisiblePosition = [position _visiblePosition];
    if (currentVisiblePosition.isNull())
        return position;
    
    Document& document = currentVisiblePosition.deepEquivalent().anchorNode()->document();
    
    id uikitDelegate = [[self webView] _UIKitDelegate];
    if (![uikitDelegate respondsToSelector:@selector(isUnperturbedDictationResultMarker:)])
        return position;
    
    while (currentVisiblePosition.isNotNull()) {
        WebVisiblePosition *currentWebVisiblePosition = [WebVisiblePosition _wrapVisiblePosition:currentVisiblePosition];
        
        Node *currentNode = currentVisiblePosition.deepEquivalent().anchorNode();
        int lastOffset = lastOffsetForEditing(currentNode);
        ASSERT(lastOffset >= 0);
        if (lastOffset < 0)
            return currentWebVisiblePosition;
        
        VisiblePosition nextVisiblePosition = currentVisiblePosition.next();
        if (nextVisiblePosition.isNull())
            return currentWebVisiblePosition;
        
        RefPtr<Range> graphemeRange = Range::create(document, currentVisiblePosition.deepEquivalent(), nextVisiblePosition.deepEquivalent());
        
        Vector<DocumentMarker*> markers = document.markers().markersInRange(graphemeRange.get(), DocumentMarker::DictationResult);
        if (markers.isEmpty())
            return currentWebVisiblePosition;
        
        // FIXME: Result markers should not overlap, so there should only ever be one for a single grapheme.
        // <rdar://problem/9810617> Too much document context is omitted when sending dictation hints because of problems with WebCore DocumentMarkers
        //ASSERT(markers.size() == 1);
        if (markers.size() > 1)
            return currentWebVisiblePosition;
        DocumentMarker* resultMarker = markers.at(0);
        
        // FIXME: WebCore doesn't always update markers correctly during editing. Bail if resultMarker extends off the edge of 
        // this node, because that means it's invalid.
        if (resultMarker->endOffset() > static_cast<unsigned>(lastOffset))
            return currentWebVisiblePosition;
        
        if (![uikitDelegate isUnperturbedDictationResultMarker:resultMarker->metadata()])
            return currentWebVisiblePosition;
        
        if (resultMarker->endOffset() <= static_cast<unsigned>(lastOffset))
            return [WebVisiblePosition _wrapVisiblePosition:VisiblePosition(createLegacyEditingPosition(currentNode, resultMarker->endOffset()))];
        
        currentVisiblePosition = VisiblePosition(createLegacyEditingPosition(currentNode, lastOffset));
    }
    
    return position;
}

- (CGRect)elementRectAtPoint:(CGPoint)point
{
    Frame *frame = [self coreFrame];
    IntPoint adjustedPoint = frame->view()->windowToContents(roundedIntPoint(point));
    HitTestResult result = frame->eventHandler().hitTestResultAtPoint(adjustedPoint, HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::AllowChildFrameContent);
    Node* hitNode = result.innerNode();
    if (!hitNode || !hitNode->renderer())
        return IntRect();
    return result.innerNodeFrame()->view()->contentsToWindow(hitNode->renderer()->absoluteBoundingBoxRect(true));
}

@end

#endif  // PLATFORM(IOS)
