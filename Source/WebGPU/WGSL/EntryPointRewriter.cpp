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

namespace WGSL {

class EntryPointRewriter {
public:
    EntryPointRewriter(AST::ShaderModule& shaderModule, AST::FunctionDecl& functionDecl)
        : m_shaderModule(shaderModule)
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
    void constructInputStruct();
    void materialize(Vector<String>& path, MemberOrParameter&, IsBuiltin);
    void visit(Vector<String>& path, MemberOrParameter&&);
    void appendBuiltins();

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
    if (type.kind() == AST::Node::Kind::TypeReference)
        return getResolvedType(downcast<AST::TypeReference>(type).type());

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

    // nothing to rewrite
    if (m_parameters.isEmpty()) {
        appendBuiltins();
        return;
    }

    // add parameter to builtins: ${structName} : ${structType}
    m_builtins.append({
        m_structParameterName,
        adoptRef(*new AST::NamedType(m_emptySourceSpan, m_structTypeName)),
        AST::Attribute::List { }
    });

    constructInputStruct();
    appendBuiltins();

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

void EntryPointRewriter::constructInputStruct()
{
    // insert `var ${parameter.name()} = ${structName}.${parameter.name()}`
    AST::StructMember::List structMembers;
    for (auto& parameter : m_parameters) {
        auto parameterType = adoptRef(*new AST::TypeReference(m_emptySourceSpan, parameter.m_type.copyRef()));

        structMembers.append(makeUniqueRef<AST::StructMember>(
            m_emptySourceSpan,
            parameter.m_name,
            parameterType.copyRef(),
            WTFMove(parameter.m_attributes)
        ));
    }

    m_shaderModule.structs().append(makeUniqueRef<AST::StructDecl>(
        m_emptySourceSpan,
        m_structTypeName,
        WTFMove(structMembers),
        AST::Attribute::List { }
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
            WTFMove(data.m_attributes)
        ));
    }
}

class RewriteEntryPoints : public AST::Visitor {
public:
    RewriteEntryPoints(AST::ShaderModule& shaderModule)
        : AST::Visitor()
        , m_shaderModule(shaderModule)
    {
    }

    void visit(AST::FunctionDecl&) override;

private:
    AST::ShaderModule& m_shaderModule;
};

void RewriteEntryPoints::visit(AST::FunctionDecl& functionDecl)
{
    for (auto& attribute : functionDecl.attributes()) {
        if (attribute->kind() == AST::Node::Kind::StageAttribute) {
            EntryPointRewriter(m_shaderModule, functionDecl).rewrite();
            return;
        }
    }

}

void rewriteEntryPoints(AST::ShaderModule& shaderModule)
{
    RewriteEntryPoints(shaderModule).AST::Visitor::visit(shaderModule);
}

} // namespace WGSL
