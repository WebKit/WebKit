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
#include "CryptoAlgorithmAES_CTR.h"

#if ENABLE(WEB_CRYPTO)

#include "CryptoAlgorithmAesCtrParams.h"
#include "CryptoKeyAES.h"
#include <pal/crypto/gcrypt/Handle.h>
#include <pal/crypto/gcrypt/Utilities.h>

namespace WebCore {

// This is a helper function that resets the cipher object, sets the provided counter data,
// and executes the encrypt or decrypt operation, retrieving and returning the output data.
static Optional<Vector<uint8_t>> callOperation(PAL::GCrypt::CipherOperation operation, gcry_cipher_hd_t handle, const Vector<uint8_t>& counter, const uint8_t* data, const size_t size)
{
    gcry_error_t error = gcry_cipher_reset(handle);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    error = gcry_cipher_setctr(handle, counter.data(), counter.size());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    error = gcry_cipher_final(handle);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    Vector<uint8_t> output(size);
    error = operation(handle, output.data(), output.size(), data, size);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    return output;
}

static Optional<Vector<uint8_t>> gcryptAES_CTR(PAL::GCrypt::CipherOperation operation, const Vector<uint8_t>& key, const Vector<uint8_t>& counter, size_t counterLength, const Vector<uint8_t>& inputText)
{
    constexpr size_t blockSize = 16;
    auto algorithm = PAL::GCrypt::aesAlgorithmForKeySize(key.size() * 8);
    if (!algorithm)
        return WTF::nullopt;

    // Construct the libgcrypt cipher object and attach the key to it. Key information on this
    // cipher object will live through any gcry_cipher_reset() calls.
    PAL::GCrypt::Handle<gcry_cipher_hd_t> handle;
    gcry_error_t error = gcry_cipher_open(&handle, *algorithm, GCRY_CIPHER_MODE_CTR, 0);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    error = gcry_cipher_setkey(handle, key.data(), key.size());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    // Calculate the block count: ((inputText.size() + blockSize - 1) / blockSize), remainder discarded.
    PAL::GCrypt::Handle<gcry_mpi_t> blockCountMPI(gcry_mpi_new(0));
    {
        PAL::GCrypt::Handle<gcry_mpi_t> blockSizeMPI(gcry_mpi_set_ui(nullptr, blockSize));
        PAL::GCrypt::Handle<gcry_mpi_t> roundedUpSize(gcry_mpi_set_ui(nullptr, inputText.size()));

        gcry_mpi_add_ui(roundedUpSize, roundedUpSize, blockSize - 1);
        gcry_mpi_div(blockCountMPI, nullptr, roundedUpSize, blockSizeMPI, 0);
    }

    // Calculate the counter limit for the specified counter length: (2 << counterLength).
    // (counterLimitMPI - 1) is the maximum value the counter can hold -- essentially it's
    // a bit-mask for valid counter values.
    PAL::GCrypt::Handle<gcry_mpi_t> counterLimitMPI(gcry_mpi_set_ui(nullptr, 1));
    gcry_mpi_mul_2exp(counterLimitMPI, counterLimitMPI, counterLength);

    // Counter values must not repeat for a given cipher text. If the counter limit (i.e.
    // the number of unique counter values we could produce for the specified counter
    // length) is lower than the deduced block count, we bail.
    if (gcry_mpi_cmp(counterLimitMPI, blockCountMPI) < 0)
        return WTF::nullopt;

    // If the counter length, in bits, matches the size of the counter data, we don't have to
    // use any part of the counter Vector<> as nonce. This allows us to directly encrypt or
    // decrypt all the provided data in a single step.
    if (counterLength == counter.size() * 8)
        return callOperation(operation, handle, counter, inputText.data(), inputText.size());

    // Scan the counter data into the MPI format. We'll do all the counter computations with
    // the MPI API.
    PAL::GCrypt::Handle<gcry_mpi_t> counterDataMPI;
    error = gcry_mpi_scan(&counterDataMPI, GCRYMPI_FMT_USG, counter.data(), counter.size(), nullptr);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    // Extract the counter MPI from the counterDataMPI: (counterDataMPI % counterLimitMPI).
    // This MPI represents solely the counter value, as initially provided.
    PAL::GCrypt::Handle<gcry_mpi_t> counterMPI(gcry_mpi_new(0));
    gcry_mpi_mod(counterMPI, counterDataMPI, counterLimitMPI);

    {
        // Calculate the leeway of the initially-provided counter: counterLimitMPI - counterMPI.
        // This is essentially the number of blocks we can encrypt/decrypt with that counter
        // (incrementing it after each operation) before the counter wraps around to 0.
        PAL::GCrypt::Handle<gcry_mpi_t> counterLeewayMPI(gcry_mpi_new(0));
        gcry_mpi_sub(counterLeewayMPI, counterLimitMPI, counterMPI);

        // If counterLeewayMPI is larger or equal to the deduced block count, we can directly
        // encrypt or decrypt the provided data in a single step since it's ensured that the
        // counter won't overflow.
        if (gcry_mpi_cmp(counterLeewayMPI, blockCountMPI) >= 0)
            return callOperation(operation, handle, counter, inputText.data(), inputText.size());
    }

    // From here onwards we're dealing with a counter of which the length doesn't match the
    // provided data, meaning we'll also have to manage the nonce data. The counter will also
    // wrap around, so we'll have to address that too.

    // Determine the nonce MPI that we'll use to reconstruct the counter data for each block:
    // (counterDataMPI - counterMPI). This is equivalent to counterDataMPI with the lowest
    // counterLength bits cleared.
    PAL::GCrypt::Handle<gcry_mpi_t> nonceMPI(gcry_mpi_new(0));
    gcry_mpi_sub(nonceMPI, counterDataMPI, counterMPI);

    // FIXME: This should be optimized further by first encrypting the amount of blocks for
    // which the counter won't yet wrap around, and then encrypting the rest of the blocks
    // starting from the counter set to 0.

    Vector<uint8_t> output;
    Vector<uint8_t> blockCounterData(16);
    size_t inputTextSize = inputText.size();

    for (size_t i = 0; i < inputTextSize; i += 16) {
        size_t blockInputSize = std::min<size_t>(16, inputTextSize - i);

        // Construct the block-specific counter: (nonceMPI + counterMPI).
        PAL::GCrypt::Handle<gcry_mpi_t> blockCounterMPI(gcry_mpi_new(0));
        gcry_mpi_add(blockCounterMPI, nonceMPI, counterMPI);

        error = gcry_mpi_print(GCRYMPI_FMT_USG, blockCounterData.data(), blockCounterData.size(), nullptr, blockCounterMPI);
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return WTF::nullopt;
        }

        // Encrypt/decrypt this single block with the block-specific counter. Output for this
        // single block is appended to the general output vector.
        auto blockOutput = callOperation(operation, handle, blockCounterData, inputText.data() + i, blockInputSize);
        if (!blockOutput)
            return WTF::nullopt;

        output.appendVector(*blockOutput);

        // Increment the counter. The modulus operation takes care of any wrap-around.
        PAL::GCrypt::Handle<gcry_mpi_t> counterIncrementMPI(gcry_mpi_new(0));
        gcry_mpi_add_ui(counterIncrementMPI, counterMPI, 1);
        gcry_mpi_mod(counterMPI, counterIncrementMPI, counterLimitMPI);
    }

    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAES_CTR::platformEncrypt(const CryptoAlgorithmAesCtrParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& plainText)
{
    auto output = gcryptAES_CTR(gcry_cipher_encrypt, key.key(), parameters.counterVector(), parameters.length, plainText);
    if (!output)
        return Exception { OperationError };
    return WTFMove(*output);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAES_CTR::platformDecrypt(const CryptoAlgorithmAesCtrParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& cipherText)
{
    auto output = gcryptAES_CTR(gcry_cipher_decrypt, key.key(), parameters.counterVector(), parameters.length, cipherText);
    if (!output)
        return Exception { OperationError };
    return WTFMove(*output);
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)
