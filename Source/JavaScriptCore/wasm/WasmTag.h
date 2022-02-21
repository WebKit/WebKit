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

#if ENABLE(WEBASSEMBLY)

#include "WasmSignature.h"

namespace JSC { namespace Wasm {

class Tag final : public ThreadSafeRefCounted<Tag> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(Tag);
public:
    static Ref<Tag> create(const Signature& signature) { return adoptRef(*new Tag(signature)); }

    SignatureArgCount parameterCount() const { return m_signature->argumentCount(); }
    Type parameter(SignatureArgCount i) const { return m_signature->argument(i); }

    // Since (1) we do not copy Wasm::Tag and (2) we always allocate Wasm::Tag from heap, we can use
    // pointer comparison for identity check.
    bool operator==(const Tag& other) const { return this == &other; }
    bool operator!=(const Tag& other) const { return this != &other; }

    const Signature& signature() const { return m_signature.get(); }

private:
    Tag(const Signature& signature)
        : m_signature(Ref { signature })
    {
    }

    Ref<const Signature> m_signature;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
