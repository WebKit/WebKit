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
    if (!data)
        return false;
    if (data.length != versionSize)
        return false;
    return !memcmp(data.bytes, version, versionSize);
}

// Confirm the FIDO applet is avaliable.
// https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#nfc-applet-selection
static bool trySelectFidoApplet(NFReaderSession *session)
{
    auto *versionData = [session transceive:adoptNS([[NSData alloc] initWithBytes:kCtapNfcAppletSelectionCommand length:sizeof(kCtapNfcAppletSelectionCommand)]).get()];
    if (compareVersion(versionData, kCtapNfcAppletSelectionU2f, sizeof(kCtapNfcAppletSelectionU2f))
        || compareVersion(versionData, kCtapNfcAppletSelectionCtap, sizeof(kCtapNfcAppletSelectionCtap)))
        return true;

    // Some legacy U2F keys such as Google T1 Titan don't understand the FIDO applet command. Instead,
    // they are configured to only have the FIDO applet. Therefore, when the above command fails, we
    // use U2F_VERSION command to double check if the connected tag can actually speak U2F, indicating
    // we are interacting with one of these legacy keys.
    versionData = [session transceive:adoptNS([[NSData alloc] initWithBytes:kCtapNfcU2fVersionCommand length:sizeof(kCtapNfcU2fVersionCommand)]).get()];
    if (compareVersion(versionData, kCtapNfcAppletSelectionU2f, sizeof(kCtapNfcAppletSelectionU2f)))
        return true;

    return false;
}

} // namespace

Ref<NfcConnection> NfcConnection::create(RetainPtr<NFReaderSession>&& session, NfcService& service)
{
    return adoptRef(*new NfcConnection(WTFMove(session), service));
}

NfcConnection::NfcConnection(RetainPtr<NFReaderSession>&& session, NfcService& service)
    : m_session(WTFMove(session))
    , m_delegate(adoptNS([[WKNFReaderSessionDelegate alloc] initWithConnection:*this]))
    , m_service(makeWeakPtr(service))
    , m_retryTimer(RunLoop::main(), this, &NfcConnection::startPolling)
{
    [m_session setDelegate:m_delegate.get()];
    startPolling();
}

NfcConnection::~NfcConnection()
{
    stop();
}

Vector<uint8_t> NfcConnection::transact(Vector<uint8_t>&& data) const
{
    Vector<uint8_t> response;
    // The method will return an empty NSData if the tag is disconnected.
    auto *responseData = [m_session transceive:adoptNS([[NSData alloc] initWithBytes:data.data() length:data.size()]).get()];
    response.append(reinterpret_cast<const uint8_t*>(responseData.bytes), responseData.length);
    return response;
}

void NfcConnection::stop() const
{
    [m_session disconnectTag];
    [m_session stopPolling];
    [m_session endSession];
}

void NfcConnection::didDetectTags(NSArray *tags)
{
    if (!m_service || !tags.count)
        return;

    // A physical NFC tag could have multiple interfaces.
    // Therefore, we use tagID to detect if there are multiple physical tags.
    NSData *tagID = ((NFTag *)tags[0]).tagID;
    for (NFTag *tag : tags) {
        if ([tagID isEqualToData:tag.tagID])
            continue;
        m_service->didDetectMultipleTags();
        restartPolling();
        return;
    }

    // FIXME(203234): Tell users to switch to a different tag if the tag is not of type NFTagTypeGeneric4A
    // or can't speak U2F/FIDO2.
    for (NFTag *tag : tags) {
        if (tag.type != NFTagTypeGeneric4A || ![m_session connectTag:tag])
            continue;

        if (!trySelectFidoApplet(m_session.get())) {
            [m_session disconnectTag];
            continue;
        }

        m_service->didConnectTag();
        return;
    }
    restartPolling();
}

// NearField polling is a one shot polling. It halts after tags are detected.
// Therefore, a restart process is needed to resume polling after error.
void NfcConnection::restartPolling()
{
    [m_session stopPolling];
    m_retryTimer.startOneShot(1_s); // Magic number to give users enough time for reactions.
}

void NfcConnection::startPolling()
{
    NSError *error = nil;
    [m_session startPollingWithError:&error];
    if (error)
        LOG_ERROR("Couldn't start NFC reader polling: %@", error);
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN) && HAVE(NEAR_FIELD)
