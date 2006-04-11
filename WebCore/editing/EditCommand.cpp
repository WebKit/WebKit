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
#include "EditCommand.h"

#include "Document.h"
#include "Frame.h"
#include "SelectionController.h"
#include "VisiblePosition.h"
#include "CSSComputedStyleDeclaration.h"
#include "dom2_eventsimpl.h"
#include "EventNames.h"
#include "htmlediting.h"

namespace WebCore {

using namespace EventNames;

#define IF_IMPL_NULL_RETURN_ARG(arg) do { \
        if (*this == 0) { return arg; } \
    } while (0)
        
#define IF_IMPL_NULL_RETURN do { \
        if (*this == 0) { return; } \
    } while (0)

EditCommandPtr::EditCommandPtr()
{
}

EditCommandPtr::EditCommandPtr(EditCommand *impl) : RefPtr<EditCommand>(impl)
{
}

bool EditCommandPtr::isCompositeStep() const
{
    IF_IMPL_NULL_RETURN_ARG(false);        
    return get()->isCompositeStep();
}

bool EditCommandPtr::isInsertTextCommand() const
{
    IF_IMPL_NULL_RETURN_ARG(false);        
    return get()->isInsertTextCommand();
}

bool EditCommandPtr::isTypingCommand() const
{
    IF_IMPL_NULL_RETURN_ARG(false);        
    return get()->isTypingCommand();
}

void EditCommandPtr::apply() const
{
    IF_IMPL_NULL_RETURN;
    get()->apply();
}

void EditCommandPtr::unapply() const
{
    IF_IMPL_NULL_RETURN;
    get()->unapply();
}

void EditCommandPtr::reapply() const
{
    IF_IMPL_NULL_RETURN;
    get()->reapply();
}

EditAction EditCommandPtr::editingAction() const
{
    IF_IMPL_NULL_RETURN_ARG(EditActionUnspecified);
    return get()->editingAction();
}

Document * const EditCommandPtr::document() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return get()->document();
}

Selection EditCommandPtr::startingSelection() const
{
    IF_IMPL_NULL_RETURN_ARG(Selection());
    return get()->startingSelection();
}

Selection EditCommandPtr::endingSelection() const
{
    IF_IMPL_NULL_RETURN_ARG(Selection());
    return get()->endingSelection();
}

void EditCommandPtr::setStartingSelection(const Selection &s) const
{
    IF_IMPL_NULL_RETURN;
    get()->setStartingSelection(s);
}

void EditCommandPtr::setStartingSelection(const VisiblePosition &p) const
{
    IF_IMPL_NULL_RETURN;
    get()->setStartingSelection(p);
}

void EditCommandPtr::setStartingSelection(const Position &p, EAffinity affinity) const
{
    IF_IMPL_NULL_RETURN;
    Selection s = Selection(p, affinity);
    get()->setStartingSelection(s);
}

void EditCommandPtr::setEndingSelection(const Selection &s) const
{
    IF_IMPL_NULL_RETURN;
    get()->setEndingSelection(s);
}

void EditCommandPtr::setEndingSelection(const VisiblePosition &p) const
{
    IF_IMPL_NULL_RETURN;
    get()->setEndingSelection(p);
}

void EditCommandPtr::setEndingSelection(const Position &p, EAffinity affinity) const
{
    IF_IMPL_NULL_RETURN;
    Selection s = Selection(p, affinity);
    get()->setEndingSelection(s);
}

CSSMutableStyleDeclaration *EditCommandPtr::typingStyle() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return get()->typingStyle();
}

void EditCommandPtr::setTypingStyle(PassRefPtr<CSSMutableStyleDeclaration> style) const
{
    IF_IMPL_NULL_RETURN;
    get()->setTypingStyle(style);
}

EditCommandPtr EditCommandPtr::parent() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return get()->parent();
}

void EditCommandPtr::setParent(const EditCommandPtr &cmd) const
{
    IF_IMPL_NULL_RETURN;
    get()->setParent(cmd.get());
}

EditCommandPtr &EditCommandPtr::emptyCommand()
{
    static EditCommandPtr m_emptyCommand;
    return m_emptyCommand;
}

EditCommand::EditCommand(Document *document) 
    : m_document(document), m_state(NotApplied), m_parent(0)
{
    ASSERT(m_document);
    ASSERT(m_document->frame());
    m_startingSelection = m_document->frame()->selection().selection();
    m_endingSelection = m_startingSelection;
}

EditCommand::~EditCommand()
{
}

void EditCommand::apply()
{
    ASSERT(m_document);
    ASSERT(m_document->frame());
    ASSERT(state() == NotApplied);
 
    Frame *frame = m_document->frame();
    
    bool topLevel = !isCompositeStep();
    if (topLevel) {
        if (!endingSelection().isContentRichlyEditable()) {
            switch (editingAction()) {
                case EditActionTyping:
                case EditActionPaste:
                case EditActionDrag:
                case EditActionSetWritingDirection:
                case EditActionCut:
                case EditActionUnspecified:
                    break;
                default:
                    ASSERT_NOT_REACHED();
                    return;
            }
        }
    }
    
    doApply();
    
    m_state = Applied;

    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    if (!preservesTypingStyle())
        setTypingStyle(0);

    if (topLevel) {
        updateLayout();
        EditCommandPtr cmd(this);
        frame->appliedEditing(cmd);
    }
}

void EditCommand::unapply()
{
    ASSERT(m_document);
    ASSERT(m_document->frame());
    ASSERT(state() == Applied);
 
    Frame *frame = m_document->frame();
    
    doUnapply();
    
    m_state = NotApplied;

    if (!isCompositeStep()) {
        updateLayout();
        EditCommandPtr cmd(this);
        frame->unappliedEditing(cmd);
    }
}

void EditCommand::reapply()
{
    ASSERT(m_document);
    ASSERT(m_document->frame());
    ASSERT(state() == NotApplied);
 
    Frame *frame = m_document->frame();
    
    doReapply();
    
    m_state = Applied;

    if (!isCompositeStep()) {
        updateLayout();
        EditCommandPtr cmd(this);
        frame->reappliedEditing(cmd);
    }
}

void EditCommand::doReapply()
{
    doApply();
}

EditAction EditCommand::editingAction() const
{
    return EditActionUnspecified;
}

void EditCommand::setStartingSelection(const Selection &s)
{
    for (EditCommand *cmd = this; cmd; cmd = cmd->m_parent)
        cmd->m_startingSelection = s;
}

void EditCommand::setStartingSelection(const VisiblePosition &p)
{
    Selection s = Selection(p.deepEquivalent(), p.affinity());
    for (EditCommand *cmd = this; cmd; cmd = cmd->m_parent)
        cmd->m_startingSelection = s;
}

void EditCommand::setStartingSelection(const Position &p, EAffinity affinity)
{
    Selection s = Selection(p, affinity);
    for (EditCommand *cmd = this; cmd; cmd = cmd->m_parent)
        cmd->m_startingSelection = s;
}

void EditCommand::setEndingSelection(const Selection &s)
{
    for (EditCommand *cmd = this; cmd; cmd = cmd->m_parent)
        cmd->m_endingSelection = s;
}

void EditCommand::setEndingSelection(const VisiblePosition &p)
{
    Selection s = Selection(p.deepEquivalent(), p.affinity());
    for (EditCommand *cmd = this; cmd; cmd = cmd->m_parent)
        cmd->m_endingSelection = s;
}

void EditCommand::setEndingSelection(const Position &p, EAffinity affinity)
{
    Selection s = Selection(p, affinity);
    for (EditCommand *cmd = this; cmd; cmd = cmd->m_parent)
        cmd->m_endingSelection = s;
}

void EditCommand::setTypingStyle(PassRefPtr<CSSMutableStyleDeclaration> style)
{
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    if (!m_parent) {
        // Special more-efficient case for where there's only one command that
        // takes advantage of the ability of PassRefPtr to pass its ref to a RefPtr.
        m_typingStyle = style;
        return;
    }
    for (EditCommand* cmd = this; cmd; cmd = cmd->m_parent)
        cmd->m_typingStyle = style.get(); // must use get() to avoid setting parent styles to 0
}

bool EditCommand::preservesTypingStyle() const
{
    return false;
}

bool EditCommand::isInsertTextCommand() const
{
    return false;
}

bool EditCommand::isTypingCommand() const
{
    return false;
}

PassRefPtr<CSSMutableStyleDeclaration> EditCommand::styleAtPosition(const Position &pos)
{
    RefPtr<CSSMutableStyleDeclaration> style = positionBeforeTabSpan(pos).computedStyle()->copyInheritableProperties();
 
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    CSSMutableStyleDeclaration* typingStyle = document()->frame()->typingStyle();
    if (typingStyle)
        style->merge(typingStyle);

    return style.release();
}

void EditCommand::updateLayout() const
{
    document()->updateLayoutIgnorePendingStylesheets();
}

} // namespace WebCore
