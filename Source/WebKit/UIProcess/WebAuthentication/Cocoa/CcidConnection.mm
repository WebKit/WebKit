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
#import <CryptoTokenKit/TKSmartCard.h>
#import <WebCore/FidoConstants.h>
#import <wtf/Algorithms.h>
#import <wtf/BlockPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/cocoa/VectorCocoa.h>

namespace WebKit {
using namespace fido;

Ref<CcidConnection> CcidConnection::create(RetainPtr<TKSmartCard>&& smartCard, CcidService& service)
{
    return adoptRef(*new CcidConnection(WTFMove(smartCard), service));
}

CcidConnection::CcidConnection(RetainPtr<TKSmartCard>&& smartCard, CcidService& service)
    : m_smartCard(WTFMove(smartCard))
    , m_service(service)
    , m_retryTimer(RunLoop::main(), this, &CcidConnection::startPolling)
{
    startPolling();
}

CcidConnection::~CcidConnection()
{
    stop();
}

const uint8_t kGetUidCommand[] = {
    0xFF, 0xCA, 0x00, 0x00, 0x00
};

void CcidConnection::detectContactless()
{
    transact(Vector(std::span { kGetUidCommand }), [weakThis = WeakPtr { *this }] (Vector<uint8_t>&& response) mutable {
        ASSERT(RunLoop::isMain());
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        // Only contactless smart cards have uid, check for longer length than apdu status
        if (response.size() > 2)
            protectedThis->m_contactless = true;
    });
}

void CcidConnection::trySelectFidoApplet()
{
    transact(Vector(std::span { kCtapNfcAppletSelectionCommand }), [weakThis = WeakPtr { *this }] (Vector<uint8_t>&& response) mutable {
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
        protectedThis->transact(Vector(std::span { kCtapNfcAppletSelectionCommand }), [weakThis = WTFMove(weakThis)] (Vector<uint8_t>&& response) mutable {
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

void CcidConnection::transact(Vector<uint8_t>&& data, DataReceivedCallback&& callback) const
{
    [m_smartCard beginSessionWithReply:makeBlockPtr([this, data = WTFMove(data), callback = WTFMove(callback)] (BOOL success, NSError *error) mutable {
        if (!success)
            return;
        [m_smartCard transmitRequest:toNSData(data).autorelease() reply:makeBlockPtr([this, callback = WTFMove(callback)](NSData * _Nullable nsResponse, NSError * _Nullable error) mutable {
            [m_smartCard endSession];
            callOnMainRunLoop([response = makeVector(nsResponse), callback = WTFMove(callback)] () mutable {
                callback(WTFMove(response));
            });
        }).get()];
    }).get()];
}


void CcidConnection::stop() const
{
}

// NearField polling is a one shot polling. It halts after tags are detected.
// Therefore, a restart process is needed to resume polling after error.
void CcidConnection::restartPolling()
{
    m_retryTimer.startOneShot(1_s); // Magic number to give users enough time for reactions.
}

void CcidConnection::startPolling()
{
    detectContactless();
    trySelectFidoApplet();
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
