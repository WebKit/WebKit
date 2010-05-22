/*
 * Copyright (C) 2007 Apple Computer, Inc.  All rights reserved.
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
#include "RemoveFormatCommand.h"

#include "CSSComputedStyleDeclaration.h"
#include "CSSMutableStyleDeclaration.h"
#include "Editor.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "VisibleSelection.h"
#include "SelectionController.h"
#include "TextIterator.h"
#include "TypingCommand.h"
#include "ApplyStyleCommand.h"

namespace WebCore {

using namespace HTMLNames;

RemoveFormatCommand::RemoveFormatCommand(Document* document)
    : CompositeEditCommand(document)
{
}

void RemoveFormatCommand::doApply()
{
    Frame* frame = document()->frame();
    
    // Make a plain text string from the selection to remove formatting like tables and lists.
    String string = plainText(frame->selection()->selection().toNormalizedRange().get());

    // Get the default style for this editable root, it's the style that we'll give the
    // content that we're operating on.
    Node* root = frame->selection()->rootEditableElement();
    RefPtr<CSSMutableStyleDeclaration> defaultStyle = ApplyStyleCommand::editingStyleAtPosition(Position(root, 0));

    // Delete the selected content.
    // FIXME: We should be able to leave this to insertText, but its delete operation
    // doesn't preserve the style we're about to set.
    deleteSelection();
    
    // Delete doesn't remove fully selected lists.
    while (breakOutOfEmptyListItem())
        ;
    
    // If the selection was all formatting (like an empty list) the format-less text will 
    // be empty. Early return since we don't need to do any of the work that follows and
    // to avoid the ASSERT that fires if input(...) is called with an empty String.
    if (string.isEmpty())
        return;
    
    // Insert the content with the default style.
    // See <rdar://problem/5794382> RemoveFormat doesn't always reset text alignment
    frame->setTypingStyle(defaultStyle.get());
    
    inputText(string, true);
}

}
