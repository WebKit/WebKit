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

#ifndef __delete_from_text_node_command_h__
#define __delete_from_text_node_command_h__

#include "EditCommand.h"

#include "PlatformString.h"

namespace DOM {
    class TextImpl;
}

namespace khtml {

class DeleteFromTextNodeCommand : public EditCommand
{
public:
    DeleteFromTextNodeCommand(DOM::DocumentImpl *document, DOM::TextImpl *node, int offset, int count);
    virtual ~DeleteFromTextNodeCommand() { }

    virtual void doApply();
    virtual void doUnapply();

    DOM::TextImpl *node() const { return m_node.get(); }
    int offset() const { return m_offset; }
    int count() const { return m_count; }

private:
    RefPtr<DOM::TextImpl> m_node;
    int m_offset;
    int m_count;
    DOM::DOMString m_text;
};

} // namespace khtml

#endif // __delete_from_text_node_command_h__
