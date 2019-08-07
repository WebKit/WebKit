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
#include "WHLSLType.h"
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>
#include <wtf/UniqueRef.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class UnnamedType : public Type, public RefCounted<UnnamedType> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(UnnamedType);
public:
    enum class Kind {
        TypeReference,
        PointerType,
        ArrayReferenceType,
        ArrayType
    };

    UnnamedType(CodeLocation location, Kind kind)
        : m_codeLocation(location)
        , m_kind(kind)
    {
    }

    virtual ~UnnamedType() = default;

    bool isUnnamedType() const override { return true; }

    Kind kind() const { return m_kind; }
    bool isTypeReference() const { return m_kind == Kind::TypeReference; }
    bool isPointerType() const { return m_kind == Kind::PointerType; }
    bool isArrayReferenceType() const { return m_kind == Kind::ArrayReferenceType; }
    bool isArrayType() const { return m_kind == Kind::ArrayType; }
    bool isReferenceType() const { return isPointerType() || isArrayReferenceType(); }

    virtual const Type& unifyNode() const { return *this; }
    virtual Type& unifyNode() { return *this; }

    unsigned hash() const;
    bool operator==(const UnnamedType&) const;

    virtual String toString() const = 0;

    const CodeLocation& codeLocation() const { return m_codeLocation; }

private:
    CodeLocation m_codeLocation;
    Kind m_kind;
};

} // namespace AST

}

}

#define SPECIALIZE_TYPE_TRAITS_WHLSL_UNNAMED_TYPE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WHLSL::AST::ToValueTypeName) \
    static bool isType(const WebCore::WHLSL::AST::UnnamedType& type) { return type.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_WHLSL_TYPE(UnnamedType, isUnnamedType())

#endif
