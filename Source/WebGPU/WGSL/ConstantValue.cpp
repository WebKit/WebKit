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
#include "ConstantValue.h"

#include "Types.h"
#include <wtf/PrintStream.h>
#include <wtf/text/WTFString.h>

namespace WGSL {

ConstantValue ConstantArray::operator[](unsigned index) const
{
    return elements[index];
}

ConstantValue ConstantVector::operator[](unsigned index) const
{
    return elements[index];
}

ConstantVector ConstantMatrix::operator[](unsigned index) const
{
    ConstantVector result(rows);
    for (unsigned i = 0; i < rows; ++i)
        result.elements[i] = elements[index * rows + i];
    return result;
}

void ConstantValue::dump(PrintStream& out) const
{
    WTF::switchOn(*this,
        [&](double d) {
            out.print(String::number(d));
        },
        [&](float f) {
            out.print(String::number(f));
            if (std::isfinite(f))
                out.print("f");
        },
        [&](half h) {
            out.print(String::number(h));
            if (std::isfinite(static_cast<double>(h)))
                out.print("h");
        },
        [&](int64_t i) {
            out.print(String::number(i));
        },
        [&](int32_t i) {
            out.print(String::number(i), "i");
        },
        [&](uint32_t u) {
            out.print(String::number(u), "u");
        },
        [&](bool b) {
            out.print(b ? "true" : "false");
        },
        [&](const ConstantArray& a) {
            out.print("array(");
            bool first = true;
            for (const auto& element : a.elements) {
                if (!first)
                    out.print(", ");
                first = false;
                out.print(element);
            }
            out.print(")");
        },
        [&](const ConstantVector& v) {
            out.print("vec", v.elements.size(), "(");
            bool first = true;
            for (const auto& element : v.elements) {
                if (!first)
                    out.print(", ");
                first = false;
                out.print(element);
            }
            out.print(")");
        },
        [&](const ConstantMatrix& m) {
            out.print("mat", m.columns, "x", m.rows, "(");
            bool first = true;
            for (const auto& element : m.elements) {
                if (!first)
                    out.print(", ");
                first = false;
                out.print(element);
            }
            out.print(")");
        },
        [&](const ConstantStruct& s) {
            out.print("struct { ");
            bool first = true;
            for (const auto& entry : s.fields) {
                if (!first)
                    out.print(", ");
                first = false;
                out.print(entry.key, ": ", entry.value);
            }
            out.print(" }");
        });
}

bool convertValueImpl(const SourceSpan& span, const Type* type, ConstantValue& value)
{
    return WTF::switchOn(*type,
        [&](const Types::Primitive& primitive) -> bool {
            switch (primitive.kind) {
            case Types::Primitive::F32: {
                std::optional<float> result;
                if (auto* f32 = std::get_if<float>(&value))
                    result = convertFloat<float>(*f32);
                else if (auto* abstractFloat = std::get_if<double>(&value))
                    result = convertFloat<float>(*abstractFloat);
                else if (auto* abstractInt = std::get_if<int64_t>(&value))
                    result = convertFloat<float>(static_cast<double>(*abstractInt));

                if (!result.has_value())
                    return false;
                value = { *result };
                return true;
            }
            case Types::Primitive::F16: {
                std::optional<half> result;
                if (auto* f16 = std::get_if<half>(&value))
                    result = convertFloat<half>(*f16);
                else if (auto* abstractFloat = std::get_if<double>(&value))
                    result = convertFloat<half>(*abstractFloat);
                else if (auto* abstractInt = std::get_if<int64_t>(&value))
                    result = convertFloat<half>(static_cast<double>(*abstractInt));

                if (!result.has_value())
                    return false;
                value = { *result };
                return true;
            }
            case Types::Primitive::I32: {
                if (std::holds_alternative<int32_t>(value))
                    return true;
                std::optional<int32_t> result;
                if (auto* abstractInt = std::get_if<int64_t>(&value))
                    result = convertInteger<int32_t>(*abstractInt);

                if (!result.has_value())
                    return false;
                value = { *result };
                return true;
            }
            case Types::Primitive::U32: {
                if (std::holds_alternative<uint32_t>(value))
                    return true;
                std::optional<uint32_t> result;
                if (auto* abstractInt = std::get_if<int64_t>(&value))
                    result = convertInteger<uint32_t>(*abstractInt);

                if (!result.has_value())
                    return false;
                value = { *result };
                return true;
            }
            case Types::Primitive::AbstractInt:
                RELEASE_ASSERT(std::holds_alternative<int64_t>(value));
                return true;
            case Types::Primitive::AbstractFloat: {
                std::optional<double> result;
                if (auto* abstractFloat = std::get_if<double>(&value))
                    result = convertFloat<double>(*abstractFloat);
                else if (auto* abstractInt = std::get_if<int64_t>(&value))
                    result = convertFloat<double>(static_cast<double>(*abstractInt));
                else
                    RELEASE_ASSERT_NOT_REACHED();
                if (!result.has_value())
                    return false;
                value = { *result };
                return true;
            }
            case Types::Primitive::Bool:
                RELEASE_ASSERT(std::holds_alternative<bool>(value));
                return true;
            case Types::Primitive::Void:
            case Types::Primitive::Sampler:
            case Types::Primitive::SamplerComparison:
            case Types::Primitive::TextureExternal:
            case Types::Primitive::AccessMode:
            case Types::Primitive::TexelFormat:
            case Types::Primitive::AddressSpace:
                return false;
            }
        },
        [&](const Types::Vector& vectorType) -> bool {
            ASSERT(value.isVector());
            auto& vector = std::get<ConstantVector>(value);
            for (auto& element : vector.elements) {
                if (!convertValueImpl(span, vectorType.element, element))
                    return false;
            }
            return true;
        },
        [&](const Types::Matrix& matrixType) -> bool {
            ASSERT(value.isMatrix());
            auto& matrix = std::get<ConstantMatrix>(value);
            for (auto& element : matrix.elements) {
                if (!convertValueImpl(span, matrixType.element, element))
                    return false;
            }
            return true;
        },
        [&](const Types::Array& arrayType) -> bool {
            ASSERT(value.isArray());
            auto& array = std::get<ConstantArray>(value);
            for (auto& element : array.elements) {
                if (!convertValueImpl(span, arrayType.element, element))
                    return false;
            }
            return true;
        },
        [&](const Types::Struct& structType) -> bool {
            auto& constantStruct = std::get<ConstantStruct>(value);
            for (auto& [key, type] : structType.fields) {
                auto it = constantStruct.fields.find(key);
                RELEASE_ASSERT(it != constantStruct.fields.end());
                if (!convertValueImpl(span, type, it->value))
                    return false;
            }
            return true;
        },
        [&](const Types::PrimitiveStruct& primitiveStruct) -> bool {
            auto& constantStruct = std::get<ConstantStruct>(value);
            const auto& keys = Types::PrimitiveStruct::keys[primitiveStruct.kind];
            for (auto& entry : constantStruct.fields) {
                auto* key = keys.tryGet(entry.key);
                RELEASE_ASSERT(key);
                auto* type = primitiveStruct.values[*key];
                if (!convertValueImpl(span, type, entry.value))
                    return false;
            }
            return true;
        },
        [&](const Types::Function&) -> bool {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Texture&) -> bool {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::TextureStorage&) -> bool {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::TextureDepth&) -> bool {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Reference&) -> bool {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Pointer&) -> bool {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Atomic&) -> bool {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::TypeConstructor&) -> bool {
            RELEASE_ASSERT_NOT_REACHED();
        });
}

} // namespace WGSL
