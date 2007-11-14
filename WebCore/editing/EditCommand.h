/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef EditCommand_h
#define EditCommand_h

#include "EditAction.h"
#include "Element.h"
#include "Selection.h"

namespace WebCore {

class CompositeEditCommand;
class CSSMutableStyleDeclaration;

class EditCommand : public RefCounted<EditCommand> {
public:
    EditCommand(Document*);
    virtual ~EditCommand();

    void setParent(CompositeEditCommand*);

    void apply();
    void unapply();
    void reapply();

    virtual EditAction editingAction() const;

    const Selection& startingSelection() const { return m_startingSelection; }
    const Selection& endingSelection() const { return m_endingSelection; }

    Element* startingRootEditableElement() const { return m_startingRootEditableElement.get(); }
    Element* endingRootEditableElement() const { return m_endingRootEditableElement.get(); }

    CSSMutableStyleDeclaration* typingStyle() const { return m_typingStyle.get(); };
    void setTypingStyle(PassRefPtr<CSSMutableStyleDeclaration>);
    
    virtual bool isInsertTextCommand() const;
    virtual bool isTypingCommand() const;

protected:
    Document* document() const { return m_document.get(); }

    void setStartingSelection(const Selection&);
    void setEndingSelection(const Selection&);

    PassRefPtr<CSSMutableStyleDeclaration> styleAtPosition(const Position&);
    void updateLayout() const;

private:
    virtual void doApply() = 0;
    virtual void doUnapply() = 0;
    virtual void doReapply(); // calls doApply()

    virtual bool preservesTypingStyle() const;

    RefPtr<Document> m_document;
    Selection m_startingSelection;
    Selection m_endingSelection;
    RefPtr<Element> m_startingRootEditableElement;
    RefPtr<Element> m_endingRootEditableElement;
    RefPtr<CSSMutableStyleDeclaration> m_typingStyle;
    CompositeEditCommand* m_parent;

    friend void applyCommand(PassRefPtr<EditCommand>);
};

void applyCommand(PassRefPtr<EditCommand>);

} // namespace WebCore

#endif // EditCommand_h
