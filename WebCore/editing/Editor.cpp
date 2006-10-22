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

#include "DeleteButtonController.h"
#include "EditorClient.h"
#include "Frame.h"
#include "HTMLElement.h"
#include "Range.h"
#include "Sound.h"

namespace WebCore {

// implement as platform-specific
static Pasteboard generalPasteboard()
{
    return 0;
}

bool Editor::canCopy()
{
    return false;
}

bool Editor::canCut()
{
    return false;
}

bool Editor::canDelete()
{
    return false;
}

bool Editor::canDeleteRange(Range* range)
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

bool Editor::canPaste()
{
    return false;
}

bool Editor::canSmartCopyOrDelete()
{
    return false;
}

void Editor::deleteSelection()
{
}

void Editor::deleteSelectionWithSmartDelete(bool enabled)
{
}

bool Editor::isSelectionRichlyEditable()
{
    return false;
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

bool Editor::shouldDeleteRange(Range* range)
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

bool Editor::shouldShowDeleteInterface(HTMLElement* element)
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
    if (isSelectionRichlyEditable())
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
