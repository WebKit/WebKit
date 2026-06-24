/*
 * Copyright (C) 2026 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformDisplayBroadcomNexus.h"

#if USE(NEXUS)

#include "GLContext.h"
#include <epoxy/egl.h>
#include <nxclient.h>

namespace WebCore {

std::unique_ptr<PlatformDisplayBroadcomNexus> PlatformDisplayBroadcomNexus::create()
{
    NxClient_JoinSettings joinSettings;
    NxClient_GetDefaultJoinSettings(&joinSettings);
    SAFE_SPRINTF(std::span { joinSettings.name }, "%s", "WPEWebProcess"_s);

    NEXUS_Error rc = NxClient_Join(&joinSettings);
    /* not running on nexus */
    if (rc != NEXUS_SUCCESS)
        return nullptr;

    auto glDisplay = GLDisplay::create(eglGetDisplay(EGL_DEFAULT_DISPLAY));
    if (!glDisplay) {
        RELEASE_LOG_ERROR(GLContext, "Could not create default EGL display: %s. Aborting...", GLContext::lastErrorString());
        CRASH();
    }

    return std::unique_ptr<PlatformDisplayBroadcomNexus>(new PlatformDisplayBroadcomNexus(glDisplay.releaseNonNull()));
}

PlatformDisplayBroadcomNexus::PlatformDisplayBroadcomNexus(Ref<GLDisplay>&& glDisplay)
    : PlatformDisplay(WTF::move(glDisplay))
{
#if ENABLE(WEBGL)
    m_anglePlatform = 0;
    m_angleNativeDisplay = EGL_DEFAULT_DISPLAY;
#endif
}

PlatformDisplayBroadcomNexus::~PlatformDisplayBroadcomNexus()
{
    NxClient_Uninit();
}

} // namespace WebCore

#endif // USE(NEXUS)
