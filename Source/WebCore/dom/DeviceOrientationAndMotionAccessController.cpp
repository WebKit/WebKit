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
#include "DeviceOrientationAndMotionAccessController.h"

#if ENABLE(DEVICE_ORIENTATION)

#include "Chrome.h"
#include "ChromeClient.h"
#include "DOMWindow.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "Page.h"
#include "UserGestureIndicator.h"

namespace WebCore {

DeviceOrientationAndMotionAccessController::DeviceOrientationAndMotionAccessController(Document& topDocument)
    : m_topDocument(topDocument)
{
}

DeviceOrientationOrMotionPermissionState DeviceOrientationAndMotionAccessController::accessState(const Document& document) const
{
    auto iterator = m_accessStatePerOrigin.find(document.securityOrigin().data());
    if (iterator != m_accessStatePerOrigin.end())
        return iterator->value;

    // Check per-site setting.
    if (&document == &m_topDocument || document.securityOrigin().isSameOriginAs(m_topDocument.securityOrigin())) {
        auto* frame = m_topDocument.frame();
        if (auto* documentLoader = frame ? frame->loader().documentLoader() : nullptr)
            return documentLoader->deviceOrientationAndMotionAccessState();
    }

    return DeviceOrientationOrMotionPermissionState::Prompt;
}

void DeviceOrientationAndMotionAccessController::shouldAllowAccess(const Document& document, Function<void(DeviceOrientationOrMotionPermissionState)>&& callback)
{
    auto* page = document.page();
    auto* frame = document.frame();
    if (!page || !frame)
        return callback(DeviceOrientationOrMotionPermissionState::Denied);

    auto accessState = this->accessState(document);
    if (accessState != DeviceOrientationOrMotionPermissionState::Prompt)
        return callback(accessState);

    bool mayPrompt = UserGestureIndicator::processingUserGesture(&document);
    page->chrome().client().shouldAllowDeviceOrientationAndMotionAccess(*document.frame(), mayPrompt, [this, weakThis = makeWeakPtr(this), securityOrigin = makeRef(document.securityOrigin()), callback = WTFMove(callback)](DeviceOrientationOrMotionPermissionState permissionState) mutable {
        if (!weakThis)
            return;

        m_accessStatePerOrigin.set(securityOrigin->data(), permissionState);
        callback(permissionState);

        if (permissionState != DeviceOrientationOrMotionPermissionState::Granted)
            return;

        for (auto* frame = m_topDocument.frame(); frame && frame->window(); frame = frame->tree().traverseNext()) {
            frame->window()->startListeningForDeviceOrientationIfNecessary();
            frame->window()->startListeningForDeviceMotionIfNecessary();
        }
    });
}

} // namespace WebCore

#endif // ENABLE(DEVICE_ORIENTATION)
