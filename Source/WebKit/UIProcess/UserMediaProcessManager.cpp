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
#include "SpeechRecognitionPermissionManager.h"
#include "WebPageProxy.h"
#include "WebProcessMessages.h"
#include "WebProcessProxy.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TranslatedProcess.h>

namespace WebKit {

#if ENABLE(SANDBOX_EXTENSIONS)
static const ASCIILiteral audioExtensionPath { "com.apple.webkit.microphone"_s };
static const ASCIILiteral videoExtensionPath { "com.apple.webkit.camera"_s };
static const ASCIILiteral appleCameraServicePath { "com.apple.applecamerad"_s };
static const ASCIILiteral additionalAppleCameraServicePath { "com.apple.appleh13camerad"_s };
#endif

UserMediaProcessManager& UserMediaProcessManager::singleton()
{
    static NeverDestroyed<UserMediaProcessManager> manager;
    return manager;
}

UserMediaProcessManager::UserMediaProcessManager()
{
}

#if ENABLE(SANDBOX_EXTENSIONS)
static bool needsAppleCameraService()
{
#if !PLATFORM(MAC) && !PLATFORM(MACCATALYST)
    return false;
#elif CPU(ARM64)
    return true;
#else
    return WTF::isX86BinaryRunningOnARM();
#endif
}
#endif

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

    bool needsAudioSandboxExtension = withAudio && !process.hasAudioCaptureExtension() && !proxy.page().preferences().captureAudioInUIProcessEnabled() && !proxy.page().preferences().captureAudioInGPUProcessEnabled();
    if (needsAudioSandboxExtension)
        extensionCount++;

    bool needsVideoSandboxExtension = withVideo && !process.hasVideoCaptureExtension() && !proxy.page().preferences().captureVideoInUIProcessEnabled() && !proxy.page().preferences().captureVideoInGPUProcessEnabled();
    if (needsVideoSandboxExtension)
        extensionCount++;

    bool needsAppleCameraSandboxExtension = needsVideoSandboxExtension && needsAppleCameraService();
    if (needsAppleCameraSandboxExtension) {
        extensionCount++;
#if HAVE(ADDITIONAL_APPLE_CAMERA_SERVICE)
        extensionCount++;
#endif
    }

    if (extensionCount) {
        Vector<SandboxExtension::Handle> handles;
        Vector<String> ids;

        if (!proxy.page().preferences().mockCaptureDevicesEnabled()) {
            handles.resize(extensionCount);
            ids.reserveInitialCapacity(extensionCount);

            if (needsAudioSandboxExtension) {
                if (auto handle = SandboxExtension::createHandleForGenericExtension(audioExtensionPath)) {
                    handles[--extensionCount] = WTFMove(*handle);
                    ids.uncheckedAppend(audioExtensionPath);
                }
            }

            if (needsVideoSandboxExtension) {
                if (auto handle = SandboxExtension::createHandleForGenericExtension(videoExtensionPath)) {
                    handles[--extensionCount] = WTFMove(*handle);
                    ids.uncheckedAppend(videoExtensionPath);
                }
            }

            auto auditToken = process.auditToken();
            if (needsAppleCameraSandboxExtension) {
                if (auto handle = SandboxExtension::createHandleForMachLookup(appleCameraServicePath, auditToken, SandboxExtension::MachBootstrapOptions::EnableMachBootstrap)) {
                    handles[--extensionCount] = WTFMove(*handle);
                    ids.uncheckedAppend(appleCameraServicePath);
                }
#if HAVE(ADDITIONAL_APPLE_CAMERA_SERVICE)
                if (auto handle = SandboxExtension::createHandleForMachLookup(additionalAppleCameraServicePath, auditToken, SandboxExtension::MachBootstrapOptions::EnableMachBootstrap)) {
                    handles[--extensionCount] = WTFMove(*handle);
                    ids.uncheckedAppend(additionalAppleCameraServicePath);
                }
#endif
            }

            if (ids.size() != handles.size()) {
                WTFLogAlways("Could not create a required sandbox extension, capture will fail!");
                return false;
            }

            // FIXME: Is it correct to ensure that the corresponding entries in `handles` and `ids` are in reverse order?
        }

        for (const auto& id : ids)
            RELEASE_LOG(WebRTC, "UserMediaProcessManager::willCreateMediaStream - granting extension %s", id.utf8().data());

        if (needsAudioSandboxExtension)
            process.grantAudioCaptureExtension();
        if (needsVideoSandboxExtension)
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
    bool hasPendingCapture = false;

    UserMediaPermissionRequestManagerProxy::forEach([&hasAudioCapture, &hasVideoCapture, &hasPendingCapture, &process](auto& managerProxy) {
        if (&process != &managerProxy.page().process())
            return;
        hasAudioCapture |= managerProxy.page().isCapturingAudio();
        hasVideoCapture |= managerProxy.page().isCapturingVideo();
        hasPendingCapture |= managerProxy.hasPendingCapture();
    });

    if (hasPendingCapture)
        return;

    if (hasAudioCapture && hasVideoCapture)
        return;

    Vector<String> params;
    if (!hasAudioCapture && process.hasAudioCaptureExtension()) {
        params.append(audioExtensionPath);
        process.revokeAudioCaptureExtension();
    }
    if (!hasVideoCapture && process.hasVideoCaptureExtension()) {
        params.append(videoExtensionPath);
        if (needsAppleCameraService()) {
            params.append(appleCameraServicePath);
#if USE(APPLE_INTERNAL_SDK) && HAVE(ADDITIONAL_APPLE_CAMERA_SERVICE)
            params.append(additionalAppleCameraServicePath);
#endif
        }
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

void UserMediaProcessManager::updateCaptureDevices(ShouldNotify shouldNotify)
{
    WebCore::RealtimeMediaSourceCenter::singleton().getMediaStreamDevices([weakThis = WeakPtr { *this }, this, shouldNotify](auto&& newDevices) mutable {
        if (!weakThis)
            return;

        if (!haveDevicesChanged(m_captureDevices, newDevices))
            return;

        m_captureDevices = WTFMove(newDevices);
        if (shouldNotify == ShouldNotify::Yes)
            captureDevicesChanged();
    });
}

void UserMediaProcessManager::devicesChanged()
{
    updateCaptureDevices(ShouldNotify::Yes);
}

void UserMediaProcessManager::beginMonitoringCaptureDevices()
{
    static std::once_flag onceFlag;

    std::call_once(onceFlag, [this] {
        updateCaptureDevices(ShouldNotify::No);
        WebCore::RealtimeMediaSourceCenter::singleton().addDevicesChangedObserver(*this);
    });
}

} // namespace WebKit

#endif
