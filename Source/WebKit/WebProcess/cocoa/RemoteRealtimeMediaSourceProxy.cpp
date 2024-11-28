/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
#include "RemoteRealtimeMediaSourceProxy.h"

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

#include "GPUProcessConnection.h"
#include "SharedCARingBuffer.h"
#include "UserMediaCaptureManager.h"
#include "UserMediaCaptureManagerMessages.h"
#include "UserMediaCaptureManagerProxyMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/MediaConstraints.h>
#include <WebCore/RealtimeMediaSource.h>
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <WebCore/WebAudioBufferList.h>
#include <wtf/NativePromise.h>

namespace WebKit {
using namespace WebCore;

static Ref<IPC::Connection> getSourceConnection(bool shouldCaptureInGPUProcess)
{
    ASSERT(isMainRunLoop());
#if ENABLE(GPU_PROCESS)
    if (shouldCaptureInGPUProcess)
        return WebProcess::singleton().ensureGPUProcessConnection().connection();
#endif
    return *WebProcess::singleton().parentProcessConnection();
}

RemoteRealtimeMediaSourceProxy::RemoteRealtimeMediaSourceProxy(WebCore::RealtimeMediaSourceIdentifier identifier, const WebCore::CaptureDevice& device, bool shouldCaptureInGPUProcess, const WebCore::MediaConstraints* constraints)
    : m_identifier(identifier)
    , m_connection(getSourceConnection(shouldCaptureInGPUProcess))
    , m_device(device)
    , m_shouldCaptureInGPUProcess(shouldCaptureInGPUProcess)
{
    if (constraints)
        m_constraints = *constraints;
}

RemoteRealtimeMediaSourceProxy::~RemoteRealtimeMediaSourceProxy()
{
    failApplyConstraintCallbacks("Source terminated"_s);
}

void RemoteRealtimeMediaSourceProxy::updateConnection()
{
    m_connection = getSourceConnection(m_shouldCaptureInGPUProcess);
}

void RemoteRealtimeMediaSourceProxy::startProducingData(WebCore::PageIdentifier pageIdentifier)
{
    m_interrupted = false;
    m_connection->send(Messages::UserMediaCaptureManagerProxy::StartProducingData { m_identifier, pageIdentifier }, 0);
}

void RemoteRealtimeMediaSourceProxy::stopProducingData()
{
    m_interrupted = false;
    m_connection->send(Messages::UserMediaCaptureManagerProxy::StopProducingData { m_identifier }, 0);
}

void RemoteRealtimeMediaSourceProxy::endProducingData()
{
    m_connection->send(Messages::UserMediaCaptureManagerProxy::EndProducingData { m_identifier }, 0);
}

void RemoteRealtimeMediaSourceProxy::createRemoteMediaSource(const MediaDeviceHashSalts& deviceIDHashSalts, WebCore::PageIdentifier pageIdentifier, CreateCallback&& callback, bool shouldUseRemoteFrame)
{
    m_connection->sendWithAsyncReply(Messages::UserMediaCaptureManagerProxy::CreateMediaSourceForCaptureDeviceWithConstraints(identifier(), m_device, deviceIDHashSalts, m_constraints, shouldUseRemoteFrame, pageIdentifier), WTFMove(callback));
}

RemoteRealtimeMediaSourceProxy RemoteRealtimeMediaSourceProxy::clone()
{
    RemoteRealtimeMediaSourceProxy clone = { RealtimeMediaSourceIdentifier::generate(), m_device, m_shouldCaptureInGPUProcess, &m_constraints };

    clone.m_interrupted = m_interrupted;
    clone.m_isEnded = m_isEnded;

    return clone;
}

void RemoteRealtimeMediaSourceProxy::createRemoteCloneSource(WebCore::RealtimeMediaSourceIdentifier cloneIdentifier, WebCore::PageIdentifier pageIdentifier)
{
    m_connection->send(Messages::UserMediaCaptureManagerProxy::Clone { m_identifier, cloneIdentifier, pageIdentifier }, 0);
}

void RemoteRealtimeMediaSourceProxy::applyConstraints(const MediaConstraints& constraints, RealtimeMediaSource::ApplyConstraintsHandler&& completionHandler)
{
    m_pendingApplyConstraintsRequests.append(std::make_pair(WTFMove(completionHandler), constraints));
    // FIXME: Use sendAsyncWithReply.
    m_connection->send(Messages::UserMediaCaptureManagerProxy::ApplyConstraints { m_identifier, constraints }, 0);
}

struct RemoteRealtimeMediaSourceProxy::PromiseConverter {
    static auto convertError(IPC::Error)
    {
        return makeUnexpected(String { "IPC Connection closed"_s });
    }
};

Ref<WebCore::RealtimeMediaSource::TakePhotoNativePromise> RemoteRealtimeMediaSourceProxy::takePhoto(PhotoSettings&& settings)
{
    return m_connection->sendWithPromisedReply<PromiseConverter>(Messages::UserMediaCaptureManagerProxy::TakePhoto { identifier(), WTFMove(settings) });
}

Ref<WebCore::RealtimeMediaSource::PhotoCapabilitiesNativePromise> RemoteRealtimeMediaSourceProxy::getPhotoCapabilities()
{
    return m_connection->sendWithPromisedReply<PromiseConverter>(Messages::UserMediaCaptureManagerProxy::GetPhotoCapabilities { identifier() });
}

Ref<WebCore::RealtimeMediaSource::PhotoSettingsNativePromise> RemoteRealtimeMediaSourceProxy::getPhotoSettings()
{
    return m_connection->sendWithPromisedReply<PromiseConverter>(Messages::UserMediaCaptureManagerProxy::GetPhotoSettings { identifier() });
}

void RemoteRealtimeMediaSourceProxy::applyConstraintsSucceeded()
{
    auto request = m_pendingApplyConstraintsRequests.takeFirst();
    m_constraints = WTFMove(request.second);
    request.first({ });
}

void RemoteRealtimeMediaSourceProxy::applyConstraintsFailed(WebCore::MediaConstraintType invalidConstraint, String&& errorMessage)
{
    auto callback = m_pendingApplyConstraintsRequests.takeFirst().first;
    callback(RealtimeMediaSource::ApplyConstraintsError { invalidConstraint, WTFMove(errorMessage) });
}

void RemoteRealtimeMediaSourceProxy::failApplyConstraintCallbacks(const String& errorMessage)
{
    auto requests = WTFMove(m_pendingApplyConstraintsRequests);
    while (!requests.isEmpty())
        requests.takeFirst().first(RealtimeMediaSource::ApplyConstraintsError { { }, errorMessage });
}

void RemoteRealtimeMediaSourceProxy::end()
{
    ASSERT(!m_isEnded);
    m_isEnded = true;
    m_connection->send(Messages::UserMediaCaptureManagerProxy::RemoveSource { m_identifier }, 0);
}

void RemoteRealtimeMediaSourceProxy::whenReady(CompletionHandler<void(WebCore::CaptureSourceError&&)>&& callback)
{
    if (m_isReady)
        return callback(WebCore::CaptureSourceError(m_failureReason));

    if (m_callback) {
        callback = [previousCallbacks = std::exchange(m_callback, { }), newCallback = WTFMove(callback)] (auto&& error) mutable {
            previousCallbacks(WebCore::CaptureSourceError { error });
            newCallback(WTFMove(error));
        };
    }

    m_callback = WTFMove(callback);
}

void RemoteRealtimeMediaSourceProxy::setAsReady()
{
    ASSERT(!m_isReady);
    m_isReady = true;
    if (m_callback)
        m_callback({ });
}

void RemoteRealtimeMediaSourceProxy::didFail(CaptureSourceError&& reason)
{
    m_isReady = true;
    m_failureReason = WTFMove(reason);
    if (m_callback)
        m_callback(WebCore::CaptureSourceError(m_failureReason));
}

bool RemoteRealtimeMediaSourceProxy::isPowerEfficient() const
{
    auto syncResult = m_connection->sendSync(Messages::UserMediaCaptureManagerProxy::IsPowerEfficient { identifier() }, 0, GPUProcessConnection::defaultTimeout);
    auto [isPowerEfficient] = syncResult.takeReplyOr(false);
    return isPowerEfficient;
}

}

#endif
