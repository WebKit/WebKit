// Copyright 2018 The Chromium Authors. All rights reserved.
// Copyright (C) 2019 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "U2fCommandConstructor.h"

#if ENABLE(WEB_AUTHN)

#include "ApduCommand.h"
#include "FidoConstants.h"
#include "PublicKeyCredentialCreationOptions.h"
#include "PublicKeyCredentialRequestOptions.h"
#include "UserVerificationRequirement.h"
#include "WebAuthenticationConstants.h"
#include "WebAuthenticationUtils.h"
#include <wtf/Optional.h>

namespace fido {
using namespace WebCore;

namespace {

static Vector<uint8_t> constructU2fRegisterCommand(const Vector<uint8_t>& applicationParameter, const Vector<uint8_t>& challengeParameter)
{
    Vector<uint8_t> data;
    data.reserveInitialCapacity(kU2fChallengeParamLength + kU2fApplicationParamLength);
    data.appendVector(challengeParameter);
    data.appendVector(applicationParameter);

    apdu::ApduCommand command;
    command.setIns(static_cast<uint8_t>(U2fApduInstruction::kRegister));
    // This is needed for test of user presence even though the spec doesn't specify it.
    command.setP1(kP1EnforceUserPresenceAndSign);
    command.setData(WTFMove(data));
    command.setResponseLength(apdu::ApduCommand::kApduMaxResponseLength);
    return command.getEncodedCommand();
}

static Optional<Vector<uint8_t>> constructU2fSignCommand(const Vector<uint8_t>& applicationParameter, const Vector<uint8_t>& challengeParameter, const Vector<uint8_t>& keyHandle, bool checkOnly)
{
    if (keyHandle.size() > kMaxKeyHandleLength)
        return WTF::nullopt;

    Vector<uint8_t> data;
    data.reserveInitialCapacity(kU2fChallengeParamLength + kU2fApplicationParamLength + 1 + keyHandle.size());
    data.appendVector(challengeParameter);
    data.appendVector(applicationParameter);
    data.append(static_cast<uint8_t>(keyHandle.size()));
    data.appendVector(keyHandle);

    apdu::ApduCommand command;
    command.setIns(static_cast<uint8_t>(U2fApduInstruction::kSign));
    command.setP1(checkOnly ? kP1CheckOnly : kP1EnforceUserPresenceAndSign);
    command.setData(WTFMove(data));
    command.setResponseLength(apdu::ApduCommand::kApduMaxResponseLength);
    return command.getEncodedCommand();
}

} // namespace

bool isConvertibleToU2fRegisterCommand(const PublicKeyCredentialCreationOptions& request)
{
    if (request.authenticatorSelection && (request.authenticatorSelection->userVerification == UserVerificationRequirement::Required || request.authenticatorSelection->requireResidentKey))
        return false;
    if (request.pubKeyCredParams.findMatching([](auto& item) { return item.alg == COSE::ES256; }) == notFound)
        return false;
    return true;
}

bool isConvertibleToU2fSignCommand(const PublicKeyCredentialRequestOptions& request)
{
    return (request.userVerification != UserVerificationRequirement::Required) && !request.allowCredentials.isEmpty();
}

Optional<Vector<uint8_t>> convertToU2fRegisterCommand(const Vector<uint8_t>& clientDataHash, const PublicKeyCredentialCreationOptions& request)
{
    if (!isConvertibleToU2fRegisterCommand(request))
        return WTF::nullopt;

    return constructU2fRegisterCommand(produceRpIdHash(request.rp.id), clientDataHash);
}

Optional<Vector<uint8_t>> convertToU2fCheckOnlySignCommand(const Vector<uint8_t>& clientDataHash, const PublicKeyCredentialCreationOptions& request, const PublicKeyCredentialDescriptor& keyHandle)
{
    if (keyHandle.type != PublicKeyCredentialType::PublicKey)
        return WTF::nullopt;

    return constructU2fSignCommand(produceRpIdHash(request.rp.id), clientDataHash, keyHandle.idVector, true /* checkOnly */);
}

Optional<Vector<uint8_t>> convertToU2fSignCommand(const Vector<uint8_t>& clientDataHash, const PublicKeyCredentialRequestOptions& request, const Vector<uint8_t>& keyHandle, bool isAppId)
{
    if (!isConvertibleToU2fSignCommand(request))
        return WTF::nullopt;

    if (!isAppId)
        return constructU2fSignCommand(produceRpIdHash(request.rpId), clientDataHash, keyHandle, false);
    ASSERT(request.extensions && !request.extensions->appid.isNull());
    return constructU2fSignCommand(produceRpIdHash(request.extensions->appid), clientDataHash, keyHandle, false);
}

Vector<uint8_t> constructBogusU2fRegistrationCommand()
{
    return constructU2fRegisterCommand(convertBytesToVector(kBogusAppParam, sizeof(kBogusAppParam)), convertBytesToVector(kBogusChallenge, sizeof(kBogusChallenge)));
}

} // namespace fido

#endif // ENABLE(WEB_AUTHN)
