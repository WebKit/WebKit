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
    return calculateAndUpdateUnlinkableToken(serverPublicKeyBase64URL, m_sourceUnlinkableToken, "source"_s);
}

Expected<PCM::DestinationUnlinkableToken, String> PrivateClickMeasurement::calculateAndUpdateDestinationUnlinkableToken(const String& serverPublicKeyBase64URL)
{
    PCM::DestinationUnlinkableToken destinationToken;
    auto errorMessage = calculateAndUpdateUnlinkableToken(serverPublicKeyBase64URL, destinationToken, "destination"_s);
    if (errorMessage)
        return makeUnexpected(*errorMessage);
    return destinationToken;
}

std::optional<String> PrivateClickMeasurement::calculateAndUpdateUnlinkableToken(const String& serverPublicKeyBase64URL, PCM::UnlinkableToken& unlinkableToken, const String& contextForLogMessage)
{
#if HAVE(RSA_BSSA)
    {
        auto serverPublicKeyData = base64URLDecode(serverPublicKeyBase64URL);
        if (!serverPublicKeyData)
            return makeString("Could not decode the ", contextForLogMessage, "'s public key data.");
        auto serverPublicKey = adoptNS([[NSData alloc] initWithBytes:serverPublicKeyData->data() length:serverPublicKeyData->size()]);

        NSError* nsError = 0;
        unlinkableToken.blinder = adoptNS([PAL::allocRSABSSATokenBlinderInstance() initWithPublicKey:serverPublicKey.get() error:&nsError]);
        if (nsError)
            return nsError.localizedDescription;
        if (!unlinkableToken.blinder)
            return makeString("Did not get a ", contextForLogMessage, " unlinkable token blinder.");
    }

    NSError* nsError = 0;
    unlinkableToken.waitingToken = [unlinkableToken.blinder tokenWaitingActivationWithContent:nullptr error:&nsError];
    if (nsError)
        return nsError.localizedDescription;
    if (!unlinkableToken.waitingToken)
        return makeString("Did not get a ", contextForLogMessage, " unlinkable token waiting token.");

    unlinkableToken.valueBase64URL = base64URLEncodeToString([unlinkableToken.waitingToken blindedMessage].bytes, [unlinkableToken.waitingToken blindedMessage].length);
    return std::nullopt;
#else
    UNUSED_PARAM(serverPublicKeyBase64URL);
    UNUSED_PARAM(unlinkableToken);
    UNUSED_PARAM(contextForLogMessage);
    return "Unlinkable tokens are not supported by this platform."_s;
#endif // HAVE(RSA_BSSA)
}

std::optional<String> PrivateClickMeasurement::calculateAndUpdateSourceSecretToken(const String& serverResponseBase64URL)
{
    PCM::SourceSecretToken secretToken;
    if (auto errorMessage = calculateAndUpdateSecretToken(serverResponseBase64URL, m_sourceUnlinkableToken, secretToken, "source"_s))
        return errorMessage;
    
    m_sourceSecretToken = WTFMove(secretToken);
    return std::nullopt;
}

Expected<PCM::DestinationSecretToken, String> PrivateClickMeasurement::calculateAndUpdateDestinationSecretToken(const String& serverResponseBase64URL, PCM::DestinationUnlinkableToken& unlinkableToken)
{
    PCM::DestinationSecretToken secretToken;
    auto errorMessage = calculateAndUpdateSecretToken(serverResponseBase64URL, unlinkableToken, secretToken, "source"_s);
    if (errorMessage)
        return makeUnexpected(*errorMessage);
    return secretToken;
}

std::optional<String> PrivateClickMeasurement::calculateAndUpdateSecretToken(const String& serverResponseBase64URL, PCM::UnlinkableToken& unlinkableToken, PCM::SecretToken& secretToken, const String& contextForLogMessage)
{
#if HAVE(RSA_BSSA)
    if (!unlinkableToken.waitingToken)
        return makeString("Did not find a ", contextForLogMessage, " unlinkable token waiting token.");

    {
        auto serverResponseData = base64URLDecode(serverResponseBase64URL);
        if (!serverResponseData)
            return makeString("Could not decode ", contextForLogMessage, " response data.");
        auto serverResponse = adoptNS([[NSData alloc] initWithBytes:serverResponseData->data() length:serverResponseData->size()]);

        NSError* nsError = 0;
        unlinkableToken.readyToken = [unlinkableToken.waitingToken activateTokenWithServerResponse:serverResponse.get() error:&nsError];
        if (nsError)
            return nsError.localizedDescription;
        if (!unlinkableToken.readyToken)
            return makeString("Did not get a ", contextForLogMessage, " unlinkable token ready token.");
    }

    secretToken.tokenBase64URL = base64URLEncodeToString([unlinkableToken.readyToken tokenContent].bytes, [unlinkableToken.readyToken tokenContent].length);
    secretToken.keyIDBase64URL = base64URLEncodeToString([unlinkableToken.readyToken keyId].bytes, [unlinkableToken.readyToken keyId].length);
    secretToken.signatureBase64URL = base64URLEncodeToString([unlinkableToken.readyToken signature].bytes, [unlinkableToken.readyToken signature].length);

    return std::nullopt;
#else
    UNUSED_PARAM(serverResponseBase64URL);
    UNUSED_PARAM(unlinkableToken);
    UNUSED_PARAM(secretToken);
    UNUSED_PARAM(contextForLogMessage);
    return "Unlinkable tokens are not supported by this platform."_s;
#endif // HAVE(RSA_BSSA)
}

} // namespace WebCore
