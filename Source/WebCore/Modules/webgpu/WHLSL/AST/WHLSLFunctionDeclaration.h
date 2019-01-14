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

#include "WHLSLEntryPointType.h"
#include "WHLSLFunctionAttribute.h"
#include "WHLSLLexer.h"
#include "WHLSLNode.h"
#include "WHLSLSemantic.h"
#include "WHLSLUnnamedType.h"
#include "WHLSLVariableDeclaration.h"
#include <wtf/UniqueRef.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class FunctionDeclaration : public Node {
public:
    FunctionDeclaration(Lexer::Token&& origin, AttributeBlock&& attributeBlock, Optional<EntryPointType> entryPointType, UniqueRef<UnnamedType>&& type, String&& name, VariableDeclarations&& parameters, Optional<Semantic>&& semantic, bool isOperator)
        : m_origin(WTFMove(origin))
        , m_attributeBlock(WTFMove(attributeBlock))
        , m_entryPointType(entryPointType)
        , m_type(WTFMove(type))
        , m_name(WTFMove(name))
        , m_parameters(WTFMove(parameters))
        , m_semantic(WTFMove(semantic))
        , m_isOperator(WTFMove(isOperator))
    {
    }

    virtual ~FunctionDeclaration() = default;

    FunctionDeclaration(const FunctionDeclaration&) = delete;
    FunctionDeclaration(FunctionDeclaration&&) = default;

    virtual bool isFunctionDefinition() const { return false; }
    virtual bool isNativeFunctionDeclaration() const { return false; }

    AttributeBlock& attributeBlock() { return m_attributeBlock; }
    const Optional<EntryPointType>& entryPointType() const { return m_entryPointType; }
    const UnnamedType& type() const { return m_type; }
    UnnamedType& type() { return m_type; }
    const String& name() const { return m_name; }
    bool isCast() const { return m_name == "operator cast"; }
    const VariableDeclarations& parameters() const { return m_parameters; }
    VariableDeclarations& parameters() { return m_parameters; }
    Optional<Semantic>& semantic() { return m_semantic; }
    bool isOperator() const { return m_isOperator; }

private:
    Lexer::Token m_origin;
    AttributeBlock m_attributeBlock;
    Optional<EntryPointType> m_entryPointType;
    UniqueRef<UnnamedType> m_type;
    String m_name;
    VariableDeclarations m_parameters;
    Optional<Semantic> m_semantic;
    bool m_isOperator;
};

} // namespace AST

}

}

#define SPECIALIZE_TYPE_TRAITS_WHLSL_FUNCTION_DECLARATION(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WHLSL::AST::ToValueTypeName) \
    static bool isType(const WebCore::WHLSL::AST::FunctionDeclaration& type) { return type.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif
