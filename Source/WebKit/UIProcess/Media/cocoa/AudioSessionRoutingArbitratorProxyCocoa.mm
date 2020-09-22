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

#import "config.h"
#import "AudioSessionRoutingArbitratorProxy.h"

#if ENABLE(ROUTING_ARBITRATION) && HAVE(AVAUDIO_ROUTING_ARBITER)

#import "AudioSessionRoutingArbitratorProxyMessages.h"
#import "Logging.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <WebCore/AudioSession.h>
#import <wtf/NeverDestroyed.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebKit {

using namespace WebCore;

class SharedArbitrator {
public:
    static SharedArbitrator& sharedInstance();

    using RoutingArbitrationError = AudioSessionRoutingArbitrationClient::RoutingArbitrationError;
    using DefaultRouteChanged = AudioSessionRoutingArbitrationClient::DefaultRouteChanged;
    using ArbitrationCallback = AudioSessionRoutingArbitratorProxy::ArbitrationCallback;

    bool isInRoutingArbitrationForArbitrator(AudioSessionRoutingArbitratorProxy&);
    void beginRoutingArbitrationForArbitrator(AudioSessionRoutingArbitratorProxy&, ArbitrationCallback&&);
    void endRoutingArbitrationForArbitrator(AudioSessionRoutingArbitratorProxy&);

private:
    Optional<AudioSession::CategoryType> m_currentCategory { AudioSession::None };
    WeakHashSet<AudioSessionRoutingArbitratorProxy> m_arbitrators;
    Vector<ArbitrationCallback> m_enqueuedCallbacks;
    bool m_setupArbitrationOngoing { false };
};

SharedArbitrator& SharedArbitrator::sharedInstance()
{
    static NeverDestroyed<SharedArbitrator> instance;
    return instance;
}

bool SharedArbitrator::isInRoutingArbitrationForArbitrator(AudioSessionRoutingArbitratorProxy& proxy)
{
    return m_arbitrators.contains(proxy);
}

void SharedArbitrator::beginRoutingArbitrationForArbitrator(AudioSessionRoutingArbitratorProxy& proxy, ArbitrationCallback&& callback)
{
    ASSERT(!isInRoutingArbitrationForArbitrator(proxy));

    if (m_setupArbitrationOngoing) {
        m_enqueuedCallbacks.append([this, weakProxy = makeWeakPtr(proxy), callback = WTFMove(callback)] (RoutingArbitrationError error, DefaultRouteChanged routeChanged) mutable {
            if (error == RoutingArbitrationError::None && weakProxy)
                m_arbitrators.add(*weakProxy);

            callback(error, routeChanged);
        });

        return;
    }

    auto requestedCategory = proxy.category();

    if (m_currentCategory) {
        if (*m_currentCategory >= requestedCategory) {
            m_arbitrators.add(proxy);
            callback(RoutingArbitrationError::None, DefaultRouteChanged::No);
            return;
        }

        [[PAL::getAVAudioRoutingArbiterClass() sharedRoutingArbiter] leaveArbitration];
    }

    m_currentCategory = requestedCategory;

    AVAudioRoutingArbitrationCategory arbitrationCategory = AVAudioRoutingArbitrationCategoryPlayback;
    switch (requestedCategory) {
    case AudioSession::MediaPlayback:
        arbitrationCategory = AVAudioRoutingArbitrationCategoryPlayback;
        break;
    case AudioSession::RecordAudio:
        arbitrationCategory = AVAudioRoutingArbitrationCategoryPlayAndRecord;
        break;
    case AudioSession::PlayAndRecord:
        arbitrationCategory = AVAudioRoutingArbitrationCategoryPlayAndRecordVoice;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    m_setupArbitrationOngoing = true;
    m_enqueuedCallbacks.append([this, weakProxy = makeWeakPtr(proxy), callback = WTFMove(callback)] (RoutingArbitrationError error, DefaultRouteChanged routeChanged) mutable {
        if (error == RoutingArbitrationError::None && weakProxy)
            m_arbitrators.add(*weakProxy);

        callback(error, routeChanged);
    });

    [[PAL::getAVAudioRoutingArbiterClass() sharedRoutingArbiter] beginArbitrationWithCategory:arbitrationCategory completionHandler:[this](BOOL defaultDeviceChanged, NSError * _Nullable error) {
        callOnMainRunLoop([this, defaultDeviceChanged, error = retainPtr(error)] {
            if (error)
                RELEASE_LOG_ERROR(Media, "SharedArbitrator::beginRoutingArbitrationForArbitrator: %s failed with error %s:%s", convertEnumerationToString(*m_currentCategory).ascii().data(), [[error domain] UTF8String], [[error localizedDescription] UTF8String]);

            // FIXME: Do we need to reset sample rate and buffer size for the new default device?
            if (defaultDeviceChanged)
                LOG(Media, "AudioSession::setCategory() - defaultDeviceChanged!");

            Vector<ArbitrationCallback> callbacks = WTFMove(m_enqueuedCallbacks);
            for (auto& callback : callbacks)
                callback(error ? RoutingArbitrationError::Failed : RoutingArbitrationError::None, defaultDeviceChanged ? DefaultRouteChanged::Yes : DefaultRouteChanged::No);

            m_setupArbitrationOngoing = false;
        });
    }];
}

void SharedArbitrator::endRoutingArbitrationForArbitrator(AudioSessionRoutingArbitratorProxy& proxy)
{
    ASSERT(isInRoutingArbitrationForArbitrator(proxy));
    m_arbitrators.remove(proxy);

    if (!m_arbitrators.computesEmpty())
        return;

    for (auto& callback : m_enqueuedCallbacks)
        callback(RoutingArbitrationError::Cancelled, DefaultRouteChanged::No);

    m_enqueuedCallbacks.clear();
    m_currentCategory.reset();
    [[PAL::getAVAudioRoutingArbiterClass() sharedRoutingArbiter] leaveArbitration];
}

AudioSessionRoutingArbitratorProxy::AudioSessionRoutingArbitratorProxy(WebProcessProxy& proxy)
    : m_process(proxy)
{
    m_process.addMessageReceiver(Messages::AudioSessionRoutingArbitratorProxy::messageReceiverName(), destinationId(), *this);
}

AudioSessionRoutingArbitratorProxy::~AudioSessionRoutingArbitratorProxy()
{
    m_process.removeMessageReceiver(Messages::AudioSessionRoutingArbitratorProxy::messageReceiverName(), destinationId());
}

void AudioSessionRoutingArbitratorProxy::processDidTerminate()
{
    if (SharedArbitrator::sharedInstance().isInRoutingArbitrationForArbitrator(*this))
        endRoutingArbitration();
}

void AudioSessionRoutingArbitratorProxy::beginRoutingArbitrationWithCategory(WebCore::AudioSession::CategoryType category, ArbitrationCallback&& callback)
{
    m_category = category;
    m_arbitrationStatus = ArbitrationStatus::Pending;
    SharedArbitrator::sharedInstance().beginRoutingArbitrationForArbitrator(*this, [weakThis = makeWeakPtr(*this), callback = WTFMove(callback)] (RoutingArbitrationError error, DefaultRouteChanged routeChanged) mutable {
        if (weakThis)
            weakThis->m_arbitrationStatus = error == RoutingArbitrationError::None ? ArbitrationStatus::Active : ArbitrationStatus::None;
        callback(error, routeChanged);
    });
}

void AudioSessionRoutingArbitratorProxy::endRoutingArbitration()
{
    SharedArbitrator::sharedInstance().endRoutingArbitrationForArbitrator(*this);
    m_arbitrationStatus = ArbitrationStatus::None;
}

}

#endif
