/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "RemoteRealtimeDisplaySource.h"
#include "RemoteRealtimeVideoSource.h"
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

void UserMediaCaptureManager::setupCaptureProcesses(bool shouldCaptureAudioInUIProcess, bool shouldCaptureAudioInGPUProcess, bool shouldCaptureVideoInUIProcess, bool shouldCaptureVideoInGPUProcess, bool shouldCaptureDisplayInUIProcess)
{
    // FIXME(rdar://84278146): Adopt AVCaptureSession attribution API for camera access in the web process if shouldCaptureVideoInGPUProcess is false.
    MockRealtimeMediaSourceCenter::singleton().setMockAudioCaptureEnabled(!shouldCaptureAudioInUIProcess && !shouldCaptureAudioInGPUProcess);
    MockRealtimeMediaSourceCenter::singleton().setMockVideoCaptureEnabled(!shouldCaptureVideoInUIProcess && !shouldCaptureVideoInGPUProcess);
    MockRealtimeMediaSourceCenter::singleton().setMockDisplayCaptureEnabled(!shouldCaptureDisplayInUIProcess);

    m_audioFactory.setShouldCaptureInGPUProcess(shouldCaptureAudioInGPUProcess);
    m_videoFactory.setShouldCaptureInGPUProcess(shouldCaptureVideoInGPUProcess);

    if (shouldCaptureAudioInGPUProcess) {
        WebCore::AudioMediaStreamTrackRendererUnit::setCreateInternalUnitFunction([](auto&& renderCallback, auto&& resetCallback) {
            return WebProcess::singleton().audioMediaStreamTrackRendererInternalUnitManager().createRemoteInternalUnit(WTFMove(renderCallback), WTFMove(resetCallback));
        });
    }

    if (shouldCaptureAudioInUIProcess || shouldCaptureAudioInGPUProcess)
        RealtimeMediaSourceCenter::singleton().setAudioCaptureFactory(m_audioFactory);
    if (shouldCaptureVideoInUIProcess || shouldCaptureVideoInGPUProcess)
        RealtimeMediaSourceCenter::singleton().setVideoCaptureFactory(m_videoFactory);
    if (shouldCaptureDisplayInUIProcess)
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

void UserMediaCaptureManager::addSource(Ref<RemoteRealtimeDisplaySource>&& source)
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

void UserMediaCaptureManager::sourceStopped(RealtimeMediaSourceIdentifier identifier)
{
    auto iterator = m_sources.find(identifier);
    if (iterator == m_sources.end())
        return;

    switchOn(iterator->value, [](Ref<RemoteRealtimeAudioSource>& source) {
        source->captureStopped();
    }, [](Ref<RemoteRealtimeVideoSource>& source) {
        source->captureStopped();
    }, [](Ref<RemoteRealtimeDisplaySource>& source) {
        source->captureStopped();
    }, [](std::nullptr_t) { });
}

void UserMediaCaptureManager::captureFailed(RealtimeMediaSourceIdentifier identifier)
{
    auto iterator = m_sources.find(identifier);
    if (iterator == m_sources.end())
        return;

    switchOn(iterator->value, [](Ref<RemoteRealtimeAudioSource>& source) {
        source->captureFailed();
    }, [](Ref<RemoteRealtimeVideoSource>& source) {
        source->captureFailed();
    }, [](Ref<RemoteRealtimeDisplaySource>& source) {
        source->captureFailed();
    }, [](std::nullptr_t) { });
}

void UserMediaCaptureManager::sourceMutedChanged(RealtimeMediaSourceIdentifier identifier, bool muted)
{
    auto iterator = m_sources.find(identifier);
    if (iterator == m_sources.end())
        return;

    switchOn(iterator->value, [muted](Ref<RemoteRealtimeAudioSource>& source) {
        source->sourceMutedChanged(muted);
    }, [muted](Ref<RemoteRealtimeVideoSource>& source) {
        source->sourceMutedChanged(muted);
    }, [muted](Ref<RemoteRealtimeDisplaySource>& source) {
        source->sourceMutedChanged(muted);
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
    }, [&](Ref<RemoteRealtimeDisplaySource>& source) {
        source->setSettings(WTFMove(settings));
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
    }, [&](Ref<RemoteRealtimeDisplaySource>& source) {
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
    }, [&](Ref<RemoteRealtimeDisplaySource>& source) {
        source->applyConstraintsFailed(WTFMove(failedConstraint), WTFMove(message));
    }, [](std::nullptr_t) { });
}

CaptureSourceOrError UserMediaCaptureManager::AudioFactory::createAudioCaptureSource(const CaptureDevice& device, String&& hashSalt, const MediaConstraints* constraints)
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

    return RemoteRealtimeAudioSource::create(device, constraints, { }, WTFMove(hashSalt), m_manager, m_shouldCaptureInGPUProcess);
}

void UserMediaCaptureManager::AudioFactory::setShouldCaptureInGPUProcess(bool value)
{
    m_shouldCaptureInGPUProcess = value;
}

CaptureSourceOrError UserMediaCaptureManager::VideoFactory::createVideoCaptureSource(const CaptureDevice& device, String&& hashSalt, const MediaConstraints* constraints)
{
#if !ENABLE(GPU_PROCESS)
    if (m_shouldCaptureInGPUProcess)
        return CaptureSourceOrError { "Video capture in GPUProcess is not implemented"_s };
#endif

    return CaptureSourceOrError(RealtimeVideoSource::create(RemoteRealtimeVideoSource::create(device, constraints, { }, WTFMove(hashSalt), m_manager, m_shouldCaptureInGPUProcess)));
}

CaptureSourceOrError UserMediaCaptureManager::DisplayFactory::createDisplayCaptureSource(const CaptureDevice& device, const MediaConstraints* constraints)
{
    return CaptureSourceOrError(RemoteRealtimeDisplaySource::create(device, constraints, { }, { }, m_manager, false));
}

}

#endif
