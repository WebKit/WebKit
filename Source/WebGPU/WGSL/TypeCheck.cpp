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
#include "ASTStringDumper.h"
#include "ASTVisitor.h"
#include "ContextProviderInlines.h"
#include "Types.h"
#include <wtf/DataLog.h>
#include <wtf/FixedVector.h>
#include <wtf/SetForScope.h>
#include <wtf/Vector.h>

namespace WGSL {

static constexpr bool shouldDumpInferredTypes = false;

class TypeChecker : public AST::Visitor, public ContextProvider<Type*> {
public:
    TypeChecker(AST::ShaderModule&);

    void check();

    // Declarations
    void visit(AST::Structure&) override;
    void visit(AST::Variable&) override;
    void visit(AST::Function&) override;

    // Statements
    void visit(AST::AssignmentStatement&) override;
    void visit(AST::ReturnStatement&) override;

    // Expressions
    void visit(AST::Expression&) override;
    void visit(AST::FieldAccessExpression&) override;
    void visit(AST::IndexAccessExpression&) override;
    void visit(AST::BinaryExpression&) override;
    void visit(AST::IdentifierExpression&) override;
    void visit(AST::CallExpression&) override;

    // Literal Expressions
    void visit(AST::BoolLiteral&) override;
    void visit(AST::Signed32Literal&) override;
    void visit(AST::Float32Literal&) override;
    void visit(AST::Unsigned32Literal&) override;
    void visit(AST::AbstractIntegerLiteral&) override;
    void visit(AST::AbstractFloatLiteral&) override;

    // Types
    void visit(AST::TypeName&) override;
    void visit(AST::ArrayTypeName&) override;
    void visit(AST::NamedTypeName&) override;
    void visit(AST::ParameterizedTypeName&) override;
    void visit(AST::StructTypeName&) override;
    void visit(AST::ReferenceTypeName&) override;

private:
    void visitFunctionBody(AST::Function&);

    Type* infer(AST::Expression&);
    Type* resolve(AST::TypeName&);
    void inferred(Type*);
    void unify(Type*, Type*);

    template<typename TypeKind, typename... Arguments>
    Type* allocateType(Arguments&&...);

    AST::ShaderModule& m_shaderModule;
    Type* m_inferredType { nullptr };

    WTF::Vector<std::unique_ptr<Type>> m_types;
    Type* m_abstractInt;
    Type* m_abstractFloat;
    Type* m_void;
    Type* m_bool;
    Type* m_i32;
    Type* m_u32;
    Type* m_f32;
    FixedVector<TypeConstructor> m_typeConstrutors;
};

TypeChecker::TypeChecker(AST::ShaderModule& shaderModule)
    : m_shaderModule(shaderModule)
    , m_typeConstrutors(AST::ParameterizedTypeName::NumberOfBaseTypes)
{
    m_abstractInt = allocateType<Primitive>(Primitive::AbstractInt);
    m_abstractFloat = allocateType<Primitive>(Primitive::AbstractFloat);
    m_void = allocateType<Primitive>(Primitive::Void);
    m_bool = allocateType<Primitive>(Primitive::Bool);
    m_i32 = allocateType<Primitive>(Primitive::I32);
    m_u32 = allocateType<Primitive>(Primitive::U32);
    m_f32 = allocateType<Primitive>(Primitive::F32);

    ContextProvider::introduceVariable(AST::Identifier::make("void"_s), m_void);
    ContextProvider::introduceVariable(AST::Identifier::make("bool"_s), m_bool);
    ContextProvider::introduceVariable(AST::Identifier::make("i32"_s), m_i32);
    ContextProvider::introduceVariable(AST::Identifier::make("u32"_s), m_u32);
    ContextProvider::introduceVariable(AST::Identifier::make("f32"_s), m_f32);

#define FOR_EACH_BASE(f) \
    f(Vec2, Vector, static_cast<uint8_t>(2)) \
    f(Vec3, Vector, static_cast<uint8_t>(3)) \
    f(Vec4, Vector, static_cast<uint8_t>(4)) \
    f(Mat2x2, Matrix, static_cast<uint8_t>(2), static_cast<uint8_t>(2)) \
    f(Mat2x3, Matrix, static_cast<uint8_t>(2), static_cast<uint8_t>(3)) \
    f(Mat2x4, Matrix, static_cast<uint8_t>(2), static_cast<uint8_t>(4)) \
    f(Mat3x2, Matrix, static_cast<uint8_t>(3), static_cast<uint8_t>(2)) \
    f(Mat3x3, Matrix, static_cast<uint8_t>(3), static_cast<uint8_t>(3)) \
    f(Mat3x4, Matrix, static_cast<uint8_t>(3), static_cast<uint8_t>(4)) \
    f(Mat4x2, Matrix, static_cast<uint8_t>(4), static_cast<uint8_t>(2)) \
    f(Mat4x3, Matrix, static_cast<uint8_t>(4), static_cast<uint8_t>(3)) \
    f(Mat4x4, Matrix, static_cast<uint8_t>(4), static_cast<uint8_t>(4))

#define DEFINE_TYPE_CONSTRUCTOR(base, type, ...) \
    m_typeConstrutors[WTF::enumToUnderlyingType(AST::ParameterizedTypeName::Base::base)] = \
        TypeConstructor { [this](Type* elementType) -> Type* { \
            /* FIXME: this should be cached */ \
            return allocateType<type>(elementType, __VA_ARGS__); \
        } };

    FOR_EACH_BASE(DEFINE_TYPE_CONSTRUCTOR)

#undef DEFINE_TYPE_CONSTRUCTOR
#undef FOR_EACH_BASE
}

void TypeChecker::check()
{
    // FIXME: fill in struct fields in a second pass since declarations might be
    // out of order
    for (auto& structure : m_shaderModule.structures())
        visit(structure);

    for (auto& variable : m_shaderModule.variables())
        visit(variable);

    for (auto& function : m_shaderModule.functions())
        visit(function);

    for (auto& function : m_shaderModule.functions())
        visitFunctionBody(function);
}

// Declarations
void TypeChecker::visit(AST::Structure& structure)
{
    Type* structType = allocateType<Struct>(structure.name());
    ContextProvider::introduceVariable(structure.name(), structType);
}

void TypeChecker::visit(AST::Variable& variable)
{
    Type* result = nullptr;
    if (variable.maybeTypeName())
        result = resolve(*variable.maybeTypeName());
    if (variable.maybeInitializer()) {
        auto* initializerType = infer(*variable.maybeInitializer());
        if (result)
            unify(result, initializerType);
        else
            result = initializerType;
    }
    ContextProvider::introduceVariable(variable.name(), result);
}

void TypeChecker::visit(AST::Function& function)
{
    // FIXME: allocate and build function type fromp parameters and return type
    Type* functionType = nullptr;
    ContextProvider::introduceVariable(function.name(), functionType);
}

void TypeChecker::visitFunctionBody(AST::Function& function)
{
    ContextProvider::ContextScope functionContext(this);

    for (auto& parameter : function.parameters()) {
        auto* parameterType = resolve(parameter.typeName());
        ContextProvider::introduceVariable(parameter.name(), parameterType);
    }

    AST::Visitor::visit(function.body());
}

// Statements
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
    // NOTE: this should never be called directly, only through `resolve`, which
    // captures the inferred type
    ASSERT_NOT_REACHED();
}

void TypeChecker::visit(AST::FieldAccessExpression& access)
{
    auto* structType = infer(access.base());
    // FIXME: implement member lookup once we have a struct type
    inferred(structType);
}

void TypeChecker::visit(AST::IndexAccessExpression& access)
{
    // FIXME: handle reference index access
    auto* arrayType = infer(access.base());
    auto* index = infer(access.index());

    // FIXME: unify index with UnionType(i32, u32)
    UNUSED_PARAM(index);

    // FIXME: set the inferred type to the array's member type
    inferred(arrayType);
}

void TypeChecker::visit(AST::BinaryExpression& binary)
{
    // FIXME: this needs to resolve overloads, not just unify both types
    auto* leftType = infer(binary.leftExpression());
    auto* rightType = infer(binary.rightExpression());
    unify(leftType, rightType);
    inferred(leftType);
}

void TypeChecker::visit(AST::IdentifierExpression& identifier)
{
    auto* const* type = ContextProvider::readVariable(identifier.identifier());
    // FIXME: report error about unknown identifier
    ASSERT(type);
    inferred(*type);
}

void TypeChecker::visit(AST::CallExpression& call)
{
    auto* target = resolve(call.target());
    // FIXME: validate arguments
    inferred(target);
}

// Literal Expressions
void TypeChecker::visit(AST::BoolLiteral&)
{
    inferred(m_bool);
}

void TypeChecker::visit(AST::Signed32Literal&)
{
    inferred(m_i32);
}

void TypeChecker::visit(AST::Float32Literal&)
{
    inferred(m_f32);
}

void TypeChecker::visit(AST::Unsigned32Literal&)
{
    inferred(m_u32);
}

void TypeChecker::visit(AST::AbstractIntegerLiteral&)
{
    inferred(m_abstractInt);
}

void TypeChecker::visit(AST::AbstractFloatLiteral&)
{
    inferred(m_abstractFloat);
}

// Types
void TypeChecker::visit(AST::TypeName&)
{
    // NOTE: this should never be called directly, only through `resolve`, which
    // captures the inferred type
    ASSERT_NOT_REACHED();
}

void TypeChecker::visit(AST::ArrayTypeName&)
{
    // FIXME: implement this
    inferred(m_void);
}

void TypeChecker::visit(AST::NamedTypeName& namedType)
{
    auto* const* type = ContextProvider::readVariable(namedType.name());
    // FIXME: report error about unknown type name
    ASSERT(type);
    inferred(*type);
}

void TypeChecker::visit(AST::ParameterizedTypeName&)
{
    // FIXME: implement this
    inferred(m_void);
}

void TypeChecker::visit(AST::StructTypeName& structType)
{
    auto* const* type = ContextProvider::readVariable(structType.structure().name());
    // FIXME: report error about unknown type name
    ASSERT(type);
    inferred(*type);
}

void TypeChecker::visit(AST::ReferenceTypeName&)
{
    // FIXME: we don't yet parse reference types
    ASSERT_NOT_REACHED();
}

// Private helpers
Type* TypeChecker::infer(AST::Expression& expression)
{
    ASSERT(!m_inferredType);
    AST::Visitor::visit(expression);
    ASSERT(m_inferredType);

    auto* type = m_inferredType;

    if (shouldDumpInferredTypes) {
        dataLog("> Type inference [expression]: ");
        dumpNode(WTF::dataFile(), expression);
        dataLog(" : ");
        dataLogLn(*type);
    }

    // FIXME: store resolved type in the expression
    m_inferredType = nullptr;

    return type;
}

Type* TypeChecker::resolve(AST::TypeName& type)
{
    ASSERT(!m_inferredType);
    // FIXME: this should call the base class and TypeChecker::visit should assert
    // that it is never called directly on types
    AST::Visitor::visit(type);
    ASSERT(m_inferredType);

    auto* inferredType = m_inferredType;

    if (shouldDumpInferredTypes) {
        dataLog("> Type inference [type]: ");
        dumpNode(WTF::dataFile(), type);
        dataLog(" : ");
        dataLogLn(*inferredType);
    }

    // FIXME: store resolved type in the AST type
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
    // FIXME: Implement all the rules and report a type error otherwise
    if (lhs == rhs)
        return;
}

template<typename TypeKind, typename... Arguments>
Type* TypeChecker::allocateType(Arguments&&... arguments)
{
    m_types.append(std::unique_ptr<Type>(new Type(TypeKind { std::forward<Arguments>(arguments)... })));
    return m_types.last().get();
}

void typeCheck(AST::ShaderModule& shaderModule)
{
    TypeChecker(shaderModule).check();
}

} // namespace WGSL
