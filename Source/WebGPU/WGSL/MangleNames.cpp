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
#include "MangleNames.h"

#include "AST.h"
#include "ASTVisitor.h"
#include "CallGraph.h"
#include "ContextProviderInlines.h"
#include "WGSL.h"
#include "WGSLShaderModule.h"
#include <wtf/HashSet.h>

namespace WGSL {

struct MangledName {
    enum Kind : uint8_t {
        Type,
        Local,
        Parameter,
        Function,
        Field,
    };
    static constexpr unsigned numberOfKinds = 6;

    Kind kind;
    uint32_t index;
    String originalName;

    String toString() const
    {
        static const ASCIILiteral prefixes[] = {
            "type"_s,
            "local"_s,
            "parameter"_s,
            "function"_s,
            "field"_s,
        };
        return makeString(prefixes[WTF::enumToUnderlyingType(kind)], String::number(index));
    }
};

class NameManglerVisitor : public AST::Visitor, public ContextProvider<MangledName> {
    using ContextProvider = ContextProvider<MangledName>;

public:
    NameManglerVisitor(const CallGraph& callGraph, PrepareResult& result)
        : m_callGraph(callGraph)
        , m_result(result)
    {
    }

    void run();

    void visit(AST::Function&) override;
    void visit(AST::VariableStatement&) override;
    void visit(AST::Structure&) override;
    void visit(AST::Variable&) override;
    void visit(AST::CompoundStatement&) override;
    void visit(AST::IdentifierExpression&) override;
    void visit(AST::FieldAccessExpression&) override;
    void visit(AST::NamedTypeName&) override;

private:
    using NameMap = ContextProvider::ContextMap;

    void introduceVariable(AST::Identifier&, MangledName::Kind);
    void readVariable(AST::Identifier&) const;

    MangledName makeMangledName(const String&, MangledName::Kind);

    void visitVariableDeclaration(AST::Variable&, MangledName::Kind);
    void visitFunctionBody(AST::Function&);

    const CallGraph& m_callGraph;
    PrepareResult& m_result;
    HashMap<AST::Structure*, NameMap> m_structFieldMapping;
    uint32_t m_indexPerType[MangledName::numberOfKinds] { 0 };
};

void NameManglerVisitor::run()
{
    auto& module = m_callGraph.ast();
    for (auto& function : module.functions()) {
        String originalName = function.name();
        introduceVariable(function.name(), MangledName::Function);
        auto it = m_result.entryPoints.find(originalName);
        if (it != m_result.entryPoints.end())
            it->value.mangledName = function.name();
    }

    for (auto& structure : module.structures())
        visit(structure);

    for (auto& variable : module.variables())
        visit(variable);

    for (auto& function : module.functions())
        visitFunctionBody(function);
}

void NameManglerVisitor::visit(AST::Function& function)
{
    introduceVariable(function.name(), MangledName::Function);
}

void NameManglerVisitor::visitFunctionBody(AST::Function& function)
{
    ContextScope functionScope(this);

    for (auto& parameter : function.parameters()) {
        AST::Visitor::visit(parameter.typeName());
        introduceVariable(parameter.name(), MangledName::Parameter);
    }

    // It's important that we call the base visitor here directly, otherwise
    // our overwritten visitor will introduce a new ContextScope for the compound
    // statement, which would allow shadowing the function's parameters
    AST::Visitor::visit(function.body());

    if (function.maybeReturnType())
        AST::Visitor::visit(*function.maybeReturnType());
}

void NameManglerVisitor::visit(AST::Structure& structure)
{
    introduceVariable(structure.name(), MangledName::Type);

    NameMap fieldMap;
    for (auto& member : structure.members()) {
        AST::Visitor::visit(member.type());
        auto mangledName = makeMangledName(member.name(), MangledName::Field);
        fieldMap.add(member.name(), mangledName);
        // FIXME: need to resolve type of expressions in order to be able to replace struct fields
    }
    auto result = m_structFieldMapping.add(&structure, WTFMove(fieldMap));
    ASSERT_UNUSED(result, result.isNewEntry);
}

void NameManglerVisitor::visit(AST::Variable& variable)
{
    String originalName = variable.name();
    for (auto& attribute : variable.attributes()) {
        if (is<AST::IdAttribute>(attribute)) {
            unsigned value;
            auto& expression = downcast<AST::IdAttribute>(attribute).value();
            if (is<AST::AbstractIntegerLiteral>(expression))
                value = downcast<AST::AbstractIntegerLiteral>(expression).value();
            else if (is<AST::Signed32Literal>(expression))
                value = downcast<AST::Signed32Literal>(expression).value();
            else if (is<AST::Unsigned32Literal>(expression))
                value = downcast<AST::Unsigned32Literal>(expression).value();
            else {
                // Constants must be resolved at an earlier phase
                RELEASE_ASSERT_NOT_REACHED();
            }
            originalName = String::number(value);
            break;
        }
    }

    const String& mangledName = variable.name();

    for (auto& entry : m_result.entryPoints) {
        auto it = entry.value.specializationConstants.find(originalName);
        if (it != entry.value.specializationConstants.end())
            it->value.mangledName = mangledName;
    }
}

void NameManglerVisitor::visit(AST::VariableStatement& variable)
{
    visitVariableDeclaration(variable.variable(), MangledName::Local);
}

void NameManglerVisitor::visitVariableDeclaration(AST::Variable& variable, MangledName::Kind kind)
{
    introduceVariable(variable.name(), kind);
    AST::Visitor::visit(variable);
}

void NameManglerVisitor::visit(AST::CompoundStatement& statement)
{
    ContextScope blockScope(this);
    AST::Visitor::visit(statement);
}

void NameManglerVisitor::visit(AST::IdentifierExpression& identifier)
{
    readVariable(identifier.identifier());
}

void NameManglerVisitor::visit(AST::FieldAccessExpression& access)
{
    // FIXME: need to resolve type of expressions in order to be able to replace struct fields
    AST::Visitor::visit(access.base());
}

void NameManglerVisitor::visit(AST::NamedTypeName& type)
{
    readVariable(type.name());
}

void NameManglerVisitor::introduceVariable(AST::Identifier& name, MangledName::Kind kind)
{
    const auto* mangledName = ContextProvider::introduceVariable(name, makeMangledName(name, kind));
    ASSERT(mangledName);
    m_callGraph.ast().replace(&name, AST::Identifier::makeWithSpan(name.span(), mangledName->toString()));
}

MangledName NameManglerVisitor::makeMangledName(const String& name, MangledName::Kind kind)
{
    return MangledName {
        kind,
        m_indexPerType[WTF::enumToUnderlyingType(kind)]++,
        name
    };
}

void NameManglerVisitor::readVariable(AST::Identifier& name) const
{
    // FIXME: this should be unconditional
    if (const auto* mangledName = ContextProvider::readVariable(name))
        m_callGraph.ast().replace(&name, AST::Identifier::makeWithSpan(name.span(), mangledName->toString()));
}

void mangleNames(CallGraph& callGraph, PrepareResult& result)
{
    NameManglerVisitor(callGraph, result).run();
}

} // namespace WGSL
