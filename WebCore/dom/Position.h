/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "Node.h"
#include "TextAffinity.h"

namespace WebCore {

class CSSComputedStyleDeclaration;
class Element;
class PositionIterator;
class Range;

enum EUsingComposedCharacters { NotUsingComposedCharacters = false, UsingComposedCharacters = true };

class Position
{
public:
    Position() : m_node(0), m_offset(0) { }
    Position(Node*, int offset);
    Position(const PositionIterator&);

    void clear();

    Node *node() const { return m_node.get(); }
    Element* documentElement() const;
    int offset() const { return m_offset; }

    bool isNull() const { return m_node == 0; }
    bool isNotNull() const { return m_node != 0; }

    Element* element() const;
    PassRefPtr<CSSComputedStyleDeclaration> computedStyle() const;

    // Move up or down the DOM by one position.
    // Offsets are computed using render text for nodes that have renderers - but note that even when
    // using composed characters, the result may be inside a single user-visible character if a ligature is formed.
    Position previous(EUsingComposedCharacters usingComposedCharacters=NotUsingComposedCharacters) const;
    Position next(EUsingComposedCharacters usingComposedCharacters=NotUsingComposedCharacters) const;
    static int uncheckedPreviousOffset(const Node*, int current);
    static int uncheckedNextOffset(const Node*, int current);

    bool atStart() const;
    bool atEnd() const;

    // FIXME: Make these non-member functions and put them somewhere in the editing directory.
    // These aren't really basic "position" operations. More high level editing helper functions.
    Position leadingWhitespacePosition(EAffinity, bool considerNonCollapsibleWhitespace = false) const;
    Position trailingWhitespacePosition(EAffinity, bool considerNonCollapsibleWhitespace = false) const;

    // p.upstream() through p.downstream() is the range of positions that map to the same VisiblePosition as p.
    Position upstream() const;
    Position downstream() const;
    
    bool isCandidate() const;
    bool inRenderedText() const;
    bool isRenderedCharacter() const;
    bool rendersInDifferentPosition(const Position &pos) const;
    
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
    RefPtr<Node> m_node;
    int m_offset;
};

inline bool operator==(const Position &a, const Position &b)
{
    return a.node() == b.node() && a.offset() == b.offset();
}

inline bool operator!=(const Position &a, const Position &b)
{
    return !(a == b);
}

Position startPosition(const Range*);
Position endPosition(const Range*);

} // namespace WebCore

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const WebCore::Position&);
void showTree(const WebCore::Position*);
#endif

#endif // Position_h
