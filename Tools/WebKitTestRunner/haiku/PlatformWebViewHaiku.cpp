/*
 * Copyright (C) 2012 Samsung Electronics
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
 */

#include "config.h"
#include "PlatformWebView.h"

#include "NotImplemented.h"
#include <WebView.h>
#include <Window.h>

using namespace WebKit;

namespace WTR {

PlatformWebView::PlatformWebView(WKContextRef context, WKPageGroupRef pageGroup,
    WKPageRef /* relatedPage */, WKDictionaryRef options)
{
    WKRetainPtr<WKStringRef> useFixedLayoutKey(AdoptWK,
        WKStringCreateWithUTF8CString("UseFixedLayout"));
    m_usingFixedLayout = options ? WKBooleanGetValue(static_cast<WKBooleanRef>(
        WKDictionaryGetItemForKey(options, useFixedLayoutKey.get()))) : false;

    m_window = new BWindow(BRect(0, 0, 800, 600), "WebKitTestRunner",
        B_DOCUMENT_WINDOW, 0);

    // TODO create m_view
    m_view = new BWebView(context, pageGroup);
    WKPageSetUseFixedLayout(WKViewGetPage(m_view->GetWKView()),
        m_usingFixedLayout);

    m_window->AddChild(m_view);

    if (m_usingFixedLayout)
        resizeTo(800, 600);

    m_windowIsKey = false;
}

PlatformWebView::~PlatformWebView()
{
    delete m_window; // The window owns the view
}

void PlatformWebView::resizeTo(unsigned width, unsigned height)
{
    m_view->ResizeTo(width, height);
}

WKPageRef PlatformWebView::page()
{
    return WKViewGetPage(m_view->GetWKView());
}

void PlatformWebView::focus()
{
    notImplemented();
}

WKRect PlatformWebView::windowFrame()
{
    BRect r = m_window->Frame();
    return WKRectMake(r.left, r.top, r.Width(), r.Height());
}

void PlatformWebView::setWindowFrame(WKRect frame)
{
    m_window->MoveTo(frame.origin.x, frame.origin.y);
    m_window->ResizeTo(frame.size.width, frame.size.height);
}

void PlatformWebView::addChromeInputField()
{
}

void PlatformWebView::removeChromeInputField()
{
}

void PlatformWebView::makeWebViewFirstResponder()
{
}

void PlatformWebView::changeWindowScaleIfNeeded(float)
{
}

void PlatformWebView::didInitializeClients()
{
}

}
