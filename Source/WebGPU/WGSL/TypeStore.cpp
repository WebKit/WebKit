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
#include <wtf/EnumTraits.h>

namespace WGSL {

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

    allocateConstructor<Vector>(AST::ParameterizedTypeName::Base::Vec2, static_cast<uint8_t>(2));
    allocateConstructor<Vector>(AST::ParameterizedTypeName::Base::Vec3, static_cast<uint8_t>(3));
    allocateConstructor<Vector>(AST::ParameterizedTypeName::Base::Vec4, static_cast<uint8_t>(4));
    allocateConstructor<Matrix>(AST::ParameterizedTypeName::Base::Mat2x2, static_cast<uint8_t>(2), static_cast<uint8_t>(2));
    allocateConstructor<Matrix>(AST::ParameterizedTypeName::Base::Mat2x3, static_cast<uint8_t>(2), static_cast<uint8_t>(3));
    allocateConstructor<Matrix>(AST::ParameterizedTypeName::Base::Mat2x4, static_cast<uint8_t>(2), static_cast<uint8_t>(4));
    allocateConstructor<Matrix>(AST::ParameterizedTypeName::Base::Mat3x2, static_cast<uint8_t>(3), static_cast<uint8_t>(2));
    allocateConstructor<Matrix>(AST::ParameterizedTypeName::Base::Mat3x3, static_cast<uint8_t>(3), static_cast<uint8_t>(3));
    allocateConstructor<Matrix>(AST::ParameterizedTypeName::Base::Mat3x4, static_cast<uint8_t>(3), static_cast<uint8_t>(4));
    allocateConstructor<Matrix>(AST::ParameterizedTypeName::Base::Mat4x2, static_cast<uint8_t>(4), static_cast<uint8_t>(2));
    allocateConstructor<Matrix>(AST::ParameterizedTypeName::Base::Mat4x3, static_cast<uint8_t>(4), static_cast<uint8_t>(3));
    allocateConstructor<Matrix>(AST::ParameterizedTypeName::Base::Mat4x4, static_cast<uint8_t>(4), static_cast<uint8_t>(4));
}

Type* TypeStore::structType(const AST::Identifier& name)
{
    return allocateType<Struct>(name);
}

template<typename TypeKind, typename... Arguments>
Type* TypeStore::allocateType(Arguments&&... arguments)
{
    m_types.append(std::unique_ptr<Type>(new Type(TypeKind { std::forward<Arguments>(arguments)... })));
    return m_types.last().get();
}

template<typename TargetType, typename Base, typename... Arguments>
void TypeStore::allocateConstructor(Base base, Arguments&&... arguments)
{
    m_typeConstrutors[WTF::enumToUnderlyingType(base)] =
        TypeConstructor { [this, arguments...](Type* elementType) -> Type* {
            // FIXME: this should be cached
            return allocateType<TargetType>(elementType, arguments...);
        } };
}

} // namespace WGSL
