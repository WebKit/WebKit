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

#ifndef __typing_command_h__
#define __typing_command_h__

#include "composite_edit_command.h"
#include "dom/dom_string.h"

namespace khtml {

class TypingCommand : public CompositeEditCommand
{
public:
    enum ETypingCommand { 
        DeleteKey, 
        ForwardDeleteKey, 
        InsertText, 
        InsertLineBreak, 
        InsertParagraphSeparator,
        InsertParagraphSeparatorInQuotedContent,
    };

    TypingCommand(DOM::DocumentImpl *document, ETypingCommand, const DOM::DOMString &text = "", bool selectInsertedText = false);

    static void deleteKeyPressed(DOM::DocumentImpl *, bool smartDelete = false);
    static void forwardDeleteKeyPressed(DOM::DocumentImpl *, bool smartDelete = false);
    static void insertText(DOM::DocumentImpl *, const DOM::DOMString &, bool selectInsertedText = false);
    static void insertLineBreak(DOM::DocumentImpl *);
    static void insertParagraphSeparator(DOM::DocumentImpl *);
    static void insertParagraphSeparatorInQuotedContent(DOM::DocumentImpl *);
    static bool isOpenForMoreTypingCommand(const EditCommandPtr &);
    static void closeTyping(const EditCommandPtr &);
    
    virtual void doApply();
    virtual EditAction editingAction() const;

    bool openForMoreTyping() const { return m_openForMoreTyping; }
    void closeTyping() { m_openForMoreTyping = false; }

    void insertText(const DOM::DOMString &text, bool selectInsertedText);
    void insertTextRunWithoutNewlines(const DOM::DOMString &text, bool selectInsertedText);
    void insertLineBreak();
    void insertParagraphSeparatorInQuotedContent();
    void insertParagraphSeparator();
    void deleteKeyPressed();
    void forwardDeleteKeyPressed();

    bool smartDelete() { return m_smartDelete; }
    void setSmartDelete(bool smartDelete) { m_smartDelete = smartDelete; }

private:
    virtual bool isTypingCommand() const;
    virtual bool preservesTypingStyle() const;

    void markMisspellingsAfterTyping();
    void typingAddedToOpenCommand();
    
    ETypingCommand m_commandType;
    DOM::DOMString m_textToInsert;
    bool m_openForMoreTyping;
    bool m_applyEditing;
    bool m_selectInsertedText;
    bool m_smartDelete;
};

} // namespace khtml

#endif // __typing_command_h__
