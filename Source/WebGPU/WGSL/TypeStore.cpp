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

#include "Types.h"
#include <wtf/EnumTraits.h>

namespace WGSL {

using namespace Types;

struct VectorKey {
    const Type* elementType;
    uint8_t size;

    TypeCache::EncodedKey encode() const { return std::tuple(TypeCache::Vector, size, 0, 0, bitwise_cast<uintptr_t>(elementType)); }
};

struct MatrixKey {
    const Type* elementType;
    uint8_t columns;
    uint8_t rows;

    TypeCache::EncodedKey encode() const { return std::tuple(TypeCache::Matrix, columns, rows, 0, bitwise_cast<uintptr_t>(elementType)); }
};

struct ArrayKey {
    const Type* elementType;
    std::optional<unsigned> size;

    TypeCache::EncodedKey encode() const { return std::tuple(TypeCache::Array, 0, 0, size.value_or(0), bitwise_cast<uintptr_t>(elementType)); }
};

struct TextureKey {
    const Type* elementType;
    Texture::Kind kind;

    TypeCache::EncodedKey encode() const { return std::tuple(TypeCache::Texture, WTF::enumToUnderlyingType(kind), 0, 0, bitwise_cast<uintptr_t>(elementType)); }
};

struct TextureStorageKey {
    TextureStorage::Kind kind;
    TexelFormat format;
    AccessMode access;

    TypeCache::EncodedKey encode() const { return std::tuple(TypeCache::TextureStorage, WTF::enumToUnderlyingType(kind), WTF::enumToUnderlyingType(format), WTF::enumToUnderlyingType(access), 0); }
};

struct ReferenceKey {
    const Type* elementType;
    AddressSpace addressSpace;
    AccessMode accessMode;

    TypeCache::EncodedKey encode() const { return std::tuple(TypeCache::Reference, WTF::enumToUnderlyingType(addressSpace), WTF::enumToUnderlyingType(accessMode), 0, bitwise_cast<uintptr_t>(elementType)); }
};

template<typename Key>
const Type* TypeCache::find(const Key& key) const
{
    auto it = m_storage.find(key.encode());
    if (it != m_storage.end())
        return it->value;
    return nullptr;
}

template<typename Key>
void TypeCache::insert(const Key& key, const Type* type)
{
    auto it = m_storage.add(key.encode(), type);
    ASSERT_UNUSED(it, it.isNewEntry);
}

TypeStore::TypeStore()
{
    m_bottom = allocateType<Bottom>();
    m_abstractInt = allocateType<Primitive>(Primitive::AbstractInt);
    m_abstractFloat = allocateType<Primitive>(Primitive::AbstractFloat);
    m_void = allocateType<Primitive>(Primitive::Void);
    m_bool = allocateType<Primitive>(Primitive::Bool);
    m_i32 = allocateType<Primitive>(Primitive::I32);
    m_u32 = allocateType<Primitive>(Primitive::U32);
    m_f32 = allocateType<Primitive>(Primitive::F32);
    m_sampler = allocateType<Primitive>(Primitive::Sampler);
    m_textureExternal = allocateType<Primitive>(Primitive::TextureExternal);
    m_accessMode = allocateType<Primitive>(Primitive::AccessMode);
    m_texelFormat = allocateType<Primitive>(Primitive::TexelFormat);
}

const Type* TypeStore::structType(AST::Structure& structure, HashMap<String, const Type*>&& fields)
{
    return allocateType<Struct>(structure, fields);
}

const Type* TypeStore::arrayType(const Type* elementType, std::optional<unsigned> size)
{
    ArrayKey key { elementType, size };
    const Type* type = m_cache.find(key);
    if (type)
        return type;
    type = allocateType<Array>(elementType, size);
    m_cache.insert(key, type);
    return type;
}

const Type* TypeStore::vectorType(const Type* elementType, uint8_t size)
{
    VectorKey key { elementType, size };
    const Type* type = m_cache.find(key);
    if (type)
        return type;
    type = allocateType<Vector>(elementType, size);
    m_cache.insert(key, type);
    return type;
}

const Type* TypeStore::matrixType(const Type* elementType, uint8_t columns, uint8_t rows)
{
    MatrixKey key { elementType, columns, rows };
    const Type* type = m_cache.find(key);
    if (type)
        return type;
    type = allocateType<Matrix>(elementType, columns, rows);
    m_cache.insert(key, type);
    return type;
}

const Type* TypeStore::textureType(const Type* elementType, Texture::Kind kind)
{
    TextureKey key { elementType, kind };
    const Type* type = m_cache.find(key);
    if (type)
        return type;
    type = allocateType<Texture>(elementType, kind);
    m_cache.insert(key, type);
    return type;
}

const Type* TypeStore::textureStorageType(TextureStorage::Kind kind, TexelFormat format, AccessMode access)
{
    TextureStorageKey key { kind, format, access };
    const Type* type = m_cache.find(key);
    if (type)
        return type;
    type = allocateType<TextureStorage>(kind, format, access);
    m_cache.insert(key, type);
    return type;
}

const Type* TypeStore::functionType(WTF::Vector<const Type*>&& parameters, const Type* result)
{
    return allocateType<Function>(WTFMove(parameters), result);
}

const Type* TypeStore::referenceType(AddressSpace addressSpace, const Type* element, AccessMode accessMode)
{
    ReferenceKey key { element, addressSpace, accessMode };
    const Type* type = m_cache.find(key);
    if (type)
        return type;
    type = allocateType<Reference>(addressSpace, accessMode, element);
    m_cache.insert(key, type);
    return type;
}

const Type* TypeStore::typeConstructorType(ASCIILiteral name, std::function<const Type*(AST::ElaboratedTypeExpression&)>&& constructor)
{
    return allocateType<TypeConstructor>(name, WTFMove(constructor));
}

template<typename TypeKind, typename... Arguments>
const Type* TypeStore::allocateType(Arguments&&... arguments)
{
    m_types.append(std::unique_ptr<Type>(new Type(TypeKind { std::forward<Arguments>(arguments)... })));
    return m_types.last().get();
}

} // namespace WGSL
