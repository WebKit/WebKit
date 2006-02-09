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

#include "config.h"
#include "rebalance_whitespace_command.h"

#include "htmlediting.h"
#include "visible_text.h"
#include "dom_textimpl.h"

#include <kxmlcore/Assertions.h>

using DOM::DOMString;
using DOM::DocumentImpl;
using DOM::Position;
using DOM::TextImpl;

namespace khtml {

RebalanceWhitespaceCommand::RebalanceWhitespaceCommand(DocumentImpl *document, const Position &pos)
    : EditCommand(document), m_position(pos), m_upstreamOffset(InvalidOffset)
{
}

static inline bool isWhitespace(const QChar &c)
{
    return c.unicode() == 0xa0 || isCollapsibleWhitespace(c);
}

void RebalanceWhitespaceCommand::doApply()
{
    if (m_position.isNull() || !m_position.node()->isTextNode())
        return;
        
    TextImpl *textNode = static_cast<TextImpl *>(m_position.node());
    DOMString text = textNode->data();
    if (text.length() == 0)
        return;
    
    int offset = m_position.offset();
    // If neither text[offset] nor text[offset - 1] are some form of whitespace, do nothing.
    if (!isWhitespace(text[offset])) {
        offset--;
        if (offset < 0 || !isWhitespace(text[offset]))
            return;
    }
    
    // Set upstream and downstream to define the extent of the whitespace surrounding text[offset].
    int upstream = offset;
    while (upstream > 0 && isWhitespace(text[upstream - 1]))
        upstream--;
    m_upstreamOffset = upstream; // Save m_upstreamOffset, it will be used during an Undo
    
    int downstream = offset;
    while ((unsigned)downstream + 1 < text.length() && isWhitespace(text[downstream + 1]))
        downstream++;
    
    int length = downstream - upstream + 1;
    ASSERT(length > 0);
    
    m_beforeString = text.substring(upstream, length);
    rebalanceWhitespaceInTextNode(textNode, upstream, length);
    m_afterString = text.substring(upstream, length);
}

void RebalanceWhitespaceCommand::doUnapply()
{
    if (m_upstreamOffset == InvalidOffset)
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

