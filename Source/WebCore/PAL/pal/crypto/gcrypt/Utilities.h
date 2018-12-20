/*
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

#pragma once

#include <gcrypt.h>
#include <wtf/Assertions.h>
#include <wtf/Optional.h>

namespace PAL {
namespace GCrypt {

// Copied from gcrypt.h. This macro is new in version 1.7.0, which we do not
// want to require yet. Remove this in summer 2019.
#ifndef gcry_cipher_final
#define gcry_cipher_final(a) \
            gcry_cipher_ctl ((a), GCRYCTL_FINALIZE, NULL, 0)
#endif

using CipherOperation = gcry_error_t(gcry_cipher_hd_t, void*, size_t, const void*, size_t);

static inline void logError(gcry_error_t error)
{
    // FIXME: Use a WebCrypto WTF log channel here once those are moved down to PAL.
#if !LOG_DISABLED
    WTFLogAlways("libgcrypt error: source '%s', description '%s'",
        gcry_strsource(error), gcry_strerror(error));
#else
    UNUSED_PARAM(error);
#endif
}

static inline Optional<int> aesAlgorithmForKeySize(size_t keySize)
{
    switch (keySize) {
    case 128:
        return GCRY_CIPHER_AES128;
    case 192:
        return GCRY_CIPHER_AES192;
    case 256:
        return GCRY_CIPHER_AES256;
    default:
        return WTF::nullopt;
    }
}

} // namespace GCrypt
} // namespace PAL
