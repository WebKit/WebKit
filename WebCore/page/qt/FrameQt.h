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

#ifndef FrameQt_h
#define FrameQt_h

#include "Frame.h"
#include "WindowFeatures.h"

class QWidget;
class QPaintEvent;

namespace KJS {
    class Interpreter;
    namespace Bindings {
        class Instance;
        class RootObject;
    }
}

namespace WebCore {

class EditorClient;

class FrameQt : public Frame {
public:
    FrameQt(Page*, HTMLFrameOwnerElement*, FrameLoaderClient*);
    virtual ~FrameQt();

    virtual KJS::Bindings::Instance* getEmbedInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getObjectInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getAppletInstanceForWidget(Widget*);
    virtual KJS::Bindings::RootObject* bindingRootObject();

    PassRefPtr<KJS::Bindings::RootObject> createRootObject(void* nativeHandle, PassRefPtr<KJS::Interpreter>);

    //should be in Chrome
    virtual void runJavaScriptAlert(const String& message);
    virtual bool runJavaScriptConfirm(const String& message);
    virtual bool runJavaScriptPrompt(const String& message, const String& defaultValue,
                                     String& result);
    virtual bool shouldInterruptJavaScript();
    virtual void focusWindow();
    virtual void unfocusWindow();
    virtual void print();

    //should be in Editor
    virtual Range* markedTextRange() const;
    virtual void issueCutCommand();
    virtual void issueCopyCommand();
    virtual void issuePasteCommand();
    virtual void issuePasteAndMatchStyleCommand();
    virtual void issueTransposeCommand();
    virtual void respondToChangedSelection(const Selection& oldSelection, bool closeTyping);
    virtual bool shouldChangeSelection(const Selection& oldSelection, const Selection& newSelection,
                                       EAffinity, bool stillSelecting) const;

    //should be somewhere in platform
    virtual String mimeTypeForFileName(const String&) const;

    bool keyEvent(const PlatformKeyboardEvent& keyEvent);

    void setFrameGeometry(const IntRect&);

    void createNewWindow(const FrameLoadRequest&, const WindowFeatures&, Frame*&);
    void goBackOrForward(int);

    KURL historyURL(int distance);
private:
    void init();

    virtual bool isLoadTypeReload();
    virtual bool passMouseDownEventToWidget(Widget*);

    bool m_beginCalled : 1;    

    RefPtr<KJS::Bindings::RootObject> m_bindingRoot; // The root object used for objects bound outside the context of a plugin.
    Vector<RefPtr<KJS::Bindings::RootObject> > m_rootObjects;

};

inline FrameQt* QtFrame(Frame* frame) { return static_cast<FrameQt*>(frame); }
inline const FrameQt* QtFrame(const Frame* frame) { return static_cast<const FrameQt*>(frame); }

}

#endif 
// vim: ts=4 sw=4 et
