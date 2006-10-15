/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef FrameQt_H
#define FrameQt_H

#include "Frame.h"
#include "FrameQtClient.h"

class QWidget;
class QPaintEvent;

namespace WebCore {

class FrameQt : public Frame {
public:
    FrameQt(Page*, Element*, FrameQtClient*);
    virtual ~FrameQt();

    virtual bool openURL(const KURL&);
    virtual void openURLRequest(const FrameLoadRequest&);
    virtual void submitForm(const FrameLoadRequest&);
    virtual void urlSelected(const FrameLoadRequest&);

    virtual void setTitle(const String&);

    virtual ObjectContentType objectContentType(const KURL&, const String& mimeType);
    virtual Plugin* createPlugin(Element*, const KURL&, const Vector<String>&, const Vector<String>&, const String&);
    virtual Frame* createFrame(const KURL&, const String& name, Element*, const String& referrer);
 
    virtual void scheduleClose();

    virtual void unfocusWindow();

    virtual void focusWindow();

    virtual void saveDocumentState();
    virtual void restoreDocumentState();

    virtual void addMessageToConsole(const String& message, unsigned lineNumber, const String& sourceID);

    virtual void runJavaScriptAlert(const String& message);
    virtual bool runJavaScriptConfirm(const String& message);
    virtual bool runJavaScriptPrompt(const String& message, const String& defaultValue, String& result);
    virtual bool locationbarVisible();
    virtual bool menubarVisible();
    virtual bool personalbarVisible();
    virtual bool statusbarVisible();
    virtual bool toolbarVisible();

    virtual void createEmptyDocument();
    virtual Range* markedTextRange() const;

    virtual String incomingReferrer() const;
    virtual String userAgent() const;

    virtual String mimeTypeForFileName(const String&) const;

    virtual void markMisspellingsInAdjacentWords(const VisiblePosition&);
    virtual void markMisspellings(const Selection&);

    virtual bool lastEventIsMouseUp() const;

    virtual bool passSubframeEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe = 0);
    virtual bool passWheelEventToChildWidget(Node*);

    virtual String overrideMediaType() const;

    virtual KJS::Bindings::Instance* getEmbedInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getObjectInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getAppletInstanceForWidget(Widget*);
    virtual KJS::Bindings::RootObject* bindingRootObject();
    void addPluginRootObject(KJS::Bindings::RootObject*);

    virtual Widget* createJavaAppletWidget(const IntSize&, Element*, const HashMap<String, String>& args);

    virtual void registerCommandForUndo(PassRefPtr<EditCommand>);
    virtual void registerCommandForRedo(PassRefPtr<EditCommand>);
    virtual void clearUndoRedoOperations();
    virtual void issueUndoCommand();
    virtual void issueRedoCommand();
    virtual void issueCutCommand();
    virtual void issueCopyCommand();
    virtual void issuePasteCommand();
    virtual void issuePasteAndMatchStyleCommand();
    virtual void issueTransposeCommand();
    virtual void respondToChangedSelection(const Selection& oldSelection, bool closeTyping);
    virtual void respondToChangedContents(const Selection& endingSelection);
    virtual bool shouldChangeSelection(const Selection& oldSelection, const Selection& newSelection, EAffinity, bool stillSelecting) const;
    virtual void partClearedInBegin();

    virtual bool canGoBackOrForward(int distance) const;
    virtual void handledOnloadEvents();

    virtual bool canPaste() const;
    virtual bool canRedo() const;
    virtual bool canUndo() const;
    virtual void print();
    virtual bool shouldInterruptJavaScript();
    virtual KURL originalRequestURL() const;

    bool keyEvent(const PlatformKeyboardEvent& keyEvent);

    void setFrameGeometry(const IntRect&);

    virtual void tokenizerProcessedData();

private:
    void init();

    virtual bool isLoadTypeReload();
    virtual bool passMouseDownEventToWidget(Widget*);

    FrameQtClient* m_client;
    bool m_beginCalled : 1;    

    KJS::Bindings::RootObject* m_bindingRoot;  // The root object used for objects
                                               // bound outside the context of a plugin.

    Vector<KJS::Bindings::RootObject*> m_rootObjects;

};

}

#endif 
// vim: ts=4 sw=4 et
