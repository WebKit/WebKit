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
#include "Types.h"

#include "ASTStructure.h"
#include <wtf/StdLibExtras.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/StringHash.h>

namespace WGSL {

using namespace Types;

void Type::dump(PrintStream& out) const
{
    WTF::switchOn(*this,
        [&](const Primitive& primitive) {
            switch (primitive.kind) {
#define PRIMITIVE_CASE(kind, name) \
            case Primitive::kind: \
                out.print(name); \
                break;
            FOR_EACH_PRIMITIVE_TYPE(PRIMITIVE_CASE)
#undef PRIMITIVE_CASE
            }
        },
        [&](const Vector& vector) {
            out.print("vec", vector.size, "<", *vector.element, ">");
        },
        [&](const Matrix& matrix) {
            out.print("mat", matrix.columns, "x", matrix.rows, "<", *matrix.element, ">");
        },
        [&](const Array& array) {
            out.print("array<", *array.element);
            if (array.size.has_value())
                out.print(", ", *array.size);
            out.print(">");
        },
        [&](const Struct& structure) {
            out.print(structure.structure.name());
        },
        [&](const Function& function) {
            out.print("(");
            bool first = true;
            for (auto* parameter : function.parameters) {
                if (!first)
                    out.print(", ");
                first = false;
                out.print(*parameter);
            }
            out.print(") -> ", *function.result);
        },
        [&](const Texture& texture) {
            switch (texture.kind) {
            case Texture::Kind::Texture1d:
                out.print("texture_1d");
                break;
            case Texture::Kind::Texture2d:
                out.print("texture_2d");
                break;
            case Texture::Kind::Texture2dArray:
                out.print("texture_2d_array");
                break;
            case Texture::Kind::Texture3d:
                out.print("texture_3d");
                break;
            case Texture::Kind::TextureCube:
                out.print("texture_cube");
                break;
            case Texture::Kind::TextureCubeArray:
                out.print("texture_cube_array");
                break;
            case Texture::Kind::TextureMultisampled2d:
                out.print("texture_multisampled_2d");
                break;
            case Texture::Kind::TextureStorage1d:
                out.print("texture_storage_1d");
                break;
            case Texture::Kind::TextureStorage2d:
                out.print("texture_storage_2d");
                break;
            case Texture::Kind::TextureStorage2dArray:
                out.print("texture_storage_2d_array");
                break;
            case Texture::Kind::TextureStorage3d:
                out.print("texture_storage_3d");
                break;
            }
            out.print("<", *texture.element, ">");
        },
        [&](const Reference& reference) {
            out.print("ref<", reference.addressSpace, ", ", *reference.element, ", ", reference.accessMode, ">");
        },
        [&](const Bottom&) {
            // Bottom is an implementation detail and should never leak, but we
            // keep the ability to print it in debug to help when dumping types
            out.print("⊥");
        });
}

constexpr unsigned primitivePair(Types::Primitive::Kind first, Types::Primitive::Kind second)
{
    static_assert(sizeof(Types::Primitive::Kind) == 1);
    return static_cast<unsigned>(first) << 8 | second;
}

// https://www.w3.org/TR/WGSL/#conversion-rank
ConversionRank conversionRank(const Type* from, const Type* to)
{
    using namespace WGSL::Types;

    if (from == to)
        return { 0 };

    if (auto* fromReference = std::get_if<Reference>(from)) {
        if (fromReference->accessMode == AccessMode::Write)
            return std::nullopt;
        return conversionRank(fromReference->element, to);
    }

    if (auto* toReference = std::get_if<Reference>(to)) {
        if (toReference->accessMode == AccessMode::Write)
            return std::nullopt;
        return conversionRank(from, toReference->element);
    }

    if (auto* fromPrimitive = std::get_if<Primitive>(from)) {
        auto* toPrimitive = std::get_if<Primitive>(to);
        if (!toPrimitive)
            return std::nullopt;

        switch (primitivePair(fromPrimitive->kind, toPrimitive->kind)) {
        case primitivePair(Primitive::AbstractFloat, Primitive::F32):
            return { 1 };
        // FIXME: AbstractFloat to f16 should return 2
        case primitivePair(Primitive::AbstractInt, Primitive::I32):
            return { 3 };
        case primitivePair(Primitive::AbstractInt, Primitive::U32):
            return { 4 };
        case primitivePair(Primitive::AbstractInt, Primitive::AbstractFloat):
            return { 5 };
        case primitivePair(Primitive::AbstractInt, Primitive::F32):
            return { 6 };
        // FIXME: AbstractInt to f16 should return 7
        default:
            return std::nullopt;
        }
    }

    if (auto* fromVector = std::get_if<Vector>(from)) {
        auto* toVector = std::get_if<Vector>(to);
        if (!toVector)
            return std::nullopt;
        if (fromVector->size != toVector->size)
            return std::nullopt;
        return conversionRank(fromVector->element, toVector->element);
    }

    if (auto* fromMatrix = std::get_if<Matrix>(from)) {
        auto* toMatrix = std::get_if<Matrix>(to);
        if (!toMatrix)
            return std::nullopt;
        if (fromMatrix->columns != toMatrix->columns)
            return std::nullopt;
        if (fromMatrix->rows != toMatrix->rows)
            return std::nullopt;
        return conversionRank(fromMatrix->element, toMatrix->element);
    }

    if (auto* fromArray = std::get_if<Array>(from)) {
        auto* toArray = std::get_if<Array>(to);
        if (!toArray)
            return std::nullopt;
        if (fromArray->size != toArray->size)
            return std::nullopt;
        return conversionRank(fromArray->element, toArray->element);
    }

    // FIXME: add the abstract result conversion rules
    return std::nullopt;
}

String Type::toString() const
{
    StringPrintStream out;
    dump(out);
    return out.toString();
}

// https://gpuweb.github.io/gpuweb/wgsl/#alignment-and-size
unsigned Type::size() const
{
    return WTF::switchOn(*this,
        [&](const Primitive& primitive) -> unsigned {
            switch (primitive.kind) {
            case Types::Primitive::F32:
            case Types::Primitive::I32:
            case Types::Primitive::U32:
                return 4;
            case Types::Primitive::Bool:
            case Types::Primitive::Void:
            case Types::Primitive::AbstractInt:
            case Types::Primitive::AbstractFloat:
            case Types::Primitive::Sampler:
            case Types::Primitive::TextureExternal:
                RELEASE_ASSERT_NOT_REACHED();
            }
        },
        [&](const Vector& vector) -> unsigned {
            return vector.element->size() * vector.size;
        },
        [&](const Matrix& matrix) -> unsigned {
            // The size of the matrix is computed as: sizeof(array<vecR<T>, C>)
            // sizeof(vecR<T>)
            auto rowSize = matrix.rows * matrix.element->size();
            // sizeof(array<vecR<T>, C>)
            auto rowAlignment = (matrix.rows == 2 ? 2 : 4) * matrix.element->alignment();
            return matrix.columns * WTF::roundUpToMultipleOf(rowAlignment, rowSize);
        },
        [&](const Array& array) -> unsigned {
            ASSERT(array.size.has_value());
            return *array.size * WTF::roundUpToMultipleOf(array.element->alignment(), array.element->size());
        },
        [&](const Struct& structure) -> unsigned {
            unsigned alignment = 0;
            unsigned size = 0;
            for (auto& [_, field] : structure.fields) {
                auto fieldAlignment = field->alignment();
                alignment = std::max(alignment, fieldAlignment);
                size = WTF::roundUpToMultipleOf(fieldAlignment, size);
                size += field->size();
            }
            return WTF::roundUpToMultipleOf(alignment, size);
        },
        [&](const Function&) -> unsigned {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Texture&) -> unsigned {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Reference&) -> unsigned {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Bottom&) -> unsigned {
            RELEASE_ASSERT_NOT_REACHED();
        });
}

unsigned Type::alignment() const
{
    return WTF::switchOn(*this,
        [&](const Primitive& primitive) -> unsigned {
            switch (primitive.kind) {
            case Types::Primitive::F32:
            case Types::Primitive::I32:
            case Types::Primitive::U32:
                return 4;
            case Types::Primitive::Bool:
            case Types::Primitive::Void:
            case Types::Primitive::AbstractInt:
            case Types::Primitive::AbstractFloat:
            case Types::Primitive::Sampler:
            case Types::Primitive::TextureExternal:
                RELEASE_ASSERT_NOT_REACHED();
            }
        },
        [&](const Vector& vector) -> unsigned {
            auto elementAlignment = vector.element->alignment();
            if (vector.size == 2)
                return 2 * elementAlignment;
            return 4 * elementAlignment;
        },
        [&](const Matrix& matrix) -> unsigned {
            auto elementAlignment = matrix.element->alignment();
            if (matrix.rows == 2)
                return 2 * elementAlignment;
            return 4 * elementAlignment;
        },
        [&](const Array& array) -> unsigned {
            return array.element->alignment();
        },
        [&](const Struct& structure) -> unsigned {
            unsigned alignment = 0;
            for (auto& [_, field] : structure.fields)
                alignment = std::max(alignment, field->alignment());
            return alignment;
        },
        [&](const Function&) -> unsigned {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Texture&) -> unsigned {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Reference&) -> unsigned {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Bottom&) -> unsigned {
            RELEASE_ASSERT_NOT_REACHED();
        });
}

bool isPrimitive(const Type* type, Primitive::Kind kind)
{
    auto* primitive = std::get_if<Primitive>(type);
    if (!primitive)
        return false;
    return primitive->kind == kind;
}

bool isPrimitiveReference(const Type* type, Primitive::Kind kind)
{
    auto* reference = std::get_if<Reference>(type);
    if (!reference)
        return false;
    return isPrimitive(reference->element, kind);
}

} // namespace WGSL

namespace WTF {

void printInternal(PrintStream& out, WGSL::AddressSpace addressSpace)
{
    switch (addressSpace) {
    case WGSL::AddressSpace::Function:
        out.print("function");
        return;
    case WGSL::AddressSpace::Private:
        out.print("private");
        return;
    case WGSL::AddressSpace::Workgroup:
        out.print("workgroup");
        return;
    case WGSL::AddressSpace::Uniform:
        out.print("uniform");
        return;
    case WGSL::AddressSpace::Storage:
        out.print("storage");
        return;
    case WGSL::AddressSpace::Handle:
        out.print("handle");
        return;
    }
}

void printInternal(PrintStream& out, WGSL::AccessMode accessMode)
{
    switch (accessMode) {
    case WGSL::AccessMode::Read:
        out.print("read");
        return;
    case WGSL::AccessMode::Write:
        out.print("write");
        return;
    case WGSL::AccessMode::ReadWrite:
        out.print("read_write");
        return;
    }
}

} // namespace WTF
