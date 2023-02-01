/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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
#include "CallGraph.h"

#include "AST.h"
#include "ASTVisitor.h"

namespace WGSL {

CallGraph::CallGraph(AST::ShaderModule& shaderModule)
    : m_ast(shaderModule)
{
}

class CallGraphBuilder : public AST::Visitor {
public:
    CallGraphBuilder(AST::ShaderModule& shaderModule)
        : m_callGraph(shaderModule)
    {
    }

    CallGraph build();

    // FIXME: we also need to visit function calls when we add support for them
    void visit(AST::Function&) override;

private:
    void initializeMappings();

    CallGraph m_callGraph;
    AST::Function* m_currentFunction;
};

CallGraph CallGraphBuilder::build()
{
    initializeMappings();
    return m_callGraph;
}

void CallGraphBuilder::initializeMappings()
{
    for (auto& functionDecl : m_callGraph.m_ast.functions()) {
        const auto& name = functionDecl.name();
        {
            auto result = m_callGraph.m_functionsByName.add(name, &functionDecl);
            ASSERT_UNUSED(result, result.isNewEntry);
        }

        {
            auto result = m_callGraph.m_callees.add(&functionDecl, Vector<CallGraph::Callee>());
            ASSERT_UNUSED(result, result.isNewEntry);
        }

        for (auto& attribute : functionDecl.attributes()) {
            if (is<AST::StageAttribute>(attribute)) {
                auto stage = downcast<AST::StageAttribute>(attribute).stage();
                m_callGraph.m_entrypoints.append({ functionDecl, stage });
                break;
            }
        }
    }
}

void CallGraphBuilder::visit(AST::Function& functionDecl)
{
    m_currentFunction = &functionDecl;
    checkErrorAndVisit(functionDecl.body());
    m_currentFunction = nullptr;
}

CallGraph buildCallGraph(AST::ShaderModule& shaderModule)
{
    return CallGraphBuilder(shaderModule).build();
}

} // namespace WGSL
