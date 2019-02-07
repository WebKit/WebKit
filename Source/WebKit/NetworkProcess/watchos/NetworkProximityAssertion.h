/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(PROXIMITY_NETWORKING)

#include "MobileWiFiSPI.h"
#include <WebCore/Timer.h>
#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS IDSService;

namespace WebKit {

enum class ResumptionReason : uint8_t;
enum class SuspensionReason : uint8_t;

class NetworkProximityAssertion {
    WTF_MAKE_NONCOPYABLE(NetworkProximityAssertion);
public:
    class Token {
        WTF_MAKE_NONCOPYABLE(Token);
    public:
        explicit Token(NetworkProximityAssertion& assertion)
            : m_assertion { assertion }
        {
            m_assertion.hold();
        }

        ~Token()
        {
            m_assertion.release();
        }

    private:
        NetworkProximityAssertion& m_assertion;
    };

    void resume(ResumptionReason);
    void suspend(SuspensionReason, CompletionHandler<void()>&&);

protected:
    NetworkProximityAssertion();
    virtual ~NetworkProximityAssertion() = default;

    bool m_isHoldingAssertion { false };

private:
    enum class State {
        Backgrounded, // Can hold assertions until m_suspendAfterBackgroundingTimer fires.
        Resumed, // Can hold assertions at any time.
        Suspended, // Can not hold assertions.
    };

    virtual void holdNow() = 0;
    virtual void releaseNow() = 0;

    void hold();
    void release();
    void releaseTimerFired();
    void suspendAfterBackgroundingTimerFired();
    void suspendNow();

    uint64_t m_assertionCount { 0 };
    State m_state { State::Suspended };
    WebCore::ResettableOneShotTimer m_releaseTimer;
    WebCore::ResettableOneShotTimer m_suspendAfterBackgroundingTimer;
};

class BluetoothProximityAssertion final : public NetworkProximityAssertion {
public:
    explicit BluetoothProximityAssertion(IDSService *);

    void suspend(SuspensionReason, CompletionHandler<void()>&&);

private:
    void holdNow() override;
    void releaseNow() override;

    RetainPtr<IDSService> m_idsService;
};

class WiFiProximityAssertion final : public NetworkProximityAssertion {
public:
    WiFiProximityAssertion();

private:
    void holdNow() override;
    void releaseNow() override;

    RetainPtr<WiFiManagerClientRef> m_wiFiManagerClient;
};

} // namespace WebKit

#endif // ENABLE(PROXIMITY_NETWORKING)
