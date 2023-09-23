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

#include "Constraints.h"
#include "TypeStore.h"

namespace WGSL {

struct ValueVariable {
    unsigned id;
};
using AbstractValue = std::variant<ValueVariable, unsigned>;

struct TypeVariable {
    unsigned id;
    Constraint constraints { Constraints::None };
};

struct AbstractVector;
struct AbstractMatrix;
struct AbstractTexture;
struct AbstractReference;
struct AbstractPointer;
struct AbstractArray;

using AbstractTypeImpl = std::variant<
    AbstractVector,
    AbstractMatrix,
    AbstractTexture,
    AbstractReference,
    AbstractPointer,
    AbstractArray,
    TypeVariable,
    const Type*
>;
using AbstractType = std::unique_ptr<AbstractTypeImpl>;

struct AbstractVector {
    AbstractType element;
    AbstractValue size;
};

struct AbstractMatrix {
    AbstractType element;
    AbstractValue columns;
    AbstractValue rows;
};

struct AbstractTexture {
    AbstractType element;
    Types::Texture::Kind kind;
};

struct AbstractReference {
    AbstractValue addressSpace;
    AbstractType element;
    AbstractValue accessMode;
};

struct AbstractPointer {
    AbstractValue addressSpace;
    AbstractType element;
    AbstractValue accessMode;
};

struct AbstractArray {
    AbstractType element;
};

struct OverloadCandidate {
    Vector<TypeVariable, 1> typeVariables;
    Vector<ValueVariable, 2> valueVariables;
    Vector<AbstractType, 2> parameters;
    AbstractType result;
};

struct SelectedOverload {
    FixedVector<const Type*> parameters;
    const Type* result;
};

std::optional<SelectedOverload> resolveOverloads(TypeStore&, const Vector<OverloadCandidate>&, const Vector<const Type*>& valueArguments, const Vector<const Type*>& typeArguments);

template<typename T>
static AbstractType allocateAbstractType(const T& type)
{
    return std::unique_ptr<AbstractTypeImpl>(new AbstractTypeImpl(type));
}

template<typename T>
static AbstractType allocateAbstractType(T&& type)
{
    return std::unique_ptr<AbstractTypeImpl>(new AbstractTypeImpl(WTFMove(type)));
}

} // namespace WGSL

namespace WTF {
void printInternal(PrintStream&, const WGSL::ValueVariable&);
void printInternal(PrintStream&, const WGSL::AbstractValue&);
void printInternal(PrintStream&, const WGSL::TypeVariable&);
void printInternal(PrintStream&, const WGSL::AbstractType&);
void printInternal(PrintStream&, const WGSL::OverloadCandidate&);
void printInternal(PrintStream&, WGSL::Types::Texture::Kind);
} // namespace WTF
