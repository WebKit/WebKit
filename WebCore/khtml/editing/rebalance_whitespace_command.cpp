/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#include "rebalance_whitespace_command.h"

#include "htmlediting.h"
#include "visible_text.h"
#include "xml/dom_docimpl.h"
#include "xml/dom_textimpl.h"

#if APPLE_CHANGES
#include <kxmlcore/Assertions.h>
#else
#define ASSERT(assertion) assert(assertion)
#endif

using DOM::DOMString;
using DOM::DocumentImpl;
using DOM::Position;
using DOM::TextImpl;

namespace khtml {

RebalanceWhitespaceCommand::RebalanceWhitespaceCommand(DocumentImpl *document, const Position &pos)
    : EditCommand(document), m_position(pos), m_upstreamOffset(InvalidOffset), m_downstreamOffset(InvalidOffset)
{
}

RebalanceWhitespaceCommand::~RebalanceWhitespaceCommand()
{
}

static inline bool isNBSP(const QChar &c)
{
    return c.unicode() == 0xa0;
}

void RebalanceWhitespaceCommand::doApply()
{
    static DOMString space(" ");

    if (m_position.isNull() || !m_position.node()->isTextNode())
        return;
        
    TextImpl *textNode = static_cast<TextImpl *>(m_position.node());
    DOMString text = textNode->data();
    if (text.length() == 0)
        return;
    
    // find upstream offset
    int upstream = m_position.offset();
    while (upstream > 0 && isCollapsibleWhitespace(text[upstream - 1]) || isNBSP(text[upstream - 1])) {
        upstream--;
        m_upstreamOffset = upstream;
    }

    // find downstream offset
    int downstream = m_position.offset();
    while ((unsigned)downstream < text.length() && isCollapsibleWhitespace(text[downstream]) || isNBSP(text[downstream])) {
        downstream++;
        m_downstreamOffset = downstream;
    }

    if (m_upstreamOffset == InvalidOffset && m_downstreamOffset == InvalidOffset)
        return;
        
    m_upstreamOffset = upstream;
    m_downstreamOffset = downstream;
    int length = m_downstreamOffset - m_upstreamOffset;
    
    m_beforeString = text.substring(m_upstreamOffset, length);
    
    // The following loop figures out a "rebalanced" whitespace string for any length
    // string, and takes into account the special cases that need to handled for the
    // start and end of strings (i.e. first and last character must be an nbsp.
    int i = m_upstreamOffset;
    while (i < m_downstreamOffset) {
        int add = (m_downstreamOffset - i) % 3;
        switch (add) {
            case 0:
                m_afterString += nonBreakingSpaceString();
                m_afterString += space;
                m_afterString += nonBreakingSpaceString();
                add = 3;
                break;
            case 1:
                if (i == 0 || (unsigned)i + 1 == text.length()) // at start or end of string
                    m_afterString += nonBreakingSpaceString();
                else
                    m_afterString += space;
                break;
            case 2:
                if ((unsigned)i + 2 == text.length()) {
                     // at end of string
                    m_afterString += nonBreakingSpaceString();
                    m_afterString += nonBreakingSpaceString();
                }
                else {
                    m_afterString += nonBreakingSpaceString();
                    m_afterString += space;
                }
                break;
        }
        i += add;
    }
    
    text.remove(m_upstreamOffset, length);
    text.insert(m_afterString, m_upstreamOffset);
}

void RebalanceWhitespaceCommand::doUnapply()
{
    if (m_upstreamOffset == InvalidOffset && m_downstreamOffset == InvalidOffset)
        return;
    
    ASSERT(m_position.node()->isTextNode());
    TextImpl *textNode = static_cast<TextImpl *>(m_position.node());
    DOMString text = textNode->data();
    text.remove(m_upstreamOffset, m_afterString.length());
    text.insert(m_beforeString, m_upstreamOffset);
}

bool RebalanceWhitespaceCommand::preservesTypingStyle() const
{
    return true;
}

} // namespace khtml

