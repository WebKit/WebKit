/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef InjectedBundlePageFormClient_h
#define InjectedBundlePageFormClient_h

#include "APIClient.h"
#include "APIInjectedBundleFormClient.h"
#include "WKBundlePageFormClient.h"

namespace API {

template<> struct ClientTraits<WKBundlePageFormClientBase> {
    typedef std::tuple<WKBundlePageFormClientV0, WKBundlePageFormClientV1, WKBundlePageFormClientV2, WKBundlePageFormClientV3> Versions;
};
}

namespace WebKit {

class InjectedBundlePageFormClient : public API::Client<WKBundlePageFormClientBase>, public API::InjectedBundle::FormClient {
public:
    explicit InjectedBundlePageFormClient(const WKBundlePageFormClientBase*);

    void didFocusTextField(WebPage*, WebCore::HTMLInputElement*, WebFrame*) override;
    void textFieldDidBeginEditing(WebPage*, WebCore::HTMLInputElement*, WebFrame*) override;
    void textFieldDidEndEditing(WebPage*, WebCore::HTMLInputElement*, WebFrame*) override;
    void textDidChangeInTextField(WebPage*, WebCore::HTMLInputElement*, WebFrame*, bool initiatedByUserTyping) override;
    void textDidChangeInTextArea(WebPage*, WebCore::HTMLTextAreaElement*, WebFrame*) override;
    bool shouldPerformActionInTextField(WebPage*, WebCore::HTMLInputElement*, InputFieldAction, WebFrame*) override;    
    void willSubmitForm(WebPage*, WebCore::HTMLFormElement*, WebFrame*, WebFrame* sourceFrame, const Vector<std::pair<String, String>>&, RefPtr<API::Object>& userData) override;
    void willSendSubmitEvent(WebPage*, WebCore::HTMLFormElement*, WebFrame*, WebFrame* sourceFrame, const Vector<std::pair<String, String>>&) override;
    void didAssociateFormControls(WebPage*, const Vector<RefPtr<WebCore::Element>>&, WebFrame*) override;
    bool shouldNotifyOnFormChanges(WebPage*) override;
};

} // namespace WebKit

#endif // InjectedBundlePageFormClient_h
