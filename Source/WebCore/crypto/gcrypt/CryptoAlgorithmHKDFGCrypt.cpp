/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Metrological Group B.V.
 * Copyright (C) 2017 Igalia S.L.
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
#include "CryptoAlgorithmHKDF.h"

#if ENABLE(WEB_CRYPTO)

#include "CryptoAlgorithmHkdfParams.h"
#include "CryptoKeyRaw.h"
#include "GCryptUtilities.h"

namespace WebCore {

// libgcrypt doesn't provide HKDF functionality, so we have to implement it manually.
// We should switch to the libgcrypt-provided implementation once it's available.
// https://bugs.webkit.org/show_bug.cgi?id=171536

static std::optional<Vector<uint8_t>> gcryptDeriveBits(const Vector<uint8_t>& key, const Vector<uint8_t>& salt, const Vector<uint8_t>& info, size_t lengthInBytes, CryptoAlgorithmIdentifier identifier)
{
    // libgcrypt doesn't provide HKDF support, so we have to implement
    // the functionality ourselves as specified in RFC5869.
    // https://www.ietf.org/rfc/rfc5869.txt

    auto macAlgorithm = hmacAlgorithm(identifier);
    if (!macAlgorithm)
        return std::nullopt;

    // We can immediately discard invalid output lengths, otherwise needed for the expand step.
    size_t macLength = gcry_mac_get_algo_maclen(*macAlgorithm);
    if (lengthInBytes > macLength * 255)
        return std::nullopt;

    PAL::GCrypt::Handle<gcry_mac_hd_t> handle;
    gcry_error_t error = gcry_mac_open(&handle, *macAlgorithm, 0, nullptr);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    // Step 1 -- Extract. A pseudo-random key is generated with the specified algorithm
    // for the given salt value (used as a key) and the 'input keying material'.
    Vector<uint8_t> pseudoRandomKey(macLength);
    {
        // If the salt vector is empty, a zeroed-out key of macLength size should be used.
        if (salt.isEmpty()) {
            Vector<uint8_t> zeroedKey(macLength, 0);
            error = gcry_mac_setkey(handle, zeroedKey.data(), zeroedKey.size());
        } else
            error = gcry_mac_setkey(handle, salt.data(), salt.size());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return std::nullopt;
        }

        error = gcry_mac_write(handle, key.data(), key.size());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return std::nullopt;
        }

        size_t pseudoRandomKeySize = pseudoRandomKey.size();
        error = gcry_mac_read(handle, pseudoRandomKey.data(), &pseudoRandomKeySize);
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return std::nullopt;
        }

        // Something went wrong if libgcrypt didn't write out the proper amount of data.
        if (pseudoRandomKeySize != macLength)
            return std::nullopt;
    }

    // Step #2 -- Expand.
    Vector<uint8_t> output;
    {
        // Deduce the number of needed iterations to retrieve the necessary amount of data.
        size_t numIterations = (lengthInBytes + macLength) / macLength;
        // Block from the previous iteration is used in the current one, except
        // in the first iteration when it's empty.
        Vector<uint8_t> lastBlock(macLength);

        for (size_t i = 0; i < numIterations; ++i) {
            error = gcry_mac_reset(handle);
            if (error != GPG_ERR_NO_ERROR) {
                PAL::GCrypt::logError(error);
                return std::nullopt;
            }

            error = gcry_mac_setkey(handle, pseudoRandomKey.data(), pseudoRandomKey.size());
            if (error != GPG_ERR_NO_ERROR) {
                PAL::GCrypt::logError(error);
                return std::nullopt;
            }

            // T(0) = empty string (zero length) -- i.e. empty lastBlock
            // T(i) = HMAC-Hash(PRK, T(i-1) | info | hex(i)) -- | represents concatenation
            Vector<uint8_t> blockData;
            if (i)
                blockData.appendVector(lastBlock);
            blockData.appendVector(info);
            blockData.append(i + 1);

            error = gcry_mac_write(handle, blockData.data(), blockData.size());
            if (error != GPG_ERR_NO_ERROR) {
                PAL::GCrypt::logError(error);
                return std::nullopt;
            }

            size_t blockSize = lastBlock.size();
            error = gcry_mac_read(handle, lastBlock.data(), &blockSize);
            if (error != GPG_ERR_NO_ERROR) {
                PAL::GCrypt::logError(error);
                return std::nullopt;
            }

            // Something went wrong if libgcrypt didn't write out the proper amount of data.
            if (blockSize != lastBlock.size())
                return std::nullopt;

            // Append the current block data to the output vector.
            output.appendVector(lastBlock);
        }
    }

    // Clip output vector to the requested size.
    output.resize(lengthInBytes);
    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmHKDF::platformDeriveBits(const CryptoAlgorithmHkdfParams& parameters, const CryptoKeyRaw& key, size_t length)
{
    auto output = gcryptDeriveBits(key.key(), parameters.saltVector(), parameters.infoVector(), length / 8, parameters.hashIdentifier);
    if (!output)
        return Exception { OperationError };
    return WTFMove(*output);
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)
