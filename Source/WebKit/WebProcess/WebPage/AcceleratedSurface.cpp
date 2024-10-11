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
#include <wtf/TZoneMallocInlines.h>

#if USE(WPE_RENDERER)
#include "AcceleratedSurfaceLibWPE.h"
#endif

#if (PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM)))
#include "AcceleratedSurfaceDMABuf.h"
#endif

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#else
#include <GLES2/gl2.h>
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(AcceleratedSurface);

std::unique_ptr<AcceleratedSurface> AcceleratedSurface::create(WebPage& webPage, Function<void()>&& frameCompleteHandler)
{
#if (PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM)))
#if USE(GBM)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::GBM)
        return AcceleratedSurfaceDMABuf::create(webPage, WTFMove(frameCompleteHandler));
#endif
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::Surfaceless)
        return AcceleratedSurfaceDMABuf::create(webPage, WTFMove(frameCompleteHandler));
#endif
#if USE(WPE_RENDERER)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::WPE)
        return AcceleratedSurfaceLibWPE::create(webPage, WTFMove(frameCompleteHandler));
#endif
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

AcceleratedSurface::AcceleratedSurface(WebPage& webPage, Function<void()>&& frameCompleteHandler)
    : m_webPage(webPage)
    , m_frameCompleteHandler(WTFMove(frameCompleteHandler))
    , m_size(webPage.size())
    , m_isOpaque(!webPage.backgroundColor().has_value() || webPage.backgroundColor()->isOpaque())
{
    m_size.scale(m_webPage->deviceScaleFactor());
}

bool AcceleratedSurface::hostResize(const IntSize& size)
{
    IntSize scaledSize(size);
    scaledSize.scale(m_webPage->deviceScaleFactor());
    if (scaledSize == m_size)
        return false;

    m_size = scaledSize;
    return true;
}

bool AcceleratedSurface::backgroundColorDidChange()
{
    const auto& color = m_webPage->backgroundColor();
    auto isOpaque = !color.has_value() || color->isOpaque();
    if (m_isOpaque == isOpaque)
        return false;

    m_isOpaque = isOpaque;
    return true;
}

void AcceleratedSurface::clearIfNeeded()
{
    if (m_isOpaque)
        return;

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
}

void AcceleratedSurface::frameComplete() const
{
    m_frameCompleteHandler();
}

} // namespace WebKit
