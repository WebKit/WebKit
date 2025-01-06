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

#include "config.h"
#include "TransformOperationsSharedPrimitivesPrefix.h"

#include "TransformOperations.h"
#include <algorithm>

namespace WebCore {

void TransformOperationsSharedPrimitivesPrefix::update(const TransformOperations& operations)
{
    size_t maxIteration = operations.size();
    if (m_indexOfFirstMismatch.has_value())
        maxIteration = std::min(*m_indexOfFirstMismatch, maxIteration);

    for (size_t i = 0; i < maxIteration; ++i) {
        Ref operation = operations[i];

        // If we haven't seen an operation at this index before, we can simply use our primitive type.
        if (i >= m_primitives.size()) {
            ASSERT(i == m_primitives.size());
            m_primitives.append(operation->primitiveType());
            continue;
        }

        if (auto sharedPrimitive = operation->sharedPrimitiveType(m_primitives[i]))
            m_primitives[i] = *sharedPrimitive;
        else {
            m_indexOfFirstMismatch = i;
            m_primitives.shrink(i);
            return;
        }
    }
}

} // namespace WebCore
