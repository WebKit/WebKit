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
#include "WHLSLNamedType.h"
#include "WHLSLNode.h"
#include "WHLSLUnnamedType.h"
#include <wtf/UniqueRef.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class TypeDefinition : public NamedType {
public:
    TypeDefinition(Lexer::Token&& origin, String&& name, UniqueRef<UnnamedType>&& type)
        : NamedType(WTFMove(origin), WTFMove(name))
        , m_type(WTFMove(type))
    {
    }

    virtual ~TypeDefinition() = default;

    TypeDefinition(const TypeDefinition&) = delete;
    TypeDefinition(TypeDefinition&&) = default;

    bool isTypeDefinition() const override { return true; }

    UnnamedType& type() { return static_cast<UnnamedType&>(m_type); }

    const Type& unifyNode() const override
    {
        return m_type->unifyNode();
    }

    Type& unifyNode() override
    {
        return m_type->unifyNode();
    }

private:
    UniqueRef<UnnamedType> m_type;
};

} // namespace AST

}

}

SPECIALIZE_TYPE_TRAITS_WHLSL_NAMED_TYPE(TypeDefinition, isTypeDefinition())

#endif
