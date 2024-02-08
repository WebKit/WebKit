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
#include "WGSLShaderModule.h"
#include <wtf/Deque.h>

namespace WGSL {

CallGraph::CallGraph(ShaderModule& shaderModule)
    : m_ast(shaderModule)
{
}

class CallGraphBuilder : public AST::Visitor {
public:
    CallGraphBuilder(ShaderModule& shaderModule, const HashMap<String, std::optional<PipelineLayout>>& pipelineLayouts, HashMap<String, Reflection::EntryPointInformation>& entryPoints)
        : m_callGraph(shaderModule)
        , m_pipelineLayouts(pipelineLayouts)
        , m_entryPoints(entryPoints)
    {
    }

    CallGraph build();

    void visit(AST::Function&) override;
    void visit(AST::CallExpression&) override;

private:
    void initializeMappings();

    CallGraph m_callGraph;
    const HashMap<String, std::optional<PipelineLayout>>& m_pipelineLayouts;
    HashMap<String, Reflection::EntryPointInformation>& m_entryPoints;
    HashMap<AST::Function*, unsigned> m_calleeBuildingMap;
    Vector<CallGraph::Callee>* m_callees { nullptr };
    Deque<AST::Function*> m_queue;
};

CallGraph CallGraphBuilder::build()
{
    initializeMappings();
    return m_callGraph;
}

void CallGraphBuilder::initializeMappings()
{
    for (auto& declaration : m_callGraph.m_ast.declarations()) {
        if (!is<AST::Function>(declaration))
            continue;

        auto& function = downcast<AST::Function>(declaration);
        const auto& name = function.name();
        {
            auto result = m_callGraph.m_functionsByName.add(name, &function);
            ASSERT_UNUSED(result, result.isNewEntry);
        }

        if (!m_pipelineLayouts.contains(name))
            continue;

        if (!function.stage())
            continue;

        auto addResult = m_entryPoints.add(function.name(), Reflection::EntryPointInformation { });
        ASSERT(addResult.isNewEntry);
        m_callGraph.m_entrypoints.append({ function, *function.stage(), addResult.iterator->value });
        m_queue.append(&function);
    }

    while (!m_queue.isEmpty())
        visit(*m_queue.takeFirst());
}

void CallGraphBuilder::visit(AST::Function& function)
{
    auto result = m_callGraph.m_calleeMap.add(&function, Vector<CallGraph::Callee>());
    if (!result.isNewEntry)
        return;

    ASSERT(!m_callees);
    m_callees = &result.iterator->value;
    AST::Visitor::visit(function);
    m_calleeBuildingMap.clear();
    m_callees = nullptr;
}

void CallGraphBuilder::visit(AST::CallExpression& call)
{
    for (auto& argument : call.arguments())
        AST::Visitor::visit(argument);

    if (!is<AST::IdentifierExpression>(call.target()))
        return;

    auto& target = downcast<AST::IdentifierExpression>(call.target());
    auto it = m_callGraph.m_functionsByName.find(target.identifier());
    if (it == m_callGraph.m_functionsByName.end())
        return;

    m_queue.append(it->value);
    auto result = m_calleeBuildingMap.add(it->value, m_callees->size());
    if (result.isNewEntry)
        m_callees->append(CallGraph::Callee { it->value, { &call } });
    else
        m_callees->at(result.iterator->value).callSites.append(&call);
}

CallGraph buildCallGraph(ShaderModule& shaderModule, const HashMap<String, std::optional<PipelineLayout>>& pipelineLayouts, HashMap<String, Reflection::EntryPointInformation>& entryPoints)
{
    return CallGraphBuilder(shaderModule, pipelineLayouts, entryPoints).build();
}

} // namespace WGSL
