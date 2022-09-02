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
#import <wtf/BlockPtr.h>
#import <wtf/cocoa/VectorCocoa.h>

namespace WebKit {
using namespace fido;

namespace {

// FIXME: This is duplicate code with compareVersion in NfcConnection.mm.
inline bool compareCcidVersion(NSData *data, const uint8_t version[], size_t versionSize)
{
    if (!data)
        return false;
    if (data.length != versionSize)
        return false;
    return !memcmp(data.bytes, version, versionSize);
}

} // namespace

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
    [m_smartCard transmitRequest:adoptNS([[NSData alloc] initWithBytes:kGetUidCommand length:sizeof(kGetUidCommand)]).get() reply:makeBlockPtr([this](NSData * _Nullable versionData, NSError * _Nullable error) {
        // Only contactless smart cards have uid, check for longer length than apdu status
        if (versionData && [versionData length] > 2) {
            callOnMainRunLoop([this] () mutable {
                m_contactless = true;
            });
        }
    }).get()];
}

void CcidConnection::trySelectFidoApplet()
{
    [m_smartCard transmitRequest:adoptNS([[NSData alloc] initWithBytes:kCtapNfcAppletSelectionCommand length:sizeof(kCtapNfcAppletSelectionCommand)]).get() reply:makeBlockPtr([this](NSData * _Nullable versionData, NSError * _Nullable error) {
        if (compareCcidVersion(versionData, kCtapNfcAppletSelectionU2f, sizeof(kCtapNfcAppletSelectionU2f))
            || compareCcidVersion(versionData, kCtapNfcAppletSelectionCtap, sizeof(kCtapNfcAppletSelectionCtap))) {
            callOnMainRunLoop([this] () mutable {
                if (m_service)
                    m_service->didConnectTag();
            });
            return;
        }
            [m_smartCard transmitRequest:adoptNS([[NSData alloc] initWithBytes:kCtapNfcU2fVersionCommand length:sizeof(kCtapNfcU2fVersionCommand)]).get() reply:makeBlockPtr([this](NSData * _Nullable versionData, NSError * _Nullable error) {
                if (compareCcidVersion(versionData, kCtapNfcAppletSelectionU2f, sizeof(kCtapNfcAppletSelectionU2f))) {
                    callOnMainRunLoop([this] () mutable {
                        if (m_service)
                            m_service->didConnectTag();
                    });
                    return;
                }
            }).get()];
    }).get()];
}

void CcidConnection::transact(Vector<uint8_t>&& data, DataReceivedCallback&& callback) const
{
    [m_smartCard transmitRequest:adoptNS([[NSData alloc] initWithBytes:data.data() length:data.size()]).autorelease() reply:makeBlockPtr([this, callback = WTFMove(callback)](NSData * _Nullable nsResponse, NSError * _Nullable error) mutable {
        auto response = vectorFromNSData(nsResponse);
        callOnMainRunLoop([this, response = WTFMove(response), callback = WTFMove(callback)] () mutable {
            callback(WTFMove(response));
            (void)this;
        });
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
    [m_smartCard beginSessionWithReply:makeBlockPtr([this] (BOOL success, NSError *error) mutable {
        detectContactless();
        trySelectFidoApplet();
    }).get()];
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
