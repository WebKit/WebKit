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

#ifndef editor_h__
#define editor_h__

namespace WebCore {

class Frame;
class Range;
class EditorClient;

// make platform-specific and implement - this is temporary placeholder
typedef int Pasteboard;

class Editor {
public:
    Editor(Frame* frame, EditorClient* client);
    ~Editor();

    void cut();
    void copy();
    void paste();
    void performDelete();

private:
    Frame* m_frame;
    EditorClient* m_client;
    
    bool canCopy();
    bool canCut();
    bool canDelete();
    bool canDeleteRange(Range* range);
    bool canPaste();
    bool canSmartCopyOrDelete();
    bool isSelectionRichlyEditable();
    Range* selectedRange();
    bool shouldDeleteRange(Range* range);
    bool tryDHTMLCopy();
    bool tryDHTMLCut();
    bool tryDHTMLPaste();
    void deleteSelection();
    void deleteSelectionWithSmartDelete(bool enabled);
    void pasteAsPlainTextWithPasteboard(Pasteboard pasteboard);
    void pasteWithPasteboard(Pasteboard pasteboard, bool allowPlainText);
    void writeSelectionToPasteboard(Pasteboard pasteboard);

};

} // namespace WebCore

#endif // editor_h__
