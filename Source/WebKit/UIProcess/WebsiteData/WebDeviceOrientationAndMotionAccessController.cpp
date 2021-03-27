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
#include "WebDeviceOrientationAndMotionAccessController.h"

#if ENABLE(DEVICE_ORIENTATION)

#include "APIUIClient.h"
#include "FrameInfoData.h"
#include "WebPageProxy.h"

namespace WebKit {

using namespace WebCore;

void WebDeviceOrientationAndMotionAccessController::shouldAllowAccess(WebPageProxy& page, WebFrameProxy& frame, FrameInfoData&& frameInfo, bool mayPrompt, CompletionHandler<void(DeviceOrientationOrMotionPermissionState)>&& completionHandler)
{
    auto originData = SecurityOrigin::createFromString(page.pageLoadState().activeURL())->data();
    auto currentPermission = cachedDeviceOrientationPermission(originData);
    if (currentPermission != DeviceOrientationOrMotionPermissionState::Prompt || !mayPrompt)
        return completionHandler(currentPermission);

    auto& pendingRequests = m_pendingRequests.ensure(originData, [] {
        return Vector<CompletionHandler<void(WebCore::DeviceOrientationOrMotionPermissionState)>> { };
    }).iterator->value;
    pendingRequests.append(WTFMove(completionHandler));
    if (pendingRequests.size() > 1)
        return;

    page.uiClient().shouldAllowDeviceOrientationAndMotionAccess(page, frame, WTFMove(frameInfo), [this, weakThis = makeWeakPtr(this), originData](bool granted) mutable {
        if (!weakThis)
            return;
        m_deviceOrientationPermissionDecisions.set(originData, granted);
        auto requests = m_pendingRequests.take(originData);
        for (auto& completionHandler : requests)
            completionHandler(granted ? DeviceOrientationOrMotionPermissionState::Granted : DeviceOrientationOrMotionPermissionState::Denied);
    });
}

DeviceOrientationOrMotionPermissionState WebDeviceOrientationAndMotionAccessController::cachedDeviceOrientationPermission(const SecurityOriginData& origin) const
{
    if (!m_deviceOrientationPermissionDecisions.isValidKey(origin))
        return DeviceOrientationOrMotionPermissionState::Denied;

    auto it = m_deviceOrientationPermissionDecisions.find(origin);
    if (it == m_deviceOrientationPermissionDecisions.end())
        return DeviceOrientationOrMotionPermissionState::Prompt;
    return it->value ? DeviceOrientationOrMotionPermissionState::Granted : DeviceOrientationOrMotionPermissionState::Denied;
}

void WebDeviceOrientationAndMotionAccessController::clearPermissions()
{
    m_deviceOrientationPermissionDecisions.clear();
}

} // namespace WebKit

#endif // ENABLE(DEVICE_ORIENTATION)
