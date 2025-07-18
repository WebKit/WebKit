/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#pragma once

#include "CodeSpecializationKind.h"
#include <wtf/PrintStream.h>
#include <wtf/SixCharacterHash.h>
#include <wtf/text/StringImpl.h>

// CodeBlock hashes are useful for informally identifying code blocks. They correspond
// to the low 32 bits of a SHA1 hash of the source code with two low bit flipped
// according to the role that the code block serves (call, construct). Additionally, the
// hashes are typically operated over using a string in which the hash is transformed
// into a 6-byte alphanumeric representation. This can be retrieved by using
// toString(const CodeBlockHash&). Finally, we support CodeBlockHashes for native
// functions, in which case the hash is replaced by the function address.

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

class SourceCode;

class CodeBlockHash {
public:
    static constexpr size_t stringLength = 6;

    CodeBlockHash() = default;

    explicit CodeBlockHash(unsigned hash)
        : m_hash(hash)
    {
    }

    JS_EXPORT_PRIVATE CodeBlockHash(const SourceCode&, CodeSpecializationKind);
    JS_EXPORT_PRIVATE CodeBlockHash(StringView codeBlockSourceCode, StringView entireSourceCode, CodeSpecializationKind);

    explicit CodeBlockHash(std::span<const char, stringLength>);

    bool isSet() const { return !!m_hash; }
    explicit operator bool() const { return isSet(); }

    unsigned hash() const { return m_hash; }

    void dump(PrintStream&) const;

    // Comparison methods useful for bisection.
    friend auto operator<=>(const CodeBlockHash&, const CodeBlockHash&) = default;

private:
    unsigned m_hash { };
};

} // namespace JSC
namespace WTF {

template<> class StringTypeAdapter<JSC::CodeBlockHash> {
public:
    explicit StringTypeAdapter(const JSC::CodeBlockHash& hash)
        : m_hash { hash }
    {
    }

    unsigned length() const { return JSC::CodeBlockHash::stringLength; }
    bool is8Bit() const { return true; }
    template<typename CharacterType> void writeTo(std::span<CharacterType> destination) const
    {
        auto buffer = integerToSixCharacterHashString(m_hash.hash());
        StringImpl::copyCharacters(destination, std::span<const LChar>(std::bit_cast<const LChar*>(buffer.data()), buffer.size()));
    }

private:
    JSC::CodeBlockHash m_hash;
};

} // namespace WTF

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
