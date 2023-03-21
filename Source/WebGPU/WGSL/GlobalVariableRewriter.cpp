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
#include "GlobalVariableRewriter.h"

#include "AST.h"
#include "ASTIdentifier.h"
#include "ASTVisitor.h"
#include "CallGraph.h"
#include "WGSL.h"
#include "WGSLShaderModule.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace WGSL {

class RewriteGlobalVariables : public AST::Visitor {
public:
    RewriteGlobalVariables(CallGraph& callGraph, const HashMap<String, PipelineLayout>& pipelineLayouts, PrepareResult& result)
        : AST::Visitor()
        , m_callGraph(callGraph)
        , m_result(result)
    {
        UNUSED_PARAM(pipelineLayouts);
    }

    void run();

    void visit(AST::Function&) override;
    void visit(AST::Variable&) override;
    void visit(AST::IdentifierExpression&) override;

private:
    template<typename Value>
    using IndexMap = HashMap<unsigned, Value, WTF::IntHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>>;

    using IndexSet = HashSet<unsigned, WTF::IntHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>>;

    struct Global {
        struct Resource {
            unsigned group;
            unsigned binding;
        };

        std::optional<Resource> resource;
        AST::Variable* declaration;
    };

    static AST::Identifier argumentBufferParameterName(unsigned group);
    static AST::Identifier argumentBufferStructName(unsigned group);

    void def(const String&);
    Global* read(const String&);

    void collectGlobals();
    void insertStructs();
    void visitEntryPoint(AST::Function&, AST::StageAttribute::Stage, PipelineLayout&);
    IndexSet requiredGroups(PipelineLayout&, AST::StageAttribute::Stage);
    void insertParameters(AST::Function&, const IndexSet& requiredGroups);

    CallGraph& m_callGraph;
    PrepareResult& m_result;
    HashMap<String, Global> m_globals;
    IndexMap<IndexMap<Global*>> m_groupBindingMap;
    HashSet<String> m_defs;
    HashSet<String> m_reads;
};

void RewriteGlobalVariables::run()
{
    collectGlobals();
    insertStructs();
    for (auto& entryPoint : m_callGraph.entrypoints()) {
        PipelineLayout pipelineLayout;
        visitEntryPoint(entryPoint.function, entryPoint.stage, pipelineLayout);

        auto it = m_result.entryPoints.find(entryPoint.function.name());
        ASSERT(it != m_result.entryPoints.end());
        it->value.defaultLayout = WTFMove(pipelineLayout);
    }
}

void RewriteGlobalVariables::visit(AST::Function& function)
{
    // FIXME: visit callee once we have any

    for (auto& parameter : function.parameters())
        def(parameter.name());

    // FIXME: detect when we shadow a global that a callee needs
    AST::Visitor::visit(function.body());
}

void RewriteGlobalVariables::visit(AST::Variable& variable)
{
    def(variable.name());
}

void RewriteGlobalVariables::visit(AST::IdentifierExpression& identifier)
{
    auto name = identifier.identifier();
    if (Global* global = read(name)) {
        if (auto resource = global->resource) {
            auto base = makeUniqueRef<AST::IdentifierExpression>(identifier.span(), argumentBufferParameterName(resource->group));
            auto structureAccess = makeUniqueRef<AST::FieldAccessExpression>(identifier.span(), WTFMove(base), WTFMove(name));
            m_callGraph.ast().replace(&identifier, AST::IdentityExpression(identifier.span(), WTFMove(structureAccess)));
        }
    }
}

void RewriteGlobalVariables::collectGlobals()
{
    auto& globalVars = m_callGraph.ast().variables();
    for (auto& globalVar : globalVars) {
        std::optional<unsigned> group;
        std::optional<unsigned> binding;
        for (auto& attribute : globalVar.attributes()) {
            if (is<AST::GroupAttribute>(attribute)) {
                group = { downcast<AST::GroupAttribute>(attribute).group() };
                continue;
            }
            if (is<AST::BindingAttribute>(attribute)) {
                binding = { downcast<AST::BindingAttribute>(attribute).binding() };
                continue;
            }
        }

        std::optional<Global::Resource> resource;
        if (group.has_value()) {
            RELEASE_ASSERT(binding.has_value());
            resource = { *group, *binding };
        }

        auto result = m_globals.add(globalVar.name(), Global {
            resource,
            &globalVar
        });
        ASSERT(result, result.isNewEntry);

        if (resource.has_value()) {
            Global& global = result.iterator->value;
            auto result = m_groupBindingMap.add(resource->group, IndexMap<Global*>());
            result.iterator->value.add(resource->binding, &global);
        }
    }
}

void RewriteGlobalVariables::visitEntryPoint(AST::Function& function, AST::StageAttribute::Stage stage, PipelineLayout& pipelineLayout)
{
    m_reads.clear();
    m_defs.clear();

    visit(function);
    if (m_reads.isEmpty())
        return;

    auto groupsToGenerate = requiredGroups(pipelineLayout, stage);
    // FIXME: not all globals are parameters
    insertParameters(function, groupsToGenerate);
}


auto RewriteGlobalVariables::requiredGroups(PipelineLayout& pipelineLayout, AST::StageAttribute::Stage stage) -> IndexSet
{
    IndexSet groups;
    for (const auto& globalName : m_reads) {
        auto it = m_globals.find(globalName);
        RELEASE_ASSERT(it != m_globals.end());
        auto& global = it->value;
        if (!global.resource.has_value())
            continue;
        auto group = global.resource->group;
        groups.add(group);

        if (pipelineLayout.bindGroupLayouts.size() <= group)
            pipelineLayout.bindGroupLayouts.grow(group + 1);

        ShaderStage shaderStage;
        switch (stage) {
        case AST::StageAttribute::Stage::Compute:
            shaderStage = ShaderStage::Compute;
            break;
        case AST::StageAttribute::Stage::Vertex:
            shaderStage = ShaderStage::Vertex;
            break;
        case AST::StageAttribute::Stage::Fragment:
            shaderStage = ShaderStage::Fragment;
            break;
        }
        // FIXME: we need to check for an existing entry with the same binding
        pipelineLayout.bindGroupLayouts[group].entries.append({
            global.resource->binding,
            shaderStage,
            // FIXME: add the missing bindingMember information
            { }
        });
    }
    return groups;
}

void RewriteGlobalVariables::insertStructs()
{
    for (auto& groupBinding : m_groupBindingMap) {
        unsigned group = groupBinding.key;
        const auto& bindingGlobalMap = groupBinding.value;

        AST::Identifier structName = argumentBufferStructName(group);
        AST::StructureMember::List structMembers;

        for (auto& bindingGlobal : bindingGlobalMap) {
            unsigned binding = bindingGlobal.key;
            auto& global = *bindingGlobal.value;

            ASSERT(global.declaration->maybeTypeName());
            auto span = global.declaration->span();
            structMembers.append(makeUniqueRef<AST::StructureMember>(
                span,
                AST::Identifier::make(global.declaration->name()),
                adoptRef(*new AST::ReferenceTypeName(span, *global.declaration->maybeTypeName())),
                AST::Attribute::List {
                    adoptRef(*new AST::BindingAttribute(span, binding))
                }
            ));
        }

        m_callGraph.ast().append(m_callGraph.ast().structures(), makeUniqueRef<AST::Structure>(
            SourceSpan::empty(),
            WTFMove(structName),
            WTFMove(structMembers),
            AST::Attribute::List { },
            AST::StructureRole::BindGroup
        ));
    }
}

void RewriteGlobalVariables::insertParameters(AST::Function& function, const IndexSet& requiredGroups)
{
    auto span = function.span();
    for (unsigned group : requiredGroups) {
        m_callGraph.ast().append(function.parameters(), adoptRef(*new AST::Parameter(
            span,
            argumentBufferParameterName(group),
            adoptRef(*new AST::NamedTypeName(span, argumentBufferStructName(group))),
            AST::Attribute::List {
                adoptRef(*new AST::GroupAttribute(span, group))
            },
            AST::ParameterRole::BindGroup
        )));
    }
}

void RewriteGlobalVariables::def(const String& name)
{
    m_defs.add(name);
}

auto RewriteGlobalVariables::read(const String& name) -> Global*
{
    if (m_defs.contains(name))
        return nullptr;
    auto it = m_globals.find(name);
    if (it == m_globals.end())
        return nullptr;
    m_reads.add(name);
    return &it->value;
}

AST::Identifier RewriteGlobalVariables::argumentBufferParameterName(unsigned group)
{
    return AST::Identifier::make(makeString("__ArgumentBufer_", String::number(group)));
}

AST::Identifier RewriteGlobalVariables::argumentBufferStructName(unsigned group)
{
    return AST::Identifier::make(makeString("__ArgumentBuferT_", String::number(group)));
}

void rewriteGlobalVariables(CallGraph& callGraph, const HashMap<String, PipelineLayout>& pipelineLayouts, PrepareResult& result)
{
    RewriteGlobalVariables(callGraph, pipelineLayouts, result).run();
}

} // namespace WGSL
