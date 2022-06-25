/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <array>
#include <wtf/Span.h>
#include <wtf/text/CString.h>

#if PLATFORM(COCOA)
#include <CommonCrypto/CommonDigest.h>
#endif

namespace WTF {

class SHA1 {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WTF_EXPORT_PRIVATE SHA1();

    WTF_EXPORT_PRIVATE void addBytes(Span<const std::byte>);

    void addBytes(Span<const uint8_t> input)
    {
        addBytes(asBytes(input));
    }

    void addBytes(const CString& input)
    {
        addBytes(input.bytes());
    }

    void addBytes(const uint8_t* input, size_t length)
    {
        addBytes(Span { input, length });
    }

    // Size of the SHA1 hash
    WTF_EXPORT_PRIVATE static constexpr size_t hashSize = 20;

    // type for computing SHA1 hash
    typedef std::array<uint8_t, hashSize> Digest;

    WTF_EXPORT_PRIVATE void computeHash(Digest&);
    
    // Get a hex hash from the digest.
    WTF_EXPORT_PRIVATE static CString hexDigest(const Digest&);
    
    // Compute the hex digest directly.
    WTF_EXPORT_PRIVATE CString computeHexDigest();

private:
#if PLATFORM(COCOA)
    CC_SHA1_CTX m_context;
#else
    void finalize();
    void processBlock();
    void reset();

    uint8_t m_buffer[64];
    size_t m_cursor; // Number of bytes filled in m_buffer (0-64).
    uint64_t m_totalBytes; // Number of bytes added so far.
    uint32_t m_hash[5];
#endif
};

} // namespace WTF

using WTF::SHA1;
