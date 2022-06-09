/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "RemoteSourceBufferProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)

#include "Connection.h"
#include "InitializationSegmentInfo.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteSourceBufferProxyMessages.h"
#include "SharedBufferReference.h"
#include "SourceBufferPrivateRemoteMessages.h"
#include <WebCore/AudioTrackPrivate.h>
#include <WebCore/ContentType.h>
#include <WebCore/MediaDescription.h>
#include <WebCore/PlatformTimeRanges.h>
#include <WebCore/VideoTrackPrivate.h>
#include <wtf/Scope.h>

namespace WebKit {

using namespace WebCore;

Ref<RemoteSourceBufferProxy> RemoteSourceBufferProxy::create(GPUConnectionToWebProcess& connectionToWebProcess, RemoteSourceBufferIdentifier identifier, Ref<SourceBufferPrivate>&& sourceBufferPrivate, RemoteMediaPlayerProxy& remoteMediaPlayerProxy)
{
    auto remoteSourceBufferProxy = adoptRef(*new RemoteSourceBufferProxy(connectionToWebProcess, identifier, WTFMove(sourceBufferPrivate), remoteMediaPlayerProxy));
    return remoteSourceBufferProxy;
}

RemoteSourceBufferProxy::RemoteSourceBufferProxy(GPUConnectionToWebProcess& connectionToWebProcess, RemoteSourceBufferIdentifier identifier, Ref<SourceBufferPrivate>&& sourceBufferPrivate, RemoteMediaPlayerProxy& remoteMediaPlayerProxy)
    : m_connectionToWebProcess(connectionToWebProcess)
    , m_identifier(identifier)
    , m_sourceBufferPrivate(WTFMove(sourceBufferPrivate))
    , m_remoteMediaPlayerProxy(remoteMediaPlayerProxy)
{
    m_connectionToWebProcess->messageReceiverMap().addMessageReceiver(Messages::RemoteSourceBufferProxy::messageReceiverName(), m_identifier.toUInt64(), *this);
    m_sourceBufferPrivate->setClient(this);
    m_sourceBufferPrivate->setIsAttached(true);
}

RemoteSourceBufferProxy::~RemoteSourceBufferProxy()
{
    m_sourceBufferPrivate->setIsAttached(false);
    m_connectionToWebProcess->messageReceiverMap().removeMessageReceiver(Messages::RemoteSourceBufferProxy::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteSourceBufferProxy::sourceBufferPrivateDidReceiveInitializationSegment(InitializationSegment&& segment, CompletionHandler<void()>&& completionHandler)
{
    if (!m_remoteMediaPlayerProxy) {
        completionHandler();
        return;
    }

    InitializationSegmentInfo segmentInfo;
    segmentInfo.duration = segment.duration;
    for (auto& audioTrackInfo : segment.audioTracks) {
        auto identifier = m_remoteMediaPlayerProxy->addRemoteAudioTrackProxy(*audioTrackInfo.track);
        segmentInfo.audioTracks.append({ MediaDescriptionInfo(*audioTrackInfo.description), identifier });

        ASSERT(!m_trackIds.contains(identifier));
        ASSERT(!m_mediaDescriptions.contains(identifier));
        m_trackIds.add(identifier, audioTrackInfo.track->id());
        m_mediaDescriptions.add(identifier, *audioTrackInfo.description);
    }

    for (auto& videoTrackInfo : segment.videoTracks) {
        auto identifier = m_remoteMediaPlayerProxy->addRemoteVideoTrackProxy(*videoTrackInfo.track);
        segmentInfo.videoTracks.append({ MediaDescriptionInfo(*videoTrackInfo.description), identifier });

        ASSERT(!m_trackIds.contains(identifier));
        ASSERT(!m_mediaDescriptions.contains(identifier));
        m_trackIds.add(identifier, videoTrackInfo.track->id());
        m_mediaDescriptions.add(identifier, *videoTrackInfo.description);
    }

    for (auto& textTrackInfo : segment.textTracks) {
        auto identifier = m_remoteMediaPlayerProxy->addRemoteTextTrackProxy(*textTrackInfo.track);
        segmentInfo.textTracks.append({ MediaDescriptionInfo(*textTrackInfo.description), identifier });

        ASSERT(!m_trackIds.contains(identifier));
        ASSERT(!m_mediaDescriptions.contains(identifier));
        m_trackIds.add(identifier, textTrackInfo.track->id());
        m_mediaDescriptions.add(identifier, *textTrackInfo.description);
    }

    if (!m_connectionToWebProcess) {
        completionHandler();
        return;
    }

    m_connectionToWebProcess->connection().sendWithAsyncReply(Messages::SourceBufferPrivateRemote::SourceBufferPrivateDidReceiveInitializationSegment(segmentInfo), WTFMove(completionHandler), m_identifier);
}

void RemoteSourceBufferProxy::sourceBufferPrivateStreamEndedWithDecodeError()
{
    if (!m_connectionToWebProcess)
        return;

    m_connectionToWebProcess->connection().send(Messages::SourceBufferPrivateRemote::SourceBufferPrivateStreamEndedWithDecodeError(), m_identifier);
}

void RemoteSourceBufferProxy::sourceBufferPrivateAppendError(bool decodeError)
{
    if (!m_connectionToWebProcess)
        return;

    m_connectionToWebProcess->connection().send(Messages::SourceBufferPrivateRemote::SourceBufferPrivateAppendError(decodeError), m_identifier);
}

void RemoteSourceBufferProxy::sourceBufferPrivateHighestPresentationTimestampChanged(const MediaTime& timestamp)
{
    if (!m_connectionToWebProcess)
        return;

    m_connectionToWebProcess->connection().send(Messages::SourceBufferPrivateRemote::SourceBufferPrivateHighestPresentationTimestampChanged(timestamp), m_identifier);
}

void RemoteSourceBufferProxy::sourceBufferPrivateDurationChanged(const MediaTime& duration)
{
    if (!m_connectionToWebProcess)
        return;

    m_connectionToWebProcess->connection().send(Messages::SourceBufferPrivateRemote::SourceBufferPrivateDurationChanged(duration), m_identifier);
}

void RemoteSourceBufferProxy::sourceBufferPrivateDidParseSample(double sampleDuration)
{
    if (!m_connectionToWebProcess)
        return;

    m_connectionToWebProcess->connection().send(Messages::SourceBufferPrivateRemote::SourceBufferPrivateDidParseSample(sampleDuration), m_identifier);
}

void RemoteSourceBufferProxy::sourceBufferPrivateDidDropSample()
{
    if (!m_connectionToWebProcess)
        return;

    m_connectionToWebProcess->connection().send(Messages::SourceBufferPrivateRemote::SourceBufferPrivateDidDropSample(), m_identifier);
}

void RemoteSourceBufferProxy::sourceBufferPrivateAppendComplete(SourceBufferPrivateClient::AppendResult appendResult)
{
    if (!m_connectionToWebProcess)
        return;

    auto buffered = m_sourceBufferPrivate->buffered()->ranges();
    m_connectionToWebProcess->connection().send(Messages::SourceBufferPrivateRemote::SourceBufferPrivateAppendComplete(appendResult, WTFMove(buffered), m_sourceBufferPrivate->totalTrackBufferSizeInBytes(), m_sourceBufferPrivate->timestampOffset()), m_identifier);
}

void RemoteSourceBufferProxy::sourceBufferPrivateDidReceiveRenderingError(int64_t errorCode)
{
    if (!m_connectionToWebProcess)
        return;

    m_connectionToWebProcess->connection().send(Messages::SourceBufferPrivateRemote::SourceBufferPrivateDidReceiveRenderingError(errorCode), m_identifier);
}

void RemoteSourceBufferProxy::sourceBufferPrivateReportExtraMemoryCost(uint64_t extraMemory)
{
    if (!m_connectionToWebProcess)
        return;

    m_connectionToWebProcess->connection().send(Messages::SourceBufferPrivateRemote::SourceBufferPrivateReportExtraMemoryCost(extraMemory), m_identifier);
}

void RemoteSourceBufferProxy::sourceBufferPrivateBufferedDirtyChanged(bool flag)
{
    if (!m_connectionToWebProcess)
        return;

    m_connectionToWebProcess->connection().send(Messages::SourceBufferPrivateRemote::SourceBufferPrivateBufferedDirtyChanged(flag), m_identifier);
}

void RemoteSourceBufferProxy::append(IPC::SharedBufferReference&& buffer, CompletionHandler<void(SharedMemory::IPCHandle&&)>&& completionHandler)
{
    SharedMemory::Handle handle;

    auto invokeCallbackAtScopeExit = makeScopeExit([&] {
        completionHandler(SharedMemory::IPCHandle { WTFMove(handle), buffer.size() });
    });

    auto sharedMemory = buffer.sharedCopy();
    if (!sharedMemory)
        return;
    m_sourceBufferPrivate->append(sharedMemory->createSharedBuffer(buffer.size()));

    sharedMemory->createHandle(handle, SharedMemory::Protection::ReadOnly);
}

void RemoteSourceBufferProxy::abort()
{
    m_sourceBufferPrivate->abort();
}

void RemoteSourceBufferProxy::resetParserState()
{
    m_sourceBufferPrivate->resetParserState();
}

void RemoteSourceBufferProxy::removedFromMediaSource()
{
    m_sourceBufferPrivate->removedFromMediaSource();
}

void RemoteSourceBufferProxy::setMediaSourceEnded(bool isEnded)
{
    m_sourceBufferPrivate->setMediaSourceEnded(isEnded);
}

void RemoteSourceBufferProxy::setActive(bool active)
{
    m_sourceBufferPrivate->setActive(active);
}

void RemoteSourceBufferProxy::canSwitchToType(const ContentType& contentType, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(m_sourceBufferPrivate->canSwitchToType(contentType));
}

void RemoteSourceBufferProxy::setMode(WebCore::SourceBufferAppendMode appendMode)
{
    m_sourceBufferPrivate->setMode(appendMode);
}

void RemoteSourceBufferProxy::setReadyState(WebCore::MediaPlayer::ReadyState state)
{
    m_sourceBufferPrivate->setReadyState(state);
}

void RemoteSourceBufferProxy::startChangingType()
{
    m_sourceBufferPrivate->startChangingType();
}

void RemoteSourceBufferProxy::updateBufferedFromTrackBuffers(bool sourceIsEnded, CompletionHandler<void(WebCore::PlatformTimeRanges&&)>&& completionHandler)
{
    m_sourceBufferPrivate->updateBufferedFromTrackBuffers(sourceIsEnded);
    auto buffered = m_sourceBufferPrivate->buffered()->ranges();
    completionHandler(WTFMove(buffered));
}

void RemoteSourceBufferProxy::removeCodedFrames(const MediaTime& start, const MediaTime& end, const MediaTime& currentTime, bool isEnded, RemoveCodedFramesAsyncReply&& completionHandler)
{
    m_sourceBufferPrivate->removeCodedFrames(start, end, currentTime, isEnded, [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        auto buffered = m_sourceBufferPrivate->buffered()->ranges();
        completionHandler(WTFMove(buffered), m_sourceBufferPrivate->totalTrackBufferSizeInBytes());
    });
}

void RemoteSourceBufferProxy::evictCodedFrames(uint64_t newDataSize, uint64_t maximumBufferSize, const MediaTime& currentTime, const MediaTime& duration, bool isEnded, EvictCodedFramesDelayedReply&& completionHandler)
{
    m_sourceBufferPrivate->evictCodedFrames(newDataSize, maximumBufferSize, currentTime, duration, isEnded);
    completionHandler(m_sourceBufferPrivate->totalTrackBufferSizeInBytes());
}

void RemoteSourceBufferProxy::addTrackBuffer(TrackPrivateRemoteIdentifier trackPrivateRemoteIdentifier)
{
    ASSERT(m_trackIds.contains(trackPrivateRemoteIdentifier));
    ASSERT(m_mediaDescriptions.contains(trackPrivateRemoteIdentifier));
    m_sourceBufferPrivate->addTrackBuffer(m_trackIds.get(trackPrivateRemoteIdentifier), m_mediaDescriptions.get(trackPrivateRemoteIdentifier));
}

void RemoteSourceBufferProxy::resetTrackBuffers()
{
    m_sourceBufferPrivate->resetTrackBuffers();
}

void RemoteSourceBufferProxy::clearTrackBuffers()
{
    m_sourceBufferPrivate->clearTrackBuffers();
}

void RemoteSourceBufferProxy::setAllTrackBuffersNeedRandomAccess()
{
    m_sourceBufferPrivate->setAllTrackBuffersNeedRandomAccess();
}

void RemoteSourceBufferProxy::reenqueueMediaIfNeeded(const MediaTime& currentMediaTime)
{
    m_sourceBufferPrivate->reenqueueMediaIfNeeded(currentMediaTime);
}

void RemoteSourceBufferProxy::setGroupStartTimestamp(const MediaTime& timestamp)
{
    m_sourceBufferPrivate->setGroupStartTimestamp(timestamp);
}

void RemoteSourceBufferProxy::setGroupStartTimestampToEndTimestamp()
{
    m_sourceBufferPrivate->setGroupStartTimestampToEndTimestamp();
}

void RemoteSourceBufferProxy::setShouldGenerateTimestamps(bool shouldGenerateTimestamps)
{
    m_sourceBufferPrivate->setShouldGenerateTimestamps(shouldGenerateTimestamps);
}

void RemoteSourceBufferProxy::resetTimestampOffsetInTrackBuffers()
{
    m_sourceBufferPrivate->resetTimestampOffsetInTrackBuffers();
}

void RemoteSourceBufferProxy::setTimestampOffset(const MediaTime& timestampOffset)
{
    m_sourceBufferPrivate->setTimestampOffset(timestampOffset);
}

void RemoteSourceBufferProxy::setAppendWindowStart(const MediaTime& appendWindowStart)
{
    m_sourceBufferPrivate->setAppendWindowStart(appendWindowStart);
}

void RemoteSourceBufferProxy::setAppendWindowEnd(const MediaTime& appendWindowEnd)
{
    m_sourceBufferPrivate->setAppendWindowEnd(appendWindowEnd);
}

void RemoteSourceBufferProxy::seekToTime(const MediaTime& mediaTime)
{
    m_sourceBufferPrivate->seekToTime(mediaTime);
}

void RemoteSourceBufferProxy::updateTrackIds(Vector<std::pair<TrackPrivateRemoteIdentifier, TrackPrivateRemoteIdentifier>>&& identifierPairs)
{
    Vector<std::pair<AtomString, AtomString>> trackIdPairs;

    for (auto& identifierPair : identifierPairs) {
        ASSERT(m_trackIds.contains(identifierPair.first));
        ASSERT(m_trackIds.contains(identifierPair.second));

        auto oldId = m_trackIds.take(identifierPair.first);
        auto newId = m_trackIds.get(identifierPair.second);
        trackIdPairs.append(std::make_pair(oldId, newId));
    }

    if (!trackIdPairs.isEmpty())
        m_sourceBufferPrivate->updateTrackIds(WTFMove(trackIdPairs));
}

void RemoteSourceBufferProxy::bufferedSamplesForTrackId(TrackPrivateRemoteIdentifier trackPrivateRemoteIdentifier, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    ASSERT(m_trackIds.contains(trackPrivateRemoteIdentifier));
    ASSERT(m_mediaDescriptions.contains(trackPrivateRemoteIdentifier));
    m_sourceBufferPrivate->bufferedSamplesForTrackId(m_trackIds.get(trackPrivateRemoteIdentifier), WTFMove(completionHandler));
}

void RemoteSourceBufferProxy::enqueuedSamplesForTrackID(TrackPrivateRemoteIdentifier trackPrivateRemoteIdentifier, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    ASSERT(m_trackIds.contains(trackPrivateRemoteIdentifier));
    ASSERT(m_mediaDescriptions.contains(trackPrivateRemoteIdentifier));
    m_sourceBufferPrivate->bufferedSamplesForTrackId(m_trackIds.get(trackPrivateRemoteIdentifier), WTFMove(completionHandler));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
