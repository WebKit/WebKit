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
    RewriteGlobalVariables(CallGraph& callGraph, const HashMap<String, std::optional<PipelineLayout>>& pipelineLayouts, PrepareResult& result)
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
    struct Global {
        struct Resource {
            unsigned group;
            unsigned binding;
        };

        std::optional<Resource> resource;
        AST::Variable* declaration;
    };

    template<typename Value>
    using IndexMap = HashMap<unsigned, Value, WTF::IntHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>>;

    using UsedResources = IndexMap<IndexMap<Global*>>;
    using UsedPrivateGlobals = Vector<Global*>;

    struct UsedGlobals {
        UsedResources resources;
        UsedPrivateGlobals privateGlobals;
    };

    static AST::Identifier argumentBufferParameterName(unsigned group);
    static AST::Identifier argumentBufferStructName(unsigned group);

    void def(const String&);
    Global* read(const String&);

    void collectGlobals();
    void visitEntryPoint(AST::Function&, AST::StageAttribute::Stage, PipelineLayout&);
    UsedGlobals determineUsedGlobals(PipelineLayout&, AST::StageAttribute::Stage);
    void usesOverride(AST::Variable&);
    void insertStructs(const UsedResources&);
    void insertParameters(AST::Function&, const UsedResources&);
    void insertMaterializations(AST::Function&, const UsedResources&);
    void insertLocalDefinitions(AST::Function&, const UsedPrivateGlobals&);
    bool shouldBeReference(const Type*) const;

    CallGraph& m_callGraph;
    PrepareResult& m_result;
    HashMap<String, Global> m_globals;
    IndexMap<Vector<std::pair<unsigned, Global*>>> m_groupBindingMap;
    IndexMap<Type*> m_structTypes;
    HashSet<String> m_defs;
    HashSet<String> m_reads;
    Reflection::EntryPointInformation* m_entryPointInformation { nullptr };
};

void RewriteGlobalVariables::run()
{
    collectGlobals();
    for (auto& entryPoint : m_callGraph.entrypoints()) {
        PipelineLayout pipelineLayout;
        auto it = m_result.entryPoints.find(entryPoint.function.name());
        RELEASE_ASSERT(it != m_result.entryPoints.end());
        m_entryPointInformation = &it->value;

        visitEntryPoint(entryPoint.function, entryPoint.stage, pipelineLayout);

        m_entryPointInformation->defaultLayout = WTFMove(pipelineLayout);
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
    AST::Visitor::visit(variable);
}

void RewriteGlobalVariables::visit(AST::IdentifierExpression& identifier)
{
    read(identifier.identifier());
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
            auto result = m_groupBindingMap.add(resource->group, Vector<std::pair<unsigned, Global*>>());
            result.iterator->value.append({ resource->binding, &global });
        }
    }
}

void RewriteGlobalVariables::visitEntryPoint(AST::Function& function, AST::StageAttribute::Stage stage, PipelineLayout& pipelineLayout)
{
    m_reads.clear();
    m_defs.clear();
    m_structTypes.clear();

    visit(function);
    if (m_reads.isEmpty())
        return;

    auto usedGlobals = determineUsedGlobals(pipelineLayout, stage);
    insertStructs(usedGlobals.resources);
    insertParameters(function, usedGlobals.resources);
    insertMaterializations(function, usedGlobals.resources);
    insertLocalDefinitions(function, usedGlobals.privateGlobals);
}


auto RewriteGlobalVariables::determineUsedGlobals(PipelineLayout& pipelineLayout, AST::StageAttribute::Stage stage) -> UsedGlobals
{
    UsedGlobals usedGlobals;
    for (const auto& globalName : m_reads) {
        auto it = m_globals.find(globalName);
        RELEASE_ASSERT(it != m_globals.end());
        auto& global = it->value;
        AST::Variable& variable = *global.declaration;
        switch (variable.flavor()) {
        case AST::VariableFlavor::Override:
            usesOverride(variable);
            break;
        case AST::VariableFlavor::Var:
        case AST::VariableFlavor::Let:
            if (!global.resource.has_value()) {
                usedGlobals.privateGlobals.append(&global);
                continue;
            }
            break;
        case AST::VariableFlavor::Const:
            // Constants must be resolved at an earlier phase
            RELEASE_ASSERT_NOT_REACHED();
        }

        auto group = global.resource->group;
        auto result = usedGlobals.resources.add(group, IndexMap<Global*>());
        result.iterator->value.add(global.resource->binding, &global);

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
    return usedGlobals;
}

void RewriteGlobalVariables::usesOverride(AST::Variable& variable)
{
    Reflection::SpecializationConstantType constantType;
    const Type* type = nullptr;
    if (auto* typeName = variable.maybeTypeName())
        type = typeName->resolvedType();
    else {
        auto* initializer = variable.maybeInitializer();
        ASSERT(initializer);
        type = initializer->inferredType();
    }
    ASSERT(std::holds_alternative<Types::Primitive>(*type));
    const auto& primitive = std::get<Types::Primitive>(*type);
    switch (primitive.kind) {
    case Types::Primitive::Bool:
        constantType = Reflection::SpecializationConstantType::Boolean;
        break;
    case Types::Primitive::F32:
        constantType = Reflection::SpecializationConstantType::Float;
        break;
    case Types::Primitive::I32:
        constantType = Reflection::SpecializationConstantType::Int;
        break;
    case Types::Primitive::U32:
        constantType = Reflection::SpecializationConstantType::Unsigned;
        break;
    case Types::Primitive::Void:
    case Types::Primitive::AbstractInt:
    case Types::Primitive::AbstractFloat:
    case Types::Primitive::Sampler:
    case Types::Primitive::TextureExternal:
        RELEASE_ASSERT_NOT_REACHED();
    }
    m_entryPointInformation->specializationConstants.add(variable.name(), Reflection::SpecializationConstant { String(), constantType });
}

void RewriteGlobalVariables::insertStructs(const UsedResources& usedResources)
{
    for (auto& groupBinding : m_groupBindingMap) {
        unsigned group = groupBinding.key;

        auto usedResource = usedResources.find(group);
        if (usedResource == usedResources.end())
            continue;

        const auto& bindingGlobalMap = groupBinding.value;
        const IndexMap<Global*>& usedBindings = usedResource->value;

        AST::Identifier structName = argumentBufferStructName(group);
        AST::StructureMember::List structMembers;

        for (auto [binding, global] : bindingGlobalMap) {
            if (!usedBindings.contains(binding))
                continue;

            ASSERT(global->declaration->maybeTypeName());
            auto span = global->declaration->span();

            AST::TypeName::Ref memberType = *global->declaration->maybeTypeName();
            auto* type = memberType.get().resolvedType();
            if (shouldBeReference(type)) {
                memberType = m_callGraph.ast().astBuilder().construct<AST::ReferenceTypeName>(span, WTFMove(memberType));
                // FIXME: we need to be able to represent reference types in the type system
                memberType.get().m_resolvedType = type;
            }
            structMembers.append(m_callGraph.ast().astBuilder().construct<AST::StructureMember>(
                span,
                AST::Identifier::make(global->declaration->name()),
                WTFMove(memberType),
                AST::Attribute::List {
                    m_callGraph.ast().astBuilder().construct<AST::BindingAttribute>(span, binding)
                }
            ));
        }

        m_callGraph.ast().append(m_callGraph.ast().structures(), m_callGraph.ast().astBuilder().construct<AST::Structure>(
            SourceSpan::empty(),
            WTFMove(structName),
            WTFMove(structMembers),
            AST::Attribute::List { },
            AST::StructureRole::BindGroup
        ));
        m_structTypes.add(groupBinding.key, m_callGraph.ast().types().structType(m_callGraph.ast().structures().last()));
    }
}

void RewriteGlobalVariables::insertParameters(AST::Function& function, const UsedResources& usedResources)
{
    auto span = function.span();
    for (auto& it : usedResources) {
        unsigned group = it.key;
        auto& type = m_callGraph.ast().astBuilder().construct<AST::NamedTypeName>(span, argumentBufferStructName(group));
        type.m_resolvedType = m_structTypes.get(group);
        m_callGraph.ast().append(function.parameters(), m_callGraph.ast().astBuilder().construct<AST::Parameter>(
            span,
            argumentBufferParameterName(group),
            type,
            AST::Attribute::List {
                m_callGraph.ast().astBuilder().construct<AST::GroupAttribute>(span, group)
            },
            AST::ParameterRole::BindGroup
        ));
    }
}

void RewriteGlobalVariables::insertMaterializations(AST::Function& function, const UsedResources& usedResources)
{
    auto span = function.span();
    for (auto& [group, bindings] : usedResources) {
        auto& argument = m_callGraph.ast().astBuilder().construct<AST::IdentifierExpression>(
            span,
            AST::Identifier::make(argumentBufferParameterName(group))
        );

        for (auto& [_, global] : bindings) {
            auto& name = global->declaration->name();
            String fieldName = name;
            AST::TypeName::Ref typeName = *global->declaration->maybeTypeName();
            auto* type = typeName.get().resolvedType();
            if (auto* primitive = std::get_if<Types::Primitive>(type); primitive && primitive->kind == Types::Primitive::TextureExternal) {
                fieldName = makeString("__", name);
                m_callGraph.ast().setUsesExternalTextures();
            }

            auto& access = m_callGraph.ast().astBuilder().construct<AST::FieldAccessExpression>(
                SourceSpan::empty(),
                argument,
                AST::Identifier::make(WTFMove(fieldName))
            );

            if (shouldBeReference(type)) {
                typeName = m_callGraph.ast().astBuilder().construct<AST::ReferenceTypeName>(
                    SourceSpan::empty(),
                    WTFMove(typeName)
                );
                typeName.get().m_resolvedType = type;
            }

            auto& variable = m_callGraph.ast().astBuilder().construct<AST::VariableStatement>(
                SourceSpan::empty(),
                m_callGraph.ast().astBuilder().construct<AST::Variable>(
                    SourceSpan::empty(),
                    AST::VariableFlavor::Let,
                    AST::Identifier::make(name),
                    nullptr,
                    &typeName.get(),
                    &access,
                    AST::Attribute::List { }
                )
            );

            m_callGraph.ast().insert(function.body().statements(), 0, AST::Statement::Ref(variable));
        }
    }
}

void RewriteGlobalVariables::insertLocalDefinitions(AST::Function& function, const UsedPrivateGlobals& usedPrivateGlobals)
{
    for (auto* global : usedPrivateGlobals) {
        auto& variableStatement = m_callGraph.ast().astBuilder().construct<AST::VariableStatement>(
            SourceSpan::empty(),
            *global->declaration
        );
        m_callGraph.ast().insert(function.body().statements(), 0, std::reference_wrapper<AST::Statement>(variableStatement));
    }
}

bool RewriteGlobalVariables::shouldBeReference(const Type* type) const
{
    if (std::get_if<Types::Texture>(type))
        return false;

    if (auto* primitive = std::get_if<Types::Primitive>(type)) {
        if (primitive->kind == Types::Primitive::Sampler)
            return false;
    }

    return true;
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

void rewriteGlobalVariables(CallGraph& callGraph, const HashMap<String, std::optional<PipelineLayout>>& pipelineLayouts, PrepareResult& result)
{
    RewriteGlobalVariables(callGraph, pipelineLayouts, result).run();
}

} // namespace WGSL
