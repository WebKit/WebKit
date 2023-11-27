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
#include "PushMessageCrypto.h"

#include "PushCrypto.h"
#include <wtf/ByteOrder.h>
#include <wtf/CryptographicallyRandomNumber.h>

namespace WebCore::PushCrypto {

// Arbitrary limit that's larger than the largest payload APNS should ever give us.
static constexpr size_t maxPushPayloadLength = 65535;

// From RFC8291.
static constexpr size_t saltLength = 16;
static constexpr size_t sharedAuthSecretLength = 16;

ClientKeys ClientKeys::generate()
{
    uint8_t sharedAuthSecret[sharedAuthSecretLength];
    cryptographicallyRandomValues(sharedAuthSecret, sizeof(sharedAuthSecret));

    return ClientKeys {
        P256DHKeyPair::generate(),
        Vector<uint8_t> { sharedAuthSecret, sizeof(sharedAuthSecret) }
    };
}

static bool areClientKeyLengthsValid(const ClientKeys& clientKeys)
{
    return clientKeys.clientP256DHKeyPair.publicKey.size() == p256dhPublicKeyLength && clientKeys.clientP256DHKeyPair.privateKey.size() == p256dhPrivateKeyLength && clientKeys.sharedAuthSecret.size() == sharedAuthSecretLength;
}

static size_t computeAES128GCMPaddingLength(const uint8_t *begin, size_t length)
{
    /*
     * Compute padding length as defined in RFC8188 Section 2:
     *
     *   +-----------+-----+
     *   |   data    | pad |
     *   +-----------+-----+
     *
     * pad must be of non-zero length and is a delimiter octet (0x02) followed by any number of 0x00 octets.
     */
    if (!length)
        return SIZE_MAX;

    const uint8_t* end = begin + length;
    const uint8_t* cur = end - 1;
    while (cur > begin && (*cur == 0x00))
        --cur;
    if (*cur != 0x02)
        return SIZE_MAX;

    return end - cur;
}

std::optional<Vector<uint8_t>> decryptAES128GCMPayload(const ClientKeys& clientKeys, std::span<const uint8_t> payload)
{
    if (!areClientKeyLengthsValid(clientKeys))
        return std::nullopt;

    // Extract encryption parameters from header as described in RFC8188.
    struct PayloadHeader {
        uint8_t salt[saltLength];
        uint8_t ignored[4];
        uint8_t keyLength;
        uint8_t serverPublicKey[p256dhPublicKeyLength];
    };
    static_assert(sizeof(PayloadHeader) == 86);
    static constexpr size_t minPushPayloadLength = sizeof(PayloadHeader) + 1 /* minPaddingLength */ + aes128GCMTagLength;

    if (payload.size() < minPushPayloadLength || payload.size() > maxPushPayloadLength)
        return std::nullopt;

    PayloadHeader header;
    memcpy(&header, payload.data(), sizeof(header));

    if (header.keyLength != p256dhPublicKeyLength)
        return std::nullopt;

    /*
     * The rest of the comments are snippets from RFC8291 3.4.
     *
     * -- For a user agent:
     * ecdh_secret = ECDH(ua_private, as_public)
     */
    auto ecdhSecretResult = computeP256DHSharedSecret(header.serverPublicKey, clientKeys.clientP256DHKeyPair);
    if (!ecdhSecretResult)
        return std::nullopt;

    /*
     * # HKDF-Extract(salt=auth_secret, IKM=ecdh_secret)
     * PRK_key = HMAC-SHA-256(auth_secret, ecdh_secret)
     */
    auto prkKey = hmacSHA256(clientKeys.sharedAuthSecret, *ecdhSecretResult);

    /*
     * # HKDF-Expand(PRK_key, key_info, L_key=32)
     * key_info = "WebPush: info" || 0x00 || ua_public || as_public
     * IKM = HMAC-SHA-256(PRK_key, key_info || 0x01)
     */
    struct KeyInfo {
        char label[14] = { "WebPush: info" };
        uint8_t clientKey[p256dhPublicKeyLength];
        uint8_t serverKey[p256dhPublicKeyLength];
        uint8_t end = 0x01;
    };
    static_assert(sizeof(KeyInfo) == 145);

    KeyInfo keyInfo;
    memcpy(keyInfo.clientKey, clientKeys.clientP256DHKeyPair.publicKey.data(), p256dhPublicKeyLength);
    memcpy(keyInfo.serverKey, header.serverPublicKey, p256dhPublicKeyLength);

    auto ikm = hmacSHA256(prkKey, std::span(reinterpret_cast<uint8_t*>(&keyInfo), sizeof(keyInfo)));

    /*
     * # HKDF-Extract(salt, IKM)
     * PRK = HMAC-SHA-256(salt, IKM)
     */
    auto prk = hmacSHA256(header.salt, ikm);

    /*
     * # HKDF-Expand(PRK, cek_info, L_cek=16)
     * cek_info = "Content-Encoding: aes128gcm" || 0x00
     * CEK = HMAC-SHA-256(PRK, cek_info || 0x01)[0..15]
     */
    static const uint8_t cekInfo[] = "Content-Encoding: aes128gcm\x00\x01";
    auto cek = hmacSHA256(prk, std::span(cekInfo, sizeof(cekInfo) - 1));
    cek.shrink(16);

    /*
     * # HKDF-Expand(PRK, nonce_info, L_nonce=12)
     * nonce_info = "Content-Encoding: nonce" || 0x00
     * NONCE = HMAC-SHA-256(PRK, nonce_info || 0x01)[0..11]
     */
    static const uint8_t nonceInfo[] = "Content-Encoding: nonce\x00\x01";
    auto nonce = hmacSHA256(prk, std::span(nonceInfo, sizeof(nonceInfo) - 1));
    nonce.shrink(12);

    // Finally, decrypt with AES128GCM and return the unpadded plaintext.
    auto cipherText = std::span(payload.data() + sizeof(header), payload.size() - sizeof(header));
    auto plainTextResult = decryptAES128GCM(cek, nonce, cipherText);
    if (!plainTextResult)
        return std::nullopt;

    auto plainText = WTFMove(plainTextResult.value());
    size_t paddingLength = computeAES128GCMPaddingLength(plainText.data(), plainText.size());
    if (paddingLength == SIZE_MAX)
        return std::nullopt;

    plainText.shrink(plainText.size() - paddingLength);
    return plainText;
}

static size_t computeAESGCMPaddingLength(const uint8_t *begin, size_t length)
{
    /*
     * Compute padding length as defined in draft-ietf-httpbis-encryption-encoding-03:
     *
     *   +-----+-----------+
     *   | pad |   data    |
     *   +-----+-----------+
     *
     * Padding consists of a two octet unsigned integer in network byte order, followed by that
     * number of 0x00 octets. The minimum padding size is 2 bytes.
     */
    if (length < 2)
        return SIZE_MAX;

    uint16_t paddingLength;
    memcpy(&paddingLength, begin, 2);
    paddingLength = ntohs(paddingLength);

    const uint8_t* cur = begin + 2;
    const uint8_t* end = begin + length;
    uint16_t paddingLeft = paddingLength;
    while (cur < end && (*cur == 0x0) && paddingLeft) {
        ++cur;
        --paddingLeft;
    }

    if (paddingLeft)
        return SIZE_MAX;

    return cur - begin;
}

std::optional<Vector<uint8_t>> decryptAESGCMPayload(const ClientKeys& clientKeys, std::span<const uint8_t> serverP256DHPublicKey, std::span<const uint8_t> salt, std::span<const uint8_t> payload)
{
    if (!areClientKeyLengthsValid(clientKeys) || serverP256DHPublicKey.size() != p256dhPublicKeyLength || salt.size() != saltLength)
        return std::nullopt;

    // Padding must be at least the size of the two octet unsigned integer used in the padding scheme plus the size of the AES128GCM tag.
    if (payload.size() < 2 + aes128GCMTagLength || payload.size() > maxPushPayloadLength)
        return std::nullopt;

    /*
     * These comments are snippets from draft-ietf-webpush-encryption-04.
     *
     * -- For a User Agent:
     * ecdh_secret = ECDH(ua_private, as_public)
     */
    auto ecdhSecretResult = computeP256DHSharedSecret(serverP256DHPublicKey, clientKeys.clientP256DHKeyPair);
    if (!ecdhSecretResult)
        return std::nullopt;

    /*
     * auth_info = "Content-Encoding: auth" || 0x00
     * PRK_combine = HMAC-SHA-256(auth_secret, ecdh_secret)
     * IKM = HMAC-SHA-256(PRK_combine, auth_info || 0x01)
     * PRK = HMAC-SHA-256(salt, IKM)
     */
    static const uint8_t authInfo[] = "Content-Encoding: auth\x00\x01";
    auto prkCombine = hmacSHA256(clientKeys.sharedAuthSecret, *ecdhSecretResult);
    auto ikm = hmacSHA256(prkCombine, std::span(authInfo, sizeof(authInfo) - 1));
    auto prk = hmacSHA256(salt, ikm);

    /*
     * context = "P-256" || 0x00 ||
     *           0x00 || 0x41 || ua_public ||
     *           0x00 || 0x41 || as_public
     *
     * Note that we also append a 0x01 byte at the end here since the cek and nonce
     * derivation functions below require that trailing 0x01 byte.
     */
    struct KeyDerivationContext {
        char label[6] = { "P-256" };
        uint8_t clientPublicKeyLength[2] = { 0, 0x41 };
        uint8_t clientPublicKey[p256dhPublicKeyLength];
        uint8_t serverPublicKeyLength[2] = { 0, 0x41 };
        uint8_t serverPublicKey[p256dhPublicKeyLength];
        uint8_t end = 0x01;
    };
    static_assert(sizeof(KeyDerivationContext) == 141);
    KeyDerivationContext context;
    memcpy(context.clientPublicKey, clientKeys.clientP256DHKeyPair.publicKey.data(), p256dhPublicKeyLength);
    memcpy(context.serverPublicKey, serverP256DHPublicKey.data(), p256dhPublicKeyLength);

    /*
     * cek_info = "Content-Encoding: aesgcm" || 0x00 || context
     * CEK = HMAC-SHA-256(PRK, cek_info || 0x01)[0..15]
     */
    static const uint8_t cekInfoHeader[] = "Content-Encoding: aesgcm";
    uint8_t cekInfo[sizeof(cekInfoHeader) + sizeof(context)];
    memcpy(cekInfo, cekInfoHeader, sizeof(cekInfoHeader));
    memcpy(cekInfo + sizeof(cekInfoHeader), &context, sizeof(context));

    auto cek = hmacSHA256(prk, cekInfo);
    cek.shrink(16);

    /*
     * nonce_info = "Content-Encoding: nonce" || 0x00 || context
     * NONCE = HMAC-SHA-256(PRK, nonce_info || 0x01)[0..11]
     */
    static const uint8_t nonceInfoHeader[] = "Content-Encoding: nonce";
    uint8_t nonceInfo[sizeof(nonceInfoHeader) + sizeof(context)];
    memcpy(nonceInfo, nonceInfoHeader, sizeof(nonceInfoHeader));
    memcpy(nonceInfo + sizeof(nonceInfoHeader), &context, sizeof(context));

    auto nonce = hmacSHA256(prk, nonceInfo);
    nonce.shrink(12);

    // Finally, decrypt with AES128GCM and return the unpadded plaintext.
    auto plainTextResult = decryptAES128GCM(cek, nonce, payload);
    if (!plainTextResult)
        return std::nullopt;

    auto plainText = WTFMove(plainTextResult.value());
    size_t paddingLength = computeAESGCMPaddingLength(plainText.data(), plainText.size());
    if (paddingLength == SIZE_MAX)
        return std::nullopt;

    return Vector<uint8_t> { plainText.data() + paddingLength, plainText.size() - paddingLength };
}

} // namespace WebCore::PushCrypto
