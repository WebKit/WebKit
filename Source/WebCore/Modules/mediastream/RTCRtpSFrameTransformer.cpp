/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RTCRtpSFrameTransformer.h"

#if ENABLE(WEB_RTC)

#include "SFrameUtils.h"

namespace WebCore {

static constexpr unsigned AES_CM_128_HMAC_SHA256_NONCE_SIZE = 12;

static inline void writeUInt64(uint8_t* data, uint64_t value, uint8_t valueLength)
{
    for (unsigned i = 0; i < valueLength; ++i)
        *data++ = (value >> ((valueLength - 1 - i) * 8)) & 0xff;
}

static inline uint64_t readUInt64(const uint8_t* data, size_t size)
{
    uint64_t value = 0;
    while (size--)
        value = (value << 8) | *data++;
    return value;
}

static inline uint8_t lengthOfUInt64(uint64_t value)
{
    uint8_t length = 0;
    do {
        ++length;
        value = value >> 8;
    } while (value);
    return length;
}

static inline uint8_t computeFirstHeaderByte(uint64_t keyId, uint64_t counter)
{
    uint8_t value = 0;
    value |= (lengthOfUInt64(counter) - 1) << 4;
    if (keyId < 8)
        value |= keyId;
    else {
        value |= (lengthOfUInt64(keyId) - 1);
        value |= 1 << 3;
    }
    return value;
}

static inline Vector<uint8_t> computeIV(uint64_t counter, const Vector<uint8_t>& saltKey)
{
    // The saltKey is 12 bytes (AES_CM_128_HMAC_SHA256_NONCE_SIZE), we XOR with the counter as 12 bytes.
    // We then extend it to 16 bytes since that is what is expected by the crypto routines.
    Vector<uint8_t> iv(16);
    for (unsigned i = 0; i < 4; ++i)
        iv[i] = saltKey[i];
    for (unsigned i = 11; i >= 4; --i) {
        auto value = counter & 0xff;
        counter = counter >> 8;
        iv[i] = value ^ saltKey[i];
    }
    for (unsigned i = 12; i < 16; ++i)
        iv[i] = 0;
    return iv;
}

static inline bool hasSignature(uint8_t firstByte)
{
    return firstByte & 0x80;
}

static inline bool hasLongKeyLength(uint8_t firstByte)
{
    return firstByte & 0x08;
}

struct SFrameHeaderInfo {
    uint8_t size;
    uint64_t keyId;
    uint64_t counter;
};
static inline Optional<SFrameHeaderInfo> parseSFrameHeader(const uint8_t* data, size_t size)
{
    auto* start = data;

    uint64_t keyId = 0;
    uint64_t counter = 0;

    auto firstByte = *data++;

    // Signature bit.
    if (hasSignature(firstByte))
        return { };

    size_t counterLength = ((firstByte >> 4) & 0x07) + 1;

    if (size < counterLength + 1)
        return { };

    if (hasLongKeyLength(firstByte)) {
        size_t keyLength = (firstByte & 0x07) + 1;
        if (size < counterLength + keyLength + 1)
            return { };

        keyId = readUInt64(data, keyLength);
        data += keyLength;

        counter = readUInt64(data, counterLength);
        data += counterLength;
    } else {
        keyId = firstByte & 0x07;
        counter = readUInt64(data, counterLength);
        data += counterLength;
    }
    uint8_t headerSize = data - start;
    return SFrameHeaderInfo { headerSize, keyId, counter };
}

Ref<RTCRtpSFrameTransformer> RTCRtpSFrameTransformer::create(CompatibilityMode mode)
{
    return adoptRef(*new RTCRtpSFrameTransformer(mode));
}

RTCRtpSFrameTransformer::RTCRtpSFrameTransformer(CompatibilityMode mode)
    : m_compatibilityMode(mode)
{
}

RTCRtpSFrameTransformer::~RTCRtpSFrameTransformer()
{
}

ExceptionOr<void> RTCRtpSFrameTransformer::setEncryptionKey(const Vector<uint8_t>& rawKey, Optional<uint64_t> keyId)
{
    auto locker = holdLock(m_keyLock);
    return updateEncryptionKey(rawKey, keyId, ShouldUpdateKeys::Yes);
}

ExceptionOr<void> RTCRtpSFrameTransformer::updateEncryptionKey(const Vector<uint8_t>& rawKey, Optional<uint64_t> keyId, ShouldUpdateKeys shouldUpdateKeys)
{
    ASSERT(m_keyLock.isLocked());

    auto saltKeyResult = computeSaltKey(rawKey);
    if (saltKeyResult.hasException())
        return saltKeyResult.releaseException();

    ASSERT(saltKeyResult.returnValue().size() >= AES_CM_128_HMAC_SHA256_NONCE_SIZE);

    auto authenticationKeyResult = computeAuthenticationKey(rawKey);
    if (authenticationKeyResult.hasException())
        return authenticationKeyResult.releaseException();

    auto encryptionKeyResult = computeEncryptionKey(rawKey);
    if (encryptionKeyResult.hasException())
        return encryptionKeyResult.releaseException();

    if (shouldUpdateKeys == ShouldUpdateKeys::No)
        m_keyId = *keyId;
    else {
        // FIXME: In case keyId is not set, it might be best to use the first non used ID.
        if (!keyId)
            keyId = m_keys.size();

        m_keyId = *keyId;
        m_keys.append({ m_keyId, rawKey });
    }

    m_saltKey = saltKeyResult.releaseReturnValue();
    m_authenticationKey = authenticationKeyResult.releaseReturnValue();
    m_encryptionKey = encryptionKeyResult.releaseReturnValue();

    updateAuthenticationSize();
    m_hasKey = true;

    return { };
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::decryptFrame(const uint8_t* frameData, size_t frameSize)
{
    Vector<uint8_t> buffer;
    switch (m_compatibilityMode) {
    case CompatibilityMode::H264: {
        auto offset = computeH264PrefixOffset(frameData, frameSize);
        frameData += offset;
        frameSize -= offset;
        if (needsRbspUnescaping(frameData, frameSize)) {
            buffer = fromRbsp(frameData, frameSize);
            frameData = buffer.data();
            frameSize = buffer.size();
        }
        break;
    }
    case CompatibilityMode::VP8: {
        auto offset = computeVP8PrefixOffset(frameData, frameSize);
        frameData += offset;
        frameSize -= offset;
        break;
    }
    case CompatibilityMode::None:
        break;
    }

    auto locker = holdLock(m_keyLock);

    auto header = parseSFrameHeader(frameData, frameSize);

    if (!header)
        return Exception { NotSupportedError };

    if (header->counter <= m_counter && m_counter)
        return Exception { InvalidStateError };
    m_counter = header->counter;

    if (header->keyId != m_keyId) {
        auto position = m_keys.findMatching([keyId = header->keyId](auto& item) { return item.keyId == keyId; });
        if (position == notFound)
            return Exception { DataError, "Key ID is unknown" };
        auto result = updateEncryptionKey(m_keys[position].keyData, header->keyId, ShouldUpdateKeys::No);
        if (result.hasException())
            return result.releaseException();
    }

    if (frameSize < (header->size + m_authenticationSize))
        return Exception { DataError, "Chunk is too small for authentication size" };

    auto iv = computeIV(m_counter, m_saltKey);

    // Compute signature
    auto* transmittedSignature = frameData + frameSize - m_authenticationSize;
    auto signature = computeEncryptedDataSignature(iv, frameData, header->size, frameData + header->size, frameSize  - m_authenticationSize - header->size, m_authenticationKey);
    for (size_t cptr = 0; cptr < m_authenticationSize; ++cptr) {
        if (signature[cptr] != transmittedSignature[cptr]) {
            // FIXME: We should try ratcheting.
            return Exception { NotSupportedError };
        }
    }

    // Decrypt data
    auto dataSize = frameSize - header->size - m_authenticationSize;
    auto result = decryptData(frameData + header->size, dataSize, iv, m_encryptionKey);

    if (result.hasException())
        return result.releaseException();

    return result.releaseReturnValue();
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::encryptFrame(const uint8_t* frameData, size_t frameSize)
{
    static const unsigned MaxHeaderSize = 17;

    Vector<uint8_t> transformedData;
    SFrameCompatibilityPrefixBuffer prefixBuffer;
    switch (m_compatibilityMode) {
    case CompatibilityMode::H264:
        prefixBuffer = computeH264PrefixBuffer(frameData, frameSize);
        break;
    case CompatibilityMode::VP8:
        prefixBuffer = computeVP8PrefixBuffer(frameData, frameSize);
        break;
    case CompatibilityMode::None:
        break;
    }

    auto locker = holdLock(m_keyLock);

    auto iv = computeIV(m_counter, m_saltKey);

    transformedData.resize(prefixBuffer.size + frameSize + MaxHeaderSize + m_authenticationSize);

    if (prefixBuffer.data)
        std::memcpy(transformedData.data(), prefixBuffer.data, prefixBuffer.size);

    auto* newDataPointer = transformedData.data() + prefixBuffer.size;
    // Fill header.
    size_t headerSize = 1;
    *newDataPointer = computeFirstHeaderByte(m_keyId, m_counter);
    if (m_keyId >= 8) {
        auto keyIdLength = lengthOfUInt64(m_keyId);
        writeUInt64(newDataPointer + headerSize, m_keyId, keyIdLength);
        headerSize += keyIdLength;
    }
    auto counterLength = lengthOfUInt64(m_counter);
    writeUInt64(newDataPointer + headerSize, m_counter, counterLength);
    headerSize += counterLength;

    transformedData.resize(prefixBuffer.size + frameSize + headerSize + m_authenticationSize);

    // Fill encrypted data
    auto encryptedData = encryptData(frameData, frameSize, iv, m_encryptionKey);
    ASSERT(!encryptedData.hasException());
    if (encryptedData.hasException())
        return encryptedData.releaseException();

    std::memcpy(newDataPointer + headerSize, encryptedData.returnValue().data(), frameSize);

    // Fill signature
    auto signature = computeEncryptedDataSignature(iv, newDataPointer, headerSize, newDataPointer + headerSize, frameSize, m_authenticationKey);
    std::memcpy(newDataPointer + frameSize + headerSize, signature.data(), m_authenticationSize);

    if (m_compatibilityMode == CompatibilityMode::H264)
        toRbsp(transformedData, prefixBuffer.size);

    ++m_counter;

    return transformedData;
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::transform(const uint8_t* data, size_t size)
{
    if (!m_hasKey)
        return Exception { InvalidStateError, "Key is not initialized"_s };

    return m_isEncrypting ? encryptFrame(data, size) : decryptFrame(data, size);
}

#if !PLATFORM(COCOA)
ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::computeSaltKey(const Vector<uint8_t>&)
{
    return Exception { NotSupportedError };
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::computeAuthenticationKey(const Vector<uint8_t>&)
{
    return Exception { NotSupportedError };
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::computeEncryptionKey(const Vector<uint8_t>&)
{
    return Exception { NotSupportedError };
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::decryptData(const uint8_t*, size_t, const Vector<uint8_t>&, const Vector<uint8_t>&)
{
    return Exception { NotSupportedError };
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::encryptData(const uint8_t*, size_t, const Vector<uint8_t>&, const Vector<uint8_t>&)
{
    return Exception { NotSupportedError };
}

Vector<uint8_t> RTCRtpSFrameTransformer::computeEncryptedDataSignature(const Vector<uint8_t>&, const uint8_t*, size_t, const uint8_t*, size_t, const Vector<uint8_t>&)
{
    return { };
}

void RTCRtpSFrameTransformer::updateAuthenticationSize()
{
}
#endif // !PLATFORM(COCOA)

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
