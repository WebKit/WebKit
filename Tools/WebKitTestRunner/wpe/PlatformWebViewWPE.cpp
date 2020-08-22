/*
 * Copyright (C) 2014 Igalia S.L.
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

#include "HeadlessViewBackend.h"
#include <WebKit/WKImageCairo.h>
#include <cairo.h>
#include <cstdio>
#include <glib.h>
#include <wtf/RunLoop.h>

namespace WTR {

PlatformWebView::PlatformWebView(WKPageConfigurationRef configuration, const TestOptions& options)
    : m_windowIsKey(true)
    , m_options(options)
{
    m_window = new WPEToolingBackends::HeadlessViewBackend(800, 600);
    m_view = WKViewCreate(m_window->backend(), configuration);
}

PlatformWebView::~PlatformWebView()
{
    delete m_window;
}

void PlatformWebView::resizeTo(unsigned, unsigned, WebViewSizingMode)
{
}

WKPageRef PlatformWebView::page()
{
    return WKViewGetPage(m_view);
}

void PlatformWebView::focus()
{
}

WKRect PlatformWebView::windowFrame()
{
    return { { 0, 0 }, { 1, 1 } };
}

void PlatformWebView::setWindowFrame(WKRect, WebViewSizingMode)
{
}

void PlatformWebView::didInitializeClients()
{
}

void PlatformWebView::addChromeInputField()
{
}

void PlatformWebView::removeChromeInputField()
{
}

void PlatformWebView::addToWindow()
{
    m_window->addActivityState(wpe_view_activity_state_in_window);
}

void PlatformWebView::removeFromWindow()
{
    m_window->removeActivityState(wpe_view_activity_state_in_window);
}

void PlatformWebView::setWindowIsKey(bool windowIsKey)
{
    m_windowIsKey = windowIsKey;
}

void PlatformWebView::makeWebViewFirstResponder()
{
}

cairo_surface_t* PlatformWebView::windowSnapshotImage()
{
    {
        struct TimeoutTimer {
            TimeoutTimer()
                : timer(RunLoop::main(), this, &TimeoutTimer::fired)
            {
                timer.startOneShot(1_s / 60);
            }

            void fired() { RunLoop::main().stop(); }
            RunLoop::Timer<TimeoutTimer> timer;
        } timeoutTimer;

        RunLoop::main().run();
    }

    return m_window->snapshot();
}

void PlatformWebView::changeWindowScaleIfNeeded(float)
{
}

void PlatformWebView::setNavigationGesturesEnabled(bool)
{
}

void PlatformWebView::forceWindowFramesChanged()
{
}

bool PlatformWebView::drawsBackground() const
{
    return false;
}

void PlatformWebView::setDrawsBackground(bool)
{
}

void PlatformWebView::setEditable(bool)
{
}

} // namespace WTR
