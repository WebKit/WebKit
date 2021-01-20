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

#pragma once

#if ENABLE(ENCRYPTED_MEDIA)

#include "CDMClearKey.h"
#include "CDMInstanceSession.h"
#include "MediaPlayerPrivate.h"
#include "SharedBuffer.h"
#include <gcrypt.h>
#include <wtf/Condition.h>
#include <wtf/Optional.h>
#include <wtf/VectorHash.h>

namespace WebCore {

class CDMProxyFactoryClearKey final : public CDMProxyFactory {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static CDMProxyFactoryClearKey& singleton();

    ~CDMProxyFactoryClearKey() = default;

private:
    friend class NeverDestroyed<CDMProxyFactoryClearKey>;

    CDMProxyFactoryClearKey() = default;
    RefPtr<CDMProxy> createCDMProxy(const String&) final;
    bool supportsKeySystem(const String&) final;
};

// This is the thread-safe API that decode threads should use to make use of a
// platform CDM module.
class CDMProxyClearKey final : public CDMProxy, public CanMakeWeakPtr<CDMProxyClearKey, WeakPtrFactoryInitialization::Eager> {
public:
    CDMProxyClearKey() = default;
    virtual ~CDMProxyClearKey();

    // FIXME: There's a lack of consistency between SharedBuffers,
    // Vector<char>'s and {uint8_t*,size_t} idioms for representing a chunk of
    // bytes. Fix that somehow. Note SharedBuffers are dangerous since
    // they're not thread-safe and this class must maintain
    // thread-safety, but maybe SharedBuffer::DataSegment could be
    // made to work, however that has zero helpers for dropping into
    // iterator-aware / comparator-aware containers...
    struct cencDecryptContext {
        // FIXME: Enacapsulate these fields in non-copied types.
        const uint8_t* keyID;
        size_t keyIDSizeInBytes;

        const uint8_t* iv;
        size_t ivSizeInBytes;

        uint8_t* encryptedBuffer;
        size_t encryptedBufferSizeInBytes;

        // FIXME: GStreamer-specific data layout.
        // If we want to get rid of the specific data layout, we can parse it before
        // assigning it here and maybe have a Vector<size_t> that is a series of
        // clear/decrypted/clear/decrypted... Or maybe even a Vector<std::pair<size_t, size_t>>
        // representing sequences of clear/decrypted sizes. That std::pair could even
        // become a struct ClearDecryptedChunkSizesInBytes for example.
        // https://bugs.webkit.org/show_bug.cgi?id=206730
        const uint8_t* subsamplesBuffer;
        size_t subsamplesBufferSizeInBytes;
        size_t numSubsamples;

        bool isSubsampled() const { return numSubsamples; }
    };

    // FIXME: Unconditional in-place decryption. What about SVP?
    // FIXME: GStreamer-specific, in that the format of the subsample
    // data is defined by whatever qtdemux decides to do with it.
    bool cencDecrypt(cencDecryptContext&);

private:
    gcry_cipher_hd_t& gCryptHandle();

    // FIXME: For now we only support AES in CTR mode, in the future
    // we will surely have to support more protection schemes. Can we
    // reuse some Crypto APIs in WebCore?
    bool cencSetCounterVector(const cencDecryptContext&);
    bool cencSetDecryptionKey(const cencDecryptContext&);
    bool cencDecryptFullSample(cencDecryptContext&);
    bool cencDecryptSubsampled(cencDecryptContext&);

    void releaseDecryptionResources() final;
    void closeGCryptHandle();

    // FIXME: It would be nice to use something in WebCore for crypto...
    Optional<gcry_cipher_hd_t> m_gCryptHandle { WTF::nullopt };
};

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)
