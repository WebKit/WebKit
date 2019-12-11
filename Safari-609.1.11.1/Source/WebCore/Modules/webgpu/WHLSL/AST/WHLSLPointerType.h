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

#include "WHLSLCodeLocation.h"
#include "WHLSLReferenceType.h"
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>
#include <wtf/UniqueRef.h>
#include <wtf/text/StringConcatenate.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class PointerType final : public ReferenceType {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(PointerType);
    using Base = ReferenceType;

    PointerType(CodeLocation location, AddressSpace addressSpace, Ref<UnnamedType> elementType)
        : Base(location, addressSpace, WTFMove(elementType), Kind::Pointer)
    {
    }

public:
    static Ref<PointerType> create(CodeLocation location, AddressSpace addressSpace, Ref<UnnamedType> elementType)
    {
        auto result = adoptRef(*new PointerType(location, addressSpace, WTFMove(elementType)));
        return result;
    }

    ~PointerType() = default;

    unsigned hash() const
    {
        return this->Base::hash() ^ StringHasher::computeLiteralHash("pointer");
    }

    bool operator==(const PointerType& other) const
    {
        return addressSpace() == other.addressSpace() && elementType() == other.elementType();
    }

    String toString() const
    {
        return makeString(elementType().toString(), '*');
    }
};

} // namespace AST

}

}

DEFINE_DEFAULT_DELETE(PointerType)

SPECIALIZE_TYPE_TRAITS_WHLSL_TYPE(PointerType, isPointerType())

#endif
