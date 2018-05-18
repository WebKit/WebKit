/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "MediaDeviceSandboxExtensions.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

#if ENABLE(SANDBOX_EXTENSIONS)
static const char* const audioExtensionPath = "com.apple.webkit.microphone";
static const char* const videoExtensionPath = "com.apple.webkit.camera";
#endif

class ProcessState {
public:
    ProcessState() { }
    ProcessState(const ProcessState&) = delete;

    void addRequestManager(UserMediaPermissionRequestManagerProxy&);
    void removeRequestManager(UserMediaPermissionRequestManagerProxy&);
    Vector<UserMediaPermissionRequestManagerProxy*>& managers() { return m_managers; }

    enum SandboxExtensionsGranted {
        None = 0,
        Video = 1 << 0,
        Audio = 1 << 1
    };

    SandboxExtensionsGranted sandboxExtensionsGranted() { return m_pageSandboxExtensionsGranted; }
    void setSandboxExtensionsGranted(unsigned granted) { m_pageSandboxExtensionsGranted = static_cast<SandboxExtensionsGranted>(granted); }

private:
    Vector<UserMediaPermissionRequestManagerProxy*> m_managers;
    SandboxExtensionsGranted m_pageSandboxExtensionsGranted { SandboxExtensionsGranted::None };
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
#if ENABLE(SANDBOX_EXTENSIONS)
    auto& processStartingCapture = proxy.page().process();

    ASSERT(stateMap().contains(&processStartingCapture));

    if (m_denyNextRequest) {
        m_denyNextRequest = false;
        return false;
    }

    auto& state = processState(processStartingCapture);
    size_t extensionCount = 0;
    unsigned requiredExtensions = ProcessState::SandboxExtensionsGranted::None;

    if (withAudio) {
        requiredExtensions |= ProcessState::SandboxExtensionsGranted::Audio;
        extensionCount++;
    }
    if (withVideo) {
        requiredExtensions |= ProcessState::SandboxExtensionsGranted::Video;
        extensionCount++;
    }

    unsigned currentExtensions = state.sandboxExtensionsGranted();

    if (!(requiredExtensions & currentExtensions)) {
        SandboxExtension::HandleArray handles;
        handles.allocate(extensionCount);

        Vector<String> ids;
        ids.reserveCapacity(extensionCount);

        if (withAudio && requiredExtensions & ProcessState::SandboxExtensionsGranted::Audio) {
            if (SandboxExtension::createHandleForGenericExtension(audioExtensionPath, handles[--extensionCount])) {
                ids.append(ASCIILiteral(audioExtensionPath));
                currentExtensions |= ProcessState::SandboxExtensionsGranted::Audio;
            }
        }

        if (withVideo && requiredExtensions & ProcessState::SandboxExtensionsGranted::Video) {
            if (SandboxExtension::createHandleForGenericExtension(videoExtensionPath, handles[--extensionCount])) {
                ids.append(ASCIILiteral(videoExtensionPath));
                currentExtensions |= ProcessState::SandboxExtensionsGranted::Video;
            }
        }

        if (ids.size() != handles.size()) {
            WTFLogAlways("Could not create a required sandbox extension, capture will fail!");
            return false;
        }

        state.setSandboxExtensionsGranted(currentExtensions);
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
        if (manager->page().hasActiveAudioStream())
            hasAudioCapture = true;
        if (manager->page().hasActiveVideoStream())
            hasVideoCapture = true;
    }

    if (hasAudioCapture && hasVideoCapture)
        return;

    Vector<String> params;
    unsigned currentExtensions = state.sandboxExtensionsGranted();
    if (!hasAudioCapture && currentExtensions & ProcessState::SandboxExtensionsGranted::Audio) {
        params.append(ASCIILiteral(audioExtensionPath));
        currentExtensions &= ~ProcessState::SandboxExtensionsGranted::Audio;
    }
    if (!hasVideoCapture && currentExtensions & ProcessState::SandboxExtensionsGranted::Video) {
        params.append(ASCIILiteral(videoExtensionPath));
        currentExtensions &= ~ProcessState::SandboxExtensionsGranted::Video;
    }

    if (params.isEmpty())
        return;

    state.setSandboxExtensionsGranted(currentExtensions);
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

} // namespace WebKit

#endif
