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

#include "Platform/chromium/public/WebRect.h"
#include "WebKit/chromium/public/WebAccessibilityNotification.h"
#include "WebKit/chromium/public/WebDragOperation.h"
#include "WebKit/chromium/public/WebNavigationPolicy.h"

namespace WebKit {
class WebAccessibilityObject;
class WebDragData;
class WebFrame;
class WebImage;
struct WebPoint;
struct WebSize;
}

namespace WebTestRunner {

class WebTestDelegate;
class WebTestInterfaces;

class WebTestProxyBase {
public:
    void setInterfaces(WebTestInterfaces*);
    void setDelegate(WebTestDelegate*);

    void setPaintRect(const WebKit::WebRect&);
    WebKit::WebRect paintRect() const;

protected:
    WebTestProxyBase();
    ~WebTestProxyBase();

    void didInvalidateRect(const WebKit::WebRect&);
    void didScrollRect(int, int, const WebKit::WebRect&);
    void scheduleComposite();
    void scheduleAnimation();
    void setWindowRect(const WebKit::WebRect&);
    void show(WebKit::WebNavigationPolicy);
    void didAutoResize(const WebKit::WebSize&);
    void postAccessibilityNotification(const WebKit::WebAccessibilityObject&, WebKit::WebAccessibilityNotification);
    void startDragging(WebKit::WebFrame*, const WebKit::WebDragData&, WebKit::WebDragOperationsMask, const WebKit::WebImage&, const WebKit::WebPoint&);

private:
    WebTestInterfaces* m_testInterfaces;
    WebTestDelegate* m_delegate;

    WebKit::WebRect m_paintRect;
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

    virtual void didInvalidateRect(const WebKit::WebRect& rect)
    {
        WebTestProxyBase::didInvalidateRect(rect);
        WebViewClientImpl::didInvalidateRect(rect);
    }
    virtual void didScrollRect(int dx, int dy, const WebKit::WebRect& clipRect)
    {
        WebTestProxyBase::didScrollRect(dx, dy, clipRect);
        WebViewClientImpl::didScrollRect(dx, dy, clipRect);
    }
    virtual void scheduleComposite()
    {
        WebTestProxyBase::scheduleComposite();
        WebViewClientImpl::scheduleComposite();
    }
    virtual void scheduleAnimation()
    {
        WebTestProxyBase::scheduleAnimation();
        WebViewClientImpl::scheduleAnimation();
    }
    virtual void setWindowRect(const WebKit::WebRect& rect)
    {
        WebTestProxyBase::setWindowRect(rect);
        WebViewClientImpl::setWindowRect(rect);
    }
    virtual void show(WebKit::WebNavigationPolicy policy)
    {
        WebTestProxyBase::show(policy);
        WebViewClientImpl::show(policy);
    }
    virtual void didAutoResize(const WebKit::WebSize& newSize)
    {
        WebTestProxyBase::didAutoResize(newSize);
        WebViewClientImpl::didAutoResize(newSize);
    }
    virtual void postAccessibilityNotification(const WebKit::WebAccessibilityObject& object, WebKit::WebAccessibilityNotification notification)
    {
        WebTestProxyBase::postAccessibilityNotification(object, notification);
        WebViewClientImpl::postAccessibilityNotification(object, notification);
    }
    virtual void startDragging(WebKit::WebFrame* frame, const WebKit::WebDragData& data, WebKit::WebDragOperationsMask mask, const WebKit::WebImage& image, const WebKit::WebPoint& point)
    {
        WebTestProxyBase::startDragging(frame, data, mask, image, point);
        WebViewClientImpl::startDragging(frame, data, mask, image, point);
    }
};

}

#endif // WebTestProxy_h
