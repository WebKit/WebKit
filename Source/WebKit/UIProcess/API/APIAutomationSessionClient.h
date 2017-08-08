/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#ifndef APIAutomationSessionClient_h
#define APIAutomationSessionClient_h

#include <wtf/text/WTFString.h>

namespace WebKit {
class WebAutomationSession;
class WebPageProxy;
}

namespace API {

class AutomationSessionClient {
public:
    enum class JavaScriptDialogType {
        Alert,
        Confirm,
        Prompt,
        BeforeUnloadConfirm
    };

    virtual ~AutomationSessionClient() { }

    virtual String sessionIdentifier() const { return String(); }
    virtual void didDisconnectFromRemote(WebKit::WebAutomationSession&) { }
    virtual WebKit::WebPageProxy* didRequestNewWindow(WebKit::WebAutomationSession&) { return nullptr; }
    virtual bool isShowingJavaScriptDialogOnPage(WebKit::WebAutomationSession&, WebKit::WebPageProxy&) { return false; }
    virtual void dismissCurrentJavaScriptDialogOnPage(WebKit::WebAutomationSession&, WebKit::WebPageProxy&) { }
    virtual void acceptCurrentJavaScriptDialogOnPage(WebKit::WebAutomationSession&, WebKit::WebPageProxy&) { }
    virtual String messageOfCurrentJavaScriptDialogOnPage(WebKit::WebAutomationSession&, WebKit::WebPageProxy&) { return String(); }
    virtual void setUserInputForCurrentJavaScriptPromptOnPage(WebKit::WebAutomationSession&, WebKit::WebPageProxy&, const String&) { }
    virtual std::optional<JavaScriptDialogType> typeOfCurrentJavaScriptDialogOnPage(WebKit::WebAutomationSession&, WebKit::WebPageProxy&) { return std::nullopt; }
};

} // namespace API

#endif // APIAutomationSessionClient_h
