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

#include "WebPopupMenuProxyMac.h"

#include "PageClientImpl.h"
#include "PlatformPopupMenuData.h"
#include "WKView.h"
#include "WebPopupItem.h"
#include <WebKitSystemInterface.h>

using namespace WebCore;

namespace WebKit {

WebPopupMenuProxyMac::WebPopupMenuProxyMac(WKView* webView, WebPopupMenuProxy::Client* client)
    : WebPopupMenuProxy(client)
    , m_webView(webView)
{
}

WebPopupMenuProxyMac::~WebPopupMenuProxyMac()
{
    if (m_popup)
        [m_popup.get() setControlView:nil];
}

void WebPopupMenuProxyMac::populate(const Vector<WebPopupItem>& items)
{
    if (m_popup)
        [m_popup.get() removeAllItems];
    else {
        m_popup.adoptNS([[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:NO]);
        [m_popup.get() setUsesItemFromMenu:NO];
        [m_popup.get() setAutoenablesItems:NO];
    }

    int size = items.size();

    for (int i = 0; i < size; i++) {
        if (items[i].m_type == WebPopupItem::Seperator)
            [[m_popup.get() menu] addItem:[NSMenuItem separatorItem]];
        else {
            [m_popup.get() addItemWithTitle:nsStringFromWebCoreString(items[i].m_text)];
            NSMenuItem* menuItem = [m_popup.get() lastItem];
            [menuItem setEnabled:items[i].m_isEnabled];
            [menuItem setToolTip:nsStringFromWebCoreString(items[i].m_toolTip)];
        }
    }
}

void WebPopupMenuProxyMac::showPopupMenu(const IntRect& rect, const Vector<WebPopupItem>& items, const PlatformPopupMenuData&, int32_t selectedIndex)
{
    populate(items);

    [m_popup.get() attachPopUpWithFrame:rect inView:m_webView];
    [m_popup.get() selectItemAtIndex:selectedIndex];

    NSMenu* menu = [m_popup.get() menu];

    // These values were borrowed from AppKit to match their placement of the menu.
    const int popOverHorizontalAdjust = -10;
    NSRect titleFrame = [m_popup.get()  titleRectForBounds:rect];
    if (titleFrame.size.width <= 0 || titleFrame.size.height <= 0)
        titleFrame = rect;
    float vertOffset = roundf((NSMaxY(rect) - NSMaxY(titleFrame)) + NSHeight(titleFrame));
    NSPoint location = NSMakePoint(NSMinX(rect) + popOverHorizontalAdjust, NSMaxY(rect) - vertOffset);

    RetainPtr<NSView> dummyView(AdoptNS, [[NSView alloc] initWithFrame:rect]);
    [m_webView addSubview:dummyView.get()];
    location = [dummyView.get() convertPoint:location fromView:m_webView];

    WKPopupMenu(menu, location, roundf(NSWidth(rect)), dummyView.get(), selectedIndex, [NSFont menuFontOfSize:0]);

    [m_popup.get() dismissPopUp];
    [dummyView.get() removeFromSuperview];

    m_client->valueChangedForPopupMenu(this, [m_popup.get() indexOfSelectedItem]);
}

void WebPopupMenuProxyMac::hidePopupMenu()
{
    [m_popup.get() dismissPopUp];
}


} // namespace WebKit
