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

#ifndef FrameWin_H
#define FrameWin_H

#include "Frame.h"

namespace WebCore {

class FrameWin : public Frame
{
public:
    FrameWin(Page*, RenderPart*);
    ~FrameWin();

    virtual bool openURL(const KURL&);
    virtual void openURLRequest(const KURL&, const URLArgs&);
    virtual void submitForm(const KURL&, const URLArgs&);

    virtual void setTitle(const String&);

    virtual void urlSelected(const KURL& url, const URLArgs& args);

    virtual ObjectContentType objectContentType(const KURL& url, const QString& mimeType);
    virtual Plugin* createPlugin(const KURL&, const QStringList& paramNames, const QStringList& paramValues, const QString& mimeType);
    virtual Frame* createFrame(const KURL&, const QString& name, RenderPart* renderer, const String& referrer);

    virtual void scheduleClose();

    virtual void unfocusWindow();
    
    virtual void saveDocumentState();
    virtual void restoreDocumentState();
    
    virtual void addMessageToConsole(const String& message,  unsigned int lineNumber, const String& sourceID);

    virtual void runJavaScriptAlert(const String& message);
    virtual bool runJavaScriptConfirm(const String& message);
    virtual bool runJavaScriptPrompt(const String& message, const String& defaultValue, String& result);
    virtual bool locationbarVisible();
    virtual bool menubarVisible();
    virtual bool personalbarVisible();
    virtual bool statusbarVisible();
    virtual bool toolbarVisible();

    virtual void createEmptyDocument();
    virtual RangeImpl* markedTextRange() const;

    virtual QString incomingReferrer() const;
    virtual QString userAgent() const;

    virtual QString mimeTypeForFileName(const QString&) const;

    virtual void markMisspellingsInAdjacentWords(const VisiblePosition&);
    virtual void markMisspellings(const SelectionController&);

    virtual bool lastEventIsMouseUp() const;

    virtual bool passSubframeEventToSubframe(MouseEventWithHitTestResults &);
    virtual bool passWheelEventToChildWidget(NodeImpl*);
    
    virtual void clearRecordedFormValues();
    virtual void recordFormValue(const QString& name, const QString& value, HTMLFormElementImpl*);

    virtual QString overrideMediaType() const;

    virtual KJS::Bindings::Instance* getEmbedInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getObjectInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getAppletInstanceForWidget(Widget*);
    
    virtual void registerCommandForUndo(const EditCommandPtr&);
    virtual void registerCommandForRedo(const EditCommandPtr&);
    virtual void clearUndoRedoOperations();
    virtual void issueUndoCommand();
    virtual void issueRedoCommand();
    virtual void issueCutCommand();
    virtual void issueCopyCommand();
    virtual void issuePasteCommand();
    virtual void issuePasteAndMatchStyleCommand();
    virtual void issueTransposeCommand();
    virtual void respondToChangedSelection(const SelectionController& oldSelection, bool closeTyping);
    virtual void respondToChangedContents();
    virtual bool shouldChangeSelection(const SelectionController& oldSelection, const SelectionController& newSelection, EAffinity affinity, bool stillSelecting) const;
    virtual void partClearedInBegin();

    virtual bool canGoBackOrForward(int distance) const;
    virtual void handledOnloadEvents();

    virtual bool canPaste() const;
    virtual bool canRedo() const;
    virtual bool canUndo() const;
    virtual void print();

protected:
    virtual String generateFrameName();
private:
    virtual bool passMouseDownEventToWidget(Widget*);
};

inline FrameWin* Win(Frame* frame) { return static_cast<FrameWin*>(frame); }
inline const FrameWin* Win(const Frame* frame) { return static_cast<const FrameWin*>(frame); }

}

#endif
