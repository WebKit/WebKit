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
#include "WebPageMessages.h"
#include "WebPageProxy.h"
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

class ProcessState {
public:
    ProcessState() { }
    ProcessState(const ProcessState&) = delete;

    void addRequestManager(UserMediaPermissionRequestManagerProxy&);
    void removeRequestManager(UserMediaPermissionRequestManagerProxy&);
    Vector<UserMediaPermissionRequestManagerProxy*>& managers() { return m_managers; }

    enum SandboxExtensionType : uint32_t {
        None = 0,
        Video = 1 << 0,
        Audio = 1 << 1
    };
    typedef uint32_t SandboxExtensionsGranted;

    bool hasVideoExtension() const { return m_pageSandboxExtensionsGranted & Video; }
    void grantVideoExtension()  { m_pageSandboxExtensionsGranted |= Video; }
    void revokeVideoExtension()  { m_pageSandboxExtensionsGranted &= ~Video; }

    bool hasAudioExtension() const { return m_pageSandboxExtensionsGranted & Audio; }
    void grantAudioExtension()  { m_pageSandboxExtensionsGranted |= Audio; }
    void revokeAudioExtension()  { m_pageSandboxExtensionsGranted &= ~Audio; }

private:

    Vector<UserMediaPermissionRequestManagerProxy*> m_managers;
    SandboxExtensionsGranted m_pageSandboxExtensionsGranted { SandboxExtensionType::None };
};

static HashMap<WebProcessProxy*, std::unique_ptr<ProcessState>>& stateMap()
{
    static NeverDestroyed<HashMap<WebProcessProxy*, std::unique_ptr<ProcessState>>> map;
    return map;
}

static ProcessState& processState(WebProcessProxy& process)
{
    auto& state = stateMap().add(&process, nullptr).iterator->value;
    if (state)
        return *state;

    state = std::make_unique<ProcessState>();
    return *state;
}

void ProcessState::addRequestManager(UserMediaPermissionRequestManagerProxy& proxy)
{
    ASSERT(!m_managers.contains(&proxy));
    m_managers.append(&proxy);
}

void ProcessState::removeRequestManager(UserMediaPermissionRequestManagerProxy& proxy)
{
    ASSERT(m_managers.contains(&proxy));
    m_managers.removeFirstMatching([&proxy](auto other) {
        return other == &proxy;
    });
}

UserMediaProcessManager& UserMediaProcessManager::singleton()
{
    static NeverDestroyed<UserMediaProcessManager> manager;
    return manager;
}

UserMediaProcessManager::UserMediaProcessManager()
    : m_debounceTimer(RunLoop::main(), this, &UserMediaProcessManager::captureDevicesChanged)
{
}

void UserMediaProcessManager::addUserMediaPermissionRequestManagerProxy(UserMediaPermissionRequestManagerProxy& proxy)
{
    processState(proxy.page().process()).addRequestManager(proxy);
}

void UserMediaProcessManager::removeUserMediaPermissionRequestManagerProxy(UserMediaPermissionRequestManagerProxy& proxy)
{
    endedCaptureSession(proxy);

    auto& state = processState(proxy.page().process());
    state.removeRequestManager(proxy);
    if (state.managers().isEmpty()) {
        auto it = stateMap().find(&proxy.page().process());
        stateMap().remove(it);
    }
}

void UserMediaProcessManager::muteCaptureMediaStreamsExceptIn(WebPageProxy& pageStartingCapture)
{
#if PLATFORM(COCOA)
    for (auto& state : stateMap()) {
        for (auto& manager : state.value->managers()) {
            if (&manager->page() == &pageStartingCapture)
                continue;
            manager->page().setMediaStreamCaptureMuted(true);
        }
    }
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
    
    if (proxy.page().preferences().mockCaptureDevicesEnabled())
        return true;
    
#if ENABLE(SANDBOX_EXTENSIONS) && USE(APPLE_INTERNAL_SDK)
    auto& processStartingCapture = proxy.page().process();

    ASSERT(stateMap().contains(&processStartingCapture));

    auto& state = processState(processStartingCapture);
    size_t extensionCount = 0;

    if (withAudio && !state.hasAudioExtension())
        extensionCount++;
    else
        withAudio = false;

    if (withVideo && !state.hasVideoExtension())
        extensionCount++;
    else
        withVideo = false;

    if (extensionCount) {
        SandboxExtension::HandleArray handles;
        handles.allocate(extensionCount);

        Vector<String> ids;
        ids.reserveCapacity(extensionCount);

        if (withAudio && SandboxExtension::createHandleForGenericExtension(audioExtensionPath, handles[--extensionCount]))
            ids.append(audioExtensionPath);

        if (withVideo && SandboxExtension::createHandleForGenericExtension(videoExtensionPath, handles[--extensionCount]))
            ids.append(videoExtensionPath);

        if (ids.size() != handles.size()) {
            WTFLogAlways("Could not create a required sandbox extension, capture will fail!");
            return false;
        }

        for (const auto& id : ids)
            RELEASE_LOG(WebRTC, "UserMediaProcessManager::willCreateMediaStream - granting extension %s", id.utf8().data());

        if (withAudio)
            state.grantAudioExtension();
        if (withVideo)
            state.grantVideoExtension();
        processStartingCapture.send(Messages::WebPage::GrantUserMediaDeviceSandboxExtensions(MediaDeviceSandboxExtensions(ids, WTFMove(handles))), proxy.page().pageID());
    }
#else
    UNUSED_PARAM(proxy);
    UNUSED_PARAM(withAudio);
    UNUSED_PARAM(withVideo);
#endif

    proxy.page().activateMediaStreamCaptureInPage();

    return true;
}

void UserMediaProcessManager::startedCaptureSession(UserMediaPermissionRequestManagerProxy& proxy)
{
    ASSERT(stateMap().contains(&proxy.page().process()));
}

void UserMediaProcessManager::endedCaptureSession(UserMediaPermissionRequestManagerProxy& proxy)
{
#if ENABLE(SANDBOX_EXTENSIONS)
    ASSERT(stateMap().contains(&proxy.page().process()));

    auto& state = processState(proxy.page().process());
    bool hasAudioCapture = false;
    bool hasVideoCapture = false;
    for (auto& manager : state.managers()) {
        if (manager == &proxy)
            continue;
        if (manager->page().hasActiveAudioStream())
            hasAudioCapture = true;
        if (manager->page().hasActiveVideoStream())
            hasVideoCapture = true;
    }

    if (hasAudioCapture && hasVideoCapture)
        return;

    Vector<String> params;
    if (!hasAudioCapture && state.hasAudioExtension()) {
        params.append(audioExtensionPath);
        state.revokeAudioExtension();
    }
    if (!hasVideoCapture && state.hasVideoExtension()) {
        params.append(videoExtensionPath);
        state.revokeVideoExtension();
    }

    if (params.isEmpty())
        return;

    for (const auto& id : params)
        RELEASE_LOG(WebRTC, "UserMediaProcessManager::endedCaptureSession - revoking extension %s", id.utf8().data());

    proxy.page().process().send(Messages::WebPage::RevokeUserMediaDeviceSandboxExtensions(params), proxy.page().pageID());
#endif
}

void UserMediaProcessManager::setCaptureEnabled(bool enabled)
{
    if (enabled == m_captureEnabled)
        return;

    m_captureEnabled = enabled;

    if (enabled)
        return;

    for (auto& state : stateMap()) {
        for (auto& manager : state.value->managers())
            manager->stopCapture();
    }
}

void UserMediaProcessManager::captureDevicesChanged()
{
    auto& map = stateMap();
    for (auto& state : map) {
        auto* process = state.key;
        for (auto& manager : state.value->managers()) {
            if (map.find(process) == map.end())
                break;
            manager->captureDevicesChanged();
        }
    }
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
