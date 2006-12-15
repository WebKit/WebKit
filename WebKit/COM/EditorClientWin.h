/*
 * Copyright (C) 2006 Don Gibson <dgibson77@gmail.com>
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

#ifndef EditorClientWin_h
#define EditorClientWin_h

#include "EditorClient.h"

class EditorClientWin : public WebCore::EditorClient {
public:
    virtual ~EditorClientWin();
    virtual void pageDestroyed();

    virtual bool shouldDeleteRange(WebCore::Range*);
    virtual bool shouldShowDeleteInterface(WebCore::HTMLElement*);
    virtual bool smartInsertDeleteEnabled();
    virtual bool isContinuousSpellCheckingEnabled();
    virtual void toggleContinuousSpellChecking();
    virtual bool isGrammarCheckingEnabled();
    virtual void toggleGrammarChecking();
    virtual int spellCheckerDocumentTag();

    virtual bool selectWordBeforeMenuEvent();
    virtual bool isEditable();

    virtual bool shouldBeginEditing(WebCore::Range*);
    virtual bool shouldEndEditing(WebCore::Range*);
    virtual bool shouldInsertNode(WebCore::Node*, WebCore::Range*,
                                  WebCore::EditorInsertAction);
    virtual bool shouldInsertText(WebCore::String, WebCore::Range*,
                                  WebCore::EditorInsertAction);
    virtual bool shouldApplyStyle(WebCore::CSSStyleDeclaration*,
                                  WebCore::Range*);

    virtual void didBeginEditing();
    virtual void respondToChangedContents();
    virtual void didEndEditing();

    virtual void registerCommandForUndo(PassRefPtr<WebCore::EditCommand>);
    virtual void registerCommandForRedo(PassRefPtr<WebCore::EditCommand>);
    virtual void clearUndoRedoOperations();

    virtual bool canUndo() const;
    virtual bool canRedo() const;

    virtual void undo();
    virtual void redo();
};

#endif // EditorClientWin_h
