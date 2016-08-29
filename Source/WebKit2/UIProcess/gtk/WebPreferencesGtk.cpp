/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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
#include "WebPreferences.h"

#include "WaylandCompositor.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformDisplay.h>

#if USE(REDIRECTED_XCOMPOSITE_WINDOW)
#include <WebCore/PlatformDisplayX11.h>
#endif

using namespace WebCore;

namespace WebKit {

void WebPreferences::platformInitializeStore()
{
#if !ENABLE(OPENGL)
    setAcceleratedCompositingEnabled(false);
#else
#if USE(COORDINATED_GRAPHICS_THREADED)
    setForceCompositingMode(true);
#else
    if (getenv("WEBKIT_FORCE_COMPOSITING_MODE"))
        setForceCompositingMode(true);
#endif

    if (getenv("WEBKIT_DISABLE_COMPOSITING_MODE")) {
        setAcceleratedCompositingEnabled(false);
        return;
    }

#if USE(REDIRECTED_XCOMPOSITE_WINDOW)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::X11) {
        auto& display = downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay());
        Optional<int> damageBase;
        if (!display.supportsXComposite() || !display.supportsXDamage(damageBase))
            setAcceleratedCompositingEnabled(false);
    }
#endif

#if PLATFORM(WAYLAND)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::Wayland) {
        if (!WaylandCompositor::singleton().isRunning())
            setAcceleratedCompositingEnabled(false);
    }
#endif

#endif // ENABLE(OPENGL)
}

void WebPreferences::platformUpdateStringValueForKey(const String&, const String&)
{
    notImplemented();
}

void WebPreferences::platformUpdateBoolValueForKey(const String&, bool)
{
    notImplemented();
}

void WebPreferences::platformUpdateUInt32ValueForKey(const String&, uint32_t)
{
    notImplemented();
}

void WebPreferences::platformUpdateDoubleValueForKey(const String&, double)
{
    notImplemented();
}

void WebPreferences::platformUpdateFloatValueForKey(const String&, float)
{
    notImplemented();
}

bool WebPreferences::platformGetStringUserValueForKey(const String&, String&)
{
    notImplemented();
    return false;
}

bool WebPreferences::platformGetBoolUserValueForKey(const String&, bool&)
{
    notImplemented();
    return false;
}

bool WebPreferences::platformGetUInt32UserValueForKey(const String&, uint32_t&)
{
    notImplemented();
    return false;
}

bool WebPreferences::platformGetDoubleUserValueForKey(const String&, double&)
{
    notImplemented();
    return false;
}

} // namespace WebKit
