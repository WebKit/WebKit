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

#import "WebVisiblePositionInternal.h"

#if PLATFORM(IOS_FAMILY)

#import "DOMNodeInternal.h"
#import "DOMRangeInternal.h"
#import <WebCore/DocumentMarkerController.h>
#import <WebCore/Editing.h>
#import <WebCore/FrameSelection.h>
#import <WebCore/HTMLTextFormControlElement.h>
#import <WebCore/Node.h>
#import <WebCore/Position.h>
#import <WebCore/Range.h>
#import <WebCore/RenderTextControl.h>
#import <WebCore/RenderedDocumentMarker.h>
#import <WebCore/TextBoundaries.h>
#import <WebCore/TextFlags.h>
#import <WebCore/TextGranularity.h>
#import <WebCore/TextIterator.h>
#import <WebCore/VisibleUnits.h>
#import <wtf/cocoa/VectorCocoa.h>

using namespace WebCore;

//-------------------

@implementation WebVisiblePosition (Internal)

// Since VisiblePosition isn't refcounted, we have to use new and delete on a copy.

+ (WebVisiblePosition *)_wrapVisiblePosition:(VisiblePosition)visiblePosition
{
    WebVisiblePosition *vp = [[WebVisiblePosition alloc] init];
    VisiblePosition *copy = new VisiblePosition(visiblePosition);
    vp->_internal = reinterpret_cast<WebObjectInternal *>(copy);
    return [vp autorelease];
}

// Returns nil if visible position is null.
+ (WebVisiblePosition *)_wrapVisiblePositionIfValid:(VisiblePosition)visiblePosition
{
    return (visiblePosition.isNotNull() ? [WebVisiblePosition _wrapVisiblePosition:visiblePosition] : nil);
}

- (VisiblePosition)_visiblePosition
{
    VisiblePosition *vp = reinterpret_cast<VisiblePosition *>(_internal);
    return *vp;
}

@end

@implementation WebVisiblePosition

- (void)dealloc
{
    VisiblePosition *vp = reinterpret_cast<VisiblePosition *>(_internal);
    delete vp;
    _internal = nil;
    [super dealloc];
}

// FIXME: Overriding isEqual: without overriding hash will cause trouble if this ever goes into an NSSet or is the key in an NSDictionary,
// since two equal objects could have different hashes.
- (BOOL)isEqual:(id)other
{
    if (![other isKindOfClass:[WebVisiblePosition class]])
        return NO;
    return [self _visiblePosition] == [(WebVisiblePosition *)other _visiblePosition];
}

- (NSComparisonResult)compare:(WebVisiblePosition *)other
{
    VisiblePosition myVP = [self _visiblePosition];
    VisiblePosition otherVP = [other _visiblePosition];
    
    if (myVP == otherVP)
        return NSOrderedSame;
    else if (myVP < otherVP)
        return NSOrderedAscending;
    else
        return NSOrderedDescending;
}

- (int)distanceFromPosition:(WebVisiblePosition *)other
{
    return distanceBetweenPositions([self _visiblePosition], [other _visiblePosition]);
}

- (NSString *)description
{
    
    NSMutableString *description = [NSMutableString stringWithString:[super description]];
    VisiblePosition vp = [self _visiblePosition];
    int offset = vp.deepEquivalent().offsetInContainerNode();

    [description appendFormat:@"(offset=%d, context=([%c|%c], [u+%04x|u+%04x])", offset, vp.characterBefore(), vp.characterAfter(),
        vp.characterBefore(), vp.characterAfter()];
    
    return description;
}


- (TextDirection)textDirection
{
    // TODO: implement
    return TextDirection::LTR;
}

- (BOOL)directionIsDownstream:(WebTextAdjustmentDirection)direction
{
    if (direction == WebTextAdjustmentBackward)
        return NO;
    
    if (direction == WebTextAdjustmentForward)
        return YES;
    
    
    if ([self textDirection] == TextDirection::LTR)
        return (direction == WebTextAdjustmentRight);
    return (direction == WebTextAdjustmentLeft);
}

- (WebVisiblePosition *)positionByMovingInDirection:(WebTextAdjustmentDirection)direction amount:(UInt32)amount withAffinityDownstream:(BOOL)affinityDownstream
{
    VisiblePosition vp = [self _visiblePosition];
                          
    vp.setAffinity(affinityDownstream ? DOWNSTREAM : VP_UPSTREAM_IF_POSSIBLE);

    switch (direction) {
        case WebTextAdjustmentForward: {
            for (UInt32 i = 0; i < amount; i++)
                vp = vp.next();
            break;
        }
        case WebTextAdjustmentBackward: {
            for (UInt32 i = 0; i < amount; i++)
                vp = vp.previous();
            break;
        }
        case WebTextAdjustmentRight: {
            for (UInt32 i = 0; i < amount; i++)
                vp = vp.right();
            break;
        }
        case WebTextAdjustmentLeft: {
            for (UInt32 i = 0; i < amount; i++)
                vp = vp.left();
            break;
        }
        case WebTextAdjustmentUp: {
            int xOffset = vp.lineDirectionPointForBlockDirectionNavigation();
            for (UInt32 i = 0; i < amount; i++)
                vp = previousLinePosition(vp, xOffset);            
            break;
        }
        case WebTextAdjustmentDown: {
            int xOffset = vp.lineDirectionPointForBlockDirectionNavigation();
            for (UInt32 i = 0; i < amount; i++)
                vp = nextLinePosition(vp, xOffset);            
            break;
        }
        default: {
            ASSERT_NOT_REACHED();
            break;
        }
    }
    return [WebVisiblePosition _wrapVisiblePositionIfValid:vp];
}

- (WebVisiblePosition *)positionByMovingInDirection:(WebTextAdjustmentDirection)direction amount:(UInt32)amount

{
    return [self positionByMovingInDirection:direction amount:amount withAffinityDownstream:YES];
}

static inline TextGranularity toTextGranularity(WebTextGranularity webGranularity)
{
    TextGranularity granularity;
    
    switch (webGranularity) {
        case WebTextGranularityCharacter:
            granularity = TextGranularity::CharacterGranularity;
            break;

        case WebTextGranularityWord:
            granularity = TextGranularity::WordGranularity;
            break;

        case WebTextGranularitySentence:
            granularity = TextGranularity::SentenceGranularity;
            break;

        case WebTextGranularityLine:
            granularity = TextGranularity::LineGranularity;
            break;

        case WebTextGranularityParagraph:
            granularity = TextGranularity::ParagraphGranularity;
            break;

        case WebTextGranularityAll:
            granularity = TextGranularity::DocumentGranularity;
            break;

        default:
            ASSERT_NOT_REACHED();
            break;
    }
            
    return granularity;
}

static inline SelectionDirection toSelectionDirection(WebTextAdjustmentDirection direction)
{
    SelectionDirection result;
    
    switch (direction) {
        case WebTextAdjustmentForward:
            result = SelectionDirection::Forward;
            break;
            
        case WebTextAdjustmentBackward:
            result = SelectionDirection::Backward;
            break;
            
        case WebTextAdjustmentRight:
            result = SelectionDirection::Right;
            break;
            
        case WebTextAdjustmentLeft:
            result = SelectionDirection::Left;
            break;
        
        case WebTextAdjustmentUp:
            result = SelectionDirection::Left;
            break;
        
        case WebTextAdjustmentDown:
            result = SelectionDirection::Right;
            break;
    }

    return result;
}

// Returnes YES only if a position is at a boundary of a text unit of the specified granularity in the particular direction.
- (BOOL)atBoundaryOfGranularity:(WebTextGranularity)granularity inDirection:(WebTextAdjustmentDirection)direction
{
    return atBoundaryOfGranularity([self _visiblePosition], toTextGranularity(granularity), toSelectionDirection(direction));
}

// Returns the next boundary position of a text unit of the given granularity in the given direction, or nil if there is no such position.
- (WebVisiblePosition *)positionOfNextBoundaryOfGranularity:(WebTextGranularity)granularity inDirection:(WebTextAdjustmentDirection)direction
{
    return [WebVisiblePosition _wrapVisiblePositionIfValid:positionOfNextBoundaryOfGranularity([self _visiblePosition], toTextGranularity(granularity), toSelectionDirection(direction))];
}

// Returns YES if position is within a text unit of the given granularity.  If the position is at a boundary, returns YES only if
// if the boundary is part of the text unit in the given direction.
- (BOOL)withinTextUnitOfGranularity:(WebTextGranularity)granularity inDirectionIfAtBoundary:(WebTextAdjustmentDirection)direction
{
    return withinTextUnitOfGranularity([self _visiblePosition], toTextGranularity(granularity), toSelectionDirection(direction));
}

// Returns range of the enclosing text unit of the given granularity, or nil if there is no such enclosing unit.  Whether a boundary position
// is enclosed depends on the given direction, using the same rule as -[WebVisiblePosition withinTextUnitOfGranularity:inDirectionAtBoundary:].
- (DOMRange *)enclosingTextUnitOfGranularity:(WebTextGranularity)granularity inDirectionIfAtBoundary:(WebTextAdjustmentDirection)direction
{
    return kit(enclosingTextUnitOfGranularity([self _visiblePosition], toTextGranularity(granularity), toSelectionDirection(direction)));
}

- (WebVisiblePosition *)positionAtStartOrEndOfWord
{
    // Ripped from WebCore::Frame::moveSelectionToStartOrEndOfCurrentWord
    
    // Note: this is the iOS notion, not the unicode notion.
    // Here, a word starts with non-whitespace or at the start of a line and
    // ends at the next whitespace, or at the end of a line.
    
    // Selection rule: If the selection is before the first character or
    // just after the first character of a word that is longer than one
    // character, move to the start of the word. Otherwise, move to the
    // end of the word.
    
    VisiblePosition pos = [self _visiblePosition];
    VisiblePosition originalPos(pos);
    
    UChar32 ch = pos.characterAfter();
    bool isComplex = requiresContextForWordBoundary(ch);
    if (isComplex) {
        // for complex layout, find word around insertion point
        VisiblePosition visibleWordStart = startOfWord(pos);
        Position wordStart = visibleWordStart.deepEquivalent();
        
        // place caret in front of word if pos is within first 2 characters of word (see Selection Rule above)
        if (wordStart.deprecatedEditingOffset() + 1 >= pos.deepEquivalent().deprecatedEditingOffset()) {
            pos = wordStart;
        } else {
            // calculate end of word to insert caret after word
            VisiblePosition visibleWordEnd = endOfWord(pos);
            Position wordEnd = visibleWordEnd.deepEquivalent();
            pos = wordEnd;
        }
    } else {
        UChar32 c = pos.characterAfter();
        CFCharacterSetRef set = CFCharacterSetGetPredefined(kCFCharacterSetWhitespaceAndNewline);
        if (c == 0 || CFCharacterSetIsLongCharacterMember(set, c)) {
            // search backward for a non-space
            while (1) {
                if (pos.isNull() || isStartOfLine(pos))
                    break;
                c = pos.characterBefore();
                if (!CFCharacterSetIsLongCharacterMember(set, c))
                    break;
                pos = pos.previous(CannotCrossEditingBoundary);  // stay in editable content
            }
        }
        else {
            // See how far the selection extends into the current word.
            // Follow the rule stated above.
            int index = 0;
            while (1) {
                if (pos.isNull() || isStartOfLine(pos))
                    break;
                c = pos.characterBefore();
                if (CFCharacterSetIsLongCharacterMember(set, c))
                    break;
                pos = pos.previous(CannotCrossEditingBoundary);  // stay in editable content
                index++;
                if (index > 1)
                    break;
            }
            if (index > 1) {
                // search forward for a space
                pos = originalPos;
                while (1) {
                    if (pos.isNull() || isEndOfLine(pos))
                        break;
                    c = pos.characterAfter();
                    if (CFCharacterSetIsLongCharacterMember(set, c))
                        break;
                    pos = pos.next(CannotCrossEditingBoundary);  // stay in editable content
                }
            }
        }
    }
    
    return [WebVisiblePosition _wrapVisiblePositionIfValid:pos];
}

- (BOOL)isEditable
{
    return isEditablePosition([self _visiblePosition].deepEquivalent());
}

- (BOOL)requiresContextForWordBoundary
{
    VisiblePosition vp = [self _visiblePosition];
    return requiresContextForWordBoundary(vp.characterAfter()) || requiresContextForWordBoundary(vp.characterBefore());
}    

- (BOOL)atAlphaNumericBoundaryInDirection:(WebTextAdjustmentDirection)direction
{
    static CFCharacterSetRef set = CFCharacterSetGetPredefined(kCFCharacterSetAlphaNumeric);
    VisiblePosition pos = [self _visiblePosition];
    UChar32 charBefore = pos.characterBefore();
    UChar32 charAfter = pos.characterAfter();
    bool before = CFCharacterSetIsCharacterMember(set, charBefore);
    bool after = CFCharacterSetIsCharacterMember(set, charAfter);
    return [self directionIsDownstream:direction] ? (before && !after) : (!before && after);
}

- (DOMRange *)enclosingRangeWithDictationPhraseAlternatives:(NSArray **)alternatives
{
    ASSERT(alternatives);
    if (!alternatives)
        return nil;

    // *alternatives should not already point to an array.
    ASSERT(!*alternatives);
    *alternatives = nil;

    auto position = [self _visiblePosition];
    auto* node = position.deepEquivalent().anchorNode();
    if (!node)
        return nil;

    unsigned offset = position.deepEquivalent().deprecatedEditingOffset();
    auto& document = node->document();
    for (auto marker : document.markers().markersFor(*node, DocumentMarker::DictationPhraseWithAlternatives)) {
        if (marker->startOffset() <= offset && marker->endOffset() >= offset) {
            *alternatives = createNSArray(WTF::get<Vector<String>>(marker->data())).autorelease();
            return kit(makeSimpleRange(*node, *marker));
        }
    }
    return nil;
}

- (DOMRange *)enclosingRangeWithCorrectionIndicator
{
    auto position = [self _visiblePosition];
    auto* node = position.deepEquivalent().anchorNode();
    if (!node)
        return nil;

    unsigned offset = position.deepEquivalent().deprecatedEditingOffset();
    auto& document = node->document();
    for (auto marker : document.markers().markersFor(*node, DocumentMarker::Spelling)) {
        if (marker->startOffset() <= offset && marker->endOffset() >= offset)
            return kit(makeSimpleRange(*node, *marker));
    }
    return nil;
}

- (NSSelectionAffinity)affinity
{
    return (NSSelectionAffinity)[self _visiblePosition].affinity();
}

- (void)setAffinity:(NSSelectionAffinity)affinity
{
    reinterpret_cast<VisiblePosition *>(_internal)->setAffinity((WebCore::EAffinity)affinity);
}

@end

@implementation DOMRange (VisiblePositionExtensions)

- (WebVisiblePosition *)startPosition
{
    Range *range = core(self);
    return [WebVisiblePosition _wrapVisiblePosition:VisiblePosition(range->startPosition())];
}

- (WebVisiblePosition *)endPosition
{
    Range *range = core(self);
    return [WebVisiblePosition _wrapVisiblePosition:VisiblePosition(range->endPosition())];
}

- (DOMRange *)enclosingWordRange
{
    VisibleSelection selection([self.startPosition _visiblePosition], [self.endPosition _visiblePosition]);
    selection = FrameSelection::wordSelectionContainingCaretSelection(selection);
    WebVisiblePosition *start = [WebVisiblePosition _wrapVisiblePosition:selection.visibleStart()];
    WebVisiblePosition *end = [WebVisiblePosition _wrapVisiblePosition:selection.visibleEnd()];
    return [DOMRange rangeForFirstPosition:start second:end];
}

+ (DOMRange *)rangeForFirstPosition:(WebVisiblePosition *)first second:(WebVisiblePosition *)second
{
    auto firstPosition = [first _visiblePosition];
    auto secondPosition = [second _visiblePosition];
    if (firstPosition < secondPosition)
        std::swap(firstPosition, secondPosition);
    return kit(makeSimpleRange(firstPosition, secondPosition));
}

@end

@implementation DOMNode (VisiblePositionExtensions)

- (DOMRange *)rangeOfContents
{
    DOMRange *range = [[self ownerDocument] createRange];
    [range setStart:self offset:0];
    [range setEnd:self offset:[[self childNodes] length]];
    return range;    
}

- (WebVisiblePosition *)startPosition
{
    // When in editable content, we need to calculate the startPosition from the beginning of the
    // editable area.
    Node* node = core(self);
    if (node->isContentEditable()) {
        VisiblePosition vp(createLegacyEditingPosition(node, 0), VP_DEFAULT_AFFINITY);
        return [WebVisiblePosition _wrapVisiblePosition:startOfEditableContent(vp)];
    }
    return [[self rangeOfContents] startPosition];
}

- (WebVisiblePosition *)endPosition
{
    // When in editable content, we need to calculate the endPosition from the end of the
    // editable area.
    Node* node = core(self);
    if (node->isContentEditable()) {
        VisiblePosition vp(createLegacyEditingPosition(node, 0), VP_DEFAULT_AFFINITY);
        return [WebVisiblePosition _wrapVisiblePosition:endOfEditableContent(vp)];
    }
    return [[self rangeOfContents] endPosition];
}

@end

@implementation DOMHTMLInputElement (VisiblePositionExtensions)

- (WebVisiblePosition *)startPosition
{
    Node* node = core(self);
    RenderObject* object = node->renderer();
    if (!is<RenderTextControl>(object))
        return [super startPosition];
    
    VisiblePosition visiblePosition = downcast<RenderTextControl>(*object).textFormControlElement().visiblePositionForIndex(0);
    return [WebVisiblePosition _wrapVisiblePosition:visiblePosition];
}

- (WebVisiblePosition *)endPosition
{
    Node* node = core(self);
    RenderObject* object = node->renderer();
    if (!is<RenderTextControl>(object))
        return [super endPosition];
    
    RenderTextControl& textControl = downcast<RenderTextControl>(*object);
    VisiblePosition visiblePosition = textControl.textFormControlElement().visiblePositionForIndex(textControl.textFormControlElement().value().length());
    return [WebVisiblePosition _wrapVisiblePosition:visiblePosition];
}

@end

@implementation DOMHTMLTextAreaElement (VisiblePositionExtensions)

- (WebVisiblePosition *)startPosition
{
    Node* node = core(self);
    RenderObject* object = node->renderer();
    if (!object) 
        return [super startPosition];
    
    VisiblePosition visiblePosition = downcast<RenderTextControl>(*object).textFormControlElement().visiblePositionForIndex(0);
    return [WebVisiblePosition _wrapVisiblePosition:visiblePosition];
}

- (WebVisiblePosition *)endPosition
{
    Node* node = core(self);
    RenderObject* object = node->renderer();
    if (!object) 
        return [super endPosition];
    
    RenderTextControl& textControl = downcast<RenderTextControl>(*object);
    VisiblePosition visiblePosition = textControl.textFormControlElement().visiblePositionForIndex(textControl.textFormControlElement().value().length());
    return [WebVisiblePosition _wrapVisiblePosition:visiblePosition];
}

@end

#endif // PLATFORM(IOS_FAMILY)
