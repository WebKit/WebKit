/*
 * Copyright (C) 2020 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CDMProxyClearKey.h"

#if ENABLE(ENCRYPTED_MEDIA)

#include "Logging.h"
#include <wtf/ByteOrder.h>

namespace WebCore {

namespace {

static bool readUInt32(const uint8_t* buffer, size_t bufferSize, size_t& offset, uint32_t& value)
{
    ASSERT_ARG(offset, offset <= bufferSize);
    if (bufferSize - offset < sizeof(value))
        return false;

    value = ntohl(*reinterpret_cast_ptr<const uint32_t*>(buffer + offset));
    offset += sizeof(value);

    return true;
}

static bool readUInt16(const uint8_t* buffer, size_t bufferSize, size_t& offset, uint16_t& value)
{
    ASSERT_ARG(offset, offset <= bufferSize);
    if (bufferSize - offset < sizeof(value))
        return false;

    value = ntohs(*reinterpret_cast_ptr<const uint16_t*>(buffer + offset));
    offset += sizeof(value);

    return true;
}

} // namespace {}

CDMProxyFactoryClearKey& CDMProxyFactoryClearKey::singleton()
{
    static NeverDestroyed<CDMProxyFactoryClearKey> s_factory;
    return s_factory;
}

RefPtr<CDMProxy> CDMProxyFactoryClearKey::createCDMProxy(const String& keySystem)
{
    ASSERT_UNUSED(keySystem, supportsKeySystem(keySystem));
    return adoptRef(new CDMProxyClearKey());
}

bool CDMProxyFactoryClearKey::supportsKeySystem(const String& keySystem)
{
    // `org.w3.clearkey` is the only supported key system.
    return equalLettersIgnoringASCIICase(keySystem, "org.w3.clearkey");
}

CDMProxyClearKey::~CDMProxyClearKey()
{
    closeGCryptHandle();
}

bool CDMProxyClearKey::cencSetCounterVector(const cencDecryptContext& input)
{
    uint8_t ctr[ClearKey::AES128CTRBlockSizeInBytes];
    if (input.ivSizeInBytes == 8) {
        // ISO/IEC 23001-7:2016 Section 9.3

        // When an 8-byte IV is indicated, the least significant 8
        // bytes of the 16 byte IV (bytes 8 to 15) SHALL be set to
        // zero.
        memset(ctr + 8, 0, 8);
        memcpy(ctr, input.iv, 8);
    } else
        memcpy(ctr, input.iv, ClearKey::IVSizeInBytes);

    if (gcry_error_t cipherError = gcry_cipher_setctr(gCryptHandle(), ctr, ClearKey::IVSizeInBytes)) {
#if !LOG_DISABLED
        LOG(EME, "EME - CDMProxyClearKey - ERROR(gcry_cipher_setctr): %s", gpg_strerror(cipherError));
#else
        UNUSED_VARIABLE(cipherError);
#endif
        return false;
    }

    return true;
}

bool CDMProxyClearKey::cencSetDecryptionKey(cencDecryptContext& in)
{
    // FIXME: Unnecessary copy, can we avoid this while still exposing
    // a non-GStreamer-specific DecryptInput API? These buffers are
    // small (16 bytes), so not a huge deal, I guess.
    Vector<uint8_t> keyIDVec { in.keyID, in.keyIDSizeInBytes };

    auto keyData = getOrWaitForKeyValue(keyIDVec, WTFMove(in.cdmProxyDecryptionClient));
    if (!keyData)
        return false;

    ASSERT(WTF::holds_alternative<Vector<uint8_t>>(keyData.value()));
    auto& keyDataValue = WTF::get<Vector<uint8_t>>(keyData.value());

    if (gcry_error_t cipherError = gcry_cipher_setkey(gCryptHandle(), keyDataValue.data(), keyDataValue.size())) {
#if !LOG_DISABLED
        LOG(EME, "EME - CDMProxyClearKey - ERROR(gcry_cipher_setkey): %s", gpg_strerror(cipherError));
#else
        UNUSED_VARIABLE(cipherError);
#endif
        return false;
    }

    return true;
}

bool CDMProxyClearKey::cencDecryptFullSample(cencDecryptContext& in)
{
    if (!in.encryptedBufferSizeInBytes)
        return true;

    LOG(EME, "EME - CDMProxyClearKey - full-sample decryption: %zu encrypted bytes", in.encryptedBufferSizeInBytes);

    if (gcry_error_t cipherError = gcry_cipher_decrypt(gCryptHandle(), in.encryptedBuffer, in.encryptedBufferSizeInBytes, 0, 0)) {
#if !LOG_DISABLED
        LOG(EME, "EME - CDMProxyClearKey - ERROR(gcry_cipher_decrypt): %s", gpg_strerror(cipherError));
#else
        UNUSED_VARIABLE(cipherError);
#endif
        return false;
    }

    return true;
}

bool CDMProxyClearKey::cencDecryptSubsampled(cencDecryptContext& input)
{
    unsigned encryptedBufferByteOffset = 0;
    size_t subSampleBufferByteOffset = 0;
    unsigned subsampleIndex = 0;
    while (encryptedBufferByteOffset < input.encryptedBufferSizeInBytes) {
        uint16_t subsampleNumClearBytes = 0;
        uint32_t subsampleNumEncryptedBytes = 0;

        if (subsampleIndex < input.numSubsamples) {
            if (!readUInt16(input.subsamplesBuffer, input.subsamplesBufferSizeInBytes, subSampleBufferByteOffset, subsampleNumClearBytes)) {
                LOG(EME, "EME - CDMProxyClearKey - could not read number of clear bytes in subsample at index %u", subsampleIndex);
                return false;
            }
            if (!readUInt32(input.subsamplesBuffer, input.subsamplesBufferSizeInBytes, subSampleBufferByteOffset, subsampleNumEncryptedBytes)) {
                LOG(EME, "EME - CDMProxyClearKey - could not read number of encrypted bytes in subsample at index %u", subsampleIndex);
                return false;
            }
            subsampleIndex++;
        } else {
            subsampleNumClearBytes = 0;
            subsampleNumEncryptedBytes = input.encryptedBufferSizeInBytes - encryptedBufferByteOffset;
        }

        // FIXME: These are high-frequency messages, not sure if there's a better logging lib in WebCore.
        LOG(EME, "EME - subsample index %u - %u bytes clear (%zu bytes left to decrypt)", subsampleIndex, subsampleNumClearBytes, input.encryptedBufferSizeInBytes - encryptedBufferByteOffset);

        encryptedBufferByteOffset += subsampleNumClearBytes;

        if (subsampleNumEncryptedBytes) {
            LOG(EME, "EME - subsample index %u - %u bytes encrypted (%zu bytes left to decrypt)", subsampleIndex, subsampleNumEncryptedBytes, input.encryptedBufferSizeInBytes - encryptedBufferByteOffset);

            if (gcry_error_t cipherError = gcry_cipher_decrypt(gCryptHandle(), input.encryptedBuffer + encryptedBufferByteOffset, subsampleNumEncryptedBytes, 0, 0)) {
#if !LOG_DISABLED
                LOG(EME, "EME - CDMProxyClearKey - ERROR(gcry_cipher_decrypt): %s", gpg_strerror(cipherError));
#else
                UNUSED_VARIABLE(cipherError);
#endif
                return false;
            }

            encryptedBufferByteOffset += subsampleNumEncryptedBytes;
        }
    }

    return true;
}

bool CDMProxyClearKey::cencDecrypt(CDMProxyClearKey::cencDecryptContext& input)
{
    if (!cencSetCounterVector(input) || !cencSetDecryptionKey(input))
        return false;

    return input.isSubsampled() ? cencDecryptSubsampled(input) : cencDecryptFullSample(input);
}

void CDMProxyClearKey::closeGCryptHandle()
{
    if (m_gCryptHandle) {
        gcry_cipher_close(*m_gCryptHandle);
        m_gCryptHandle.reset();
    }
}

gcry_cipher_hd_t& CDMProxyClearKey::gCryptHandle()
{
    if (!m_gCryptHandle) {
        m_gCryptHandle.emplace();
        if (gcry_error_t error = gcry_cipher_open(&m_gCryptHandle.value(), GCRY_CIPHER_AES128, GCRY_CIPHER_MODE_CTR, GCRY_CIPHER_SECURE)) {
#if !LOG_DISABLED
            LOG(EME, "EME - CDMProxyClearKey - ERROR(gcry_cipher_open): %s", gpg_strerror(error));
#else
            UNUSED_VARIABLE(error);
#endif
            RELEASE_ASSERT(false && "Should not get this far with a functional GCrypt!");
        }
    }

    return *m_gCryptHandle;
}

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)
