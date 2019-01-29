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

#include "NetworkProcessSupplement.h"
#include "NetworkProximityAssertion.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS IDSService;
OBJC_CLASS NSArray;
OBJC_CLASS NSURLRequest;
OBJC_CLASS WKProximityServiceDelegate;
OBJC_CLASS WRM_iRATInterface;

namespace WebCore {
class ResourceRequest;
}

namespace WebKit {

class NetworkProcess;
class NetworkDataTaskCocoa;
enum class NetworkProximityRecommendation : uint8_t;

enum class ResumptionReason : uint8_t {
    ProcessForegrounding,
    ProcessResuming,
};

enum class SuspensionReason : uint8_t {
    ProcessBackgrounding,
    ProcessSuspending
};

class NetworkProximityServiceClient {
public:
    virtual ~NetworkProximityServiceClient() = default;
    virtual void devicesChanged(NSArray *devices) = 0;
};

class NetworkProximityManager final : public NetworkProcessSupplement, private NetworkProximityServiceClient {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(NetworkProximityManager);
public:
    explicit NetworkProximityManager(NetworkProcess&);
    ~NetworkProximityManager();

    static const char* supplementName();

    void applyProperties(const WebCore::ResourceRequest&, NetworkDataTaskCocoa&, NSURLRequest *&);
    void resume(ResumptionReason);
    void suspend(SuspensionReason, CompletionHandler<void()>&&);

private:
    enum class State : uint8_t {
        Backgrounded,
        Foregrounded,
        Initialized,
        Resumed,
        Suspended,
    };

    bool isCompanionInProximity() const { return m_isCompanionInProximity; }
    NetworkProximityRecommendation recommendation() const;
    bool shouldUseDirectWiFiInProximity() const { return m_shouldUseDirectWiFiInProximity; }

    void processRecommendations(NSArray *recommendations);
    void resumeRecommendations();
    void suspendRecommendations(State oldState);
    void updateCompanionProximity();
    void updateRecommendation();

    // NetworkProcessSupplement
    void initialize(const NetworkProcessCreationParameters&) override;

    // NetworkProximityServiceClient
    void devicesChanged(NSArray *devices) override;

    unsigned m_contextIdentifier { 0 };
    bool m_isCompanionInProximity { false };
    bool m_shouldUseDirectWiFiInProximity { false };
    State m_state { State::Initialized };
    RetainPtr<WRM_iRATInterface> m_iRATInterface;
    RetainPtr<IDSService> m_idsService;
    RetainPtr<WKProximityServiceDelegate> m_idsServiceDelegate;
    OSObjectPtr<dispatch_queue_t> m_recommendationQueue;
    BluetoothProximityAssertion m_bluetoothProximityAssertion;
    WiFiProximityAssertion m_wiFiProximityAssertion;
};

} // namespace WebKit

#endif // ENABLE(PROXIMITY_NETWORKING)
