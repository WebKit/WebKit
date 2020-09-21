/*
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "VisiblePosition.h"

#include "BoundaryPoint.h"
#include "Document.h"
#include "Editing.h"
#include "FloatQuad.h"
#include "HTMLElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLNames.h"
#include "InlineTextBox.h"
#include "Logging.h"
#include "Range.h"
#include "RenderBlock.h"
#include "RootInlineBox.h"
#include "SimpleRange.h"
#include "Text.h"
#include "VisibleUnits.h"
#include <stdio.h>
#include <wtf/text/CString.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

using namespace HTMLNames;

VisiblePosition::VisiblePosition(const Position& position, Affinity affinity)
    : m_deepPosition { canonicalPosition(position) }
{
    if (affinity == Affinity::Upstream && !isNull()) {
        auto upstreamCopy = *this;
        upstreamCopy.m_affinity = Affinity::Upstream;
        if (!inSameLine(*this, upstreamCopy))
            m_affinity = Affinity::Upstream;
    }
}

VisiblePosition VisiblePosition::next(EditingBoundaryCrossingRule rule, bool* reachedBoundary) const
{
    if (reachedBoundary)
        *reachedBoundary = false;
    // FIXME: Support CanSkipEditingBoundary
    ASSERT(rule == CanCrossEditingBoundary || rule == CannotCrossEditingBoundary);
    VisiblePosition next(nextVisuallyDistinctCandidate(m_deepPosition), m_affinity);

    if (rule == CanCrossEditingBoundary)
        return next;

    return honorEditingBoundaryAtOrAfter(next, reachedBoundary);
}

VisiblePosition VisiblePosition::previous(EditingBoundaryCrossingRule rule, bool* reachedBoundary) const
{
    if (reachedBoundary)
        *reachedBoundary = false;
    // FIXME: Support CanSkipEditingBoundary
    ASSERT(rule == CanCrossEditingBoundary || rule == CannotCrossEditingBoundary);
    // find first previous DOM position that is visible
    Position pos = previousVisuallyDistinctCandidate(m_deepPosition);

    // return null visible position if there is no previous visible position
    if (pos.atStartOfTree()) {
        if (reachedBoundary)
            *reachedBoundary = true;
        return VisiblePosition();
    }

    VisiblePosition prev = pos;
    ASSERT(prev != *this);

#if ASSERT_ENABLED
    // We should always be able to make the affinity downstream, because going previous from an
    // upstream position can never yield another upstream position unless line wrap length is 0.
    if (prev.isNotNull() && m_affinity == Affinity::Upstream) {
        auto upstreamCopy = prev;
        upstreamCopy.setAffinity(Affinity::Upstream);
        ASSERT(inSameLine(upstreamCopy, prev));
    }
#endif

    if (rule == CanCrossEditingBoundary)
        return prev;
    
    return honorEditingBoundaryAtOrBefore(prev, reachedBoundary);
}

Position VisiblePosition::leftVisuallyDistinctCandidate() const
{
    Position p = m_deepPosition;
    if (p.isNull())
        return Position();

    Position downstreamStart = p.downstream();
    TextDirection primaryDirection = p.primaryDirection();

    while (true) {
        auto [box, offset] = p.inlineBoxAndOffset(m_affinity, primaryDirection);
        if (!box)
            return primaryDirection == TextDirection::LTR ? previousVisuallyDistinctCandidate(m_deepPosition) : nextVisuallyDistinctCandidate(m_deepPosition);

        RenderObject* renderer = &box->renderer();

        while (true) {
            if ((renderer->isReplaced() || renderer->isBR()) && offset == box->caretRightmostOffset())
                return box->isLeftToRightDirection() ? previousVisuallyDistinctCandidate(m_deepPosition) : nextVisuallyDistinctCandidate(m_deepPosition);

            if (!renderer->node()) {
                box = box->previousLeafOnLine();
                if (!box)
                    return primaryDirection == TextDirection::LTR ? previousVisuallyDistinctCandidate(m_deepPosition) : nextVisuallyDistinctCandidate(m_deepPosition);
                renderer = &box->renderer();
                offset = box->caretRightmostOffset();
                continue;
            }

            offset = box->isLeftToRightDirection() ? renderer->previousOffset(offset) : renderer->nextOffset(offset);

            int caretMinOffset = box->caretMinOffset();
            int caretMaxOffset = box->caretMaxOffset();

            if (offset > caretMinOffset && offset < caretMaxOffset)
                break;

            if (box->isLeftToRightDirection() ? offset < caretMinOffset : offset > caretMaxOffset) {
                // Overshot to the left.
                InlineBox* prevBox = box->previousLeafOnLineIgnoringLineBreak();
                if (!prevBox) {
                    Position positionOnLeft = primaryDirection == TextDirection::LTR ? previousVisuallyDistinctCandidate(m_deepPosition) : nextVisuallyDistinctCandidate(m_deepPosition);
                    auto boxOnLeft = positionOnLeft.inlineBoxAndOffset(m_affinity, primaryDirection).box;
                    if (boxOnLeft && &boxOnLeft->root() == &box->root())
                        return Position();
                    return positionOnLeft;
                }

                // Reposition at the other logical position corresponding to our edge's visual position and go for another round.
                box = prevBox;
                renderer = &box->renderer();
                offset = prevBox->caretRightmostOffset();
                continue;
            }

            ASSERT(offset == box->caretLeftmostOffset());

            unsigned char level = box->bidiLevel();
            InlineBox* prevBox = box->previousLeafOnLine();

            if (box->direction() == primaryDirection) {
                if (!prevBox) {
                    InlineBox* logicalStart = nullptr;
                    if (primaryDirection == TextDirection::LTR ? box->root().getLogicalStartBoxWithNode(logicalStart) : box->root().getLogicalEndBoxWithNode(logicalStart)) {
                        box = logicalStart;
                        renderer = &box->renderer();
                        offset = primaryDirection == TextDirection::LTR ? box->caretMinOffset() : box->caretMaxOffset();
                    }
                    break;
                }
                if (prevBox->bidiLevel() >= level)
                    break;

                level = prevBox->bidiLevel();

                InlineBox* nextBox = box;
                do {
                    nextBox = nextBox->nextLeafOnLine();
                } while (nextBox && nextBox->bidiLevel() > level);

                if (nextBox && nextBox->bidiLevel() == level)
                    break;

                box = prevBox;
                renderer = &box->renderer();
                offset = box->caretRightmostOffset();
                if (box->direction() == primaryDirection)
                    break;
                continue;
            }

            while (prevBox && !prevBox->renderer().node())
                prevBox = prevBox->previousLeafOnLine();

            if (prevBox) {
                box = prevBox;
                renderer = &box->renderer();
                offset = box->caretRightmostOffset();
                if (box->bidiLevel() > level) {
                    do {
                        prevBox = prevBox->previousLeafOnLine();
                    } while (prevBox && prevBox->bidiLevel() > level);

                    if (!prevBox || prevBox->bidiLevel() < level)
                        continue;
                }
            } else {
                // Trailing edge of a secondary run. Set to the leading edge of the entire run.
                while (true) {
                    while (InlineBox* nextBox = box->nextLeafOnLine()) {
                        if (nextBox->bidiLevel() < level)
                            break;
                        box = nextBox;
                    }
                    if (box->bidiLevel() == level)
                        break;
                    level = box->bidiLevel();
                    while (InlineBox* prevBox = box->previousLeafOnLine()) {
                        if (prevBox->bidiLevel() < level)
                            break;
                        box = prevBox;
                    }
                    if (box->bidiLevel() == level)
                        break;
                    level = box->bidiLevel();
                }
                renderer = &box->renderer();
                offset = primaryDirection == TextDirection::LTR ? box->caretMinOffset() : box->caretMaxOffset();
            }
            break;
        }

        p = makeDeprecatedLegacyPosition(renderer->node(), offset);

        if ((p.isCandidate() && p.downstream() != downstreamStart) || p.atStartOfTree() || p.atEndOfTree())
            return p;

        ASSERT(p != m_deepPosition);
    }
}

VisiblePosition VisiblePosition::left(bool stayInEditableContent, bool* reachedBoundary) const
{
    if (reachedBoundary)
        *reachedBoundary = false;
    Position pos = leftVisuallyDistinctCandidate();
    // FIXME: Why can't we move left from the last position in a tree?
    if (pos.atStartOfTree() || pos.atEndOfTree()) {
        if (reachedBoundary)
            *reachedBoundary = true;
        return VisiblePosition();
    }

    VisiblePosition left = pos;
    ASSERT(left != *this);

    if (!stayInEditableContent)
        return left;

    // FIXME: This may need to do something different from "before".
    return honorEditingBoundaryAtOrBefore(left, reachedBoundary);
}

Position VisiblePosition::rightVisuallyDistinctCandidate() const
{
    Position p = m_deepPosition;
    if (p.isNull())
        return Position();

    Position downstreamStart = p.downstream();
    TextDirection primaryDirection = p.primaryDirection();

    while (true) {
        auto [box, offset] = p.inlineBoxAndOffset(m_affinity, primaryDirection);
        if (!box)
            return primaryDirection == TextDirection::LTR ? nextVisuallyDistinctCandidate(m_deepPosition) : previousVisuallyDistinctCandidate(m_deepPosition);

        RenderObject* renderer = &box->renderer();

        while (true) {
            if ((renderer->isReplaced() || renderer->isBR()) && offset == box->caretLeftmostOffset())
                return box->isLeftToRightDirection() ? nextVisuallyDistinctCandidate(m_deepPosition) : previousVisuallyDistinctCandidate(m_deepPosition);

            if (!renderer->node()) {
                box = box->nextLeafOnLine();
                if (!box)
                    return primaryDirection == TextDirection::LTR ? nextVisuallyDistinctCandidate(m_deepPosition) : previousVisuallyDistinctCandidate(m_deepPosition);
                renderer = &box->renderer();
                offset = box->caretLeftmostOffset();
                continue;
            }

            offset = box->isLeftToRightDirection() ? renderer->nextOffset(offset) : renderer->previousOffset(offset);

            int caretMinOffset = box->caretMinOffset();
            int caretMaxOffset = box->caretMaxOffset();

            if (offset > caretMinOffset && offset < caretMaxOffset)
                break;

            if (box->isLeftToRightDirection() ? offset > caretMaxOffset : offset < caretMinOffset) {
                // Overshot to the right.
                InlineBox* nextBox = box->nextLeafOnLineIgnoringLineBreak();
                if (!nextBox) {
                    Position positionOnRight = primaryDirection == TextDirection::LTR ? nextVisuallyDistinctCandidate(m_deepPosition) : previousVisuallyDistinctCandidate(m_deepPosition);
                    auto boxOnRight = positionOnRight.inlineBoxAndOffset(m_affinity, primaryDirection).box;
                    if (boxOnRight && &boxOnRight->root() == &box->root())
                        return Position();
                    return positionOnRight;
                }

                // Reposition at the other logical position corresponding to our edge's visual position and go for another round.
                box = nextBox;
                renderer = &box->renderer();
                offset = nextBox->caretLeftmostOffset();
                continue;
            }

            ASSERT(offset == box->caretRightmostOffset());

            unsigned char level = box->bidiLevel();
            InlineBox* nextBox = box->nextLeafOnLine();

            if (box->direction() == primaryDirection) {
                if (!nextBox) {
                    InlineBox* logicalEnd = nullptr;
                    if (primaryDirection == TextDirection::LTR ? box->root().getLogicalEndBoxWithNode(logicalEnd) : box->root().getLogicalStartBoxWithNode(logicalEnd)) {
                        box = logicalEnd;
                        renderer = &box->renderer();
                        offset = primaryDirection == TextDirection::LTR ? box->caretMaxOffset() : box->caretMinOffset();
                    }
                    break;
                }

                if (nextBox->bidiLevel() >= level)
                    break;

                level = nextBox->bidiLevel();

                InlineBox* prevBox = box;
                do {
                    prevBox = prevBox->previousLeafOnLine();
                } while (prevBox && prevBox->bidiLevel() > level);

                if (prevBox && prevBox->bidiLevel() == level)   // For example, abc FED 123 ^ CBA
                    break;

                // For example, abc 123 ^ CBA or 123 ^ CBA abc
                box = nextBox;
                renderer = &box->renderer();
                offset = box->caretLeftmostOffset();
                if (box->direction() == primaryDirection)
                    break;
                continue;
            }

            while (nextBox && !nextBox->renderer().node())
                nextBox = nextBox->nextLeafOnLine();

            if (nextBox) {
                box = nextBox;
                renderer = &box->renderer();
                offset = box->caretLeftmostOffset();

                if (box->bidiLevel() > level) {
                    do {
                        nextBox = nextBox->nextLeafOnLine();
                    } while (nextBox && nextBox->bidiLevel() > level);

                    if (!nextBox || nextBox->bidiLevel() < level)
                        continue;
                }
            } else {
                // Trailing edge of a secondary run. Set to the leading edge of the entire run.
                while (true) {
                    while (InlineBox* prevBox = box->previousLeafOnLine()) {
                        if (prevBox->bidiLevel() < level)
                            break;
                        box = prevBox;
                    }
                    if (box->bidiLevel() == level)
                        break;
                    level = box->bidiLevel();
                    while (InlineBox* nextBox = box->nextLeafOnLine()) {
                        if (nextBox->bidiLevel() < level)
                            break;
                        box = nextBox;
                    }
                    if (box->bidiLevel() == level)
                        break;
                    level = box->bidiLevel();
                }
                renderer = &box->renderer();
                offset = primaryDirection == TextDirection::LTR ? box->caretMaxOffset() : box->caretMinOffset();
            }
            break;
        }

        p = makeDeprecatedLegacyPosition(renderer->node(), offset);

        if ((p.isCandidate() && p.downstream() != downstreamStart) || p.atStartOfTree() || p.atEndOfTree())
            return p;

        ASSERT(p != m_deepPosition);
    }
}

VisiblePosition VisiblePosition::right(bool stayInEditableContent, bool* reachedBoundary) const
{
    if (reachedBoundary)
        *reachedBoundary = false;
    Position pos = rightVisuallyDistinctCandidate();
    // FIXME: Why can't we move left from the last position in a tree?
    if (pos.atStartOfTree() || pos.atEndOfTree()) {
        if (reachedBoundary)
            *reachedBoundary = true;
        return VisiblePosition();
    }

    VisiblePosition right = pos;
    ASSERT(right != *this);

    if (!stayInEditableContent)
        return right;

    // FIXME: This may need to do something different from "after".
    return honorEditingBoundaryAtOrAfter(right, reachedBoundary);
}

VisiblePosition VisiblePosition::honorEditingBoundaryAtOrBefore(const VisiblePosition& position, bool* reachedBoundary) const
{
    if (reachedBoundary)
        *reachedBoundary = false;
    if (position.isNull())
        return position;
    
    auto* highestRoot = highestEditableRoot(deepEquivalent());
    
    // Return empty position if pos is not somewhere inside the editable region containing this position
    if (highestRoot && !position.deepEquivalent().deprecatedNode()->isDescendantOf(*highestRoot)) {
        if (reachedBoundary)
            *reachedBoundary = true;
        return VisiblePosition();
    }
    
    // Return position itself if the two are from the very same editable region, or both are non-editable
    // FIXME: In the non-editable case, just because the new position is non-editable doesn't mean movement
    // to it is allowed.  VisibleSelection::adjustForEditableContent has this problem too.
    if (highestEditableRoot(position.deepEquivalent()) == highestRoot) {
        if (reachedBoundary)
            *reachedBoundary = *this == position;
        return position;
    }
  
    // Return empty position if this position is non-editable, but pos is editable
    // FIXME: Move to the previous non-editable region.
    if (!highestRoot) {
        if (reachedBoundary)
            *reachedBoundary = true;
        return VisiblePosition();
    }

    // Return the last position before pos that is in the same editable region as this position
    return lastEditablePositionBeforePositionInRoot(position.deepEquivalent(), highestRoot);
}

VisiblePosition VisiblePosition::honorEditingBoundaryAtOrAfter(const VisiblePosition& otherPosition, bool* reachedBoundary) const
{
    if (reachedBoundary)
        *reachedBoundary = false;
    if (otherPosition.isNull())
        return otherPosition;
    
    auto* highestRoot = highestEditableRoot(deepEquivalent());
    
    // Return empty position if otherPosition is not somewhere inside the editable region containing this position
    if (highestRoot && !otherPosition.deepEquivalent().deprecatedNode()->isDescendantOf(*highestRoot)) {
        if (reachedBoundary)
            *reachedBoundary = true;
        return VisiblePosition();
    }
    
    // Return otherPosition itself if the two are from the very same editable region, or both are non-editable
    // FIXME: In the non-editable case, just because the new position is non-editable doesn't mean movement
    // to it is allowed.  VisibleSelection::adjustForEditableContent has this problem too.
    if (highestEditableRoot(otherPosition.deepEquivalent()) == highestRoot) {
        if (reachedBoundary)
            *reachedBoundary = *this == otherPosition;
        return otherPosition;
    }

    // Return empty position if this position is non-editable, but otherPosition is editable
    // FIXME: Move to the next non-editable region.
    if (!highestRoot) {
        if (reachedBoundary)
            *reachedBoundary = true;
        return VisiblePosition();
    }

    // Return the next position after pos that is in the same editable region as this position
    return firstEditablePositionAfterPositionInRoot(otherPosition.deepEquivalent(), highestRoot);
}

static Position canonicalizeCandidate(const Position& candidate)
{
    if (candidate.isNull())
        return Position();
    ASSERT(candidate.isCandidate());
    Position upstream = candidate.upstream();
    if (upstream.isCandidate())
        return upstream;
    return candidate;
}

Position VisiblePosition::canonicalPosition(const Position& passedPosition)
{
    // The updateLayout call below can do so much that even the position passed
    // in to us might get changed as a side effect. Specifically, there are code
    // paths that pass selection endpoints, and updateLayout can change the selection.
    Position position = passedPosition;

    // FIXME (9535):  Canonicalizing to the leftmost candidate means that if we're at a line wrap, we will 
    // ask renderers to paint downstream carets for other renderers.
    // To fix this, we need to either a) add code to all paintCarets to pass the responsibility off to
    // the appropriate renderer for VisiblePosition's like these, or b) canonicalize to the rightmost candidate
    // unless the affinity is upstream.
    if (position.isNull())
        return Position();

    ASSERT(position.document());
    position.document()->updateLayoutIgnorePendingStylesheets();

    Node* node = position.containerNode();

    Position candidate = position.upstream();
    if (candidate.isCandidate())
        return candidate;
    candidate = position.downstream();
    if (candidate.isCandidate())
        return candidate;

    // When neither upstream or downstream gets us to a candidate (upstream/downstream won't leave 
    // blocks or enter new ones), we search forward and backward until we find one.
    Position next = canonicalizeCandidate(nextCandidate(position));
    Position prev = canonicalizeCandidate(previousCandidate(position));
    Node* nextNode = next.deprecatedNode();
    Node* prevNode = prev.deprecatedNode();

    // The new position must be in the same editable element. Enforce that first.
    // Unless the descent is from a non-editable html element to an editable body.
    if (is<HTMLHtmlElement>(node) && !node->hasEditableStyle()) {
        auto* body = node->document().bodyOrFrameset();
        if (body && body->hasEditableStyle())
            return next.isNotNull() ? next : prev;
    }

    Node* editingRoot = editableRootForPosition(position);
        
    // If the html element is editable, descending into its body will look like a descent 
    // from non-editable to editable content since rootEditableElement() always stops at the body.
    if ((editingRoot && editingRoot->hasTagName(htmlTag)) || (node && (node->isDocumentNode() || node->isShadowRoot())))
        return next.isNotNull() ? next : prev;
        
    bool prevIsInSameEditableElement = prevNode && editableRootForPosition(prev) == editingRoot;
    bool nextIsInSameEditableElement = nextNode && editableRootForPosition(next) == editingRoot;
    if (prevIsInSameEditableElement && !nextIsInSameEditableElement)
        return prev;

    if (nextIsInSameEditableElement && !prevIsInSameEditableElement)
        return next;

    if (!nextIsInSameEditableElement && !prevIsInSameEditableElement)
        return Position();

    // The new position should be in the same block flow element. Favor that.
    Element* originalBlock = deprecatedEnclosingBlockFlowElement(node);
    bool nextIsOutsideOriginalBlock = !nextNode->isDescendantOf(originalBlock) && nextNode != originalBlock;
    bool prevIsOutsideOriginalBlock = !prevNode->isDescendantOf(originalBlock) && prevNode != originalBlock;
    if (nextIsOutsideOriginalBlock && !prevIsOutsideOriginalBlock)
        return prev;
        
    return next;
}

UChar32 VisiblePosition::characterAfter() const
{
    // We canonicalize to the first of two equivalent candidates, but the second of the two candidates
    // is the one that will be inside the text node containing the character after this visible position.
    Position pos = m_deepPosition.downstream();
    if (!pos.containerNode() || !pos.containerNode()->isTextNode())
        return 0;
    switch (pos.anchorType()) {
    case Position::PositionIsAfterChildren:
    case Position::PositionIsAfterAnchor:
    case Position::PositionIsBeforeAnchor:
    case Position::PositionIsBeforeChildren:
        return 0;
    case Position::PositionIsOffsetInAnchor:
        break;
    }
    unsigned offset = static_cast<unsigned>(pos.offsetInContainerNode());
    Text* textNode = pos.containerText();
    unsigned length = textNode->length();
    if (offset >= length)
        return 0;

    UChar32 ch;
    U16_NEXT(textNode->data(), offset, length, ch);
    return ch;
}

auto VisiblePosition::localCaretRect() const -> LocalCaretRect
{
    auto node = m_deepPosition.anchorNode();
    if (!node)
        return { };

    auto [inlineBox, caretOffset] = inlineBoxAndOffset();
    auto renderer = inlineBox ? &inlineBox->renderer() : node->renderer();
    if (!renderer)
        return { };

    return { renderer->localCaretRect(inlineBox, caretOffset), renderer };
}

IntRect VisiblePosition::absoluteCaretBounds(bool* insideFixed) const
{
    RenderBlock* renderer = nullptr;
    LayoutRect localRect = localCaretRectInRendererForCaretPainting(*this, renderer);
    return absoluteBoundsForLocalCaretRect(renderer, localRect, insideFixed);
}

FloatRect VisiblePosition::absoluteSelectionBoundsForLine() const
{
    auto inlineBox = inlineBoxAndOffset().box;
    if (!inlineBox)
        return { };

    auto& root = inlineBox->root();
    auto localRect = FloatRect { root.x(), root.selectionTop(), root.width(), root.selectionHeight() };
    return root.renderer().localToAbsoluteQuad(localRect).boundingBox();
}

int VisiblePosition::lineDirectionPointForBlockDirectionNavigation() const
{
    auto localRect = localCaretRect();
    if (localRect.rect.isEmpty() || !localRect.renderer)
        return 0;

    // This ignores transforms on purpose, for now. Vertical navigation is done
    // without consulting transforms, so that 'up' in transformed text is 'up'
    // relative to the text, not absolute 'up'.
    auto caretPoint = localRect.renderer->localToAbsolute(localRect.rect.location());
    RenderObject* containingBlock = localRect.renderer->containingBlock();
    if (!containingBlock)
        containingBlock = localRect.renderer; // Just use ourselves to determine the writing mode if we have no containing block.
    return containingBlock->isHorizontalWritingMode() ? caretPoint.x() : caretPoint.y();
}

#if ENABLE(TREE_DEBUGGING)

void VisiblePosition::debugPosition(const char* msg) const
{
    if (isNull())
        fprintf(stderr, "Position [%s]: null\n", msg);
    else {
        fprintf(stderr, "Position [%s]: %s, ", msg, m_deepPosition.deprecatedNode()->nodeName().utf8().data());
        m_deepPosition.showAnchorTypeAndOffset();
    }
}

String VisiblePosition::debugDescription() const
{
    return m_deepPosition.debugDescription();
}

void VisiblePosition::showTreeForThis() const
{
    m_deepPosition.showTreeForThis();
}

#endif

// FIXME: Maybe this should be deprecated too, like the underlying function?
Element* enclosingBlockFlowElement(const VisiblePosition& visiblePosition)
{
    if (visiblePosition.isNull())
        return nullptr;

    return deprecatedEnclosingBlockFlowElement(visiblePosition.deepEquivalent().deprecatedNode());
}

bool isFirstVisiblePositionInNode(const VisiblePosition& visiblePosition, const Node* node)
{
    if (visiblePosition.isNull())
        return false;

    if (!visiblePosition.deepEquivalent().containerNode()->isDescendantOf(node))
        return false;

    VisiblePosition previous = visiblePosition.previous();
    return previous.isNull() || !previous.deepEquivalent().deprecatedNode()->isDescendantOf(node);
}

bool isLastVisiblePositionInNode(const VisiblePosition& visiblePosition, const Node* node)
{
    if (visiblePosition.isNull())
        return false;

    if (!visiblePosition.deepEquivalent().containerNode()->isDescendantOf(node))
        return false;

    VisiblePosition next = visiblePosition.next();
    return next.isNull() || !next.deepEquivalent().deprecatedNode()->isDescendantOf(node);
}

bool areVisiblePositionsInSameTreeScope(const VisiblePosition& a, const VisiblePosition& b)
{
    return connectedInSameTreeScope(a.deepEquivalent().anchorNode(), b.deepEquivalent().anchorNode());
}

bool VisiblePosition::equals(const VisiblePosition& other) const
{
    return m_affinity == other.m_affinity && m_deepPosition.equals(other.m_deepPosition);
}

Optional<BoundaryPoint> makeBoundaryPoint(const VisiblePosition& position)
{
    return makeBoundaryPoint(position.deepEquivalent());
}

TextStream& operator<<(TextStream& stream, Affinity affinity)
{
    switch (affinity) {
    case Affinity::Upstream:
        stream << "upstream";
        break;
    case Affinity::Downstream:
        stream << "downstream";
        break;
    }
    return stream;
}

TextStream& operator<<(TextStream& stream, const VisiblePosition& visiblePosition)
{
    TextStream::GroupScope scope(stream);
    stream << "VisiblePosition " << &visiblePosition;

    stream.dumpProperty("position", visiblePosition.deepEquivalent());
    stream.dumpProperty("affinity", visiblePosition.affinity());

    return stream;
}

Optional<SimpleRange> makeSimpleRange(const VisiblePositionRange& range)
{
    return makeSimpleRange(range.start, range.end);
}

PartialOrdering documentOrder(const VisiblePosition& a, const VisiblePosition& b)
{
    // FIXME: Should two positions with different affinity be considered equivalent or not?
    return documentOrder(a.deepEquivalent(), b.deepEquivalent());
}

}  // namespace WebCore

#if ENABLE(TREE_DEBUGGING)

void showTree(const WebCore::VisiblePosition* vpos)
{
    if (vpos)
        vpos->showTreeForThis();
}

void showTree(const WebCore::VisiblePosition& vpos)
{
    vpos.showTreeForThis();
}

#endif
