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
#include "EntryPointRewriter.h"

#include "AST.h"
#include "ASTVisitor.h"
#include "CallGraph.h"
#include "TypeStore.h"
#include "Types.h"
#include "WGSL.h"
#include "WGSLShaderModule.h"

namespace WGSL {

class EntryPointRewriter {
public:
    EntryPointRewriter(ShaderModule&, const AST::Function&, AST::StageAttribute::Stage);

    void rewrite();
    Reflection::EntryPointInformation takeEntryPointInformation();

private:
    struct MemberOrParameter {
        AST::Identifier name;
        AST::TypeName& type;
        AST::Attribute::List attributes;
    };

    enum class IsBuiltin {
        No = 0,
        Yes = 1,
    };

    void collectParameters();
    void checkReturnType();
    void constructInputStruct();
    void materialize(Vector<String>& path, MemberOrParameter&, IsBuiltin);
    void visit(Vector<String>& path, MemberOrParameter&&);
    void appendBuiltins();

    AST::StageAttribute::Stage m_stage;
    ShaderModule& m_shaderModule;
    const AST::Function& m_function;

    Vector<MemberOrParameter> m_builtins;
    Vector<MemberOrParameter> m_parameters;
    AST::Statement::List m_materializations;
    const Type* m_structType;
    String m_structTypeName;
    String m_structParameterName;
    Reflection::EntryPointInformation m_information;
};

EntryPointRewriter::EntryPointRewriter(ShaderModule& shaderModule, const AST::Function& function, AST::StageAttribute::Stage stage)
    : m_stage(stage)
    , m_shaderModule(shaderModule)
    , m_function(function)
{
    switch (m_stage) {
    case AST::StageAttribute::Stage::Compute: {
        unsigned x = 0;
        unsigned y = 1;
        unsigned z = 1;
        for (auto& attribute : function.attributes()) {
            if (!is<AST::WorkgroupSizeAttribute>(attribute))
                continue;
            auto& workgroupSize = downcast<AST::WorkgroupSizeAttribute>(attribute);
            x = *AST::extractInteger(workgroupSize.x());
            if (auto* maybeY = workgroupSize.maybeY())
                y = *AST::extractInteger(*maybeY);
            if (auto* maybeZ = workgroupSize.maybeZ())
                z = *AST::extractInteger(*maybeZ);
        }
        ASSERT(x);
        m_information.typedEntryPoint = Reflection::Compute { x, y, z };
        break;
    }
    case AST::StageAttribute::Stage::Vertex:
        m_information.typedEntryPoint = Reflection::Vertex { false };
        break;
    case AST::StageAttribute::Stage::Fragment:
        m_information.typedEntryPoint = Reflection::Fragment { };
        break;
    }
}

void EntryPointRewriter::rewrite()
{
    m_structTypeName = makeString("__", m_function.name(), "_inT");
    m_structParameterName = makeString("__", m_function.name(), "_in");

    collectParameters();
    checkReturnType();
    appendBuiltins();

    if (!m_parameters.isEmpty()) {
        constructInputStruct();

        // add parameter to builtins: ${structName} : ${structType}
        auto& type = m_shaderModule.astBuilder().construct<AST::NamedTypeName>(SourceSpan::empty(), AST::Identifier::make(m_structTypeName));
        type.m_resolvedType = m_structType;
        auto& parameter = m_shaderModule.astBuilder().construct<AST::Parameter>(
            SourceSpan::empty(),
            AST::Identifier::make(m_structParameterName),
            type,
            AST::Attribute::List { },
            AST::ParameterRole::StageIn
        );
        m_shaderModule.append(m_function.parameters(), parameter);
    }

    while (m_materializations.size())
        m_shaderModule.insert(m_function.body().statements(), 0, m_materializations.takeLast());
}

Reflection::EntryPointInformation EntryPointRewriter::takeEntryPointInformation()
{
    return WTFMove(m_information);
}

void EntryPointRewriter::collectParameters()
{
    while (m_function.parameters().size()) {
        AST::Parameter& parameter = m_shaderModule.takeLast(m_function.parameters());
        Vector<String> path;
        visit(path, MemberOrParameter { parameter.name(), parameter.typeName(), parameter.attributes() });
    }
}

void EntryPointRewriter::checkReturnType()
{
    if (m_stage != AST::StageAttribute::Stage::Vertex)
        return;

    if (auto* maybeReturnType = m_function.maybeReturnType()) {
        if (!is<AST::NamedTypeName>(*maybeReturnType))
            return;

        auto& namedTypeName = downcast<AST::NamedTypeName>(*maybeReturnType);
        if (auto* structType = std::get_if<Types::Struct>(namedTypeName.resolvedType())) {
            ASSERT(structType->structure.role() == AST::StructureRole::UserDefined);

            String returnStructName = makeString("__", structType->structure.name(), "_VertexOutput");
            auto& returnStruct = m_shaderModule.astBuilder().construct<AST::Structure>(
                SourceSpan::empty(),
                AST::Identifier::make(returnStructName),
                AST::StructureMember::List(structType->structure.members()),
                AST::Attribute::List { },
                AST::StructureRole::VertexOutput

            );
            m_shaderModule.append(m_shaderModule.structures(), returnStruct);

            auto& returnType = m_shaderModule.astBuilder().construct<AST::NamedTypeName>(
                SourceSpan::empty(),
                AST::Identifier::make(returnStructName)
            );
            returnType.m_resolvedType = m_shaderModule.types().structType(returnStruct);
            m_shaderModule.replace(namedTypeName, returnType);
        }
    }
}

void EntryPointRewriter::constructInputStruct()
{
    // insert `var ${parameter.name()} = ${structName}.${parameter.name()}`
    AST::StructureMember::List structMembers;
    for (auto& parameter : m_parameters) {
        structMembers.append(m_shaderModule.astBuilder().construct<AST::StructureMember>(
            SourceSpan::empty(),
            WTFMove(parameter.name),
            parameter.type,
            WTFMove(parameter.attributes)
        ));
    }

    AST::StructureRole role;
    switch (m_stage) {
    case AST::StageAttribute::Stage::Compute:
        role = AST::StructureRole::ComputeInput;
        break;
    case AST::StageAttribute::Stage::Vertex:
        role = AST::StructureRole::VertexInput;
        break;
    case AST::StageAttribute::Stage::Fragment:
        role = AST::StructureRole::FragmentInput;
        break;
    }

    m_shaderModule.append(m_shaderModule.structures(), m_shaderModule.astBuilder().construct<AST::Structure>(
        SourceSpan::empty(),
        AST::Identifier::make(m_structTypeName),
        WTFMove(structMembers),
        AST::Attribute::List { },
        role
    ));
    m_structType = m_shaderModule.types().structType(m_shaderModule.structures().last());
}

void EntryPointRewriter::materialize(Vector<String>& path, MemberOrParameter& data, IsBuiltin isBuiltin)
{
    AST::Expression::Ptr rhs;
    if (isBuiltin == IsBuiltin::Yes)
        rhs = &m_shaderModule.astBuilder().construct<AST::IdentifierExpression>(SourceSpan::empty(), AST::Identifier::make(data.name));
    else {
        rhs = &m_shaderModule.astBuilder().construct<AST::FieldAccessExpression>(
            SourceSpan::empty(),
            m_shaderModule.astBuilder().construct<AST::IdentifierExpression>(SourceSpan::empty(), AST::Identifier::make(m_structParameterName)),
            AST::Identifier::make(data.name)
        );
    }

    if (!path.size()) {
        m_materializations.append(m_shaderModule.astBuilder().construct<AST::VariableStatement>(
            SourceSpan::empty(),
            m_shaderModule.astBuilder().construct<AST::Variable>(
                SourceSpan::empty(),
                AST::VariableFlavor::Var,
                AST::Identifier::make(data.name),
                nullptr, // TODO: do we need a VariableQualifier?
                &data.type,
                rhs,
                AST::Attribute::List { }
            )
        ));
        return;
    }

    path.append(data.name);
    unsigned i = 0;
    AST::Expression::Ref lhs = m_shaderModule.astBuilder().construct<AST::IdentifierExpression>(SourceSpan::empty(), AST::Identifier::make(path[i++]));
    while (i < path.size()) {
        lhs = m_shaderModule.astBuilder().construct<AST::FieldAccessExpression>(
            SourceSpan::empty(),
            WTFMove(lhs),
            AST::Identifier::make(path[i++])
        );
    }
    path.removeLast();
    m_materializations.append(m_shaderModule.astBuilder().construct<AST::AssignmentStatement>(
        SourceSpan::empty(),
        WTFMove(lhs),
        *rhs
    ));
}

void EntryPointRewriter::visit(Vector<String>& path, MemberOrParameter&& data)
{
    if (auto* structType = std::get_if<Types::Struct>(data.type.resolvedType())) {
        m_materializations.append(m_shaderModule.astBuilder().construct<AST::VariableStatement>(
            SourceSpan::empty(),
            m_shaderModule.astBuilder().construct<AST::Variable>(
                SourceSpan::empty(),
                AST::VariableFlavor::Var,
                AST::Identifier::make(data.name),
                nullptr,
                &data.type,
                nullptr,
                AST::Attribute::List { }
            )
        ));
        path.append(data.name);
        for (auto& member : structType->structure.members())
            visit(path, MemberOrParameter { member.name(), member.type(), member.attributes() });
        path.removeLast();
        return;
    }

    bool isBuiltin = false;
    for (auto& attribute : data.attributes) {
        if (is<AST::BuiltinAttribute>(attribute)) {
            isBuiltin = true;
            break;
        }
    }

    if (isBuiltin) {
        // if path is empty, then it was already a parameter and there's nothing to do
        if (!path.isEmpty())
            materialize(path, data, IsBuiltin::Yes);

        // builtin was hoisted from a struct into a parameter, we need to reconstruct the struct
        // ${path}.${data.name} = ${data.name}
        m_builtins.append(WTFMove(data));
        return;
    }

    // parameter was moved into a struct, so we need to reload it
    // ${path}.${data.name} = ${struct}.${data.name}
    materialize(path, data, IsBuiltin::No);
    m_parameters.append(WTFMove(data));
}

void EntryPointRewriter::appendBuiltins()
{
    for (auto& data : m_builtins) {
        m_shaderModule.append(m_function.parameters(), m_shaderModule.astBuilder().construct<AST::Parameter>(
            SourceSpan::empty(),
            AST::Identifier::make(data.name),
            data.type,
            WTFMove(data.attributes),
            AST::ParameterRole::UserDefined
        ));
    }
}

void rewriteEntryPoints(CallGraph& callGraph, PrepareResult& result)
{
    for (auto& entryPoint : callGraph.entrypoints()) {
        EntryPointRewriter rewriter(callGraph.ast(), entryPoint.function, entryPoint.stage);
        rewriter.rewrite();
        auto addResult = result.entryPoints.add(entryPoint.function.name().id(), rewriter.takeEntryPointInformation());
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }
}

} // namespace WGSL
