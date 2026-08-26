/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if OS(MACOS) || USE(APPLE_INTERNAL_SDK)

#include <JavaScriptCore/CorpseByteParser.h>
#include <span>
#include <stdint.h>
#include <string_view>
#include <wtf/Expected.h>

namespace JSC {
namespace Corpse {

// The dyld exports trie of one Mach-O image: a prefix tree over exported symbol
// names, whose terminals say how to compute each symbol's address.
//
// A trie read out of a corpse is untrusted input, so the walk is bounded and a
// malformed encoding is reported rather than guessed at.
class ExportsTrie {
public:
    // A matched terminal, and how to turn it into an address.
    struct Export {
        enum class Kind : uint8_t {
            Regular, // An offset from the image's base address.
            Absolute, // Already an address, not relative to the image.
        };
        Kind kind { Kind::Regular };
        uint64_t value { 0 };
    };

    enum class Failure : uint8_t {
        Absent,
        Malformed, // An encoding did not decode, or an offset led outside the trie.
        ReExport, // Matched, but the symbol is defined in another image.
        UnsupportedKind, // Matched, but the kind has no one address, such as a thread-local.
    };

    static Expected<Export, Failure> lookUp(std::span<const uint8_t> trie, std::string_view name);
};

} // namespace Corpse
} // namespace JSC

#endif // OS(MACOS) || USE(APPLE_INTERNAL_SDK)
