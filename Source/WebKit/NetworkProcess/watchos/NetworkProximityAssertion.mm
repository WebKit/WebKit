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

#import "config.h"
#import "NetworkProximityAssertion.h"

#if ENABLE(PROXIMITY_NETWORKING)

#import "Logging.h"
#import "NetworkProcess.h"
#import "NetworkProximityManager.h"
#import <IDS/IDS_Private.h>
#import <wtf/BlockPtr.h>

namespace WebKit {

static auto assertionReleaseDelayAfterBackgrounding = 10_s;
static auto assertionReleaseDelayForHysteresis = 10_s;

NetworkProximityAssertion::NetworkProximityAssertion()
    : m_releaseTimer { *this, &NetworkProximityAssertion::releaseTimerFired, assertionReleaseDelayForHysteresis }
    , m_suspendAfterBackgroundingTimer { *this, &NetworkProximityAssertion::suspendAfterBackgroundingTimerFired, assertionReleaseDelayAfterBackgrounding }
{
}

void NetworkProximityAssertion::hold()
{
    ++m_assertionCount;

    if (m_state == State::Suspended) {
        ASSERT(!m_isHoldingAssertion);
        ASSERT(!m_releaseTimer.isActive());
        ASSERT(!m_suspendAfterBackgroundingTimer.isActive());
        return;
    }

    if (m_releaseTimer.isActive()) {
        ASSERT(m_isHoldingAssertion);
        m_releaseTimer.stop();
        return;
    }

    if (m_assertionCount == 1) {
        ASSERT(!m_isHoldingAssertion);
        holdNow();
    }

    ASSERT(m_isHoldingAssertion);
}

void NetworkProximityAssertion::release()
{
    ASSERT(m_assertionCount);
    if (!m_assertionCount)
        return;

    --m_assertionCount;

    if (m_state == State::Suspended) {
        ASSERT(!m_isHoldingAssertion);
        ASSERT(!m_releaseTimer.isActive());
        ASSERT(!m_suspendAfterBackgroundingTimer.isActive());
        return;
    }

    ASSERT(m_isHoldingAssertion);
    ASSERT(!m_releaseTimer.isActive());
    if (!m_assertionCount)
        m_releaseTimer.restart();
}

void NetworkProximityAssertion::resume(ResumptionReason)
{
    switch (m_state) {
    case State::Backgrounded:
        m_suspendAfterBackgroundingTimer.stop();
        break;
    case State::Resumed:
        ASSERT(!m_suspendAfterBackgroundingTimer.isActive());
        break;
    case State::Suspended:
        ASSERT(!m_isHoldingAssertion);
        ASSERT(!m_releaseTimer.isActive());
        ASSERT(!m_suspendAfterBackgroundingTimer.isActive());
        holdNow();
        if (!m_assertionCount)
            m_releaseTimer.restart();
        break;
    }

    m_state = State::Resumed;
}

void NetworkProximityAssertion::suspend(SuspensionReason reason, CompletionHandler<void()>&& completionHandler)
{
    switch (reason) {
    case SuspensionReason::ProcessBackgrounding:
        // The Networking process is being backgrounded. We should drop our assertions
        // soon, in case the system suspends the process before we receive ProcessSuspending.
        m_state = State::Backgrounded;
        m_suspendAfterBackgroundingTimer.restart();
        break;
    case SuspensionReason::ProcessSuspending:
        // The Networking process is being suspended, so we need to drop our assertions
        // immediately before that happens.
        suspendNow();
        break;
    }

    completionHandler();
}

void NetworkProximityAssertion::suspendNow()
{
    if (m_state == State::Suspended)
        return;

    m_state = State::Suspended;
    m_releaseTimer.stop();
    m_suspendAfterBackgroundingTimer.stop();
    releaseNow();
}

void NetworkProximityAssertion::releaseTimerFired()
{
    ASSERT(m_isHoldingAssertion);
    ASSERT(m_state != State::Suspended);
    ASSERT(!m_assertionCount);
    ASSERT(!m_releaseTimer.isActive());
    releaseNow();
}

void NetworkProximityAssertion::suspendAfterBackgroundingTimerFired()
{
    ASSERT(!m_suspendAfterBackgroundingTimer.isActive());
    ASSERT(m_state != State::Suspended);
    suspendNow();
}

BluetoothProximityAssertion::BluetoothProximityAssertion(IDSService *idsService)
    : m_idsService { idsService }
{
}

void BluetoothProximityAssertion::suspend(SuspensionReason reason, CompletionHandler<void()>&& completionHandler)
{
    auto wasHoldingAssertion = m_isHoldingAssertion;
    NetworkProximityAssertion::suspend(reason, [] { });

    switch (reason) {
    case SuspensionReason::ProcessBackgrounding:
        completionHandler();
        break;
    case SuspensionReason::ProcessSuspending:
        ASSERT(!m_isHoldingAssertion);
        if (!wasHoldingAssertion) {
            completionHandler();
            break;
        }

        // IDS processes -setLinkPreferences: on a background dispatch queue and provides no
        // completion handler. If we suspend before IDS finishes setting link preferences, the
        // Bluetooth radio might stay in a high power mode, harming battery life. Delay suspension
        // by 30 seconds to ensure -setLinkPreferences: always finishes its work.
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 30 * NSEC_PER_SEC), dispatch_get_main_queue(), makeBlockPtr(WTFMove(completionHandler)).get());
        break;
    }
}

void BluetoothProximityAssertion::holdNow()
{
    if (m_isHoldingAssertion)
        return;

    RELEASE_LOG(ProximityNetworking, "Holding Bluetooth Classic assertion.");
    [m_idsService setLinkPreferences:@{ IDSLinkPreferenceOptionPacketsPerSecondKey : @(200) }];
    m_isHoldingAssertion = true;
}

void BluetoothProximityAssertion::releaseNow()
{
    if (!m_isHoldingAssertion)
        return;

    RELEASE_LOG(ProximityNetworking, "Releasing Bluetooth Classic assertion.");
    [m_idsService setLinkPreferences:@{ IDSLinkPreferenceOptionPacketsPerSecondKey : @(0) }];
    m_isHoldingAssertion = false;
}

WiFiProximityAssertion::WiFiProximityAssertion()
    : m_wiFiManagerClient { adoptCF(WiFiManagerClientCreate(kCFAllocatorDefault, kWiFiClientTypeNormal)) }
{
}

void WiFiProximityAssertion::holdNow()
{
    if (m_isHoldingAssertion)
        return;

    RELEASE_LOG(ProximityNetworking, "Holding Wi-Fi assertion.");
    ASSERT(WiFiManagerClientGetType(m_wiFiManagerClient.get()) == kWiFiClientTypeNormal);
    WiFiManagerClientSetType(m_wiFiManagerClient.get(), kWiFiClientTypeDirectToCloud);
    m_isHoldingAssertion = true;
}

void WiFiProximityAssertion::releaseNow()
{
    if (!m_isHoldingAssertion)
        return;

    RELEASE_LOG(ProximityNetworking, "Releasing Wi-Fi assertion.");
    ASSERT(WiFiManagerClientGetType(m_wiFiManagerClient.get()) == kWiFiClientTypeDirectToCloud);
    WiFiManagerClientSetType(m_wiFiManagerClient.get(), kWiFiClientTypeNormal);
    m_isHoldingAssertion = false;
}

} // namespace WebKit

#endif // ENABLE(PROXIMITY_NETWORKING)
