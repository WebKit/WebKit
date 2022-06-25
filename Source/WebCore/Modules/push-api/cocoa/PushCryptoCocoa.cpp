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

#include "config.h"
#include "PushCrypto.h"

#if ENABLE(SERVICE_WORKER)

#include <CommonCrypto/CommonHMAC.h>
#include <pal/spi/cocoa/CommonCryptoSPI.h>
#include <wtf/Scope.h>

namespace WebCore::PushCrypto {

P256DHKeyPair P256DHKeyPair::generate(void)
{
    CCECCryptorRef ccPublicKey = nullptr;
    CCECCryptorRef ccPrivateKey = nullptr;
    auto releaser = WTF::makeScopeExit([&ccPublicKey, &ccPrivateKey]() {
        if (ccPublicKey)
            CCECCryptorRelease(ccPublicKey);
        if (ccPrivateKey)
            CCECCryptorRelease(ccPrivateKey);
    });

    CCCryptorStatus status = CCECCryptorGeneratePair(256, &ccPublicKey, &ccPrivateKey);
    RELEASE_ASSERT(status == kCCSuccess);

    uint8_t publicKey[p256dhPublicKeyLength];
    size_t publicKeyLength = sizeof(publicKey);
    status = CCECCryptorExportKey(kCCImportKeyBinary, publicKey, &publicKeyLength, ccECKeyPublic, ccPublicKey);
    RELEASE_ASSERT(status == kCCSuccess && publicKeyLength == sizeof(publicKey));

    // CommonCrypto expects the binary format to be 65 byte public key followed by the 32 byte private key, so we want to extract the last 32 bytes from the buffer.
    uint8_t key[p256dhPublicKeyLength + p256dhPrivateKeyLength];
    size_t keyLength = sizeof(key);
    status = CCECCryptorExportKey(kCCImportKeyBinary, key, &keyLength, ccECKeyPrivate, ccPrivateKey);
    RELEASE_ASSERT(status == kCCSuccess && keyLength == sizeof(key));

    return P256DHKeyPair {
        Vector<uint8_t> { publicKey, p256dhPublicKeyLength },
        Vector<uint8_t> { key + p256dhPublicKeyLength, p256dhPrivateKeyLength }
    };
}

bool validateP256DHPublicKey(Span<const uint8_t> publicKey)
{
    CCECCryptorRef ccPublicKey = nullptr;
    CCCryptorStatus status = CCECCryptorImportKey(kCCImportKeyBinary, publicKey.data(), publicKey.size(), ccECKeyPublic, &ccPublicKey);
    if (!ccPublicKey)
        return false;
    CCECCryptorRelease(ccPublicKey);
    return status == kCCSuccess;
}

std::optional<Vector<uint8_t>> computeP256DHSharedSecret(Span<const uint8_t> publicKey, const P256DHKeyPair& keyPair)
{
    if (publicKey.size() != p256dhPublicKeyLength || keyPair.publicKey.size() != p256dhPublicKeyLength || keyPair.privateKey.size() != p256dhPrivateKeyLength)
        return std::nullopt;

    CCECCryptorRef ccPublicKey = nullptr;
    CCECCryptorRef ccPrivateKey = nullptr;
    auto releaser = WTF::makeScopeExit([&ccPublicKey, &ccPrivateKey]() {
        if (ccPublicKey)
            CCECCryptorRelease(ccPublicKey);
        if (ccPrivateKey)
            CCECCryptorRelease(ccPrivateKey);
    });

    if (CCECCryptorImportKey(kCCImportKeyBinary, publicKey.data(), p256dhPublicKeyLength, ccECKeyPublic, &ccPublicKey) != kCCSuccess)
        return std::nullopt;

    // CommonCrypto expects the binary format to be 65 byte public key followed by the 32 byte private key.
    uint8_t keyBuf[p256dhPublicKeyLength + p256dhPrivateKeyLength];
    memcpy(keyBuf, keyPair.publicKey.data(), p256dhPublicKeyLength);
    memcpy(keyBuf + p256dhPublicKeyLength, keyPair.privateKey.data(), p256dhPrivateKeyLength);
    if (CCECCryptorImportKey(kCCImportKeyBinary, keyBuf, sizeof(keyBuf), ccECKeyPrivate, &ccPrivateKey) != kCCSuccess)
        return std::nullopt;

    Vector<uint8_t> sharedSecret(p256dhSharedSecretLength);
    size_t sharedSecretLength = sharedSecret.size();
    if (CCECCryptorComputeSharedSecret(ccPrivateKey, ccPublicKey, sharedSecret.begin(), &sharedSecretLength) != kCCSuccess || sharedSecretLength != p256dhSharedSecretLength)
        return std::nullopt;

    return sharedSecret;
}

Vector<uint8_t> hmacSHA256(Span<const uint8_t> key, Span<const uint8_t> message)
{
    Vector<uint8_t> result(sha256DigestLength);
    CCHmac(kCCHmacAlgSHA256, key.data(), key.size(), message.data(), message.size(), result.begin());
    return result;
}

std::optional<Vector<uint8_t>> decryptAES128GCM(Span<const uint8_t> key, Span<const uint8_t> iv, Span<const uint8_t> cipherTextWithTag)
{
    if (cipherTextWithTag.size() < aes128GCMTagLength)
        return std::nullopt;

    Vector<uint8_t> plainText(cipherTextWithTag.size() - aes128GCMTagLength);
    auto result = CCCryptorGCMOneshotDecrypt(kCCAlgorithmAES, key.data(), key.size(), iv.data(), iv.size(), nullptr /* additionalData */, 0 /* additionalDataLength */, cipherTextWithTag.data(), cipherTextWithTag.size() - aes128GCMTagLength, plainText.data(), cipherTextWithTag.end() - aes128GCMTagLength, aes128GCMTagLength);
    if (result != kCCSuccess)
        return std::nullopt;

    return plainText;
}

} // namespace WebCore::PushCrypto

#endif // ENABLE(SERVICE_WORKER)
