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

#include "CompilationMessage.h"
#include <wtf/TypeCasts.h>
#include <wtf/text/StringConcatenate.h>
#include <wtf/text/WTFString.h>

namespace WGSL::Semantics::Types {

class Type {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Kind {
        Bool,

        // Creation-time constant types.
        AbstractInt,
        AbstractFloat,

        // Integer types.
        U32, // NOLINT
        I32, // NOLINT

        // Floating point types.
        F32, // NOLINT
        F16, // NOLINT

        Atomic,

        // Structure types.
        Vector,
        Matrix,
        Array,
        Struct,

        // Textures.
        Texture,
        MultisampledTexture,
        StorageTexture,
        DepthTexture,

        // Samplers.
        Sampler,
        ComparisonSampler,

        Pointer
    };

    virtual ~Type() { }

    virtual bool operator==(const Type &other) const = 0;
    virtual String string() const = 0;

    Kind kind() const { return m_kind; }

    bool isBool() const { return kind() == Kind::Bool; }
    bool isAbstractInt() const { return kind() == Kind::AbstractInt; }
    bool isAbstractFloat() const { return kind() == Kind::AbstractFloat; }
    bool isU32() const { return kind() == Kind::U32; }
    bool isI32() const { return kind() == Kind::I32; }
    bool isF32() const { return kind() == Kind::F32; }
    bool isF16() const { return kind() == Kind::F16; }
    bool isAtomic() const { return kind() == Kind::Atomic; }
    bool isVector() const { return kind() == Kind::Vector; }
    bool isMatrix() const { return kind() == Kind::Matrix; }
    bool isArray() const { return kind() == Kind::Array; }
    bool isStruct() const { return kind() == Kind::Struct; }
    bool isTexture() const { return kind() == Kind::Texture; }
    bool isMultisampledTexture() const { return kind() == Kind::MultisampledTexture; }
    bool isStorageTexture() const { return kind() == Kind::StorageTexture; }
    bool isDepthTexture() const { return kind() == Kind::DepthTexture; }
    bool isSampler() const { return kind() == Kind::Sampler; }
    bool isComparisonSampler() const { return kind() == Kind::ComparisonSampler; }
    bool isPointer() const { return kind() == Kind::Pointer; }

    // https://gpuweb.github.io/gpuweb/wgsl/#integer-scalar
    bool isIntegerScalar() const;

    // https://gpuweb.github.io/gpuweb/wgsl/#concrete
    bool isConcrete() const;

    // https://gpuweb.github.io/gpuweb/wgsl/#scalar
    bool isScalar() const;

    // https://gpuweb.github.io/gpuweb/wgsl/#abstract-numeric-types
    bool isAbstractNumeric() const;

    // https://gpuweb.github.io/gpuweb/wgsl/#creation-fixed-footprint
    bool hasCreationFixedFootprint() const;

    SourceSpan span() const { return m_span; }

protected:
    Type(Kind, SourceSpan);

    Kind m_kind;

    // The source span where the type is defined. This is intended to be used
    // to produce diagnostic messages.
    // For example:
    //   * The span of an Abstract(Float|Int) is the constant literal
    //   * The span of a type of an identifier type is either the type declaration,
    //     or the initial value of that identifier when it's declared.
    //   * The span of a type of an expression is the entire expression.
    SourceSpan m_span;
};

} // namespace WGSL::Semantics::Types

namespace WTF {

using WGSL::Semantics::Types::Type;

template<> class StringTypeAdapter<const Type&, void> : public StringTypeAdapter<String, void> {
public:
    StringTypeAdapter(const Type& type)
        : StringTypeAdapter<String, void>(type.string())
    {
    }
};

} // namespace WTF

#define SPECIALIZE_TYPE_TRAITS_WGSL_SEMANTICS_TYPES(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WGSL::Semantics::Types::ToValueTypeName) \
    static bool isType(const WGSL::Semantics::Types::Type& type) { return type.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
