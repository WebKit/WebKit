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
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace WGSL {

class RewriteGlobalVariables : public AST::Visitor {
public:
    RewriteGlobalVariables(CallGraph& callGraph, const HashMap<String, PipelineLayout>& pipelineLayouts)
        : AST::Visitor()
        , m_callGraph(callGraph)
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
            unsigned m_group;
            unsigned m_binding;
        };

        std::optional<Resource> m_resource;
        AST::Variable* m_declaration;
    };

    static AST::Identifier argumentBufferParameterName(unsigned group);
    static AST::Identifier argumentBufferStructName(unsigned group);

    void def(const String&);
    Global* read(const String&);

    void collectGlobals();
    void insertStructs();
    void visitEntryPoint(AST::Function&);
    IndexSet requiredGroups();
    void insertParameters(AST::Function&, const IndexSet& requiredGroups);

    CallGraph& m_callGraph;
    HashMap<String, Global> m_globals;
    IndexMap<IndexMap<Global*>> m_groupBindingMap;
    HashSet<String> m_defs;
    HashSet<String> m_reads;
};

void RewriteGlobalVariables::run()
{
    collectGlobals();
    insertStructs();
    for (auto& entryPoint : m_callGraph.entrypoints())
        visitEntryPoint(entryPoint.m_function);
    m_callGraph.ast().variables().clear();
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

template <typename TargetType, typename CurrentType, typename... Arguments>
void replace(CurrentType&& dst, Arguments&&... arguments)
{
    static_assert(sizeof(TargetType) <= sizeof(CurrentType));
    auto span = dst.span();
    dst.~CurrentType();
    new (&dst) TargetType(span, std::forward<Arguments>(arguments)...);
}

void RewriteGlobalVariables::visit(AST::IdentifierExpression& identifier)
{
    auto name = identifier.identifier();
    if (Global* global = read(name)) {
        if (auto resource = global->m_resource) {
            auto base = makeUniqueRef<AST::IdentifierExpression>(identifier.span(), argumentBufferParameterName(resource->m_group));
            auto structureAccess = makeUniqueRef<AST::FieldAccessExpression>(identifier.span(), WTFMove(base), WTFMove(name));
            replace<AST::IdentityExpression>(WTFMove(identifier), WTFMove(structureAccess));
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
            auto result = m_groupBindingMap.add(resource->m_group, IndexMap<Global*>());
            result.iterator->value.add(resource->m_binding, &global);
        }
    }
}

void RewriteGlobalVariables::visitEntryPoint(AST::Function& function)
{
    m_reads.clear();
    m_defs.clear();

    visit(function);
    if (m_reads.isEmpty())
        return;

    auto groupsToGenerate = requiredGroups();
    // FIXME: not all globals are parameters
    insertParameters(function, groupsToGenerate);
}


auto RewriteGlobalVariables::requiredGroups() -> IndexSet
{
    IndexSet groups;
    for (const auto& globalName : m_reads) {
        auto it = m_globals.find(globalName);
        RELEASE_ASSERT(it != m_globals.end());
        auto& global = it->value;
        if (!global.m_resource.has_value())
            continue;
        groups.add(global.m_resource->m_group);
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

            ASSERT(global.m_declaration->maybeTypeName());
            auto span = global.m_declaration->span();
            structMembers.append(makeUniqueRef<AST::StructureMember>(
                span,
                AST::Identifier::make(global.m_declaration->name()),
                adoptRef(*new AST::ReferenceTypeName(span, *global.m_declaration->maybeTypeName())),
                AST::Attribute::List {
                    adoptRef(*new AST::BindingAttribute(span, binding))
                }
            ));
        }

        m_callGraph.ast().structures().append(makeUniqueRef<AST::Structure>(
            SourceSpan(0, 0, 0, 0),
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
        function.parameters().append(makeUniqueRef<AST::ParameterValue>(
            span,
            argumentBufferParameterName(group),
            adoptRef(*new AST::NamedTypeName(span, argumentBufferStructName(group))),
            AST::Attribute::List {
                adoptRef(*new AST::GroupAttribute(span, group))
            },
            AST::ParameterRole::BindGroup
        ));
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

void rewriteGlobalVariables(CallGraph& callGraph, const HashMap<String, PipelineLayout>& pipelineLayouts)
{
    RewriteGlobalVariables(callGraph, pipelineLayouts).run();
}

} // namespace WGSL
