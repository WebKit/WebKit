/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "Selection.h"

#include "Document.h"
#include "Element.h"
#include "htmlediting.h"
#include "VisiblePosition.h"
#include "visible_units.h"
#include "Range.h"
#include <wtf/Assertions.h>

namespace WebCore {

Selection::Selection()
    : m_affinity(DOWNSTREAM)
    , m_granularity(CharacterGranularity)
    , m_state(NONE)
    , m_baseIsFirst(true)
{
}

Selection::Selection(const Position& pos, EAffinity affinity)
    : m_base(pos)
    , m_extent(pos)
    , m_affinity(affinity)
    , m_granularity(CharacterGranularity)
{
    validate();
}

Selection::Selection(const Position& base, const Position& extent, EAffinity affinity)
    : m_base(base)
    , m_extent(extent)
    , m_affinity(affinity)
    , m_granularity(CharacterGranularity)
{
    validate();
}

Selection::Selection(const VisiblePosition& pos)
    : m_base(pos.deepEquivalent())
    , m_extent(pos.deepEquivalent())
    , m_affinity(pos.affinity())
    , m_granularity(CharacterGranularity)
{
    validate();
}

Selection::Selection(const VisiblePosition& base, const VisiblePosition& extent)
    : m_base(base.deepEquivalent())
    , m_extent(extent.deepEquivalent())
    , m_affinity(base.affinity())
    , m_granularity(CharacterGranularity)
{
    validate();
}

Selection::Selection(const Range* range, EAffinity affinity)
    : m_base(range->startPosition())
    , m_extent(range->endPosition())
    , m_affinity(affinity)
    , m_granularity(CharacterGranularity)
{
    validate();
}

Selection Selection::selectionFromContentsOfNode(Node* node)
{
    return Selection(Position(node, 0), Position(node, maxDeepOffset(node)), DOWNSTREAM);
}

void Selection::setBase(const Position& position)
{
    m_base = position;
    validate();
}

void Selection::setBase(const VisiblePosition& visiblePosition)
{
    m_base = visiblePosition.deepEquivalent();
    validate();
}

void Selection::setExtent(const Position& position)
{
    m_extent = position;
    validate();
}

void Selection::setExtent(const VisiblePosition& visiblePosition)
{
    m_extent = visiblePosition.deepEquivalent();
    validate();
}

PassRefPtr<Range> Selection::toRange() const
{
    if (isNone())
        return 0;

    // Make sure we have an updated layout since this function is called
    // in the course of running edit commands which modify the DOM.
    // Failing to call this can result in equivalentXXXPosition calls returning
    // incorrect results.
    m_start.node()->document()->updateLayout();

    Position s, e;
    if (isCaret()) {
        // If the selection is a caret, move the range start upstream. This helps us match
        // the conventions of text editors tested, which make style determinations based
        // on the character before the caret, if any. 
        s = rangeCompliantEquivalent(m_start.upstream());
        e = s;
    } else {
        // If the selection is a range, select the minimum range that encompasses the selection.
        // Again, this is to match the conventions of text editors tested, which make style 
        // determinations based on the first character of the selection. 
        // For instance, this operation helps to make sure that the "X" selected below is the 
        // only thing selected. The range should not be allowed to "leak" out to the end of the 
        // previous text node, or to the beginning of the next text node, each of which has a 
        // different style.
        // 
        // On a treasure map, <b>X</b> marks the spot.
        //                       ^ selected
        //
        ASSERT(isRange());
        s = m_start.downstream();
        e = m_end.upstream();
        if (Range::compareBoundaryPoints(s.node(), s.offset(), e.node(), e.offset()) > 0) {
            // Make sure the start is before the end.
            // The end can wind up before the start if collapsed whitespace is the only thing selected.
            Position tmp = s;
            s = e;
            e = tmp;
        }
        s = rangeCompliantEquivalent(s);
        e = rangeCompliantEquivalent(e);
    }

    ExceptionCode ec = 0;
    RefPtr<Range> result(new Range(s.node()->document()));
    result->setStart(s.node(), s.offset(), ec);
    if (ec) {
        LOG_ERROR("Exception setting Range start from Selection: %d", ec);
        return 0;
    }
    result->setEnd(e.node(), e.offset(), ec);
    if (ec) {
        LOG_ERROR("Exception setting Range end from Selection: %d", ec);
        return 0;
    }
    return result.release();
}

bool Selection::expandUsingGranularity(TextGranularity granularity)
{
    if (isNone())
        return false;

    m_granularity = granularity;
    validate();
    return true;
}

void Selection::validate()
{
    // Move the selection to rendered positions, if possible.
    bool baseAndExtentEqual = m_base == m_extent;
    if (m_base.isNotNull()) {
        m_base = VisiblePosition(m_base, m_affinity).deepEquivalent();
        if (baseAndExtentEqual)
            m_extent = m_base;
    }
    if (m_extent.isNotNull() && !baseAndExtentEqual)
        m_extent = VisiblePosition(m_extent, m_affinity).deepEquivalent();

    // Make sure we do not have a dangling base or extent.
    if (m_base.isNull() && m_extent.isNull())
        m_baseIsFirst = true;
    else if (m_base.isNull()) {
        m_base = m_extent;
        m_baseIsFirst = true;
    } else if (m_extent.isNull()) {
        m_extent = m_base;
        m_baseIsFirst = true;
    } else {
        m_baseIsFirst = comparePositions(m_base, m_extent) <= 0;
    }

    if (m_baseIsFirst) {
        m_start = m_base;
        m_end = m_extent;
    } else {
        m_start = m_extent;
        m_end = m_base;
    }

    // Expand the selection if requested.
    switch (m_granularity) {
        case CharacterGranularity:
            // Don't do any expansion.
            break;
        case WordGranularity: {
            // General case: Select the word the caret is positioned inside of, or at the start of (RightWordIfOnBoundary).
            // Edge case: If the caret is after the last word in a soft-wrapped line or the last word in
            // the document, select that last word (LeftWordIfOnBoundary).
            // Edge case: If the caret is after the last word in a paragraph, select from the the end of the
            // last word to the line break (also RightWordIfOnBoundary);
            VisiblePosition start = VisiblePosition(m_start, m_affinity);
            VisiblePosition end   = VisiblePosition(m_end, m_affinity);
            EWordSide side = RightWordIfOnBoundary;
            if (isEndOfDocument(start) || (isEndOfLine(start) && !isStartOfLine(start) && !isEndOfParagraph(start)))
                side = LeftWordIfOnBoundary;
            m_start = startOfWord(start, side).deepEquivalent();
            side = RightWordIfOnBoundary;
            if (isEndOfDocument(end) || (isEndOfLine(end) && !isStartOfLine(end) && !isEndOfParagraph(end)))
                side = LeftWordIfOnBoundary;
            m_end = endOfWord(end, side).deepEquivalent();
            break;
        }
        case SentenceGranularity: {
            m_start = startOfSentence(VisiblePosition(m_start, m_affinity)).deepEquivalent();
            m_end = endOfSentence(VisiblePosition(m_end, m_affinity)).deepEquivalent();
            break;
        }
        case LineGranularity: {
            m_start = startOfLine(VisiblePosition(m_start, m_affinity)).deepEquivalent();
            VisiblePosition end = endOfLine(VisiblePosition(m_end, m_affinity));
            // If the end of this line is at the end of a paragraph, include the space 
            // after the end of the line in the selection.
            if (isEndOfParagraph(end)) {
                VisiblePosition next = end.next();
                if (next.isNotNull())
                    end = next;
            }
            m_end = end.deepEquivalent();
            break;
        }
        case LineBoundary:
            m_start = startOfLine(VisiblePosition(m_start, m_affinity)).deepEquivalent();
            m_end = endOfLine(VisiblePosition(m_end, m_affinity)).deepEquivalent();
            break;
        case ParagraphGranularity: {
            VisiblePosition pos(m_start, m_affinity);
            if (isStartOfLine(pos) && isEndOfDocument(pos))
                pos = pos.previous();
            m_start = startOfParagraph(pos).deepEquivalent();
            VisiblePosition visibleParagraphEnd = endOfParagraph(VisiblePosition(m_end, m_affinity));
            // Include the space after the end of the paragraph in the selection.
            VisiblePosition startOfNextParagraph = visibleParagraphEnd.next();
            if (startOfNextParagraph.isNull())
                m_end = visibleParagraphEnd.deepEquivalent();
            else {
                m_end = startOfNextParagraph.deepEquivalent();
                // Stay within enclosing node, e.g. do not span end of table.
                if (visibleParagraphEnd.deepEquivalent().node()->isDescendantOf(m_end.node()))
                    m_end = visibleParagraphEnd.deepEquivalent();
            }
            break;
        }
        case DocumentBoundary:
            m_start = startOfDocument(VisiblePosition(m_start, m_affinity)).deepEquivalent();
            m_end = endOfDocument(VisiblePosition(m_end, m_affinity)).deepEquivalent();
            break;
        case ParagraphBoundary:
            m_start = startOfParagraph(VisiblePosition(m_start, m_affinity)).deepEquivalent();
            m_end = endOfParagraph(VisiblePosition(m_end, m_affinity)).deepEquivalent();
            break;
        case SentenceBoundary:
            m_start = startOfSentence(VisiblePosition(m_start, m_affinity)).deepEquivalent();
            m_end = endOfSentence(VisiblePosition(m_end, m_affinity)).deepEquivalent();
            break;
    }
    
    // Make sure we do not have a dangling start or end.
    if (m_start.isNull())
        m_start = m_end;
    if (m_end.isNull())
        m_end = m_start;
    
    adjustForEditableContent();

    // adjust the state
    if (m_start.isNull()) {
        ASSERT(m_end.isNull());
        m_state = NONE;

        // enforce downstream affinity if not caret, as affinity only
        // makes sense for caret
        m_affinity = DOWNSTREAM;
    } else if (m_start == m_end || m_start.upstream() == m_end.upstream()) {
        m_state = CARET;
    } else {
        m_state = RANGE;

        // enforce downstream affinity if not caret, as affinity only
        // makes sense for caret
        m_affinity = DOWNSTREAM;

        // "Constrain" the selection to be the smallest equivalent range of nodes.
        // This is a somewhat arbitrary choice, but experience shows that it is
        // useful to make to make the selection "canonical" (if only for
        // purposes of comparing selections). This is an ideal point of the code
        // to do this operation, since all selection changes that result in a RANGE 
        // come through here before anyone uses it.
        m_start = m_start.downstream();
        m_end = m_end.upstream();
    }
}

void Selection::adjustForEditableContent()
{
    if (m_base.isNull() || m_start.isNull() || m_end.isNull())
        return;

    Node* baseRoot = highestEditableRoot(m_base);
    Node* startRoot = highestEditableRoot(m_start);
    Node* endRoot = highestEditableRoot(m_end);
    
    Node* baseEditableAncestor = lowestEditableAncestor(m_base.node());
    
    // The base, start and end are all in the same region.  No adjustment necessary.
    if (baseRoot == startRoot && baseRoot == endRoot)
        return;
    
    // The selection is based in editable content.
    if (baseRoot) {
        // If the start is outside the base's editable root, cap it at the start of that root.
        // If the start is in non-editable content that is inside the base's editable root, put it
        // at the first editable position after start inside the base's editable root.
        if (startRoot != baseRoot) {
            VisiblePosition first = firstEditablePositionAfterPositionInRoot(m_start, baseRoot);
            m_start = first.deepEquivalent();
            if (m_start.isNull()) {
                ASSERT_NOT_REACHED();
                m_start = m_end;
            }
        }
        // If the end is outside the base's editable root, cap it at the end of that root.
        // If the end is in non-editable content that is inside the base's root, put it
        // at the last editable position before the end inside the base's root.
        if (endRoot != baseRoot) {
            VisiblePosition last = lastEditablePositionBeforePositionInRoot(m_end, baseRoot);
            m_end = last.deepEquivalent();
            if (m_end.isNull()) {
                ASSERT_NOT_REACHED();
                m_end = m_start;
            }
        }
    // The selection is based in non-editable content.
    } else {
        // FIXME: Non-editable pieces inside editable content should be atomic, in the same way that editable
        // pieces in non-editable content are atomic.
    
        // The selection ends in editable content or non-editable content inside a different editable ancestor, 
        // move backward until non-editable content inside the same lowest editable ancestor is reached.
        Node* endEditableAncestor = lowestEditableAncestor(m_end.node());
        if (endRoot || endEditableAncestor != baseEditableAncestor) {
            
            Position p = previousVisuallyDistinctCandidate(m_end);
            Node* shadowAncestor = endRoot ? endRoot->shadowAncestorNode() : 0;
            if (p.isNull() && endRoot && (shadowAncestor != endRoot))
                p = Position(shadowAncestor, maxDeepOffset(shadowAncestor));
            while (p.isNotNull() && !(lowestEditableAncestor(p.node()) == baseEditableAncestor && !isEditablePosition(p))) {
                Node* root = editableRootForPosition(p);
                shadowAncestor = root ? root->shadowAncestorNode() : 0;
                p = isAtomicNode(p.node()) ? positionBeforeNode(p.node()) : previousVisuallyDistinctCandidate(p);
                if (p.isNull() && (shadowAncestor != root))
                    p = Position(shadowAncestor, maxDeepOffset(shadowAncestor));
            }
            VisiblePosition previous(p);
            
            if (previous.isNull()) {
                ASSERT_NOT_REACHED();
                m_base = Position();
                m_extent = Position();
                validate();
                return;
            }
            m_end = previous.deepEquivalent();
        }

        // The selection starts in editable content or non-editable content inside a different editable ancestor, 
        // move forward until non-editable content inside the same lowest editable ancestor is reached.
        Node* startEditableAncestor = lowestEditableAncestor(m_start.node());      
        if (startRoot || startEditableAncestor != baseEditableAncestor) {
            Position p = nextVisuallyDistinctCandidate(m_start);
            Node* shadowAncestor = startRoot ? startRoot->shadowAncestorNode() : 0;
            if (p.isNull() && startRoot && (shadowAncestor != startRoot))
                p = Position(shadowAncestor, 0);
            while (p.isNotNull() && !(lowestEditableAncestor(p.node()) == baseEditableAncestor && !isEditablePosition(p))) {
                Node* root = editableRootForPosition(p);
                shadowAncestor = root ? root->shadowAncestorNode() : 0;
                p = isAtomicNode(p.node()) ? positionAfterNode(p.node()) : nextVisuallyDistinctCandidate(p);
                if (p.isNull() && (shadowAncestor != root))
                    p = Position(shadowAncestor, 0);
            }
            VisiblePosition next(p);
            
            if (next.isNull()) {
                ASSERT_NOT_REACHED();
                m_base = Position();
                m_extent = Position();
                validate();
                return;
            }
            m_start = next.deepEquivalent();
        }
    }
    
    // Correct the extent if necessary.
    if (baseEditableAncestor != lowestEditableAncestor(m_extent.node()))
        m_extent = m_baseIsFirst ? m_end : m_start;
}

bool Selection::isContentEditable() const
{
    return isEditablePosition(start());
}

bool Selection::isContentRichlyEditable() const
{
    return isRichlyEditablePosition(start());
}

Element* Selection::rootEditableElement() const
{
    return editableRootForPosition(start());
}

void Selection::debugPosition() const
{
    if (!m_start.node())
        return;

    fprintf(stderr, "Selection =================\n");

    if (m_start == m_end) {
        Position pos = m_start;
        fprintf(stderr, "pos:        %s %p:%d\n", pos.node()->nodeName().deprecatedString().latin1(), pos.node(), pos.offset());
    } else {
        Position pos = m_start;
        fprintf(stderr, "start:      %s %p:%d\n", pos.node()->nodeName().deprecatedString().latin1(), pos.node(), pos.offset());
        fprintf(stderr, "-----------------------------------\n");
        pos = m_end;
        fprintf(stderr, "end:        %s %p:%d\n", pos.node()->nodeName().deprecatedString().latin1(), pos.node(), pos.offset());
        fprintf(stderr, "-----------------------------------\n");
    }

    fprintf(stderr, "================================\n");
}

#ifndef NDEBUG

void Selection::formatForDebugger(char* buffer, unsigned length) const
{
    String result;
    String s;
    
    if (isNone()) {
        result = "<none>";
    } else {
        const int FormatBufferSize = 1024;
        char s[FormatBufferSize];
        result += "from ";
        start().formatForDebugger(s, FormatBufferSize);
        result += s;
        result += " to ";
        end().formatForDebugger(s, FormatBufferSize);
        result += s;
    }

    strncpy(buffer, result.deprecatedString().latin1(), length - 1);
}

void Selection::showTreeForThis() const
{
    if (start().node()) {
        start().node()->showTreeAndMark(start().node(), "S", end().node(), "E");
        fprintf(stderr, "start offset: %d, end offset: %d\n", start().offset(), end().offset());
    }
}

#endif

} // namespace WebCore

#ifndef NDEBUG

void showTree(const WebCore::Selection& sel)
{
    sel.showTreeForThis();
}

void showTree(const WebCore::Selection* sel)
{
    if (sel)
        sel->showTreeForThis();
}

#endif
