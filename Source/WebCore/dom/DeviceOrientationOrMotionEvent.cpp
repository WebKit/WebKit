/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DeviceOrientationOrMotionEvent.h"

#include "Document.h"
#include "UserGestureIndicator.h"

namespace WebCore {

#if ENABLE(DEVICE_ORIENTATION)
void DeviceOrientationOrMotionEvent::requestPermission(Document& document, PermissionPromise&& promise)
{
    auto* window = document.domWindow();
    if (!window)
        return promise.reject(Exception { InvalidStateError, "No browsing context"_s });

    if (!UserGestureIndicator::processingUserGesture())
        return promise.reject(Exception { NotAllowedError, "Calling requestPermission() requires a user gesture"_s });

    String errorMessage;
    if (!window->isAllowedToUseDeviceMotionOrientation(errorMessage)) {
        document.addConsoleMessage(MessageSource::JS, MessageLevel::Warning, makeString("Call to requestPermission() failed, reason: ", errorMessage, "."));
        return promise.resolve(PermissionState::Denied);
    }

    document.deviceOrientationAndMotionAccessController().shouldAllowAccess([promise = WTFMove(promise)](bool granted) mutable {
        promise.resolve(granted ? PermissionState::Granted : PermissionState::Denied);
    });
}
#endif

} // namespace WebCore
