/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "QtWebPagePolicyClient.h"

#include "WKURLQt.h"
#include "qquickwebview_p.h"
#include "qquickwebview_p_p.h"
#include <WKFramePolicyListener.h>
#include <WKURLRequest.h>

QtWebPagePolicyClient::QtWebPagePolicyClient(WKPageRef pageRef, QQuickWebView* webView)
    : m_webView(webView)
{
    WKPagePolicyClient policyClient;
    memset(&policyClient, 0, sizeof(WKPagePolicyClient));
    policyClient.version = kWKPagePolicyClientCurrentVersion;
    policyClient.clientInfo = this;
    policyClient.decidePolicyForNavigationAction = decidePolicyForNavigationAction;
    policyClient.decidePolicyForResponse = decidePolicyForResponse;
    WKPageSetPagePolicyClient(pageRef, &policyClient);
}

QtWebPagePolicyClient::PolicyAction QtWebPagePolicyClient::decidePolicyForNavigationAction(const QUrl& url, Qt::MouseButton mouseButton, Qt::KeyboardModifiers keyboardModifiers)
{
    return m_webView->d_func()->navigationPolicyForURL(url, mouseButton, keyboardModifiers);
}

static inline QtWebPagePolicyClient* toQtWebPagePolicyClient(const void* clientInfo)
{
    ASSERT(clientInfo);
    return reinterpret_cast<QtWebPagePolicyClient*>(const_cast<void*>(clientInfo));
}

static Qt::MouseButton toQtMouseButton(WKEventMouseButton button)
{
    switch (button) {
    case kWKEventMouseButtonLeftButton:
        return Qt::LeftButton;
    case kWKEventMouseButtonMiddleButton:
        return Qt::MiddleButton;
    case kWKEventMouseButtonRightButton:
        return Qt::RightButton;
    case kWKEventMouseButtonNoButton:
        return Qt::NoButton;
    }
    ASSERT_NOT_REACHED();
    return Qt::NoButton;
}

static Qt::KeyboardModifiers toQtKeyboardModifiers(WKEventModifiers modifiers)
{
    Qt::KeyboardModifiers qtModifiers = Qt::NoModifier;
    if (modifiers & kWKEventModifiersShiftKey)
        qtModifiers |= Qt::ShiftModifier;
    if (modifiers & kWKEventModifiersControlKey)
        qtModifiers |= Qt::ControlModifier;
    if (modifiers & kWKEventModifiersAltKey)
        qtModifiers |= Qt::AltModifier;
    if (modifiers & kWKEventModifiersMetaKey)
        qtModifiers |= Qt::MetaModifier;
    return qtModifiers;
}

void QtWebPagePolicyClient::decidePolicyForNavigationAction(WKPageRef, WKFrameRef, WKFrameNavigationType, WKEventModifiers modifiers, WKEventMouseButton mouseButton, WKURLRequestRef request, WKFramePolicyListenerRef listener, WKTypeRef, const void* clientInfo)
{
    WKURLRef requestURL = WKURLRequestCopyURL(request);
    QUrl qUrl = WKURLCopyQUrl(requestURL);
    WKRelease(requestURL);

    PolicyAction action = toQtWebPagePolicyClient(clientInfo)->decidePolicyForNavigationAction(qUrl, toQtMouseButton(mouseButton), toQtKeyboardModifiers(modifiers));
    switch (action) {
    case Use:
        WKFramePolicyListenerUse(listener);
        return;
    case Download:
        WKFramePolicyListenerDownload(listener);
        return;
    case Ignore:
        WKFramePolicyListenerIgnore(listener);
        return;
    }
    ASSERT_NOT_REACHED();
}

void QtWebPagePolicyClient::decidePolicyForResponse(WKPageRef page, WKFrameRef frame, WKURLResponseRef response, WKURLRequestRef, WKFramePolicyListenerRef listener, WKTypeRef, const void*)
{
    String type = toImpl(response)->resourceResponse().mimeType();
    type.makeLower();
    bool canShowMIMEType = toImpl(frame)->canShowMIMEType(type);

    if (WKPageGetMainFrame(page) == frame) {
        if (canShowMIMEType) {
            WKFramePolicyListenerUse(listener);
            return;
        }

        // If we can't use (show) it then we should download it.
        WKFramePolicyListenerDownload(listener);
        return;
    }

    // We should ignore downloadable top-level content for subframes, with an exception for text/xml and application/xml so we can still support Acid3 test.
    // It makes the browser intentionally behave differently when it comes to text(application)/xml content in subframes vs. mainframe.
    if (!canShowMIMEType && !(type == "text/xml" || type == "application/xml")) {
        WKFramePolicyListenerIgnore(listener);
        return;
    }

    WKFramePolicyListenerUse(listener);
}

