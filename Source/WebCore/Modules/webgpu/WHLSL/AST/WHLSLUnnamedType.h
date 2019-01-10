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
#include "WHLSLNode.h"
#include "WHLSLType.h"
#include <wtf/UniqueRef.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class UnnamedType : public Type {
public:
    UnnamedType(Lexer::Token&& origin)
        : m_origin(WTFMove(origin))
    {
    }

    virtual ~UnnamedType() = default;

    UnnamedType(const UnnamedType&) = delete;
    UnnamedType(UnnamedType&&) = default;

    bool isUnnamedType() const override { return true; }
    virtual bool isTypeReference() const { return false; }
    virtual bool isPointerType() const { return false; }
    virtual bool isArrayReferenceType() const { return false; }
    virtual bool isArrayType() const { return false; }
    virtual bool isReferenceType() const { return false; }

    virtual const Type& unifyNode() const { return *this; }
    virtual Type& unifyNode() { return *this; }

    virtual UniqueRef<UnnamedType> clone() const = 0;

    const Lexer::Token& origin() const { return m_origin; }

private:
    Lexer::Token m_origin;
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
