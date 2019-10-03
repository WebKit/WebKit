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
#import "NfcConnection.h"

#if ENABLE(WEB_AUTHN) && HAVE(NEAR_FIELD)

#import "NfcService.h"
#import "WKNFReaderSessionDelegate.h"
#import <WebCore/FidoConstants.h>
#import <wtf/text/Base64.h>

namespace WebKit {
using namespace fido;

namespace {
inline bool compareVersion(NSData *data, const uint8_t version[], size_t versionSize)
{
    if (data.length != versionSize)
        return false;
    return !memcmp(data.bytes, version, versionSize);
}
} // namespace

NfcConnection::NfcConnection(RetainPtr<NFReaderSession>&& session, NfcService& service)
    : m_session(WTFMove(session))
    , m_delegate(adoptNS([[WKNFReaderSessionDelegate alloc] initWithConnection:*this]))
    , m_service(makeWeakPtr(service))
{
    [m_session setDelegate:m_delegate.get()];
    // FIXME(200933)
    [m_session updateUIAlertMessage:@"Insert your security key or hold the key against the top of your device."];
    [m_session startPolling];
}

NfcConnection::~NfcConnection()
{
    [m_session disconnectTag];
    [m_session stopPolling];
    [m_session endSession];
}

Vector<uint8_t> NfcConnection::transact(Vector<uint8_t>&& data) const
{
    Vector<uint8_t> response;
    @autoreleasepool {
        auto responseData = [m_session transceive:[NSData dataWithBytes:data.data() length:data.size()]];
        response.append(reinterpret_cast<const uint8_t*>(responseData.bytes), responseData.length);
    }
    return response;
}

void NfcConnection::didDetectTags(NSArray *tags) const
{
    if (!m_service)
        return;

    // FIXME(200932): Warn users when multiple NFC tags present
    for (NFTag *tag in tags) {
        if (tag.type != NFTagTypeGeneric4A)
            continue;
        if (![m_session connectTag:tag])
            continue;

        // Confirm the FIDO applet is avaliable before return.
        // https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#nfc-applet-selection
        @autoreleasepool {
            auto versionData = [m_session transceive:[NSData dataWithBytes:kCtapNfcAppletSelectionCommand length:sizeof(kCtapNfcAppletSelectionCommand)]];
            if (!versionData || (!compareVersion(versionData, kCtapNfcAppletSelectionU2f, sizeof(kCtapNfcAppletSelectionU2f)) && !compareVersion(versionData, kCtapNfcAppletSelectionCtap, sizeof(kCtapNfcAppletSelectionCtap)))) {
                [m_session disconnectTag];
                continue;
            }
        }

        m_service->didConnectTag();
        break;
    }
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN) && HAVE(NEAR_FIELD)
