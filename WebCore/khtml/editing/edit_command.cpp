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

#include "edit_command.h"
#include "selection.h"
#include "khtml_part.h"

#include "xml/dom_position.h"
#include "xml/dom_docimpl.h"
#include "css/css_valueimpl.h"
#include "css/css_computedstyle.h"

#if APPLE_CHANGES
#include "KWQAssertions.h"
#else
#define ASSERT(assertion) assert(assertion)
#endif

using DOM::DocumentImpl;
using DOM::Position;
using DOM::CSSMutableStyleDeclarationImpl;
using DOM::CSSComputedStyleDeclarationImpl;

#define IF_IMPL_NULL_RETURN_ARG(arg) do { \
        if (isNull()) { return arg; } \
    } while (0)
        
#define IF_IMPL_NULL_RETURN do { \
        if (isNull()) { return; } \
    } while (0)

namespace khtml {

EditCommandPtr::EditCommandPtr()
{
}

EditCommandPtr::EditCommandPtr(EditCommand *impl) : SharedPtr<EditCommand>(impl)
{
}

EditCommandPtr::EditCommandPtr(const EditCommandPtr &o) : SharedPtr<EditCommand>(o)
{
}

EditCommandPtr::~EditCommandPtr()
{
}

EditCommandPtr &EditCommandPtr::operator=(const EditCommandPtr &c)
{
    static_cast<SharedPtr<EditCommand> &>(*this) = c;
    return *this;
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

DocumentImpl * const EditCommandPtr::document() const
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

CSSMutableStyleDeclarationImpl *EditCommandPtr::typingStyle() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return get()->typingStyle();
}

void EditCommandPtr::setTypingStyle(CSSMutableStyleDeclarationImpl *style) const
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

EditCommand::EditCommand(DocumentImpl *document) 
    : m_document(document), m_state(NotApplied), m_typingStyle(0), m_parent(0)
{
    ASSERT(m_document);
    ASSERT(m_document->part());
    m_document->ref();
    m_startingSelection = m_document->part()->selection();
    m_endingSelection = m_startingSelection;

    m_document->part()->setSelection(Selection(), false, true);
}

EditCommand::~EditCommand()
{
    ASSERT(m_document);
    m_document->deref();
    if (m_typingStyle)
        m_typingStyle->deref();
}

void EditCommand::apply()
{
    ASSERT(m_document);
    ASSERT(m_document->part());
    ASSERT(state() == NotApplied);
 
    KHTMLPart *part = m_document->part();

    ASSERT(part->selection().isNone());

    doApply();
    
    m_state = Applied;

    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    if (!preservesTypingStyle())
        setTypingStyle(0);

    if (!isCompositeStep()) {
        document()->updateLayout();
        EditCommandPtr cmd(this);
        part->appliedEditing(cmd);
    }
}

void EditCommand::unapply()
{
    ASSERT(m_document);
    ASSERT(m_document->part());
    ASSERT(state() == Applied);

    bool topLevel = !isCompositeStep();
 
    KHTMLPart *part = m_document->part();

    if (topLevel) {
        part->setSelection(Selection(), false, true);
    }
    ASSERT(part->selection().isNone());
    
    doUnapply();
    
    m_state = NotApplied;

    if (topLevel) {
        document()->updateLayout();
        EditCommandPtr cmd(this);
        part->unappliedEditing(cmd);
    }
}

void EditCommand::reapply()
{
    ASSERT(m_document);
    ASSERT(m_document->part());
    ASSERT(state() == NotApplied);
    
    bool topLevel = !isCompositeStep();
 
    KHTMLPart *part = m_document->part();

    if (topLevel) {
        part->setSelection(Selection(), false, true);
    }
    ASSERT(part->selection().isNone());
    
    doReapply();
    
    m_state = Applied;

    if (topLevel) {
        document()->updateLayout();
        EditCommandPtr cmd(this);
        part->reappliedEditing(cmd);
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
    Selection s = Selection(p);
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
    Selection s = Selection(p);
    for (EditCommand *cmd = this; cmd; cmd = cmd->m_parent)
        cmd->m_endingSelection = s;
}

void EditCommand::setEndingSelection(const Position &p, EAffinity affinity)
{
    Selection s = Selection(p, affinity);
    for (EditCommand *cmd = this; cmd; cmd = cmd->m_parent)
        cmd->m_endingSelection = s;
}

void EditCommand::assignTypingStyle(CSSMutableStyleDeclarationImpl *style)
{
    if (m_typingStyle == style)
        return;
        
    CSSMutableStyleDeclarationImpl *old = m_typingStyle;
    m_typingStyle = style;
    if (m_typingStyle)
        m_typingStyle->ref();
    if (old)
        old->deref();
}

void EditCommand::setTypingStyle(CSSMutableStyleDeclarationImpl *style)
{
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    for (EditCommand *cmd = this; cmd; cmd = cmd->m_parent)
        cmd->assignTypingStyle(style);
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

CSSMutableStyleDeclarationImpl *EditCommand::styleAtPosition(const Position &pos)
{
    CSSComputedStyleDeclarationImpl *computedStyle = pos.computedStyle();
    computedStyle->ref();
    CSSMutableStyleDeclarationImpl *style = computedStyle->copyInheritableProperties();
    computedStyle->deref();
 
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    CSSMutableStyleDeclarationImpl *typingStyle = document()->part()->typingStyle();
    if (typingStyle)
        style->merge(typingStyle);
    
    return style;
}

} // namespace khtml
