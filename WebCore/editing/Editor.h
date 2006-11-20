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

#ifndef Editor_h
#define Editor_h

#include "EditorInsertAction.h"
#include "Frame.h"
#include <wtf/Forward.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class DeleteButtonController;
class Editor;
class EditorClient;
class Frame;
class HTMLElement;
class Range;
class Selection;

// make platform-specific and implement - this is temporary placeholder
typedef int Pasteboard;

class Editor {
public:
    Editor(Frame*, PassRefPtr<EditorClient>);
    ~Editor();

    EditorClient* client() const;
    Frame* frame() const { return m_frame; }
    DeleteButtonController* deleteButtonController() const { return m_deleteButtonController.get(); }
    EditCommand* lastEditCommand() { return m_lastEditCommand.get(); }
    
    bool canEdit() const;
    bool canEditRichly() const;

    bool canCut() const;
    bool canCopy() const;
    bool canPaste() const;
    bool canDelete() const;
    
    void cut();
    void copy();
    void paste();
    void performDelete();

    void indent();
    void outdent();

    bool shouldInsertText(String, Range*, EditorInsertAction) const;
    bool shouldShowDeleteInterface(HTMLElement*) const;
    bool shouldDeleteRange(Range*) const;

    void respondToChangedSelection(const Selection& oldSelection);
    void respondToChangedContents(const Selection& endingSelection);
    
    Frame::TriState selectionUnorderedListState() const;
    Frame::TriState selectionOrderedListState() const;
    
    void removeFormattingAndStyle();

    // FIXME: Once the Editor implements all editing commands, it should track 
    // the lastEditCommand on its own, and we should remove this function.
    void setLastEditCommand(PassRefPtr<EditCommand> lastEditCommand);

    void deleteSelectionWithSmartDelete(bool smartDelete);
    
    Node* removedAnchor() const { return m_removedAnchor.get(); }
    void setRemovedAnchor(PassRefPtr<Node> n) { m_removedAnchor = n; }

    void applyStyle(CSSStyleDeclaration*, EditAction = EditActionUnspecified);
    void applyParagraphStyle(CSSStyleDeclaration*, EditAction = EditActionUnspecified);
    void applyStyleToSelection(CSSStyleDeclaration*, EditAction);
    void applyParagraphStyleToSelection(CSSStyleDeclaration*, EditAction);

    void appliedEditing(PassRefPtr<EditCommand>);
    void unappliedEditing(PassRefPtr<EditCommand>);
    void reappliedEditing(PassRefPtr<EditCommand>);
    
    bool selectionStartHasStyle(CSSStyleDeclaration*) const;
    
    bool selectWordBeforeMenuEvent() const;
    bool clientIsEditable() const;
    
    bool execCommand(const String&);
private:
    Frame* m_frame;
    RefPtr<EditorClient> m_client;
    OwnPtr<DeleteButtonController> m_deleteButtonController;
    RefPtr<EditCommand> m_lastEditCommand;
    RefPtr<Node> m_removedAnchor;

    bool canDeleteRange(Range*) const;
    bool canSmartCopyOrDelete();
    Range* selectedRange();
    bool tryDHTMLCopy();
    bool tryDHTMLCut();
    bool tryDHTMLPaste();
    void deleteSelection();
    void pasteAsPlainTextWithPasteboard(Pasteboard);
    void pasteWithPasteboard(Pasteboard, bool allowPlainText);
    void writeSelectionToPasteboard(Pasteboard);
};

} // namespace WebCore

#endif // Editor_h
