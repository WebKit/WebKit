/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

namespace COSE {

// See RFC 8152 - CBOR Object Signing and Encryption <https://tools.ietf.org/html/rfc8152>
// Labels
const int64_t alg = 3;
const int64_t crv = -1;
const int64_t kty = 1;
const int64_t x = -2;
const int64_t y = -3;

// Values
const int64_t EC2 = 2;
const int64_t ES256 = -7;
const int64_t P_256 = 1;

} // namespace COSE

namespace WebCore {

// Length of the SHA-256 hash of the RP ID asssociated with the credential:
// https://www.w3.org/TR/webauthn/#sec-authenticator-data
const size_t rpIdHashLength = 32;

// Length of the flags:
// https://www.w3.org/TR/webauthn/#sec-authenticator-data
const size_t flagsLength = 1;

// Length of the signature counter, 32-bit unsigned big-endian integer:
// https://www.w3.org/TR/webauthn/#sec-authenticator-data
const size_t signCounterLength = 4;

// Length of the AAGUID of the authenticator:
// https://www.w3.org/TR/webauthn/#sec-attested-credential-data
const size_t aaguidLength = 16;

// Length of the byte length L of Credential ID, 16-bit unsigned big-endian
// integer: https://www.w3.org/TR/webauthn/#sec-attested-credential-data
const size_t credentialIdLengthLength = 2;

// Per Section 2.3.5 of http://www.secg.org/sec1-v2.pdf
const size_t ES256FieldElementLength = 32;

// https://www.w3.org/TR/webauthn/#none-attestation
const char noneAttestationValue[] = "none";

} // namespace WebCore
