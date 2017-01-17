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
#include "Editor.h"
#include "Element.h"
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLTextAreaElement.h"
#include "NodeTraversal.h"
#include "htmlediting.h"

namespace WebCore {

String inputTypeNameForEditingAction(EditAction action)
{
    switch (action) {
    case EditActionJustify:
    case EditActionAlignLeft:
        return ASCIILiteral("formatJustifyLeft");
    case EditActionAlignRight:
        return ASCIILiteral("formatJustifyRight");
    case EditActionCenter:
        return ASCIILiteral("formatJustifyCenter");
    case EditActionSubscript:
        return ASCIILiteral("formatSubscript");
    case EditActionSuperscript:
        return ASCIILiteral("formatSuperscript");
    case EditActionUnderline:
        return ASCIILiteral("formatUnderline");
    case EditActionSetColor:
        return ASCIILiteral("formatForeColor");
    case EditActionDeleteByDrag:
        return ASCIILiteral("deleteByDrag");
    case EditActionCut:
        return ASCIILiteral("deleteByCut");
    case EditActionBold:
        return ASCIILiteral("formatBold");
    case EditActionItalics:
        return ASCIILiteral("formatItalic");
    case EditActionPaste:
        return ASCIILiteral("insertFromPaste");
    case EditActionDelete:
    case EditActionTypingDeleteSelection:
        return ASCIILiteral("deleteContent");
    case EditActionTypingDeleteBackward:
        return ASCIILiteral("deleteContentBackward");
    case EditActionTypingDeleteForward:
        return ASCIILiteral("deleteContentForward");
    case EditActionTypingDeleteWordBackward:
        return ASCIILiteral("deleteWordBackward");
    case EditActionTypingDeleteWordForward:
        return ASCIILiteral("deleteWordForward");
    case EditActionTypingDeleteLineBackward:
        return ASCIILiteral("deleteHardLineBackward");
    case EditActionTypingDeleteLineForward:
        return ASCIILiteral("deleteHardLineForward");
    case EditActionTypingDeletePendingComposition:
        return ASCIILiteral("deleteCompositionText");
    case EditActionTypingDeleteFinalComposition:
        return ASCIILiteral("deleteByComposition");
    case EditActionInsert:
    case EditActionTypingInsertText:
        return ASCIILiteral("insertText");
    case EditActionInsertReplacement:
        return ASCIILiteral("insertReplacementText");
    case EditActionInsertFromDrop:
        return ASCIILiteral("insertFromDrop");
    case EditActionTypingInsertLineBreak:
        return ASCIILiteral("insertLineBreak");
    case EditActionTypingInsertParagraph:
        return ASCIILiteral("insertParagraph");
    case EditActionInsertOrderedList:
        return ASCIILiteral("insertOrderedList");
    case EditActionInsertUnorderedList:
        return ASCIILiteral("insertUnorderedList");
    case EditActionTypingInsertPendingComposition:
        return ASCIILiteral("insertCompositionText");
    case EditActionTypingInsertFinalComposition:
        return ASCIILiteral("insertFromComposition");
    case EditActionIndent:
        return ASCIILiteral("formatIndent");
    case EditActionOutdent:
        return ASCIILiteral("formatOutdent");
    case EditActionSetWritingDirection:
        return ASCIILiteral("formatSetInlineTextDirection");
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

EditCommand::~EditCommand()
{
}

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
