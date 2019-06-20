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

#include "WHLSLNameContext.h"
#include "WHLSLVisitor.h"
#include <wtf/HashSet.h>

namespace WebCore {

namespace WHLSL {

class Program;

class NameResolver : public Visitor {
public:
    NameResolver(NameContext&);
    NameResolver(NameResolver&, NameContext&);

    virtual ~NameResolver();

    void visit(AST::FunctionDefinition&) override;

    void setCurrentFunctionDefinition(AST::FunctionDefinition* functionDefinition)
    {
        m_currentFunction = functionDefinition;
    }

    void setIsResolvingCalls(bool isResolvingCalls) { m_isResolvingCalls = isResolvingCalls; }

private:
    void visit(AST::NativeFunctionDeclaration&) override;
    void visit(AST::TypeReference&) override;
    void visit(AST::Block&) override;
    void visit(AST::IfStatement&) override;
    void visit(AST::WhileLoop&) override;
    void visit(AST::DoWhileLoop&) override;
    void visit(AST::ForLoop&) override;
    void visit(AST::VariableDeclaration&) override;
    void visit(AST::VariableReference&) override;
    void visit(AST::Return&) override;
    void visit(AST::PropertyAccessExpression&) override;
    void visit(AST::DotExpression&) override;
    void visit(AST::CallExpression&) override;
    void visit(AST::EnumerationMemberLiteral&) override;

    NameContext& m_nameContext;
    HashSet<AST::TypeReference*> m_typeReferences;
    AST::FunctionDefinition* m_currentFunction { nullptr };
    NameResolver* m_parentNameResolver { nullptr };
    bool m_isResolvingCalls { false };
};

bool resolveNamesInTypes(Program&, NameResolver&);
bool resolveTypeNamesInFunctions(Program&, NameResolver&);
bool resolveCallsInFunctions(Program&, NameResolver&);

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
