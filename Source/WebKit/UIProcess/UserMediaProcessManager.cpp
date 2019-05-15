/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "UserMediaProcessManager.h"

#if ENABLE(MEDIA_STREAM)

#include "Logging.h"
#include "MediaDeviceSandboxExtensions.h"
#include "WebPageProxy.h"
#include "WebProcessMessages.h"
#include "WebProcessProxy.h"
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

#if ENABLE(SANDBOX_EXTENSIONS)
static const ASCIILiteral audioExtensionPath { "com.apple.webkit.microphone"_s };
static const ASCIILiteral videoExtensionPath { "com.apple.webkit.camera"_s };
#endif

static const Seconds deviceChangeDebounceTimerInterval { 200_ms };

UserMediaProcessManager& UserMediaProcessManager::singleton()
{
    static NeverDestroyed<UserMediaProcessManager> manager;
    return manager;
}

UserMediaProcessManager::UserMediaProcessManager()
    : m_debounceTimer(RunLoop::main(), this, &UserMediaProcessManager::captureDevicesChanged)
{
}

void UserMediaProcessManager::muteCaptureMediaStreamsExceptIn(WebPageProxy& pageStartingCapture)
{
#if PLATFORM(COCOA)
    UserMediaPermissionRequestManagerProxy::forEach([&pageStartingCapture](auto& proxy) {
        if (&proxy.page() != &pageStartingCapture)
            proxy.page().setMediaStreamCaptureMuted(true);
    });
#else
    UNUSED_PARAM(pageStartingCapture);
#endif
}

bool UserMediaProcessManager::willCreateMediaStream(UserMediaPermissionRequestManagerProxy& proxy, bool withAudio, bool withVideo)
{
    ASSERT(withAudio || withVideo);

    if (m_denyNextRequest) {
        m_denyNextRequest = false;
        return false;
    }
    
#if ENABLE(SANDBOX_EXTENSIONS) && USE(APPLE_INTERNAL_SDK)
    auto& process = proxy.page().process();
    size_t extensionCount = 0;

    if (withAudio && !process.hasAudioCaptureExtension())
        extensionCount++;
    else
        withAudio = false;

    if (withVideo && !process.hasVideoCaptureExtension())
        extensionCount++;
    else
        withVideo = false;

    if (extensionCount) {
        SandboxExtension::HandleArray handles;
        Vector<String> ids;

        if (!proxy.page().preferences().mockCaptureDevicesEnabled()) {
            handles.allocate(extensionCount);
            ids.reserveInitialCapacity(extensionCount);

            if (withAudio && SandboxExtension::createHandleForGenericExtension(audioExtensionPath, handles[--extensionCount]))
                ids.append(audioExtensionPath);

            if (withVideo && SandboxExtension::createHandleForGenericExtension(videoExtensionPath, handles[--extensionCount]))
                ids.append(videoExtensionPath);

            if (ids.size() != handles.size()) {
                WTFLogAlways("Could not create a required sandbox extension, capture will fail!");
                return false;
            }
        }

        for (const auto& id : ids)
            RELEASE_LOG(WebRTC, "UserMediaProcessManager::willCreateMediaStream - granting extension %s", id.utf8().data());

        if (withAudio)
            process.grantAudioCaptureExtension();
        if (withVideo)
            process.grantVideoCaptureExtension();
        process.send(Messages::WebProcess::GrantUserMediaDeviceSandboxExtensions(MediaDeviceSandboxExtensions(ids, WTFMove(handles))), 0);
    }
#else
    UNUSED_PARAM(proxy);
    UNUSED_PARAM(withAudio);
    UNUSED_PARAM(withVideo);
#endif

    proxy.page().activateMediaStreamCaptureInPage();

    return true;
}

void UserMediaProcessManager::revokeSandboxExtensionsIfNeeded(WebProcessProxy& process)
{
#if ENABLE(SANDBOX_EXTENSIONS)
    bool hasAudioCapture = false;
    bool hasVideoCapture = false;

    UserMediaPermissionRequestManagerProxy::forEach([&hasAudioCapture, &hasVideoCapture, &process](auto& managerProxy) {
        if (&process != &managerProxy.page().process())
            return;
        hasAudioCapture |= managerProxy.page().isCapturingAudio();
        hasVideoCapture |= managerProxy.page().isCapturingVideo();
    });

    if (hasAudioCapture && hasVideoCapture)
        return;

    Vector<String> params;
    if (!hasAudioCapture && process.hasAudioCaptureExtension()) {
        params.append(audioExtensionPath);
        process.revokeAudioCaptureExtension();
    }
    if (!hasVideoCapture && process.hasVideoCaptureExtension()) {
        params.append(videoExtensionPath);
        process.revokeVideoCaptureExtension();
    }

    if (params.isEmpty())
        return;

    for (const auto& id : params)
        RELEASE_LOG(WebRTC, "UserMediaProcessManager::endedCaptureSession - revoking extension %s", id.utf8().data());

    process.send(Messages::WebProcess::RevokeUserMediaDeviceSandboxExtensions(params), 0);
#endif
}

void UserMediaProcessManager::setCaptureEnabled(bool enabled)
{
    if (enabled == m_captureEnabled)
        return;

    m_captureEnabled = enabled;

    if (enabled)
        return;

    UserMediaPermissionRequestManagerProxy::forEach([](auto& proxy) {
        proxy.stopCapture();
    });
}

void UserMediaProcessManager::captureDevicesChanged()
{
    UserMediaPermissionRequestManagerProxy::forEach([](auto& proxy) {
        proxy.captureDevicesChanged();
    });
}

void UserMediaProcessManager::beginMonitoringCaptureDevices()
{
    static std::once_flag onceFlag;

    std::call_once(onceFlag, [this] {
        m_captureDevices = WebCore::RealtimeMediaSourceCenter::singleton().getMediaStreamDevices();

        WebCore::RealtimeMediaSourceCenter::singleton().setDevicesChangedObserver([this]() {
            auto oldDevices = WTFMove(m_captureDevices);
            m_captureDevices = WebCore::RealtimeMediaSourceCenter::singleton().getMediaStreamDevices();

            if (m_captureDevices.size() == oldDevices.size()) {
                bool haveChanges = false;
                for (auto &newDevice : m_captureDevices) {
                    if (newDevice.type() != WebCore::CaptureDevice::DeviceType::Camera && newDevice.type() != WebCore::CaptureDevice::DeviceType::Microphone)
                        continue;

                    auto index = oldDevices.findMatching([&newDevice] (auto& oldDevice) {
                        return newDevice.persistentId() == oldDevice.persistentId() && newDevice.enabled() != oldDevice.enabled();
                    });

                    if (index == notFound) {
                        haveChanges = true;
                        break;
                    }
                }

                if (!haveChanges)
                    return;
            }

            // When a device with camera and microphone is attached or detached, the CaptureDevice notification for
            // the different devices won't arrive at the same time so delay a bit so we can coalesce the callbacks.
            if (!m_debounceTimer.isActive())
                m_debounceTimer.startOneShot(deviceChangeDebounceTimerInterval);
        });
    });
}

} // namespace WebKit

#endif
