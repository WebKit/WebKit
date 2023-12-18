/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "WasmTypeDefinition.h"
#include <wtf/TZoneMalloc.h>

namespace JSC { namespace Wasm {

class Tag final : public ThreadSafeRefCounted<Tag> {
    WTF_MAKE_TZONE_ALLOCATED(Tag);
    WTF_MAKE_NONCOPYABLE(Tag);
public:
    static Ref<Tag> create(const TypeDefinition& type) { return adoptRef(*new Tag(type)); }

    FunctionArgCount parameterCount() const { return m_type->as<FunctionSignature>()->argumentCount(); }
    Type parameter(FunctionArgCount i) const { return m_type->as<FunctionSignature>()->argumentType(i); }
    TypeIndex typeIndex() const { return m_type->index(); }

    // Since (1) we do not copy Wasm::Tag and (2) we always allocate Wasm::Tag from heap, we can use
    // pointer comparison for identity check.
    bool operator==(const Tag& other) const { return this == &other; }

    const FunctionSignature& type() const { return *m_type->as<FunctionSignature>(); }

private:
    Tag(const TypeDefinition& type)
        : m_type(Ref { type })
    {
        ASSERT(type.is<FunctionSignature>());
    }

    Ref<const TypeDefinition> m_type;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
