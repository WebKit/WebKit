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
#include "WebKit/chromium/public/WebEditingAction.h"
#include "WebKit/chromium/public/WebNavigationPolicy.h"
#include "WebKit/chromium/public/WebTextAffinity.h"
#include "WebKit/chromium/public/WebTextDirection.h"

namespace WebKit {
class WebAccessibilityObject;
class WebDragData;
class WebFrame;
class WebImage;
class WebIntentRequest;
class WebIntentServiceInfo;
class WebNode;
class WebRange;
class WebSecurityOrigin;
class WebSerializedScriptValue;
class WebString;
class WebURL;
class WebURLRequest;
class WebView;
struct WebPoint;
struct WebSize;
struct WebURLError;
struct WebWindowFeatures;
}

namespace WebTestRunner {

class WebTestDelegate;
class WebTestInterfaces;
class WebTestRunner;

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
    bool shouldBeginEditing(const WebKit::WebRange&);
    bool shouldEndEditing(const WebKit::WebRange&);
    bool shouldInsertNode(const WebKit::WebNode&, const WebKit::WebRange&, WebKit::WebEditingAction);
    bool shouldInsertText(const WebKit::WebString& text, const WebKit::WebRange&, WebKit::WebEditingAction);
    bool shouldChangeSelectedRange(const WebKit::WebRange& fromRange, const WebKit::WebRange& toRange, WebKit::WebTextAffinity, bool stillSelecting);
    bool shouldDeleteRange(const WebKit::WebRange&);
    bool shouldApplyStyle(const WebKit::WebString& style, const WebKit::WebRange&);
    void didBeginEditing();
    void didChangeSelection(bool isEmptySelection);
    void didChangeContents();
    void didEndEditing();
    void registerIntentService(WebKit::WebFrame*, const WebKit::WebIntentServiceInfo&);
    void dispatchIntent(WebKit::WebFrame* source, const WebKit::WebIntentRequest&);
    WebKit::WebView* createView(WebKit::WebFrame* creator, const WebKit::WebURLRequest&, const WebKit::WebWindowFeatures&, const WebKit::WebString& frameName, WebKit::WebNavigationPolicy);
    void willPerformClientRedirect(WebKit::WebFrame*, const WebKit::WebURL& from, const WebKit::WebURL& to, double interval, double fire_time);
    void didCancelClientRedirect(WebKit::WebFrame*);
    void didStartProvisionalLoad(WebKit::WebFrame*);
    void didReceiveServerRedirectForProvisionalLoad(WebKit::WebFrame*);
    void didFailProvisionalLoad(WebKit::WebFrame*, const WebKit::WebURLError&);
    void didCommitProvisionalLoad(WebKit::WebFrame*, bool isNewNavigation);
    void didReceiveTitle(WebKit::WebFrame*, const WebKit::WebString& title, WebKit::WebTextDirection);
    void didFinishDocumentLoad(WebKit::WebFrame*);
    void didHandleOnloadEvents(WebKit::WebFrame*);
    void didFailLoad(WebKit::WebFrame*, const WebKit::WebURLError&);
    void didFinishLoad(WebKit::WebFrame*);
    void didChangeLocationWithinPage(WebKit::WebFrame*);
    void didDisplayInsecureContent(WebKit::WebFrame*);
    void didRunInsecureContent(WebKit::WebFrame*, const WebKit::WebSecurityOrigin&, const WebKit::WebURL& insecureURL);
    void didDetectXSS(WebKit::WebFrame*, const WebKit::WebURL& insecureURL, bool didBlockEntirePage);

private:
    WebTestInterfaces* m_testInterfaces;
    WebTestDelegate* m_delegate;

    WebKit::WebRect m_paintRect;
};

// Use this template to inject methods into your WebViewClient/WebFrameClient
// implementation required for the running layout tests.
template<class Base, typename T>
class WebTestProxy : public Base, public WebTestProxyBase {
public:
    explicit WebTestProxy(T t)
        : Base(t)
    {
    }

    virtual ~WebTestProxy() { }

    // WebViewClient implementation.
    virtual void didInvalidateRect(const WebKit::WebRect& rect)
    {
        WebTestProxyBase::didInvalidateRect(rect);
        Base::didInvalidateRect(rect);
    }
    virtual void didScrollRect(int dx, int dy, const WebKit::WebRect& clipRect)
    {
        WebTestProxyBase::didScrollRect(dx, dy, clipRect);
        Base::didScrollRect(dx, dy, clipRect);
    }
    virtual void scheduleComposite()
    {
        WebTestProxyBase::scheduleComposite();
        Base::scheduleComposite();
    }
    virtual void scheduleAnimation()
    {
        WebTestProxyBase::scheduleAnimation();
        Base::scheduleAnimation();
    }
    virtual void setWindowRect(const WebKit::WebRect& rect)
    {
        WebTestProxyBase::setWindowRect(rect);
        Base::setWindowRect(rect);
    }
    virtual void show(WebKit::WebNavigationPolicy policy)
    {
        WebTestProxyBase::show(policy);
        Base::show(policy);
    }
    virtual void didAutoResize(const WebKit::WebSize& newSize)
    {
        WebTestProxyBase::didAutoResize(newSize);
        Base::didAutoResize(newSize);
    }
    virtual void postAccessibilityNotification(const WebKit::WebAccessibilityObject& object, WebKit::WebAccessibilityNotification notification)
    {
        WebTestProxyBase::postAccessibilityNotification(object, notification);
        Base::postAccessibilityNotification(object, notification);
    }
    virtual void startDragging(WebKit::WebFrame* frame, const WebKit::WebDragData& data, WebKit::WebDragOperationsMask mask, const WebKit::WebImage& image, const WebKit::WebPoint& point)
    {
        WebTestProxyBase::startDragging(frame, data, mask, image, point);
        Base::startDragging(frame, data, mask, image, point);
    }
    virtual bool shouldBeginEditing(const WebKit::WebRange& range)
    {
        WebTestProxyBase::shouldBeginEditing(range);
        return Base::shouldBeginEditing(range);
    }
    virtual bool shouldEndEditing(const WebKit::WebRange& range)
    {
        WebTestProxyBase::shouldEndEditing(range);
        return Base::shouldEndEditing(range);
    }
    virtual bool shouldInsertNode(const WebKit::WebNode& node, const WebKit::WebRange& range, WebKit::WebEditingAction action)
    {
        WebTestProxyBase::shouldInsertNode(node, range, action);
        return Base::shouldInsertNode(node, range, action);
    }
    virtual bool shouldInsertText(const WebKit::WebString& text, const WebKit::WebRange& range, WebKit::WebEditingAction action)
    {
        WebTestProxyBase::shouldInsertText(text, range, action);
        return Base::shouldInsertText(text, range, action);
    }
    virtual bool shouldChangeSelectedRange(const WebKit::WebRange& fromRange, const WebKit::WebRange& toRange, WebKit::WebTextAffinity affinity, bool stillSelecting)
    {
        WebTestProxyBase::shouldChangeSelectedRange(fromRange, toRange, affinity, stillSelecting);
        return Base::shouldChangeSelectedRange(fromRange, toRange, affinity, stillSelecting);
    }
    virtual bool shouldDeleteRange(const WebKit::WebRange& range)
    {
        WebTestProxyBase::shouldDeleteRange(range);
        return Base::shouldDeleteRange(range);
    }
    virtual bool shouldApplyStyle(const WebKit::WebString& style, const WebKit::WebRange& range)
    {
        WebTestProxyBase::shouldApplyStyle(style, range);
        return Base::shouldApplyStyle(style, range);
    }
    virtual void didBeginEditing()
    {
        WebTestProxyBase::didBeginEditing();
        Base::didBeginEditing();
    }
    virtual void didChangeSelection(bool isEmptySelection)
    {
        WebTestProxyBase::didChangeSelection(isEmptySelection);
        Base::didChangeSelection(isEmptySelection);
    }
    virtual void didChangeContents()
    {
        WebTestProxyBase::didChangeContents();
        Base::didChangeContents();
    }
    virtual void didEndEditing()
    {
        WebTestProxyBase::didEndEditing();
        Base::didEndEditing();
    }
    virtual void registerIntentService(WebKit::WebFrame* frame, const WebKit::WebIntentServiceInfo& service)
    {
        WebTestProxyBase::registerIntentService(frame, service);
        Base::registerIntentService(frame, service);
    }
    virtual void dispatchIntent(WebKit::WebFrame* source, const WebKit::WebIntentRequest& request)
    {
        WebTestProxyBase::dispatchIntent(source, request);
        Base::dispatchIntent(source, request);
    }
    virtual WebKit::WebView* createView(WebKit::WebFrame* creator, const WebKit::WebURLRequest& request, const WebKit::WebWindowFeatures& features, const WebKit::WebString& frameName, WebKit::WebNavigationPolicy policy)
    {
        WebTestProxyBase::createView(creator, request, features, frameName, policy);
        return Base::createView(creator, request, features, frameName, policy);
    }

    // WebFrameClient implementation.
    virtual void willPerformClientRedirect(WebKit::WebFrame* frame, const WebKit::WebURL& from, const WebKit::WebURL& to, double interval, double fireTime)
    {
        WebTestProxyBase::willPerformClientRedirect(frame, from, to, interval, fireTime);
        Base::willPerformClientRedirect(frame, from, to, interval, fireTime);
    }
    virtual void didCancelClientRedirect(WebKit::WebFrame* frame)
    {
        WebTestProxyBase::didCancelClientRedirect(frame);
        Base::didCancelClientRedirect(frame);
    }
    virtual void didStartProvisionalLoad(WebKit::WebFrame* frame)
    {
        WebTestProxyBase::didStartProvisionalLoad(frame);
        Base::didStartProvisionalLoad(frame);
    }
    virtual void didReceiveServerRedirectForProvisionalLoad(WebKit::WebFrame* frame)
    {
        WebTestProxyBase::didReceiveServerRedirectForProvisionalLoad(frame);
        Base::didReceiveServerRedirectForProvisionalLoad(frame);
    }
    virtual void didFailProvisionalLoad(WebKit::WebFrame* frame, const WebKit::WebURLError& error)
    {
        WebTestProxyBase::didFailProvisionalLoad(frame, error);
        Base::didFailProvisionalLoad(frame, error);
    }
    virtual void didCommitProvisionalLoad(WebKit::WebFrame* frame, bool isNewNavigation)
    {
        WebTestProxyBase::didCommitProvisionalLoad(frame, isNewNavigation);
        Base::didCommitProvisionalLoad(frame, isNewNavigation);
    }
    virtual void didReceiveTitle(WebKit::WebFrame* frame, const WebKit::WebString& title, WebKit::WebTextDirection direction)
    {
        WebTestProxyBase::didReceiveTitle(frame, title, direction);
        Base::didReceiveTitle(frame, title, direction);
    }
    virtual void didFinishDocumentLoad(WebKit::WebFrame* frame)
    {
        WebTestProxyBase::didFinishDocumentLoad(frame);
        Base::didFinishDocumentLoad(frame);
    }
    virtual void didHandleOnloadEvents(WebKit::WebFrame* frame)
    {
        WebTestProxyBase::didHandleOnloadEvents(frame);
        Base::didHandleOnloadEvents(frame);
    }
    virtual void didFailLoad(WebKit::WebFrame* frame, const WebKit::WebURLError& error)
    {
        WebTestProxyBase::didFailLoad(frame, error);
        Base::didFailLoad(frame, error);
    }
    virtual void didFinishLoad(WebKit::WebFrame* frame)
    {
        WebTestProxyBase::didFinishLoad(frame);
        Base::didFinishLoad(frame);
    }
    virtual void didChangeLocationWithinPage(WebKit::WebFrame* frame)
    {
        WebTestProxyBase::didChangeLocationWithinPage(frame);
        Base::didChangeLocationWithinPage(frame);
    }
    virtual void didDisplayInsecureContent(WebKit::WebFrame* frame)
    {
        WebTestProxyBase::didDisplayInsecureContent(frame);
        Base::didDisplayInsecureContent(frame);
    }
    virtual void didRunInsecureContent(WebKit::WebFrame* frame, const WebKit::WebSecurityOrigin& origin, const WebKit::WebURL& insecureURL)
    {
        WebTestProxyBase::didRunInsecureContent(frame, origin, insecureURL);
        Base::didRunInsecureContent(frame, origin, insecureURL);
    }
    virtual void didDetectXSS(WebKit::WebFrame* frame, const WebKit::WebURL& insecureURL, bool didBlockEntirePage)
    {
        WebTestProxyBase::didDetectXSS(frame, insecureURL, didBlockEntirePage);
        Base::didDetectXSS(frame, insecureURL, didBlockEntirePage);
    }
};

}

#endif // WebTestProxy_h
