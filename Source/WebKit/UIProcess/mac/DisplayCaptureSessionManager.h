/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

#include "UserMediaPermissionRequestProxy.h"
#include "WebPageProxy.h"
#include <WebCore/SecurityOriginData.h>
#include <wtf/CompletionHandler.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class SecurityOrigin;
}

namespace WebKit {

class DisplayCaptureSessionManager {
public:
    static DisplayCaptureSessionManager& singleton();
    static bool isAvailable();

    DisplayCaptureSessionManager();
    ~DisplayCaptureSessionManager();

    void promptForGetDisplayMedia(UserMediaPermissionRequestProxy::UserMediaDisplayCapturePromptType, WebPageProxy&, const WebCore::SecurityOriginData&, CompletionHandler<void(std::optional<WebCore::CaptureDevice>)>&&);
    void cancelGetDisplayMediaPrompt(WebPageProxy&);
    bool canRequestDisplayCapturePermission();
    void setIndexOfDeviceSelectedForTesting(std::optional<unsigned> index) { m_indexOfDeviceSelectedForTesting = index; }

    enum class PromptOverride { Default, CanPrompt, CanNotPrompt };
    void setSystemCanPromptForTesting(bool canPrompt) { m_systemCanPromptForTesting = canPrompt ? PromptOverride::CanPrompt : PromptOverride::CanNotPrompt; }
    bool overrideCanRequestDisplayCapturePermissionForTesting() const { return useMockCaptureDevices() && m_systemCanPromptForTesting != PromptOverride::Default; }

private:

#if HAVE(SCREEN_CAPTURE_KIT)
    enum class CaptureSessionType { None, Screen, Window };
    void alertForGetDisplayMedia(WebPageProxy&, const WebCore::SecurityOriginData&, CompletionHandler<void(DisplayCaptureSessionManager::CaptureSessionType)>&&);
#endif
    void showWindowPicker(const WebCore::SecurityOriginData&, CompletionHandler<void(std::optional<WebCore::CaptureDevice>)>&&);
    void showScreenPicker(const WebCore::SecurityOriginData&, CompletionHandler<void(std::optional<WebCore::CaptureDevice>)>&&);
    std::optional<WebCore::CaptureDevice> deviceSelectedForTesting(WebCore::CaptureDevice::DeviceType, unsigned);

    bool useMockCaptureDevices() const;

    std::optional<unsigned> m_indexOfDeviceSelectedForTesting;
    PromptOverride m_systemCanPromptForTesting { PromptOverride::Default };
};

} // namespace WebKit

#endif // PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
