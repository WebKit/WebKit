/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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
#include "UserMediaCaptureManager.h"

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

#include "AudioMediaStreamTrackRendererInternalUnitManager.h"
#include "GPUProcessConnection.h"
#include "RemoteRealtimeAudioSource.h"
#include "RemoteRealtimeVideoSource.h"
#include "RemoteVideoFrameObjectHeapProxy.h"
#include "UserMediaCaptureManagerMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/AudioMediaStreamTrackRendererUnit.h>
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <WebCore/RealtimeVideoSource.h>
#include <wtf/Assertions.h>

namespace WebKit {
using namespace PAL;
using namespace WebCore;

UserMediaCaptureManager::UserMediaCaptureManager(WebProcess& process)
    : m_process(process)
    , m_audioFactory(*this)
    , m_videoFactory(*this)
    , m_displayFactory(*this)
{
    m_process.addMessageReceiver(Messages::UserMediaCaptureManager::messageReceiverName(), *this);
}

UserMediaCaptureManager::~UserMediaCaptureManager()
{
    RealtimeMediaSourceCenter::singleton().unsetAudioCaptureFactory(m_audioFactory);
    RealtimeMediaSourceCenter::singleton().unsetDisplayCaptureFactory(m_displayFactory);
    RealtimeMediaSourceCenter::singleton().unsetVideoCaptureFactory(m_videoFactory);
    m_process.removeMessageReceiver(Messages::UserMediaCaptureManager::messageReceiverName());
    m_remoteCaptureSampleManager.stopListeningForIPC();
}

const char* UserMediaCaptureManager::supplementName()
{
    return "UserMediaCaptureManager";
}

void UserMediaCaptureManager::setupCaptureProcesses(bool shouldCaptureAudioInUIProcess, bool shouldCaptureAudioInGPUProcess, bool shouldCaptureVideoInUIProcess, bool shouldCaptureVideoInGPUProcess, bool shouldCaptureDisplayInUIProcess, bool shouldCaptureDisplayInGPUProcess, bool shouldUseGPUProcessRemoteFrames)
{
    m_shouldUseGPUProcessRemoteFrames = shouldUseGPUProcessRemoteFrames;
    // FIXME(rdar://84278146): Adopt AVCaptureSession attribution API for camera access in the web process if shouldCaptureVideoInGPUProcess is false.
    MockRealtimeMediaSourceCenter::singleton().setMockAudioCaptureEnabled(!shouldCaptureAudioInUIProcess && !shouldCaptureAudioInGPUProcess);
    MockRealtimeMediaSourceCenter::singleton().setMockVideoCaptureEnabled(!shouldCaptureVideoInUIProcess && !shouldCaptureVideoInGPUProcess);
    MockRealtimeMediaSourceCenter::singleton().setMockDisplayCaptureEnabled(!shouldCaptureDisplayInUIProcess && !shouldCaptureDisplayInGPUProcess);

    m_audioFactory.setShouldCaptureInGPUProcess(shouldCaptureAudioInGPUProcess);
    m_videoFactory.setShouldCaptureInGPUProcess(shouldCaptureVideoInGPUProcess);
    m_displayFactory.setShouldCaptureInGPUProcess(shouldCaptureDisplayInGPUProcess);

    if (shouldCaptureAudioInUIProcess || shouldCaptureAudioInGPUProcess)
        WebCore::AudioMediaStreamTrackRendererInternalUnit::setCreateFunction(createRemoteAudioMediaStreamTrackRendererInternalUnitProxy);

    if (shouldCaptureAudioInUIProcess || shouldCaptureAudioInGPUProcess)
        RealtimeMediaSourceCenter::singleton().setAudioCaptureFactory(m_audioFactory);
    if (shouldCaptureVideoInUIProcess || shouldCaptureVideoInGPUProcess)
        RealtimeMediaSourceCenter::singleton().setVideoCaptureFactory(m_videoFactory);
    if (shouldCaptureDisplayInUIProcess || shouldCaptureDisplayInGPUProcess)
        RealtimeMediaSourceCenter::singleton().setDisplayCaptureFactory(m_displayFactory);
}

void UserMediaCaptureManager::addSource(Ref<RemoteRealtimeAudioSource>&& source)
{
    auto identifier = source->identifier();
    ASSERT(!m_sources.contains(identifier));
    m_sources.add(identifier, Source(WTFMove(source)));
}

void UserMediaCaptureManager::addSource(Ref<RemoteRealtimeVideoSource>&& source)
{
    auto identifier = source->identifier();
    ASSERT(!m_sources.contains(identifier));
    m_sources.add(identifier, Source(WTFMove(source)));
}

void UserMediaCaptureManager::removeSource(RealtimeMediaSourceIdentifier identifier)
{
    ASSERT(m_sources.contains(identifier));
    m_sources.remove(identifier);
}

void UserMediaCaptureManager::sourceStopped(RealtimeMediaSourceIdentifier identifier, bool didFail)
{
    auto iterator = m_sources.find(identifier);
    if (iterator == m_sources.end())
        return;

    switchOn(iterator->value, [didFail](Ref<RemoteRealtimeAudioSource>& source) {
        source->captureStopped(didFail);
    }, [didFail](Ref<RemoteRealtimeVideoSource>& source) {
        source->captureStopped(didFail);
    }, [](std::nullptr_t) { });
}

void UserMediaCaptureManager::sourceMutedChanged(RealtimeMediaSourceIdentifier identifier, bool muted, bool interrupted)
{
    auto iterator = m_sources.find(identifier);
    if (iterator == m_sources.end())
        return;

    switchOn(iterator->value, [muted, interrupted](Ref<RemoteRealtimeAudioSource>& source) {
        source->sourceMutedChanged(muted, interrupted);
    }, [muted, interrupted](Ref<RemoteRealtimeVideoSource>& source) {
        source->sourceMutedChanged(muted, interrupted);
    }, [](std::nullptr_t) { });
}

void UserMediaCaptureManager::sourceSettingsChanged(RealtimeMediaSourceIdentifier identifier, RealtimeMediaSourceSettings&& settings)
{
    auto iterator = m_sources.find(identifier);
    if (iterator == m_sources.end())
        return;

    switchOn(iterator->value, [&](Ref<RemoteRealtimeAudioSource>& source) {
        source->setSettings(WTFMove(settings));
    }, [&](Ref<RemoteRealtimeVideoSource>& source) {
        source->setSettings(WTFMove(settings));
    }, [](std::nullptr_t) { });
}

void UserMediaCaptureManager::sourceConfigurationChanged(RealtimeMediaSourceIdentifier identifier, String&& persistentID, RealtimeMediaSourceSettings&& settings, RealtimeMediaSourceCapabilities&& capabilities)
{
    auto iterator = m_sources.find(identifier);
    if (iterator == m_sources.end())
        return;

    switchOn(iterator->value, [&](Ref<RemoteRealtimeAudioSource>& source) {
        source->configurationChanged(WTFMove(persistentID), WTFMove(settings), WTFMove(capabilities));
    }, [&](Ref<RemoteRealtimeVideoSource>& source) {
        source->configurationChanged(WTFMove(persistentID), WTFMove(settings), WTFMove(capabilities));
    }, [](std::nullptr_t) { });
}

void UserMediaCaptureManager::applyConstraintsSucceeded(RealtimeMediaSourceIdentifier identifier, RealtimeMediaSourceSettings&& settings)
{
    auto iterator = m_sources.find(identifier);
    if (iterator == m_sources.end())
        return;

    switchOn(iterator->value, [&](Ref<RemoteRealtimeAudioSource>& source) {
        source->applyConstraintsSucceeded(WTFMove(settings));
    }, [&](Ref<RemoteRealtimeVideoSource>& source) {
        source->applyConstraintsSucceeded(WTFMove(settings));
    }, [](std::nullptr_t) { });
}

void UserMediaCaptureManager::applyConstraintsFailed(RealtimeMediaSourceIdentifier identifier, String&& failedConstraint, String&& message)
{
    auto iterator = m_sources.find(identifier);
    if (iterator == m_sources.end())
        return;

    switchOn(iterator->value, [&](Ref<RemoteRealtimeAudioSource>& source) {
        source->applyConstraintsFailed(WTFMove(failedConstraint), WTFMove(message));
    }, [&](Ref<RemoteRealtimeVideoSource>& source) {
        source->applyConstraintsFailed(WTFMove(failedConstraint), WTFMove(message));
    }, [](std::nullptr_t) { });
}

CaptureSourceOrError UserMediaCaptureManager::AudioFactory::createAudioCaptureSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, PageIdentifier pageIdentifier)
{
#if !ENABLE(GPU_PROCESS)
    if (m_shouldCaptureInGPUProcess)
        return CaptureSourceOrError { "Audio capture in GPUProcess is not implemented"_s };
#endif

#if PLATFORM(IOS_FAMILY) || ENABLE(ROUTING_ARBITRATION)
    // FIXME: Remove disabling of the audio session category management once we move all media playing to GPUProcess.
    if (m_shouldCaptureInGPUProcess)
        DeprecatedGlobalSettings::setShouldManageAudioSessionCategory(true);
#endif

    return RemoteRealtimeAudioSource::create(device, constraints, WTFMove(hashSalts), m_manager, m_shouldCaptureInGPUProcess, pageIdentifier);
}

void UserMediaCaptureManager::AudioFactory::setShouldCaptureInGPUProcess(bool value)
{
    m_shouldCaptureInGPUProcess = value;
}

void UserMediaCaptureManager::VideoFactory::setShouldCaptureInGPUProcess(bool value)
{
    m_shouldCaptureInGPUProcess = value;
}

CaptureSourceOrError UserMediaCaptureManager::VideoFactory::createVideoCaptureSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, PageIdentifier pageIdentifier)
{
#if !ENABLE(GPU_PROCESS)
    if (m_shouldCaptureInGPUProcess)
        return CaptureSourceOrError { "Video capture in GPUProcess is not implemented"_s };
#endif
    if (m_shouldCaptureInGPUProcess)
        m_manager.m_remoteCaptureSampleManager.setVideoFrameObjectHeapProxy(&WebProcess::singleton().ensureGPUProcessConnection().videoFrameObjectHeapProxy());

    return RemoteRealtimeVideoSource::create(device, constraints, WTFMove(hashSalts), m_manager, m_shouldCaptureInGPUProcess, pageIdentifier);
}

CaptureSourceOrError UserMediaCaptureManager::DisplayFactory::createDisplayCaptureSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, PageIdentifier pageIdentifier)
{
#if !ENABLE(GPU_PROCESS)
    if (m_shouldCaptureInGPUProcess)
        return CaptureSourceOrError { "Display capture in GPUProcess is not implemented"_s };
#endif
    if (m_shouldCaptureInGPUProcess)
        m_manager.m_remoteCaptureSampleManager.setVideoFrameObjectHeapProxy(&WebProcess::singleton().ensureGPUProcessConnection().videoFrameObjectHeapProxy());

    return RemoteRealtimeVideoSource::create(device, constraints, WTFMove(hashSalts), m_manager, m_shouldCaptureInGPUProcess, pageIdentifier);
}

void UserMediaCaptureManager::DisplayFactory::setShouldCaptureInGPUProcess(bool value)
{
    m_shouldCaptureInGPUProcess = value;
}


}

#endif // PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
