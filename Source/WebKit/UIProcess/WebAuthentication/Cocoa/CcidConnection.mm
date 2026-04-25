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
#import "CcidConnection.h"

#if ENABLE(WEB_AUTHN)
#import "CcidService.h"
#import "Logging.h"
#import <CryptoTokenKit/TKSmartCard.h>
#import <WebCore/FidoConstants.h>
#import <wtf/BlockPtr.h>
#import <wtf/Function.h>
#import <wtf/StdLibExtras.h>
#import <wtf/cocoa/VectorCocoa.h>

#define CCID_RELEASE_LOG(fmt, ...) RELEASE_LOG(WebAuthn, "%p - CcidConnection::" fmt, this, ##__VA_ARGS__)

// SPI for TKSmartCardSlot reinsertion
@interface TKSmartCardSlot (SPI)
- (BOOL)simulateCardReinsertionWithError:(NSError **)error;
@end

@interface WKSmartCardObserver : NSObject
- (instancetype)initWithCard:(TKSmartCard *)card invalidationHandler:(Function<void()>&&)handler;
@end

@implementation WKSmartCardObserver {
    RetainPtr<TKSmartCard> _card;
    Function<void()> _invalidationHandler;
}

- (instancetype)initWithCard:(TKSmartCard *)card invalidationHandler:(Function<void()>&&)handler
{
    if (!(self = [super init]))
        return nil;
    _card = card;
    _invalidationHandler = WTF::move(handler);
    [_card addObserver:self forKeyPath:@"valid" options:NSKeyValueObservingOptionNew context:nil];
    return self;
}

- (void)dealloc
{
    [_card removeObserver:self forKeyPath:@"valid"];
    [super dealloc];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if ([keyPath isEqualToString:@"valid"]) {
        BOOL valid = [[change objectForKey:NSKeyValueChangeNewKey] boolValue];
        if (!valid) {
            @synchronized(self) {
                if (_invalidationHandler) {
                    callOnMainRunLoop([handler = WTF::move(_invalidationHandler)] () mutable {
                        handler();
                    });
                }
            }
        }
    }
}
@end

namespace WebKit {
using namespace fido;

Ref<CcidConnection> CcidConnection::create(RetainPtr<TKSmartCard>&& smartCard, RetainPtr<TKSmartCardSlot>&& slot, CcidService& service)
{
    return adoptRef(*new CcidConnection(WTF::move(smartCard), WTF::move(slot), service));
}

CcidConnection::CcidConnection(RetainPtr<TKSmartCard>&& smartCard, RetainPtr<TKSmartCardSlot>&& slot, CcidService& service)
    : m_smartCard(WTF::move(smartCard))
    , m_slot(WTF::move(slot))
    , m_service(service)
{
    CCID_RELEASE_LOG("created, smartCard=%p, slot=%p", m_smartCard.get(), m_slot.get());

    m_observer = adoptNS([[WKSmartCardObserver alloc]
        initWithCard:m_smartCard.get()
        invalidationHandler:[weakThis = ThreadSafeWeakPtr { *this }] {
            if (RefPtr protectedThis = weakThis.get()) {
                RELEASE_LOG(WebAuthn, "%p - CcidConnection::invalidationHandler fired, m_hasSession=%d, m_sessionPending=%d", protectedThis.get(), protectedThis->m_hasSession, protectedThis->m_sessionPending);
                protectedThis->m_sessionPending = false;
                protectedThis->m_pendingRequests.clear();
                if (protectedThis->m_hasSession) {
                    [protectedThis->m_smartCard endSession];
                    protectedThis->m_hasSession = false;
                }
            }
        }]);

    startPolling();
}

CcidConnection::~CcidConnection()
{
    CCID_RELEASE_LOG("destroyed, m_hasSession=%d", m_hasSession);
    stop();
}

const uint8_t kGetUidCommand[] = {
    0xFF, 0xCA, 0x00, 0x00, 0x00
};

void CcidConnection::detectContactless()
{
    transact(Vector(std::span { kGetUidCommand }), [weakThis = ThreadSafeWeakPtr { *this }] (Vector<uint8_t>&& response) mutable {
        ASSERT(RunLoop::isMain());
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        // Only contactless smart cards have uid, check for longer length than apdu status
        if (response.size() > 2)
            protectedThis->m_contactless = true;
        protectedThis->trySelectFidoApplet();
    });
}

void CcidConnection::trySelectFidoApplet()
{
    transact(Vector(std::span { kCtapNfcAppletSelectionCommand }), [weakThis = ThreadSafeWeakPtr { *this }] (Vector<uint8_t>&& response) mutable {
        ASSERT(RunLoop::isMain());
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (equalSpans(response.span(), std::span { kCtapNfcAppletSelectionU2f })
            || equalSpans(response.span(), std::span { kCtapNfcAppletSelectionCtap })) {
            if (RefPtr service = protectedThis->m_service.get())
                service->didConnectTag();
            return;
        }
        protectedThis->transact(Vector(std::span { kCtapNfcAppletSelectionCommand }), [weakThis = WTF::move(weakThis)] (Vector<uint8_t>&& response) mutable {
            ASSERT(RunLoop::isMain());
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            if (equalSpans(response.span(), std::span { kCtapNfcAppletSelectionU2f })) {
                if (RefPtr service = protectedThis->m_service.get())
                    service->didConnectTag();
            }
        });
    });
}

void CcidConnection::transact(Vector<uint8_t>&& data, DataReceivedCallback&& callback)
{
    CCID_RELEASE_LOG("transact, m_hasSession=%d, m_sessionPending=%d", m_hasSession, m_sessionPending);
    if (m_sessionPending) {
        CCID_RELEASE_LOG("session pending, queuing request");
        m_pendingRequests.append({ WTF::move(data), WTF::move(callback) });
        return;
    }
    if (m_hasSession) {
        CCID_RELEASE_LOG("reusing session, calling transmitRequest");
        [m_smartCard transmitRequest:toNSData(data).get() reply:makeBlockPtr([protectedThis = Ref { *this }, callback = WTF::move(callback)](NSData * _Nullable nsResponse, NSError * _Nullable error) mutable {
            RELEASE_LOG(WebAuthn, "%p - CcidConnection::transmitRequest reply, error=%p", protectedThis.ptr(), error);
            bool hasError = !!error;
            callOnMainRunLoop([protectedThis = WTF::move(protectedThis), response = makeVector(nsResponse), callback = WTF::move(callback), hasError] () mutable {
                if (hasError) {
                    [protectedThis->m_smartCard endSession];
                    protectedThis->m_hasSession = false;
                }
                callback(WTF::move(response));
            });
        }).get()];
    } else {
        CCID_RELEASE_LOG("no session, calling beginSessionWithReply");
        m_sessionPending = true;
        [m_smartCard beginSessionWithReply:makeBlockPtr([protectedThis = Ref { *this }, data = WTF::move(data), callback = WTF::move(callback)] (BOOL success, NSError *error) mutable {
            RELEASE_LOG(WebAuthn, "%p - CcidConnection::beginSessionWithReply reply, success=%d, error=%p", protectedThis.ptr(), success, error);
            if (!success) {
                callOnMainRunLoop([protectedThis = WTF::move(protectedThis), callback = WTF::move(callback)] () mutable {
                    protectedThis->m_sessionPending = false;
                    callback({ });
                    // Drain pending requests with empty callbacks on failure
                    while (!protectedThis->m_pendingRequests.isEmpty()) {
                        auto pending = protectedThis->m_pendingRequests.takeFirst();
                        pending.second({ });
                    }
                });
                return;
            }
            callOnMainRunLoop([protectedThis = WTF::move(protectedThis), data = WTF::move(data), callback = WTF::move(callback)] () mutable {
                protectedThis->m_sessionPending = false;
                protectedThis->m_hasSession = true;
                RELEASE_LOG(WebAuthn, "%p - CcidConnection::session started", protectedThis.ptr());
                [protectedThis->m_smartCard transmitRequest:toNSData(data).get() reply:makeBlockPtr([protectedThis = WTF::move(protectedThis), callback = WTF::move(callback)](NSData * _Nullable nsResponse, NSError * _Nullable error) mutable {
                    RELEASE_LOG(WebAuthn, "%p - CcidConnection::transmitRequest reply, error=%p", protectedThis.ptr(), error);
                    bool hasError = !!error;
                    callOnMainRunLoop([protectedThis = WTF::move(protectedThis), response = makeVector(nsResponse), callback = WTF::move(callback), hasError] () mutable {
                        if (hasError) {
                            [protectedThis->m_smartCard endSession];
                            protectedThis->m_hasSession = false;
                        }
                        callback(WTF::move(response));
                        protectedThis->processPendingRequests();
                    });
                }).get()];
            });
        }).get()];
    }
}


void CcidConnection::processPendingRequests()
{
    if (m_pendingRequests.isEmpty() || !m_hasSession)
        return;
    CCID_RELEASE_LOG("processPendingRequests, count=%zu", m_pendingRequests.size());
    auto pending = m_pendingRequests.takeFirst();
    transact(WTF::move(pending.first), WTF::move(pending.second));
}

void CcidConnection::stop()
{
    CCID_RELEASE_LOG("stop, m_hasSession=%d, m_sessionPending=%d", m_hasSession, m_sessionPending);
    m_sessionPending = false;
    m_pendingRequests.clear();
    if (m_hasSession) {
        CCID_RELEASE_LOG("ending session");
        [m_smartCard endSession];
        m_hasSession = false;
    }
}

void CcidConnection::startPolling()
{
    detectContactless();
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
