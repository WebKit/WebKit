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

namespace DOM {

class CSSComputedStyleDeclarationImpl;
class CSSStyleDeclarationImpl;
class ElementImpl;
class NodeImpl;

// These match the AppKit values for these concepts.
// From NSTextView.h:
// NSSelectionAffinityUpstream = 0
// NSSelectionAffinityDownstream = 1
enum EAffinity { UPSTREAM = 0, DOWNSTREAM = 1 };

static const bool StayInBlock = true;

class Position
{
public:
    Position() : m_node(0), m_offset(0) {};
    Position(NodeImpl *node, long offset);
    Position(const Position &);
    ~Position();

    NodeImpl *node() const { return m_node; }
    long offset() const { return m_offset; }

    ElementImpl *element() const;
    CSSComputedStyleDeclarationImpl *computedStyle() const;

    long renderedOffset() const;

    bool isEmpty() const { return m_node == 0; }
    bool notEmpty() const { return m_node != 0; }
    
    Position equivalentLeafPosition() const;
    Position previousRenderedEditablePosition() const;
    Position nextRenderedEditablePosition() const;
    Position previousCharacterPosition() const;
    Position nextCharacterPosition() const;
    
    // suitable for moving by word in the UI
    Position previousWordPosition() const;
    Position nextWordPosition() const;
    Position previousLinePosition(int x) const;
    Position nextLinePosition(int x) const;

    // next word boundary - would be too tedious to use in UI
    Position previousWordBoundary() const;
    Position nextWordBoundary() const;

    Position leadingWhitespacePosition() const;
    Position trailingWhitespacePosition() const;

    Position upstream(bool stayInBlock=false) const;
    Position downstream(bool stayInBlock=false) const;
    Position equivalentRangeCompliantPosition() const;
    Position equivalentShallowPosition() const;
    Position equivalentDeepPosition() const;
    Position closestRenderedPosition(EAffinity) const;
    bool atStartOfContainingEditableBlock() const;
    bool atStartOfRootEditableElement() const;
    bool inRenderedContent() const;
    bool inRenderedText() const;
    bool isRenderedCharacter() const;
    bool rendersOnSameLine(const Position &pos) const;
    bool rendersInDifferentPosition(const Position &pos) const;
    bool isFirstRenderedPositionOnLine() const;
    bool isLastRenderedPositionOnLine() const;
    bool isLastRenderedPositionInEditableBlock() const;
    bool inFirstEditableInRootEditableElement() const;
    bool inLastEditableInRootEditableElement() const;
    bool inFirstEditableInContainingEditableBlock() const;
    bool inLastEditableInContainingEditableBlock() const;
    
    Position &operator=(const Position &o);
    
    friend bool operator==(const Position &a, const Position &b);
    friend bool operator!=(const Position &a, const Position &b);
    
    void debugPosition(const char *msg="") const;

#ifndef NDEBUG
    void formatForDebugger(char *buffer, unsigned length) const;
#endif
    
private:
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

} // namespace DOM

#endif // __dom_position_h__