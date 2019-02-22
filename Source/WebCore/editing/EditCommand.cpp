/*
 * Copyright (C) 2005, 2006, 2007, 2013 Apple, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

#include "AXObjectCache.h"
#include "CompositeEditCommand.h"
#include "Document.h"
#include "Editing.h"
#include "Editor.h"
#include "Element.h"
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLTextAreaElement.h"
#include "NodeTraversal.h"

namespace WebCore {

String inputTypeNameForEditingAction(EditAction action)
{
    switch (action) {
    case EditAction::Justify:
        return "formatJustifyFull"_s;
    case EditAction::AlignLeft:
        return "formatJustifyLeft"_s;
    case EditAction::AlignRight:
        return "formatJustifyRight"_s;
    case EditAction::Center:
        return "formatJustifyCenter"_s;
    case EditAction::Subscript:
        return "formatSubscript"_s;
    case EditAction::Superscript:
        return "formatSuperscript"_s;
    case EditAction::Underline:
        return "formatUnderline"_s;
    case EditAction::SetColor:
        return "formatFontColor"_s;
    case EditAction::DeleteByDrag:
        return "deleteByDrag"_s;
    case EditAction::Cut:
        return "deleteByCut"_s;
    case EditAction::Bold:
        return "formatBold"_s;
    case EditAction::Italics:
        return "formatItalic"_s;
    case EditAction::Paste:
        return "insertFromPaste"_s;
    case EditAction::Delete:
    case EditAction::TypingDeleteSelection:
        return "deleteContent"_s;
    case EditAction::TypingDeleteBackward:
        return "deleteContentBackward"_s;
    case EditAction::TypingDeleteForward:
        return "deleteContentForward"_s;
    case EditAction::TypingDeleteWordBackward:
        return "deleteWordBackward"_s;
    case EditAction::TypingDeleteWordForward:
        return "deleteWordForward"_s;
    case EditAction::TypingDeleteLineBackward:
        return "deleteHardLineBackward"_s;
    case EditAction::TypingDeleteLineForward:
        return "deleteHardLineForward"_s;
    case EditAction::TypingDeletePendingComposition:
        return "deleteCompositionText"_s;
    case EditAction::TypingDeleteFinalComposition:
        return "deleteByComposition"_s;
    case EditAction::Insert:
    case EditAction::TypingInsertText:
        return "insertText"_s;
    case EditAction::InsertReplacement:
        return "insertReplacementText"_s;
    case EditAction::InsertFromDrop:
        return "insertFromDrop"_s;
    case EditAction::TypingInsertLineBreak:
        return "insertLineBreak"_s;
    case EditAction::TypingInsertParagraph:
        return "insertParagraph"_s;
    case EditAction::InsertOrderedList:
        return "insertOrderedList"_s;
    case EditAction::InsertUnorderedList:
        return "insertUnorderedList"_s;
    case EditAction::TypingInsertPendingComposition:
        return "insertCompositionText"_s;
    case EditAction::TypingInsertFinalComposition:
        return "insertFromComposition"_s;
    case EditAction::Indent:
        return "formatIndent"_s;
    case EditAction::Outdent:
        return "formatOutdent"_s;
    case EditAction::SetInlineWritingDirection:
        return "formatSetInlineTextDirection"_s;
    case EditAction::SetBlockWritingDirection:
        return "formatSetBlockTextDirection"_s;
    default:
        return emptyString();
    }
}

EditCommand::EditCommand(Document& document, EditAction editingAction)
    : m_document(document)
    , m_editingAction(editingAction)
{
    ASSERT(document.frame());
    setStartingSelection(m_document->frame()->selection().selection());
    setEndingSelection(m_startingSelection);
}

EditCommand::EditCommand(Document& document, const VisibleSelection& startingSelection, const VisibleSelection& endingSelection)
    : m_document(document)
{
    ASSERT(document.frame());
    setStartingSelection(startingSelection);
    setEndingSelection(endingSelection);
}

EditCommand::~EditCommand() = default;

Frame& EditCommand::frame()
{
    ASSERT(document().frame());
    return *document().frame();
}

const Frame& EditCommand::frame() const
{
    ASSERT(document().frame());
    return *document().frame();
}

EditAction EditCommand::editingAction() const
{
    return m_editingAction;
}

static inline EditCommandComposition* compositionIfPossible(EditCommand* command)
{
    if (!command->isCompositeEditCommand())
        return 0;
    return toCompositeEditCommand(command)->composition();
}

bool EditCommand::isEditingTextAreaOrTextInput() const
{
    auto* frame = m_document->frame();
    if (!frame)
        return false;

    auto* container = frame->selection().selection().start().containerNode();
    if (!container)
        return false;

    auto* ancestor = container->shadowHost();
    if (!ancestor)
        return false;

    return is<HTMLTextAreaElement>(*ancestor) || (is<HTMLInputElement>(*ancestor) && downcast<HTMLInputElement>(*ancestor).isText());
}

void EditCommand::setStartingSelection(const VisibleSelection& s)
{
    for (EditCommand* cmd = this; ; cmd = cmd->m_parent) {
        if (auto* composition = compositionIfPossible(cmd))
            composition->setStartingSelection(s);
        cmd->m_startingSelection = s;
        if (!cmd->m_parent || cmd->m_parent->isFirstCommand(cmd))
            break;
    }
}

void EditCommand::setEndingSelection(const VisibleSelection &s)
{
    for (EditCommand* cmd = this; cmd; cmd = cmd->m_parent) {
        if (auto* composition = compositionIfPossible(cmd))
            composition->setEndingSelection(s);
        cmd->m_endingSelection = s;
    }
}

void EditCommand::setParent(CompositeEditCommand* parent)
{
    ASSERT((parent && !m_parent) || (!parent && m_parent));
    m_parent = parent;
    if (parent) {
        m_startingSelection = parent->m_endingSelection;
        m_endingSelection = parent->m_endingSelection;
    }
}

void EditCommand::postTextStateChangeNotification(AXTextEditType type, const String& text)
{
    if (!AXObjectCache::accessibilityEnabled())
        return;
    postTextStateChangeNotification(type, text, frame().selection().selection().start());
}

void EditCommand::postTextStateChangeNotification(AXTextEditType type, const String& text, const VisiblePosition& position)
{
    if (!AXObjectCache::accessibilityEnabled())
        return;
    if (!text.length())
        return;
    auto* cache = document().existingAXObjectCache();
    if (!cache)
        return;
    auto* node = highestEditableRoot(position.deepEquivalent(), HasEditableAXRole);
    cache->postTextStateChangeNotification(node, type, text, position);
}

SimpleEditCommand::SimpleEditCommand(Document& document, EditAction editingAction)
    : EditCommand(document, editingAction)
{
}

void SimpleEditCommand::doReapply()
{
    doApply();
}

#ifndef NDEBUG
void SimpleEditCommand::addNodeAndDescendants(Node* startNode, HashSet<Node*>& nodes)
{
    for (Node* node = startNode; node; node = NodeTraversal::next(*node, startNode))
        nodes.add(node);
}
#endif

} // namespace WebCore
