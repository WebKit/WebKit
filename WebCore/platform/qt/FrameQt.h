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

#ifndef FrameQt_H_
#define FrameQt_H_

#include "Frame.h"
#include "ResourceLoaderClient.h"

class QWidget;
class QPaintEvent;

namespace WebCore {

class FrameQtClient {
public:
    virtual ~FrameQtClient();

    virtual void openURL(const DeprecatedString&) = 0;
    virtual void submitForm(const String& method, const KURL&, const FormData*) = 0;
};

class FrameQt : public Frame,
                public ResourceLoaderClient {
public:
    FrameQt(QWidget* parent);
    FrameQt();
    ~FrameQt();

    virtual bool openURL(const KURL&);
    virtual void openURLRequest(const ResourceRequest&);
    virtual void submitForm(const ResourceRequest&);
    virtual void urlSelected(const ResourceRequest&);

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
    virtual void markMisspellings(const SelectionController&);

    virtual bool lastEventIsMouseUp() const;

    virtual bool passSubframeEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe = 0);
    virtual bool passWheelEventToChildWidget(Node*);

    virtual String overrideMediaType() const;

    virtual KJS::Bindings::Instance* getEmbedInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getObjectInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getAppletInstanceForWidget(Widget*);

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
    virtual void respondToChangedSelection(const SelectionController& oldSelection, bool closeTyping);
    virtual void respondToChangedContents();
    virtual bool shouldChangeSelection(const SelectionController& oldSelection, const SelectionController& newSelection, EAffinity, bool stillSelecting) const;
    virtual void partClearedInBegin();

    virtual bool canGoBackOrForward(int distance) const;
    virtual void handledOnloadEvents();

    virtual bool canPaste() const;
    virtual bool canRedo() const;
    virtual bool canUndo() const;
    virtual void print();
    virtual bool shouldInterruptJavaScript();

    bool keyEvent(const PlatformKeyboardEvent& keyEvent);

    virtual void receivedResponse(ResourceLoader*, PlatformResponse);
    virtual void receivedData(ResourceLoader*, const char*, int);
    virtual void receivedAllData(ResourceLoader*, PlatformData);

    void setFrameGeometry(const IntRect&);

private:
    void init();
    virtual bool passMouseDownEventToWidget(Widget*);

    FrameQtClient* m_client;
    
};

}

#endif

// vim: ts=4 sw=4 et
