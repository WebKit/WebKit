/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "SharedRoutingArbitrator.h"

#if ENABLE(ROUTING_ARBITRATION) && HAVE(AVAUDIO_ROUTING_ARBITER)

#import <wtf/NeverDestroyed.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {

UniqueRef<SharedRoutingArbitrator::Token> SharedRoutingArbitrator::Token::create()
{
    return makeUniqueRef<Token>();
}

SharedRoutingArbitrator& SharedRoutingArbitrator::sharedInstance()
{
    static NeverDestroyed<SharedRoutingArbitrator> instance;
    return instance;
}

bool SharedRoutingArbitrator::isInRoutingArbitrationForToken(const Token& token)
{
    return m_tokens.contains(token);
}

void SharedRoutingArbitrator::beginRoutingArbitrationForToken(const Token& token, AudioSession::CategoryType requestedCategory, ArbitrationCallback&& callback)
{
    ASSERT(!isInRoutingArbitrationForToken(token));

    if (m_setupArbitrationOngoing) {
        m_enqueuedCallbacks.append([this, weakToken = WeakPtr { token }, callback = WTFMove(callback)] (RoutingArbitrationError error, DefaultRouteChanged routeChanged) mutable {
            if (error == RoutingArbitrationError::None && weakToken)
                m_tokens.add(*weakToken);

            callback(error, routeChanged);
        });

        return;
    }

    if (m_currentCategory) {
        if (*m_currentCategory >= requestedCategory) {
            m_tokens.add(token);
            callback(RoutingArbitrationError::None, DefaultRouteChanged::No);
            return;
        }

        [[PAL::getAVAudioRoutingArbiterClass() sharedRoutingArbiter] leaveArbitration];
    }

    m_currentCategory = requestedCategory;

    AVAudioRoutingArbitrationCategory arbitrationCategory = AVAudioRoutingArbitrationCategoryPlayback;
    switch (requestedCategory) {
    case AudioSession::CategoryType::MediaPlayback:
        arbitrationCategory = AVAudioRoutingArbitrationCategoryPlayback;
        break;
    case AudioSession::CategoryType::RecordAudio:
        arbitrationCategory = AVAudioRoutingArbitrationCategoryPlayAndRecord;
        break;
    case AudioSession::CategoryType::PlayAndRecord:
        arbitrationCategory = AVAudioRoutingArbitrationCategoryPlayAndRecordVoice;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    m_setupArbitrationOngoing = true;
    m_enqueuedCallbacks.append([this, weakToken = WeakPtr { token }, callback = WTFMove(callback)] (RoutingArbitrationError error, DefaultRouteChanged routeChanged) mutable {
        if (error == RoutingArbitrationError::None && weakToken)
            m_tokens.add(*weakToken);

        callback(error, routeChanged);
    });

    [[PAL::getAVAudioRoutingArbiterClass() sharedRoutingArbiter] beginArbitrationWithCategory:arbitrationCategory completionHandler:[this](BOOL defaultDeviceChanged, NSError * _Nullable error) {
        callOnMainRunLoop([this, defaultDeviceChanged, error = retainPtr(error)] {
            if (error)
                RELEASE_LOG_ERROR(Media, "SharedRoutingArbitrator::beginRoutingArbitrationForToken: %s failed with error %s:%s", convertEnumerationToString(*m_currentCategory).ascii().data(), [[error domain] UTF8String], [[error localizedDescription] UTF8String]);

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

void SharedRoutingArbitrator::endRoutingArbitrationForToken(const Token& token)
{
    m_tokens.remove(token);

    if (!m_tokens.computesEmpty())
        return;

    for (auto& callback : m_enqueuedCallbacks)
        callback(RoutingArbitrationError::Cancelled, DefaultRouteChanged::No);

    m_enqueuedCallbacks.clear();
    m_currentCategory.reset();
    [[PAL::getAVAudioRoutingArbiterClass() sharedRoutingArbiter] leaveArbitration];
}

}
#endif
