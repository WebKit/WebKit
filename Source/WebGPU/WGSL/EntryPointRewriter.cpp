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
#include "WGSL.h"
#include "WGSLShaderModule.h"

namespace WGSL {

class EntryPointRewriter {
public:
    EntryPointRewriter(ShaderModule&, AST::Function&, AST::StageAttribute::Stage);

    void rewrite();
    Reflection::EntryPointInformation takeEntryPointInformation();

private:
    struct MemberOrParameter {
        AST::Identifier m_name;
        AST::TypeName::Ref m_type;
        AST::Attribute::List m_attributes;
    };

    enum class IsBuiltin {
        No = 0,
        Yes = 1,
    };

    static AST::TypeName& getResolvedType(AST::TypeName&);

    void collectParameters();
    void checkReturnType();
    void constructInputStruct();
    void materialize(Vector<String>& path, MemberOrParameter&, IsBuiltin);
    void visit(Vector<String>& path, MemberOrParameter&&);
    void appendBuiltins();

    AST::StageAttribute::Stage m_stage;
    ShaderModule& m_shaderModule;
    AST::Function& m_function;

    Vector<MemberOrParameter> m_builtins;
    Vector<MemberOrParameter> m_parameters;
    AST::Statement::List m_materializations;
    String m_structTypeName;
    String m_structParameterName;
    Reflection::EntryPointInformation m_information;
};

EntryPointRewriter::EntryPointRewriter(ShaderModule& shaderModule, AST::Function& function, AST::StageAttribute::Stage stage)
    : m_stage(stage)
    , m_shaderModule(shaderModule)
    , m_function(function)
{
    switch (m_stage) {
    case AST::StageAttribute::Stage::Compute:
        m_information.typedEntryPoint = Reflection::Compute { 1, 1, 1 };
        break;
    case AST::StageAttribute::Stage::Vertex:
        m_information.typedEntryPoint = Reflection::Vertex { false };
        break;
    case AST::StageAttribute::Stage::Fragment:
        m_information.typedEntryPoint = Reflection::Fragment { };
        break;
    }
}

AST::TypeName& EntryPointRewriter::getResolvedType(AST::TypeName& type)
{
    if (is<AST::NamedTypeName>(type)) {
        if (auto* resolvedType = downcast<AST::NamedTypeName>(type).maybeResolvedReference())
            return getResolvedType(*resolvedType);
    }

    return type;
}

void EntryPointRewriter::rewrite()
{
    m_structTypeName = makeString("__", m_function.name(), "_inT");
    m_structParameterName = makeString("__", m_function.name(), "_in");

    collectParameters();
    checkReturnType();

    // nothing to rewrite
    if (m_parameters.isEmpty()) {
        appendBuiltins();
        return;
    }

    constructInputStruct();
    appendBuiltins();

    // add parameter to builtins: ${structName} : ${structType}
    m_function.parameters().append(makeUniqueRef<AST::Parameter>(
        SourceSpan::empty(),
        AST::Identifier::make(m_structParameterName),
        adoptRef(*new AST::NamedTypeName(SourceSpan::empty(), AST::Identifier::make(m_structTypeName))),
        AST::Attribute::List { },
        AST::ParameterRole::StageIn
    ));

    while (m_materializations.size())
        m_function.body().statements().insert(0, m_materializations.takeLast());
}

Reflection::EntryPointInformation EntryPointRewriter::takeEntryPointInformation()
{
    return WTFMove(m_information);
}

void EntryPointRewriter::collectParameters()
{
    while (m_function.parameters().size()) {
        auto parameter = m_function.parameters().takeLast();
        Vector<String> path;
        visit(path, MemberOrParameter { parameter->name(), parameter->typeName(), WTFMove(parameter->attributes()) });
    }
}

void EntryPointRewriter::checkReturnType()
{
    if (m_stage != AST::StageAttribute::Stage::Vertex)
        return;

    // FIXME: we might have to duplicate this struct if it has other uses
    if (auto* maybeReturnType = m_function.maybeReturnType()) {
        auto& returnType = getResolvedType(*maybeReturnType);
        if (is<AST::StructTypeName>(returnType)) {
            auto& structDecl = downcast<AST::StructTypeName>(returnType).structure();
            ASSERT(structDecl.role() == AST::StructureRole::UserDefined);
            structDecl.setRole(AST::StructureRole::VertexOutput);
        }
    }
}

void EntryPointRewriter::constructInputStruct()
{
    // insert `var ${parameter.name()} = ${structName}.${parameter.name()}`
    AST::StructureMember::List structMembers;
    for (auto& parameter : m_parameters) {
        structMembers.append(makeUniqueRef<AST::StructureMember>(
            SourceSpan::empty(),
            WTFMove(parameter.m_name),
            WTFMove(parameter.m_type),
            WTFMove(parameter.m_attributes)
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

    m_shaderModule.structures().append(makeUniqueRef<AST::Structure>(
        SourceSpan::empty(),
        AST::Identifier::make(m_structTypeName),
        WTFMove(structMembers),
        AST::Attribute::List { },
        role
    ));
}

void EntryPointRewriter::materialize(Vector<String>& path, MemberOrParameter& data, IsBuiltin isBuiltin)
{
    std::unique_ptr<AST::Expression> rhs;
    if (isBuiltin == IsBuiltin::Yes)
        rhs = makeUnique<AST::IdentifierExpression>(SourceSpan::empty(), AST::Identifier::make(data.m_name));
    else {
        rhs = makeUnique<AST::FieldAccessExpression>(
            SourceSpan::empty(),
            makeUniqueRef<AST::IdentifierExpression>(SourceSpan::empty(), AST::Identifier::make(m_structParameterName)),
            AST::Identifier::make(data.m_name)
        );
    }

    if (!path.size()) {
        m_materializations.append(makeUniqueRef<AST::VariableStatement>(
            SourceSpan::empty(),
            makeUniqueRef<AST::Variable>(
                SourceSpan::empty(),
                AST::VariableFlavor::Var,
                AST::Identifier::make(data.m_name),
                nullptr, // TODO: do we need a VariableQualifier?
                data.m_type.copyRef(),
                WTFMove(rhs),
                AST::Attribute::List { }
            )
        ));
        return;
    }

    path.append(data.m_name);
    unsigned i = 0;
    UniqueRef<AST::Expression> lhs = makeUniqueRef<AST::IdentifierExpression>(SourceSpan::empty(), AST::Identifier::make(path[i++]));
    while (i < path.size()) {
        lhs = makeUniqueRef<AST::FieldAccessExpression>(
            SourceSpan::empty(),
            WTFMove(lhs),
            AST::Identifier::make(path[i++])
        );
    }
    path.removeLast();
    m_materializations.append(makeUniqueRef<AST::AssignmentStatement>(
        SourceSpan::empty(),
        WTFMove(lhs),
        makeUniqueRefFromNonNullUniquePtr(WTFMove(rhs))
    ));
}

void EntryPointRewriter::visit(Vector<String>& path, MemberOrParameter&& data)
{
    auto& type = getResolvedType(data.m_type);

    if (is<AST::StructTypeName>(type)) {
        m_materializations.append(makeUniqueRef<AST::VariableStatement>(
            SourceSpan::empty(),
            makeUniqueRef<AST::Variable>(
                SourceSpan::empty(),
                AST::VariableFlavor::Var,
                AST::Identifier::make(data.m_name),
                nullptr,
                &type,
                nullptr,
                AST::Attribute::List { }
            )
        ));
        path.append(data.m_name);
        for (auto& member : downcast<AST::StructTypeName>(type).structure().members())
            visit(path, MemberOrParameter { member.name(), member.type(), member.attributes() });
        path.removeLast();
        return;
    }

    bool isBuiltin = false;
    for (auto& attribute : data.m_attributes) {
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
        // ${path}.${data.m_name} = ${data.name}
        m_builtins.append(WTFMove(data));
        return;
    }

    // parameter was moved into a struct, so we need to reload it
    // ${path}.${data.m_name} = ${struct}.${data.name}
    materialize(path, data, IsBuiltin::No);
    m_parameters.append(WTFMove(data));
}

void EntryPointRewriter::appendBuiltins()
{
    for (auto& data : m_builtins) {
        m_function.parameters().append(makeUniqueRef<AST::Parameter>(
            SourceSpan::empty(),
            AST::Identifier::make(data.m_name),
            WTFMove(data.m_type),
            WTFMove(data.m_attributes),
            AST::ParameterRole::UserDefined
        ));
    }
}

void rewriteEntryPoints(CallGraph& callGraph, PrepareResult& result)
{
    for (auto& entryPoint : callGraph.entrypoints()) {
        EntryPointRewriter rewriter(callGraph.ast(), entryPoint.m_function, entryPoint.m_stage);
        rewriter.rewrite();
        auto addResult = result.entryPoints.add(entryPoint.m_function.name().id(), rewriter.takeEntryPointInformation());
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }
}

} // namespace WGSL
