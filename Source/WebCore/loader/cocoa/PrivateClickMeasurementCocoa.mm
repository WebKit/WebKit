/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "PrivateClickMeasurement.h"

#import <pal/cocoa/CryptoKitPrivateSoftLink.h>

namespace WebCore {

std::optional<String> PrivateClickMeasurement::calculateAndUpdateSourceUnlinkableToken(const String& serverPublicKeyBase64URL)
{
#if HAVE(RSA_BSSA)
    {
        auto serverPublicKeyData = base64URLDecode(serverPublicKeyBase64URL);
        if (!serverPublicKeyData)
            return "Could not decode the source's public key data."_s;
        auto serverPublicKey = adoptNS([[NSData alloc] initWithBytes:serverPublicKeyData->data() length:serverPublicKeyData->size()]);

        NSError* nsError = 0;
        m_sourceUnlinkableToken.blinder = adoptNS([PAL::allocRSABSSATokenBlinderInstance() initWithPublicKey:serverPublicKey.get() error:&nsError]);
        if (nsError)
            return nsError.localizedDescription;
        if (!m_sourceUnlinkableToken.blinder)
            return "Did not get a source unlinkable token blinder."_s;
    }

    NSError* nsError = 0;
    m_sourceUnlinkableToken.waitingToken = [m_sourceUnlinkableToken.blinder tokenWaitingActivationWithContent:nullptr error:&nsError];
    if (nsError)
        return nsError.localizedDescription;
    if (!m_sourceUnlinkableToken.waitingToken)
        return "Did not get a source unlinkable token waiting token."_s;

    m_sourceUnlinkableToken.valueBase64URL = base64URLEncodeToString([m_sourceUnlinkableToken.waitingToken blindedMessage].bytes, [m_sourceUnlinkableToken.waitingToken blindedMessage].length);
    return std::nullopt;
#else
    UNUSED_PARAM(serverPublicKeyBase64URL);
    return "Unlinkable tokens are not supported by this platform."_s;
#endif // HAVE(RSA_BSSA)
}

std::optional<String> PrivateClickMeasurement::calculateAndUpdateSourceSecretToken(const String& serverResponseBase64URL)
{
#if HAVE(RSA_BSSA)
    if (!m_sourceUnlinkableToken.waitingToken)
        return "Did not find a source unlinkable token waiting token."_s;

    {
        auto serverResponseData = base64URLDecode(serverResponseBase64URL);
        if (!serverResponseData)
            return "Could not decode source response data."_s;
        auto serverResponse = adoptNS([[NSData alloc] initWithBytes:serverResponseData->data() length:serverResponseData->size()]);

        NSError* nsError = 0;
        m_sourceUnlinkableToken.readyToken = [m_sourceUnlinkableToken.waitingToken activateTokenWithServerResponse:serverResponse.get() error:&nsError];
        if (nsError)
            return nsError.localizedDescription;
        if (!m_sourceUnlinkableToken.readyToken)
            return "Did not get a source unlinkable token ready token."_s;
    }

    SourceSecretToken token;
    token.tokenBase64URL = base64URLEncodeToString([m_sourceUnlinkableToken.readyToken tokenContent].bytes, [m_sourceUnlinkableToken.readyToken tokenContent].length);
    token.keyIDBase64URL = base64URLEncodeToString([m_sourceUnlinkableToken.readyToken keyId].bytes, [m_sourceUnlinkableToken.readyToken keyId].length);
    token.signatureBase64URL = base64URLEncodeToString([m_sourceUnlinkableToken.readyToken signature].bytes, [m_sourceUnlinkableToken.readyToken signature].length);

    m_sourceSecretToken = WTFMove(token);
    return std::nullopt;
#else
    UNUSED_PARAM(serverResponseBase64URL);
    return "Unlinkable tokens are not supported by this platform."_s;
#endif // HAVE(RSA_BSSA)
}

} // namespace WebCore
