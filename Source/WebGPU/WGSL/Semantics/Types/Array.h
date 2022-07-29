/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "Matrix.h"
#include "Type.h"
#include "Vector.h"

namespace WGSL::Semantics::Types {

// Array type.
// https://gpuweb.github.io/gpuweb/wgsl/#array-types
// Currently does not support runtime-sized arrays.
class Array : public Type {
public:
    static Expected<UniqueRef<Type>, Error> tryCreate(SourceSpan, UniqueRef<Type> elementType, size_t count);

    bool operator==(const Type &other) const override;
    String string() const override;

    const Type& elementType() const { return m_elementType; }
    size_t count() const { return m_count; }

private:
    Array(SourceSpan, UniqueRef<Type> elementType, size_t count);

    UniqueRef<Type> m_elementType;
    size_t m_count;
};

} // namespace WGSL::Semantics::Types

SPECIALIZE_TYPE_TRAITS_WGSL_SEMANTICS_TYPES(Array, isArray())
