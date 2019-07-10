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
#include "WHLSLReferenceType.h"
#include <wtf/UniqueRef.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class ArrayReferenceType : public ReferenceType {
    using Base = ReferenceType;
public:
    ArrayReferenceType(CodeLocation location, AddressSpace addressSpace, UniqueRef<UnnamedType>&& elementType)
        : Base(location, addressSpace, WTFMove(elementType))
    {
    }

    virtual ~ArrayReferenceType() = default;

    ArrayReferenceType(const ArrayReferenceType&) = delete;
    ArrayReferenceType(ArrayReferenceType&&) = default;

    bool isArrayReferenceType() const override { return true; }

    UniqueRef<UnnamedType> clone() const override
    {
        return makeUniqueRef<ArrayReferenceType>(codeLocation(), addressSpace(), elementType().clone());
    }

    unsigned hash() const override
    {
        return this->Base::hash() ^ StringHasher::computeLiteralHash("array");
    }

    bool operator==(const UnnamedType& other) const override
    {
        if (!is<ArrayReferenceType>(other))
            return false;

        return addressSpace() == downcast<ArrayReferenceType>(other).addressSpace()
            && elementType() == downcast<ArrayReferenceType>(other).elementType();
    }

private:
};

} // namespace AST

}

}

SPECIALIZE_TYPE_TRAITS_WHLSL_UNNAMED_TYPE(ArrayReferenceType, isArrayReferenceType())

#endif
