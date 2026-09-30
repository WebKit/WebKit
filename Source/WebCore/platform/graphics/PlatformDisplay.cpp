/*
 * Copyright (C) 2015 Igalia S.L
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformDisplay.h"

#include "GLContext.h"
#include <wtf/MainThread.h>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(WIN)
#include "PlatformDisplayWin.h"
#endif

#if USE(LIBEPOXY)
#include <epoxy/egl.h>
#else
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlatformDisplay);

#if PLATFORM(WIN)
PlatformDisplay& PlatformDisplay::sharedDisplay()
{
    // ANGLE D3D renderer isn't thread-safe. Don't destruct it on non-main threads which calls _exit().
    static PlatformDisplay* display = PlatformDisplayWin::create().release();
    return *display;
}
#else
// The shared display is destroyed by destroySharedDisplay() and never at exit: EGL frees its
// displays in its own atexit handler, and threads that are still running can use them.
static PlatformDisplay* s_sharedDisplay;

void PlatformDisplay::setSharedDisplay(std::unique_ptr<PlatformDisplay>&& display)
{
    RELEASE_ASSERT(!s_sharedDisplay);
    s_sharedDisplay = display.release();
}

PlatformDisplay& PlatformDisplay::sharedDisplay()
{
    RELEASE_ASSERT(s_sharedDisplay);
    return *s_sharedDisplay;
}

PlatformDisplay* PlatformDisplay::sharedDisplayIfExists()
{
    return s_sharedDisplay;
}

void PlatformDisplay::destroySharedDisplay()
{
    RELEASE_ASSERT(isMainThread());
    if (!s_sharedDisplay)
        return;

    s_sharedDisplay->clearGLContexts();
    s_sharedDisplay->terminateEGLDisplay();
    delete std::exchange(s_sharedDisplay, nullptr);
}
#endif

PlatformDisplay::PlatformDisplay(Ref<GLDisplay>&& glDisplay)
    : m_eglDisplay(WTF::move(glDisplay))
{
}

PlatformDisplay::~PlatformDisplay() = default;

void PlatformDisplay::terminateEGLDisplay()
{
#if ENABLE(VIDEO) && USE(GSTREAMER_GL)
    m_gstGLDisplay = nullptr;
#endif
    m_eglDisplay->terminate();
}

GLContext* PlatformDisplay::sharingGLContext()
{
    if (!m_sharingGLContext)
        m_sharingGLContext = GLContext::createSharing(*this);
    return m_sharingGLContext.get();
}

void PlatformDisplay::clearGLContexts()
{
#if USE(SKIA)
    clearSkiaGLContext();
#endif
#if ENABLE(VIDEO) && USE(GSTREAMER_GL)
    m_gstGLContext = nullptr;
#endif
    m_sharingGLContext = nullptr;
}

EGLDisplay PlatformDisplay::eglDisplay() const
{
    return m_eglDisplay->eglDisplay();
}

bool PlatformDisplay::eglCheckVersion(int major, int minor) const
{
    return m_eglDisplay->checkVersion(major, minor);
}

const GLDisplay::Extensions& PlatformDisplay::eglExtensions() const
{
    return m_eglDisplay->extensions();
}

EGLImage PlatformDisplay::createEGLImage(EGLContext context, EGLenum target, EGLClientBuffer clientBuffer, const Vector<EGLAttrib>& attributes) const
{
    return m_eglDisplay->createImage(context, target, clientBuffer, attributes);
}

bool PlatformDisplay::destroyEGLImage(EGLImage image) const
{
    return m_eglDisplay->destroyImage(image);
}

#if USE(GBM) || OS(ANDROID)
const Vector<GLDisplay::BufferFormat>& PlatformDisplay::bufferFormats()
{
    return m_eglDisplay->bufferFormats();
}
#endif // USE(GBM) || OS(ANDROID)

#if USE(GBM) && USE(GSTREAMER)
const Vector<GLDisplay::BufferFormat>& PlatformDisplay::bufferFormatsForVideo()
{
    return m_eglDisplay->bufferFormatsForVideo();
}
#endif // USE(GBM) && USE(GSTREAMER)

} // namespace WebCore
