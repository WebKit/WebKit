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

    Type* bottomType() const { return m_bottom; }
    Type* voidType() const { return m_void; }
    Type* boolType() const { return m_bool; }

    Type* abstractIntType() const { return m_abstractInt; }
    Type* i32Type() const { return m_i32; }
    Type* u32Type() const { return m_u32; }

    Type* abstractFloatType() const { return m_abstractFloat; }
    Type* f32Type() const { return m_f32; }

    Type* structType(const AST::Identifier& name);
    Type* arrayType(Type*, std::optional<unsigned>);
    Type* vectorType(Type*, uint8_t);
    Type* matrixType(Type*, uint8_t columns, uint8_t rows);

    Type* constructType(AST::ParameterizedTypeName::Base, Type*);

private:
    class TypeCache {
    public:
        template<typename Key>
        Type* find(const Key&) const;

        template<typename Key>
        void insert(const Key&, Type*);

    private:
        HashMap<std::pair<Type*, uint64_t>, Type*> m_storage;
    };

    template<typename TypeKind, typename... Arguments>
    Type* allocateType(Arguments&&...);

    template<typename TargetConstructor, typename Base, typename... Arguments>
    void allocateConstructor(TargetConstructor, Base, Arguments&&...);

    struct TypeConstructor {
        std::function<Type*(Type*)> construct;
    };

    Vector<std::unique_ptr<Type>> m_types;
    FixedVector<TypeConstructor> m_typeConstrutors;
    TypeCache m_cache;

    Type* m_bottom;
    Type* m_abstractInt;
    Type* m_abstractFloat;
    Type* m_void;
    Type* m_bool;
    Type* m_i32;
    Type* m_u32;
    Type* m_f32;
};

} // namespace WGSL
