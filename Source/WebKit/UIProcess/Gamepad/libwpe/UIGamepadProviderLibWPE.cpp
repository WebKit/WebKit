/*
 * Copyright (C) 2022 Igalia S.L. All rights reserved.
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
#include "UIGamepadProvider.h"

#if ENABLE(GAMEPAD)

#if USE(LIBWPE)
#include <WebCore/GamepadProviderLibWPE.h>
#include <wpe/wpe.h>
#endif

#if PLATFORM(WPE)
#include "WPEWebViewLegacy.h"

#if ENABLE(WPE_PLATFORM)
#include "GamepadProviderWPE.h"
#include "WPEUtilities.h"
#include "WPEWebViewPlatform.h"
#include <wpe/wpe-platform.h>
#endif
#endif

namespace WebKit {
using namespace WebCore;

void UIGamepadProvider::platformSetDefaultGamepadProvider()
{
    if (GamepadProvider::singleton().isMockGamepadProvider())
        return;

#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        GamepadProvider::setSharedProvider(GamepadProviderWPE::singleton());
        return;
    }
#endif

#if USE(LIBWPE)
#if WPE_CHECK_VERSION(1, 13, 90)
    GamepadProvider::setSharedProvider(GamepadProviderLibWPE::singleton());
#endif
#endif
}

WebPageProxy* UIGamepadProvider::platformWebPageProxyForGamepadInput()
{
#if PLATFORM(WPE)
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI())
        return WKWPE::ViewPlatform::platformWebPageProxyForGamepadInput();
#endif
#if USE(LIBWPE)
    return WKWPE::ViewLegacy::platformWebPageProxyForGamepadInput();
#endif
#endif
    return nullptr;
}

void UIGamepadProvider::platformStopMonitoringInput()
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI())
        GamepadProviderWPE::singleton().stopMonitoringInput();
#endif
}

void UIGamepadProvider::platformStartMonitoringInput()
{
#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI())
        GamepadProviderWPE::singleton().startMonitoringInput();
#endif
}

} // namespace WebKit

#endif // ENABLE(GAMEPAD)
