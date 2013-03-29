/*
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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
#include "EGLHelper.h"

#if USE(EGL)

namespace WebCore {

struct EGLDisplayConnection {

    EGLDisplayConnection(NativeSharedDisplay* display = 0)
    {
        if (display)
            m_eglDisplay = eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(display));
        else
            m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);

        if (m_eglDisplay == EGL_NO_DISPLAY) {
            LOG_ERROR("EGLDisplay Initialization failed.");
            return;
        }

        EGLBoolean success;
        success = eglInitialize(m_eglDisplay, 0, 0);

        if (success != EGL_TRUE) {
            LOG_ERROR("EGLInitialization failed.");
            terminate();
            return;
        }

        success = eglBindAPI(eglAPIVersion);

        if (success != EGL_TRUE) {
            LOG_ERROR("Failed to set EGL API(%d).", eglGetError());
            terminate();
            return;
        }
    }

    ~EGLDisplayConnection()
    {
        terminate();
    }

    EGLDisplay display() { return m_eglDisplay; }

private:
    void terminate()
    {
        if (m_eglDisplay == EGL_NO_DISPLAY)
            return;

        eglTerminate(m_eglDisplay);
        m_eglDisplay = EGL_NO_DISPLAY;
    }

    EGLDisplay m_eglDisplay;
};

PlatformDisplay EGLHelper::eglDisplay()
{
    // Display connection will only be broken at program shutdown.
#if PLATFORM(X11)
    static EGLDisplayConnection displayConnection(NativeWrapper::nativeDisplay());
#else
    static EGLDisplayConnection displayConnection;
#endif
    return displayConnection.display();
}

}

#endif
