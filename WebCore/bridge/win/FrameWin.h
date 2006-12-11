/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
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

#ifndef FrameWin_h
#define FrameWin_h

#include "Frame.h"

namespace WebCore {

    class FormData;
    class FrameLoadRequest;
    class ResourceRequest;
    struct WindowFeatures;

    class FrameWinClient {
    public:
        virtual void createNewWindow(const ResourceRequest&, const WindowFeatures&, Frame*& newFrame) = 0;
        virtual void openURL(const DeprecatedString&, bool lockHistory) = 0;
        virtual void submitForm(const String& method, const KURL&, const FormData*) = 0;
        virtual void setTitle(const String& title) = 0;
        virtual void setStatusText(const String& statusText) = 0;
    };

    class FrameWin : public Frame {
    public:
        FrameWin(Page*, HTMLFrameOwnerElement*, FrameWinClient*);
        virtual ~FrameWin();

        virtual KJS::Bindings::Instance* getEmbedInstanceForWidget(Widget*);
        virtual KJS::Bindings::Instance* getObjectInstanceForWidget(Widget*);
        virtual KJS::Bindings::Instance* getAppletInstanceForWidget(Widget*);
        virtual KJS::Bindings::RootObject* bindingRootObject();

        virtual void runJavaScriptAlert(const String& message);
        virtual bool runJavaScriptConfirm(const String& message);
        virtual bool runJavaScriptPrompt(const String& message, const String& defaultValue, String& result);
        virtual bool shouldInterruptJavaScript();
        virtual void scheduleClose();
        virtual void focusWindow();
        virtual void unfocusWindow();
        virtual void print();

        virtual Range* markedTextRange() const;
        virtual void issueCutCommand();
        virtual void issueCopyCommand();
        virtual void issuePasteCommand();
        virtual void issuePasteAndMatchStyleCommand();
        virtual void issueTransposeCommand();
        virtual void respondToChangedSelection(const Selection& oldSelection, bool closeTyping);
        virtual bool shouldChangeSelection(const Selection& oldSelection, const Selection& newSelection, EAffinity affinity, bool stillSelecting) const;

        virtual String mimeTypeForFileName(const String&) const;

        virtual bool keyPress(const PlatformKeyboardEvent&);

        FrameWinClient* client() const;

    protected:
        virtual bool isLoadTypeReload();

    private:
        virtual bool passMouseDownEventToWidget(Widget*);

        FrameWinClient* m_client;
    };

    inline FrameWin* Win(Frame* frame) { return static_cast<FrameWin*>(frame); }
    inline const FrameWin* Win(const Frame* frame) { return static_cast<const FrameWin*>(frame); }

} // namespace WebCore

#endif // FrameWin_h
