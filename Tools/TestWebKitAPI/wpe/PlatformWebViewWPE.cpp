/*
 * Copyright (C) 2012 Igalia S.L.
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

#include <WPEToolingBackends/HeadlessViewBackend.h>
#include <WebKit/WKPagePrivateWPE.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKView.h>

namespace TestWebKitAPI {

PlatformWebView::PlatformWebView(WKContextRef contextRef)
    : m_window(nullptr)
{
    WKRetainPtr<WKPageConfigurationRef> configuration = adoptWK(WKPageConfigurationCreate());
    WKPageConfigurationSetContext(configuration.get(), contextRef);

    initialize(configuration.get());
}

PlatformWebView::PlatformWebView(WKPageConfigurationRef configuration)
    : m_window(nullptr)
{
    initialize(configuration);
}

PlatformWebView::PlatformWebView(WKPageRef relatedPage)
    : m_window(nullptr)
{
    WKRetainPtr<WKPageConfigurationRef> configuration = adoptWK(WKPageConfigurationCreate());
    WKPageConfigurationSetContext(configuration.get(), WKPageGetContext(relatedPage));
    WKPageConfigurationSetRelatedPage(configuration.get(), relatedPage);

    auto relatedConfiguration = adoptWK(WKPageCopyPageConfiguration(relatedPage));
    if (auto* preferences = WKPageConfigurationGetPreferences(relatedConfiguration.get()))
        WKPageConfigurationSetPreferences(configuration.get(), preferences);

    initialize(configuration.get());
}

PlatformWebView::~PlatformWebView()
{
    delete m_window;
}

void PlatformWebView::initialize(WKPageConfigurationRef configuration)
{
    m_window = new WPEToolingBackends::HeadlessViewBackend(800, 600);
    m_view = WKViewCreateDeprecated(m_window->backend(), configuration);
}

WKPageRef PlatformWebView::page() const
{
    return WKViewGetPage(m_view);
}

void PlatformWebView::resizeTo(unsigned width, unsigned height)
{
    // FIXME: implement this.
}

void PlatformWebView::simulateSpacebarKeyPress()
{
    // https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/code
    WKPageHandleKeyboardEvent(page(), WKKeyboardEventMake(kWKEventKeyDown, kWKInputTypeNormal, " ", 1, WPE_KEY_space, 0x0041, 0));
    WKPageHandleKeyboardEvent(page(), WKKeyboardEventMake(kWKEventKeyUp, kWKInputTypeNormal, " ", 1, WPE_KEY_space, 0x0041, 0));
}

void PlatformWebView::simulateAltKeyPress()
{
    // https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/code
    WKPageHandleKeyboardEvent(page(), WKKeyboardEventMake(kWKEventKeyDown, kWKInputTypeNormal, nullptr, 0, WPE_KEY_Alt_L, 0x0040, 0));
    WKPageHandleKeyboardEvent(page(), WKKeyboardEventMake(kWKEventKeyUp, kWKInputTypeNormal, nullptr, 0, WPE_KEY_Alt_L, 0x0040, 0));
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

void PlatformWebView::simulateMouseMove(unsigned x, unsigned y, WKEventModifiers modifiers)
{
    WKPageHandleMouseEvent(page(), WKMouseEventMake(kWKEventMouseMove, kWKEventMouseButtonNoButton, WKPointMake(x, y), 0, modifiers));
}

} // namespace TestWebKitAPI
