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
#include "WGSLEnums.h"
#include <wtf/FixedVector.h>
#include <wtf/HashMap.h>
#include <wtf/Markable.h>
#include <wtf/PrintStream.h>
#include <wtf/SortedArrayMap.h>
#include <wtf/text/WTFString.h>

namespace WGSL {

class TypeChecker;
class TypeStore;
struct Type;

enum Packing : uint8_t {
    Packed       = 1 << 0,
    Unpacked     = 1 << 1,
    Either       = Packed | Unpacked,

    PStruct = 1 << 2,
    Vec3   = 1 << 3,

    PackedStruct = Packed | PStruct,
    PackedVec3   = Packed | Vec3,
};


namespace Types {

#define FOR_EACH_PRIMITIVE_TYPE(f) \
    f(AbstractInt, "<AbstractInt>") \
    f(I32, "i32") \
    f(U32, "u32") \
    f(AbstractFloat, "<AbstractFloat>") \
    f(F16, "f16") \
    f(F32, "f32") \
    f(Void, "void") \
    f(Bool, "bool") \
    f(Sampler, "sampler") \
    f(SamplerComparison, "sampler_comparion") \
    f(TextureExternal, "texture_external") \
    f(AccessMode, "access_mode") \
    f(TexelFormat, "texel_format") \
    f(AddressSpace, "address_space") \

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
    };

    const Type* element;
    Kind kind;
};

struct TextureStorage {
    enum class Kind : uint8_t {
        TextureStorage1d = 1,
        TextureStorage2d,
        TextureStorage2dArray,
        TextureStorage3d,
    };

    Kind kind;
    TexelFormat format;
    AccessMode access;
};

struct TextureDepth {
    enum class Kind : uint8_t {
        TextureDepth2d = 1,
        TextureDepth2dArray,
        TextureDepthCube,
        TextureDepthCubeArray,
        TextureDepthMultisampled2d,
    };

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
    // std::monostate represents a runtime-sized array
    // unsigned represents a creation fixed array (constant size)
    // AST::Expression* represents a fixed array (override size)
    using Size = std::variant<std::monostate, unsigned, AST::Expression*>;

    const Type* element;
    Size size;

    bool isRuntimeSized() const { return std::holds_alternative<std::monostate>(size); }
    bool isCreationFixed() const { return std::holds_alternative<unsigned>(size); }
    bool isOverrideSized() const { return std::holds_alternative<AST::Expression*>(size); }
};

struct Struct {
    AST::Structure& structure;
    HashMap<String, const Type*> fields { };
};

struct PrimitiveStruct {
private:
    enum Kind : uint8_t {
        FrexpResult,
        ModfResult,
        AtomicCompareExchangeResult,
    };

public:
    struct FrexpResult {
        static constexpr Kind kind = Kind::FrexpResult;
        static constexpr unsigned fract = 0;
        static constexpr unsigned exp = 1;

        static constexpr std::pair<ComparableASCIILiteral, unsigned> mapEntries[] {
            { "exp", exp },
            { "fract", fract },
        };

        static constexpr SortedArrayMap map { mapEntries };
    };

    struct ModfResult {
        static constexpr Kind kind = Kind::ModfResult;
        static constexpr unsigned fract = 0;
        static constexpr unsigned whole = 1;

        static constexpr std::pair<ComparableASCIILiteral, unsigned> mapEntries[] {
            { "fract", fract },
            { "whole", whole },
        };

        static constexpr SortedArrayMap map { mapEntries };
    };

    struct AtomicCompareExchangeResult {
        static constexpr Kind kind = Kind::AtomicCompareExchangeResult;
        static constexpr unsigned oldValue = 0;
        static constexpr unsigned exchanged = 1;

        static constexpr std::pair<ComparableASCIILiteral, unsigned> mapEntries[] {
            { "exchanged", exchanged },
            { "old_value", oldValue },
        };

        static constexpr SortedArrayMap map { mapEntries };
    };

    static constexpr SortedArrayMap<std::pair<ComparableASCIILiteral, unsigned>[2]> keys[] {
        FrexpResult::map,
        ModfResult::map,
        AtomicCompareExchangeResult::map,
    };

    String name;
    Kind kind;
    FixedVector<const Type*> values;
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

struct Pointer {
    AddressSpace addressSpace;
    AccessMode accessMode;
    const Type* element;
};

struct Atomic {
    const Type* element;
};

struct TypeConstructor {
    ASCIILiteral name;
    std::function<const Type*(AST::ElaboratedTypeExpression&)> construct;
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
    Types::PrimitiveStruct,
    Types::Function,
    Types::Texture,
    Types::TextureStorage,
    Types::TextureDepth,
    Types::Reference,
    Types::Pointer,
    Types::Atomic,
    Types::TypeConstructor,
    Types::Bottom
> {
    using std::variant<
        Types::Primitive,
        Types::Vector,
        Types::Matrix,
        Types::Array,
        Types::Struct,
        Types::PrimitiveStruct,
        Types::Function,
        Types::Texture,
        Types::TextureStorage,
        Types::TextureDepth,
        Types::Reference,
        Types::Pointer,
        Types::Atomic,
        Types::TypeConstructor,
        Types::Bottom
        >::variant;
    void dump(PrintStream&) const;
    String toString() const;
    unsigned size() const;
    unsigned alignment() const;
    Packing packing() const;
    bool isConstructible() const;
    bool isStorable() const;
    bool isHostShareable() const;
    bool hasFixedFootprint() const;
    bool hasCreationFixedFootprint() const;
    bool containsRuntimeArray() const;
    bool containsOverrideArray() const;
};

using ConversionRank = Markable<unsigned, IntegralMarkableTraits<unsigned, std::numeric_limits<unsigned>::max()>>;
ConversionRank conversionRank(const Type* from, const Type* to);

bool isPrimitive(const Type*, Types::Primitive::Kind);
bool isPrimitiveReference(const Type*, Types::Primitive::Kind);
const Type* shaderTypeForTexelFormat(TexelFormat, const TypeStore&);

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
