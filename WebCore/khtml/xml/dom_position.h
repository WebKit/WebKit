/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#ifndef __dom_position_h__
#define __dom_position_h__

#include "text_affinity.h"

namespace DOM {

class CSSComputedStyleDeclarationImpl;
class ElementImpl;
class NodeImpl;
class RangeImpl;

enum EUsingComposedCharacters { NotUsingComposedCharacters = false, UsingComposedCharacters = true };

class Position
{
public:
    Position() : m_node(0), m_offset(0) { }
    Position(NodeImpl *node, long offset);
    Position(const Position &);
    ~Position();

    Position &operator=(const Position &o);

    void clear();

    NodeImpl *node() const { return m_node; }
    long offset() const { return m_offset; }

    bool isNull() const { return m_node == 0; }
    bool isNotNull() const { return m_node != 0; }

    ElementImpl *element() const;
    CSSComputedStyleDeclarationImpl *computedStyle() const;

    // Move up or down the DOM by one position
    Position previous(EUsingComposedCharacters usingComposedCharacters=NotUsingComposedCharacters) const;
    Position next(EUsingComposedCharacters usingComposedCharacters=NotUsingComposedCharacters) const;
    bool atStart() const;
    bool atEnd() const;
    
    // FIXME: Make these non-member functions and put them somewhere in the editing directory.
    // These aren't really basic "position" operations. More high level editing helper functions.
    Position leadingWhitespacePosition(khtml::EAffinity affinity, bool considerNonCollapsibleWhitespace = false) const;
    Position trailingWhitespacePosition(khtml::EAffinity affinity, bool considerNonCollapsibleWhitespace = false) const;

    // These functions only consider leaf nodes and blocks.
    // Hence, the results from these functions are idiosyncratic, and until you
    // become familiar with the results, you may find using these functions confusing.
    // I have hopes to make the results of these functions less ambiguous in the near
    // future, and have them consider all nodes, and have the Positions that are 
    // returned follow a simple rule: The upstream position is the position
    // earliest in document order that will make the insertion point render in the
    // same position as the caller's position. The same goes for downstream position
    // except that it is the latest position for earliest position in the above 
    // description.
    Position upstream() const;
    Position downstream() const;
    
    Position equivalentRangeCompliantPosition() const;
    Position equivalentDeepPosition() const;
    bool inRenderedContent() const;
    bool isRenderedCharacter() const;
    bool rendersInDifferentPosition(const Position &pos) const;
    
    void debugPosition(const char *msg="") const;

#ifndef NDEBUG
    void formatForDebugger(char *buffer, unsigned length) const;
#endif
    
private:
    long renderedOffset() const;

    bool inRenderedText() const;

    Position previousCharacterPosition(khtml::EAffinity affinity) const;
    Position nextCharacterPosition(khtml::EAffinity affinity) const;
    
    NodeImpl *m_node;
    long m_offset;
};

inline bool operator==(const Position &a, const Position &b)
{
    return a.node() == b.node() && a.offset() == b.offset();
}

inline bool operator!=(const Position &a, const Position &b)
{
    return !(a == b);
}

Position startPosition(const RangeImpl *);
Position endPosition(const RangeImpl *);

} // namespace DOM

#endif // __dom_position_h__
