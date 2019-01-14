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

#include "WHLSLExpression.h"
#include "WHLSLLexer.h"
#include "WHLSLQualifier.h"
#include "WHLSLSemantic.h"
#include "WHLSLType.h"
#include "WHLSLValue.h"
#include <memory>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class VariableDeclaration : public Value {
public:
    VariableDeclaration(Lexer::Token&& origin, Qualifiers&& qualifiers, Optional<UniqueRef<UnnamedType>>&& type, String&& name, Optional<Semantic>&& semantic, Optional<UniqueRef<Expression>>&& initializer)
        : m_origin(WTFMove(origin))
        , m_qualifiers(WTFMove(qualifiers))
        , m_type(WTFMove(type))
        , m_name(WTFMove(name))
        , m_semantic(WTFMove(semantic))
        , m_initializer(WTFMove(initializer))
    {
    }

    virtual ~VariableDeclaration() = default;

    VariableDeclaration(const VariableDeclaration&) = delete;
    VariableDeclaration(VariableDeclaration&&) = default;

    const Lexer::Token& origin() const { return m_origin; }
    String& name() { return m_name; }

    const Optional<UniqueRef<UnnamedType>>& type() const { return m_type; } // Anonymous variables inside ReadModifyWriteExpressions have their type set by the type checker.
    UnnamedType* type() { return m_type ? &*m_type : nullptr; }
    Optional<Semantic>& semantic() { return m_semantic; }
    Expression* initializer() { return m_initializer ? &*m_initializer : nullptr; }
    bool isAnonymous() const { return m_name.isNull(); }

private:
    Lexer::Token m_origin;
    Qualifiers m_qualifiers;
    Optional<UniqueRef<UnnamedType>> m_type;
    String m_name;
    Optional<Semantic> m_semantic;
    Optional<UniqueRef<Expression>> m_initializer;
};

using VariableDeclarations = Vector<VariableDeclaration>;

} // namespace AST

}

}

#endif
