/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM)
#include "MediaEndpointPeerConnection.h"

#include "DOMError.h"
#include "JSDOMError.h"
#include "JSRTCSessionDescription.h"
#include "MediaEndpointSessionConfiguration.h"
#include "MediaStreamTrack.h"
#include "RTCOfferAnswerOptions.h"
#include "RTCRtpSender.h"
#include "SDPProcessor.h"
#include <wtf/MainThread.h>
#include <wtf/text/Base64.h>

namespace WebCore {

using namespace PeerConnection;

static std::unique_ptr<PeerConnectionBackend> createMediaEndpointPeerConnection(PeerConnectionBackendClient* client)
{
    return std::unique_ptr<PeerConnectionBackend>(new MediaEndpointPeerConnection(client));
}

CreatePeerConnectionBackend PeerConnectionBackend::create = createMediaEndpointPeerConnection;

class WrappedSessionDescriptionPromise : public RefCounted<WrappedSessionDescriptionPromise> {
public:
    static Ref<WrappedSessionDescriptionPromise> create(SessionDescriptionPromise&& promise)
    {
        return *adoptRef(new WrappedSessionDescriptionPromise(WTFMove(promise)));
    }

    SessionDescriptionPromise& promise() { return m_promise; }

private:
    WrappedSessionDescriptionPromise(SessionDescriptionPromise&& promise)
        : m_promise(WTFMove(promise))
    { }

    SessionDescriptionPromise m_promise;
};

static String randomString(size_t length)
{
    const size_t size = ceil(length * 3 / 4);
    unsigned char randomValues[size];
    cryptographicallyRandomValues(randomValues, size);
    return base64Encode(randomValues, size);
}

MediaEndpointPeerConnection::MediaEndpointPeerConnection(PeerConnectionBackendClient* client)
    : m_client(client)
    , m_sdpProcessor(std::unique_ptr<SDPProcessor>(new SDPProcessor(m_client->scriptExecutionContext())))
    , m_cname(randomString(16))
    , m_iceUfrag(randomString(4))
    , m_icePassword(randomString(22))
{
    m_mediaEndpoint = MediaEndpoint::create(*this);
    ASSERT(m_mediaEndpoint);

    m_defaultAudioPayloads = m_mediaEndpoint->getDefaultAudioPayloads();
    m_defaultVideoPayloads = m_mediaEndpoint->getDefaultVideoPayloads();

    // Tasks (see runTask()) will be deferred until we get the DTLS fingerprint.
    m_mediaEndpoint->generateDtlsInfo();
}

void MediaEndpointPeerConnection::runTask(std::function<void()> task)
{
    if (m_dtlsFingerprint.isNull()) {
        // Only one task needs to be deferred since it will hold off any others until completed.
        ASSERT(!m_initialDeferredTask);
        m_initialDeferredTask = task;
    } else
        callOnMainThread(task);
}

void MediaEndpointPeerConnection::startRunningTasks()
{
    if (!m_initialDeferredTask)
        return;

    m_initialDeferredTask();
    m_initialDeferredTask = nullptr;
}

void MediaEndpointPeerConnection::createOffer(RTCOfferOptions& options, SessionDescriptionPromise&& promise)
{
    const RefPtr<RTCOfferOptions> protectedOptions = &options;
    RefPtr<WrappedSessionDescriptionPromise> wrappedPromise = WrappedSessionDescriptionPromise::create(WTFMove(promise));

    runTask([this, protectedOptions, wrappedPromise]() {
        createOfferTask(*protectedOptions, wrappedPromise->promise());
    });
}

void MediaEndpointPeerConnection::createOfferTask(RTCOfferOptions&, SessionDescriptionPromise& promise)
{
    ASSERT(!m_dtlsFingerprint.isEmpty());

    RefPtr<MediaEndpointSessionConfiguration> configurationSnapshot = MediaEndpointSessionConfiguration::create();

    configurationSnapshot->setSessionVersion(m_sdpSessionVersion++);

    RtpSenderVector senders = m_client->getSenders();

    // Add media descriptions for senders.
    for (auto& sender : senders) {
        RefPtr<PeerMediaDescription> mediaDescription = PeerMediaDescription::create();
        MediaStreamTrack* track = sender->track();

        mediaDescription->setMediaStreamId(sender->mediaStreamIds()[0]);
        mediaDescription->setMediaStreamTrackId(track->id());
        mediaDescription->setType(track->kind());
        mediaDescription->setPayloads(track->kind() == "audio" ? m_defaultAudioPayloads : m_defaultVideoPayloads);
        mediaDescription->setDtlsFingerprintHashFunction(m_dtlsFingerprintFunction);
        mediaDescription->setDtlsFingerprint(m_dtlsFingerprint);
        mediaDescription->setCname(m_cname);
        mediaDescription->addSsrc(cryptographicallyRandomNumber());
        mediaDescription->setIceUfrag(m_iceUfrag);
        mediaDescription->setIcePassword(m_icePassword);

        configurationSnapshot->addMediaDescription(WTFMove(mediaDescription));
    }

    String sdpString;
    SDPProcessor::Result result = m_sdpProcessor->generate(*configurationSnapshot, sdpString);
    if (result != SDPProcessor::Result::Success) {
        LOG_ERROR("SDPProcessor internal error");
        return;
    }

    promise.resolve(RTCSessionDescription::create("offer", sdpString));
}

void MediaEndpointPeerConnection::createAnswer(RTCAnswerOptions& options, SessionDescriptionPromise&& promise)
{
    UNUSED_PARAM(options);

    notImplemented();

    promise.reject(DOMError::create("NotSupportedError"));
}

void MediaEndpointPeerConnection::setLocalDescription(RTCSessionDescription& description, VoidPromise&& promise)
{
    UNUSED_PARAM(description);

    notImplemented();

    promise.reject(DOMError::create("NotSupportedError"));
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::localDescription() const
{
    notImplemented();

    return nullptr;
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::currentLocalDescription() const
{
    notImplemented();

    return nullptr;
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::pendingLocalDescription() const
{
    notImplemented();

    return nullptr;
}

void MediaEndpointPeerConnection::setRemoteDescription(RTCSessionDescription& description, VoidPromise&& promise)
{
    UNUSED_PARAM(description);

    notImplemented();

    promise.reject(DOMError::create("NotSupportedError"));
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::remoteDescription() const
{
    notImplemented();

    return nullptr;
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::currentRemoteDescription() const
{
    notImplemented();

    return nullptr;
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::pendingRemoteDescription() const
{
    notImplemented();

    return nullptr;
}

void MediaEndpointPeerConnection::setConfiguration(RTCConfiguration& configuration)
{
    UNUSED_PARAM(configuration);

    notImplemented();
}

void MediaEndpointPeerConnection::addIceCandidate(RTCIceCandidate& rtcCandidate, PeerConnection::VoidPromise&& promise)
{
    UNUSED_PARAM(rtcCandidate);

    notImplemented();

    promise.reject(DOMError::create("NotSupportedError"));
}

void MediaEndpointPeerConnection::getStats(MediaStreamTrack*, PeerConnection::StatsPromise&& promise)
{
    notImplemented();

    promise.reject(DOMError::create("NotSupportedError"));
}

void MediaEndpointPeerConnection::replaceTrack(RTCRtpSender& sender, MediaStreamTrack& withTrack, PeerConnection::VoidPromise&& promise)
{
    UNUSED_PARAM(sender);
    UNUSED_PARAM(withTrack);
    UNUSED_PARAM(promise);

    notImplemented();

    promise.reject(DOMError::create("NotSupportedError"));
}

void MediaEndpointPeerConnection::stop()
{
    notImplemented();
}

void MediaEndpointPeerConnection::markAsNeedingNegotiation()
{
    notImplemented();
}

void MediaEndpointPeerConnection::gotDtlsFingerprint(const String& fingerprint, const String& fingerprintFunction)
{
    ASSERT(isMainThread());

    m_dtlsFingerprint = fingerprint;
    m_dtlsFingerprintFunction = fingerprintFunction;

    startRunningTasks();
}

void MediaEndpointPeerConnection::gotIceCandidate(unsigned mdescIndex, RefPtr<IceCandidate>&& candidate)
{
    ASSERT(isMainThread());

    UNUSED_PARAM(mdescIndex);
    UNUSED_PARAM(candidate);

    notImplemented();
}

void MediaEndpointPeerConnection::doneGatheringCandidates(unsigned mdescIndex)
{
    ASSERT(isMainThread());

    UNUSED_PARAM(mdescIndex);

    notImplemented();
}

void MediaEndpointPeerConnection::gotRemoteSource(unsigned mdescIndex, RefPtr<RealtimeMediaSource>&& source)
{
    ASSERT(isMainThread());

    UNUSED_PARAM(mdescIndex);
    UNUSED_PARAM(source);

    notImplemented();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
