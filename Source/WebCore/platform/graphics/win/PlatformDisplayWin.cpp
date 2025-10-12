/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "PlatformDisplayWin.h"

#include "GLContext.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace WebCore {

std::unique_ptr<PlatformDisplayWin> PlatformDisplayWin::create()
{
    EGLint attributes[] = {
        // Disable debug layers that makes some tests fail because
        // ANGLE fills uninitialized buffers with
        // kDebugColorInitClearValue in debug builds.
        EGL_PLATFORM_ANGLE_DEBUG_LAYERS_ENABLED_ANGLE,
        EGL_FALSE,
        EGL_NONE,
    };
    auto glDisplay = GLDisplay::create(eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, attributes));
    if (!glDisplay) {
        WTFLogAlways("Could not create EGL display: %s. Aborting...", GLContext::lastErrorString());
        CRASH();
    }

    return std::unique_ptr<PlatformDisplayWin>(new PlatformDisplayWin(glDisplay.releaseNonNull()));
}

PlatformDisplayWin::PlatformDisplayWin(Ref<GLDisplay>&& glDisplay)
    : PlatformDisplay(WTFMove(glDisplay))
{
}

} // namespace WebCore
