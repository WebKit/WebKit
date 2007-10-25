/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc.  All rights reserved.
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

#ifndef TypingCommand_h
#define TypingCommand_h

#include "CompositeEditCommand.h"

namespace WebCore {

class TypingCommand : public CompositeEditCommand {
public:
    enum ETypingCommand { 
        DeleteSelection,
        DeleteKey, 
        ForwardDeleteKey, 
        InsertText, 
        InsertLineBreak, 
        InsertParagraphSeparator,
        InsertParagraphSeparatorInQuotedContent
    };

    TypingCommand(Document*, ETypingCommand, const String& text = "", bool selectInsertedText = false, TextGranularity = CharacterGranularity);

    static void deleteSelection(Document*, bool smartDelete = false);
    static void deleteKeyPressed(Document*, bool smartDelete = false, TextGranularity = CharacterGranularity);
    static void forwardDeleteKeyPressed(Document*, bool smartDelete = false, TextGranularity = CharacterGranularity);
    static void insertText(Document*, const String&, bool selectInsertedText = false, bool insertedTextIsComposition = false);
    static void insertText(Document*, const String&, const Selection&, bool selectInsertedText = false, bool insertedTextIsComposition = false);
    static void insertLineBreak(Document*);
    static void insertParagraphSeparator(Document*);
    static void insertParagraphSeparatorInQuotedContent(Document*);
    static bool isOpenForMoreTypingCommand(const EditCommand*);
    static void closeTyping(EditCommand*);
    
    virtual void doApply();
    virtual EditAction editingAction() const;

    bool isOpenForMoreTyping() const { return m_openForMoreTyping; }
    void closeTyping() { m_openForMoreTyping = false; }

    void insertText(const String &text, bool selectInsertedText);
    void insertTextRunWithoutNewlines(const String &text, bool selectInsertedText);
    void insertLineBreak();
    void insertParagraphSeparatorInQuotedContent();
    void insertParagraphSeparator();
    void deleteKeyPressed(TextGranularity);
    void forwardDeleteKeyPressed(TextGranularity);
    void deleteSelection(bool);

private:
    bool smartDelete() { return m_smartDelete; }
    void setSmartDelete(bool smartDelete) { m_smartDelete = smartDelete; }
    
    virtual bool isTypingCommand() const;
    virtual bool preservesTypingStyle() const;

    void markMisspellingsAfterTyping();
    void typingAddedToOpenCommand();
    
    ETypingCommand m_commandType;
    String m_textToInsert;
    bool m_openForMoreTyping;
    bool m_applyEditing;
    bool m_selectInsertedText;
    bool m_smartDelete;
    TextGranularity m_granularity;
    
    // Undoing a series of backward deletes will restore a selection around all of the
    // characters that were deleted, but only if the typing command being undone
    // was opened with a backward delete.
    bool m_openedByBackwardDelete;
};

} // namespace WebCore

#endif // TypingCommand_h
