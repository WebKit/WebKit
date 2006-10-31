/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "Editor.h"

#include "ApplyStyleCommand.h"
#include "CSSComputedStyleDeclaration.h"
#include "DeleteButtonController.h"
#include "DeleteSelectionCommand.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "EditCommand.h"
#include "Editor.h"
#include "EditorClient.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "Range.h"
#include "ReplaceSelectionCommand.h"
#include "SelectionController.h"
#include "Sound.h"
#include "htmlediting.h"
#include "markup.h"

namespace WebCore {

using namespace HTMLNames;

// implement as platform-specific
static Pasteboard generalPasteboard()
{
    return 0;
}

EditorClient* Editor::client() const
{
    return m_client.get();
}

bool Editor::canEdit() const
{
    SelectionController* selectionController = m_frame->selectionController();
    return selectionController->isCaretOrRange() && selectionController->isContentEditable();
}

bool Editor::canEditRichly() const
{
    SelectionController* selectionController = m_frame->selectionController();
    return canEdit() && selectionController->isContentRichlyEditable();
}

bool Editor::canCut() const
{
    return canCopy() && canDelete();
}

bool Editor::canCopy() const
{
    SelectionController* selectionController = m_frame->selectionController();
    return selectionController->isRange() && !selectionController->isInPasswordField();
}

bool Editor::canPaste() const
{
    SelectionController* selectionController = m_frame->selectionController();
    return selectionController->isCaretOrRange() && selectionController->isContentEditable();
}

bool Editor::canDelete() const
{
    SelectionController* selectionController = m_frame->selectionController();
    return selectionController->isRange() && selectionController->isContentEditable();
}

bool Editor::canDeleteRange(Range* range) const
{
    ExceptionCode ec = 0;
    Node* startContainer = range->startContainer(ec);
    Node* endContainer = range->endContainer(ec);
    if (!startContainer || !endContainer)
        return false;
    
    if (!startContainer->isContentEditable() || !endContainer->isContentEditable())
        return false;
    
    if (range->collapsed(ec)) {
        VisiblePosition start(startContainer, range->startOffset(ec), DOWNSTREAM);
        VisiblePosition previous = start.previous();
        // FIXME: We sometimes allow deletions at the start of editable roots, like when the caret is in an empty list item.
        if (previous.isNull() || previous.deepEquivalent().node()->rootEditableElement() != startContainer->rootEditableElement())
            return false;
    }
    return true;
}

bool Editor::canSmartCopyOrDelete()
{
    return false;
}

void Editor::deleteSelection()
{
}

void Editor::deleteSelectionWithSmartDelete(bool smartDelete)
{
    if (m_frame->selectionController()->isNone())
        return;
    
    applyCommand(new DeleteSelectionCommand(m_frame->document(), smartDelete));
}

void Editor::pasteAsPlainTextWithPasteboard(Pasteboard pasteboard)
{
}

void Editor::pasteWithPasteboard(Pasteboard pasteboard, bool allowPlainText)
{
}

Range* Editor::selectedRange()
{
    return 0;
}

bool Editor::shouldDeleteRange(Range* range) const
{
    ExceptionCode ec;
    if (!range || range->collapsed(ec))
        return false;
    
    if (!canDeleteRange(range))
        return false;

    return m_client->shouldDeleteRange(range);
 }

bool Editor::tryDHTMLCopy()
{
    bool handled = false;
    return handled;
}

bool Editor::tryDHTMLCut()
{
    bool handled = false;
    return handled;
}

bool Editor::tryDHTMLPaste()
{
    bool handled = false;
    return handled;
}

void Editor::writeSelectionToPasteboard(Pasteboard pasteboard)
{
}

bool Editor::shouldShowDeleteInterface(HTMLElement* element) const
{
    return m_client->shouldShowDeleteInterface(element);
}

void Editor::respondToChangedSelection(const Selection& oldSelection)
{
    m_deleteButtonController->respondToChangedSelection(oldSelection);
}

void Editor::respondToChangedContents()
{
    m_deleteButtonController->respondToChangedContents();
}

Frame::TriState Editor::selectionUnorderedListState() const
{
    if (m_frame->selectionController()->isCaret()) {
        Node* selectionNode = m_frame->selectionController()->selection().start().node();
        if (enclosingNodeWithTag(selectionNode, ulTag))
            return Frame::trueTriState;
    } else if (m_frame->selectionController()->isRange()) {
        Node* startNode = enclosingNodeWithTag(m_frame->selectionController()->selection().start().node(), ulTag);
        Node* endNode = enclosingNodeWithTag(m_frame->selectionController()->selection().end().node(), ulTag);
        if (startNode && endNode && startNode == endNode)
            return Frame::trueTriState;
    }

    return Frame::falseTriState;
}

Frame::TriState Editor::selectionOrderedListState() const
{
    if (m_frame->selectionController()->isCaret()) {
        Node* selectionNode = m_frame->selectionController()->selection().start().node();
        if (enclosingNodeWithTag(selectionNode, olTag))
            return Frame::trueTriState;
    } else if (m_frame->selectionController()->isRange()) {
        Node* startNode = enclosingNodeWithTag(m_frame->selectionController()->selection().start().node(), olTag);
        Node* endNode = enclosingNodeWithTag(m_frame->selectionController()->selection().end().node(), olTag);
        if (startNode && endNode && startNode == endNode)
            return Frame::trueTriState;
    }

    return Frame::falseTriState;
}

void Editor::removeFormattingAndStyle()
{
    Document* document = frame()->document();
    
    // Make a plain text string from the selection to remove formatting like tables and lists.
    RefPtr<DocumentFragment> text = createFragmentFromText(frame()->selectionController()->toRange().get(), frame()->selectionController()->toString());
    
    // Put the fragment made from that string into a style span with the document's
    // default style to make sure that it is unstyled regardless of where it is inserted.
    Position pos(document->documentElement(), 0);
    RefPtr<CSSComputedStyleDeclaration> computedStyle = pos.computedStyle();
    RefPtr<CSSMutableStyleDeclaration> defaultStyle = computedStyle->copyInheritableProperties();
    
    RefPtr<Element> span = createStyleSpanElement(document);
    span->setAttribute(styleAttr, defaultStyle->cssText());
    
    ExceptionCode ec;
    
    while (text->lastChild())
        span->appendChild(text->lastChild(), ec);
    
    RefPtr<DocumentFragment> fragment = new DocumentFragment(document);
    fragment->appendChild(span, ec);
    
    applyCommand(new ReplaceSelectionCommand(document, fragment, false, false, false, true, EditActionUnspecified));
}

void Editor::setLastEditCommand(PassRefPtr<EditCommand> lastEditCommand) 
{
    m_lastEditCommand = lastEditCommand;
}

// =============================================================================
//
// public editing commands
//
// =============================================================================

Editor::Editor(Frame* frame, PassRefPtr<EditorClient> client)
    : m_frame(frame)
    , m_client(client)
    , m_deleteButtonController(new DeleteButtonController(frame))
{ 
}

Editor::~Editor()
{
}

void Editor::cut()
{
    if (tryDHTMLCut())
        return; // DHTML did the whole operation
    if (!canCut()) {
        systemBeep();
        return;
    }
    
    if (shouldDeleteRange(selectedRange())) {
        writeSelectionToPasteboard(generalPasteboard());
        deleteSelectionWithSmartDelete(canSmartCopyOrDelete());
    }
}

void Editor::copy()
{
    if (tryDHTMLCopy())
        return; // DHTML did the whole operation
    if (!canCopy()) {
        systemBeep();
        return;
    }
    writeSelectionToPasteboard(generalPasteboard());
}

void Editor::paste()
{
    if (tryDHTMLPaste())
        return;     // DHTML did the whole operation
    if (!canPaste())
        return;
    if (canEditRichly())
        pasteWithPasteboard(generalPasteboard(), true);
    else
        pasteAsPlainTextWithPasteboard(generalPasteboard());
}

void Editor::performDelete()
{
    if (!canDelete()) {
        systemBeep();
        return;
    }
    deleteSelection();
}

} // namespace WebCore
