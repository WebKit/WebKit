/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebTestProxy_h
#define WebTestProxy_h

#include "WebKit/chromium/public/WebAccessibilityNotification.h"

namespace WebKit {
class WebAccessibilityObject;
}

namespace WebTestRunner {

class WebTestDelegate;
class WebTestInterfaces;

class WebTestProxyBase {
public:
    void setInterfaces(WebTestInterfaces*);
    void setDelegate(WebTestDelegate*);

protected:
    WebTestProxyBase();
    ~WebTestProxyBase();

    void doPostAccessibilityNotification(const WebKit::WebAccessibilityObject&, WebKit::WebAccessibilityNotification);

private:
    WebTestInterfaces* m_testInterfaces;
    WebTestDelegate* m_delegate;
};

// Use this template to inject methods into your WebViewClient implementation
// required for the running layout tests.
template<class WebViewClientImpl, typename T>
class WebTestProxy : public WebViewClientImpl, public WebTestProxyBase {
public:
    explicit WebTestProxy(T t)
        : WebViewClientImpl(t)
    {
    }

    virtual ~WebTestProxy() { }

    virtual void postAccessibilityNotification(const WebKit::WebAccessibilityObject& object, WebKit::WebAccessibilityNotification notification)
    {
        WebTestProxyBase::doPostAccessibilityNotification(object, notification);
        WebViewClientImpl::postAccessibilityNotification(object, notification);
    }
};

}

#endif // WebTestProxy_h
