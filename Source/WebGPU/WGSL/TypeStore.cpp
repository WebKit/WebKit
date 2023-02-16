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

#include "config.h"
#include "TypeStore.h"

#include "ASTTypeName.h"
#include "Types.h"
#include <wtf/EnumTraits.h>

namespace WGSL {

using namespace Types;

// These keys are used so that, for a given type T, we can have keys for all of
// the following types:
// vecN<T>, matCxR<T>, array<T, N?>
//
// To make sure they never collide, we encode them into a pair<Type*, uint64_t>
// where the first element of the pair is always T and the second word is used
// to disambiguate between all the possible types. That's possible because we
// we only have 3 possibilities for Vector (2, 3, 4), 9 possibilities for Matrix
// ((2, 3, 4) * (2, 3, 4)) and 2**32 for Array. To avoid collisions, the
// data is encoded as follows:
//
// Vector: size in the least significant byte.
// Matrix: rows in byte 1 and columns in byte 2
// Array: 0 for dynamic array or 32-bit size in the upper 32-bits
struct VectorKey {
    Type* elementType;
    uint8_t size;

    uint64_t extra() const { return size; }
};

struct MatrixKey {
    Type* elementType;
    uint8_t columns;
    uint8_t rows;

    uint64_t extra() const { return (static_cast<uint64_t>(columns) << 8) | rows; }
};

struct ArrayKey {
    Type* elementType;
    std::optional<unsigned> size;

    uint64_t extra() const { return size.has_value() ? static_cast<uint64_t>(*size) << 32 : 0; }
};

template<typename Key>
Type* TypeStore::TypeCache::find(const Key& key) const
{
    auto it = m_storage.find(std::pair(key.elementType, key.extra()));
    if (it != m_storage.end())
        return it->value;
    return nullptr;
}

template<typename Key>
void TypeStore::TypeCache::insert(const Key& key, Type* type)
{
    auto it = m_storage.add(std::pair(key.elementType, key.extra()), type);
    ASSERT_UNUSED(it, it.isNewEntry);
}

TypeStore::TypeStore()
    : m_typeConstrutors(AST::ParameterizedTypeName::NumberOfBaseTypes)
{
    m_bottom = allocateType<Bottom>();
    m_abstractInt = allocateType<Primitive>(Primitive::AbstractInt);
    m_abstractFloat = allocateType<Primitive>(Primitive::AbstractFloat);
    m_void = allocateType<Primitive>(Primitive::Void);
    m_bool = allocateType<Primitive>(Primitive::Bool);
    m_i32 = allocateType<Primitive>(Primitive::I32);
    m_u32 = allocateType<Primitive>(Primitive::U32);
    m_f32 = allocateType<Primitive>(Primitive::F32);

    allocateConstructor(&TypeStore::vectorType, AST::ParameterizedTypeName::Base::Vec2, 2);
    allocateConstructor(&TypeStore::vectorType, AST::ParameterizedTypeName::Base::Vec3, 3);
    allocateConstructor(&TypeStore::vectorType, AST::ParameterizedTypeName::Base::Vec4, 4);
    allocateConstructor(&TypeStore::matrixType, AST::ParameterizedTypeName::Base::Mat2x2, 2, 2);
    allocateConstructor(&TypeStore::matrixType, AST::ParameterizedTypeName::Base::Mat2x3, 2, 3);
    allocateConstructor(&TypeStore::matrixType, AST::ParameterizedTypeName::Base::Mat2x4, 2, 4);
    allocateConstructor(&TypeStore::matrixType, AST::ParameterizedTypeName::Base::Mat3x2, 3, 2);
    allocateConstructor(&TypeStore::matrixType, AST::ParameterizedTypeName::Base::Mat3x3, 3, 3);
    allocateConstructor(&TypeStore::matrixType, AST::ParameterizedTypeName::Base::Mat3x4, 3, 4);
    allocateConstructor(&TypeStore::matrixType, AST::ParameterizedTypeName::Base::Mat4x2, 4, 2);
    allocateConstructor(&TypeStore::matrixType, AST::ParameterizedTypeName::Base::Mat4x3, 4, 3);
    allocateConstructor(&TypeStore::matrixType, AST::ParameterizedTypeName::Base::Mat4x4, 4, 4);
}

Type* TypeStore::structType(const AST::Identifier& name)
{
    return allocateType<Struct>(name);
}

Type* TypeStore::constructType(AST::ParameterizedTypeName::Base base, Type* elementType)
{
    auto& typeConstructor = m_typeConstrutors[WTF::enumToUnderlyingType(base)];
    return typeConstructor.construct(elementType);
}

Type* TypeStore::arrayType(Type* elementType, std::optional<unsigned> size)
{
    ArrayKey key { elementType, size };
    Type* type = m_cache.find(key);
    if (type)
        return type;
    type = allocateType<Array>(elementType, size);
    m_cache.insert(key, type);
    return type;
}

Type* TypeStore::vectorType(Type* elementType, uint8_t size)
{
    VectorKey key { elementType, size };
    Type* type = m_cache.find(key);
    if (type)
        return type;
    type = allocateType<Vector>(elementType, size);
    m_cache.insert(key, type);
    return type;
}

Type* TypeStore::matrixType(Type* elementType, uint8_t columns, uint8_t rows)
{
    MatrixKey key { elementType, columns, rows };
    Type* type = m_cache.find(key);
    if (type)
        return type;
    type = allocateType<Matrix>(elementType, columns, rows);
    m_cache.insert(key, type);
    return type;
}

template<typename TypeKind, typename... Arguments>
Type* TypeStore::allocateType(Arguments&&... arguments)
{
    m_types.append(std::unique_ptr<Type>(new Type(TypeKind { std::forward<Arguments>(arguments)... })));
    return m_types.last().get();
}

template<typename TargetConstructor, typename Base, typename... Arguments>
void TypeStore::allocateConstructor(TargetConstructor constructor, Base base, Arguments&&... arguments)
{
    m_typeConstrutors[WTF::enumToUnderlyingType(base)] =
        TypeConstructor { [this, constructor, arguments...](Type* elementType) -> Type* {
            return (this->*constructor)(elementType, arguments...);
        } };
}

} // namespace WGSL
