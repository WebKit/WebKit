/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
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

#ifndef FrameGdk_H_
#define FrameGdk_H_

#include "Frame.h"
#include "EditorClient.h"
#include "ResourceHandleClient.h"
#include <gdk/gdk.h>

namespace WebCore {

class Element;
class FrameGdk;
class FormData;

class FrameGdkClient {
public:
    virtual ~FrameGdkClient() {};

    virtual void setFrame(const FrameGdk*) = 0;

    virtual void openURL(const KURL&) = 0;
    virtual void submitForm(const String& method, const KURL&, const FormData*) = 0;
};

class FrameGdkClientDefault : public FrameGdkClient,
                              public  ResourceHandleClient 
{
public:

    FrameGdkClientDefault();
    virtual ~FrameGdkClientDefault();

    // FrameGdkClient
    virtual void setFrame(const FrameGdk*);
    virtual void openURL(const KURL&);
    virtual void submitForm(const String& method, const KURL&, const FormData*);

    // ResourceHandleClient
    virtual void receivedResponse(ResourceHandle*, PlatformResponse);
    virtual void didReceiveData(ResourceHandle*, const char*, int);
    virtual void receivedAllData(ResourceHandle*, PlatformData);

private:
    FrameGdk* m_frame;
    bool m_beginCalled : 1;

};

class FrameGdk : public Frame {
public:
    FrameGdk(Page*, Element*, PassRefPtr<EditorClient>);
    FrameGdk(GdkDrawable*);
    virtual ~FrameGdk();

    void handleGdkEvent(GdkEvent*);
    virtual void submitForm(const FrameLoadRequest&, Event*);
    virtual void urlSelected(const FrameLoadRequest&, Event*);

    virtual void setTitle(const String&);

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

    virtual bool passSubframeEventToSubframe(MouseEventWithHitTestResults &, Frame* subframe = 0);
    virtual bool passWheelEventToChildWidget(Node*);

    virtual String overrideMediaType() const;

    virtual KJS::Bindings::Instance* getEmbedInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getObjectInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getAppletInstanceForWidget(Widget*);
    virtual KJS::Bindings::RootObject* bindingRootObject();

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
    virtual bool shouldChangeSelection(const Selection& oldSelection, const Selection& newSelection, EAffinity affinity, bool stillSelecting) const;
    bool shouldChangeSelection(const Selection&) const;
    virtual void partClearedInBegin();

    virtual bool canGoBackOrForward(int distance) const;
    virtual void goBackOrForward(int distance);
    virtual int getHistoryLength();
    virtual KURL historyURL(int distance);

    virtual void handledOnloadEvents();

    virtual bool canPaste() const;
    virtual bool canRedo() const;
    virtual bool canUndo() const;
    virtual void print();
    virtual bool shouldInterruptJavaScript();

    bool keyPress(const PlatformKeyboardEvent&);
    virtual KURL originalRequestURL() const;

    IntRect frameGeometry() const;
    void setFrameGeometry(const IntRect&);
    virtual Frame* createFrame(const KURL&, const String& name, Element* ownerElement, const String& referrer);
    Widget* createJavaAppletWidget(const IntSize&, Element*, const HashMap<String, String>&);

    FrameGdkClient* client() const { return m_client; }

private:
    virtual bool isLoadTypeReload();
    virtual bool passMouseDownEventToWidget(Widget*);

    FrameGdkClient* m_client;
    GdkDrawable* m_drawable;
};

inline FrameGdk* Win(Frame* frame) { return static_cast<FrameGdk*>(frame); }
inline const FrameGdk* Win(const Frame* frame) { return static_cast<const FrameGdk*>(frame); }

}

#endif
