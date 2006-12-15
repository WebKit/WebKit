/*
 * Copyright (C) 2006 Marvin Decker <marv.decker@gmail.com>
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

#pragma warning(push, 0)
#include "EditCommand.h"
#include "EditorClientWin.h"
#include "PlatformString.h"
#pragma warning(pop)

#define notImplemented() {}

EditorClientWin::~EditorClientWin()
{
}

void EditorClientWin::pageDestroyed()
{
    notImplemented();
}

bool EditorClientWin::shouldDeleteRange(WebCore::Range*)
{
    notImplemented();
    return false;
}

bool EditorClientWin::shouldShowDeleteInterface(WebCore::HTMLElement*)
{
    notImplemented();
    return false;
}

bool EditorClientWin::smartInsertDeleteEnabled()
{
    notImplemented();
    return false;
}

bool EditorClientWin::isContinuousSpellCheckingEnabled()
{
    notImplemented();
    return false;
}

void EditorClientWin::toggleContinuousSpellChecking()
{
    notImplemented();
}

bool EditorClientWin::isGrammarCheckingEnabled()
{
    notImplemented();
    return false;
}

void EditorClientWin::toggleGrammarChecking()
{
    notImplemented();
}

int EditorClientWin::spellCheckerDocumentTag()
{
    notImplemented();
    return 0;
}

bool EditorClientWin::selectWordBeforeMenuEvent()
{
    notImplemented();
    return false;
}

bool EditorClientWin::isEditable()
{
    notImplemented();
    return false;
}

bool EditorClientWin::shouldBeginEditing(WebCore::Range*)
{{
}
    notImplemented();
    return false;
}

bool EditorClientWin::shouldEndEditing(WebCore::Range*)
{
    notImplemented();
    return false;
}

bool EditorClientWin::shouldInsertNode(WebCore::Node*, WebCore::Range*,
                                       WebCore::EditorInsertAction)
{
    notImplemented();
    return false;
}

bool EditorClientWin::shouldInsertText(WebCore::String, WebCore::Range*,
                                       WebCore::EditorInsertAction)
{
    notImplemented();
    return false;
}

bool EditorClientWin::shouldApplyStyle(WebCore::CSSStyleDeclaration*,
                                       WebCore::Range*)
{
    notImplemented();
    return false;
}

void EditorClientWin::didBeginEditing()
{
    notImplemented();
}

void EditorClientWin::respondToChangedContents()
{
    notImplemented();
}

void EditorClientWin::didEndEditing()
{
    notImplemented();
}

void EditorClientWin::registerCommandForUndo(PassRefPtr<WebCore::EditCommand>)
{
    notImplemented();
}

void EditorClientWin::registerCommandForRedo(PassRefPtr<WebCore::EditCommand>)
{
    notImplemented();
}

void EditorClientWin::clearUndoRedoOperations()
{
    notImplemented();
}

bool EditorClientWin::canUndo() const
{
    notImplemented();
    return false;
}

bool EditorClientWin::canRedo() const
{
    notImplemented();
    return false;
}

void EditorClientWin::undo()
{
    notImplemented();
}

void EditorClientWin::redo()
{
    notImplemented();
}
