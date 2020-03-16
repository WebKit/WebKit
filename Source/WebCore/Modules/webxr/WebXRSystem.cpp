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
#include "WebXRSystem.h"

#if ENABLE(WEBXR)

#include "PlatformXR.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRSystem);

Ref<WebXRSystem> WebXRSystem::create(ScriptExecutionContext& scriptExecutionContext)
{
    return adoptRef(*new WebXRSystem(scriptExecutionContext));
}

WebXRSystem::WebXRSystem(ScriptExecutionContext& scriptExecutionContext)
    : ActiveDOMObject(&scriptExecutionContext)
{
}

WebXRSystem::~WebXRSystem() = default;

void WebXRSystem::isSessionSupported(XRSessionMode, IsSessionSupportedPromise&&)
{
    // When the supportsSession(mode) method is invoked, it MUST return a new Promise promise and run the following steps in parallel:
    //   1. Ensure an XR device is selected.
    //   2. If the XR device is null, reject promise with a "NotSupportedError" DOMException and abort these steps.
    //   3. If the XR device's list of supported modes does not contain mode, reject promise with a "NotSupportedError" DOMException and abort these steps.
    //   4. Else resolve promise.
}

void WebXRSystem::requestSession(XRSessionMode, const XRSessionInit&, RequestSessionPromise&&)
{
    PlatformXR::Instance::singleton();

    // When the requestSession(mode) method is invoked, the user agent MUST return a new Promise promise and run the following steps in parallel:
    //   1. Let immersive be true if mode is "immersive-vr" or "immersive-ar", and false otherwise.
    //   2. If immersive is true:
    //     1. If pending immersive session is true or active immersive session is not null, reject promise with an "InvalidStateError" DOMException and abort these steps.
    //     2. Else set pending immersive session to be true.
    //   3. Ensure an XR device is selected.
    //   4. If the XR device is null, reject promise with null.
    //   5. Else if the XR device's list of supported modes does not contain mode, reject promise with a "NotSupportedError" DOMException.
    //   6. Else If immersive is true and the algorithm is not triggered by user activation, reject promise with a "SecurityError" DOMException and abort these steps.
    //   7. If promise was rejected and immersive is true, set pending immersive session to false.
    //   8. If promise was rejected, abort these steps.
    //   9. Let session be a new XRSession object.
    //   10. Initialize the session with session and mode.
    //   11. If immersive is true, set the active immersive session to session, and set pending immersive session to false.
    //   12. Else append session to the list of inline sessions.
    //   13. Resolve promise with session.
}

const char* WebXRSystem::activeDOMObjectName() const
{
    return "XRSystem";
}

void WebXRSystem::stop()
{
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
