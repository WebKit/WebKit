/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
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
#include "NavigatorWebXR.h"

#if ENABLE(WEBXR)

#include "Navigator.h"
#include "WebXRSystem.h"

namespace WebCore {

WebXRSystem& NavigatorWebXR::xr(Navigator& navigatorObject)
{
    auto& navigator = NavigatorWebXR::from(navigatorObject);
    if (!navigator.m_xr)
        navigator.m_xr = WebXRSystem::create(*navigatorObject.scriptExecutionContext());
    return *navigator.m_xr;
}

NavigatorWebXR& NavigatorWebXR::from(Navigator& navigator)
{
    auto* supplement = static_cast<NavigatorWebXR*>(Supplement<Navigator>::from(&navigator, "NavigatorWebXR"));
    if (!supplement) {
        auto newSupplement = makeUnique<NavigatorWebXR>();
        supplement = newSupplement.get();
        provideTo(&navigator, "NavigatorWebXR", WTFMove(newSupplement));
    }
    return *supplement;
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
