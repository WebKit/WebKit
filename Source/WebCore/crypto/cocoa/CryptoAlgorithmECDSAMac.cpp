/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "CryptoAlgorithmECDSA.h"

#include "CommonCryptoDERUtilities.h"
#include "CommonCryptoUtilities.h"
#include "CryptoAlgorithmEcdsaParams.h"
#include "CryptoDigestAlgorithm.h"
#include "CryptoKeyEC.h"
#if HAVE(SWIFT_CPP_INTEROP)
#include <pal/PALSwiftUtils.h>
#endif

namespace WebCore {
#if HAVE(SWIFT_CPP_INTEROP)

static ExceptionOr<Vector<uint8_t>> signECDSACryptoKit(CryptoAlgorithmIdentifier hash, const PlatformECKeyContainer& key, const Vector<uint8_t>& data)
{
    if (!isValidHashParameter(hash))
        return Exception { ExceptionCode::OperationError };
    auto rv = key->sign(data.span(), toCKHashFunction(hash));
    if (rv.errorCode != Cpp::ErrorCodes::Success)
        return Exception { ExceptionCode::OperationError };
    return WTFMove(rv.result);
}

static ExceptionOr<bool> verifyECDSACryptoKit(CryptoAlgorithmIdentifier hash, const PlatformECKeyContainer& key, const Vector<uint8_t>& signature, const Vector<uint8_t> data)
{
    if (!isValidHashParameter(hash))
        return Exception { ExceptionCode::OperationError };
    return key->verify(data.span(), signature.span(), toCKHashFunction(hash)).errorCode == Cpp::ErrorCodes::Success;
}
#else

static ExceptionOr<Vector<uint8_t>> signECDSA(CryptoAlgorithmIdentifier hash, const PlatformECKeyContainer& key, size_t keyLengthInBytes, const Vector<uint8_t>& data)
{
    CCDigestAlgorithm digestAlgorithm;
    if (!getCommonCryptoDigestAlgorithm(hash, digestAlgorithm))
        return Exception { ExceptionCode::OperationError };

    auto cryptoDigestAlgorithm = WebCore::cryptoDigestAlgorithm(hash);
    if (!cryptoDigestAlgorithm)
        return Exception { ExceptionCode::OperationError };
    auto digest = PAL::CryptoDigest::create(*cryptoDigestAlgorithm);
    if (!digest)
        return Exception { ExceptionCode::OperationError };
    digest->addBytes(data.span());
    auto digestData = digest->computeHash();

    // The signature produced by CCECCryptorSignHash is in DER format.
    // tag + length(1) + tag + length(1) + InitialOctet(?) + keyLength in bytes + tag + length(1) + InitialOctet(?) + keyLength in bytes
    Vector<uint8_t> signature(8 + keyLengthInBytes * 2);
    size_t signatureSize = signature.size();
    CCCryptorStatus status = CCECCryptorSignHash(key.get(), digestData.data(), digestData.size(), signature.data(), &signatureSize);
    if (status)
        return Exception { ExceptionCode::OperationError };

    // FIXME: <rdar://problem/31618371>
    // convert the DER binary into r + s
    Vector<uint8_t> newSignature;
    newSignature.reserveInitialCapacity(keyLengthInBytes * 2);
    size_t offset = 1; // skip tag
    offset += bytesUsedToEncodedLength(signature[offset]); // skip length
    ++offset; // skip tag

    // If r < keyLengthInBytes, fill the head of r with 0s.
    size_t bytesToCopy = keyLengthInBytes;
    if (signature[offset] < keyLengthInBytes) {
        newSignature.grow(keyLengthInBytes - signature[offset]);
        memset(newSignature.data(), InitialOctet, keyLengthInBytes - signature[offset]);
        bytesToCopy = signature[offset];
    } else if (signature[offset] > keyLengthInBytes) // Otherwise skip the leading 0s of r.
        offset += signature[offset] - keyLengthInBytes;
    offset++; // skip length
    ASSERT_WITH_SECURITY_IMPLICATION(signature.size() > offset + bytesToCopy);
    newSignature.append(signature.subspan(offset, bytesToCopy));
    offset += bytesToCopy + 1; // skip r, tag

    // If s < keyLengthInBytes, fill the head of s with 0s.
    bytesToCopy = keyLengthInBytes;
    if (signature[offset] < keyLengthInBytes) {
        size_t pos = newSignature.size();
        newSignature.resize(pos + keyLengthInBytes - signature[offset]);
        memset(newSignature.data() + pos, InitialOctet, keyLengthInBytes - signature[offset]);
        bytesToCopy = signature[offset];
    } else if (signature[offset] > keyLengthInBytes) // Otherwise skip the leading 0s of s.
        offset += signature[offset] - keyLengthInBytes;
    ++offset; // skip length
    ASSERT_WITH_SECURITY_IMPLICATION(signature.size() >= offset + bytesToCopy);
    newSignature.append(signature.subspan(offset, bytesToCopy));

    return WTFMove(newSignature);
}

static ExceptionOr<bool> verifyECDSA(CryptoAlgorithmIdentifier hash, const PlatformECKeyContainer& key, size_t keyLengthInBytes, const Vector<uint8_t>& signature, const Vector<uint8_t> data)
{
    CCDigestAlgorithm digestAlgorithm;
    if (!getCommonCryptoDigestAlgorithm(hash, digestAlgorithm))
        return Exception { ExceptionCode::OperationError };

    auto cryptoDigestAlgorithm = WebCore::cryptoDigestAlgorithm(hash);
    if (!cryptoDigestAlgorithm)
        return Exception { ExceptionCode::OperationError };
    auto digest = PAL::CryptoDigest::create(*cryptoDigestAlgorithm);
    if (!digest)
        return Exception { ExceptionCode::OperationError };
    digest->addBytes(data.span());
    auto digestData = digest->computeHash();

    if (signature.size() != keyLengthInBytes * 2)
        return false;

    // FIXME: <rdar://problem/31618371>
    // Convert the signature into DER format.
    // tag + length(1) + tag + length(1) + InitialOctet(?) + r + tag + length(1) + InitialOctet(?) + s
    // Skip any heading 0s of r and s.
    size_t rStart = 0;
    while (rStart < keyLengthInBytes && !signature[rStart])
        rStart++;
    size_t sStart = keyLengthInBytes;
    while (sStart < signature.size() && !signature[sStart])
        sStart++;
    if (rStart >= keyLengthInBytes || sStart >= signature.size())
        return false;

    // InitialOctet is needed when the first byte of r/s is larger than or equal to 128.
    bool rNeedsInitialOctet = signature[rStart] >= 128;
    bool sNeedsInitialOctet = signature[sStart] >= 128;

    // Construct the DER signature.
    Vector<uint8_t> newSignature;
    newSignature.reserveInitialCapacity(6 + keyLengthInBytes * 3  + rNeedsInitialOctet + sNeedsInitialOctet - rStart - sStart);
    newSignature.append(SequenceMark);
    addEncodedASN1Length(newSignature, 4 + keyLengthInBytes * 3  + rNeedsInitialOctet + sNeedsInitialOctet - rStart - sStart);
    newSignature.append(IntegerMark);
    addEncodedASN1Length(newSignature, keyLengthInBytes + rNeedsInitialOctet - rStart);
    if (rNeedsInitialOctet)
        newSignature.append(InitialOctet);
    newSignature.append(signature.subspan(rStart, keyLengthInBytes - rStart));
    newSignature.append(IntegerMark);
    addEncodedASN1Length(newSignature, keyLengthInBytes * 2 + sNeedsInitialOctet - sStart);
    if (sNeedsInitialOctet)
        newSignature.append(InitialOctet);
    newSignature.append(signature.subspan(sStart, keyLengthInBytes * 2 - sStart));

    uint32_t valid;
    CCCryptorStatus status = CCECCryptorVerifyHash(key.get(), digestData.data(), digestData.size(), newSignature.data(), newSignature.size(), &valid);
    if (status) {
        WTFLogAlways("ERROR: CCECCryptorVerifyHash() returns error=%d", status);
        return false;
    }
    return valid;
}
#endif

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmECDSA::platformSign(const CryptoAlgorithmEcdsaParams& parameters, const CryptoKeyEC& key, const Vector<uint8_t>& data)
{
#if HAVE(SWIFT_CPP_INTEROP)
    return signECDSACryptoKit(parameters.hashIdentifier, key.platformKey(), data);
#else
    return signECDSA(parameters.hashIdentifier, key.platformKey(), key.keySizeInBytes(), data);
#endif
}

ExceptionOr<bool> CryptoAlgorithmECDSA::platformVerify(const CryptoAlgorithmEcdsaParams& parameters, const CryptoKeyEC& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
#if HAVE(SWIFT_CPP_INTEROP)
    return verifyECDSACryptoKit(parameters.hashIdentifier, key.platformKey(), signature, data);
#else
    return verifyECDSA(parameters.hashIdentifier, key.platformKey(), key.keySizeInBytes(), signature, data);
#endif
}

} // namespace WebCore
