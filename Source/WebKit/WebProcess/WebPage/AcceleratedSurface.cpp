/*
 * Copyright (C) 2016 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AcceleratedSurface.h"

#include "WebPage.h"
#include <WebCore/PlatformDisplay.h>

#if PLATFORM(WAYLAND)
#include "AcceleratedSurfaceWayland.h"
#endif

#if USE(REDIRECTED_XCOMPOSITE_WINDOW)
#include "AcceleratedSurfaceX11.h"
#endif

#if PLATFORM(WPE)
#include "AcceleratedSurfaceWPE.h"
#endif

namespace WebKit {
using namespace WebCore;

std::unique_ptr<AcceleratedSurface> AcceleratedSurface::create(WebPage& webPage, Client& client)
{
#if PLATFORM(WAYLAND)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::Wayland)
        return AcceleratedSurfaceWayland::create(webPage, client);
#endif
#if USE(REDIRECTED_XCOMPOSITE_WINDOW)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::X11)
        return AcceleratedSurfaceX11::create(webPage, client);
#endif
#if PLATFORM(WPE)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::WPE)
        return AcceleratedSurfaceWPE::create(webPage, client);
#endif
    return nullptr;
}

AcceleratedSurface::AcceleratedSurface(WebPage& webPage, Client& client)
    : m_webPage(webPage)
    , m_client(client)
    , m_size(webPage.size())
{
    m_size.scale(m_webPage.deviceScaleFactor());
}

bool AcceleratedSurface::hostResize(const IntSize& size)
{
    IntSize scaledSize(size);
    scaledSize.scale(m_webPage.deviceScaleFactor());
    if (scaledSize == m_size)
        return false;

    m_size = scaledSize;
    return true;
}

} // namespace WebKit
