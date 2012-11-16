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

#include "config.h"
#include "WebTestProxy.h"

#include "WebAccessibilityController.h"
#include "WebAccessibilityNotification.h"
#include "WebAccessibilityObject.h"
#include "WebElement.h"
#include "WebEventSender.h"
#include "WebNode.h"
#include "WebTestDelegate.h"
#include "WebTestInterfaces.h"
#include "platform/WebCString.h"

using namespace WebKit;
using namespace std;

namespace WebTestRunner {

WebTestProxyBase::WebTestProxyBase()
    : m_testInterfaces(0)
    , m_delegate(0)
{
}

WebTestProxyBase::~WebTestProxyBase()
{
}

void WebTestProxyBase::setInterfaces(WebTestInterfaces* interfaces)
{
    m_testInterfaces = interfaces;
}

void WebTestProxyBase::setDelegate(WebTestDelegate* delegate)
{
    m_delegate = delegate;
}

void WebTestProxyBase::setPaintRect(const WebRect& rect)
{
    m_paintRect = rect;
}

WebRect WebTestProxyBase::paintRect() const
{
    return m_paintRect;
}

void WebTestProxyBase::didInvalidateRect(const WebRect& rect)
{
    // m_paintRect = m_paintRect U rect
    if (rect.isEmpty())
        return;
    if (m_paintRect.isEmpty()) {
        m_paintRect = rect;
        return;
    }
    int left = min(m_paintRect.x, rect.x);
    int top = min(m_paintRect.y, rect.y);
    int right = max(m_paintRect.x + m_paintRect.width, rect.x + rect.width);
    int bottom = max(m_paintRect.y + m_paintRect.height, rect.y + rect.height);
    m_paintRect = WebRect(left, top, right - left, bottom - top);
}

void WebTestProxyBase::didScrollRect(int, int, const WebRect& clipRect)
{
    didInvalidateRect(clipRect);
}

void WebTestProxyBase::scheduleComposite()
{
    m_paintRect = WebRect(0, 0, INT_MAX, INT_MAX);
}

void WebTestProxyBase::scheduleAnimation()
{
    scheduleComposite();
}

void WebTestProxyBase::show(WebNavigationPolicy)
{
    scheduleComposite();
}

void WebTestProxyBase::setWindowRect(const WebRect& rect)
{
    scheduleComposite();
}

void WebTestProxyBase::didAutoResize(const WebSize&)
{
    scheduleComposite();
}

void WebTestProxyBase::postAccessibilityNotification(const WebKit::WebAccessibilityObject& obj, WebKit::WebAccessibilityNotification notification)
{
    if (notification == WebKit::WebAccessibilityNotificationFocusedUIElementChanged)
        m_testInterfaces->accessibilityController()->setFocusedElement(obj);

    const char* notificationName;
    switch (notification) {
    case WebKit::WebAccessibilityNotificationActiveDescendantChanged:
        notificationName = "ActiveDescendantChanged";
        break;
    case WebKit::WebAccessibilityNotificationAutocorrectionOccured:
        notificationName = "AutocorrectionOccured";
        break;
    case WebKit::WebAccessibilityNotificationCheckedStateChanged:
        notificationName = "CheckedStateChanged";
        break;
    case WebKit::WebAccessibilityNotificationChildrenChanged:
        notificationName = "ChildrenChanged";
        break;
    case WebKit::WebAccessibilityNotificationFocusedUIElementChanged:
        notificationName = "FocusedUIElementChanged";
        break;
    case WebKit::WebAccessibilityNotificationLayoutComplete:
        notificationName = "LayoutComplete";
        break;
    case WebKit::WebAccessibilityNotificationLoadComplete:
        notificationName = "LoadComplete";
        break;
    case WebKit::WebAccessibilityNotificationSelectedChildrenChanged:
        notificationName = "SelectedChildrenChanged";
        break;
    case WebKit::WebAccessibilityNotificationSelectedTextChanged:
        notificationName = "SelectedTextChanged";
        break;
    case WebKit::WebAccessibilityNotificationValueChanged:
        notificationName = "ValueChanged";
        break;
    case WebKit::WebAccessibilityNotificationScrolledToAnchor:
        notificationName = "ScrolledToAnchor";
        break;
    case WebKit::WebAccessibilityNotificationLiveRegionChanged:
        notificationName = "LiveRegionChanged";
        break;
    case WebKit::WebAccessibilityNotificationMenuListItemSelected:
        notificationName = "MenuListItemSelected";
        break;
    case WebKit::WebAccessibilityNotificationMenuListValueChanged:
        notificationName = "MenuListValueChanged";
        break;
    case WebKit::WebAccessibilityNotificationRowCountChanged:
        notificationName = "RowCountChanged";
        break;
    case WebKit::WebAccessibilityNotificationRowCollapsed:
        notificationName = "RowCollapsed";
        break;
    case WebKit::WebAccessibilityNotificationRowExpanded:
        notificationName = "RowExpanded";
        break;
    case WebKit::WebAccessibilityNotificationInvalidStatusChanged:
        notificationName = "InvalidStatusChanged";
        break;
    case WebKit::WebAccessibilityNotificationTextChanged:
        notificationName = "TextChanged";
        break;
    case WebKit::WebAccessibilityNotificationAriaAttributeChanged:
        notificationName = "AriaAttributeChanged";
        break;
    default:
        notificationName = "UnknownNotification";
        break;
    }

    m_testInterfaces->accessibilityController()->notificationReceived(obj, notificationName);

    if (m_testInterfaces->accessibilityController()->shouldLogAccessibilityEvents()) {
        std::string message("AccessibilityNotification - ");
        message += notificationName;

        WebKit::WebNode node = obj.node();
        if (!node.isNull() && node.isElementNode()) {
            WebKit::WebElement element = node.to<WebKit::WebElement>();
            if (element.hasAttribute("id")) {
                message += " - id:";
                message += element.getAttribute("id").utf8().data();
            }
        }

        m_delegate->printMessage(message + "\n");
    }
}

void WebTestProxyBase::startDragging(WebFrame*, const WebDragData& data, WebDragOperationsMask mask, const WebImage&, const WebPoint&)
{
    // When running a test, we need to fake a drag drop operation otherwise
    // Windows waits for real mouse events to know when the drag is over.
    m_testInterfaces->eventSender()->doDragDrop(data, mask);
}

}
