/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#ifndef __htmlediting_h__
#define __htmlediting_h__

#include <khtml_selection.h>
#include <dom_docimpl.h>
#include <dom_string.h>
#include <dom_node.h>
#include <dom_nodeimpl.h>
#include <dom2_range.h>

class KHTMLSelection;

namespace khtml {

enum EditCommandID { InputTextCommandID, DeleteTextCommandID, };

class EditCommand
{
public:    
    EditCommand(DOM::DocumentImpl *document);
    virtual ~EditCommand();

    virtual EditCommandID commandID() const = 0;

    DOM::DocumentImpl *document() const { return m_document; }

    virtual bool apply() = 0;
    virtual bool canUndo() const = 0;
    
protected:
    void deleteSelection();
    void pruneEmptyNodes() const;
    void removeNode(DOM::NodeImpl *) const;
    
private:
    DOM::DocumentImpl *m_document;
};


class InputTextCommand : public EditCommand
{
public:
    InputTextCommand(DOM::DocumentImpl *document, const DOM::DOMString &text);
    virtual ~InputTextCommand() {};
    
    virtual EditCommandID commandID() const;
    
    virtual bool apply();
    virtual bool canUndo() const;

    DOM::DOMString text() const { return m_text; }
    bool isLineBreak() const;
    bool isSpace() const;
    
private:
    DOM::DOMString m_text;
};

class DeleteTextCommand : public EditCommand
{
public:
    DeleteTextCommand(DOM::DocumentImpl *document);
    virtual ~DeleteTextCommand() {};
    
    virtual EditCommandID commandID() const;
    
    virtual bool apply();
    virtual bool canUndo() const;
};

}; // end namespace khtml

#endif
