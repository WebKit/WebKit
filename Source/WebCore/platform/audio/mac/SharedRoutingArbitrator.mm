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

#import "Logging.h"
#import <wtf/LoggerHelper.h>
#import <wtf/NeverDestroyed.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {

#define TOKEN_LOGIDENTIFIER(token) WTF::Logger::LogSiteIdentifier(logClassName(), __func__, token.logIdentifier())

UniqueRef<SharedRoutingArbitrator::Token> SharedRoutingArbitrator::Token::create()
{
    return makeUniqueRef<Token>();
}

const void* SharedRoutingArbitrator::Token::logIdentifier() const
{
    if (!m_logIdentifier)
        m_logIdentifier = LoggerHelper::uniqueLogIdentifier();

    return m_logIdentifier;
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

    auto identifier = TOKEN_LOGIDENTIFIER(token);
    ALWAYS_LOG_IF(m_logger, identifier, requestedCategory);

    if (m_setupArbitrationOngoing) {
        ALWAYS_LOG_IF(m_logger, identifier, "enqueing callback, arbitration ongoing");
        m_enqueuedCallbacks.append([this, weakToken = WeakPtr { token }, callback = WTFMove(callback), identifier = WTFMove(identifier)] (RoutingArbitrationError error, DefaultRouteChanged routeChanged) mutable {
            if (error == RoutingArbitrationError::None && weakToken)
                m_tokens.add(*weakToken);

            ALWAYS_LOG_IF(m_logger, identifier, "pending arbitration finished, error = ", error, ", routeChanged = ", routeChanged);
            callback(error, routeChanged);
        });

        return;
    }

    if (m_currentCategory) {
        if (*m_currentCategory >= requestedCategory) {
            m_tokens.add(token);
            ALWAYS_LOG_IF(m_logger, identifier, "ignoring, nothing to change");
            callback(RoutingArbitrationError::None, DefaultRouteChanged::No);
            return;
        }

        ALWAYS_LOG_IF(m_logger, identifier, "leaving current arbitration");
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

    [[PAL::getAVAudioRoutingArbiterClass() sharedRoutingArbiter] beginArbitrationWithCategory:arbitrationCategory completionHandler:[this, identifier = WTFMove(identifier)](BOOL defaultDeviceChanged, NSError * _Nullable error) mutable {
        callOnMainRunLoop([this, defaultDeviceChanged, error = retainPtr(error), identifier = WTFMove(identifier)] {
            if (error)
                ERROR_LOG(identifier, error.get(), ", routeChanged = ", !!defaultDeviceChanged);

            // FIXME: Do we need to reset sample rate and buffer size if the default device changes?

            ALWAYS_LOG_IF(m_logger, identifier, "arbitration completed, category = ", m_currentCategory ? *m_currentCategory : AudioSession::CategoryType::None, ", default device changed = ", !!defaultDeviceChanged);

            Vector<ArbitrationCallback> callbacks = WTFMove(m_enqueuedCallbacks);
            for (auto& callback : callbacks)
                callback(error ? RoutingArbitrationError::Failed : RoutingArbitrationError::None, defaultDeviceChanged ? DefaultRouteChanged::Yes : DefaultRouteChanged::No);

            m_setupArbitrationOngoing = false;
        });
    }];
}

void SharedRoutingArbitrator::endRoutingArbitrationForToken(const Token& token)
{
    ALWAYS_LOG_IF(m_logger, TOKEN_LOGIDENTIFIER(token));

    m_tokens.remove(token);

    if (!m_tokens.isEmptyIgnoringNullReferences())
        return;

    for (auto& callback : m_enqueuedCallbacks)
        callback(RoutingArbitrationError::Cancelled, DefaultRouteChanged::No);

    m_enqueuedCallbacks.clear();
    m_currentCategory.reset();
    [[PAL::getAVAudioRoutingArbiterClass() sharedRoutingArbiter] leaveArbitration];
}

void SharedRoutingArbitrator::setLogger(const Logger& logger)
{
    if (!m_logger)
        m_logger = &logger;
}

const Logger& SharedRoutingArbitrator::logger()
{
    ASSERT(m_logger);
    return *m_logger.get();
}

WTFLogChannel& SharedRoutingArbitrator::logChannel() const
{
    return LogMedia;
}

}
#endif
