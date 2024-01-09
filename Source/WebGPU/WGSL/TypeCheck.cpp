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
#include "ConstantFunctions.h"
#include "ContextProviderInlines.h"
#include "Overload.h"
#include "TypeStore.h"
#include "Types.h"
#include "WGSLShaderModule.h"
#include <wtf/DataLog.h>
#include <wtf/SortedArrayMap.h>

namespace WGSL {

static constexpr bool shouldDumpInferredTypes = false;
static constexpr bool shouldDumpConstantValues = false;

struct Binding {
    enum Kind : uint8_t {
        Value,
        Type,
    };

    Kind kind;
    const struct Type* type;
    std::optional<ConstantValue> constantValue;
};

class TypeChecker : public AST::Visitor, public ContextProvider<Binding> {
public:
    TypeChecker(ShaderModule&);

    std::optional<FailedCheck> check();

    // Declarations
    void visit(AST::Structure&) override;
    void visit(AST::Variable&) override;
    void visit(AST::Function&) override;
    void visit(AST::TypeAlias&) override;

    // Attributes
    void visit(AST::AlignAttribute&) override;
    void visit(AST::BindingAttribute&) override;
    void visit(AST::GroupAttribute&) override;
    void visit(AST::IdAttribute&) override;
    void visit(AST::LocationAttribute&) override;
    void visit(AST::SizeAttribute&) override;
    void visit(AST::WorkgroupSizeAttribute&) override;

    // Statements
    void visit(AST::VariableStatement&) override;
    void visit(AST::AssignmentStatement&) override;
    void visit(AST::CallStatement&) override;
    void visit(AST::CompoundAssignmentStatement&) override;
    void visit(AST::DecrementIncrementStatement&) override;
    void visit(AST::IfStatement&) override;
    void visit(AST::PhonyAssignmentStatement&) override;
    void visit(AST::ReturnStatement&) override;
    void visit(AST::CompoundStatement&) override;
    void visit(AST::ForStatement&) override;
    void visit(AST::LoopStatement&) override;
    void visit(AST::WhileStatement&) override;
    void visit(AST::SwitchStatement&) override;

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
    void visit(AST::Float16Literal&) override;
    void visit(AST::Unsigned32Literal&) override;
    void visit(AST::AbstractIntegerLiteral&) override;
    void visit(AST::AbstractFloatLiteral&) override;

    // Types
    void visit(AST::ArrayTypeExpression&) override;
    void visit(AST::ElaboratedTypeExpression&) override;
    void visit(AST::ReferenceTypeExpression&) override;

    void visit(AST::Continuing&) override;

private:
    enum class VariableKind : uint8_t {
        Local,
        Global,
    };

    void visitVariable(AST::Variable&, VariableKind);
    const Type* vectorFieldAccess(const Types::Vector&, AST::FieldAccessExpression&);
    void visitAttributes(AST::Attribute::List&);
    void bitcast(AST::CallExpression&, const Vector<const Type*>&);

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
    void introduceValue(const AST::Identifier&, const Type*, std::optional<ConstantValue> = std::nullopt);
    bool convertValue(const SourceSpan&, const Type*, ConstantValue&);

    template<typename TargetConstructor, typename... Arguments>
    void allocateSimpleConstructor(ASCIILiteral, TargetConstructor, Arguments&&...);
    void allocateTextureStorageConstructor(ASCIILiteral, Types::TextureStorage::Kind);

    std::optional<AccessMode> accessMode(AST::Expression&);
    std::optional<TexelFormat> texelFormat(AST::Expression&);
    std::optional<AddressSpace> addressSpace(AST::Expression&);

    template<typename CallArguments>
    const Type* chooseOverload(const char*, AST::Expression&, const String&, CallArguments&& valueArguments, const Vector<const Type*>& typeArguments);

    template<typename Node>
    void setConstantValue(Node&, const ConstantValue&);

    ShaderModule& m_shaderModule;
    const Type* m_inferredType { nullptr };
    const Type* m_returnType { nullptr };

    TypeStore& m_types;
    Vector<Error> m_errors;
    HashMap<String, OverloadedDeclaration> m_overloadedOperations;
};

TypeChecker::TypeChecker(ShaderModule& shaderModule)
    : m_shaderModule(shaderModule)
    , m_types(shaderModule.types())
{
    introduceType(AST::Identifier::make("bool"_s), m_types.boolType());
    introduceType(AST::Identifier::make("i32"_s), m_types.i32Type());
    introduceType(AST::Identifier::make("u32"_s), m_types.u32Type());
    introduceType(AST::Identifier::make("f32"_s), m_types.f32Type());
    introduceType(AST::Identifier::make("f16"_s), m_types.f16Type());
    introduceType(AST::Identifier::make("sampler"_s), m_types.samplerType());
    introduceType(AST::Identifier::make("sampler_comparison"_s), m_types.samplerComparisonType());
    introduceType(AST::Identifier::make("texture_external"_s), m_types.textureExternalType());

    introduceType(AST::Identifier::make("texture_depth_2d"_s), m_types.textureDepth2dType());
    introduceType(AST::Identifier::make("texture_depth_2d_array"_s), m_types.textureDepth2dArrayType());
    introduceType(AST::Identifier::make("texture_depth_cube"_s), m_types.textureDepthCubeType());
    introduceType(AST::Identifier::make("texture_depth_cube_array"_s), m_types.textureDepthCubeArrayType());
    introduceType(AST::Identifier::make("texture_depth_multisampled_2d"_s), m_types.textureDepthMultisampled2dType());

    introduceType(AST::Identifier::make("ptr"_s), m_types.typeConstructorType(
        "ptr"_s,
        [this](AST::ElaboratedTypeExpression& type) -> const Type* {
            auto argumentCount = type.arguments().size();
            if (argumentCount < 2) {
                typeError(InferBottom::No, type.span(), "'ptr' requires at least 2 template argument");
                return m_types.bottomType();
            }

            if (argumentCount > 3) {
                typeError(InferBottom::No, type.span(), "'ptr' requires at most 3 template argument");
                return m_types.bottomType();
            }

            auto maybeAddressSpace = addressSpace(type.arguments()[0]);
            if (!maybeAddressSpace)
                return m_types.bottomType();

            auto* elementType = resolve(type.arguments()[1]);
            if (isBottom(elementType))
                return m_types.bottomType();

            AccessMode accessMode;
            if (argumentCount > 2) {
                if (*maybeAddressSpace != AddressSpace::Storage) {
                    typeError(InferBottom::No, type.arguments()[2].span(), "only pointers in <storage> address space may specify an access mode");
                    return m_types.bottomType();
                }

                auto maybeAccessMode = this->accessMode(type.arguments()[2]);
                if (!maybeAccessMode)
                    return m_types.bottomType();
                accessMode = *maybeAccessMode;
            } else {
                switch (*maybeAddressSpace) {
                case AddressSpace::Function:
                case AddressSpace::Private:
                case AddressSpace::Workgroup:
                    accessMode = AccessMode::ReadWrite;
                    break;
                case AddressSpace::Uniform:
                case AddressSpace::Storage:
                case AddressSpace::Handle:
                    accessMode = AccessMode::Read;
                    break;
                }
            }

            return m_types.pointerType(*maybeAddressSpace, elementType, accessMode);
        }
    ));

    introduceType(AST::Identifier::make("atomic"_s), m_types.typeConstructorType(
        "atomic"_s,
        [this](AST::ElaboratedTypeExpression& type) -> const Type* {
            if (type.arguments().size() != 1) {
                typeError(InferBottom::No, type.span(), "'atomic' requires 1 template arguments");
                return m_types.bottomType();
            }

            auto* elementType = resolve(type.arguments()[0]);
            if (isBottom(elementType))
                return m_types.bottomType();

            if (elementType != m_types.i32Type() && elementType != m_types.u32Type()) {
                typeError(InferBottom::No, type.arguments()[0].span(), "atomic only supports i32 or u32 types");
                return m_types.bottomType();
            }

            return m_types.atomicType(elementType);
        }
    ));

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

    if (m_shaderModule.hasFeature("bgra8unorm-storage"_s))
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

    introduceValue(AST::Identifier::make("function"_s), m_types.addressSpaceType());
    introduceValue(AST::Identifier::make("private"_s), m_types.addressSpaceType());
    introduceValue(AST::Identifier::make("workgroup"_s), m_types.addressSpaceType());
    introduceValue(AST::Identifier::make("uniform"_s), m_types.addressSpaceType());
    introduceValue(AST::Identifier::make("storage"_s), m_types.addressSpaceType());

    // This file contains the declarations generated from `TypeDeclarations.rb`
#include "TypeDeclarations.h" // NOLINT
}

std::optional<FailedCheck> TypeChecker::check()
{
    AST::Visitor::visit(m_shaderModule);

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
    visitVariable(variable, VariableKind::Global);
}

void TypeChecker::visit(AST::TypeAlias& alias)
{
    auto* type = resolve(alias.type());
    introduceType(alias.name(), type);
}

void TypeChecker::visit(AST::VariableStatement& statement)
{
    visitVariable(statement.variable(), VariableKind::Local);
}

void TypeChecker::visitVariable(AST::Variable& variable, VariableKind variableKind)
{
    visitAttributes(variable.attributes());

    const Type* result = nullptr;
    ConstantValue* value = nullptr;
    if (variable.maybeTypeName())
        result = resolve(*variable.maybeTypeName());
    if (variable.maybeInitializer()) {
        auto* initializerType = infer(*variable.maybeInitializer());
        auto& constantValue = variable.maybeInitializer()->m_constantValue;
        if (constantValue.has_value())
            value = &*constantValue;

        if (auto* reference = std::get_if<Types::Reference>(initializerType)) {
            initializerType = reference->element;
            variable.maybeInitializer()->m_inferredType = initializerType;
        }

        if (!result) {
            if (variable.flavor() == AST::VariableFlavor::Const)
                result = initializerType;
            else {
                result = concretize(initializerType, m_types);
                variable.maybeInitializer()->m_inferredType = result;
            }
        } else if (unify(result, initializerType))
            variable.maybeInitializer()->m_inferredType = result;
        else {
            typeError(InferBottom::No, variable.span(), "cannot initialize var of type '", *result, "' with value of type '", *initializerType, "'");
            result = m_types.bottomType();
        }
    }

    if (value && !isBottom(result))
        convertValue(variable.span(), result, *value);

    if (variable.flavor() != AST::VariableFlavor::Const || result == m_types.bottomType())
        value = nullptr;

    if (variable.flavor() == AST::VariableFlavor::Var) {
        AddressSpace addressSpace;
        AccessMode accessMode;
        if (auto* maybeQualifier = variable.maybeQualifier()) {
            addressSpace = maybeQualifier->addressSpace();
            accessMode = maybeQualifier->accessMode();
        } else if (variableKind == VariableKind::Local) {
            addressSpace = AddressSpace::Function;
            accessMode = AccessMode::ReadWrite;
        } else {
            addressSpace = AddressSpace::Handle;
            accessMode = AccessMode::Read;
        }
        variable.m_addressSpace = addressSpace;
        variable.m_accessMode = accessMode;
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

    introduceValue(variable.name(), result, value ? std::optional<ConstantValue>(*value) : std::nullopt);
}

void TypeChecker::visit(AST::Function& function)
{
    visitAttributes(function.attributes());

    Vector<const Type*> parameters;
    parameters.reserveInitialCapacity(function.parameters().size());
    for (auto& parameter : function.parameters()) {
        visitAttributes(parameter.attributes());
        parameters.append(resolve(parameter.typeName()));
    }

    visitAttributes(function.returnAttributes());
    if (function.maybeReturnType())
        m_returnType = resolve(*function.maybeReturnType());
    else
        m_returnType = m_types.voidType();

    const Type* functionType = m_types.functionType(WTFMove(parameters), m_returnType);
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
    if (!unify(reference->element, rhs)) {
        typeError(InferBottom::No, statement.span(), "cannot assign value of type '", *rhs, "' to '", *reference->element, "'");
        return;
    }

    statement.rhs().m_inferredType = reference->element;
    auto& value = statement.rhs().m_constantValue;
    if (value.has_value())
        convertValue(statement.rhs().span(), statement.rhs().inferredType(), *value);
}

void TypeChecker::visit(AST::CallStatement& statement)
{
    auto* result = infer(statement.call());
    // FIXME: this should check if the function has a must_use attribute
    UNUSED_PARAM(result);
}

void TypeChecker::visit(AST::CompoundAssignmentStatement& statement)
{
    // FIXME: Implement type checking - infer is called to avoid ASSERT in
    // TypeChecker::visit(AST::Expression&)
    infer(statement.leftExpression());
    infer(statement.rightExpression());

    const char* operationName = nullptr;
    if (statement.operation() == AST::BinaryOperation::Divide)
        operationName = "division";
    else if (statement.operation() == AST::BinaryOperation::Modulo)
        operationName = "modulo";
    if (operationName) {
        auto* rightType = statement.rightExpression().inferredType();
        if (auto* vectorType = std::get_if<Types::Vector>(rightType))
            rightType = vectorType->element;
        if (satisfies(rightType, Constraints::Integer)) {
            if (statement.operation() == AST::BinaryOperation::Divide)
                m_shaderModule.setUsesDivision();
            else
                m_shaderModule.setUsesModulo();
            auto rightValue = statement.rightExpression().constantValue();
            if (rightValue && containsZero(*rightValue, statement.rightExpression().inferredType()))
                typeError(InferBottom::No, statement.span(), "invalid ", operationName, " by zero");
        }
    }
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

    visit(statement.trueBody());
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
    const Type* type;
    auto* expression = statement.maybeExpression();
    if (expression)
        type = infer(*expression);
    else
        type = m_types.voidType();

    if (!unify(m_returnType, type))
        typeError(InferBottom::No, statement.span(), "return statement type does not match its function return type, returned '", *type, "', expected '", *m_returnType, "'");
    else if (expression) {
        expression->m_inferredType = m_returnType;
        if (auto& value = expression->m_constantValue)
            convertValue(expression->span(), m_returnType, *value);
    }
}

void TypeChecker::visit(AST::CompoundStatement& statement)
{
    ContextScope blockScope(this);
    AST::Visitor::visit(statement);
}

void TypeChecker::visit(AST::ForStatement& statement)
{
    ContextScope forScope(this);
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

void TypeChecker::visit(AST::LoopStatement& statement)
{
    ContextScope loopScope(this);
    visitAttributes(statement.attributes());

    for (auto& statement : statement.body())
        AST::Visitor::visit(statement);

    if (auto continuing = statement.continuing())
        visit(*continuing);
}

void TypeChecker::visit(AST::WhileStatement& statement)
{
    auto* testType = infer(statement.test());
    if (!unify(m_types.boolType(), testType))
        typeError(InferBottom::No, statement.test().span(), "while condition must be bool, got ", *testType);

    visit(statement.body());
}

void TypeChecker::visit(AST::SwitchStatement& statement)
{
    auto* valueType = infer(statement.value());
    if (!satisfies(valueType, Constraints::ConcreteInteger)) {
        typeError(InferBottom::No, statement.value().span(), "switch selector must be of type i32 or u32");
        valueType = m_types.bottomType();
    }

    const auto& visitClause = [&](AST::SwitchClause& clause) {
        for (auto& selector : clause.selectors) {
            auto* selectorType = infer(selector);
            if (unify(valueType, selectorType)) {
                // If the selectorType can satisfy the value type, we're good to go.
                // e.g. valueType is i32 or u32 and the selector is a literal of type AbstractInt
                continue;
            }
            if (unify(selectorType, valueType)) {
                // If the opposite is true, we have to promote valueType
                // e.g. valueType is a constant of type AbstractInt and the selector has type i32 or u32
                valueType = selectorType;
                continue;
            }
            // Otherwise, the types are incompatible, and we have an error
            // e.g. valueType has type u32 the selector has type i32
            typeError(InferBottom::No, selector.span(), "the case selector values must have the same type as the selector expression: the selector expression has type '", *valueType, "' and case selector has type '", *selectorType, "'");
        }
        visit(clause.body);
    };

    visitAttributes(statement.valueAttributes());
    visitClause(statement.defaultClause());
    for (auto& clause : statement.clauses())
        visitClause(clause);
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
    const auto& accessImpl = [&](const Type* baseType, bool* canBeReference = nullptr) -> const Type* {
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

        if (auto* primitiveStruct = std::get_if<Types::PrimitiveStruct>(baseType)) {
            const auto& keys = Types::PrimitiveStruct::keys[primitiveStruct->kind];
            auto* key = keys.tryGet(access.fieldName().id());
            if (!key) {
                typeError(access.span(), "struct '", *baseType, "' does not have a member called '", access.fieldName(), "'");
                return nullptr;
            }
            if (auto constant = access.base().constantValue()) {
                auto& constantStruct = std::get<ConstantStruct>(*constant);
                access.setConstantValue(constantStruct.fields.get(access.fieldName().id()));
            }
            return primitiveStruct->values[*key];
        }

        if (std::holds_alternative<Types::Vector>(*baseType)) {
            auto& vector = std::get<Types::Vector>(*baseType);
            auto* result = vectorFieldAccess(vector, access);
            if (canBeReference)
                *canBeReference = !std::holds_alternative<Types::Vector>(*result);
            return result;
        }

        typeError(access.span(), "invalid member access expression. Expected vector or struct, got '", *baseType, "'");
        return nullptr;
    };

    auto* baseType = infer(access.base());
    if (const auto* reference = std::get_if<Types::Reference>(baseType)) {
        bool canBeReference = true;
        if (const Type* result = accessImpl(reference->element, &canBeReference)) {
            if (canBeReference)
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
    const auto& constantAccess = [&]<typename T>() {
        auto constantBase = access.base().constantValue();
        auto constantIndex = access.index().constantValue();
        bool isConstant = constantBase && constantIndex;

        if (!isConstant)
            return;

        auto constant = std::get<T>(*constantBase);
        auto index = constantIndex->integerValue();
        auto size = constant.upperBound();
        if (index < 0 || static_cast<size_t>(index) >= size)
            typeError(InferBottom::No, access.span(), "index ", String::number(index), " is out of bounds [0..", String::number(size - 1), "]");
        else
            access.setConstantValue(constant[index]);
    };

    const auto& accessImpl = [&](const Type* base) -> const Type* {
        if (isBottom(base))
            return m_types.bottomType();


        const Type* result = nullptr;
        if (auto* array = std::get_if<Types::Array>(base)) {
            result = array->element;
            constantAccess.operator()<ConstantArray>();
        } else if (auto* vector = std::get_if<Types::Vector>(base)) {
            result = vector->element;
            constantAccess.operator()<ConstantVector>();
        } else if (auto* matrix = std::get_if<Types::Matrix>(base)) {
            result = m_types.vectorType(matrix->rows, matrix->element);
            constantAccess.operator()<ConstantMatrix>();
        }

        if (!result) {
            typeError(access.span(), "cannot index type '", *base, "'");
            return nullptr;
        }

        if (!access.index().constantValue().has_value())
            result = concretize(result, m_types);
        return result;
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
    chooseOverload("operator", binary, toString(binary.operation()), ReferenceWrapperVector<AST::Expression, 2> { binary.leftExpression(), binary.rightExpression() }, { });

    const char* operationName = nullptr;
    if (binary.operation() == AST::BinaryOperation::Divide)
        operationName = "division";
    else if (binary.operation() == AST::BinaryOperation::Modulo)
        operationName = "modulo";
    if (operationName) {
        auto* rightType = binary.rightExpression().inferredType();
        if (auto* vectorType = std::get_if<Types::Vector>(rightType))
            rightType = vectorType->element;
        if (satisfies(rightType, Constraints::Integer)) {
            if (binary.operation() == AST::BinaryOperation::Divide)
                m_shaderModule.setUsesDivision();
            else
                m_shaderModule.setUsesModulo();
            auto leftValue = binary.leftExpression().constantValue();
            auto rightValue = binary.rightExpression().constantValue();
            if (!leftValue && rightValue && containsZero(*rightValue, binary.rightExpression().inferredType()))
                typeError(InferBottom::No, binary.span(), "invalid ", operationName, " by zero");
        }
    }
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
    if (binding->constantValue.has_value())
        setConstantValue(identifier, *binding->constantValue);
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
                        auto& value = argument.m_constantValue;
                        if (value.has_value())
                            convertValue(argument.span(), argument.inferredType(), *value);
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

                if (auto* matrixType = std::get_if<Types::Matrix>(targetBinding->type)) {
                    typeArguments.append(matrixType->element);
                    targetName = makeString("mat", String::number(matrixType->columns), "x", String::number(matrixType->rows));
                }

                if (std::holds_alternative<Types::Primitive>(*targetBinding->type))
                    targetName = targetBinding->type->toString();
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
                        auto& value = argument.m_constantValue;
                        if (value.has_value())
                            convertValue(argument.span(), argument.inferredType(), *value);
                    }
                    inferred(functionType->result);
                    return;
                }
            }
        }

        auto* result = chooseOverload("initializer", call, targetName, call.arguments(), typeArguments);
        if (result) {
            // FIXME: this will go away once we track used intrinsics properly
            if (targetName == "workgroupUniformLoad"_s)
                m_shaderModule.setUsesWorkgroupUniformLoad();
            else if (targetName == "frexp"_s)
                m_shaderModule.setUsesFrexp();
            else if (targetName == "modf"_s)
                m_shaderModule.setUsesModf();
            else if (targetName == "atomicCompareExchangeWeak"_s)
                m_shaderModule.setUsesAtomicCompareExchange();
            else if (targetName == "dot"_s)
                m_shaderModule.setUsesDot();
            else if (targetName == "firstLeadingBit"_s)
                m_shaderModule.setUsesFirstLeadingBit();
            else if (targetName == "firstTrailingBit"_s)
                m_shaderModule.setUsesFirstTrailingBit();
            else if (targetName == "sign"_s)
                m_shaderModule.setUsesSign();
            else if (targetName == "dot4I8Packed"_s)
                m_shaderModule.setUsesDot4I8Packed();
            else if (targetName == "dot4U8Packed"_s)
                m_shaderModule.setUsesDot4U8Packed();
            target.m_inferredType = result;
            return;
        }

        // FIXME: similarly to above: this shouldn't be a string check
        if (targetName == "bitcast"_s) {
            bitcast(call, typeArguments);
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

            auto elementCountType = infer(*array.maybeElementCount());
            if (!unify(m_types.i32Type(), elementCountType) && !unify(m_types.u32Type(), elementCountType)) {
                typeError(array.span(), "array count must be an i32 or u32 value, found '", *elementCountType, "'");
                return;
            }

            elementCount = array.maybeElementCount()->constantValue()->integerValue();
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

        unsigned argumentCount = call.arguments().size();
        FixedVector<ConstantValue> arguments(argumentCount);
        bool isConstant = true;
        for (unsigned i = 0; i < argumentCount; ++i) {
            auto& argument = call.arguments()[i];
            auto& value = argument.m_constantValue;
            if (!value.has_value() || !convertValue(argument.span(), argument.inferredType(), *value))
                isConstant = false;
            else
                arguments[i] = *value;
        }
        if (isConstant)
            setConstantValue(call, ConstantArray(WTFMove(arguments)));

        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void TypeChecker::bitcast(AST::CallExpression& call, const Vector<const Type*>& typeArguments)
{
    if (call.arguments().size() != 1) {
        typeError(call.span(), "bitcast expects a single argument, found ", String::number(call.arguments().size()));
        return;
    }

    if (typeArguments.size() != 1) {
        typeError(call.span(), "bitcast expects a single template argument, found ", String::number(typeArguments.size()));
        return;
    }

    auto& argument = call.arguments()[0];
    auto* sourceType = infer(argument);
    auto* destinationType = typeArguments[0];
    if (auto* reference = std::get_if<Types::Reference>(sourceType))
        sourceType = reference->element;
    sourceType = concretize(sourceType, m_types);

    const auto& primitivePrimitive = [&](const Type* p1, const Type* p2) {
        return (satisfies(p1, Constraints::Concrete32BitNumber) && satisfies(p2, Constraints::Concrete32BitNumber))
            || (p1 == m_types.f16Type() && p2 == m_types.f16Type());
    };

    const auto& vectorVector16To32Bit = [&](const Types::Vector& v1, const Types::Vector& v2) {
        return v1.size == 2 && satisfies(v1.element, Constraints::Concrete32BitNumber) && v2.size == 4 && v2.element == m_types.f16Type();
    };

    const auto& vectorVector = [&](const Types::Vector& v1, const Types::Vector& v2) {
        return (v1.size == v2.size && primitivePrimitive(v1.element, v2.element))
        || vectorVector16To32Bit(v1, v2)
        || vectorVector16To32Bit(v2, v1);
    };

    const auto& vectorPrimitive = [&](const Types::Vector& v, const Type* p) {
        return v.size == 2 && v.element == m_types.f16Type() && satisfies(p, Constraints::Concrete32BitNumber);
    };

    bool allowed = false;
    if (auto* dstVector = std::get_if<Types::Vector>(destinationType)) {
        if (auto* srcVector = std::get_if<Types::Vector>(sourceType))
            allowed = vectorVector(*srcVector, *dstVector);
        else
            allowed = vectorPrimitive(*dstVector, sourceType);
    } else if (auto* srcVector = std::get_if<Types::Vector>(sourceType))
        allowed = vectorPrimitive(*srcVector, destinationType);
    else
        allowed = primitivePrimitive(sourceType, destinationType);

    if (allowed) {
        call.target().m_inferredType = destinationType;
        if (argument.m_constantValue.has_value()) {
            auto result = constantBitcast(destinationType, { *argument.m_constantValue });
            if (!result)
                typeError(InferBottom::No, call.span(), result.error());
            else if (convertValue(call.span(), destinationType, *result))
                setConstantValue(call, WTFMove(*result));
        }
        inferred(destinationType);
        return;
    }

    typeError(call.span(), "cannot bitcast from '", *sourceType, "' to '", *destinationType, "'");
}

void TypeChecker::visit(AST::UnaryExpression& unary)
{
    chooseOverload("operator", unary, toString(unary.operation()), ReferenceWrapperVector<AST::Expression, 1> { unary.expression() }, { });
}

// Literal Expressions
void TypeChecker::visit(AST::BoolLiteral& literal)
{
    inferred(m_types.boolType());
    literal.setConstantValue(literal.value());
}

void TypeChecker::visit(AST::Signed32Literal& literal)
{
    inferred(m_types.i32Type());
    literal.setConstantValue(literal.value());
}

void TypeChecker::visit(AST::Float32Literal& literal)
{
    inferred(m_types.f32Type());
    literal.setConstantValue(literal.value());
}

void TypeChecker::visit(AST::Float16Literal& literal)
{
    inferred(m_types.f16Type());
    literal.setConstantValue(literal.value());
}

void TypeChecker::visit(AST::Unsigned32Literal& literal)
{
    inferred(m_types.u32Type());
    literal.setConstantValue(literal.value());
}

void TypeChecker::visit(AST::AbstractIntegerLiteral& literal)
{
    inferred(m_types.abstractIntType());
    literal.setConstantValue(literal.value());
}

void TypeChecker::visit(AST::AbstractFloatLiteral& literal)
{
    inferred(m_types.abstractFloatType());
    literal.setConstantValue(literal.value());
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
        auto elementCountType = infer(*array.maybeElementCount());
        if (!unify(m_types.i32Type(), elementCountType) && !unify(m_types.u32Type(), elementCountType)) {
            typeError(array.span(), "array count must be an i32 or u32 value, found '", *elementCountType, "'");
            return;
        }

        auto value = array.maybeElementCount()->constantValue();
        if (!value.has_value()) {
            typeError(array.span(), "array count must evaluate to a constant integer expression or override variable");
            return;
        }

        size = value->integerValue();
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
    RELEASE_ASSERT_NOT_REACHED();
}


void TypeChecker::visit(AST::Continuing& continuing)
{
    ContextScope continuingScope(this);

    visitAttributes(continuing.attributes);

    for (auto& statement : continuing.body)
        AST::Visitor::visit(statement);

    if (auto* breakIf = continuing.breakIf) {
        auto* type = infer(*breakIf);
        if (!unify(m_types.boolType(), type))
            typeError(breakIf->span(), "expected 'bool', found ", *type);
    }
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

    return m_types.vectorType(length, vector.element);
}

template<typename CallArguments>
const Type* TypeChecker::chooseOverload(const char* kind, AST::Expression& expression, const String& target, CallArguments&& callArguments, const Vector<const Type*>& typeArguments)
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

    auto overload = resolveOverloads(m_types, it->value.overloads, valueArguments, typeArguments);
    if (overload.has_value()) {
        ASSERT(overload->parameters.size() == callArguments.size());
        for (unsigned i = 0; i < callArguments.size(); ++i)
            callArguments[i].m_inferredType = overload->parameters[i];
        inferred(overload->result);

        if (it->value.kind == OverloadedDeclaration::Constructor && is<AST::CallExpression>(expression)) {
            auto& call = downcast<AST::CallExpression>(expression);
            call.m_isConstructor = true;
        }

        unsigned argumentCount = callArguments.size();
        FixedVector<ConstantValue> arguments(argumentCount);
        bool isConstant = true;
        for (unsigned i = 0; i < argumentCount; ++i) {
            auto& argument = callArguments[i];
            auto& value = argument.m_constantValue;
            if (!value.has_value() || !convertValue(argument.span(), argument.inferredType(), *value))
                isConstant = false;
            else
                arguments[i] = *value;
        }

        if (auto constantFunction = it->value.constantFunction; isConstant && constantFunction) {
            auto result = constantFunction(overload->result, WTFMove(arguments));
            if (!result)
                typeError(InferBottom::No, expression.span(), result.error());
            else if (convertValue(expression.span(), overload->result, *result))
                setConstantValue(expression, WTFMove(*result));
        }

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
    typeError(expression.span(), "no matching overload for ", kind, " ", target, typeArgumentsStream.toString(), "(", valueArgumentsStream.toString(), ")");
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
    if (!introduceVariable(name, { Binding::Type, type, std::nullopt }))
        typeError(InferBottom::No, name.span(), "redeclaration of '", name, "'");
}

bool TypeChecker::convertValue(const SourceSpan& span, const Type* type, ConstantValue& value)
{
    if (shouldDumpConstantValues) {
        StringPrintStream valueString;
        value.dump(valueString);
        dataLogLn("converting value ", valueString.toString(), " to '", *type, "'");
    }

    enum Conversion { Failed, FailedInner, Success };
    auto converted = WTF::switchOn(*type,
        [&](const Types::Primitive& primitive) -> Conversion {
            switch (primitive.kind) {
            case Types::Primitive::F32: {
                std::optional<float> result;
                if (auto* f32 = std::get_if<float>(&value))
                    result = convertFloat<float>(*f32);
                else if (auto* abstractFloat = std::get_if<double>(&value))
                    result = convertFloat<float>(*abstractFloat);
                else if (auto* abstractInt = std::get_if<int64_t>(&value))
                    result = convertFloat<float>(static_cast<double>(*abstractInt));

                if (!result.has_value())
                    return Failed;
                value = { *result };
                return Success;
            }
            case Types::Primitive::F16: {
                std::optional<half> result;
                if (auto* f16 = std::get_if<half>(&value))
                    result = convertFloat<half>(*f16);
                else if (auto* abstractFloat = std::get_if<double>(&value))
                    result = convertFloat<half>(*abstractFloat);
                else if (auto* abstractInt = std::get_if<int64_t>(&value))
                    result = convertFloat<half>(static_cast<double>(*abstractInt));

                if (!result.has_value())
                    return Failed;
                value = { *result };
                return Success;
            }
            case Types::Primitive::I32: {
                if (std::holds_alternative<int32_t>(value))
                    return Success;
                std::optional<int32_t> result;
                if (auto* abstractInt = std::get_if<int64_t>(&value))
                    result = convertInteger<int32_t>(*abstractInt);

                if (!result.has_value())
                    return Failed;
                value = { *result };
                return Success;
            }
            case Types::Primitive::U32: {
                if (std::holds_alternative<uint32_t>(value))
                    return Success;
                std::optional<uint32_t> result;
                if (auto* abstractInt = std::get_if<int64_t>(&value))
                    result = convertInteger<uint32_t>(*abstractInt);

                if (!result.has_value())
                    return Failed;
                value = { *result };
                return Success;
            }
            case Types::Primitive::AbstractInt:
                ASSERT(std::holds_alternative<int64_t>(value));
                return Success;
            case Types::Primitive::AbstractFloat: {
                std::optional<double> result;
                if (auto* abstractFloat = std::get_if<double>(&value))
                    result = convertFloat<double>(*abstractFloat);
                else if (auto* abstractInt = std::get_if<int64_t>(&value))
                    result = convertFloat<double>(static_cast<double>(*abstractInt));
                else
                    RELEASE_ASSERT_NOT_REACHED();
                if (!result.has_value())
                    return Failed;
                value = { *result };
                return Success;
            }
            case Types::Primitive::Bool:
                RELEASE_ASSERT(std::holds_alternative<bool>(value));
                return Success;
            case Types::Primitive::Void:
            case Types::Primitive::Sampler:
            case Types::Primitive::SamplerComparison:
            case Types::Primitive::TextureExternal:
            case Types::Primitive::AccessMode:
            case Types::Primitive::TexelFormat:
            case Types::Primitive::AddressSpace:
                return Failed;
            }
        },
        [&](const Types::Vector& vectorType) -> Conversion {
            ASSERT(value.isVector());
            auto& vector = std::get<ConstantVector>(value);
            for (auto& element : vector.elements) {
                if (!convertValue(span, vectorType.element, element))
                    return FailedInner;
            }
            return Success;
        },
        [&](const Types::Matrix& matrixType) -> Conversion {
            ASSERT(value.isMatrix());
            auto& matrix = std::get<ConstantMatrix>(value);
            for (auto& element : matrix.elements) {
                if (!convertValue(span, matrixType.element, element))
                    return FailedInner;
            }
            return Success;
        },
        [&](const Types::Array& arrayType) -> Conversion {
            ASSERT(value.isArray());
            auto& array = std::get<ConstantArray>(value);
            for (auto& element : array.elements) {
                if (!convertValue(span, arrayType.element, element))
                    return FailedInner;
            }
            return Success;
        },
        [&](const Types::Struct&) -> Conversion {
            // FIXME: this should be supported
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::PrimitiveStruct& primitiveStruct) -> Conversion {
            auto& constantStruct = std::get<ConstantStruct>(value);
            const auto& keys = Types::PrimitiveStruct::keys[primitiveStruct.kind];
            for (auto& entry : constantStruct.fields) {
                auto* key = keys.tryGet(entry.key);
                RELEASE_ASSERT(key);
                auto* type = primitiveStruct.values[*key];
                if (!convertValue(span, type, entry.value))
                    return FailedInner;
            }
            return Success;
        },
        [&](const Types::Function&) -> Conversion {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Texture&) -> Conversion {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::TextureStorage&) -> Conversion {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::TextureDepth&) -> Conversion {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Reference&) -> Conversion {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Pointer&) -> Conversion {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Atomic&) -> Conversion {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::TypeConstructor&) -> Conversion {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Bottom&) -> Conversion {
            RELEASE_ASSERT_NOT_REACHED();
        });

    if (UNLIKELY(converted == Failed)) {
        StringPrintStream valueString;
        value.dump(valueString);
        typeError(InferBottom::No, span, "value ", valueString.toString(), " cannot be represented as '", *type, "'");
    }

    return converted == Success;
}

void TypeChecker::introduceValue(const AST::Identifier& name, const Type* type, std::optional<ConstantValue> value)
{
    if (shouldDumpConstantValues && value.has_value())
        dataLogLn("> Assigning value: ", name, " => ", value);
    if (!introduceVariable(name, { Binding::Value, type , value }))
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

            return (m_types.*constructor)(arguments..., elementType);
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

            auto maybeFormat = texelFormat(type.arguments()[0]);
            if (!maybeFormat)
                return m_types.bottomType();

            auto maybeAccess = accessMode(type.arguments()[1]);
            if (!maybeAccess)
                return m_types.bottomType();
            return m_types.textureStorageType(kind, *maybeFormat, *maybeAccess);
        }
    ));
}

std::optional<TexelFormat> TypeChecker::texelFormat(AST::Expression& expression)
{
    auto* formatType = infer(expression);
    if (!unify(formatType, m_types.texelFormatType())) {
        typeError(InferBottom::No, expression.span(), "cannot use '", *formatType, "' as texel format");
        return std::nullopt;
    }

    ASSERT(is<AST::IdentifierExpression>(expression));
    auto& formatName = downcast<AST::IdentifierExpression>(expression).identifier();

    auto* format = parseTexelFormat(formatName.id());
    ASSERT(format);
    return { *format };
}

std::optional<AccessMode> TypeChecker::accessMode(AST::Expression& expression)
{
    auto* accessType = infer(expression);
    if (!unify(accessType, m_types.accessModeType())) {
        typeError(InferBottom::No, expression.span(), "cannot use '", *accessType, "' as access mode");
        return std::nullopt;
    }

    ASSERT(is<AST::IdentifierExpression>(expression));
    auto& accessName = downcast<AST::IdentifierExpression>(expression).identifier();

    auto* accessMode = parseAccessMode(accessName.id());
    ASSERT(accessMode);
    return { *accessMode };
}

std::optional<AddressSpace> TypeChecker::addressSpace(AST::Expression& expression)
{
    auto* addressSpaceType = infer(expression);
    if (!unify(addressSpaceType, m_types.addressSpaceType())) {
        typeError(InferBottom::No, expression.span(), "cannot use '", *addressSpaceType, "' as address space");
        return std::nullopt;
    }

    ASSERT(is<AST::IdentifierExpression>(expression));
    auto& addressSpaceName = downcast<AST::IdentifierExpression>(expression).identifier();

    auto* addressSpace = parseAddressSpace(addressSpaceName.id());
    ASSERT(addressSpace);
    return { *addressSpace };
}

template<typename Node>
void TypeChecker::setConstantValue(Node& expression, const ConstantValue& value)
{
    using namespace Types;

    if (shouldDumpConstantValues) {
        dataLog("> Setting constantValue for expression: ");
        dumpNode(WTF::dataFile(), expression);
        dataLogLn(" = ", value);
    }
    expression.setConstantValue(value);
}


} // namespace WGSL
