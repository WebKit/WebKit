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
#import "NetworkProximityManager.h"

#if ENABLE(PROXIMITY_NETWORKING)

#import "Logging.h"
#import "NetworkDataTaskCocoa.h"
#import "NetworkProcess.h"
#import "NetworkProcessCreationParameters.h"
#import <IDS/IDS.h>
#import <WirelessCoexManager/WRM_iRATInterface.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/Box.h>
#import <wtf/CompletionHandler.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/threads/BinarySemaphore.h>

WTF_DECLARE_CF_TYPE_TRAIT(SCNetworkInterface);

@interface WKProximityServiceDelegate : NSObject <IDSServiceDelegate>
- (void)setClient:(WebKit::NetworkProximityServiceClient*)client;
@end

@implementation WKProximityServiceDelegate {
    WebKit::NetworkProximityServiceClient* _client;
}

- (void)setClient:(WebKit::NetworkProximityServiceClient*)client
{
    _client = client;
}

- (void)service:(IDSService *)service devicesChanged:(NSArray *)devices
{
    if (_client)
        _client->devicesChanged(devices);
}

- (void)service:(IDSService *)service nearbyDevicesChanged:(NSArray *)devices
{
    if (_client)
        _client->devicesChanged(devices);
}

@end

namespace WebKit {

enum class NetworkProximityRecommendation : uint8_t {
    Bluetooth,
    None,
    WiFi,
};

NetworkProximityManager::NetworkProximityManager(NetworkProcess&)
    : m_idsService { adoptNS([[IDSService alloc] initWithService:@"com.apple.private.alloy.webkitnetworking"]) }
    , m_idsServiceDelegate { adoptNS([[WKProximityServiceDelegate alloc] init]) }
    , m_recommendationQueue { adoptOSObject(dispatch_queue_create("WebKit Network Proximity Recommendation Queue", DISPATCH_QUEUE_SERIAL)) }
    , m_bluetoothProximityAssertion { m_idsService.get() }
{
    [m_idsServiceDelegate setClient:this];
    [m_idsService addDelegate:m_idsServiceDelegate.get() queue:dispatch_get_main_queue()];
}

NetworkProximityManager::~NetworkProximityManager()
{
    [m_idsServiceDelegate setClient:nullptr];
}

const char* NetworkProximityManager::supplementName()
{
    return "NetworkProximityManager";
}

static void bindRequestToWiFi(NSURLRequest *& request)
{
    static NSString *wiFiInterfaceIdentifier = []() -> NSString * {
        auto interfaces = adoptCF(SCNetworkInterfaceCopyAll());
        for (CFIndex i = 0, count = CFArrayGetCount(interfaces.get()); i < count; ++i) {
            auto interface = checked_cf_cast<SCNetworkInterfaceRef>(CFArrayGetValueAtIndex(interfaces.get(), i));
            if (!CFEqual(SCNetworkInterfaceGetInterfaceType(interface), kSCNetworkInterfaceTypeIEEE80211))
                continue;

            auto identifier = retainPtr(SCNetworkInterfaceGetBSDName(interface));
            RELEASE_LOG(ProximityNetworking, "Determined Wi-Fi interface identifier is %@.", identifier.get());
            return (NSString *)identifier.leakRef();
        }

        RELEASE_LOG(ProximityNetworking, "Failed to determine Wi-Fi interface identifier.");
        return nil;
    }();

    if (!wiFiInterfaceIdentifier)
        return;

    auto mutableRequest = adoptNS([request mutableCopy]);
    [mutableRequest setBoundInterfaceIdentifier:wiFiInterfaceIdentifier];
    request = mutableRequest.autorelease();
}

void NetworkProximityManager::applyProperties(const WebCore::ResourceRequest& request, WebKit::NetworkDataTaskCocoa& dataTask, NSURLRequest *& nsRequest)
{
    // If the client has already bound the request to an identifier, don't override it.
    if (nsRequest.boundInterfaceIdentifier.length)
        return;

    if (dataTask.isTopLevelNavigation())
        updateRecommendation();

    switch (recommendation()) {
    case NetworkProximityRecommendation::Bluetooth:
        dataTask.holdProximityAssertion(m_bluetoothProximityAssertion);
        break;
    case NetworkProximityRecommendation::None:
        break;
    case NetworkProximityRecommendation::WiFi:
        dataTask.holdProximityAssertion(m_wiFiProximityAssertion);
        bindRequestToWiFi(nsRequest);
        break;
    }
}

void NetworkProximityManager::resume(ResumptionReason reason)
{
    switch (reason) {
    case ResumptionReason::ProcessForegrounding:
        m_state = State::Foregrounded;
        break;
    case ResumptionReason::ProcessResuming:
        m_state = State::Resumed;
        break;
    }

    resumeRecommendations();

    switch (recommendation()) {
    case NetworkProximityRecommendation::Bluetooth:
        m_bluetoothProximityAssertion.resume(reason);
        break;
    case NetworkProximityRecommendation::None:
        break;
    case NetworkProximityRecommendation::WiFi:
        m_wiFiProximityAssertion.resume(reason);
        break;
    }
}

void NetworkProximityManager::suspend(SuspensionReason reason, CompletionHandler<void()>&& completionHandler)
{
    auto oldState = m_state;
    switch (reason) {
    case SuspensionReason::ProcessBackgrounding:
        m_state = State::Backgrounded;
        break;
    case SuspensionReason::ProcessSuspending:
        m_state = State::Suspended;
        break;
    }

    suspendRecommendations(oldState);

    auto wrappedCompletionHandler = [this, completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler();
        RELEASE_LOG(ProximityNetworking, "Finished suspending NetworkProximityManager (state: %hhu).", m_state);
    };

    RELEASE_LOG(ProximityNetworking, "Suspending NetworkProximityManager (state: %hhu).", m_state);
    m_bluetoothProximityAssertion.suspend(reason, WTFMove(wrappedCompletionHandler));
    m_wiFiProximityAssertion.suspend(reason, [] { });
}

NetworkProximityRecommendation NetworkProximityManager::recommendation() const
{
    if (!isCompanionInProximity())
        return NetworkProximityRecommendation::None;

    if (shouldUseDirectWiFiInProximity())
        return NetworkProximityRecommendation::WiFi;

    return NetworkProximityRecommendation::Bluetooth;
}

void NetworkProximityManager::processRecommendations(NSArray *recommendations)
{
    m_shouldUseDirectWiFiInProximity = false;
    bool didSeeDirectWiFiRecommendation = false;
    for (WRM_iRATProximityRecommendation *recommendation in recommendations) {
        switch (recommendation.RecommendationType) {
        case WRMRecommendationLinkTypeBT:
        case WRMRecommendationLinkTypeCompanionWifi:
            break;
        case WRMRecommendationLinkTypeDirectWifi:
            ASSERT(!didSeeDirectWiFiRecommendation);
            didSeeDirectWiFiRecommendation = true;
            m_shouldUseDirectWiFiInProximity = recommendation.linkRecommendationIsValid && recommendation.linkIsRecommended;
            break;
        }
    }

    RELEASE_LOG(ProximityNetworking, "Processed iRATManager recommendations (shouldUseDirectWiFiInProximity: %d).\n", m_shouldUseDirectWiFiInProximity);
}

static WCMProcessId toProcessID(unsigned contextIdentifier)
{
    switch (contextIdentifier) {
    case WRMWebkit:
        return WRMWebkit;
    case WRMWebkitMail:
        return WRMWebkitMail;
    case WRMWebkitNotification:
        return WRMWebkitNotification;
    default:
        return WCMUnknown;
    }
}

void NetworkProximityManager::resumeRecommendations()
{
    if (m_iRATInterface)
        return;

    updateCompanionProximity();

    auto processID = toProcessID(m_contextIdentifier);
    if (processID == WCMUnknown) {
        RELEASE_LOG(ProximityNetworking, "Invalid WCM process ID (%d).", m_contextIdentifier);
        return;
    }

    RELEASE_LOG(ProximityNetworking, "Registering with iRATManager (WCM process ID: %d, state: %hhu).", processID, m_state);
    m_iRATInterface = adoptNS([[WRM_iRATInterface alloc] init]);
    [m_iRATInterface registerClient:processID queue:m_recommendationQueue.get()];
    updateRecommendation();
}

void NetworkProximityManager::suspendRecommendations(State oldState)
{
    // If this is not a foreground-to-background transition, avoid unregistering. Page loads can
    // often cause the network process to resume, transition to the background, then transition to
    // the foreground in rapid succession, causing unnecessary iRATManager registration churn and
    // exposing bugs such as <rdar://problem/42560320>.
    if (m_state == State::Backgrounded && oldState != State::Foregrounded)
        return;

    if (!m_iRATInterface)
        return;

    RELEASE_LOG(ProximityNetworking, "Unregistering with iRATManager (WCM process ID: %d, state: %hhu).", m_contextIdentifier, m_state);
    [m_iRATInterface unregisterClient];
    m_iRATInterface = nil;
    m_isCompanionInProximity = false;
    m_shouldUseDirectWiFiInProximity = false;
}

void NetworkProximityManager::updateCompanionProximity()
{
    m_isCompanionInProximity = false;
    for (IDSDevice *device in [m_idsService devices]) {
        if (device.isDefaultPairedDevice && device.isNearby) {
            m_isCompanionInProximity = true;
            break;
        }
    }

    RELEASE_LOG(ProximityNetworking, "Determined companion proximity (isCompanionInProximity: %d).", m_isCompanionInProximity);
}

void NetworkProximityManager::updateRecommendation()
{
    if (!isCompanionInProximity())
        return;

    if (!m_iRATInterface)
        return;

    RELEASE_LOG(ProximityNetworking, "Requesting an immediate recommendation from iRATManager.");

    auto semaphore = Box<BinarySemaphore>::create();
    [m_iRATInterface getProximityLinkRecommendation:NO recommendation:[this, semaphore](NSArray<WRM_iRATProximityRecommendation *> *recommendations) {
        processRecommendations(recommendations);
        semaphore->signal();
    }];
    semaphore->waitFor(1_s);
}

void NetworkProximityManager::initialize(const NetworkProcessCreationParameters& parameters)
{
    m_contextIdentifier = parameters.wirelessContextIdentifier;
    resume(ResumptionReason::ProcessResuming);
}

void NetworkProximityManager::devicesChanged(NSArray *)
{
    updateCompanionProximity();
}

} // namespace WebKit

#endif // ENABLE(PROXIMITY_NETWORKING)
