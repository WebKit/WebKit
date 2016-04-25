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
#include "PlatformDisplayX11.h"

#if PLATFORM(X11)
#include <X11/Xlib.h>

#if USE(EGL)
#include <EGL/egl.h>
#endif

namespace WebCore {

PlatformDisplayX11::PlatformDisplayX11()
    : m_display(XOpenDisplay(nullptr))
{
    m_ownedDisplay = m_display != nullptr;
}

PlatformDisplayX11::PlatformDisplayX11(Display* display)
    : m_display(display)
    , m_ownedDisplay(false)
{
}

PlatformDisplayX11::~PlatformDisplayX11()
{
    if (m_ownedDisplay)
        XCloseDisplay(m_display);
}

#if USE(EGL)
void PlatformDisplayX11::initializeEGLDisplay()
{
    m_eglDisplay = eglGetDisplay(m_display);
    PlatformDisplay::initializeEGLDisplay();
}
#endif

} // namespace WebCore

#endif // PLATFORM(X11)

