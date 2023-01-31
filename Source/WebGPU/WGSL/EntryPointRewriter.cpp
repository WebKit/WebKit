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

namespace WGSL {

class EntryPointRewriter {
public:
    EntryPointRewriter(AST::ShaderModule& shaderModule, AST::FunctionDecl& functionDecl, AST::StageAttribute::Stage stage)
        : m_stage(stage)
        , m_shaderModule(shaderModule)
        , m_functionDecl(functionDecl)
        , m_emptySourceSpan(0, 0, 0, 0)
    {
    }

    void rewrite();

private:
    struct MemberOrParameter {
        String m_name;
        Ref<AST::TypeDecl> m_type;
        AST::Attribute::List m_attributes;
    };

    enum class IsBuiltin {
        No = 0,
        Yes = 1,
    };

    static AST::TypeDecl& getResolvedType(AST::TypeDecl&);

    void collectParameters();
    void checkReturnType();
    void constructInputStruct();
    void materialize(Vector<String>& path, MemberOrParameter&, IsBuiltin);
    void visit(Vector<String>& path, MemberOrParameter&&);
    void appendBuiltins();

    AST::StageAttribute::Stage m_stage;
    AST::ShaderModule& m_shaderModule;
    AST::FunctionDecl& m_functionDecl;

    const SourceSpan m_emptySourceSpan;
    Vector<MemberOrParameter> m_builtins;
    Vector<MemberOrParameter> m_parameters;
    AST::Statement::List m_materializations;
    String m_structTypeName;
    String m_structParameterName;
};

AST::TypeDecl& EntryPointRewriter::getResolvedType(AST::TypeDecl& type)
{
    if (type.kind() == AST::Node::Kind::NamedType) {
        if (auto* resolvedType = downcast<AST::NamedType>(type).maybeResolvedReference())
            return getResolvedType(*resolvedType);
    }

    return type;
}

void EntryPointRewriter::rewrite()
{
    m_structTypeName = makeString("__", m_functionDecl.name(), "_inT");
    m_structParameterName = makeString("__", m_functionDecl.name(), "_in");

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
    m_functionDecl.parameters().append(makeUniqueRef<AST::Parameter>(
        m_emptySourceSpan,
        m_structParameterName,
        adoptRef(*new AST::NamedType(m_emptySourceSpan, m_structTypeName)),
        AST::Attribute::List { },
        AST::ParameterRole::StageIn
    ));

    while (m_materializations.size())
        m_functionDecl.body().statements().insert(0, m_materializations.takeLast());
}

void EntryPointRewriter::collectParameters()
{
    while (m_functionDecl.parameters().size()) {
        auto parameter = m_functionDecl.parameters().takeLast();
        Vector<String> path;
        visit(path, MemberOrParameter { parameter->name(), parameter->type(), WTFMove(parameter->attributes()) });
    }
}

void EntryPointRewriter::checkReturnType()
{
    if (m_stage != AST::StageAttribute::Stage::Vertex)
        return;

    // FIXME: we might have to duplicate this struct if it has other uses
    if (auto* maybeReturnType = m_functionDecl.maybeReturnType()) {
        auto& returnType = getResolvedType(*maybeReturnType);
        if (returnType.kind() == AST::Node::Kind::StructType) {
            auto& structDecl = downcast<AST::StructType>(returnType).structDecl();
            ASSERT(structDecl.role() == AST::StructRole::UserDefined);
            structDecl.setRole(AST::StructRole::VertexOutput);
        }
    }
}

void EntryPointRewriter::constructInputStruct()
{
    // insert `var ${parameter.name()} = ${structName}.${parameter.name()}`
    AST::StructMember::List structMembers;
    for (auto& parameter : m_parameters) {
        structMembers.append(makeUniqueRef<AST::StructMember>(
            m_emptySourceSpan,
            WTFMove(parameter.m_name),
            WTFMove(parameter.m_type),
            WTFMove(parameter.m_attributes)
        ));
    }

    AST::StructRole role;
    switch (m_stage) {
    case AST::StageAttribute::Stage::Compute:
        role = AST::StructRole::ComputeInput;
        break;
    case AST::StageAttribute::Stage::Vertex:
        role = AST::StructRole::VertexInput;
        break;
    case AST::StageAttribute::Stage::Fragment:
        role = AST::StructRole::FragmentInput;
        break;
    }

    m_shaderModule.structs().append(makeUniqueRef<AST::StructDecl>(
        m_emptySourceSpan,
        m_structTypeName,
        WTFMove(structMembers),
        AST::Attribute::List { },
        role
    ));
}

void EntryPointRewriter::materialize(Vector<String>& path, MemberOrParameter& data, IsBuiltin isBuiltin)
{
    std::unique_ptr<AST::Expression> rhs;
    if (isBuiltin == IsBuiltin::Yes)
        rhs = makeUnique<AST::IdentifierExpression>(m_emptySourceSpan, data.m_name);
    else {
        rhs = makeUnique<AST::StructureAccess>(
            m_emptySourceSpan,
            makeUniqueRef<AST::IdentifierExpression>(m_emptySourceSpan, m_structParameterName),
            data.m_name
        );
    }

    if (!path.size()) {
        m_materializations.append(makeUniqueRef<AST::VariableStatement>(
            m_emptySourceSpan,
            AST::VariableDecl(
                m_emptySourceSpan,
                data.m_name,
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
    UniqueRef<AST::Expression> lhs = makeUniqueRef<AST::IdentifierExpression>(m_emptySourceSpan, path[i++]);
    while (i < path.size()) {
        lhs = makeUniqueRef<AST::StructureAccess>(
            m_emptySourceSpan,
            WTFMove(lhs),
            path[i++]
        );
    }
    path.removeLast();
    m_materializations.append(makeUniqueRef<AST::AssignmentStatement>(
        m_emptySourceSpan,
        lhs.moveToUniquePtr(),
        makeUniqueRefFromNonNullUniquePtr(WTFMove(rhs))
    ));
}

void EntryPointRewriter::visit(Vector<String>& path, MemberOrParameter&& data)
{
    auto& type = getResolvedType(data.m_type);

    if (type.kind() == AST::Node::Kind::StructType) {
        m_materializations.append(makeUniqueRef<AST::VariableStatement>(
            m_emptySourceSpan,
            AST::VariableDecl(
                m_emptySourceSpan,
                data.m_name,
                nullptr,
                &type,
                nullptr,
                AST::Attribute::List { }
            )
        ));
        path.append(data.m_name);
        for (auto& member : downcast<AST::StructType>(type).structDecl().members())
            visit(path, MemberOrParameter { member.name(), member.type(), member.attributes() });
        path.removeLast();
        return;
    }

    bool isBuiltin = false;
    for (auto& attribute : data.m_attributes) {
        if (attribute->kind() == AST::Node::Kind::BuiltinAttribute) {
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
        m_functionDecl.parameters().append(makeUniqueRef<AST::Parameter>(
            m_emptySourceSpan,
            data.m_name,
            WTFMove(data.m_type),
            WTFMove(data.m_attributes),
            AST::ParameterRole::UserDefined
        ));
    }
}

void rewriteEntryPoints(CallGraph& callGraph)
{
    for (auto& entryPoint : callGraph.entrypoints())
        EntryPointRewriter(callGraph.ast(), entryPoint.m_function, entryPoint.m_stage).rewrite();
}

} // namespace WGSL
