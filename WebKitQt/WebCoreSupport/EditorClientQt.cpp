/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org> 
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * All rights reserved.
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
#include "EditorClientQt.h"

#include "EditCommand.h"

#include <stdio.h>

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d(%s)\n", __FILE__, __LINE__, __FUNCTION__); } while(0)

namespace WebCore {

void EditorClientQt::ref()
{
    Shared<EditorClientQt>::ref();
}

void EditorClientQt::deref()
{
    Shared<EditorClientQt>::deref();
}

bool EditorClientQt::shouldDeleteRange(Range*)
{
    notImplemented();
    return false;
}

bool EditorClientQt::shouldShowDeleteInterface(HTMLElement*)
{
    return false;
}

bool EditorClientQt::isContinuousSpellCheckingEnabled()
{
    notImplemented();
    return false;
}

bool EditorClientQt::isGrammarCheckingEnabled()
{
    notImplemented();
    return false;
}

int EditorClientQt::spellCheckerDocumentTag()
{
    notImplemented();
    return 0;
}

bool EditorClientQt::shouldBeginEditing(WebCore::Range*)
{
    notImplemented();
    return false;
}

bool EditorClientQt::shouldEndEditing(WebCore::Range*)
{
    notImplemented();
    return false;
}

bool EditorClientQt::shouldInsertText(String, Range*, EditorInsertAction)
{
    notImplemented();
    return false;
}

bool EditorClientQt::shouldApplyStyle(WebCore::CSSStyleDeclaration*,
                                      WebCore::Range*)
{
    notImplemented();
    return false;
}

void EditorClientQt::didBeginEditing()
{
    notImplemented();
}

void EditorClientQt::respondToChangedContents()
{
    notImplemented();
}

void EditorClientQt::didEndEditing()
{
    notImplemented();
}

bool EditorClientQt::selectWordBeforeMenuEvent()
{
    notImplemented();
    return false;
}

bool EditorClientQt::isEditable()
{
    notImplemented();
    return false;
}

void EditorClientQt::registerCommandForUndo(WTF::PassRefPtr<WebCore::EditCommand>)
{
    notImplemented();
}

void EditorClientQt::registerCommandForRedo(WTF::PassRefPtr<WebCore::EditCommand>)
{
    notImplemented();
}

void EditorClientQt::clearUndoRedoOperations()
{
    notImplemented();
}

bool EditorClientQt::canUndo() const
{
    notImplemented();
    return false;
}

bool EditorClientQt::canRedo() const
{
    notImplemented();
    return false;
}

void EditorClientQt::undo()
{
    notImplemented();
}

void EditorClientQt::redo()
{
    notImplemented();
}

bool EditorClientQt::shouldInsertNode(Node*, Range*, EditorInsertAction)
{
    notImplemented();
}

void WebCore::EditorClientQt::pageDestroyed()
{
    notImplemented();
}

bool EditorClientQt::smartInsertDeleteEnabled()
{
    notImplemented();
    return false;
}

}

// vim: ts=4 sw=4 et
