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
#include "Type.h"

#include "Array.h"
#include "Matrix.h"
#include "Vector.h"

namespace WGSL::Semantics::Types {

Type::Type(Kind kind, SourceSpan span)
    : m_kind(kind)
    , m_span(span)
{
}

bool Type::isIntegerScalar() const
{
    return isI32() || isU32();
}

// TODO for code review: should this logic be implemented entirely here,
// or should we delegate the logic to individual types.
bool Type::isConcrete() const
{
    // A concrete type is not a abstract numberic type...
    if (isAbstractInt() || isAbstractFloat())
        return false;

    // nor a container type that contains an abstract numberic type...

    // vector without abstract numeric elements.
    if (isVector()) {
        const auto& vectorThis = downcast<Vector>(*this);
        return !vectorThis.componentType().isAbstractNumeric();
    }

    // Matrix without abstract numeric elements.
    if (isMatrix()) {
        const auto& matrixThis = downcast<Matrix>(*this);
        return !matrixThis.componentType().isAbstractNumeric();
    }

    // atomic
    if (isAtomic())
        return true;

    // fixed-size array without abstract numeric elements.
    if (isArray()) {
        const auto& arrayThis = downcast<Array>(*this);
        return !arrayThis.elementType().isAbstractNumeric();
    }

    // structure, when all members are not abstract numeric elements.
    // TODO: implement this.
    if (isStruct())
        return true;

    return false;
}

bool Type::isScalar() const
{
    return isBool() || isI32() || isU32() || isF32() || isF16();
}

bool Type::isAbstractNumeric() const
{
    return isAbstractInt() || isAbstractFloat();
}

// TODO for code review: should this logic be implemented entirely here,
// or should we delegate the logic to individual types.
bool Type::hasCreationFixedFootprint() const
{
    // A type has creation-fixed footprint when it's a ...

    // scalar type
    if (isScalar())
        return true;

    // vector with concrete components
    if (isVector()) {
        const auto& vectorThis = downcast<Vector>(*this);
        return vectorThis.componentType().isConcrete();
    }

    // matrix with concrete components
    if (isMatrix()) {
        const auto& matrixThis = downcast<Matrix>(*this);
        return matrixThis.componentType().isConcrete();
    }

    // atomic
    if (isAtomic())
        return true;

    // fixed-size array, when the element count is a creation-time
    // expression.
    // TODO: fix this when we support pipeline overridables as element count.
    if (isArray())
        return true;

    // structure, when all members has creation-fixed footprint.
    // TODO: implement this.
    if (isStruct())
        return true;

    return false;
}

} // namespace WGSL::Semantics::Type
