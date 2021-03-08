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

#import <pal/cocoa/CryptoKitCBridgingSoftLink.h>

namespace WebCore {

bool PrivateClickMeasurement::calculateAndUpdateSourceUnlinkableToken(const String& serverPublicKeyBase64URL)
{
#if HAVE(RSA_BSSA)
    {
        Vector<uint8_t> serverPublicKeyData;
        if (!base64URLDecode(serverPublicKeyBase64URL, serverPublicKeyData))
            return false;
        auto serverPublicKey = adoptNS([[NSData alloc] initWithBytes:serverPublicKeyData.data() length:serverPublicKeyData.size()]);

        // FIXME(222018): Check error.
        m_sourceUnlinkableToken.blinder = adoptNS([PAL::allocRSABSSATokenBlinderInstance() initWithPublicKey:serverPublicKey.get() error:nullptr]);
        if (!m_sourceUnlinkableToken.blinder)
            return false;
    }

    // FIXME(222018): Check error.
    m_sourceUnlinkableToken.waitingToken = [m_sourceUnlinkableToken.blinder tokenWaitingActivationWithContent:nullptr error:nullptr];
    if (!m_sourceUnlinkableToken.waitingToken)
        return false;

    m_sourceUnlinkableToken.valueBase64URL = WTF::base64URLEncode([m_sourceUnlinkableToken.waitingToken blindedMessage].bytes, [m_sourceUnlinkableToken.waitingToken blindedMessage].length);
    return true;
#else
    UNUSED_PARAM(serverPublicKeyBase64URL);
    return false;
#endif // HAVE(RSA_BSSA)
}

bool PrivateClickMeasurement::calculateAndUpdateSourceSecretToken(const String& serverResponseBase64URL)
{
#if HAVE(RSA_BSSA)
    if (!m_sourceUnlinkableToken.waitingToken)
        return false;

    {
        Vector<uint8_t> serverResponseData;
        if (!base64URLDecode(serverResponseBase64URL, serverResponseData))
            return false;
        auto serverResponse = adoptNS([[NSData alloc] initWithBytes:serverResponseData.data() length:serverResponseData.size()]);

        // FIXME(222018): Check error.
        m_sourceUnlinkableToken.readyToken = [m_sourceUnlinkableToken.waitingToken activateTokenWithServerResponse:serverResponse.get() error:nullptr];
        if (!m_sourceUnlinkableToken.readyToken)
            return false;
    }

    SourceSecretToken token;
    token.tokenBase64URL = WTF::base64URLEncode([m_sourceUnlinkableToken.readyToken tokenContent].bytes, [m_sourceUnlinkableToken.readyToken tokenContent].length);
    token.keyIDBase64URL = WTF::base64URLEncode([m_sourceUnlinkableToken.readyToken keyId].bytes, [m_sourceUnlinkableToken.readyToken keyId].length);
    token.signatureBase64URL = WTF::base64URLEncode([m_sourceUnlinkableToken.readyToken signature].bytes, [m_sourceUnlinkableToken.readyToken signature].length);

    m_sourceSecretToken = WTFMove(token);
    return true;
#else
    UNUSED_PARAM(serverResponseBase64URL);
    return false;
#endif // HAVE(RSA_BSSA)
}

} // namespace WebCore
