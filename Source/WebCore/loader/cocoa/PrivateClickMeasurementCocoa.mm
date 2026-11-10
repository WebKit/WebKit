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

#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/text/MakeString.h>

#import <pal/cocoa/CryptoKitPrivateSoftLink.h>

namespace WebCore {

std::optional<RefPtr<StringImpl>> PrivateClickMeasurement::calculateAndUpdateSourceUnlinkableToken(StringImpl* serverPublicKeyBase64URL)
{
    return calculateAndUpdateUnlinkableToken(serverPublicKeyBase64URL, m_sourceUnlinkableToken, "source"_si);
}

std::expected<PCM::DestinationUnlinkableToken, RefPtr<StringImpl>> PrivateClickMeasurement::calculateAndUpdateDestinationUnlinkableToken(StringImpl* serverPublicKeyBase64URL)
{
    PCM::DestinationUnlinkableToken destinationToken;
    auto errorMessage = calculateAndUpdateUnlinkableToken(serverPublicKeyBase64URL, destinationToken, "destination"_si);
    if (errorMessage)
        return makeUnexpected(*errorMessage);
    return destinationToken;
}

std::optional<RefPtr<StringImpl>> PrivateClickMeasurement::calculateAndUpdateUnlinkableToken(StringImpl* serverPublicKeyBase64URL, PCM::UnlinkableToken& unlinkableToken, StringImpl* contextForLogMessage)
{
#if HAVE(RSA_BSSA)
    {
        auto serverPublicKeyData = base64URLDecode(serverPublicKeyBase64URL);
        if (!serverPublicKeyData)
            return makeString("Could not decode the "_s, contextForLogMessage, "'s public key data."_s);
        RetainPtr serverPublicKey = WTF::toNSData(serverPublicKeyData->span());

        NSError* nsError = 0;
        unlinkableToken.blinder = adoptNS([PAL::allocRSABSSATokenBlinderInstance() initWithPublicKey:serverPublicKey.get() error:&nsError]);
        if (nsError)
            return StringImpl::create(nsError.localizedDescription);
        if (!unlinkableToken.blinder)
            return makeString("Did not get a "_s, contextForLogMessage, " unlinkable token blinder."_s);
    }

    NSError* nsError = 0;
    unlinkableToken.waitingToken = [unlinkableToken.blinder tokenWaitingActivationWithContent:nullptr error:&nsError];
    if (nsError)
        return StringImpl::create(nsError.localizedDescription);
    if (!unlinkableToken.waitingToken)
        return makeString("Did not get a "_s, contextForLogMessage, " unlinkable token waiting token."_s);

    unlinkableToken.valueBase64URL = base64URLEncodeToString(span([unlinkableToken.waitingToken blindedMessage]));
    return std::nullopt;
#else
    UNUSED_PARAM(serverPublicKeyBase64URL);
    UNUSED_PARAM(unlinkableToken);
    UNUSED_PARAM(contextForLogMessage);
    return "Unlinkable tokens are not supported by this platform."_s;
#endif // HAVE(RSA_BSSA)
}

std::optional<RefPtr<StringImpl>> PrivateClickMeasurement::calculateAndUpdateSourceSecretToken(StringImpl* serverResponseBase64URL)
{
    PCM::SourceSecretToken secretToken;
    if (auto errorMessage = calculateAndUpdateSecretToken(serverResponseBase64URL, m_sourceUnlinkableToken, secretToken, "source"_si))
        return errorMessage;
    
    m_sourceSecretToken = WTF::move(secretToken);
    return std::nullopt;
}

std::expected<PCM::DestinationSecretToken, RefPtr<StringImpl>> PrivateClickMeasurement::calculateAndUpdateDestinationSecretToken(StringImpl* serverResponseBase64URL, PCM::DestinationUnlinkableToken& unlinkableToken)
{
    PCM::DestinationSecretToken secretToken;
    auto errorMessage = calculateAndUpdateSecretToken(serverResponseBase64URL, unlinkableToken, secretToken, "destination"_si);
    if (errorMessage)
        return makeUnexpected(*errorMessage);
    return secretToken;
}

std::optional<RefPtr<StringImpl>> PrivateClickMeasurement::calculateAndUpdateSecretToken(StringImpl* serverResponseBase64URL, PCM::UnlinkableToken& unlinkableToken, PCM::SecretToken& secretToken, StringImpl* contextForLogMessage)
{
#if HAVE(RSA_BSSA)
    if (!unlinkableToken.waitingToken)
        return makeString("Did not find a "_s, contextForLogMessage, " unlinkable token waiting token."_s);

    {
        auto serverResponseData = base64URLDecode(serverResponseBase64URL);
        if (!serverResponseData)
            return makeString("Could not decode "_s, contextForLogMessage, " response data."_s);
        RetainPtr serverResponse = WTF::toNSData(serverResponseData->span());

        NSError* nsError = 0;
        unlinkableToken.readyToken = [unlinkableToken.waitingToken activateTokenWithServerResponse:serverResponse.get() error:&nsError];
        if (nsError)
            return StringImpl::create(nsError.localizedDescription);
        if (!unlinkableToken.readyToken)
            return makeString("Did not get a "_s, contextForLogMessage, " unlinkable token ready token."_s);
    }

    secretToken.tokenBase64URL = base64URLEncodeToString(span([unlinkableToken.readyToken tokenContent]));
    secretToken.keyIDBase64URL = base64URLEncodeToString(span([unlinkableToken.readyToken keyId]));
    secretToken.signatureBase64URL = base64URLEncodeToString(span([unlinkableToken.readyToken signature]));

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
