/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Matrix.h"

namespace WGSL::Semantics::Types {

static std::optional<Error> checkValidComponentType(const Type& type, SourceSpan span)
{
    if (!type.isF32() && !type.isF16() && !type.isAbstractFloat()) {
        auto err = makeString(
            "matrix component type '",
            type.string(),
            "' is neither f32, f15, nor abstract float"
        );

        return Error { WTFMove(err), span };
    }

    return { };
}

static std::optional<Error> checkValidColCount(uint8_t size, SourceSpan span)
{
    if (size != 2 && size != 3 && size != 4)
        return { Error { "matrix column size must either be 2, 3, or 4"_s, span } };

    return { };
}

static std::optional<Error> checkValidRowCount(uint8_t size, SourceSpan span)
{
    if (size != 2 && size != 3 && size != 4)
        return { Error { "matrix row size must either be 2, 3, or 4"_s, span } };

    return { };
}

Matrix::Matrix(SourceSpan span, UniqueRef<Type> componentType, uint8_t colCount, uint8_t rowCount)
    : Type(Type::Kind::Matrix, span)
    , m_componentType(WTFMove(componentType))
    , m_colCount(colCount)
    , m_rowCount(rowCount)
{
}

Expected<UniqueRef<Type>, Error> Matrix::tryCreate(SourceSpan span, UniqueRef<Type> componentType, uint8_t colCount, uint8_t rowCount)
{
    if (auto err = checkValidComponentType(componentType, span))
        return makeUnexpected(*err);

    if (auto err = checkValidColCount(colCount, span))
        return makeUnexpected(*err);

    if (auto err = checkValidRowCount(rowCount, span))
        return makeUnexpected(*err);

    // return { makeUniqueRef<Matrix>(span, WTFMove(componentType), colCount, rowCount) };
    return { UniqueRef(*new Matrix(span, WTFMove(componentType), colCount, rowCount)) };
}

bool Matrix::operator==(const Type& other) const
{
    if (!other.isMatrix())
        return false;

    const auto& matrixOther = downcast<Matrix>(other);
    return (m_componentType.get() == matrixOther.m_componentType.get())
        && (m_colCount == matrixOther.m_colCount)
        && (m_rowCount == matrixOther.m_rowCount);
}

String Matrix::string() const
{
    return makeString("mat", toString(m_colCount), "x", toString(m_rowCount), "<", m_componentType->string(), ">");
}

}
