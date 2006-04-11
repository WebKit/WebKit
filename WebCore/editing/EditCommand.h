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

#ifndef edit_command_h__
#define edit_command_h__

#include "EditAction.h"
#include "Selection.h"

namespace WebCore {

class CSSMutableStyleDeclaration;
class Document;
class VisiblePosition;

//------------------------------------------------------------------------------------------
// EditCommand

class EditCommand : public Shared<EditCommand>
{
public:
    EditCommand(Document *);
    virtual ~EditCommand();

    bool isCompositeStep() const { return m_parent != 0; }
    EditCommand *parent() const { return m_parent; }
    void setParent(EditCommand *parent) { m_parent = parent; }

    enum ECommandState { NotApplied, Applied };
    
    void apply();
    void unapply();
    void reapply();

    virtual void doApply() = 0;
    virtual void doUnapply() = 0;
    virtual void doReapply();  // calls doApply()

    virtual EditAction editingAction() const;

    virtual Document * const document() const { return m_document.get(); }

    Selection startingSelection() const { return m_startingSelection; }
    Selection endingSelection() const { return m_endingSelection; }

    ECommandState state() const { return m_state; }
    void setState(ECommandState state) { m_state = state; }

    void setStartingSelection(const Selection &s);
    void setStartingSelection(const VisiblePosition &p);
    void setStartingSelection(const Position &p, EAffinity affinity);
    void setEndingSelection(const Selection &s);
    void setEndingSelection(const VisiblePosition &p);
    void setEndingSelection(const Position &p, EAffinity affinity);

    CSSMutableStyleDeclaration* typingStyle() const { return m_typingStyle.get(); };
    void setTypingStyle(PassRefPtr<CSSMutableStyleDeclaration>);
    
    PassRefPtr<CSSMutableStyleDeclaration> styleAtPosition(const Position&);
    
    virtual bool isInsertTextCommand() const;
    virtual bool isTypingCommand() const;

    void updateLayout() const;
    
private:
    virtual bool preservesTypingStyle() const;

    RefPtr<Document> m_document;
    ECommandState m_state;
    Selection m_startingSelection;
    Selection m_endingSelection;
    RefPtr<CSSMutableStyleDeclaration> m_typingStyle;
    EditCommand *m_parent;
};

class EditCommandPtr : public RefPtr<EditCommand>
{
public:
    EditCommandPtr();
    EditCommandPtr(EditCommand *);

    bool isCompositeStep() const;

    void apply() const;
    void unapply() const;
    void reapply() const;

    EditAction editingAction() const;

    Document * const document() const;

    Selection startingSelection() const;
    Selection endingSelection() const;

    void setStartingSelection(const Selection &s) const;
    void setStartingSelection(const VisiblePosition &p) const;
    void setStartingSelection(const Position &p, EAffinity affinity) const;
    void setEndingSelection(const Selection &s) const;
    void setEndingSelection(const VisiblePosition &p) const;
    void setEndingSelection(const Position &p, EAffinity affinity) const;

    CSSMutableStyleDeclaration* typingStyle() const;
    void setTypingStyle(PassRefPtr<CSSMutableStyleDeclaration>) const;

    EditCommandPtr parent() const;
    void setParent(const EditCommandPtr &) const;

    bool isInsertTextCommand() const;
    bool isInsertLineBreakCommand() const;
    bool isTypingCommand() const;

    static EditCommandPtr &emptyCommand();
};

} // namespace WebCore

#endif // __edit_command_h__
