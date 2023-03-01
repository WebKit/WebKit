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

#include "TypeStore.h"

namespace WGSL {

struct NumericVariable {
    unsigned id;
};

using AbstractValue = std::variant<NumericVariable, unsigned>;

struct TypeVariable {
    enum Constraint : uint8_t {
        None = 0,
        F16 = 1 << 0,
        F32 = 1 << 1,
        AbstractFloat = 1 << 2,
        Float = F16 | F32 | AbstractFloat,

        I32 = 1 << 3,
        U32 = 1 << 4,
        AbstractInt = 1 << 5,
        Integer = I32 | U32 | AbstractInt,

        Number = Float | Integer,
    };

    unsigned id;
    Constraint constraints { None };
};

struct AbstractVector;
struct AbstractMatrix;
using AbstractType = std::variant<
    AbstractVector,
    AbstractMatrix,
    TypeVariable,
    Type*
>;

struct AbstractVector {
    TypeVariable element;
    AbstractValue size;
};

struct AbstractMatrix {
    TypeVariable element;
    AbstractValue columns;
    AbstractValue rows;
};

struct OverloadCandidate {
    WTF::Vector<TypeVariable, 1> typeVariables;
    WTF::Vector<NumericVariable, 2> numericVariables;
    WTF::Vector<AbstractType, 2> parameters;
    AbstractType result;
};

Type* resolveOverloads(TypeStore&, const WTF::Vector<OverloadCandidate>&, const WTF::Vector<Type*>&);

} // namespace WGSL

namespace WTF {
void printInternal(PrintStream&, const WGSL::NumericVariable&);
void printInternal(PrintStream&, const WGSL::AbstractValue&);
void printInternal(PrintStream&, const WGSL::TypeVariable&);
void printInternal(PrintStream&, const WGSL::AbstractType&);
void printInternal(PrintStream&, const WGSL::OverloadCandidate&);
} // namespace WTF
