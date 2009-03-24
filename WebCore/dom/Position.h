/*
 * Copyright (C) 2004, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef Position_h
#define Position_h

#include "TextAffinity.h"
#include "TextDirection.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSComputedStyleDeclaration;
class Element;
class InlineBox;
class Node;
class Range;
class RenderObject;

enum PositionMoveType {
    CodePoint,       // Move by a single code point.
    Character,       // Move to the next Unicode character break.
    BackwardDeletion // Subject to platform conventions.
};

// FIXME: Reduce the number of operations we have on a Position.
// This should be more like a humble struct, without so many different
// member functions. We should find better homes for these functions.

class Position {
public:
    Position() : m_offset(0) { }

    // This constructor should be private
    Position(PassRefPtr<Node> anchorNode, int offset)
        : m_anchorNode(anchorNode)
        , m_offset(offset)
    {}

    void clear() { m_anchorNode.clear(); m_offset = 0; }

    Node* anchorNode() const { return m_anchorNode.get(); }

    // FIXME: Callers should be moved off of node(), node() is not always the container for this position.
    // For nodes which editingIgnoresContent(node()) returns true, positions like [ignoredNode, 0]
    // will be treated as before ignoredNode (thus node() is really after the position, not containing it).
    Node* node() const { return m_anchorNode.get(); }
    Element* documentElement() const;

    void moveToPosition(PassRefPtr<Node> node, int offset)
    {
        m_anchorNode = node;
        m_offset = offset;
    }
    void moveToOffset(int offset)
    {
        m_offset = offset;
    }

    bool isNull() const { return !m_anchorNode; }
    bool isNotNull() const { return m_anchorNode; }

    Element* element() const;
    PassRefPtr<CSSComputedStyleDeclaration> computedStyle() const;

    // Move up or down the DOM by one position.
    // Offsets are computed using render text for nodes that have renderers - but note that even when
    // using composed characters, the result may be inside a single user-visible character if a ligature is formed.
    Position previous(PositionMoveType = CodePoint) const;
    Position next(PositionMoveType = CodePoint) const;
    static int uncheckedPreviousOffset(const Node*, int current);
    static int uncheckedPreviousOffsetForBackwardDeletion(const Node*, int current);
    static int uncheckedNextOffset(const Node*, int current);

    // These can be either inside or just before/after the node, depending on
    // if the node is ignored by editing or not.
    bool atFirstEditingPositionForNode() const;
    bool atLastEditingPositionForNode() const;

    bool atStartOfTree() const;
    bool atEndOfTree() const;

    // FIXME: Make these non-member functions and put them somewhere in the editing directory.
    // These aren't really basic "position" operations. More high level editing helper functions.
    Position leadingWhitespacePosition(EAffinity, bool considerNonCollapsibleWhitespace = false) const;
    Position trailingWhitespacePosition(EAffinity, bool considerNonCollapsibleWhitespace = false) const;
    
    // These return useful visually equivalent positions.
    Position upstream() const;
    Position downstream() const;
    
    bool isCandidate() const;
    bool inRenderedText() const;
    bool isRenderedCharacter() const;
    bool rendersInDifferentPosition(const Position&) const;

    void getInlineBoxAndOffset(EAffinity, InlineBox*&, int& caretOffset) const;
    void getInlineBoxAndOffset(EAffinity, TextDirection primaryDirection, InlineBox*&, int& caretOffset) const;

    static bool hasRenderedNonAnonymousDescendantsWithHeight(RenderObject*);
    static bool nodeIsUserSelectNone(Node*);
    
    void debugPosition(const char* msg = "") const;

#ifndef NDEBUG
    void formatForDebugger(char* buffer, unsigned length) const;
    void showTreeForThis() const;
#endif
    
private:
    int renderedOffset() const;

    Position previousCharacterPosition(EAffinity) const;
    Position nextCharacterPosition(EAffinity) const;

    RefPtr<Node> m_anchorNode;
public:
    // m_offset can be the offset inside m_anchorNode, or if editingIgnoresContent(m_anchorNode)
    // returns true, then other places in editing will treat m_offset == 0 as "before the anchor"
    // and m_offset > 0 as "after the anchor node".  See rangeCompliantEquivalent for more info.
    int m_offset; // FIXME: This should be made private.
};

inline bool operator==(const Position& a, const Position& b)
{
    // FIXME: In <div><img></div> [div, 0] != [img, 0] even though most of the
    // editing code will treat them as identical.
    return a.anchorNode() == b.anchorNode() && a.m_offset == b.m_offset;
}

inline bool operator!=(const Position& a, const Position& b)
{
    return !(a == b);
}

Position startPosition(const Range*);
Position endPosition(const Range*);

// NOTE: first/lastDeepEditingPositionForNode can return "editing positions" (like [img, 0])
// for elements which editing "ignores".  the rest of the editing code will treat [img, 0]
// as "the last position before the img"
Position firstDeepEditingPositionForNode(Node*);
Position lastDeepEditingPositionForNode(Node*);

} // namespace WebCore

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const WebCore::Position&);
void showTree(const WebCore::Position*);
#endif

#endif // Position_h
