// Copyright 2018 The Chromium Authors. All rights reserved.
// Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEB_AUTHN)

#include "AttestationConveyancePreference.h"
#include "AuthenticatorGetInfoResponse.h"
#include "FidoConstants.h"
#include "PublicKeyCredentialData.h"

// Converts response from authenticators to CTAPResponse objects. If the
// response of the authenticator does not conform to format specified by the
// CTAP protocol, null optional is returned.
namespace fido {

// Parses response code from response received from the authenticator. If
// unknown response code value is received, then CTAP2_ERR_OTHER is returned.
WEBCORE_EXPORT CtapDeviceResponseCode getResponseCode(const Vector<uint8_t>&);

// De-serializes CBOR encoded response, checks for valid CBOR map formatting,
// and converts response to AuthenticatorMakeCredentialResponse object with
// CBOR map keys that conform to format of attestation object defined by the
// WebAuthN spec : https://w3c.github.io/webauthn/#fig-attStructs
WEBCORE_EXPORT Optional<WebCore::PublicKeyCredentialData> readCTAPMakeCredentialResponse(const Vector<uint8_t>&, const WebCore::AttestationConveyancePreference& attestation = WebCore::AttestationConveyancePreference::Direct);

// De-serializes CBOR encoded response to AuthenticatorGetAssertion /
// AuthenticatorGetNextAssertion request to AuthenticatorGetAssertionResponse
// object.
// FIXME(190783): Probably need to remake AuthenticatorResponse to include more fields like numberOfCredentials,
// and use it here instead of PublicKeyCredentialData.
WEBCORE_EXPORT Optional<WebCore::PublicKeyCredentialData> readCTAPGetAssertionResponse(const Vector<uint8_t>&);

// De-serializes CBOR encoded response to AuthenticatorGetInfo request to
// AuthenticatorGetInfoResponse object.
WEBCORE_EXPORT Optional<AuthenticatorGetInfoResponse> readCTAPGetInfoResponse(const Vector<uint8_t>&);

} // namespace fido

#endif // ENABLE(WEB_AUTHN)
