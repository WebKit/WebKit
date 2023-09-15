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
#include "CompilationMessage.h"
#include "ContextProviderInlines.h"
#include "Overload.h"
#include "TypeStore.h"
#include "Types.h"
#include "WGSLShaderModule.h"
#include <wtf/DataLog.h>
#include <wtf/SortedArrayMap.h>

namespace WGSL {

static constexpr bool shouldDumpInferredTypes = false;

struct Binding {
    enum Kind : uint8_t {
        Value,
        Type,
    };

    Kind kind;
    const struct Type* type;
};

class TypeChecker : public AST::Visitor, public ContextProvider<Binding> {
public:
    TypeChecker(ShaderModule&);

    std::optional<FailedCheck> check();

    // Declarations
    void visit(AST::Structure&) override;
    void visit(AST::Variable&) override;
    void visit(AST::Function&) override;

    // Attributes
    void visit(AST::AlignAttribute&) override;
    void visit(AST::BindingAttribute&) override;
    void visit(AST::GroupAttribute&) override;
    void visit(AST::IdAttribute&) override;
    void visit(AST::LocationAttribute&) override;
    void visit(AST::SizeAttribute&) override;
    void visit(AST::WorkgroupSizeAttribute&) override;

    // Statements
    void visit(AST::AssignmentStatement&) override;
    void visit(AST::CompoundAssignmentStatement&) override;
    void visit(AST::DecrementIncrementStatement&) override;
    void visit(AST::IfStatement&) override;
    void visit(AST::PhonyAssignmentStatement&) override;
    void visit(AST::ReturnStatement&) override;
    void visit(AST::CompoundStatement&) override;
    void visit(AST::ForStatement&) override;

    // Expressions
    void visit(AST::Expression&) override;
    void visit(AST::FieldAccessExpression&) override;
    void visit(AST::IndexAccessExpression&) override;
    void visit(AST::BinaryExpression&) override;
    void visit(AST::IdentifierExpression&) override;
    void visit(AST::CallExpression&) override;
    void visit(AST::UnaryExpression&) override;

    // Literal Expressions
    void visit(AST::BoolLiteral&) override;
    void visit(AST::Signed32Literal&) override;
    void visit(AST::Float32Literal&) override;
    void visit(AST::Unsigned32Literal&) override;
    void visit(AST::AbstractIntegerLiteral&) override;
    void visit(AST::AbstractFloatLiteral&) override;

    // Types
    void visit(AST::ArrayTypeExpression&) override;
    void visit(AST::ElaboratedTypeExpression&) override;
    void visit(AST::ReferenceTypeExpression&) override;

private:
    enum class VariableKind : uint8_t {
        Local,
        Global,
    };

    void visitVariable(AST::Variable&, VariableKind);
    const Type* vectorFieldAccess(const Types::Vector&, AST::FieldAccessExpression&);
    void visitAttributes(AST::Attribute::List&);

    template<typename... Arguments>
    void typeError(const SourceSpan&, Arguments&&...);

    enum class InferBottom : bool { No, Yes };
    template<typename... Arguments>
    void typeError(InferBottom, const SourceSpan&, Arguments&&...);

    const Type* infer(AST::Expression&);
    const Type* resolve(AST::Expression&);
    const Type* lookupType(const AST::Identifier&);
    void inferred(const Type*);
    bool unify(const Type*, const Type*) WARN_UNUSED_RETURN;
    bool isBottom(const Type*) const;
    void introduceType(const AST::Identifier&, const Type*);
    void introduceValue(const AST::Identifier&, const Type*);

    template<typename TargetConstructor, typename... Arguments>
    void allocateSimpleConstructor(ASCIILiteral, TargetConstructor, Arguments&&...);
    void allocateTextureStorageConstructor(ASCIILiteral, Types::TextureStorage::Kind);

    template<typename CallArguments>
    const Type* chooseOverload(const char*, const SourceSpan&, const String&, CallArguments&& valueArguments, const Vector<const Type*>& typeArguments);

    ShaderModule& m_shaderModule;
    const Type* m_inferredType { nullptr };

    TypeStore& m_types;
    Vector<Error> m_errors;
    // FIXME: maybe these should live in the context
    HashMap<String, Vector<OverloadCandidate>> m_overloadedOperations;
};

TypeChecker::TypeChecker(ShaderModule& shaderModule)
    : m_shaderModule(shaderModule)
    , m_types(shaderModule.types())
{
    introduceType(AST::Identifier::make("bool"_s), m_types.boolType());
    introduceType(AST::Identifier::make("i32"_s), m_types.i32Type());
    introduceType(AST::Identifier::make("u32"_s), m_types.u32Type());
    introduceType(AST::Identifier::make("f32"_s), m_types.f32Type());
    introduceType(AST::Identifier::make("sampler"_s), m_types.samplerType());
    introduceType(AST::Identifier::make("texture_external"_s), m_types.textureExternalType());

    allocateSimpleConstructor("vec2"_s, &TypeStore::vectorType, 2);
    allocateSimpleConstructor("vec3"_s, &TypeStore::vectorType, 3);
    allocateSimpleConstructor("vec4"_s, &TypeStore::vectorType, 4);
    allocateSimpleConstructor("mat2x2"_s, &TypeStore::matrixType, 2, 2);
    allocateSimpleConstructor("mat2x3"_s, &TypeStore::matrixType, 2, 3);
    allocateSimpleConstructor("mat2x4"_s, &TypeStore::matrixType, 2, 4);
    allocateSimpleConstructor("mat3x2"_s, &TypeStore::matrixType, 3, 2);
    allocateSimpleConstructor("mat3x3"_s, &TypeStore::matrixType, 3, 3);
    allocateSimpleConstructor("mat3x4"_s, &TypeStore::matrixType, 3, 4);
    allocateSimpleConstructor("mat4x2"_s, &TypeStore::matrixType, 4, 2);
    allocateSimpleConstructor("mat4x3"_s, &TypeStore::matrixType, 4, 3);
    allocateSimpleConstructor("mat4x4"_s, &TypeStore::matrixType, 4, 4);

    allocateSimpleConstructor("texture_1d"_s, &TypeStore::textureType, Types::Texture::Kind::Texture1d);
    allocateSimpleConstructor("texture_2d"_s, &TypeStore::textureType, Types::Texture::Kind::Texture2d);
    allocateSimpleConstructor("texture_2d_array"_s, &TypeStore::textureType, Types::Texture::Kind::Texture2dArray);
    allocateSimpleConstructor("texture_3d"_s, &TypeStore::textureType, Types::Texture::Kind::Texture3d);
    allocateSimpleConstructor("texture_cube"_s, &TypeStore::textureType, Types::Texture::Kind::TextureCube);
    allocateSimpleConstructor("texture_cube_array"_s, &TypeStore::textureType, Types::Texture::Kind::TextureCubeArray);
    allocateSimpleConstructor("texture_multisampled_2d"_s, &TypeStore::textureType, Types::Texture::Kind::TextureMultisampled2d);

    allocateTextureStorageConstructor("texture_storage_1d"_s, Types::TextureStorage::Kind::TextureStorage1d);
    allocateTextureStorageConstructor("texture_storage_2d"_s, Types::TextureStorage::Kind::TextureStorage2d);
    allocateTextureStorageConstructor("texture_storage_2d_array"_s, Types::TextureStorage::Kind::TextureStorage2dArray);
    allocateTextureStorageConstructor("texture_storage_3d"_s, Types::TextureStorage::Kind::TextureStorage3d);

    introduceValue(AST::Identifier::make("read"_s), m_types.accessModeType());
    introduceValue(AST::Identifier::make("write"_s), m_types.accessModeType());
    introduceValue(AST::Identifier::make("read_write"_s), m_types.accessModeType());

    introduceValue(AST::Identifier::make("bgra8unorm"_s), m_types.texelFormatType());
    introduceValue(AST::Identifier::make("r32float"_s), m_types.texelFormatType());
    introduceValue(AST::Identifier::make("r32sint"_s), m_types.texelFormatType());
    introduceValue(AST::Identifier::make("r32uint"_s), m_types.texelFormatType());
    introduceValue(AST::Identifier::make("rg32float"_s), m_types.texelFormatType());
    introduceValue(AST::Identifier::make("rg32sint"_s), m_types.texelFormatType());
    introduceValue(AST::Identifier::make("rg32uint"_s), m_types.texelFormatType());
    introduceValue(AST::Identifier::make("rgba16float"_s), m_types.texelFormatType());
    introduceValue(AST::Identifier::make("rgba16sint"_s), m_types.texelFormatType());
    introduceValue(AST::Identifier::make("rgba16uint"_s), m_types.texelFormatType());
    introduceValue(AST::Identifier::make("rgba32float"_s), m_types.texelFormatType());
    introduceValue(AST::Identifier::make("rgba32sint"_s), m_types.texelFormatType());
    introduceValue(AST::Identifier::make("rgba32uint"_s), m_types.texelFormatType());
    introduceValue(AST::Identifier::make("rgba8sint"_s), m_types.texelFormatType());
    introduceValue(AST::Identifier::make("rgba8snorm"_s), m_types.texelFormatType());
    introduceValue(AST::Identifier::make("rgba8uint"_s), m_types.texelFormatType());
    introduceValue(AST::Identifier::make("rgba8unorm"_s), m_types.texelFormatType());

    // This file contains the declarations generated from `TypeDeclarations.rb`
#include "TypeDeclarations.h" // NOLINT
}

std::optional<FailedCheck> TypeChecker::check()
{
    // FIXME: fill in struct fields in a second pass since declarations might be
    // out of order
    for (auto& structure : m_shaderModule.structures())
        visit(structure);

    for (auto& variable : m_shaderModule.variables())
        visitVariable(variable, VariableKind::Global);

    for (auto& function : m_shaderModule.functions())
        visit(function);

    if (shouldDumpInferredTypes) {
        for (auto& error : m_errors)
            dataLogLn(error);
    }

    if (m_errors.isEmpty())
        return std::nullopt;

    // FIXME: add support for warnings
    Vector<Warning> warnings { };
    return FailedCheck { WTFMove(m_errors), WTFMove(warnings) };
}

// Declarations
void TypeChecker::visit(AST::Structure& structure)
{
    HashMap<String, const Type*> fields;
    for (auto& member : structure.members()) {
        visitAttributes(member.attributes());
        auto* memberType = resolve(member.type());
        auto result = fields.add(member.name().id(), memberType);
        ASSERT_UNUSED(result, result.isNewEntry);
    }
    const Type* structType = m_types.structType(structure, WTFMove(fields));
    introduceType(structure.name(), structType);
}

void TypeChecker::visit(AST::Variable& variable)
{
    visitVariable(variable, VariableKind::Local);
}

void TypeChecker::visitVariable(AST::Variable& variable, VariableKind variableKind)
{
    visitAttributes(variable.attributes());

    const Type* result = nullptr;
    if (variable.maybeTypeName())
        result = resolve(*variable.maybeTypeName());
    if (variable.maybeInitializer()) {
        auto* initializerType = infer(*variable.maybeInitializer());
        if (auto* reference = std::get_if<Types::Reference>(initializerType)) {
            initializerType = reference->element;
            variable.maybeInitializer()->m_inferredType = initializerType;
        }

        if (!result)
            result = initializerType;
        else if (unify(result, initializerType))
            variable.maybeInitializer()->m_inferredType = result;
        else
            typeError(InferBottom::No, variable.span(), "cannot initialize var of type '", *result, "' with value of type '", *initializerType, "'");
    }
    if (variable.flavor() == AST::VariableFlavor::Var) {
        AddressSpace addressSpace;
        AccessMode accessMode;
        if (auto* maybeQualifier = variable.maybeQualifier()) {
            addressSpace = static_cast<AddressSpace>(maybeQualifier->storageClass());
            accessMode = static_cast<AccessMode>(maybeQualifier->accessMode());
        } else if (variableKind == VariableKind::Local) {
            addressSpace = AddressSpace::Function;
            accessMode = AccessMode::ReadWrite;
        } else {
            addressSpace = AddressSpace::Handle;
            accessMode = AccessMode::Read;
        }
        result = m_types.referenceType(addressSpace, result, accessMode);
        if (auto* maybeTypeName = variable.maybeTypeName()) {
            auto& referenceType = m_shaderModule.astBuilder().construct<AST::ReferenceTypeExpression>(
                maybeTypeName->span(),
                *maybeTypeName
            );
            referenceType.m_inferredType = result;
            variable.m_referenceType = &referenceType;
        }
    }
    introduceValue(variable.name(), result);
}

void TypeChecker::visit(AST::Function& function)
{
    visitAttributes(function.attributes());

    Vector<const Type*> parameters;
    const Type* result;
    parameters.reserveInitialCapacity(function.parameters().size());
    for (auto& parameter : function.parameters())
        parameters.append(resolve(parameter.typeName()));
    if (function.maybeReturnType())
        result = resolve(*function.maybeReturnType());
    else
        result = m_types.voidType();

    const Type* functionType = m_types.functionType(WTFMove(parameters), result);
    introduceValue(function.name(), functionType);

    ContextScope functionContext(this);
    for (auto& parameter : function.parameters()) {
        auto* parameterType = resolve(parameter.typeName());
        introduceValue(parameter.name(), parameterType);
    }
    AST::Visitor::visit(function.body());
}

// Attributes
void TypeChecker::visit(AST::AlignAttribute& attribute)
{
    auto* type = infer(attribute.alignment());
    if (!satisfies(type, Constraints::ConcreteInteger))
        typeError(InferBottom::No, attribute.span(), "@align must be an i32 or u32 value");
}

void TypeChecker::visit(AST::BindingAttribute& attribute)
{
    auto* type = infer(attribute.binding());
    if (!satisfies(type, Constraints::ConcreteInteger))
        typeError(InferBottom::No, attribute.span(), "@binding must be an i32 or u32 value");
}

void TypeChecker::visit(AST::GroupAttribute& attribute)
{
    auto* type = infer(attribute.group());
    if (!satisfies(type, Constraints::ConcreteInteger))
        typeError(InferBottom::No, attribute.span(), "@group must be an i32 or u32 value");
}

void TypeChecker::visit(AST::IdAttribute& attribute)
{
    auto* type = infer(attribute.value());
    if (!satisfies(type, Constraints::ConcreteInteger))
        typeError(InferBottom::No, attribute.span(), "@id must be an i32 or u32 value");
}

void TypeChecker::visit(AST::LocationAttribute& attribute)
{
    auto* type = infer(attribute.location());
    if (!satisfies(type, Constraints::ConcreteInteger))
        typeError(InferBottom::No, attribute.span(), "@location must be an i32 or u32 value");
}

void TypeChecker::visit(AST::SizeAttribute& attribute)
{
    auto* type = infer(attribute.size());
    if (!satisfies(type, Constraints::ConcreteInteger))
        typeError(InferBottom::No, attribute.span(), "@size must be an i32 or u32 value");
}

void TypeChecker::visit(AST::WorkgroupSizeAttribute& attribute)
{
    auto* xType = infer(attribute.x());
    if (!satisfies(xType, Constraints::ConcreteInteger)) {
        typeError(InferBottom::No, attribute.span(), "@workgroup_size x dimension must be an i32 or u32 value");
        return;
    }

    const Type* yType = nullptr;
    const Type* zType = nullptr;
    if (auto* y = attribute.maybeY()) {
        yType = infer(*y);
        if (!satisfies(yType, Constraints::ConcreteInteger)) {
            typeError(InferBottom::No, attribute.span(), "@workgroup_size y dimension must be an i32 or u32 value");
            return;
        }

        if (auto* z = attribute.maybeZ()) {
            zType = infer(*z);
            if (!satisfies(zType, Constraints::ConcreteInteger)) {
                typeError(InferBottom::No, attribute.span(), "@workgroup_size z dimension must be an i32 or u32 value");
                return;
            }
        }

    }

    const auto& satisfies = [&](const Type* type) {
        return unify(type, xType)
            && (!yType || unify(type, yType))
            && (!zType || unify(type, zType));
    };

    if (!satisfies(m_types.i32Type()) && !satisfies(m_types.u32Type()))
        typeError(InferBottom::No, attribute.span(), "@workgroup_size arguments must be of the same type, either i32 or u32");
}

// Statements
void TypeChecker::visit(AST::AssignmentStatement& statement)
{
    auto* lhs = infer(statement.lhs());
    auto* rhs = infer(statement.rhs());

    if (isBottom(lhs))
        return;

    auto* reference = std::get_if<Types::Reference>(lhs);
    if (!reference) {
        typeError(InferBottom::No, statement.span(), "cannot assign to a value of type '", *lhs, "'");
        return;
    }
    if (reference->accessMode == AccessMode::Read) {
        typeError(InferBottom::No, statement.span(), "cannot store into a read-only type '", *lhs, "'");
        return;
    }
    if (!unify(reference->element, rhs))
        typeError(InferBottom::No, statement.span(), "cannot assign value of type '", *rhs, "' to '", *reference->element, "'");
}

void TypeChecker::visit(AST::CompoundAssignmentStatement& statement)
{
    // FIXME: Implement type checking - infer is called to avoid ASSERT in
    // TypeChecker::visit(AST::Expression&)
    infer(statement.leftExpression());
    infer(statement.rightExpression());
}

void TypeChecker::visit(AST::DecrementIncrementStatement& statement)
{
    auto* expression = infer(statement.expression());
    if (isBottom(expression))
        return;

    auto* reference = std::get_if<Types::Reference>(expression);
    if (!reference) {
        typeError(InferBottom::No, statement.span(), "cannot modify a value of type '", *expression, "'");
        return;
    }
    if (reference->accessMode == AccessMode::Read) {
        typeError(InferBottom::No, statement.span(), "cannot modify read-only type '", *expression, "'");
        return;
    }
    if (!unify(m_types.i32Type(), reference->element) && !unify(m_types.u32Type(), reference->element)) {
        const char* operation;
        switch (statement.operation()) {
        case AST::DecrementIncrementStatement::Operation::Increment:
            operation = "increment";
            break;
        case AST::DecrementIncrementStatement::Operation::Decrement:
            operation = "decrement";
            break;
        }
        typeError(InferBottom::No, statement.span(), operation, " can only be applied to integers, found ", *reference->element);
    }
}

void TypeChecker::visit(AST::IfStatement& statement)
{
    auto* test = infer(statement.test());

    if (!unify(test, m_types.boolType()))
        typeError(statement.test().span(), "expected 'bool', found ", *test);

    AST::Visitor::visit(statement.trueBody());
    if (statement.maybeFalseBody())
        AST::Visitor::visit(*statement.maybeFalseBody());
}

void TypeChecker::visit(AST::PhonyAssignmentStatement& statement)
{
    infer(statement.rhs());
    // There is nothing to unify with since result of the right-hand side is
    // discarded.
}

void TypeChecker::visit(AST::ReturnStatement& statement)
{
    // FIXME: handle functions that return void
    auto* type = infer(*statement.maybeExpression());

    // FIXME: unify type with the curent function's return type
    UNUSED_PARAM(type);
}

void TypeChecker::visit(AST::CompoundStatement& statement)
{
    ContextScope blockScope(this);
    AST::Visitor::visit(statement);
}

void TypeChecker::visit(AST::ForStatement& statement)
{
    if (auto* initializer = statement.maybeInitializer())
        AST::Visitor::visit(*initializer);

    if (auto* test = statement.maybeTest()) {
        auto* testType = infer(*test);
        if (!unify(m_types.boolType(), testType))
            typeError(InferBottom::No, test->span(), "for-loop condition must be bool, got ", *testType);
    }

    if (auto* update = statement.maybeUpdate())
        AST::Visitor::visit(*update);

    visit(statement.body());
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
    const auto& accessImpl = [&](const Type* baseType) -> const Type* {
        if (isBottom(baseType))
            return m_types.bottomType();

        if (std::holds_alternative<Types::Struct>(*baseType)) {
            auto& structType = std::get<Types::Struct>(*baseType);
            auto it = structType.fields.find(access.fieldName().id());
            if (it == structType.fields.end()) {
                typeError(access.span(), "struct '", *baseType, "' does not have a member called '", access.fieldName(), "'");
                return nullptr;
            }
            return it->value;
        }

        if (std::holds_alternative<Types::Vector>(*baseType)) {
            auto& vector = std::get<Types::Vector>(*baseType);
            return vectorFieldAccess(vector, access);
        }

        typeError(access.span(), "invalid member access expression. Expected vector or struct, got '", *baseType, "'");
        return nullptr;
    };

    auto* baseType = infer(access.base());
    if (const auto* reference = std::get_if<Types::Reference>(baseType)) {
        if (const Type* result = accessImpl(reference->element)) {
            result = m_types.referenceType(reference->addressSpace, result, reference->accessMode);
            inferred(result);
        }
        return;
    }

    if (const Type* result = accessImpl(baseType))
        inferred(result);
}

void TypeChecker::visit(AST::IndexAccessExpression& access)
{
    const auto& accessImpl = [&](const Type* base) -> const Type* {
        if (isBottom(base))
            return m_types.bottomType();

        if (std::holds_alternative<Types::Array>(*base)) {
            // FIXME: check bounds if index is constant
            auto& array = std::get<Types::Array>(*base);
            return array.element;
        }

        if (std::holds_alternative<Types::Vector>(*base)) {
            // FIXME: check bounds if index is constant
            auto& vector = std::get<Types::Vector>(*base);
            return vector.element;
        }

        // FIXME: Implement matrix accesses
        typeError(access.span(), "cannot index type '", *base, "'");
        return nullptr;
    };

    auto* base = infer(access.base());
    auto* index = infer(access.index());

    if (!unify(m_types.i32Type(), index) && !unify(m_types.u32Type(), index) && !unify(m_types.abstractIntType(), index)) {
        typeError(access.span(), "index must be of type 'i32' or 'u32', found: '", *index, "'");
        return;
    }

    if (const auto* reference = std::get_if<Types::Reference>(base)) {
        if (const Type* result = accessImpl(reference->element)) {
            result = m_types.referenceType(reference->addressSpace, result, reference->accessMode);
            inferred(result);
        }
        return;
    }

    if (const Type* result = accessImpl(base))
        inferred(result);
}

void TypeChecker::visit(AST::BinaryExpression& binary)
{
    chooseOverload("operator", binary.span(), toString(binary.operation()), ReferenceWrapperVector<AST::Expression, 2> { binary.leftExpression(), binary.rightExpression() }, { });
}

void TypeChecker::visit(AST::IdentifierExpression& identifier)
{
    auto* binding = readVariable(identifier.identifier());
    if (!binding) {
        typeError(identifier.span(), "unresolved identifier '", identifier.identifier(), "'");
        return;
    }

    if (binding->kind != Binding::Value) {
        typeError(identifier.span(), "cannot use type '", identifier.identifier(), "' as value");
        return;
    }

    inferred(binding->type);
}

void TypeChecker::visit(AST::CallExpression& call)
{
    auto& target = call.target();
    bool isNamedType = is<AST::IdentifierExpression>(target);
    bool isParameterizedType = is<AST::ElaboratedTypeExpression>(target);
    if (isNamedType || isParameterizedType) {
        Vector<const Type*> typeArguments;
        String targetName = [&]() -> String {
            if (isNamedType)
                return downcast<AST::IdentifierExpression>(target).identifier();
            auto& elaborated = downcast<AST::ElaboratedTypeExpression>(target);
            for (auto& argument : elaborated.arguments())
                typeArguments.append(resolve(argument));
            return elaborated.base();
        }();

        auto* targetBinding = isNamedType ? readVariable(targetName) : nullptr;
        if (targetBinding) {
            target.m_inferredType = targetBinding->type;
            if (targetBinding->kind == Binding::Type) {
                if (auto* structType = std::get_if<Types::Struct>(targetBinding->type)) {
                    auto numberOfArguments = call.arguments().size();
                    auto numberOfFields = structType->fields.size();
                    if (numberOfArguments && numberOfArguments != numberOfFields) {
                        const char* errorKind = numberOfArguments < numberOfFields ? "few" : "many";
                        typeError(call.span(), "struct initializer has too ", errorKind, " inputs: expected ", String::number(numberOfFields), ", found ", String::number(numberOfArguments));
                        return;
                    }

                    for (unsigned i = 0; i < numberOfArguments; ++i) {
                        auto& argument = call.arguments()[i];
                        auto& member = structType->structure.members()[i];
                        auto* fieldType = structType->fields.get(member.name());
                        auto* argumentType = infer(argument);
                        if (!unify(fieldType, argumentType)) {
                            typeError(argument.span(), "type in struct initializer does not match struct member type: expected '", *fieldType, "', found '", *argumentType, "'");
                            return;
                        }
                        argument.m_inferredType = fieldType;
                    }
                    inferred(targetBinding->type);
                    return;
                }

                if (auto* vectorType = std::get_if<Types::Vector>(targetBinding->type)) {
                    typeArguments.append(vectorType->element);
                    switch (vectorType->size) {
                    case 2:
                        targetName = "vec2"_s;
                        break;
                    case 3:
                        targetName = "vec3"_s;
                        break;
                    case 4:
                        targetName = "vec4"_s;
                        break;
                    default:
                        RELEASE_ASSERT_NOT_REACHED();
                    }
                }
            }

            if (targetBinding->kind == Binding::Value) {
                if (auto* functionType = std::get_if<Types::Function>(targetBinding->type)) {
                    auto numberOfArguments = call.arguments().size();
                    auto numberOfParameters = functionType->parameters.size();
                    if (numberOfArguments != numberOfParameters) {
                        const char* errorKind = numberOfArguments < numberOfParameters ? "few" : "many";
                        typeError(call.span(), "funtion call has too ", errorKind, " arguments: expected ", String::number(numberOfParameters), ", found ", String::number(numberOfArguments));
                        return;
                    }

                    for (unsigned i = 0; i < numberOfArguments; ++i) {
                        auto& argument = call.arguments()[i];
                        auto* parameterType = functionType->parameters[i];
                        auto* argumentType = infer(argument);
                        if (!unify(parameterType, argumentType)) {
                            typeError(argument.span(), "type in function call does not match parameter type: expected '", *parameterType, "', found '", *argumentType, "'");
                            return;
                        }
                        argument.m_inferredType = parameterType;
                    }
                    inferred(functionType->result);
                    return;
                }
            }
        }

        auto* result = chooseOverload("initializer", call.span(), targetName, call.arguments(), typeArguments);
        if (result) {
            target.m_inferredType = result;
            return;
        }

        if (targetBinding)
            typeError(target.span(), "cannot call value of type '", *targetBinding->type, "'");
        else
            typeError(target.span(), "unresolved call target '", targetName, "'");
        return;
    }

    if (is<AST::ArrayTypeExpression>(target)) {
        AST::ArrayTypeExpression& array = downcast<AST::ArrayTypeExpression>(target);
        const Type* elementType = nullptr;
        unsigned elementCount;

        if (array.maybeElementType()) {
            if (!array.maybeElementCount()) {
                typeError(call.span(), "cannot construct a runtime-sized array");
                return;
            }
            elementType = resolve(*array.maybeElementType());
            elementCount = *extractInteger(*array.maybeElementCount());
            if (!elementCount) {
                typeError(call.span(), "array count must be greater than 0");
                return;
            }
            unsigned numberOfArguments = call.arguments().size();
            if (numberOfArguments && numberOfArguments != elementCount) {
                const char* errorKind = call.arguments().size() < elementCount ? "few" : "many";
                typeError(call.span(), "array constructor has too ", errorKind, " elements: expected ", String::number(elementCount), ", found ", String::number(call.arguments().size()));
                return;
            }
            for (auto& argument : call.arguments()) {
                auto* argumentType = infer(argument);
                if (!unify(elementType, argumentType)) {
                    typeError(argument.span(), "'", *argumentType, "' cannot be used to construct an array of '", *elementType, "'");
                    return;
                }
                argument.m_inferredType = elementType;
            }
        } else {
            ASSERT(!array.maybeElementCount());
            elementCount = call.arguments().size();
            if (!elementCount) {
                typeError(call.span(), "cannot infer array element type from constructor");
                return;
            }
            for (auto& argument : call.arguments()) {
                if (!elementType) {
                    elementType = infer(argument);
                    continue;
                }
                auto* argumentType = infer(argument);
                if (unify(elementType, argumentType))
                    continue;
                if (unify(argumentType, elementType)) {
                    elementType = argumentType;
                    continue;
                }
                typeError(argument.span(), "cannot infer common array element type from constructor arguments");
                return;
            }
            for (auto& argument : call.arguments())
                argument.m_inferredType = elementType;
        }
        auto* result = m_types.arrayType(elementType, { elementCount });
        inferred(result);
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void TypeChecker::visit(AST::UnaryExpression& unary)
{
    chooseOverload("operator", unary.span(), toString(unary.operation()), ReferenceWrapperVector<AST::Expression, 1> { unary.expression() }, { });
}

// Literal Expressions
void TypeChecker::visit(AST::BoolLiteral&)
{
    inferred(m_types.boolType());
}

void TypeChecker::visit(AST::Signed32Literal&)
{
    inferred(m_types.i32Type());
}

void TypeChecker::visit(AST::Float32Literal&)
{
    inferred(m_types.f32Type());
}

void TypeChecker::visit(AST::Unsigned32Literal&)
{
    inferred(m_types.u32Type());
}

void TypeChecker::visit(AST::AbstractIntegerLiteral&)
{
    inferred(m_types.abstractIntType());
}

void TypeChecker::visit(AST::AbstractFloatLiteral&)
{
    inferred(m_types.abstractFloatType());
}

// Types
void TypeChecker::visit(AST::ArrayTypeExpression& array)
{
    // FIXME: handle the case where there is no element type
    ASSERT(array.maybeElementType());

    auto* elementType = resolve(*array.maybeElementType());
    if (isBottom(elementType)) {
        inferred(m_types.bottomType());
        return;
    }

    std::optional<unsigned> size;
    if (array.maybeElementCount()) {
        size = extractInteger(*array.maybeElementCount());
        if (!size) {
            typeError(array.span(), "array count must evaluate to a constant integer expression or override variable");
            return;
        }
    }

    inferred(m_types.arrayType(elementType, size));
}

const Type* TypeChecker::lookupType(const AST::Identifier& name)
{
    auto* binding = readVariable(name);
    if (!binding) {
        typeError(InferBottom::No, name.span(), "unresolved type '", name, "'");
        return m_types.bottomType();
    }

    if (binding->kind != Binding::Type) {
        typeError(InferBottom::No, name.span(), "cannot use value '", name, "' as type");
        return m_types.bottomType();
    }

    return binding->type;
}

void TypeChecker::visit(AST::ElaboratedTypeExpression& type)
{
    auto* base = lookupType(type.base());
    if (isBottom(base)) {
        inferred(m_types.bottomType());
        return;
    }

    auto* constructor = std::get_if<Types::TypeConstructor>(base);
    if (!constructor) {
        typeError(type.span(), "type '", *base, "' does not take template arguments");
        return;
    }

    inferred(constructor->construct(type));
}

void TypeChecker::visit(AST::ReferenceTypeExpression&)
{
    // FIXME: we don't yet parse reference types
    ASSERT_NOT_REACHED();
}

void TypeChecker::visitAttributes(AST::Attribute::List& attributes)
{
    for (auto& attribute : attributes)
        AST::Visitor::visit(attribute);
}

// Private helpers
const Type* TypeChecker::vectorFieldAccess(const Types::Vector& vector, AST::FieldAccessExpression& access)
{
    const auto& fieldName = access.fieldName().id();
    auto length = fieldName.length();
    auto vectorSize = vector.size;

    bool isValid = true;
    const auto& isXYZW = [&](char c) {
        switch (c) {
        case 'x':
        case 'y':
            return true;
        case 'z':
            isValid &= vectorSize >= 3;
            return true;
        case 'w':
            isValid &= vectorSize == 4;
            return true;
        default:
            return false;
        }
    };
    const auto& isRGBA = [&](char c) {
        switch (c) {
        case 'r':
        case 'g':
            return true;
        case 'b':
            isValid &= vectorSize >= 3;
            return true;
        case 'a':
            isValid &= vectorSize == 4;
            return true;
        default:
            return false;
        }
    };

    bool hasXYZW = false;
    bool hasRGBA = false;
    for (unsigned i = 0; i < length; ++i) {
        char c = fieldName[i];
        if (isXYZW(c))
            hasXYZW = true;
        else if (isRGBA(c))
            hasRGBA = true;
        else {
            typeError(access.span(), "invalid vector swizzle character");
            return nullptr;
        }
    }

    if (!isValid || (hasRGBA && hasXYZW)) {
        typeError(access.span(), "invalid vector swizzle member");
        return nullptr;
    }

    switch (length) {
    case 1:
        return vector.element;
    case 2:
    case 3:
    case 4:
        break;
    default:
        typeError(access.span(), "invalid vector swizzle size");
        return nullptr;
    }

    return m_types.vectorType(vector.element, length);
}

template<typename CallArguments>
const Type* TypeChecker::chooseOverload(const char* kind, const SourceSpan& span, const String& target, CallArguments&& callArguments, const Vector<const Type*>& typeArguments)
{
    auto it = m_overloadedOperations.find(target);
    if (it == m_overloadedOperations.end())
        return nullptr;

    Vector<const Type*> valueArguments;
    valueArguments.reserveInitialCapacity(callArguments.size());
    for (unsigned i = 0; i < callArguments.size(); ++i) {
        auto* type = infer(callArguments[i]);
        if (isBottom(type)) {
            inferred(m_types.bottomType());
            return m_types.bottomType();
        }
        valueArguments.append(type);
    }

    auto overload = resolveOverloads(m_types, it->value, valueArguments, typeArguments);
    if (overload.has_value()) {
        ASSERT(overload->parameters.size() == callArguments.size());
        for (unsigned i = 0; i < callArguments.size(); ++i)
            callArguments[i].m_inferredType = overload->parameters[i];
        inferred(overload->result);
        return overload->result;
    }

    StringPrintStream valueArgumentsStream;
    bool first = true;
    for (auto* argument : valueArguments) {
        if (!first)
            valueArgumentsStream.print(", ");
        first = false;
        valueArgumentsStream.print(*argument);
    }
    StringPrintStream typeArgumentsStream;
    first = true;
    if (typeArguments.size()) {
        typeArgumentsStream.print("<");
        for (auto* typeArgument : typeArguments) {
            if (!first)
                typeArgumentsStream.print(", ");
            first = false;
            typeArgumentsStream.print(*typeArgument);
        }
        typeArgumentsStream.print(">");
    }
    typeError(span, "no matching overload for ", kind, " ", target, typeArgumentsStream.toString(), "(", valueArgumentsStream.toString(), ")");
    return m_types.bottomType();
}

const Type* TypeChecker::infer(AST::Expression& expression)
{
    ASSERT(!m_inferredType);
    AST::Visitor::visit(expression);
    ASSERT(m_inferredType);

    if (shouldDumpInferredTypes) {
        dataLog("> Type inference [expression]: ");
        dumpNode(WTF::dataFile(), expression);
        dataLog(" : ");
        dataLogLn(*m_inferredType);
    }

    expression.m_inferredType = m_inferredType;
    const Type* inferredType = m_inferredType;
    m_inferredType = nullptr;

    return inferredType;
}

const Type* TypeChecker::resolve(AST::Expression& type)
{
    ASSERT(!m_inferredType);
    if (is<AST::IdentifierExpression>(type))
        inferred(lookupType(downcast<AST::IdentifierExpression>(type).identifier()));
    else
        AST::Visitor::visit(type);
    ASSERT(m_inferredType);

    if (shouldDumpInferredTypes) {
        dataLog("> Type inference [type]: ");
        dumpNode(WTF::dataFile(), type);
        dataLog(" : ");
        dataLogLn(*m_inferredType);
    }

    type.m_inferredType = m_inferredType;
    const Type* inferredType = m_inferredType;
    m_inferredType = nullptr;

    return inferredType;
}

void TypeChecker::inferred(const Type* type)
{
    ASSERT(type);
    ASSERT(!m_inferredType);
    m_inferredType = type;
}

bool TypeChecker::unify(const Type* lhs, const Type* rhs)
{
    if (shouldDumpInferredTypes)
        dataLogLn("[unify] '", *lhs, "' <", RawPointer(lhs), ">  and '", *rhs, "' <", RawPointer(rhs), ">");

    if (lhs == rhs)
        return true;

    // Bottom is only inferred when a type error is reported, so we skip further
    // checks that are a consequence of an already reported error.
    if (isBottom(lhs) || isBottom(rhs))
        return true;

    return !!conversionRank(rhs, lhs);
}

bool TypeChecker::isBottom(const Type* type) const
{
    return type == m_types.bottomType();
}

void TypeChecker::introduceType(const AST::Identifier& name, const Type* type)
{
    if (!introduceVariable(name, { Binding::Type, type }))
        typeError(InferBottom::No, name.span(), "redeclaration of '", name, "'");
}

void TypeChecker::introduceValue(const AST::Identifier& name, const Type* type)
{
    if (!introduceVariable(name, { Binding::Value, type }))
        typeError(InferBottom::No, name.span(), "redeclaration of '", name, "'");
}

template<typename... Arguments>
void TypeChecker::typeError(const SourceSpan& span, Arguments&&... arguments)
{
    typeError(InferBottom::Yes, span, std::forward<Arguments>(arguments)...);
}

template<typename... Arguments>
void TypeChecker::typeError(InferBottom inferBottom, const SourceSpan& span, Arguments&&... arguments)
{
    m_errors.append({ makeString(std::forward<Arguments>(arguments)...), span });
    if (inferBottom == InferBottom::Yes)
        inferred(m_types.bottomType());
}

std::optional<FailedCheck> typeCheck(ShaderModule& shaderModule)
{
    return TypeChecker(shaderModule).check();
}

template<typename TargetConstructor, typename... Arguments>
void TypeChecker::allocateSimpleConstructor(ASCIILiteral name, TargetConstructor constructor, Arguments&&... arguments)
{
    introduceType(AST::Identifier::make(name), m_types.typeConstructorType(
        name,
        [this, constructor, arguments...](AST::ElaboratedTypeExpression& type) -> const Type* {
            if (type.arguments().size() != 1) {
                typeError(InferBottom::No, type.span(), "'", type.base(), "' requires 1 template argument");
                return m_types.bottomType();
            }
            auto* elementType = resolve(type.arguments().first());
            if (isBottom(elementType))
                return m_types.bottomType();

            return (m_types.*constructor)(elementType, arguments...);
        }
    ));
}

void TypeChecker::allocateTextureStorageConstructor(ASCIILiteral name, Types::TextureStorage::Kind kind)
{
    introduceType(AST::Identifier::make(name), m_types.typeConstructorType(
        name,
        [this, kind](AST::ElaboratedTypeExpression& type) -> const Type* {
            if (type.arguments().size() != 2) {
                typeError(InferBottom::No, type.span(), "'", type.base(), "' requires 2 template argument");
                return m_types.bottomType();
            }

            auto* formatType = infer(type.arguments()[0]);
            if (!unify(formatType, m_types.texelFormatType())) {
                typeError(InferBottom::No, type.span(), "cannot use '", *formatType, "' as texel format");
                return m_types.bottomType();
            }

            auto* accessType = infer(type.arguments()[1]);
            if (!unify(accessType, m_types.accessModeType())) {
                typeError(InferBottom::No, type.span(), "cannot use '", *accessType, "' as access mode");
                return m_types.bottomType();
            }

            ASSERT(is<AST::IdentifierExpression>(type.arguments()[0]));
            ASSERT(is<AST::IdentifierExpression>(type.arguments()[1]));
            auto& formatName = downcast<AST::IdentifierExpression>(type.arguments()[0]).identifier();
            auto& accessName = downcast<AST::IdentifierExpression>(type.arguments()[1]).identifier();

            static constexpr std::pair<ComparableASCIILiteral, TexelFormat> texelFormatMappings[] {
                { "bgra8unorm", TexelFormat::BGRA8unorm },
                { "r32float", TexelFormat::R32float },
                { "r32sint", TexelFormat::R32sint },
                { "r32uint", TexelFormat::R32uint },
                { "rg32float", TexelFormat::RG32float },
                { "rg32sint", TexelFormat::RG32sint },
                { "rg32uint", TexelFormat::RG32uint },
                { "rgba16float", TexelFormat::RGBA16float },
                { "rgba16sint", TexelFormat::RGBA16sint },
                { "rgba16uint", TexelFormat::RGBA16uint },
                { "rgba32float", TexelFormat::RGBA32float },
                { "rgba32sint", TexelFormat::RGBA32sint },
                { "rgba32uint", TexelFormat::RGBA32uint },
                { "rgba8sint", TexelFormat::RGBA8sint },
                { "rgba8snorm", TexelFormat::RGBA8snorm },
                { "rgba8uint", TexelFormat::RGBA8uint },
                { "rgba8unorm", TexelFormat::RGBA8unorm },
            };
            static constexpr SortedArrayMap texelFormats { texelFormatMappings };

            static constexpr std::pair<ComparableASCIILiteral, AccessMode> accessModeMappings[] {
                { "read", AccessMode::Read },
                { "read_write", AccessMode::ReadWrite },
                { "write", AccessMode::Write },
            };
            static constexpr SortedArrayMap accessModes { accessModeMappings };

            auto format = texelFormats.get(formatName.id());
            auto access = accessModes.get(accessName.id());
            return m_types.textureStorageType(kind, format, access);
        }
    ));
}

} // namespace WGSL
