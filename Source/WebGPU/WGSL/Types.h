/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

#include "ASTForward.h"
#include <wtf/HashMap.h>
#include <wtf/Markable.h>
#include <wtf/PrintStream.h>
#include <wtf/text/WTFString.h>

namespace WGSL {

struct Type;

enum class AddressSpace : uint8_t {
    Function,
    Private,
    Workgroup,
    Uniform,
    Storage,
    Handle,
};

enum class AccessMode : uint8_t {
    Read,
    Write,
    ReadWrite,
};

namespace Types {

#define FOR_EACH_PRIMITIVE_TYPE(f) \
    f(AbstractInt, "<AbstractInt>") \
    f(I32, "i32") \
    f(U32, "u32") \
    f(AbstractFloat, "<AbstractFloat>") \
    f(F32, "f32") \
    f(Void, "void") \
    f(Bool, "bool") \
    f(Sampler, "sampler") \
    f(TextureExternal, "texture_external") \

struct Primitive {
    enum Kind : uint8_t {
#define PRIMITIVE_KIND(kind, name) kind,
    FOR_EACH_PRIMITIVE_TYPE(PRIMITIVE_KIND)
#undef PRIMITIVE_KIND
    };

    Kind kind;
};

struct Texture {
    enum class Kind : uint8_t {
        Texture1d = 1,
        Texture2d,
        Texture2dArray,
        Texture3d,
        TextureCube,
        TextureCubeArray,
        TextureMultisampled2d,
        TextureStorage1d,
        TextureStorage2d,
        TextureStorage2dArray,
        TextureStorage3d,
    };

    const Type* element;
    Kind kind;
};

struct Vector {
    const Type* element;
    uint8_t size;
};

struct Matrix {
    const Type* element;
    uint8_t columns;
    uint8_t rows;
};

struct Array {
    const Type* element;
    std::optional<unsigned> size;
};

struct Struct {
    AST::Structure& structure;
    HashMap<String, const Type*> fields { };
};

struct Function {
    WTF::Vector<const Type*> parameters;
    const Type* result;
};

struct Reference {
    AddressSpace addressSpace;
    AccessMode accessMode;
    const Type* element;
};

struct Bottom {
};

} // namespace Types

struct Type : public std::variant<
    Types::Primitive,
    Types::Vector,
    Types::Matrix,
    Types::Array,
    Types::Struct,
    Types::Function,
    Types::Texture,
    Types::Reference,
    Types::Bottom
> {
    using std::variant<
        Types::Primitive,
        Types::Vector,
        Types::Matrix,
        Types::Array,
        Types::Struct,
        Types::Function,
        Types::Texture,
        Types::Reference,
        Types::Bottom
        >::variant;
    void dump(PrintStream&) const;
    String toString() const;
    unsigned size() const;
    unsigned alignment() const;
};

using ConversionRank = Markable<unsigned, IntegralMarkableTraits<unsigned, std::numeric_limits<unsigned>::max()>>;
ConversionRank conversionRank(const Type* from, const Type* to);

bool isPrimitive(const Type*, Types::Primitive::Kind);
bool isPrimitiveReference(const Type*, Types::Primitive::Kind);

} // namespace WGSL

namespace WTF {

template<> class StringTypeAdapter<WGSL::Type, void> {
public:
    StringTypeAdapter(const WGSL::Type& type)
        : m_string { type.toString() }
    {
    }

    unsigned length() const { return m_string.length(); }
    bool is8Bit() const { return m_string.is8Bit(); }
    template<typename CharacterType>
    void writeTo(CharacterType* destination) const
    {
        StringView { m_string }.getCharacters(destination);
        WTF_STRINGTYPEADAPTER_COPIED_WTF_STRING();
    }

private:
    String const m_string;
};

} // namespace WTF

namespace WTF {
void printInternal(PrintStream&, WGSL::AddressSpace);
void printInternal(PrintStream&, WGSL::AccessMode);
} // namespace WTF
