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
#include "TypeCheck.h"

#include "AST.h"
#include "ASTVisitor.h"
#include "ContextProviderInlines.h"
#include <wtf/SetForScope.h>
#include <wtf/Vector.h>

namespace WGSL {

class UnificationContext;

struct Type {
    enum Kind {
        Variable,
        Primitive,
        Constructor,
        Application,
        Function,
    };
};

using SubstituionCallback = std::function<void(Type*)>;

class TypeChecker : public AST::Visitor, public ContextProvider<Type*> {
    friend class UnificationContext;

public:
    TypeChecker(AST::ShaderModule& shaderModule)
        : m_shaderModule(shaderModule)
    {
    }

    void check();

    void visit(AST::Structure&) override;
    void visit(AST::Variable&) override;
    void visit(AST::Function&) override;
    void visit(AST::Statement&) override;
    void visit(AST::AssignmentStatement&) override;
    void visit(AST::ReturnStatement&) override;
    void visit(AST::Expression&) override;
    void visit(AST::FieldAccessExpression&) override;
    void visit(AST::IndexAccessExpression&) override;
    void visit(AST::BinaryExpression&) override;
    void visit(AST::IdentifierExpression&) override;
    void visit(AST::TypeName&) override;

private:
    Type* infer(AST::Expression&);
    Type* variable();
    Type* resolve(AST::TypeName&);
    void inferred(Type*);
    void unify(Type*, Type*);

    void substitute(Type*, SubstituionCallback&&);

    AST::ShaderModule& m_shaderModule;
    Type* m_inferredType { nullptr };
    UnificationContext* m_unificationContext { nullptr };

    // FIXME: remove this once we start allocating types
    Type m_type;
};

class UnificationContext {
public:
    UnificationContext(TypeChecker* typeChecker)
        : m_setForScope(typeChecker->m_unificationContext, this)
    {
    }

    ~UnificationContext()
    {
        solveConstraints();
        runSubstitutions();
    }

    void unify(Type* lhs, Type* rhs)
    {
        m_constraints.append({ lhs, rhs });
    }

    void substitute(Type* type, SubstituionCallback&& callback)
    {
        m_substitutions.append({ type, WTFMove(callback) });
    }

private:
    void solveConstraints()
    {
        // FIXME: implement this
    }

    void runSubstitutions()
    {
        // FIXME: implement this
    }

    SetForScope<UnificationContext*> m_setForScope;
    Vector<std::pair<Type*, Type*>> m_constraints;
    Vector<std::pair<Type*, SubstituionCallback>> m_substitutions;
};

void TypeChecker::check()
{
    for (auto& structure : m_shaderModule.structures())
        visit(structure);

    for (auto& variable : m_shaderModule.variables())
        visit(variable);

    for (auto& function : m_shaderModule.functions())
        visit(function);

    for (auto& function : m_shaderModule.functions())
        visit(function.body());
}

// Declarations
void TypeChecker::visit(AST::Structure& structure)
{
    UnificationContext declarationContext(this);

    // FIXME: allocate and build struct type from struct members
    Type* structType = nullptr;
    ContextProvider::introduceVariable(structure.name(), structType);
}

void TypeChecker::visit(AST::Variable& variable)
{
    UnificationContext declarationContext(this);

    auto* result = TypeChecker::variable();
    if (variable.maybeTypeName())
        unify(result, resolve(*variable.maybeTypeName()));
    if (variable.maybeInitializer())
        unify(result, infer(*variable.maybeInitializer()));
    ContextProvider::introduceVariable(variable.name(), result);
}

void TypeChecker::visit(AST::Function& function)
{
    UnificationContext declarationContext(this);

    // FIXME: allocate and build function type fromp parameters and return type
    Type* functionType = nullptr;
    ContextProvider::introduceVariable(function.name(), functionType);
}

// Statements
void TypeChecker::visit(AST::Statement& statement)
{
    UnificationContext statementContext(this);
    AST::Visitor::visit(statement);
}

void TypeChecker::visit(AST::AssignmentStatement& statement)
{
    auto* lhs = infer(statement.lhs());
    auto* rhs = infer(statement.rhs());
    unify(lhs, rhs);
}

void TypeChecker::visit(AST::ReturnStatement& statement)
{
    // FIXME: handle functions that return void
    auto* type = infer(*statement.maybeExpression());

    // FIXME: unify type with the curent function's return type
    UNUSED_PARAM(type);
}

// Expressions
void TypeChecker::visit(AST::Expression&)
{
    // FIXME: remove this function once we start allocating types
    inferred(&m_type);
}

void TypeChecker::visit(AST::FieldAccessExpression& access)
{
    auto* structType = infer(access.base());
    // FIXME: implement member lookup once we have a struct type
    UNUSED_PARAM(structType);

    Type* fieldType = nullptr;
    inferred(fieldType);
}

void TypeChecker::visit(AST::IndexAccessExpression& access)
{
    // FIXME: handle reference index access
    auto* arrayType = infer(access.base());
    auto* index = infer(access.index());

    // FIXME: unify index with UnionType(i32, u32)
    UNUSED_PARAM(index);

    // FIXME: set the inferred type to the array's member type
    UNUSED_PARAM(arrayType);
}

void TypeChecker::visit(AST::BinaryExpression& binary)
{
    // FIXME: this needs to resolve overloads, not just unify both types
    auto* result = variable();
    auto* leftType = infer(binary.leftExpression());
    auto* rightType = infer(binary.rightExpression());
    unify(result, leftType);
    unify(result, rightType);
    inferred(result);
}

void TypeChecker::visit(AST::IdentifierExpression& identifier)
{
    auto* const* type = ContextProvider::readVariable(identifier.identifier());
    // FIXME: this should be unconditional
    ASSERT(type);
    if (type)
        inferred(*type);
}

// Types
void TypeChecker::visit(AST::TypeName&)
{
    // FIXME: remove this function once we start allocating types
    inferred(&m_type);
}

// Private helpers
Type* TypeChecker::infer(AST::Expression& expression)
{
    ASSERT(!m_inferredType);
    // FIXME: this should call the base class and TypeChecker::visit should assert
    // that it is never called directly on expressions
    visit(expression);
    ASSERT(m_inferredType);

    auto* type = m_inferredType;
    substitute(type, [&](Type* resolvedType) {
        // FIXME: store resolved type in the expression
        UNUSED_PARAM(resolvedType);
    });
    m_inferredType = nullptr;

    return type;
}

Type* TypeChecker::variable()
{
    // FIXME: allocate new type variables
    return nullptr;
}

Type* TypeChecker::resolve(AST::TypeName& type)
{
    ASSERT(!m_inferredType);
    // FIXME: this should call the base class and TypeChecker::visit should assert
    // that it is never called directly on types
    visit(type);
    ASSERT(m_inferredType);

    auto* inferredType = m_inferredType;
    substitute(inferredType, [&](Type* resolvedType) {
        // FIXME: store resolved type in the AST type
        UNUSED_PARAM(resolvedType);
    });
    m_inferredType = nullptr;

    return inferredType;
}

void TypeChecker::inferred(Type* type)
{
    ASSERT(type);
    m_inferredType = type;
}

void TypeChecker::unify(Type* lhs, Type* rhs)
{
    m_unificationContext->unify(lhs, rhs);
}

void TypeChecker::substitute(Type* type, SubstituionCallback&& callback)
{
    m_unificationContext->substitute(type, WTFMove(callback));
}

void typeCheck(AST::ShaderModule& shaderModule)
{
    TypeChecker(shaderModule).check();
}

} // namespace WGSL
