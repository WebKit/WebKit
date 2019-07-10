/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBGPU)

#include "WHLSLLexer.h"
#include "WHLSLTypeArgument.h"
#include "WHLSLUnnamedType.h"
#include <wtf/UniqueRef.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class ArrayType : public UnnamedType {
public:
    ArrayType(CodeLocation location, UniqueRef<UnnamedType>&& elementType, unsigned numElements)
        : UnnamedType(location)
        , m_elementType(WTFMove(elementType))
        , m_numElements(numElements)
    {
    }

    virtual ~ArrayType() = default;

    ArrayType(const ArrayType&) = delete;
    ArrayType(ArrayType&&) = default;

    bool isArrayType() const override { return true; }

    const UnnamedType& type() const { return m_elementType; }
    UnnamedType& type() { return m_elementType; }
    unsigned numElements() const { return m_numElements; }

    UniqueRef<UnnamedType> clone() const override
    {
        return makeUniqueRef<ArrayType>(codeLocation(), m_elementType->clone(), m_numElements);
    }

    unsigned hash() const override
    {
        return WTF::IntHash<unsigned>::hash(m_numElements) ^ m_elementType->hash();
    }

    bool operator==(const UnnamedType& other) const override
    {
        if (!is<ArrayType>(other))
            return false;

        return numElements() == downcast<ArrayType>(other).numElements()
            && type() == downcast<ArrayType>(other).type();
    }

private:
    UniqueRef<UnnamedType> m_elementType;
    unsigned m_numElements;
};

} // namespace AST

}

}

SPECIALIZE_TYPE_TRAITS_WHLSL_UNNAMED_TYPE(ArrayType, isArrayType())

#endif
