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

#ifndef _DOM_Position_h_
#define _DOM_Position_h_

namespace DOM {

class NodeImpl;

class DOMPosition
{
public:
    DOMPosition() : m_node(0), m_offset(0) {};
    DOMPosition(NodeImpl *node, long offset);
    DOMPosition(const DOMPosition &);
    ~DOMPosition();

    NodeImpl *node() const { return m_node; }
    long offset() const { return m_offset; }

    long renderedOffset() const;

    bool isEmpty() const { return m_node == 0; }
    bool notEmpty() const { return m_node != 0; }
    
    DOMPosition equivalentLeafPosition() const;
    DOMPosition previousRenderedEditablePosition() const;
    DOMPosition nextRenderedEditablePosition() const;
    DOMPosition previousCharacterPosition() const;
    DOMPosition nextCharacterPosition() const;
    DOMPosition previousWordPosition() const;
    DOMPosition nextWordPosition() const;
    DOMPosition previousLinePosition(int x) const;
    DOMPosition nextLinePosition(int x) const;
    DOMPosition equivalentUpstreamPosition() const;
    DOMPosition equivalentDownstreamPosition() const;
    bool atStartOfContainingEditableBlock() const;
    bool atStartOfRootEditableBlock() const;
    bool inRenderedContent() const;
    bool inRenderedText() const;
    bool rendersOnSameLine(const DOMPosition &pos) const;
    bool rendersInDifferentPosition(const DOMPosition &pos) const;
    bool isFirstRenderedPositionOnLine() const;
    bool isLastRenderedPositionOnLine() const;
    bool isLastRenderedPositionInEditableBlock() const;
    bool inFirstEditableInRootEditableBlock() const;
    bool inLastEditableInRootEditableBlock() const;
    bool inFirstEditableInContainingEditableBlock() const;
    bool inLastEditableInContainingEditableBlock() const;
    
    DOMPosition &operator=(const DOMPosition &o);
    
    friend bool operator==(const DOMPosition &a, const DOMPosition &b);
    friend bool operator!=(const DOMPosition &a, const DOMPosition &b);
    
private:
    NodeImpl *m_node;
    long m_offset;
};

inline bool operator==(const DOMPosition &a, const DOMPosition &b)
{
    return a.node() == b.node() && a.offset() == b.offset();
}

inline bool operator!=(const DOMPosition &a, const DOMPosition &b)
{
    return !(a == b);
}

}; // namespace DOM

#endif // _DOM_Position_h_