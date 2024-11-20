/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "TransformFunctionType.h"
#include <algorithm>
#include <optional>
#include <wtf/Vector.h>

namespace WebCore {

// This class is used to find a shared prefix of transform function primitives (as
// defined by CSS Transforms Level 1 & 2). Given a series of `TransformList`s in
// the keyframes of an animation. After `update()` is called with the `TransformList`
// of every keyframe, `primitives()` will return the prefix of primitives that are shared
// by all keyframes passed to `update()`.

// This adapter allows using `TransformListSharedPrimitivesPrefix` multiple transform types.
template<typename Op> struct PrimitiveTransformType;
template<typename Op> TransformFunctionType primitiveTransformType(const Op& op)
{
    return PrimitiveTransformType<Op>{}(op);
}

class TransformListSharedPrimitivesPrefix final {
public:
    static std::optional<TransformFunctionType> sharedPrimitive(TransformFunctionType, TransformFunctionType);

    template<typename T> void update(const T&);
    bool hadIncompatibleTransformFunctions() { return m_indexOfFirstMismatch.has_value(); }
    const Vector<TransformFunctionType>& primitives() const { return m_primitives; }

private:
    std::optional<size_t> m_indexOfFirstMismatch;
    Vector<TransformFunctionType> m_primitives;
};

template<typename T> void TransformListSharedPrimitivesPrefix::update(const T& transform)
{
    unsigned i = 0;
    for (auto operation : transform) {
        if (i == m_indexOfFirstMismatch)
            return;

        auto primitive = WebCore::primitiveTransformType(operation);

        // If we haven't seen an operation at this index before, we can simply use our primitive type.
        if (i >= m_primitives.size()) {
            ASSERT(i == m_primitives.size());
            m_primitives.append(primitive);
            ++i;
            continue;
        }

        if (auto newSharedPrimitive = sharedPrimitive(primitive, m_primitives[i])) {
            m_primitives[i] = *newSharedPrimitive;
            ++i;
            continue;
        }

        m_indexOfFirstMismatch = i;
        m_primitives.shrink(i);
        return;
    }
}

} // namespace WebCore
