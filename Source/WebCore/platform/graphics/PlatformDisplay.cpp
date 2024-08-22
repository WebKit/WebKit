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
#include <cstdlib>
#include <mutex>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>

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

#if PLATFORM(WIN)
PlatformDisplay& PlatformDisplay::sharedDisplay()
{
    // ANGLE D3D renderer isn't thread-safe. Don't destruct it on non-main threads which calls _exit().
    static PlatformDisplay* display = PlatformDisplayWin::create().release();
    return *display;
}
#else
IGNORE_CLANG_WARNINGS_BEGIN("exit-time-destructors")
static std::unique_ptr<PlatformDisplay> s_sharedDisplay;
IGNORE_CLANG_WARNINGS_END

void PlatformDisplay::setSharedDisplay(std::unique_ptr<PlatformDisplay>&& display)
{
    RELEASE_ASSERT(!s_sharedDisplay);
    s_sharedDisplay = WTFMove(display);
}

PlatformDisplay& PlatformDisplay::sharedDisplay()
{
    RELEASE_ASSERT(s_sharedDisplay);
    return *s_sharedDisplay;
}

PlatformDisplay* PlatformDisplay::sharedDisplayIfExists()
{
    return s_sharedDisplay.get();
}
#endif

static HashSet<PlatformDisplay*>& eglDisplays()
{
    static NeverDestroyed<HashSet<PlatformDisplay*>> displays;
    return displays;
}

PlatformDisplay::PlatformDisplay(std::unique_ptr<GLDisplay>&& glDisplay)
    : m_eglDisplay(WTFMove(glDisplay))
{
    RELEASE_ASSERT(m_eglDisplay);

    eglDisplays().add(this);

#if !PLATFORM(WIN)
    static bool eglAtexitHandlerInitialized = false;
    if (!eglAtexitHandlerInitialized) {
        // EGL registers atexit handlers to cleanup its global display list.
        // Since the global PlatformDisplay instance is created before,
        // when the PlatformDisplay destructor is called, EGL has already removed the
        // display from the list, causing eglTerminate() to crash. So, here we register
        // our own atexit handler, after EGL has been initialized and after the global
        // instance has been created to ensure we call eglTerminate() before the other
        // EGL atexit handlers and the PlatformDisplay destructor.
        // See https://bugs.webkit.org/show_bug.cgi?id=157973.
        eglAtexitHandlerInitialized = true;
        std::atexit([] {
            while (!eglDisplays().isEmpty()) {
                auto* display = eglDisplays().takeAny();
                display->terminateEGLDisplay();
            }
        });
    }
#endif
}

PlatformDisplay::~PlatformDisplay()
{
    if (eglDisplays().remove(this))
        m_eglDisplay->terminate();
}

GLContext* PlatformDisplay::sharingGLContext()
{
    if (!m_sharingGLContext)
        m_sharingGLContext = GLContext::createSharing(*this);
    return m_sharingGLContext.get();
}

void PlatformDisplay::clearSharingGLContext()
{
#if USE(SKIA)
    invalidateSkiaGLContexts();
#endif
#if ENABLE(VIDEO) && USE(GSTREAMER_GL)
    m_gstGLContext = nullptr;
#endif
#if ENABLE(WEBGL) && !PLATFORM(WIN)
    clearANGLESharingGLContext();
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

void PlatformDisplay::terminateEGLDisplay()
{
#if ENABLE(VIDEO) && USE(GSTREAMER_GL)
    m_gstGLDisplay = nullptr;
#endif
    clearSharingGLContext();

    m_eglDisplay->terminate();
}

EGLImage PlatformDisplay::createEGLImage(EGLContext context, EGLenum target, EGLClientBuffer clientBuffer, const Vector<EGLAttrib>& attributes) const
{
    return m_eglDisplay->createImage(context, target, clientBuffer, attributes);
}

bool PlatformDisplay::destroyEGLImage(EGLImage image) const
{
    return m_eglDisplay->destroyImage(image);
}

#if USE(GBM)
const Vector<GLDisplay::DMABufFormat>& PlatformDisplay::dmabufFormats()
{
    return m_eglDisplay->dmabufFormats();
}
#endif // USE(GBM)

} // namespace WebCore
