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

#pragma once

#if ENABLE(ROUTING_ARBITRATION) && HAVE(AVAUDIO_ROUTING_ARBITER)

#include "AudioSession.h"
#include <wtf/UniqueRef.h>

namespace WTF {
class Logger;
}

namespace WebCore {

class WEBCORE_EXPORT SharedRoutingArbitrator {
public:
    static SharedRoutingArbitrator& sharedInstance();

    using RoutingArbitrationError = AudioSessionRoutingArbitrationClient::RoutingArbitrationError;
    using DefaultRouteChanged = AudioSessionRoutingArbitrationClient::DefaultRouteChanged;
    using ArbitrationCallback = AudioSessionRoutingArbitrationClient::ArbitrationCallback;

    class WEBCORE_EXPORT Token : public CanMakeWeakPtr<Token> {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        static UniqueRef<Token> create();
        const void* logIdentifier() const;
    private:
        friend UniqueRef<Token> WTF::makeUniqueRefWithoutFastMallocCheck<Token>();
        Token() = default;
        mutable const void* m_logIdentifier;
    };

    bool isInRoutingArbitrationForToken(const Token&);
    void beginRoutingArbitrationForToken(const Token&, AudioSession::CategoryType, ArbitrationCallback&&);
    void endRoutingArbitrationForToken(const Token&);

    void setLogger(const Logger&);

private:
    const Logger& logger();
    const char* logClassName() const { return "SharedRoutingArbitrator"; }
    WTFLogChannel& logChannel() const;

    std::optional<AudioSession::CategoryType> m_currentCategory { AudioSession::CategoryType::None };
    WeakHashSet<Token> m_tokens;
    Vector<ArbitrationCallback> m_enqueuedCallbacks;
    RefPtr<const Logger> m_logger;
    bool m_setupArbitrationOngoing { false };
};

}

#endif
