/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "Document.h"
#include "FloatQuad.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "InlineTextBox.h"
#include "Logging.h"
#include "Range.h"
#include "Text.h"
#include "htmlediting.h"
#include "visible_units.h"
#include <stdio.h>
#include <wtf/text/CString.h>

namespace WebCore {

using namespace HTMLNames;

VisiblePosition::VisiblePosition(const Position &pos, EAffinity affinity)
{
    init(pos, affinity);
}

VisiblePosition::VisiblePosition(Node *node, int offset, EAffinity affinity)
{
    ASSERT(offset >= 0);
    init(Position(node, offset), affinity);
}

void VisiblePosition::init(const Position& position, EAffinity affinity)
{
    m_affinity = affinity;
    
    m_deepPosition = canonicalPosition(position);
    
    // When not at a line wrap, make sure to end up with DOWNSTREAM affinity.
    if (m_affinity == UPSTREAM && (isNull() || inSameLine(VisiblePosition(position, DOWNSTREAM), *this)))
        m_affinity = DOWNSTREAM;
}

VisiblePosition VisiblePosition::next(bool stayInEditableContent) const
{
    VisiblePosition next(nextVisuallyDistinctCandidate(m_deepPosition), m_affinity);
    
    if (!stayInEditableContent)
        return next;
    
    return honorEditableBoundaryAtOrAfter(next);
}

VisiblePosition VisiblePosition::previous(bool stayInEditableContent) const
{
    // find first previous DOM position that is visible
    Position pos = previousVisuallyDistinctCandidate(m_deepPosition);
    
    // return null visible position if there is no previous visible position
    if (pos.atStartOfTree())
        return VisiblePosition();
        
    VisiblePosition prev = VisiblePosition(pos, DOWNSTREAM);
    ASSERT(prev != *this);
    
#ifndef NDEBUG
    // we should always be able to make the affinity DOWNSTREAM, because going previous from an
    // UPSTREAM position can never yield another UPSTREAM position (unless line wrap length is 0!).
    if (prev.isNotNull() && m_affinity == UPSTREAM) {
        VisiblePosition temp = prev;
        temp.setAffinity(UPSTREAM);
        ASSERT(inSameLine(temp, prev));
    }
#endif

    if (!stayInEditableContent)
        return prev;
    
    return honorEditableBoundaryAtOrBefore(prev);
}

Position VisiblePosition::leftVisuallyDistinctCandidate() const
{
    Position p = m_deepPosition;
    if (!p.node())
        return Position();

    Position downstreamStart = p.downstream();
    TextDirection primaryDirection = p.primaryDirection();

    while (true) {
        InlineBox* box;
        int offset;
        p.getInlineBoxAndOffset(m_affinity, primaryDirection, box, offset);
        if (!box)
            return primaryDirection == LTR ? previousVisuallyDistinctCandidate(m_deepPosition) : nextVisuallyDistinctCandidate(m_deepPosition);

        RenderObject* renderer = box->renderer();

        while (true) {
            if ((renderer->isReplaced() || renderer->isBR()) && offset == box->caretRightmostOffset())
                return box->isLeftToRightDirection() ? previousVisuallyDistinctCandidate(m_deepPosition) : nextVisuallyDistinctCandidate(m_deepPosition);

            offset = box->isLeftToRightDirection() ? renderer->previousOffset(offset) : renderer->nextOffset(offset);

            int caretMinOffset = box->caretMinOffset();
            int caretMaxOffset = box->caretMaxOffset();

            if (offset > caretMinOffset && offset < caretMaxOffset)
                break;

            if (box->isLeftToRightDirection() ? offset < caretMinOffset : offset > caretMaxOffset) {
                // Overshot to the left.
                InlineBox* prevBox = box->prevLeafChild();
                if (!prevBox)
                    return primaryDirection == LTR ? previousVisuallyDistinctCandidate(m_deepPosition) : nextVisuallyDistinctCandidate(m_deepPosition);

                // Reposition at the other logical position corresponding to our edge's visual position and go for another round.
                box = prevBox;
                renderer = box->renderer();
                offset = prevBox->caretRightmostOffset();
                continue;
            }

            ASSERT(offset == box->caretLeftmostOffset());

            unsigned char level = box->bidiLevel();
            InlineBox* prevBox = box->prevLeafChild();

            if (box->direction() == primaryDirection) {
                if (!prevBox || prevBox->bidiLevel() >= level)
                    break;

                level = prevBox->bidiLevel();

                InlineBox* nextBox = box;
                do {
                    nextBox = nextBox->nextLeafChild();
                } while (nextBox && nextBox->bidiLevel() > level);

                if (nextBox && nextBox->bidiLevel() == level)
                    break;

                while (InlineBox* prevBox = box->prevLeafChild()) {
                    if (prevBox->bidiLevel() < level)
                        break;
                    box = prevBox;
                }
                renderer = box->renderer();
                offset = box->caretRightmostOffset();
                if (box->direction() == primaryDirection)
                    break;
                continue;
            }

            if (prevBox) {
                box = prevBox;
                renderer = box->renderer();
                offset = box->caretRightmostOffset();
                if (box->bidiLevel() > level) {
                    do {
                        prevBox = box->prevLeafChild();
                    } while (prevBox && prevBox->bidiLevel() > level);

                    if (!prevBox || prevBox->bidiLevel() < level)
                        continue;
                }
            } else {
                // Trailing edge of a secondary run. Set to the leading edge of the entire run.
                while (true) {
                    while (InlineBox* nextBox = box->nextLeafChild()) {
                        if (nextBox->bidiLevel() < level)
                            break;
                        box = nextBox;
                    }
                    if (box->bidiLevel() == level)
                        break;
                    level = box->bidiLevel();
                    while (InlineBox* prevBox = box->prevLeafChild()) {
                        if (prevBox->bidiLevel() < level)
                            break;
                        box = prevBox;
                    }
                    if (box->bidiLevel() == level)
                        break;
                    level = box->bidiLevel();
                }
                renderer = box->renderer();
                offset = primaryDirection == LTR ? box->caretMinOffset() : box->caretMaxOffset();
            }
            break;
        }

        p = Position(renderer->node(), offset);

        if ((p.isCandidate() && p.downstream() != downstreamStart) || p.atStartOfTree() || p.atEndOfTree())
            return p;
    }
}

VisiblePosition VisiblePosition::left(bool stayInEditableContent) const
{
    Position pos = leftVisuallyDistinctCandidate();
    // FIXME: Why can't we move left from the last position in a tree?
    if (pos.atStartOfTree() || pos.atEndOfTree())
        return VisiblePosition();

    VisiblePosition left = VisiblePosition(pos, DOWNSTREAM);
    ASSERT(left != *this);

    if (!stayInEditableContent)
        return left;

    // FIXME: This may need to do something different from "before".
    return honorEditableBoundaryAtOrBefore(left);
}

Position VisiblePosition::rightVisuallyDistinctCandidate() const
{
    Position p = m_deepPosition;
    if (!p.node())
        return Position();

    Position downstreamStart = p.downstream();
    TextDirection primaryDirection = p.primaryDirection();

    while (true) {
        InlineBox* box;
        int offset;
        p.getInlineBoxAndOffset(m_affinity, primaryDirection, box, offset);
        if (!box)
            return primaryDirection == LTR ? nextVisuallyDistinctCandidate(m_deepPosition) : previousVisuallyDistinctCandidate(m_deepPosition);

        RenderObject* renderer = box->renderer();

        while (true) {
            if ((renderer->isReplaced() || renderer->isBR()) && offset == box->caretLeftmostOffset())
                return box->isLeftToRightDirection() ? nextVisuallyDistinctCandidate(m_deepPosition) : previousVisuallyDistinctCandidate(m_deepPosition);

            offset = box->isLeftToRightDirection() ? renderer->nextOffset(offset) : renderer->previousOffset(offset);

            int caretMinOffset = box->caretMinOffset();
            int caretMaxOffset = box->caretMaxOffset();

            if (offset > caretMinOffset && offset < caretMaxOffset)
                break;

            if (box->isLeftToRightDirection() ? offset > caretMaxOffset : offset < caretMinOffset) {
                // Overshot to the right.
                InlineBox* nextBox = box->nextLeafChild();
                if (!nextBox)
                    return primaryDirection == LTR ? nextVisuallyDistinctCandidate(m_deepPosition) : previousVisuallyDistinctCandidate(m_deepPosition);

                // Reposition at the other logical position corresponding to our edge's visual position and go for another round.
                box = nextBox;
                renderer = box->renderer();
                offset = nextBox->caretLeftmostOffset();
                continue;
            }

            ASSERT(offset == box->caretRightmostOffset());

            unsigned char level = box->bidiLevel();
            InlineBox* nextBox = box->nextLeafChild();

            if (box->direction() == primaryDirection) {
                if (!nextBox || nextBox->bidiLevel() >= level)
                    break;

                level = nextBox->bidiLevel();

                InlineBox* prevBox = box;
                do {
                    prevBox = prevBox->prevLeafChild();
                } while (prevBox && prevBox->bidiLevel() > level);

                if (prevBox && prevBox->bidiLevel() == level)   // For example, abc FED 123 ^ CBA
                    break;

                // For example, abc 123 ^ CBA
                while (InlineBox* nextBox = box->nextLeafChild()) {
                    if (nextBox->bidiLevel() < level)
                        break;
                    box = nextBox;
                }
                renderer = box->renderer();
                offset = box->caretLeftmostOffset();
                if (box->direction() == primaryDirection)
                    break;
                continue;
            }

            if (nextBox) {
                box = nextBox;
                renderer = box->renderer();
                offset = box->caretLeftmostOffset();
                if (box->bidiLevel() > level) {
                    do {
                        nextBox = box->nextLeafChild();
                    } while (nextBox && nextBox->bidiLevel() > level);

                    if (!nextBox || nextBox->bidiLevel() < level)
                        continue;
                }
            } else {
                // Trailing edge of a secondary run. Set to the leading edge of the entire run.
                while (true) {
                    while (InlineBox* prevBox = box->prevLeafChild()) {
                        if (prevBox->bidiLevel() < level)
                            break;
                        box = prevBox;
                    }
                    if (box->bidiLevel() == level)
                        break;
                    level = box->bidiLevel();
                    while (InlineBox* nextBox = box->nextLeafChild()) {
                        if (nextBox->bidiLevel() < level)
                            break;
                        box = nextBox;
                    }
                    if (box->bidiLevel() == level)
                        break;
                    level = box->bidiLevel();
                }
                renderer = box->renderer();
                offset = primaryDirection == LTR ? box->caretMaxOffset() : box->caretMinOffset();
            }
            break;
        }

        p = Position(renderer->node(), offset);

        if ((p.isCandidate() && p.downstream() != downstreamStart) || p.atStartOfTree() || p.atEndOfTree())
            return p;
    }
}

VisiblePosition VisiblePosition::right(bool stayInEditableContent) const
{
    Position pos = rightVisuallyDistinctCandidate();
    // FIXME: Why can't we move left from the last position in a tree?
    if (pos.atStartOfTree() || pos.atEndOfTree())
        return VisiblePosition();

    VisiblePosition right = VisiblePosition(pos, DOWNSTREAM);
    ASSERT(right != *this);

    if (!stayInEditableContent)
        return right;

    // FIXME: This may need to do something different from "after".
    return honorEditableBoundaryAtOrAfter(right);
}

VisiblePosition VisiblePosition::honorEditableBoundaryAtOrBefore(const VisiblePosition &pos) const
{
    if (pos.isNull())
        return pos;
    
    Node* highestRoot = highestEditableRoot(deepEquivalent());
    
    // Return empty position if pos is not somewhere inside the editable region containing this position
    if (highestRoot && !pos.deepEquivalent().node()->isDescendantOf(highestRoot))
        return VisiblePosition();
        
    // Return pos itself if the two are from the very same editable region, or both are non-editable
    // FIXME: In the non-editable case, just because the new position is non-editable doesn't mean movement
    // to it is allowed.  VisibleSelection::adjustForEditableContent has this problem too.
    if (highestEditableRoot(pos.deepEquivalent()) == highestRoot)
        return pos;
  
    // Return empty position if this position is non-editable, but pos is editable
    // FIXME: Move to the previous non-editable region.
    if (!highestRoot)
        return VisiblePosition();

    // Return the last position before pos that is in the same editable region as this position
    return lastEditablePositionBeforePositionInRoot(pos.deepEquivalent(), highestRoot);
}

VisiblePosition VisiblePosition::honorEditableBoundaryAtOrAfter(const VisiblePosition &pos) const
{
    if (pos.isNull())
        return pos;
    
    Node* highestRoot = highestEditableRoot(deepEquivalent());
    
    // Return empty position if pos is not somewhere inside the editable region containing this position
    if (highestRoot && !pos.deepEquivalent().node()->isDescendantOf(highestRoot))
        return VisiblePosition();
    
    // Return pos itself if the two are from the very same editable region, or both are non-editable
    // FIXME: In the non-editable case, just because the new position is non-editable doesn't mean movement
    // to it is allowed.  VisibleSelection::adjustForEditableContent has this problem too.
    if (highestEditableRoot(pos.deepEquivalent()) == highestRoot)
        return pos;

    // Return empty position if this position is non-editable, but pos is editable
    // FIXME: Move to the next non-editable region.
    if (!highestRoot)
        return VisiblePosition();

    // Return the next position after pos that is in the same editable region as this position
    return firstEditablePositionAfterPositionInRoot(pos.deepEquivalent(), highestRoot);
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
    Node* node = position.node();
    if (!node)
        return Position();

    ASSERT(node->document());
    node->document()->updateLayoutIgnorePendingStylesheets();

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
    Node* nextNode = next.node();
    Node* prevNode = prev.node();

    // The new position must be in the same editable element. Enforce that first.
    // Unless the descent is from a non-editable html element to an editable body.
    if (node->hasTagName(htmlTag) && !node->isContentEditable() && node->document()->body() && node->document()->body()->isContentEditable())
        return next.isNotNull() ? next : prev;

    Node* editingRoot = editableRootForPosition(position);
        
    // If the html element is editable, descending into its body will look like a descent 
    // from non-editable to editable content since rootEditableElement() always stops at the body.
    if ((editingRoot && editingRoot->hasTagName(htmlTag)) || position.node()->isDocumentNode())
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
    Node *originalBlock = node->enclosingBlockFlowElement();
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
    Node* node = pos.node();
    if (!node || !node->isTextNode())
        return 0;
    Text* textNode = static_cast<Text*>(pos.node());
    unsigned offset = pos.deprecatedEditingOffset();
    unsigned length = textNode->length();
    if (offset >= length)
        return 0;

    UChar32 ch;
    const UChar* characters = textNode->data().characters();
    U16_NEXT(characters, offset, length, ch);
    return ch;
}

IntRect VisiblePosition::localCaretRect(RenderObject*& renderer) const
{
    Node* node = m_deepPosition.node();
    if (!node) {
        renderer = 0;
        return IntRect();
    }
    
    renderer = node->renderer();
    if (!renderer)
        return IntRect();

    InlineBox* inlineBox;
    int caretOffset;
    getInlineBoxAndOffset(inlineBox, caretOffset);

    if (inlineBox)
        renderer = inlineBox->renderer();

    return renderer->localCaretRect(inlineBox, caretOffset);
}

IntRect VisiblePosition::absoluteCaretBounds() const
{
    RenderObject* renderer;
    IntRect localRect = localCaretRect(renderer);
    if (localRect.isEmpty() || !renderer)
        return IntRect();

    return renderer->localToAbsoluteQuad(FloatRect(localRect)).enclosingBoundingBox();
}

int VisiblePosition::xOffsetForVerticalNavigation() const
{
    RenderObject* renderer;
    IntRect localRect = localCaretRect(renderer);
    if (localRect.isEmpty() || !renderer)
        return 0;

    // This ignores transforms on purpose, for now. Vertical navigation is done
    // without consulting transforms, so that 'up' in transformed text is 'up'
    // relative to the text, not absolute 'up'.
    return renderer->localToAbsolute(localRect.location()).x();
}

void VisiblePosition::debugPosition(const char* msg) const
{
    if (isNull())
        fprintf(stderr, "Position [%s]: null\n", msg);
    else
        fprintf(stderr, "Position [%s]: %s [%p] at %d\n", msg, m_deepPosition.node()->nodeName().utf8().data(), m_deepPosition.node(), m_deepPosition.deprecatedEditingOffset());
}

#ifndef NDEBUG

void VisiblePosition::formatForDebugger(char* buffer, unsigned length) const
{
    m_deepPosition.formatForDebugger(buffer, length);
}

void VisiblePosition::showTreeForThis() const
{
    m_deepPosition.showTreeForThis();
}

#endif

PassRefPtr<Range> makeRange(const VisiblePosition &start, const VisiblePosition &end)
{
    if (start.isNull() || end.isNull())
        return 0;
    
    Position s = rangeCompliantEquivalent(start);
    Position e = rangeCompliantEquivalent(end);
    return Range::create(s.node()->document(), s.node(), s.deprecatedEditingOffset(), e.node(), e.deprecatedEditingOffset());
}

VisiblePosition startVisiblePosition(const Range *r, EAffinity affinity)
{
    int exception = 0;
    return VisiblePosition(r->startContainer(exception), r->startOffset(exception), affinity);
}

VisiblePosition endVisiblePosition(const Range *r, EAffinity affinity)
{
    int exception = 0;
    return VisiblePosition(r->endContainer(exception), r->endOffset(exception), affinity);
}

bool setStart(Range *r, const VisiblePosition &visiblePosition)
{
    if (!r)
        return false;
    Position p = rangeCompliantEquivalent(visiblePosition);
    int code = 0;
    r->setStart(p.node(), p.deprecatedEditingOffset(), code);
    return code == 0;
}

bool setEnd(Range *r, const VisiblePosition &visiblePosition)
{
    if (!r)
        return false;
    Position p = rangeCompliantEquivalent(visiblePosition);
    int code = 0;
    r->setEnd(p.node(), p.deprecatedEditingOffset(), code);
    return code == 0;
}

Node *enclosingBlockFlowElement(const VisiblePosition &visiblePosition)
{
    if (visiblePosition.isNull())
        return NULL;

    return visiblePosition.deepEquivalent().node()->enclosingBlockFlowElement();
}

bool isFirstVisiblePositionInNode(const VisiblePosition &visiblePosition, const Node *node)
{
    if (visiblePosition.isNull())
        return false;
    
    if (!visiblePosition.deepEquivalent().node()->isDescendantOf(node))
        return false;
        
    VisiblePosition previous = visiblePosition.previous();
    return previous.isNull() || !previous.deepEquivalent().node()->isDescendantOf(node);
}

bool isLastVisiblePositionInNode(const VisiblePosition &visiblePosition, const Node *node)
{
    if (visiblePosition.isNull())
        return false;
    
    if (!visiblePosition.deepEquivalent().node()->isDescendantOf(node))
        return false;
                
    VisiblePosition next = visiblePosition.next();
    return next.isNull() || !next.deepEquivalent().node()->isDescendantOf(node);
}

}  // namespace WebCore

#ifndef NDEBUG

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
