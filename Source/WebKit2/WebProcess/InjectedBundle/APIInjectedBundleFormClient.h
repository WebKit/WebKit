/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef APIInjectedBundleFormClient_h
#define APIInjectedBundleFormClient_h

#include <wtf/Forward.h>
#include <wtf/Vector.h>

namespace WebCore {
class Element;
class HTMLFormElement;
class HTMLInputElement;
class HTMLTextAreaElement;
}

namespace WebKit {
class WebFrame;
class WebPage;
}

namespace API {

class Object;

namespace InjectedBundle {

class FormClient {
public:
    virtual ~FormClient() { }

    virtual void didFocusTextField(WebKit::WebPage*, WebCore::HTMLInputElement*, WebKit::WebFrame*) { }
    virtual void textFieldDidBeginEditing(WebKit::WebPage*, WebCore::HTMLInputElement*, WebKit::WebFrame*) { }
    virtual void textFieldDidEndEditing(WebKit::WebPage*, WebCore::HTMLInputElement*, WebKit::WebFrame*) { }
    virtual void textDidChangeInTextField(WebKit::WebPage*, WebCore::HTMLInputElement*, WebKit::WebFrame*) { }
    virtual void textDidChangeInTextArea(WebKit::WebPage*, WebCore::HTMLTextAreaElement*, WebKit::WebFrame*) { }

    enum class InputFieldAction {
        MoveUp,
        MoveDown,
        Cancel,
        InsertTab,
        InsertBacktab,
        InsertNewline,
        InsertDelete,
    };

    virtual bool shouldPerformActionInTextField(WebKit::WebPage*, WebCore::HTMLInputElement*, InputFieldAction, WebKit::WebFrame*) { return false; }
    virtual void willSubmitForm(WebKit::WebPage*, WebCore::HTMLFormElement*, WebKit::WebFrame*, WebKit::WebFrame* sourceFrame, const Vector<std::pair<WTF::String, WTF::String>>&, RefPtr<API::Object>& userData) { UNUSED_PARAM(userData); }
    virtual void willSendSubmitEvent(WebKit::WebPage*, WebCore::HTMLFormElement*, WebKit::WebFrame*, WebKit::WebFrame* sourceFrame, const Vector<std::pair<WTF::String, WTF::String>>&) { }
    virtual void didAssociateFormControls(WebKit::WebPage*, const Vector<RefPtr<WebCore::Element>>&) { }
    virtual bool shouldNotifyOnFormChanges(WebKit::WebPage*) { return false; }
    virtual void willBeginInputSession(WebKit::WebPage*, WebCore::Element*, WebKit::WebFrame*, RefPtr<API::Object>& userData) { UNUSED_PARAM(userData); }
};

} // namespace InjectedBundle

} // namespace API

#endif // APIInjectedBundleFormClient_h
