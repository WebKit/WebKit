/*
 * Copyright (c) 2023-2024 Apple Inc. All rights reserved.
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
#include "ASTScopedVisitorInlines.h"
#include "ASTStringDumper.h"
#include "CompilationMessage.h"
#include "ConstantFunctions.h"
#include "ContextProviderInlines.h"
#include "Overload.h"
#include "TypeStore.h"
#include "Types.h"
#include "WGSLShaderModule.h"
#include <wtf/DataLog.h>
#include <wtf/OptionSet.h>
#include <wtf/SetForScope.h>
#include <wtf/SortedArrayMap.h>
#include <wtf/text/MakeString.h>

namespace WGSL {

static constexpr bool shouldDumpInferredTypes = false;
static constexpr bool shouldDumpConstantValues = false;

enum class Evaluation : uint8_t {
    Constant = 1,
    Override = 2,
    Runtime = 3,
};

enum class DiscardResult : bool {
    No,
    Yes,
};

struct Binding {
    enum Kind : uint8_t {
        Value,
        Type,
        Function,
    };

    Kind kind;
    const struct Type* type;
    Evaluation evaluation;
    std::optional<ConstantValue> constantValue;
};

enum class Behavior : uint8_t {
    Return = 1 << 0,
    Break = 1 << 1,
    Continue = 1 << 2,
    Next = 1 << 3,
};
using Behaviors = OptionSet<Behavior>;

using BreakTarget = std::variant<
    AST::SwitchStatement*,
    AST::LoopStatement*,
    AST::ForStatement*,
    AST::WhileStatement*,
    AST::Continuing*
>;

static ASCIILiteral bindingKindToString(Binding::Kind kind)
{
    switch (kind) {
    case Binding::Value:
        return "value"_s;
    case Binding::Type:
        return "type"_s;
    case Binding::Function:
        return "function"_s;
    }
}

static ASCIILiteral evaluationToString(Evaluation evaluation)
{
    switch (evaluation) {
    case Evaluation::Constant:
        return "constant"_s;
    case Evaluation::Override:
        return "override"_s;
    case Evaluation::Runtime:
        return "runtime"_s;
    }
}

static ASCIILiteral variableFlavorToString(AST::VariableFlavor flavor)
{
    switch (flavor) {
    case AST::VariableFlavor::Var:
        return "var"_s;
    case AST::VariableFlavor::Let:
        return "let"_s;
    case AST::VariableFlavor::Const:
        return "const"_s;
    case AST::VariableFlavor::Override:
        return "override"_s;
    }
}

class TypeChecker : public AST::ScopedVisitor<Binding> {
    using Base  = AST::ScopedVisitor<Binding>;
    using Base::visit;

public:
    TypeChecker(ShaderModule&);

    std::optional<FailedCheck> check();

    // Declarations
    void visit(AST::Structure&) override;
    void visit(AST::Variable&) override;
    void visit(AST::Function&) override;
    void visit(AST::TypeAlias&) override;
    void visit(AST::ConstAssert&) override;

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
    void visit(AST::CallStatement&) override;
    void visit(AST::CompoundAssignmentStatement&) override;
    void visit(AST::DecrementIncrementStatement&) override;
    void visit(AST::IfStatement&) override;
    void visit(AST::PhonyAssignmentStatement&) override;
    void visit(AST::ReturnStatement&) override;
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
    const Type* vectorFieldAccess(const Types::Vector&, AST::FieldAccessExpression&);
    void visitAttributes(AST::Attribute::List&);
    void bitcast(AST::CallExpression&, const Vector<const Type*>&);

    template<typename... Arguments>
    void typeError(const SourceSpan&, Arguments&&...);

    enum class InferBottom : bool { No, Yes };
    template<typename... Arguments>
    void typeError(InferBottom, const SourceSpan&, Arguments&&...);

    const Type* check(AST::Expression&, Constraint, Evaluation);
    const Type* infer(AST::Expression&, Evaluation, DiscardResult = DiscardResult::No);
    const Type* resolve(AST::Expression&);
    const Type* lookupType(const AST::Identifier&);
    void inferred(const Type*);
    bool unify(const Type*, const Type*) WARN_UNUSED_RETURN;
    bool isBottom(const Type*) const;
    void introduceType(const AST::Identifier&, const Type*);
    void introduceValue(const AST::Identifier&, const Type*, Evaluation = Evaluation::Runtime, std::optional<ConstantValue> = std::nullopt);
    void introduceFunction(const AST::Identifier&, const Type*);
    bool convertValue(const SourceSpan&, const Type*, std::optional<ConstantValue>&);
    bool convertValueImpl(const SourceSpan&, const Type*, ConstantValue&);

    void binaryExpression(const SourceSpan&, AST::Expression*, AST::BinaryOperation, AST::Expression&, AST::Expression&);

    template<typename TargetConstructor, typename Validator, typename... Arguments>
    void allocateSimpleConstructor(ASCIILiteral, TargetConstructor, const Validator&, Arguments&&...);
    void allocateTextureStorageConstructor(ASCIILiteral, Types::TextureStorage::Kind);

    bool isModuleScope() const;

    std::optional<AccessMode> accessMode(AST::Expression&);
    std::optional<TexelFormat> texelFormat(AST::Expression&);
    std::optional<AddressSpace> addressSpace(AST::Expression&);

    template<typename CallArguments>
    const Type* chooseOverload(ASCIILiteral, const SourceSpan&, AST::Expression*, const String&, CallArguments&& valueArguments, const Vector<const Type*>& typeArguments);

    template<typename Node>
    void setConstantValue(Node&, const Type*, const ConstantValue&);

    Behaviors analyze(AST::Statement&);
    Behaviors analyze(AST::CompoundStatement&);
    Behaviors analyze(AST::ForStatement&);
    Behaviors analyze(AST::IfStatement&);
    Behaviors analyze(AST::LoopStatement&);
    Behaviors analyze(AST::SwitchStatement&);
    Behaviors analyze(AST::WhileStatement&);
    Behaviors analyzeStatements(AST::Statement::List&);

    ShaderModule& m_shaderModule;
    const Type* m_inferredType { nullptr };
    const Type* m_returnType { nullptr };
    Evaluation m_evaluation { Evaluation::Runtime };
    DiscardResult m_discardResult { DiscardResult::No };

    TypeStore& m_types;
    Vector<Error> m_errors;
    Vector<BreakTarget> m_breakTargetStack;
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
                typeError(InferBottom::No, type.span(), "'ptr' requires at least 2 template argument"_s);
                return m_types.bottomType();
            }

            if (argumentCount > 3) {
                typeError(InferBottom::No, type.span(), "'ptr' requires at most 3 template argument"_s);
                return m_types.bottomType();
            }

            auto maybeAddressSpace = addressSpace(type.arguments()[0]);
            if (!maybeAddressSpace)
                return m_types.bottomType();
            auto addressSpace = *maybeAddressSpace;

            auto* elementType = resolve(type.arguments()[1]);
            if (isBottom(elementType))
                return m_types.bottomType();

            if (UNLIKELY(!elementType->isStorable())) {
                typeError(InferBottom::No, type.span(), '\'', *elementType, "' cannot be used as the store type of a pointer"_s);
                return m_types.bottomType();
            }

            if (UNLIKELY(std::holds_alternative<Types::Atomic>(*elementType) && addressSpace != AddressSpace::Storage && addressSpace != AddressSpace::Workgroup)) {
                typeError(InferBottom::No, type.span(), '\'', *elementType, "' atomic variables must have <storage> or <workgroup> address space"_s);
                return m_types.bottomType();
            }

            if (UNLIKELY(elementType->containsRuntimeArray() && addressSpace != AddressSpace::Storage)) {
                typeError(InferBottom::No, type.span(), "runtime-sized arrays can only be used in the <storage> address space"_s);
                return m_types.bottomType();
            }

            AccessMode accessMode;
            if (argumentCount > 2) {
                if (addressSpace != AddressSpace::Storage) {
                    typeError(InferBottom::No, type.arguments()[2].span(), "only pointers in <storage> address space may specify an access mode"_s);
                    return m_types.bottomType();
                }

                auto maybeAccessMode = this->accessMode(type.arguments()[2]);
                if (!maybeAccessMode)
                    return m_types.bottomType();
                accessMode = *maybeAccessMode;
            } else {
                switch (addressSpace) {
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

            return m_types.pointerType(addressSpace, elementType, accessMode);
        }
    ));

    introduceType(AST::Identifier::make("atomic"_s), m_types.typeConstructorType(
        "atomic"_s,
        [this](AST::ElaboratedTypeExpression& type) -> const Type* {
            if (type.arguments().size() != 1) {
                typeError(InferBottom::No, type.span(), "'atomic' requires 1 template arguments"_s);
                return m_types.bottomType();
            }

            auto* elementType = resolve(type.arguments()[0]);
            if (isBottom(elementType))
                return m_types.bottomType();

            if (elementType != m_types.i32Type() && elementType != m_types.u32Type()) {
                typeError(InferBottom::No, type.arguments()[0].span(), "atomic only supports i32 or u32 types"_s);
                return m_types.bottomType();
            }

            return m_types.atomicType(elementType);
        }
    ));

    const auto& validateVector = [&](const Type* element) -> std::optional<String> {
        if (!satisfies(element, Constraints::Scalar))
            return { "vector element type must be a scalar type"_s };
        return std::nullopt;
    };

    allocateSimpleConstructor("vec2"_s, &TypeStore::vectorType, validateVector, 2);
    allocateSimpleConstructor("vec3"_s, &TypeStore::vectorType, validateVector, 3);
    allocateSimpleConstructor("vec4"_s, &TypeStore::vectorType, validateVector, 4);

    const auto& validateMatrix = [&](const Type* element) -> std::optional<String> {
        if (!satisfies(element, Constraints::Float))
            return { "matrix element type must be a floating point type"_s };
        return std::nullopt;
    };
    allocateSimpleConstructor("mat2x2"_s, &TypeStore::matrixType, validateMatrix, 2, 2);
    allocateSimpleConstructor("mat2x3"_s, &TypeStore::matrixType, validateMatrix, 2, 3);
    allocateSimpleConstructor("mat2x4"_s, &TypeStore::matrixType, validateMatrix, 2, 4);
    allocateSimpleConstructor("mat3x2"_s, &TypeStore::matrixType, validateMatrix, 3, 2);
    allocateSimpleConstructor("mat3x3"_s, &TypeStore::matrixType, validateMatrix, 3, 3);
    allocateSimpleConstructor("mat3x4"_s, &TypeStore::matrixType, validateMatrix, 3, 4);
    allocateSimpleConstructor("mat4x2"_s, &TypeStore::matrixType, validateMatrix, 4, 2);
    allocateSimpleConstructor("mat4x3"_s, &TypeStore::matrixType, validateMatrix, 4, 3);
    allocateSimpleConstructor("mat4x4"_s, &TypeStore::matrixType, validateMatrix, 4, 4);

    const auto& validateTexture = [&](const Type* element) -> std::optional<String> {
        if (!satisfies(element, Constraints::Concrete32BitNumber))
            return { "texture sampled type must be one 'i32', 'u32' or 'f32'"_s };
        return std::nullopt;
    };
    allocateSimpleConstructor("texture_1d"_s, &TypeStore::textureType, validateTexture, Types::Texture::Kind::Texture1d);
    allocateSimpleConstructor("texture_2d"_s, &TypeStore::textureType, validateTexture, Types::Texture::Kind::Texture2d);
    allocateSimpleConstructor("texture_2d_array"_s, &TypeStore::textureType, validateTexture, Types::Texture::Kind::Texture2dArray);
    allocateSimpleConstructor("texture_3d"_s, &TypeStore::textureType, validateTexture, Types::Texture::Kind::Texture3d);
    allocateSimpleConstructor("texture_cube"_s, &TypeStore::textureType, validateTexture, Types::Texture::Kind::TextureCube);
    allocateSimpleConstructor("texture_cube_array"_s, &TypeStore::textureType, validateTexture, Types::Texture::Kind::TextureCubeArray);
    allocateSimpleConstructor("texture_multisampled_2d"_s, &TypeStore::textureType, validateTexture, Types::Texture::Kind::TextureMultisampled2d);

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
    ContextScope moduleScope(this);

    Base::visit(m_shaderModule);

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
    visitAttributes(structure.attributes());

    HashMap<String, const Type*> fields;
    for (unsigned i = 0; i < structure.members().size(); ++i) {
        auto& member = structure.members()[i];
        visitAttributes(member.attributes());
        auto* memberType = resolve(member.type());

        if (UNLIKELY(std::holds_alternative<Types::Bottom>(*memberType))) {
            introduceType(structure.name(), m_types.bottomType());
            return;
        }
        if (UNLIKELY(!memberType->hasCreationFixedFootprint())) {
            if (!memberType->containsRuntimeArray()) {
                typeError(InferBottom::No, member.span(), "type '"_s, *memberType, "' cannot be used as a struct member because it does not have creation-fixed footprint"_s);
                introduceType(structure.name(), m_types.bottomType());
                return;
            }

            if (i != structure.members().size() - 1) {
                typeError(InferBottom::No, member.span(), "runtime arrays may only appear as the last member of a struct"_s);
                introduceType(structure.name(), m_types.bottomType());
                return;
            }
        }

        auto result = fields.add(member.name().id(), memberType);
        ASSERT_UNUSED(result, result.isNewEntry);
    }
    const Type* structType = m_types.structType(structure, WTFMove(fields));
    structure.m_inferredType = structType;
    introduceType(structure.name(), structType);
}

void TypeChecker::visit(AST::Variable& variable)
{
    visitAttributes(variable.attributes());

    const Type* result = nullptr;
    std::optional<ConstantValue>* value = nullptr;

    const auto& error = [&](auto... arguments) {
        typeError(InferBottom::No, variable.span(), arguments...);
        introduceValue(variable.name(), m_types.bottomType());
    };

    Evaluation evaluation { Evaluation::Runtime };
    if (variable.flavor() == AST::VariableFlavor::Const)
        evaluation = Evaluation::Constant;
    else if (variable.flavor() == AST::VariableFlavor::Override)
        evaluation = Evaluation::Override;

    if (variable.maybeTypeName())
        result = resolve(*variable.maybeTypeName());
    if (variable.maybeInitializer()) {
        auto initializerEvaluation = evaluation;
        if (variable.flavor() == AST::VariableFlavor::Var && isModuleScope())
            initializerEvaluation = Evaluation::Override;
        auto* initializerType = infer(*variable.maybeInitializer(), initializerEvaluation);
        auto& constantValue = variable.maybeInitializer()->m_constantValue;
        if (constantValue.has_value())
            value = &constantValue;

        if (auto* reference = std::get_if<Types::Reference>(initializerType)) {
            initializerType = reference->element;
            variable.maybeInitializer()->m_inferredType = initializerType;
        }

        if (!result) {
            if (initializerType == m_types.voidType()) {
                typeError(InferBottom::No, variable.span(), "cannot initialize variable with expression of type 'void'"_s);
                initializerType = m_types.bottomType();
            }

            if (variable.flavor() == AST::VariableFlavor::Const)
                result = initializerType;
            else {
                result = concretize(initializerType, m_types);
                if (!result)
                    return error('\'', *initializerType, "' cannot be used as the type of a '"_s, variableFlavorToString(variable.flavor()), '\'');
                variable.maybeInitializer()->m_inferredType = result;
            }
        } else if (unify(result, initializerType))
            variable.maybeInitializer()->m_inferredType = result;
        else {
            return error("cannot initialize var of type '"_s, *result, "' with value of type '"_s, *initializerType, '\'');
        }
    }

    switch (variable.flavor()) {
    case AST::VariableFlavor::Let:
        if (isModuleScope())
            return error("module-scope 'let' is invalid, use 'const'"_s);
        if (!result->isConstructible() && !std::holds_alternative<Types::Pointer>(*result))
            return error('\'', *result, "' cannot be used as the type of a 'let'"_s);
        RELEASE_ASSERT(variable.maybeInitializer());
        break;
    case AST::VariableFlavor::Const:
        RELEASE_ASSERT(variable.maybeInitializer());
        if (!result->isConstructible())
            return error('\'', *result, "' cannot be used as the type of a 'const'"_s);
        break;
    case AST::VariableFlavor::Override:
        RELEASE_ASSERT(isModuleScope());
        if (!satisfies(result, Constraints::ConcreteScalar))
            return error('\'', *result, "' cannot be used as the type of an 'override'"_s);
        break;
    case AST::VariableFlavor::Var:
        AddressSpace addressSpace;
        AccessMode accessMode;
        if (auto* maybeQualifier = variable.maybeQualifier()) {
            addressSpace = maybeQualifier->addressSpace();
            accessMode = maybeQualifier->accessMode();
        } else if (!isModuleScope()) {
            addressSpace = AddressSpace::Function;
            accessMode = AccessMode::ReadWrite;
        } else {
            addressSpace = AddressSpace::Handle;
            accessMode = AccessMode::Read;
        }
        variable.m_addressSpace = addressSpace;
        variable.m_accessMode = accessMode;

        // https://www.w3.org/TR/WGSL/#var-and-value
        switch (addressSpace) {
        case AddressSpace::Storage:
            if (accessMode == AccessMode::Write)
                return error("access mode 'write' is not valid for the <storage> address space"_s);
            if (!result->isHostShareable())
                return error("type '"_s, *result, "' cannot be used in address space <storage> because it's not host-shareable"_s);
            if (accessMode == AccessMode::Read && std::holds_alternative<Types::Atomic>(*result))
                return error("atomic variables in <storage> address space must have read_write access mode"_s);
            break;
        case AddressSpace::Uniform:
            if (!result->isHostShareable())
                return error("type '"_s, *result, "' cannot be used in address space <uniform> because it's not host-shareable"_s);
            if (!result->isConstructible())
                return error("type '"_s, *result, "' cannot be used in address space <uniform> because it's not constructible"_s);
            break;
        case AddressSpace::Workgroup:
            if (!result->hasFixedFootprint())
                return error("type '"_s, *result, "' cannot be used in address space <workgroup> because it doesn't have fixed footprint"_s);
            break;
        case AddressSpace::Function:
            if (!result->isConstructible())
                return error("type '"_s, *result, "' cannot be used in address space <function> because it's not constructible"_s);
            break;
        case AddressSpace::Private:
            if (!result->isConstructible())
                return error("type '"_s, *result, "' cannot be used in address space <private> because it's not constructible"_s);
            break;
        case AddressSpace::Handle: {
            auto* primitive = std::get_if<Types::Primitive>(result);
            bool isTextureOrSampler = std::holds_alternative<Types::Texture>(*result)
                || std::holds_alternative<Types::TextureStorage>(*result)
                || std::holds_alternative<Types::TextureDepth>(*result)
                || (primitive && (
                    primitive->kind == Types::Primitive::TextureExternal
                    || primitive->kind == Types::Primitive::Sampler
                    || primitive->kind == Types::Primitive::SamplerComparison
                ));
            if (!isTextureOrSampler)
                return error("module-scope 'var' declarations that are not of texture or sampler types must provide an address space"_s);
            break;
        }
        }

        if (addressSpace == AddressSpace::Function && isModuleScope())
            return error("module-scope 'var' must not use address space 'function'"_s);
        if (addressSpace != AddressSpace::Function && !isModuleScope())
            return error("function-scope 'var' declaration must use 'function' address space"_s);
        if ((addressSpace == AddressSpace::Storage || addressSpace == AddressSpace::Uniform || addressSpace == AddressSpace::Handle || addressSpace == AddressSpace::Workgroup) && variable.maybeInitializer())
            return error("variables in the address space '"_s, toString(addressSpace), "' cannot have an initializer"_s);
        if (addressSpace != AddressSpace::Workgroup && result->containsOverrideArray())
            return error("array with an 'override' element count can only be used as the store type of a 'var<workgroup>'"_s);
    }

    if (value && !isBottom(result))
        convertValue(variable.span(), result, *value);

    if (variable.flavor() != AST::VariableFlavor::Const || result == m_types.bottomType())
        value = nullptr;

    variable.m_storeType = result;

    if (variable.flavor() == AST::VariableFlavor::Var) {
        result = m_types.referenceType(*variable.addressSpace(), result, *variable.accessMode());
        auto* typeName = variable.maybeTypeName();
        if (!typeName)
            typeName = &m_shaderModule.astBuilder().construct<AST::IdentifierExpression>(SourceSpan::empty(), AST::Identifier::make(result->toString()));
        auto& referenceType = m_shaderModule.astBuilder().construct<AST::ReferenceTypeExpression>(
            typeName->span(),
            *typeName
        );
        referenceType.m_inferredType = result;
        variable.m_referenceType = &referenceType;
    }

    introduceValue(variable.name(), result, evaluation, value ? std::optional<ConstantValue>(*value) : std::nullopt);
}

void TypeChecker::visit(AST::TypeAlias& alias)
{
    auto* type = resolve(alias.type());
    introduceType(alias.name(), type);
}

void TypeChecker::visit(AST::ConstAssert& assertion)
{
    auto* testType = infer(assertion.test(), Evaluation::Constant);
    if (!unify(m_types.boolType(), testType)) {
        typeError(InferBottom::No, assertion.test().span(), "const assertion condition must be a bool, got '"_s, *testType, '\'');
        return;
    }

    if (isBottom(testType))
        return;

    auto constantValue = assertion.test().constantValue();
    if (!constantValue) {
        typeError(InferBottom::No, assertion.test().span(), "const assertion requires a const-expression"_s);
        return;
    }

    if (!std::get<bool>(*constantValue))
        typeError(InferBottom::No, assertion.span(), "const assertion failed"_s);
}

void TypeChecker::visit(AST::Function& function)
{
    bool mustUse = false;
    for (auto& attribute : function.attributes()) {
        if (is<AST::MustUseAttribute>(attribute)) {
            mustUse = true;
            continue;
        }

        visit(attribute);
    }

    Vector<const Type*> parameters;
    parameters.reserveInitialCapacity(function.parameters().size());
    for (auto& parameter : function.parameters()) {
        visitAttributes(parameter.attributes());
        auto* parameterType = resolve(parameter.typeName());
        if (!parameterType->isConstructible() && !std::holds_alternative<Types::Pointer>(*parameterType) && !parameterType->isTexture() && !parameterType->isSampler()) {
            typeError(InferBottom::No, parameter.span(), "type of function parameter must be constructible or a pointer, sampler or texture"_s);
            parameterType = m_types.bottomType();
        }
        parameters.append(parameterType);
    }

    visitAttributes(function.returnAttributes());
    if (!function.maybeReturnType())
        m_returnType = m_types.voidType();
    else {
        m_returnType = resolve(*function.maybeReturnType());
        if (!m_returnType->isConstructible()) {
            m_returnType = m_types.bottomType();
            typeError(InferBottom::No, function.maybeReturnType()->span(), "function return type must be a constructible type"_s);
        }
    }

    {
        ContextScope functionContext(this);
        for (unsigned i = 0; i < parameters.size(); ++i)
            introduceValue(function.parameters()[i].name(), parameters[i]);
        AST::Visitor::visit(function.body());

        auto behaviors = analyze(function.body());
        if (behaviors.contains(Behavior::Next) && function.maybeReturnType())
            typeError(InferBottom::No, function.span(), "missing return at end of function"_s);
        ASSERT(!behaviors.containsAny({ Behavior::Break, Behavior::Continue }) || !m_errors.isEmpty());
    }

    const Type* functionType = m_types.functionType(WTFMove(parameters), m_returnType, mustUse);
    introduceFunction(function.name(), functionType);

    m_returnType = nullptr;
}

// Attributes
void TypeChecker::visit(AST::AlignAttribute& attribute)
{
    if (!check(attribute.alignment(), Constraints::ConcreteInteger, Evaluation::Constant))
        typeError(InferBottom::No, attribute.span(), "@align must be an i32 or u32 value"_s);
}

void TypeChecker::visit(AST::BindingAttribute& attribute)
{
    if (!check(attribute.binding(), Constraints::ConcreteInteger, Evaluation::Constant))
        typeError(InferBottom::No, attribute.span(), "@binding must be an i32 or u32 value"_s);
}

void TypeChecker::visit(AST::GroupAttribute& attribute)
{
    if (!check(attribute.group(), Constraints::ConcreteInteger, Evaluation::Constant))
        typeError(InferBottom::No, attribute.span(), "@group must be an i32 or u32 value"_s);
}

void TypeChecker::visit(AST::IdAttribute& attribute)
{
    if (!check(attribute.value(), Constraints::ConcreteInteger, Evaluation::Constant))
        typeError(InferBottom::No, attribute.span(), "@id must be an i32 or u32 value"_s);
}

void TypeChecker::visit(AST::LocationAttribute& attribute)
{
    if (!check(attribute.location(), Constraints::ConcreteInteger, Evaluation::Constant))
        typeError(InferBottom::No, attribute.span(), "@location must be an i32 or u32 value"_s);
}

void TypeChecker::visit(AST::SizeAttribute& attribute)
{
    if (!check(attribute.size(), Constraints::ConcreteInteger, Evaluation::Constant))
        typeError(InferBottom::No, attribute.span(), "@size must be an i32 or u32 value"_s);
}

void TypeChecker::visit(AST::WorkgroupSizeAttribute& attribute)
{
    auto* xType = infer(attribute.x(), Evaluation::Override);
    if (!satisfies(xType, Constraints::ConcreteInteger)) {
        typeError(InferBottom::No, attribute.span(), "@workgroup_size x dimension must be an i32 or u32 value"_s);
        return;
    }

    const Type* yType = nullptr;
    const Type* zType = nullptr;
    if (auto* y = attribute.maybeY()) {
        yType = infer(*y, Evaluation::Override);
        if (!satisfies(yType, Constraints::ConcreteInteger)) {
            typeError(InferBottom::No, attribute.span(), "@workgroup_size y dimension must be an i32 or u32 value"_s);
            return;
        }

        if (auto* z = attribute.maybeZ()) {
            zType = infer(*z, Evaluation::Override);
            if (!satisfies(zType, Constraints::ConcreteInteger)) {
                typeError(InferBottom::No, attribute.span(), "@workgroup_size z dimension must be an i32 or u32 value"_s);
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
        typeError(InferBottom::No, attribute.span(), "@workgroup_size arguments must be of the same type, either i32 or u32"_s);
}

// Statements
void TypeChecker::visit(AST::AssignmentStatement& statement)
{
    auto* lhs = infer(statement.lhs(), Evaluation::Runtime);
    auto* rhs = infer(statement.rhs(), Evaluation::Runtime);

    if (isBottom(lhs))
        return;

    auto* reference = std::get_if<Types::Reference>(lhs);
    if (!reference) {
        typeError(InferBottom::No, statement.span(), "cannot assign to a value of type '"_s, *lhs, '\'');
        return;
    }
    if (reference->accessMode == AccessMode::Read) {
        typeError(InferBottom::No, statement.span(), "cannot store into a read-only type '"_s, *lhs, '\'');
        return;
    }
    if (!unify(reference->element, rhs)) {
        typeError(InferBottom::No, statement.span(), "cannot assign value of type '"_s, *rhs, "' to '"_s, *reference->element, '\'');
        return;
    }

    statement.rhs().m_inferredType = reference->element;
    auto& value = statement.rhs().m_constantValue;
    if (value.has_value())
        convertValue(statement.rhs().span(), statement.rhs().inferredType(), value);
}

void TypeChecker::visit(AST::CallStatement& statement)
{
    infer(statement.call(), Evaluation::Runtime, DiscardResult::Yes);
}

void TypeChecker::visit(AST::CompoundAssignmentStatement& statement)
{
    auto* left = infer(statement.leftExpression(), Evaluation::Runtime);
    auto* referenceType = std::get_if<Types::Reference>(left);
    if (!referenceType) {
        typeError(InferBottom::No, statement.span(), "cannot assign to a value of type '"_s, *left, '\'');
        return;
    }

    binaryExpression(statement.span(), nullptr, statement.operation(), statement.leftExpression(), statement.rightExpression());
    // Reset the inferred type since this is a statement
    m_inferredType = nullptr;
}

void TypeChecker::visit(AST::DecrementIncrementStatement& statement)
{
    auto* expression = infer(statement.expression(), Evaluation::Runtime);
    if (isBottom(expression))
        return;

    auto* reference = std::get_if<Types::Reference>(expression);
    if (!reference) {
        typeError(InferBottom::No, statement.span(), "cannot modify a value of type '"_s, *expression, '\'');
        return;
    }
    if (reference->accessMode == AccessMode::Read) {
        typeError(InferBottom::No, statement.span(), "cannot modify read-only type '"_s, *expression, '\'');
        return;
    }
    if (!unify(m_types.i32Type(), reference->element) && !unify(m_types.u32Type(), reference->element)) {
        ASCIILiteral operation;
        switch (statement.operation()) {
        case AST::DecrementIncrementStatement::Operation::Increment:
            operation = "increment"_s;
            break;
        case AST::DecrementIncrementStatement::Operation::Decrement:
            operation = "decrement"_s;
            break;
        }
        typeError(InferBottom::No, statement.span(), operation, " can only be applied to integers, found "_s, *reference->element);
    }
}

void TypeChecker::visit(AST::IfStatement& statement)
{
    auto* test = infer(statement.test(), Evaluation::Runtime);

    if (!unify(test, m_types.boolType()))
        typeError(InferBottom::No, statement.test().span(), "expected 'bool', found '"_s, *test, "'"_s);

    visit(statement.trueBody());
    if (statement.maybeFalseBody())
        visit(*statement.maybeFalseBody());
}

void TypeChecker::visit(AST::PhonyAssignmentStatement& statement)
{
    infer(statement.rhs(), Evaluation::Runtime);
    // There is nothing to unify with since result of the right-hand side is
    // discarded.
}

void TypeChecker::visit(AST::ReturnStatement& statement)
{
    const Type* type;
    auto* expression = statement.maybeExpression();
    if (expression)
        type = infer(*expression, Evaluation::Runtime);
    else
        type = m_types.voidType();

    if (!unify(m_returnType, type))
        typeError(InferBottom::No, statement.span(), "return statement type does not match its function return type, returned '"_s, *type, "', expected '"_s, *m_returnType, '\'');
    else if (expression) {
        expression->m_inferredType = m_returnType;
        if (auto& value = expression->m_constantValue)
            convertValue(expression->span(), m_returnType, value);
    }
}

void TypeChecker::visit(AST::ForStatement& statement)
{
    ContextScope forScope(this);
    if (auto* initializer = statement.maybeInitializer())
        Base::visit(*initializer);

    if (auto* test = statement.maybeTest()) {
        auto* testType = infer(*test, Evaluation::Runtime);
        if (!unify(m_types.boolType(), testType))
            typeError(InferBottom::No, test->span(), "for-loop condition must be bool, got "_s, *testType);
    }

    if (auto* update = statement.maybeUpdate())
        Base::visit(*update);

    visit(statement.body());
}

void TypeChecker::visit(AST::LoopStatement& statement)
{
    ContextScope loopScope(this);
    visitAttributes(statement.attributes());

    for (auto& statement : statement.body())
        Base::visit(statement);

    if (auto& continuing = statement.continuing())
        visit(*continuing);
}

void TypeChecker::visit(AST::WhileStatement& statement)
{
    auto* testType = infer(statement.test(), Evaluation::Runtime);
    if (!unify(m_types.boolType(), testType))
        typeError(InferBottom::No, statement.test().span(), "while condition must be bool, got "_s, *testType);

    visit(statement.body());
}

void TypeChecker::visit(AST::SwitchStatement& statement)
{
    auto* valueType = infer(statement.value(), Evaluation::Runtime);
    if (!satisfies(valueType, Constraints::ConcreteInteger)) {
        typeError(InferBottom::No, statement.value().span(), "switch selector must be of type i32 or u32"_s);
        valueType = m_types.bottomType();
    }

    const auto& visitClause = [&](AST::SwitchClause& clause) {
        for (auto& selector : clause.selectors) {
            auto* selectorType = infer(selector, Evaluation::Runtime);
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
            typeError(InferBottom::No, selector.span(), "the case selector values must have the same type as the selector expression: the selector expression has type '"_s, *valueType, "' and case selector has type '"_s, *selectorType, '\'');
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
    const auto& accessImpl = [&](const Type* baseType, bool* canBeReference = nullptr, bool* isVector = nullptr) -> const Type* {
        if (isBottom(baseType))
            return m_types.bottomType();

        if (std::holds_alternative<Types::Struct>(*baseType)) {
            auto& structType = std::get<Types::Struct>(*baseType);
            auto it = structType.fields.find(access.fieldName().id());
            if (it == structType.fields.end()) {
                typeError(access.span(), "struct '"_s, *baseType, "' does not have a member called '"_s, access.fieldName(), '\'');
                return nullptr;
            }
            if (auto constant = access.base().constantValue()) {
                auto& constantStruct = std::get<ConstantStruct>(*constant);
                access.setConstantValue(constantStruct.fields.get(access.fieldName().id()));
            }
            return it->value;
        }

        if (auto* primitiveStruct = std::get_if<Types::PrimitiveStruct>(baseType)) {
            const auto& keys = Types::PrimitiveStruct::keys[primitiveStruct->kind];
            auto* key = keys.tryGet(access.fieldName().id());
            if (!key) {
                typeError(access.span(), "struct '"_s, *baseType, "' does not have a member called '"_s, access.fieldName(), '\'');
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
            if (isVector)
                *isVector = true;
            if (result && canBeReference)
                *canBeReference = !std::holds_alternative<Types::Vector>(*result);
            return result;
        }

        typeError(access.span(), "invalid member access expression. Expected vector or struct, got '"_s, *baseType, '\'');
        return nullptr;
    };

    const auto& referenceImpl = [&](const auto& type) {
        bool canBeReference = true;
        bool isVector = false;
        if (const Type* result = accessImpl(type.element, &canBeReference, &isVector)) {
            if (canBeReference)
                result = m_types.referenceType(type.addressSpace, result, type.accessMode, isVector);
            inferred(result);
        }
        return;
    };

    auto* baseType = infer(access.base(), m_evaluation);
    if (const auto* reference = std::get_if<Types::Reference>(baseType)) {
        referenceImpl(*reference);
        return;
    }

    if (const auto* pointer = std::get_if<Types::Pointer>(baseType)) {
        referenceImpl(*pointer);
        return;
    }

    if (const Type* result = accessImpl(baseType))
        inferred(result);
}

void TypeChecker::visit(AST::IndexAccessExpression& access)
{
    const auto& constantAccess = [&]<typename T>(std::optional<unsigned> typeSize) {
        auto constantBase = access.base().constantValue();
        auto constantIndex = access.index().constantValue();

        if (!constantIndex)
            return;

        auto size = typeSize.value_or(0);
        if (!size && constantBase)
            size = std::get<T>(*constantBase).upperBound();
        if (!size)
            return;

        auto index = constantIndex->integerValue();
        if (index < 0 || static_cast<size_t>(index) >= size) {
            typeError(InferBottom::No, access.span(), "index "_s, index, " is out of bounds [0.."_s, size - 1, ']');
            return;
        }

        if (constantBase)
            access.setConstantValue(std::get<T>(*constantBase)[index]);
    };

    const auto& accessImpl = [&](const Type* base, bool* isVector = nullptr) -> const Type* {
        if (isBottom(base))
            return m_types.bottomType();


        const Type* result = nullptr;
        if (auto* array = std::get_if<Types::Array>(base)) {
            result = array->element;
            std::optional<unsigned> size;
            if (auto* constantSize = std::get_if<unsigned>(&array->size))
                size = *constantSize;
            constantAccess.operator()<ConstantArray>(size);
        } else if (auto* vector = std::get_if<Types::Vector>(base)) {
            if (isVector)
                *isVector = true;
            result = vector->element;
            constantAccess.operator()<ConstantVector>(vector->size);
        } else if (auto* matrix = std::get_if<Types::Matrix>(base)) {
            result = m_types.vectorType(matrix->rows, matrix->element);
            constantAccess.operator()<ConstantMatrix>(matrix->columns);
        }

        if (!result) {
            typeError(access.span(), "cannot index type '"_s, *base, '\'');
            return nullptr;
        }

        if (!access.index().constantValue().has_value()) {
            result = concretize(result, m_types);
            RELEASE_ASSERT(result);
        }
        return result;
    };

    auto* base = infer(access.base(), m_evaluation);
    auto* index = infer(access.index(), m_evaluation);

    if (!unify(m_types.i32Type(), index) && !unify(m_types.u32Type(), index) && !unify(m_types.abstractIntType(), index)) {
        typeError(access.span(), "index must be of type 'i32' or 'u32', found: '"_s, *index, '\'');
        return;
    }

    const auto& referenceImpl = [&](const auto& type) {
        bool isVector = false;
        if (const Type* result = accessImpl(type.element, &isVector)) {
            result = m_types.referenceType(type.addressSpace, result, type.accessMode, isVector);
            inferred(result);
        }
        return;
    };

    if (const auto* reference = std::get_if<Types::Reference>(base)) {
        referenceImpl(*reference);
        return;
    }

    if (const auto* pointer = std::get_if<Types::Pointer>(base)) {
        referenceImpl(*pointer);
        return;
    }

    if (const Type* result = accessImpl(base))
        inferred(result);
}

void TypeChecker::visit(AST::BinaryExpression& binary)
{
    binaryExpression(binary.span(), &binary, binary.operation(), binary.leftExpression(), binary.rightExpression());
}

void TypeChecker::binaryExpression(const SourceSpan& span, AST::Expression* expression, AST::BinaryOperation operation, AST::Expression& leftExpression, AST::Expression& rightExpression)
{
    chooseOverload("operator"_s, span, expression, toASCIILiteral(operation), ReferenceWrapperVector<AST::Expression, 2> { leftExpression, rightExpression }, { });

    ASCIILiteral operationName;
    if (operation == AST::BinaryOperation::Divide)
        operationName = "division"_s;
    else if (operation == AST::BinaryOperation::Modulo)
        operationName = "modulo"_s;
    if (!operationName.isNull()) {
        auto* rightType = rightExpression.inferredType();
        if (auto* vectorType = std::get_if<Types::Vector>(rightType))
            rightType = vectorType->element;
        if (satisfies(rightType, Constraints::Integer)) {
            if (operation == AST::BinaryOperation::Divide)
                m_shaderModule.setUsesDivision();
            else
                m_shaderModule.setUsesModulo();
            auto leftValue = leftExpression.constantValue();
            auto rightValue = rightExpression.constantValue();
            if (!leftValue && rightValue && containsZero(*rightValue, rightExpression.inferredType()))
                typeError(InferBottom::No, span, "invalid "_s, operationName, " by zero"_s);
        }
    }
}

void TypeChecker::visit(AST::IdentifierExpression& identifier)
{
    auto* binding = readVariable(identifier.identifier());
    if (UNLIKELY(!binding)) {
        typeError(identifier.span(), "unresolved identifier '"_s, identifier.identifier(), '\'');
        return;
    }

    if (UNLIKELY(binding->kind != Binding::Value)) {
        typeError(identifier.span(), "cannot use "_s, bindingKindToString(binding->kind), " '"_s, identifier.identifier(), "' as value"_s);
        return;
    }

    if (UNLIKELY(binding->evaluation > m_evaluation)) {
        typeError(identifier.span(), "cannot use "_s, evaluationToString(binding->evaluation), " value in "_s, evaluationToString(m_evaluation), " expression"_s);
        return;
    }

    inferred(binding->type);
    if (binding->constantValue.has_value())
        setConstantValue(identifier, binding->type, *binding->constantValue);
}

void TypeChecker::visit(AST::CallExpression& call)
{
    auto& target = call.target();
    bool isNamedType = is<AST::IdentifierExpression>(target);
    bool isParameterizedType = is<AST::ElaboratedTypeExpression>(target);
    bool isArrayType = is<AST::ArrayTypeExpression>(target);

    Vector<const Type*> typeArguments;
    String targetName = [&]() -> String {
        if (isNamedType)
            return downcast<AST::IdentifierExpression>(target).identifier();
        if (isArrayType)
            return "array"_s;
        auto& elaborated = downcast<AST::ElaboratedTypeExpression>(target);
        for (auto& argument : elaborated.arguments())
            typeArguments.append(resolve(argument));
        return elaborated.base();
    }();

    auto* targetBinding = readVariable(targetName);
    if (targetBinding) {
        target.m_inferredType = targetBinding->type;
        if (targetBinding->kind == Binding::Type) {
            call.m_isConstructor = true;
            if (auto* structType = std::get_if<Types::Struct>(targetBinding->type)) {
                if (!targetBinding->type->isConstructible()) {
                    typeError(call.span(), "struct is not constructible"_s);
                    return;
                }

                if (UNLIKELY(m_discardResult == DiscardResult::Yes)) {
                    typeError(call.span(), "value constructor evaluated but not used"_s);
                    return;
                }

                auto numberOfArguments = call.arguments().size();
                auto numberOfFields = structType->fields.size();
                if (numberOfArguments && numberOfArguments != numberOfFields) {
                    auto errorKind = numberOfArguments < numberOfFields ? "few"_s : "many"_s;
                    typeError(call.span(), "struct initializer has too "_s, errorKind, " inputs: expected "_s, numberOfFields, ", found "_s, numberOfArguments);
                    return;
                }

                HashMap<String, ConstantValue> constantFields;
                bool isConstant = true;
                for (unsigned i = 0; i < numberOfArguments; ++i) {
                    auto& argument = call.arguments()[i];
                    auto& member = structType->structure.members()[i];
                    auto* fieldType = structType->fields.get(member.name());
                    auto* argumentType = infer(argument, m_evaluation);
                    if (!unify(fieldType, argumentType)) {
                        typeError(argument.span(), "type in struct initializer does not match struct member type: expected '"_s, *fieldType, "', found '"_s, *argumentType, '\'');
                        return;
                    }
                    argument.m_inferredType = fieldType;
                    auto& value = argument.m_constantValue;
                    if (value.has_value() && convertValue(argument.span(), argument.inferredType(), value)) {
                        constantFields.set(member.name(), *value);
                        continue;
                    }
                    isConstant = false;
                }
                if (isConstant) {
                    if (numberOfArguments)
                        setConstantValue(call, targetBinding->type, ConstantStruct { WTFMove(constantFields) });
                    else
                        setConstantValue(call, targetBinding->type, zeroValue(targetBinding->type));
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
                targetName = makeString("mat"_s, matrixType->columns, 'x', matrixType->rows);
            }

            if (std::holds_alternative<Types::Primitive>(*targetBinding->type))
                targetName = targetBinding->type->toString();
        } else if (targetBinding->kind == Binding::Function) {
            auto& functionType = std::get<Types::Function>(*targetBinding->type);
            auto numberOfArguments = call.arguments().size();
            auto numberOfParameters = functionType.parameters.size();
            if (UNLIKELY(m_evaluation < Evaluation::Runtime)) {
                typeError(call.span(), "cannot call function from "_s, evaluationToString(m_evaluation), " context"_s);
                return;
            }

            if (UNLIKELY(numberOfArguments != numberOfParameters)) {
                auto errorKind = numberOfArguments < numberOfParameters ? "few"_s : "many"_s;
                typeError(call.span(), "funtion call has too "_s, errorKind, " arguments: expected "_s, numberOfParameters, ", found "_s, numberOfArguments);
                return;
            }

            if (m_discardResult == DiscardResult::Yes && functionType.mustUse)
                typeError(InferBottom::No, call.span(), "ignoring return value of function '"_s, targetName, "' annotated with @must_use"_s);

            for (unsigned i = 0; i < numberOfArguments; ++i) {
                auto& argument = call.arguments()[i];
                auto* parameterType = functionType.parameters[i];
                auto* argumentType = infer(argument, m_evaluation);
                if (!unify(parameterType, argumentType)) {
                    typeError(argument.span(), "type in function call does not match parameter type: expected '"_s, *parameterType, "', found '"_s, *argumentType, '\'');
                    return;
                }
                argument.m_inferredType = parameterType;
                auto& value = argument.m_constantValue;
                if (value.has_value())
                    convertValue(argument.span(), argument.inferredType(), value);
            }
            inferred(functionType.result);
            return;
        } else {
            typeError(target.span(), "cannot call value of type '"_s, *targetBinding->type, '\'');
            return;
        }
    }

    if (isNamedType || isParameterizedType) {
        auto* result = chooseOverload("initializer"_s, call.span(), &call, targetName, call.arguments(), typeArguments);
        if (result) {
            target.m_inferredType = result;
            if (isBottom(result))
                return;

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
            else if (targetName == "extractBits"_s)
                m_shaderModule.setUsesExtractBits();
            else if (
                targetName == "textureGather"_s
                || targetName == "textureGatherCompare"_s
                || targetName == "textureSample"_s
                || targetName == "textureSampleBias"_s
                || targetName == "textureSampleCompare"_s
                || targetName == "textureSampleCompareLevel"_s
                || targetName == "textureSampleGrad"_s
                || targetName == "textureSampleLevel"_s
            ) {
                if (targetName == "textureGather"_s) {
                    auto& component = call.arguments()[0];
                    if (satisfies(component.inferredType(), Constraints::ConcreteInteger)) {
                        auto& constant = component.constantValue();
                        if (!constant)
                            typeError(InferBottom::No, component.span(), "the component argument must be a const-expression"_s);
                        else {
                            auto componentValue = constant->integerValue();
                            if (componentValue < 0 || componentValue > 3)
                                typeError(InferBottom::No, component.span(), "the component argument must be at least 0 and at most 3. component is "_s, String::number(componentValue));
                        }
                    }
                }

                auto& lastArg = call.arguments().last();
                auto* vectorType = std::get_if<Types::Vector>(lastArg.inferredType());
                if (!vectorType || vectorType->size != 2 || vectorType->element != m_types.i32Type())
                    return;

                auto& maybeConstant = lastArg.constantValue();
                if (!maybeConstant.has_value()) {
                    typeError(InferBottom::No, lastArg.span(), "the offset argument must be a const-expression"_s);
                    return;
                }

                auto& vector = std::get<ConstantVector>(*maybeConstant);
                for (unsigned i = 0; i < 2; ++i) {
                    auto& i32 = std::get<int32_t>(vector.elements[i]);
                    if (i32 < -8 || i32 > 7) {
                        typeError(InferBottom::No, lastArg.span(), "each component of the offset argument must be at least -8 and at most 7. offset component "_s, String::number(i), " is "_s, String::number(i32));
                        break;
                    }
                }
            }
            return;
        }

        // FIXME: similarly to above: this shouldn't be a string check
        if (targetName == "bitcast"_s) {
            bitcast(call, typeArguments);
            return;
        }

        typeError(target.span(), "unresolved call target '"_s, targetName, '\'');
        return;
    }

    RELEASE_ASSERT(isArrayType);
    auto& array = uncheckedDowncast<AST::ArrayTypeExpression>(target);
    const Type* elementType = nullptr;
    unsigned elementCount;

    if (array.maybeElementType()) {
        if (!array.maybeElementCount()) {
            typeError(call.span(), "cannot construct a runtime-sized array"_s);
            return;
        }
        elementType = resolve(*array.maybeElementType());
        auto* elementCountType = infer(*array.maybeElementCount(), m_evaluation);

        if (isBottom(elementType) || isBottom(elementCountType)) {
            inferred(m_types.bottomType());
            return;
        }

        if (!unify(m_types.i32Type(), elementCountType) && !unify(m_types.u32Type(), elementCountType)) {
            typeError(array.span(), "array count must be an i32 or u32 value, found '"_s, *elementCountType, '\'');
            return;
        }

        if (!elementType->isConstructible()) {
            typeError(array.span(), '\'', *elementType, "' cannot be used as an element type of an array"_s);
            return;
        }

        auto constantValue = array.maybeElementCount()->constantValue();
        if (!constantValue) {
            typeError(call.span(), "array must have constant size in order to be constructed"_s);
            return;
        }

        auto intElementCount = constantValue->integerValue();
        if (intElementCount < 1) {
            typeError(call.span(), "array count must be greater than 0"_s);
            return;
        }

        if (intElementCount > std::numeric_limits<uint16_t>::max()) {
            typeError(call.span(), "array count ("_s, intElementCount, ") must be less than 65536"_s);
            return;
        }
        elementCount = static_cast<unsigned>(intElementCount);

        unsigned numberOfArguments = call.arguments().size();
        if (numberOfArguments && numberOfArguments != elementCount) {
            auto errorKind = call.arguments().size() < elementCount ? "few"_s : "many"_s;
            typeError(call.span(), "array constructor has too "_s, errorKind, " elements: expected "_s, elementCount, ", found "_s, call.arguments().size());
            return;
        }
        for (auto& argument : call.arguments()) {
            auto* argumentType = infer(argument, m_evaluation);
            if (!unify(elementType, argumentType)) {
                typeError(argument.span(), '\'', *argumentType, "' cannot be used to construct an array of '"_s, *elementType, '\'');
                return;
            }
            argument.m_inferredType = elementType;
        }
    } else {
        ASSERT(!array.maybeElementCount());
        elementCount = call.arguments().size();
        if (!elementCount) {
            typeError(call.span(), "cannot infer array element type from constructor"_s);
            return;
        }
        for (auto& argument : call.arguments()) {
            auto* argumentType = infer(argument, m_evaluation);
            if (auto* reference = std::get_if<Types::Reference>(argumentType))
                argumentType = reference->element;

            if (!elementType) {
                elementType = argumentType;

                if (!elementType->isConstructible()) {
                    typeError(array.span(), '\'', *elementType, "' cannot be used as an element type of an array"_s);
                    return;
                }

                continue;
            }
            if (unify(elementType, argumentType))
                continue;
            if (unify(argumentType, elementType)) {
                elementType = argumentType;
                continue;
            }
            typeError(argument.span(), "cannot infer common array element type from constructor arguments"_s);
            return;
        }
        for (auto& argument : call.arguments())
            argument.m_inferredType = elementType;
    }

    call.m_isConstructor = true;
    auto* result = m_types.arrayType(elementType, { elementCount });
    inferred(result);

    unsigned argumentCount = call.arguments().size();
    FixedVector<ConstantValue> arguments(argumentCount);
    bool isConstant = true;
    for (unsigned i = 0; i < argumentCount; ++i) {
        auto& argument = call.arguments()[i];
        auto& value = argument.m_constantValue;
        if (!value.has_value() || !convertValue(argument.span(), argument.inferredType(), value))
            isConstant = false;
        else
            arguments[i] = *value;
    }
    if (isConstant) {
        if (argumentCount) {
            // https://www.w3.org/TR/WGSL/#limits
            constexpr unsigned maximumConstantArraySize = 2047;
            if (UNLIKELY(argumentCount > maximumConstantArraySize))
                typeError(InferBottom::No, call.span(), "constant array cannot have more than "_s, String::number(maximumConstantArraySize), " elements"_s);
            setConstantValue(call, result, ConstantArray(WTFMove(arguments)));
        } else
            setConstantValue(call, result, zeroValue(result));
    }
}

void TypeChecker::bitcast(AST::CallExpression& call, const Vector<const Type*>& typeArguments)
{
    if (call.arguments().size() != 1) {
        typeError(call.span(), "bitcast expects a single argument, found "_s, call.arguments().size());
        return;
    }

    if (typeArguments.size() != 1) {
        typeError(call.span(), "bitcast expects a single template argument, found "_s, typeArguments.size());
        return;
    }

    if (UNLIKELY(m_discardResult == DiscardResult::Yes)) {
        typeError(call.span(), "cannot discard the result of bitcast"_s);
        return;
    }

    auto& argument = call.arguments()[0];
    auto* sourceType = infer(argument, m_evaluation);
    auto* destinationType = typeArguments[0];

    if (isBottom(sourceType) || isBottom(destinationType)) {
        inferred(m_types.bottomType());
        return;
    }

    if (auto* reference = std::get_if<Types::Reference>(sourceType))
        sourceType = reference->element;

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
            else
                setConstantValue(call, destinationType, WTFMove(*result));
        }
        inferred(destinationType);
        return;
    }

    auto* concreteType = concretize(sourceType, m_types) ?: sourceType;
    typeError(call.span(), "cannot bitcast from '"_s, *concreteType, "' to '"_s, *destinationType, '\'');
}

void TypeChecker::visit(AST::UnaryExpression& unary)
{
    if (unary.operation() == AST::UnaryOperation::AddressOf) {
        auto* type = infer(unary.expression(), Evaluation::Runtime);
        auto* reference = std::get_if<Types::Reference>(type);
        if (!reference) {
            typeError(unary.span(), "cannot take address of expression"_s);
            return;
        }

        if (reference->addressSpace == AddressSpace::Handle) {
            typeError(unary.span(), "cannot take the address of expression in handle address space"_s);
            return;
        }

        if (reference->isVectorComponent) {
            typeError(unary.span(), "cannot take the address of a vector component"_s);
            return;
        }

        inferred(m_types.pointerType(reference->addressSpace, reference->element, reference->accessMode));
        return;
    }

    if (unary.operation() == AST::UnaryOperation::Dereference) {
        auto* type = infer(unary.expression(), Evaluation::Runtime);
        auto* pointer = std::get_if<Types::Pointer>(type);
        if (!pointer) {
            typeError(unary.span(), "cannot dereference expression of type '"_s, *type, '\'');
            return;
        }

        inferred(m_types.referenceType(pointer->addressSpace, pointer->element, pointer->accessMode));
        return;
    }
    chooseOverload("operator"_s, unary.span(), &unary, toASCIILiteral(unary.operation()), ReferenceWrapperVector<AST::Expression, 1> { unary.expression() }, { });
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
    if (!array.maybeElementType()) {
        typeError(array.span(), "'array' requires at least 1 template argument"_s);
        return;
    }

    auto* elementType = resolve(*array.maybeElementType());
    if (isBottom(elementType)) {
        inferred(m_types.bottomType());
        return;
    }

    if (!elementType->hasCreationFixedFootprint()) {
        typeError(array.span(), '\'', *elementType, "' cannot be used as an element type of an array"_s);
        return;
    }

    Types::Array::Size size;
    if (array.maybeElementCount()) {
        auto elementCountType = infer(*array.maybeElementCount(), Evaluation::Override);
        if (!unify(m_types.i32Type(), elementCountType) && !unify(m_types.u32Type(), elementCountType)) {
            typeError(array.span(), "array count must be an i32 or u32 value, found '"_s, *elementCountType, '\'');
            return;
        }

        auto value = array.maybeElementCount()->constantValue();
        if (value.has_value()) {
            int64_t elementCount = 0;
            if (convertValue(array.maybeElementCount()->span(), concretize(elementCountType, m_types), value)) {
                elementCount = value->integerValue();
                if (elementCount < 1) {
                    typeError(array.span(), "array count must be greater than 0"_s);
                    return;
                }
            }
            size = { static_cast<unsigned>(elementCount) };
        } else {
            m_shaderModule.addOverrideValidation(*array.maybeElementCount(), [&](const ConstantValue& elementCount) -> std::optional<String> {
                if (elementCount.integerValue() < 1)
                    return { "array count must be greater than 0"_s };
                return std::nullopt;
            });
            size = { array.maybeElementCount() };
        }
    }

    inferred(m_types.arrayType(elementType, size));
}

const Type* TypeChecker::lookupType(const AST::Identifier& name)
{
    auto* binding = readVariable(name);
    if (!binding) {
        typeError(InferBottom::No, name.span(), "unresolved type '"_s, name, '\'');
        return m_types.bottomType();
    }

    if (binding->kind != Binding::Type) {
        typeError(InferBottom::No, name.span(), "cannot use "_s, bindingKindToString(binding->kind), " '"_s, name, "' as type"_s);
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
        typeError(type.span(), "type '"_s, *base, "' does not take template arguments"_s);
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
        Base::visit(statement);

    if (auto* breakIf = continuing.breakIf) {
        auto* type = infer(*breakIf, Evaluation::Runtime);
        if (!unify(m_types.boolType(), type))
            typeError(InferBottom::No, breakIf->span(), "expected 'bool', found "_s, *type);
    }
}

void TypeChecker::visitAttributes(AST::Attribute::List& attributes)
{
    for (auto& attribute : attributes)
        Base::visit(attribute);
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
            typeError(access.span(), "invalid vector swizzle character"_s);
            return nullptr;
        }
    }

    if (!isValid || (hasRGBA && hasXYZW)) {
        typeError(access.span(), "invalid vector swizzle member"_s);
        return nullptr;
    }

    const auto& constAccess = [&](const ConstantVector& vector, char field) -> ConstantValue {
        switch (field) {
        case 'r':
        case 'x':
            return vector.elements[0];
        case 'g':
        case 'y':
            return vector.elements[1];
        case 'b':
        case 'z':
            return vector.elements[2];
        case 'a':
        case 'w':
            return vector.elements[3];
        default:
            RELEASE_ASSERT_NOT_REACHED();
        };
    };

    const auto& constantValue = access.base().constantValue();

    switch (length) {
    case 1:
        if (constantValue)
            access.setConstantValue(constAccess(std::get<ConstantVector>(*constantValue), fieldName[0]));
        return vector.element;
    case 2:
    case 3:
    case 4:
        break;
    default:
        typeError(access.span(), "invalid vector swizzle size"_s);
        return nullptr;
    }

    if (constantValue) {
        const auto& vector = std::get<ConstantVector>(*constantValue);
        ConstantVector result(length);
        for (unsigned i = 0; i < length; ++i)
            result.elements[i] = constAccess(vector, fieldName[i]);
        access.setConstantValue(result);
    }
    return m_types.vectorType(length, vector.element);
}

template<typename CallArguments>
const Type* TypeChecker::chooseOverload(ASCIILiteral kind, const SourceSpan& span, AST::Expression* expression, const String& target, CallArguments&& callArguments, const Vector<const Type*>& typeArguments)
{
    auto it = m_overloadedOperations.find(target);
    if (it == m_overloadedOperations.end())
        return nullptr;

    Vector<const Type*> valueArguments;
    valueArguments.reserveInitialCapacity(callArguments.size());
    for (unsigned i = 0; i < callArguments.size(); ++i) {
        auto* type = infer(callArguments[i], m_evaluation);
        if (isBottom(type)) {
            inferred(m_types.bottomType());
            return m_types.bottomType();
        }
        valueArguments.append(type);
    }

    auto overload = resolveOverloads(m_types, it->value.overloads, valueArguments, typeArguments);
    if (overload.has_value()) {
        ASSERT(overload->parameters.size() == callArguments.size());
        if (m_discardResult == DiscardResult::Yes && it->value.mustUse)
            typeError(InferBottom::No, span, "ignoring return value of builtin '"_s, target, '\'');

        for (unsigned i = 0; i < callArguments.size(); ++i)
            callArguments[i].m_inferredType = overload->parameters[i];
        inferred(overload->result);

        if (expression && is<AST::CallExpression>(*expression)) {
            auto& call = uncheckedDowncast<AST::CallExpression>(*expression);
            call.m_isConstructor = it->value.kind == OverloadedDeclaration::Constructor;
            call.m_visibility = it->value.visibility;
        }

        unsigned argumentCount = callArguments.size();
        FixedVector<ConstantValue> arguments(argumentCount);
        bool isConstant = true;
        for (unsigned i = 0; i < argumentCount; ++i) {
            auto& argument = callArguments[i];
            auto& value = argument.m_constantValue;
            if (!value.has_value() || !convertValue(argument.span(), argument.inferredType(), value))
                isConstant = false;
            else
                arguments[i] = *value;
        }

        auto constantFunction = it->value.constantFunction;
        if (!constantFunction && m_evaluation < Evaluation::Runtime) {
            typeError(InferBottom::No, span, "cannot call function from "_s, evaluationToString(m_evaluation), " context"_s);
            return m_types.bottomType();
        }

        if (isConstant && constantFunction) {
            auto result = constantFunction(overload->result, WTFMove(arguments));
            if (!result)
                typeError(InferBottom::No, span, result.error());
            else if (expression)
                setConstantValue(*expression, overload->result, WTFMove(*result));
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
    typeError(span, "no matching overload for "_s, kind, ' ', target, typeArgumentsStream.toString(), '(', valueArgumentsStream.toString(), ')');
    return m_types.bottomType();
}

const Type* TypeChecker::infer(AST::Expression& expression, Evaluation evaluation, DiscardResult discardResult)
{
    auto discardResultScope = SetForScope(m_discardResult, discardResult);
    auto evaluationScope = SetForScope(m_evaluation, evaluation);

    ASSERT(!m_inferredType);
    Base::visit(expression);
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

Behaviors TypeChecker::analyze(AST::Statement& statement)
{
    switch (statement.kind()) {
    case AST::NodeKind::AssignmentStatement:
    case AST::NodeKind::CallStatement:
    case AST::NodeKind::CompoundAssignmentStatement:
    case AST::NodeKind::ConstAssertStatement:
    case AST::NodeKind::DecrementIncrementStatement:
    case AST::NodeKind::DiscardStatement:
    case AST::NodeKind::PhonyAssignmentStatement:
    case AST::NodeKind::StaticAssertStatement:
    case AST::NodeKind::VariableStatement:
        return Behavior::Next;
    case AST::NodeKind::BreakStatement:
        if (m_breakTargetStack.isEmpty())
            typeError(InferBottom::No, statement.span(), "break statement must be in a loop or switch case"_s);
        else if (std::holds_alternative<AST::Continuing*>(m_breakTargetStack.last()))
            typeError(InferBottom::No, statement.span(), "`break` must not be used to exit from a continuing block. Use `break-if` instead"_s);
        return Behavior::Break;
    case AST::NodeKind::ReturnStatement:
        if (m_breakTargetStack.containsIf([&](auto& it) { return std::holds_alternative<AST::Continuing*>(it); }))
            typeError(InferBottom::No, statement.span(), "continuing blocks must not contain a return statement"_s);
        return Behavior::Return;
    case AST::NodeKind::ContinueStatement: {
        bool hasLoopTarget = false;
        for (int i = m_breakTargetStack.size() - 1; i >= 0; --i) {
            auto& target = m_breakTargetStack[i];
            if (std::holds_alternative<AST::SwitchStatement*>(target))
                continue;

            hasLoopTarget = true;

            if (std::holds_alternative<AST::Continuing*>(target)) {
                typeError(InferBottom::No, statement.span(), "continuing blocks must not contain a continue statement"_s);
                break;
            }

            if (auto** loop = std::get_if<AST::LoopStatement*>(&target)) {
                if ((*loop)->continuing().has_value()) {
                    (*loop)->setContainsSwitch();
                    auto& continueStatement = downcast<AST::ContinueStatement>(statement);
                    continueStatement.setIsFromSwitchToContinuing();
                    for (size_t j = i + 1; j < m_breakTargetStack.size(); ++j) {
                        auto* switchStatement = std::get<AST::SwitchStatement*>(m_breakTargetStack[j]);
                        if (j == static_cast<size_t>(i + 1))
                            switchStatement->setIsInsideLoop();
                        else
                            switchStatement->setIsNestedInsideLoop();
                    }
                }
                break;
            }

            ASSERT(std::holds_alternative<AST::ForStatement*>(target) || std::holds_alternative<AST::WhileStatement*>(target));
            break;

        }

        if (!hasLoopTarget) {
            typeError(InferBottom::No, statement.span(), "continue statement must be in a loop"_s);
            return Behavior::Next;
        }
        return Behavior::Continue;
    }
    case AST::NodeKind::CompoundStatement:
        return analyze(uncheckedDowncast<AST::CompoundStatement>(statement));
    case AST::NodeKind::ForStatement:
        return analyze(uncheckedDowncast<AST::ForStatement>(statement));
    case AST::NodeKind::IfStatement:
        return analyze(uncheckedDowncast<AST::IfStatement>(statement));
    case AST::NodeKind::LoopStatement:
        return analyze(uncheckedDowncast<AST::LoopStatement>(statement));
    case AST::NodeKind::SwitchStatement:
        return analyze(uncheckedDowncast<AST::SwitchStatement>(statement));
    case AST::NodeKind::WhileStatement:
        return analyze(uncheckedDowncast<AST::WhileStatement>(statement));
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

Behaviors TypeChecker::analyze(AST::CompoundStatement& statement)
{
    return analyzeStatements(statement.statements());
}

Behaviors TypeChecker::analyze(AST::ForStatement& statement)
{
    auto behaviors = Behaviors();
    if (statement.maybeTest())
        behaviors.add({ Behavior::Next, Behavior::Break });

    m_breakTargetStack.append(&statement);
    behaviors.add(analyze(statement.body()));
    m_breakTargetStack.removeLast();

    if (behaviors.contains(Behavior::Break)) {
        behaviors.remove({ Behavior::Break, Behavior::Continue });
        behaviors.add(Behavior::Next);
    } else
        behaviors.remove({ Behavior::Next, Behavior::Continue });

    if (behaviors.isEmpty())
        typeError(InferBottom::No, statement.span(), "for-loop does not exit"_s);

    return behaviors;
}

Behaviors TypeChecker::analyze(AST::IfStatement& statement)
{
    auto behaviors = analyze(statement.trueBody());
    if (auto* elseBody = statement.maybeFalseBody())
        behaviors.add(analyze(*elseBody));
    else
        behaviors.add(Behavior::Next);
    return behaviors;
}

Behaviors TypeChecker::analyze(AST::LoopStatement& statement)
{
    m_breakTargetStack.append(&statement);
    auto behaviors = analyzeStatements(statement.body());
    if (auto& continuing = statement.continuing()) {
        m_breakTargetStack.append(&continuing.value());
        behaviors.add(analyzeStatements(continuing->body));
        m_breakTargetStack.removeLast();
        if (auto* breakIf = continuing->breakIf)
            behaviors.add({ Behavior::Break, Behavior::Continue });
    }
    m_breakTargetStack.removeLast();
    if (behaviors.contains(Behavior::Break)) {
        behaviors.remove({ Behavior::Break, Behavior::Continue });
        behaviors.add(Behavior::Next);
    } else
        behaviors.remove({ Behavior::Next, Behavior::Continue });

    if (behaviors.isEmpty())
        typeError(InferBottom::No, statement.span(), "loop does not exit"_s);

    return behaviors;
}

Behaviors TypeChecker::analyze(AST::SwitchStatement& statement)
{
    m_breakTargetStack.append(&statement);
    auto behaviors = analyze(statement.defaultClause().body);
    for (auto& clause : statement.clauses())
        behaviors.add(analyze(clause.body));
    m_breakTargetStack.removeLast();

    if (behaviors.contains(Behavior::Break)) {
        behaviors.remove(Behavior::Break);
        behaviors.add(Behavior::Next);
    }
    return behaviors;
}

Behaviors TypeChecker::analyze(AST::WhileStatement& statement)
{
    auto behaviors = Behaviors({ Behavior::Next, Behavior::Break });
    m_breakTargetStack.append(&statement);
    behaviors.add(analyze(statement.body()));
    m_breakTargetStack.removeLast();
    behaviors.remove({ Behavior::Break, Behavior::Continue });
    return behaviors;
}

Behaviors TypeChecker::analyzeStatements(AST::Statement::List& statements)
{
    auto behaviors = Behaviors(Behavior::Next);
    for (auto& statement : statements) {
        auto behavior = analyze(statement);
        if (behaviors.contains(Behavior::Next)) {
            behaviors.remove(Behavior::Next);
            behaviors.add(behavior);
        }
    }
    return behaviors;
}

const Type* TypeChecker::check(AST::Expression& expression, Constraint constraint, Evaluation evaluation)
{
    auto* type = infer(expression, evaluation);
    type = satisfyOrPromote(type, constraint, m_types);
    if (!type)
        return nullptr;
    convertValue(expression.span(), type, expression.m_constantValue);
    return type;
}

const Type* TypeChecker::resolve(AST::Expression& type)
{
    ASSERT(!m_inferredType);
    if (auto* identifierExpression = dynamicDowncast<AST::IdentifierExpression>(type))
        inferred(lookupType(identifierExpression->identifier()));
    else
        Base::visit(type);
    ASSERT(m_inferredType);

    if (std::holds_alternative<Types::TypeConstructor>(*m_inferredType)) {
        typeError(InferBottom::No, type.span(), "type '"_s, *m_inferredType, "' requires template arguments"_s);
        m_inferredType = m_types.bottomType();
    }

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
    ASSERT(type);
    if (!introduceVariable(name, { Binding::Type, type, Evaluation::Runtime, std::nullopt }))
        typeError(InferBottom::No, name.span(), "redeclaration of '"_s, name, '\'');
}

bool TypeChecker::convertValue(const SourceSpan& span, const Type* type, std::optional<ConstantValue>& value)
{
    if (!value)
        return true;

    if (isBottom(type)) {
        value = std::nullopt;
        return false;
    }

    if (UNLIKELY(!convertValueImpl(span, type, *value))) {
        StringPrintStream valueString;
        value->dump(valueString);
        typeError(InferBottom::No, span, "value "_s, valueString.toString(), " cannot be represented as '"_s, *type, '\'');

        value = std::nullopt;

        return false;
    }

    return true;
}

bool TypeChecker::convertValueImpl(const SourceSpan& span, const Type* type, ConstantValue& value)
{
    if (shouldDumpConstantValues) {
        StringPrintStream valueString;
        value.dump(valueString);
        dataLogLn("converting value ", valueString.toString(), " to '", *type, "'");
    }

    return WTF::switchOn(*type,
        [&](const Types::Primitive& primitive) -> bool {
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
                    return false;
                value = { *result };
                return true;
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
                    return false;
                value = { *result };
                return true;
            }
            case Types::Primitive::I32: {
                if (std::holds_alternative<int32_t>(value))
                    return true;
                std::optional<int32_t> result;
                if (auto* abstractInt = std::get_if<int64_t>(&value))
                    result = convertInteger<int32_t>(*abstractInt);

                if (!result.has_value())
                    return false;
                value = { *result };
                return true;
            }
            case Types::Primitive::U32: {
                if (std::holds_alternative<uint32_t>(value))
                    return true;
                std::optional<uint32_t> result;
                if (auto* abstractInt = std::get_if<int64_t>(&value))
                    result = convertInteger<uint32_t>(*abstractInt);

                if (!result.has_value())
                    return false;
                value = { *result };
                return true;
            }
            case Types::Primitive::AbstractInt:
                RELEASE_ASSERT(std::holds_alternative<int64_t>(value));
                return true;
            case Types::Primitive::AbstractFloat: {
                std::optional<double> result;
                if (auto* abstractFloat = std::get_if<double>(&value))
                    result = convertFloat<double>(*abstractFloat);
                else if (auto* abstractInt = std::get_if<int64_t>(&value))
                    result = convertFloat<double>(static_cast<double>(*abstractInt));
                else
                    RELEASE_ASSERT_NOT_REACHED();
                if (!result.has_value())
                    return false;
                value = { *result };
                return true;
            }
            case Types::Primitive::Bool:
                RELEASE_ASSERT(std::holds_alternative<bool>(value));
                return true;
            case Types::Primitive::Void:
            case Types::Primitive::Sampler:
            case Types::Primitive::SamplerComparison:
            case Types::Primitive::TextureExternal:
            case Types::Primitive::AccessMode:
            case Types::Primitive::TexelFormat:
            case Types::Primitive::AddressSpace:
                return false;
            }
        },
        [&](const Types::Vector& vectorType) -> bool {
            ASSERT(value.isVector());
            auto& vector = std::get<ConstantVector>(value);
            for (auto& element : vector.elements) {
                if (!convertValueImpl(span, vectorType.element, element))
                    return false;
            }
            return true;
        },
        [&](const Types::Matrix& matrixType) -> bool {
            ASSERT(value.isMatrix());
            auto& matrix = std::get<ConstantMatrix>(value);
            for (auto& element : matrix.elements) {
                if (!convertValueImpl(span, matrixType.element, element))
                    return false;
            }
            return true;
        },
        [&](const Types::Array& arrayType) -> bool {
            ASSERT(value.isArray());
            auto& array = std::get<ConstantArray>(value);
            for (auto& element : array.elements) {
                if (!convertValueImpl(span, arrayType.element, element))
                    return false;
            }
            return true;
        },
        [&](const Types::Struct& structType) -> bool {
            auto& constantStruct = std::get<ConstantStruct>(value);
            for (auto& [key, type] : structType.fields) {
                auto it = constantStruct.fields.find(key);
                RELEASE_ASSERT(it != constantStruct.fields.end());
                if (!convertValueImpl(span, type, it->value))
                    return false;
            }
            return true;
        },
        [&](const Types::PrimitiveStruct& primitiveStruct) -> bool {
            auto& constantStruct = std::get<ConstantStruct>(value);
            const auto& keys = Types::PrimitiveStruct::keys[primitiveStruct.kind];
            for (auto& entry : constantStruct.fields) {
                auto* key = keys.tryGet(entry.key);
                RELEASE_ASSERT(key);
                auto* type = primitiveStruct.values[*key];
                if (!convertValueImpl(span, type, entry.value))
                    return false;
            }
            return true;
        },
        [&](const Types::Function&) -> bool {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Texture&) -> bool {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::TextureStorage&) -> bool {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::TextureDepth&) -> bool {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Reference&) -> bool {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Pointer&) -> bool {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Atomic&) -> bool {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::TypeConstructor&) -> bool {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Bottom&) -> bool {
            RELEASE_ASSERT_NOT_REACHED();
        });
}

void TypeChecker::introduceValue(const AST::Identifier& name, const Type* type, Evaluation evaluation, std::optional<ConstantValue> value)
{
    ASSERT(type);
    if (shouldDumpConstantValues && value.has_value())
        dataLogLn("> Assigning value: ", name, " => ", value);
    if (!introduceVariable(name, { Binding::Value, type, evaluation, value }))
        typeError(InferBottom::No, name.span(), "redeclaration of '"_s, name, '\'');
}

void TypeChecker::introduceFunction(const AST::Identifier& name, const Type* type)
{
    ASSERT(type);
    if (!introduceVariable(name, { Binding::Function, type, Evaluation::Runtime , std::nullopt }))
        typeError(InferBottom::No, name.span(), "redeclaration of '"_s, name, '\'');
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

template<typename TargetConstructor, typename Validator, typename... Arguments>
void TypeChecker::allocateSimpleConstructor(ASCIILiteral name, TargetConstructor constructor, const Validator& validate, Arguments&&... arguments)
{
    introduceType(AST::Identifier::make(name), m_types.typeConstructorType(
        name,
        [this, constructor, &validate, arguments...](AST::ElaboratedTypeExpression& type) -> const Type* {
            if (type.arguments().size() != 1) {
                typeError(InferBottom::No, type.span(), '\'', type.base(), "' requires 1 template argument"_s);
                return m_types.bottomType();
            }
            auto* elementType = resolve(type.arguments().first());
            if (isBottom(elementType))
                return m_types.bottomType();

            if (auto error = validate(elementType)) {
                typeError(InferBottom::No, type.span(), *error);
                return m_types.bottomType();
            }

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
                typeError(InferBottom::No, type.span(), '\'', type.base(), "' requires 2 template argument"_s);
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
    auto* formatType = infer(expression, Evaluation::Runtime);
    if (isBottom(formatType))
        return std::nullopt;

    if (!unify(formatType, m_types.texelFormatType())) {
        typeError(InferBottom::No, expression.span(), "cannot use '"_s, *formatType, "' as texel format"_s);
        return std::nullopt;
    }

    auto& formatName = downcast<AST::IdentifierExpression>(expression).identifier();

    auto* format = parseTexelFormat(formatName.id());
    ASSERT(format);
    return { *format };
}

std::optional<AccessMode> TypeChecker::accessMode(AST::Expression& expression)
{
    auto* accessType = infer(expression, Evaluation::Runtime);
    if (isBottom(accessType))
        return std::nullopt;

    if (!unify(accessType, m_types.accessModeType())) {
        typeError(InferBottom::No, expression.span(), "cannot use '"_s, *accessType, "' as access mode"_s);
        return std::nullopt;
    }

    auto& accessName = downcast<AST::IdentifierExpression>(expression).identifier();

    auto* accessMode = parseAccessMode(accessName.id());
    ASSERT(accessMode);
    return { *accessMode };
}

std::optional<AddressSpace> TypeChecker::addressSpace(AST::Expression& expression)
{
    auto* addressSpaceType = infer(expression, Evaluation::Runtime);
    if (isBottom(addressSpaceType))
        return std::nullopt;

    if (!unify(addressSpaceType, m_types.addressSpaceType())) {
        typeError(InferBottom::No, expression.span(), "cannot use '"_s, *addressSpaceType, "' as address space"_s);
        return std::nullopt;
    }

    auto& addressSpaceName = downcast<AST::IdentifierExpression>(expression).identifier();

    auto* addressSpace = parseAddressSpace(addressSpaceName.id());
    ASSERT(addressSpace);
    return { *addressSpace };
}

template<typename Node>
void TypeChecker::setConstantValue(Node& expression, const Type* type, const ConstantValue& value)
{
    using namespace Types;

    if (shouldDumpConstantValues) {
        dataLog("> Setting constantValue for expression: ");
        dumpNode(WTF::dataFile(), expression);
        dataLogLn(" = ", value);
    }
    expression.setConstantValue(value);
    convertValue(expression.span(), type, expression.m_constantValue);
}

bool TypeChecker::isModuleScope() const
{
    return !m_returnType;
}


} // namespace WGSL
