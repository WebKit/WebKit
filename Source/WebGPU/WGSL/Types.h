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

#include <wtf/HashMap.h>
#include <wtf/PrintStream.h>
#include <wtf/text/WTFString.h>

namespace WGSL {

struct Type;

namespace Types {

#define FOR_EACH_PRIMITIVE_TYPE(f) \
    f(AbstractInt, "<AbstractInt>") \
    f(I32, "i32") \
    f(U32, "u32") \
    f(AbstractFloat, "<AbstractFloat>") \
    f(F32, "f32") \
    f(Void, "void") \
    f(Bool, "bool") \

struct Primitive {
    enum Kind : uint8_t {
#define PRIMITIVE_KIND(kind, name) kind,
    FOR_EACH_PRIMITIVE_TYPE(PRIMITIVE_KIND)
#undef PRIMITIVE_KIND
    };

    Kind kind;
};

struct Vector {
    Type* element;
    uint8_t size;
};

struct Matrix {
    Type* element;
    uint8_t columns;
    uint8_t rows;
};

struct Array {
    Type* element;
    std::optional<unsigned> size;
};

struct Struct {
    String name;
    HashMap<String, Type*> fields { };
};

struct Function {
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
    Types::Bottom
> {
    using std::variant<
        Types::Primitive,
        Types::Vector,
        Types::Matrix,
        Types::Array,
        Types::Struct,
        Types::Function,
        Types::Bottom
        >::variant;
    void dump(PrintStream&) const;
    String toString() const;
};

} // namespace WGSL

namespace WTF {

template<> class StringTypeAdapter<WGSL::Type, void> : public StringTypeAdapter<String, void> {
public:
    StringTypeAdapter(const WGSL::Type& type)
        : StringTypeAdapter<String, void> { type.toString() }
    { }
};

} // namespace WTF
