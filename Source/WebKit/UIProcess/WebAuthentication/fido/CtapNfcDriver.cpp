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

#include "config.h"
#include "CtapNfcDriver.h"

#if ENABLE(WEB_AUTHN) && HAVE(NEAR_FIELD)

#include <WebCore/ApduCommand.h>
#include <WebCore/ApduResponse.h>
#include <wtf/RunLoop.h>

namespace WebKit {
using namespace apdu;
using namespace fido;

CtapNfcDriver::CtapNfcDriver(UniqueRef<NfcConnection>&& connection)
    : m_connection(WTFMove(connection))
{
}

// FIXME(200934): Support NFCCTAP_GETRESPONSE
void CtapNfcDriver::transact(Vector<uint8_t>&& data, ResponseCallback&& callback)
{
    // For CTAP2, commands follow:
    // https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#nfc-command-framing
    if (protocol() == ProtocolVersion::kCtap) {
        ApduCommand command;
        command.setCla(kCtapNfcApduCla);
        command.setIns(kCtapNfcApduIns);
        command.setData(WTFMove(data));
        command.setResponseLength(ApduCommand::kApduMaxResponseLength);

        auto apduResponse = ApduResponse::createFromMessage(m_connection->transact(command.getEncodedCommand()));
        if (!apduResponse) {
            respondAsync(WTFMove(callback), { });
            return;
        }
        if (apduResponse->status() == ApduResponse::Status::SW_INS_NOT_SUPPORTED) {
            // Return kCtap1ErrInvalidCommand instead of an empty response to signal FidoService to create a U2F authenticator
            // for the getInfo stage.
            respondAsync(WTFMove(callback), { static_cast<uint8_t>(CtapDeviceResponseCode::kCtap1ErrInvalidCommand) });
            return;
        }
        if (apduResponse->status() != ApduResponse::Status::SW_NO_ERROR) {
            respondAsync(WTFMove(callback), { });
            return;
        }

        respondAsync(WTFMove(callback), WTFMove(apduResponse->data()));
        return;
    }

    // For U2F, U2fAuthenticator would handle the APDU encoding.
    // https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-nfc-protocol-v1.2-ps-20170411.html#framing
    callback(m_connection->transact(WTFMove(data)));
}

// Return the response async to match the HID behaviour, such that nfc could fit into the current infra.
void CtapNfcDriver::respondAsync(ResponseCallback&& callback, Vector<uint8_t>&& response) const
{
    RunLoop::main().dispatch([callback = WTFMove(callback), response = WTFMove(response)] () mutable {
        callback(WTFMove(response));
    });
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN) && HAVE(NEAR_FIELD)
