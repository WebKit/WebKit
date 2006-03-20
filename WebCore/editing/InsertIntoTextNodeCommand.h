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

#ifndef __insert_into_text_node_command_h__
#define __insert_into_text_node_command_h__

#include "EditCommand.h"

#include "PlatformString.h"

namespace WebCore {
    class Text;
    class String;
}

namespace WebCore {

class InsertIntoTextNodeCommand : public EditCommand
{
public:
    InsertIntoTextNodeCommand(WebCore::Document *document, WebCore::Text *, int, const WebCore::String &);
    virtual ~InsertIntoTextNodeCommand() { }

    virtual void doApply();
    virtual void doUnapply();

    WebCore::Text *node() const { return m_node.get(); }
    int offset() const { return m_offset; }
    WebCore::String text() const { return m_text; }

private:
    RefPtr<WebCore::Text> m_node;
    int m_offset;
    WebCore::String m_text;
};

} // namespace WebCore

#endif // __insert_into_text_node_command_h__
