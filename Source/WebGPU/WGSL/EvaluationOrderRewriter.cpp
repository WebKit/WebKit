/*
 * Copyright (c) 2026 Apple Inc. All rights reserved.
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
#include "EvaluationOrderRewriter.h"

#include "AST.h"
#include "ASTVisitor.h"
#include "Types.h"
#include "WGSLShaderModule.h"
#include <wtf/HashSet.h>
#include <wtf/text/MakeString.h>

namespace WGSL {

// WGSL evaluates the reference of an assignment's left hand side before it evaluates the value
// being assigned, and evaluates that reference exactly once even for a compound assignment.
// Metal inherits C++17's rules instead, where the right operand of `=` is sequenced *before* the
// left one, and where a compound assignment has to be expanded into `lhs = lhs op rhs`, which
// mentions the left hand side twice. Either difference is observable whenever an index expression
// in the assignment target calls a function that writes to memory.
//
// Both are fixed by pinning every side effecting index in an assignment target to a let declared
// immediately before the statement, which leaves the assignment itself indexing by plain values.
// The same applies to a pointer valued let, whose initializer the pointer rewriter inlines into
// each use: hoisting the indices out first keeps them from being evaluated at those uses instead.
class EvaluationOrderRewriter final : public AST::Visitor {
    using Base = AST::Visitor;
    using Base::visit;

public:
    EvaluationOrderRewriter(ShaderModule& shaderModule)
        : m_shaderModule(shaderModule)
    {
    }

    void run();

    void visit(AST::CompoundStatement&) override;
    void visit(AST::LoopStatement&) override;

private:
    void rewrite(AST::Statement::List&);
    void hoistAssignmentTarget(AST::Statement&);
    void hoistIndices(AST::Expression&);
    void hoist(AST::IndexAccessExpression&);
    bool mayHaveSideEffects(AST::Expression&);

    ShaderModule& m_shaderModule;
    HashSet<String> m_userFunctions;
    AST::Statement::List m_hoisted;
    unsigned m_hoistCount { 0 };
};

// Only a call to a user declared function or to an atomic built-in can write to memory that a
// later part of the same statement could observe. Every other call, including a type constructor
// or a built-in such as min(), is a pure computation whose position does not matter.
class SideEffectFinder final : public AST::Visitor {
    using Base = AST::Visitor;
    using Base::visit;

public:
    SideEffectFinder(const HashSet<String>& userFunctions)
        : m_userFunctions(userFunctions)
    {
    }

    bool check(AST::Expression& expression)
    {
        Base::visit(expression);
        return m_found;
    }

    void visit(AST::CallExpression& call) override
    {
        if (!call.isConstructor()) {
            if (auto* target = dynamicDowncast<AST::IdentifierExpression>(call.target())) {
                const auto& name = target->identifier().id();
                if (m_userFunctions.contains(name) || name.startsWith("atomic"_s))
                    m_found = true;
            }
        }
        Base::visit(call);
    }

private:
    const HashSet<String>& m_userFunctions;
    bool m_found { false };
};

void EvaluationOrderRewriter::run()
{
    for (auto& declaration : m_shaderModule.declarations()) {
        if (auto* function = dynamicDowncast<AST::Function>(declaration))
            m_userFunctions.add(function->name().id());
    }

    Base::visit(m_shaderModule);
}

void EvaluationOrderRewriter::rewrite(AST::Statement::List& statements)
{
    Vector<std::pair<unsigned, AST::Statement::List>> insertions;

    for (unsigned index = 0; index < statements.size(); ++index) {
        hoistAssignmentTarget(statements[index]);
        // The hoisted declarations have to be taken before descending into the statement, since a
        // nested block hoists into its own list.
        if (!m_hoisted.isEmpty())
            insertions.append({ index, std::exchange(m_hoisted, AST::Statement::List { }) });
        Base::visit(statements[index]);
    }

    // Inserting from the back leaves the indices of the earlier insertion points untouched.
    for (unsigned i = insertions.size(); i--;)
        m_shaderModule.insertVector(statements, insertions[i].first, insertions[i].second);
}

void EvaluationOrderRewriter::visit(AST::CompoundStatement& statement)
{
    rewrite(statement.statements());
}

void EvaluationOrderRewriter::visit(AST::LoopStatement& statement)
{
    rewrite(statement.body());
    if (auto& continuing = statement.continuing())
        rewrite(continuing->body);
}

void EvaluationOrderRewriter::hoistAssignmentTarget(AST::Statement& statement)
{
    switch (statement.kind()) {
    case AST::NodeKind::AssignmentStatement:
        hoistIndices(uncheckedDowncast<AST::AssignmentStatement>(statement).lhs());
        break;
    case AST::NodeKind::CompoundAssignmentStatement:
        hoistIndices(uncheckedDowncast<AST::CompoundAssignmentStatement>(statement).leftExpression());
        break;
    case AST::NodeKind::VariableStatement: {
        auto* initializer = uncheckedDowncast<AST::VariableStatement>(statement).variable().maybeInitializer();
        if (!initializer || !initializer->inferredType())
            break;
        if (std::holds_alternative<Types::Pointer>(*initializer->inferredType()))
            hoistIndices(*initializer);
        break;
    }
    default:
        break;
    }
}

void EvaluationOrderRewriter::hoistIndices(AST::Expression& expression)
{
    switch (expression.kind()) {
    case AST::NodeKind::IdentityExpression:
        hoistIndices(uncheckedDowncast<AST::IdentityExpression>(expression).expression());
        break;
    case AST::NodeKind::UnaryExpression:
        hoistIndices(uncheckedDowncast<AST::UnaryExpression>(expression).expression());
        break;
    case AST::NodeKind::PointerDereferenceExpression:
        hoistIndices(uncheckedDowncast<AST::PointerDereferenceExpression>(expression).target());
        break;
    case AST::NodeKind::FieldAccessExpression:
        hoistIndices(uncheckedDowncast<AST::FieldAccessExpression>(expression).base());
        break;
    case AST::NodeKind::IndexAccessExpression: {
        auto& access = uncheckedDowncast<AST::IndexAccessExpression>(expression);
        // The base sits to the left of the index, so anything hoisted out of it has to come first.
        hoistIndices(access.base());
        if (mayHaveSideEffects(access.index()))
            hoist(access);
        break;
    }
    default:
        break;
    }
}

bool EvaluationOrderRewriter::mayHaveSideEffects(AST::Expression& expression)
{
    // A const expression was already folded, so whatever it called happened at compile time.
    if (expression.constantValue())
        return false;

    auto* type = expression.inferredType();
    if (!type)
        return false;

    // An index is normally a plain integer. A reference or a pointer would need the hoisted let to
    // be declared with a type this pass has no way to spell, and cannot be produced by a call, so
    // leave those alone.
    if (std::holds_alternative<Types::Reference>(*type) || std::holds_alternative<Types::Pointer>(*type))
        return false;

    return SideEffectFinder(m_userFunctions).check(expression);
}

void EvaluationOrderRewriter::hoist(AST::IndexAccessExpression& access)
{
    auto& index = access.index();
    auto name = makeString("__evalOrder"_s, m_hoistCount++);

    auto& variable = m_shaderModule.astBuilder().construct<AST::Variable>(
        SourceSpan::empty(),
        AST::VariableFlavor::Let,
        AST::Identifier::make(name),
        nullptr,
        nullptr,
        &index,
        AST::Attribute::List { }
    );
    m_hoisted.append(m_shaderModule.astBuilder().construct<AST::VariableStatement>(
        SourceSpan::empty(),
        variable
    ));

    auto& identifier = m_shaderModule.astBuilder().construct<AST::IdentifierExpression>(
        SourceSpan::empty(),
        AST::Identifier::make(name)
    );
    identifier.m_inferredType = index.inferredType();

    auto& newAccess = m_shaderModule.astBuilder().construct<AST::IndexAccessExpression>(
        access.span(),
        access.base(),
        identifier
    );
    newAccess.m_inferredType = access.inferredType();

    m_shaderModule.replace(access, newAccess);
}

void rewriteEvaluationOrder(ShaderModule& shaderModule)
{
    EvaluationOrderRewriter(shaderModule).run();
}

} // namespace WGSL
