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

#include "ASTTypeName.h"
#include "Types.h"
#include <wtf/FixedVector.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WGSL {

struct Type;

namespace AST {
class Identifier;
}

class TypeStore {
public:
    TypeStore();

    const Type* bottomType() const { return m_bottom; }
    const Type* voidType() const { return m_void; }
    const Type* boolType() const { return m_bool; }

    const Type* abstractIntType() const { return m_abstractInt; }
    const Type* i32Type() const { return m_i32; }
    const Type* u32Type() const { return m_u32; }

    const Type* abstractFloatType() const { return m_abstractFloat; }
    const Type* f32Type() const { return m_f32; }
    const Type* samplerType() const { return m_sampler; }
    const Type* textureExternalType() const { return m_textureExternal; }

    const Type* structType(AST::Structure&);
    const Type* arrayType(const Type*, std::optional<unsigned>);
    const Type* vectorType(const Type*, uint8_t);
    const Type* matrixType(const Type*, uint8_t columns, uint8_t rows);
    const Type* textureType(const Type*, Types::Texture::Kind);
    const Type* functionType(Vector<const Type*>&&, const Type*);
    const Type* referenceType(AddressSpace, const Type*, AccessMode);

    const Type* constructType(AST::ParameterizedTypeName::Base, const Type*);

private:
    class TypeCache {
    public:
        template<typename Key>
        const Type* find(const Key&) const;

        template<typename Key>
        void insert(const Key&, const Type*);

    private:
        HashMap<std::pair<const Type*, uint64_t>, const Type*> m_storage;
    };

    template<typename TypeKind, typename... Arguments>
    const Type* allocateType(Arguments&&...);

    template<typename TargetConstructor, typename Base, typename... Arguments>
    void allocateConstructor(TargetConstructor, Base, Arguments&&...);

    struct TypeConstructor {
        std::function<const Type*(const Type*)> construct;
    };

    Vector<std::unique_ptr<const Type>> m_types;
    FixedVector<TypeConstructor> m_typeConstrutors;
    TypeCache m_cache;

    const Type* m_bottom;
    const Type* m_abstractInt;
    const Type* m_abstractFloat;
    const Type* m_void;
    const Type* m_bool;
    const Type* m_i32;
    const Type* m_u32;
    const Type* m_f32;
    const Type* m_sampler;
    const Type* m_textureExternal;
};

} // namespace WGSL
