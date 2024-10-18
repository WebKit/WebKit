/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "CcidService.h"

#if ENABLE(WEB_AUTHN)

#import "CcidConnection.h"
#import "CtapCcidDriver.h"
#import <CryptoTokenKit/TKSmartCard.h>
#import <WebCore/AuthenticatorTransport.h>
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>

@interface _WKSmartCardSlotObserver : NSObject {
    WeakPtr<WebKit::CcidService> m_service;
}

- (instancetype)initWithService:(WeakPtr<WebKit::CcidService>&&)service;
- (void)observeValueForKeyPath:(id)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context;
@end

@interface _WKSmartCardSlotStateObserver : NSObject {
    WeakPtr<WebKit::CcidService> m_service;
    RetainPtr<TKSmartCardSlot> m_slot;
}

- (instancetype)initWithService:(WeakPtr<WebKit::CcidService>&&)service slot:(RetainPtr<TKSmartCardSlot>&&)slot;
- (void)observeValueForKeyPath:(id)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context;
- (void)removeObserver;
@end

namespace WebKit {

Ref<CcidService> CcidService::create(AuthenticatorTransportServiceObserver& observer)
{
    return adoptRef(*new CcidService(observer));
}

CcidService::CcidService(AuthenticatorTransportServiceObserver& observer)
    : FidoService(observer)
    , m_restartTimer(RunLoop::main(), this, &CcidService::platformStartDiscovery)
{
}

CcidService::~CcidService()
{
    removeObservers();
}

void CcidService::didConnectTag()
{
    auto connection = m_connection;
    getInfo(CtapCcidDriver::create(connection.releaseNonNull(), m_connection->contactless() ? WebCore::AuthenticatorTransport::Nfc : WebCore::AuthenticatorTransport::SmartCard));
}

void CcidService::startDiscoveryInternal()
{
    platformStartDiscovery();
}

void CcidService::restartDiscoveryInternal()
{
    m_restartTimer.startOneShot(1_s); // Magic number to give users enough time for reactions.
}

void CcidService::removeObservers()
{
    if (m_slotsObserver) {
        [[TKSmartCardSlotManager defaultManager] removeObserver:m_slotsObserver.get() forKeyPath:@"slotNames"];
        m_slotsObserver.clear();
    }
    for (auto observer : m_slotObservers.values())
        [observer removeObserver];
    m_slotObservers.clear();
}

void CcidService::platformStartDiscovery()
{
    removeObservers();
    m_slotsObserver = adoptNS([[_WKSmartCardSlotObserver alloc] initWithService:this]);
    [[TKSmartCardSlotManager defaultManager] addObserver:m_slotsObserver.get() forKeyPath:@"slotNames" options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionInitial context:nil];
}

void CcidService::onValidCard(RetainPtr<TKSmartCard>&& smartCard)
{
    m_connection = WebKit::CcidConnection::create(WTFMove(smartCard), *this);
}

void CcidService::updateSlots(NSArray *slots)
{
    HashSet<String> slotsSet;
    for (NSString *nsName : slots) {
        auto name = String(nsName);
        slotsSet.add(name);
        auto it = m_slotObservers.find(name);
        if (it == m_slotObservers.end()) {
            [[TKSmartCardSlotManager defaultManager] getSlotWithName:nsName reply:makeBlockPtr([this, name](TKSmartCardSlot * _Nullable slot) mutable {
                auto slotObserver = adoptNS([[_WKSmartCardSlotStateObserver alloc] initWithService:this slot:WTFMove(slot)]);
                m_slotObservers.add(name, slotObserver);
                [slot addObserver:slotObserver.get() forKeyPath:@"state" options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionInitial context:nil];
            }).get()];
        }
    }
    HashSet<String> staleSlots;
    for (auto& slotPair : m_slotObservers) {
        if (!slotsSet.contains(slotPair.key)) {
            staleSlots.add(slotPair.key);
            [slotPair.value removeObserver];
        }
    }
    for (const String& slot : staleSlots)
        m_slotObservers.remove(slot);
}

} // namespace WebKit

@implementation _WKSmartCardSlotObserver
- (instancetype)initWithService:(WeakPtr<WebKit::CcidService>&&)service
{
    if (!(self = [super init]))
        return nil;

    m_service = WTFMove(service);

    return self;
}

- (void)observeValueForKeyPath:(id)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    UNUSED_PARAM(object);
    UNUSED_PARAM(change);
    UNUSED_PARAM(context);

    callOnMainRunLoop([service = m_service, change = retainPtr(change)] () mutable {
        if (!service)
            return;
        service->updateSlots(change.get()[NSKeyValueChangeNewKey]);
    });
}
@end

@implementation _WKSmartCardSlotStateObserver
- (instancetype)initWithService:(WeakPtr<WebKit::CcidService>&&)service slot:(RetainPtr<TKSmartCardSlot>&&)slot
{
    if (!(self = [super init]))
        return nil;

    m_service = WTFMove(service);
    m_slot = WTFMove(slot);

    return self;
}

- (void)observeValueForKeyPath:(id)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    UNUSED_PARAM(object);
    UNUSED_PARAM(change);
    UNUSED_PARAM(context);

    if (!m_service)
        return;
    switch ([change[NSKeyValueChangeNewKey] intValue]) {
    case TKSmartCardSlotStateMissing:
        [self removeObserver];
        return;
    case TKSmartCardSlotStateValidCard: {
        auto* smartCard = [object makeSmartCard];
        callOnMainRunLoop([service = m_service, smartCard = retainPtr(smartCard)] () mutable {
            if (!service)
                return;
            service->onValidCard(WTFMove(smartCard));
        });
        break;
    }
    default:
        break;
    }
}

- (void)removeObserver
{
    if (m_slot) {
        [m_slot removeObserver:self forKeyPath:@"state"];
        m_slot.clear();
    }
}
@end

#endif // ENABLE(WEB_AUTHN)
