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
#include "WebFakeXRInputController.h"

#if ENABLE(WEBXR)

namespace WebCore {

void WebFakeXRInputController::setHandedness(XRHandedness)
{
}

void WebFakeXRInputController::setTargetRayMode(XRTargetRayMode)
{
}

void WebFakeXRInputController::setProfiles(Vector<String>)
{
}

void WebFakeXRInputController::setGripOrigin(FakeXRRigidTransformInit gripOrigin, bool emulatedPosition)
{
    UNUSED_PARAM(gripOrigin);
    UNUSED_PARAM(emulatedPosition);
}

void WebFakeXRInputController::clearGripOrigin()
{
}

void WebFakeXRInputController::setPointerOrigin(FakeXRRigidTransformInit pointerOrigin, bool emulatedPosition)
{
    UNUSED_PARAM(pointerOrigin);
    UNUSED_PARAM(emulatedPosition);
}

void WebFakeXRInputController::disconnect()
{
}

void WebFakeXRInputController::reconnect()
{
}

void WebFakeXRInputController::startSelection()
{
}

void WebFakeXRInputController::endSelection()
{
}

void WebFakeXRInputController::simulateSelect()
{
}

void WebFakeXRInputController::setSupportedButtons(Vector<FakeXRButtonStateInit>)
{
}

void WebFakeXRInputController::updateButtonState(FakeXRButtonStateInit)
{
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
