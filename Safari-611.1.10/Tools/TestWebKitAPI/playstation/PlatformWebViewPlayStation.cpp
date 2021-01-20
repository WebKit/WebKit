/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc
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

#include "config.h"
#include "PlatformWebView.h"

#include <KeyboardEvents.h>
#include <WebKit/WKContextConfigurationPlayStation.h>
#include <WebKit/WKPageConfigurationRef.h>
#include <WebKit/WKPagePrivatePlayStation.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WebKit2_C.h>

namespace TestWebKitAPI {
PlatformWebView::PlatformWebView(WKContextRef contextRef, WKPageGroupRef pageGroupRef)
{
    WKRetainPtr<WKPageConfigurationRef> configuration = adoptWK(WKPageConfigurationCreate());
    WKPageConfigurationSetContext(configuration.get(), contextRef);
    WKPageConfigurationSetPageGroup(configuration.get(), pageGroupRef);

    initialize(configuration.get());
}

PlatformWebView::PlatformWebView(WKPageConfigurationRef configuration)
{
    initialize(configuration);
}

PlatformWebView::PlatformWebView(WKPageRef relatedPage)
{
    WKRetainPtr<WKPageConfigurationRef> configuration = adoptWK(WKPageConfigurationCreate());
    WKPageConfigurationSetContext(configuration.get(), WKPageGetContext(relatedPage));
    WKPageConfigurationSetPageGroup(configuration.get(), WKPageGetPageGroup(relatedPage));
    WKPageConfigurationSetRelatedPage(configuration.get(), relatedPage);

    initialize(configuration.get());
}

PlatformWebView::~PlatformWebView()
{
    WKPageClose(page());
    WKRelease(m_view);
}

void PlatformWebView::initialize(WKPageConfigurationRef configuration)
{
    m_view = WKViewCreate(configuration);
    resizeTo(800, 600);
}

void PlatformWebView::resizeTo(unsigned width, unsigned height)
{
    // Not implemented.
}

WKPageRef PlatformWebView::page() const
{
    return WKViewGetPage(m_view);
}

void PlatformWebView::simulateSpacebarKeyPress()
{
    WKPageHandleKeyboardEvent(page(), WKKeyboardEventMake(kWKEventKeyDown, kWKInputTypeNormal, " ", 1, keyIdentifierForKeyCode(0x20), 0x20, -1, 0, 0));
    WKPageHandleKeyboardEvent(page(), WKKeyboardEventMake(kWKEventKeyUp, kWKInputTypeNormal, " ", 1, keyIdentifierForKeyCode(0x20), 0x20, -1, 0, 0));
}

void PlatformWebView::simulateMouseMove(unsigned x, unsigned y, WKEventModifiers modifiers)
{
    WKPageHandleMouseEvent(page(), WKMouseEventMake(kWKEventMouseMove, kWKEventMouseButtonNoButton, WKPointMake(x, y), 0, modifiers));
}

void PlatformWebView::simulateRightClick(unsigned x, unsigned y)
{
    simulateButtonClick(kWKEventMouseButtonRightButton, x, y, 0);
}

void PlatformWebView::simulateButtonClick(WKEventMouseButton button, unsigned x, unsigned y, WKEventModifiers modifiers)
{
    WKPageHandleMouseEvent(page(), WKMouseEventMake(kWKEventMouseDown, button, WKPointMake(x, y), 0, modifiers));
    WKPageHandleMouseEvent(page(), WKMouseEventMake(kWKEventMouseUp, button, WKPointMake(x, y), 0, modifiers));
}

} // namespace TestWebKitAPI
