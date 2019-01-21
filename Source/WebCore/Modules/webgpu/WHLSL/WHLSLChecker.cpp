/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WHLSLChecker.h"

#if ENABLE(WEBGPU)

#include "WHLSLArrayReferenceType.h"
#include "WHLSLArrayType.h"
#include "WHLSLAssignmentExpression.h"
#include "WHLSLCallExpression.h"
#include "WHLSLCommaExpression.h"
#include "WHLSLDereferenceExpression.h"
#include "WHLSLDoWhileLoop.h"
#include "WHLSLDotExpression.h"
#include "WHLSLForLoop.h"
#include "WHLSLGatherEntryPointItems.h"
#include "WHLSLIfStatement.h"
#include "WHLSLIndexExpression.h"
#include "WHLSLInferTypes.h"
#include "WHLSLLogicalExpression.h"
#include "WHLSLLogicalNotExpression.h"
#include "WHLSLMakeArrayReferenceExpression.h"
#include "WHLSLMakePointerExpression.h"
#include "WHLSLPointerType.h"
#include "WHLSLProgram.h"
#include "WHLSLReadModifyWriteExpression.h"
#include "WHLSLResolvableType.h"
#include "WHLSLResolveOverloadImpl.h"
#include "WHLSLResolvingType.h"
#include "WHLSLReturn.h"
#include "WHLSLSwitchStatement.h"
#include "WHLSLTernaryExpression.h"
#include "WHLSLVisitor.h"
#include "WHLSLWhileLoop.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Ref.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace WHLSL {

class PODChecker : public Visitor {
public:
    PODChecker() = default;

    virtual ~PODChecker() = default;

    void visit(AST::EnumerationDefinition& enumerationDefinition) override
    {
        Visitor::visit(enumerationDefinition);
    }

    void visit(AST::NativeTypeDeclaration& nativeTypeDeclaration) override
    {
        if (!nativeTypeDeclaration.isNumber()
            && !nativeTypeDeclaration.isVector()
            && !nativeTypeDeclaration.isMatrix())
            setError();
    }

    void visit(AST::StructureDefinition& structureDefinition) override
    {
        Visitor::visit(structureDefinition);
    }

    void visit(AST::TypeDefinition& typeDefinition) override
    {
        Visitor::visit(typeDefinition);
    }

    void visit(AST::ArrayType& arrayType) override
    {
        Visitor::visit(arrayType);
    }

    void visit(AST::PointerType&) override
    {
        setError();
    }

    void visit(AST::ArrayReferenceType&) override
    {
        setError();
    }

    void visit(AST::TypeReference& typeReference) override
    {
        ASSERT(typeReference.resolvedType());
        checkErrorAndVisit(*typeReference.resolvedType());
    }
};

static AST::NativeFunctionDeclaration resolveWithOperatorAnderIndexer(AST::CallExpression& callExpression, AST::ArrayReferenceType& firstArgument, const Intrinsics& intrinsics)
{
    const bool isOperator = true;
    auto returnType = makeUniqueRef<AST::PointerType>(Lexer::Token(callExpression.origin()), firstArgument.addressSpace(), firstArgument.elementType().clone());
    AST::VariableDeclarations parameters;
    parameters.append(AST::VariableDeclaration(Lexer::Token(callExpression.origin()), AST::Qualifiers(), { firstArgument.clone() }, String(), WTF::nullopt, WTF::nullopt));
    parameters.append(AST::VariableDeclaration(Lexer::Token(callExpression.origin()), AST::Qualifiers(), { AST::TypeReference::wrap(Lexer::Token(callExpression.origin()), intrinsics.uintType()) }, String(), WTF::nullopt, WTF::nullopt));
    return AST::NativeFunctionDeclaration(AST::FunctionDeclaration(Lexer::Token(callExpression.origin()), AST::AttributeBlock(), WTF::nullopt, WTFMove(returnType), String("operator&[]", String::ConstructFromLiteral), WTFMove(parameters), WTF::nullopt, isOperator));
}

static AST::NativeFunctionDeclaration resolveWithOperatorLength(AST::CallExpression& callExpression, AST::UnnamedType& firstArgument, const Intrinsics& intrinsics)
{
    const bool isOperator = true;
    auto returnType = AST::TypeReference::wrap(Lexer::Token(callExpression.origin()), intrinsics.uintType());
    AST::VariableDeclarations parameters;
    parameters.append(AST::VariableDeclaration(Lexer::Token(callExpression.origin()), AST::Qualifiers(), { firstArgument.clone() }, String(), WTF::nullopt, WTF::nullopt));
    return AST::NativeFunctionDeclaration(AST::FunctionDeclaration(Lexer::Token(callExpression.origin()), AST::AttributeBlock(), WTF::nullopt, WTFMove(returnType), String("operator.length", String::ConstructFromLiteral), WTFMove(parameters), WTF::nullopt, isOperator));
}

static AST::NativeFunctionDeclaration resolveWithReferenceComparator(AST::CallExpression& callExpression, ResolvingType& firstArgument, ResolvingType& secondArgument, const Intrinsics& intrinsics)
{
    const bool isOperator = true;
    auto returnType = AST::TypeReference::wrap(Lexer::Token(callExpression.origin()), intrinsics.boolType());
    auto argumentType = WTF::visit(WTF::makeVisitor([](UniqueRef<AST::UnnamedType>& unnamedType) -> UniqueRef<AST::UnnamedType> {
        return unnamedType->clone();
    }, [&](Ref<ResolvableTypeReference>&) -> UniqueRef<AST::UnnamedType> {
        return WTF::visit(WTF::makeVisitor([](UniqueRef<AST::UnnamedType>& unnamedType) -> UniqueRef<AST::UnnamedType> {
            return unnamedType->clone();
        }, [&](Ref<ResolvableTypeReference>&) -> UniqueRef<AST::UnnamedType> {
            // We encountered "null == null".
            // The type isn't observable, so we can pick whatever we want.
            // FIXME: This can probably be generalized, using the "preferred type" infrastructure used by generic literals
            return AST::TypeReference::wrap(Lexer::Token(callExpression.origin()), intrinsics.intType());
        }), secondArgument);
    }), firstArgument);
    AST::VariableDeclarations parameters;
    parameters.append(AST::VariableDeclaration(Lexer::Token(callExpression.origin()), AST::Qualifiers(), { argumentType->clone() }, String(), WTF::nullopt, WTF::nullopt));
    parameters.append(AST::VariableDeclaration(Lexer::Token(callExpression.origin()), AST::Qualifiers(), { WTFMove(argumentType) }, String(), WTF::nullopt, WTF::nullopt));
    return AST::NativeFunctionDeclaration(AST::FunctionDeclaration(Lexer::Token(callExpression.origin()), AST::AttributeBlock(), WTF::nullopt, WTFMove(returnType), String("operator==", String::ConstructFromLiteral), WTFMove(parameters), WTF::nullopt, isOperator));
}

static Optional<AST::NativeFunctionDeclaration> resolveByInstantiation(AST::CallExpression& callExpression, const Vector<std::reference_wrapper<ResolvingType>>& types, const Intrinsics& intrinsics)
{
    if (callExpression.name() == "operator&[]" && types.size() == 2) {
        auto* firstArgumentArrayRef = WTF::visit(WTF::makeVisitor([](UniqueRef<AST::UnnamedType>& unnamedType) -> AST::ArrayReferenceType* {
            if (is<AST::ArrayReferenceType>(static_cast<AST::UnnamedType&>(unnamedType)))
                return &downcast<AST::ArrayReferenceType>(static_cast<AST::UnnamedType&>(unnamedType));
            return nullptr;
        }, [](Ref<ResolvableTypeReference>&) -> AST::ArrayReferenceType* {
            return nullptr;
        }), types[0].get());
        bool secondArgumentIsUint = WTF::visit(WTF::makeVisitor([&](UniqueRef<AST::UnnamedType>& unnamedType) -> bool {
            return matches(unnamedType, intrinsics.uintType());
        }, [&](Ref<ResolvableTypeReference>& resolvableTypeReference) -> bool {
            return resolvableTypeReference->resolvableType().canResolve(intrinsics.uintType());
        }), types[1].get());
        if (firstArgumentArrayRef && secondArgumentIsUint)
            return resolveWithOperatorAnderIndexer(callExpression, *firstArgumentArrayRef, intrinsics);
    } else if (callExpression.name() == "operator.length" && types.size() == 1) {
        auto* firstArgumentReference = WTF::visit(WTF::makeVisitor([](UniqueRef<AST::UnnamedType>& unnamedType) -> AST::UnnamedType* {
            if (is<AST::ArrayReferenceType>(static_cast<AST::UnnamedType&>(unnamedType)) || is<AST::ArrayType>(static_cast<AST::UnnamedType&>(unnamedType)))
                return &unnamedType;
            return nullptr;
        }, [](Ref<ResolvableTypeReference>&) -> AST::UnnamedType* {
            return nullptr;
        }), types[0].get());
        if (firstArgumentReference)
            return resolveWithOperatorLength(callExpression, *firstArgumentReference, intrinsics);
    } else if (callExpression.name() == "operator==" && types.size() == 2) {
        auto isAcceptable = [](ResolvingType& resolvingType) -> bool {
            return WTF::visit(WTF::makeVisitor([](UniqueRef<AST::UnnamedType>& unnamedType) -> bool {
                return is<AST::ReferenceType>(static_cast<AST::UnnamedType&>(unnamedType));
            }, [](Ref<ResolvableTypeReference>& resolvableTypeReference) -> bool {
                return is<AST::NullLiteralType>(resolvableTypeReference->resolvableType());
            }), resolvingType);
        };
        if (isAcceptable(types[0].get()) && isAcceptable(types[1].get()))
            return resolveWithReferenceComparator(callExpression, types[0].get(), types[1].get(), intrinsics);
    }
    return WTF::nullopt;
}

static bool checkSemantics(Vector<EntryPointItem>& inputItems, Vector<EntryPointItem>& outputItems, const Optional<AST::EntryPointType>& entryPointType, const Intrinsics& intrinsics)
{
    {
        auto checkDuplicateSemantics = [&](const Vector<EntryPointItem>& items) -> bool {
            for (size_t i = 0; i < items.size(); ++i) {
                for (size_t j = i + 1; j < items.size(); ++j) {
                    if (items[i].semantic == items[j].semantic)
                        return false;
                }
            }
            return true;
        };
        if (!checkDuplicateSemantics(inputItems))
            return false;
        if (!checkDuplicateSemantics(outputItems))
            return false;
    }

    {
        auto checkSemanticTypes = [&](const Vector<EntryPointItem>& items) -> bool {
            for (auto& item : items) {
                auto acceptable = WTF::visit(WTF::makeVisitor([&](const AST::BaseSemantic& semantic) -> bool {
                    return semantic.isAcceptableType(*item.unnamedType, intrinsics);
                }), *item.semantic);
                if (!acceptable)
                    return false;
            }
            return true;
        };
        if (!checkSemanticTypes(inputItems))
            return false;
        if (!checkSemanticTypes(outputItems))
            return false;
    }

    {
        auto checkSemanticForShaderType = [&](const Vector<EntryPointItem>& items, AST::BaseSemantic::ShaderItemDirection direction) -> bool {
            for (auto& item : items) {
                auto acceptable = WTF::visit(WTF::makeVisitor([&](const AST::BaseSemantic& semantic) -> bool {
                    return semantic.isAcceptableForShaderItemDirection(direction, entryPointType);
                }), *item.semantic);
                if (!acceptable)
                    return false;
            }
            return true;
        };
        if (!checkSemanticForShaderType(inputItems, AST::BaseSemantic::ShaderItemDirection::Input))
            return false;
        if (!checkSemanticForShaderType(outputItems, AST::BaseSemantic::ShaderItemDirection::Output))
            return false;
    }

    {
        auto checkPODData = [&](const Vector<EntryPointItem>& items) -> bool {
            for (auto& item : items) {
                PODChecker podChecker;
                if (is<AST::PointerType>(item.unnamedType))
                    podChecker.checkErrorAndVisit(downcast<AST::PointerType>(*item.unnamedType).elementType());
                else if (is<AST::ArrayReferenceType>(item.unnamedType))
                    podChecker.checkErrorAndVisit(downcast<AST::ArrayReferenceType>(*item.unnamedType).elementType());
                else if (is<AST::ArrayType>(item.unnamedType))
                    podChecker.checkErrorAndVisit(downcast<AST::ArrayType>(*item.unnamedType).type());
                else
                    continue;
                if (podChecker.error())
                    return false;
            }
            return true;
        };
        if (!checkPODData(inputItems))
            return false;
        if (!checkPODData(outputItems))
            return false;
    }

    return true;
}

static bool checkOperatorOverload(const AST::FunctionDefinition& functionDefinition, const Intrinsics& intrinsics, NameContext& nameContext)
{
    enum class CheckKind {
        Index,
        Dot
    };

    auto checkGetter = [&](CheckKind kind) -> bool {
        size_t numExpectedParameters = kind == CheckKind::Index ? 2 : 1;
        if (functionDefinition.parameters().size() != numExpectedParameters)
            return false;
        auto& firstParameterUnifyNode = (*functionDefinition.parameters()[0].type())->unifyNode();
        if (is<AST::UnnamedType>(firstParameterUnifyNode)) {
            auto& unnamedType = downcast<AST::UnnamedType>(firstParameterUnifyNode);
            if (is<AST::PointerType>(unnamedType) || is<AST::ArrayReferenceType>(unnamedType) || is<AST::ArrayType>(unnamedType))
                return false;
        }
        if (kind == CheckKind::Index) {
            auto& secondParameterUnifyNode = (*functionDefinition.parameters()[1].type())->unifyNode();
            if (!is<AST::NamedType>(secondParameterUnifyNode))
                return false;
            auto& namedType = downcast<AST::NamedType>(secondParameterUnifyNode);
            if (!is<AST::NativeTypeDeclaration>(namedType))
                return false;
            auto& nativeTypeDeclaration = downcast<AST::NativeTypeDeclaration>(namedType);
            if (!nativeTypeDeclaration.isInt())
                return false;
        }
        return true;
    };

    auto checkSetter = [&](CheckKind kind) -> bool {
        size_t numExpectedParameters = kind == CheckKind::Index ? 3 : 2;
        if (functionDefinition.parameters().size() != numExpectedParameters)
            return false;
        auto& firstArgumentUnifyNode = (*functionDefinition.parameters()[0].type())->unifyNode();
        if (is<AST::UnnamedType>(firstArgumentUnifyNode)) {
            auto& unnamedType = downcast<AST::UnnamedType>(firstArgumentUnifyNode);
            if (is<AST::PointerType>(unnamedType) || is<AST::ArrayReferenceType>(unnamedType) || is<AST::ArrayType>(unnamedType))
                return false;
        }
        if (kind == CheckKind::Index) {
            auto& secondParameterUnifyNode = (*functionDefinition.parameters()[1].type())->unifyNode();
            if (!is<AST::NamedType>(secondParameterUnifyNode))
                return false;
            auto& namedType = downcast<AST::NamedType>(secondParameterUnifyNode);
            if (!is<AST::NativeTypeDeclaration>(namedType))
                return false;
            auto& nativeTypeDeclaration = downcast<AST::NativeTypeDeclaration>(namedType);
            if (!nativeTypeDeclaration.isInt())
                return false;
        }
        if (!matches(functionDefinition.type(), *functionDefinition.parameters()[0].type()))
            return false;
        auto& valueType = *functionDefinition.parameters()[numExpectedParameters - 1].type();
        auto getterName = functionDefinition.name().substring(0, functionDefinition.name().length() - 1);
        auto* getterFuncs = nameContext.getFunctions(getterName);
        if (!getterFuncs)
            return false;
        Vector<ResolvingType> argumentTypes;
        Vector<std::reference_wrapper<ResolvingType>> argumentTypeReferences;
        for (size_t i = 0; i < numExpectedParameters - 1; ++i)
            argumentTypes.append((*functionDefinition.parameters()[0].type())->clone());
        for (auto& argumentType : argumentTypes)
            argumentTypeReferences.append(argumentType);
        Optional<std::reference_wrapper<AST::NamedType>> castReturnType;
        auto* overload = resolveFunctionOverloadImpl(*getterFuncs, argumentTypeReferences, castReturnType);
        if (!overload)
            return false;
        auto& resultType = overload->type();
        return matches(resultType, valueType);
    };

    auto checkAnder = [&](CheckKind kind) -> bool {
        size_t numExpectedParameters = kind == CheckKind::Index ? 2 : 1;
        if (functionDefinition.parameters().size() != numExpectedParameters)
            return false;
        {
            auto& unifyNode = functionDefinition.type().unifyNode();
            if (!is<AST::UnnamedType>(unifyNode))
                return false;
            auto& unnamedType = downcast<AST::UnnamedType>(unifyNode);
            if (!is<AST::PointerType>(unnamedType))
                return false;
        }
        {
            auto& unifyNode = (*functionDefinition.parameters()[0].type())->unifyNode();
            if (!is<AST::UnnamedType>(unifyNode))
                return false;
            auto& unnamedType = downcast<AST::UnnamedType>(unifyNode);
            return is<AST::PointerType>(unnamedType) || is<AST::ArrayReferenceType>(unnamedType);
        }
    };

    if (!functionDefinition.isOperator())
        return true;
    if (functionDefinition.isCast())
        return true;
    if (functionDefinition.name() == "operator++" || functionDefinition.name() == "operator--") {
        return functionDefinition.parameters().size() == 1
            && matches(*functionDefinition.parameters()[0].type(), functionDefinition.type());
    }
    if (functionDefinition.name() == "operator+" || functionDefinition.name() == "operator-")
        return functionDefinition.parameters().size() == 1 || functionDefinition.parameters().size() == 2;
    if (functionDefinition.name() == "operator*"
        || functionDefinition.name() == "operator/"
        || functionDefinition.name() == "operator%"
        || functionDefinition.name() == "operator&"
        || functionDefinition.name() == "operator|"
        || functionDefinition.name() == "operator^"
        || functionDefinition.name() == "operator<<"
        || functionDefinition.name() == "opreator>>")
        return functionDefinition.parameters().size() == 2;
    if (functionDefinition.name() == "operator~")
        return functionDefinition.parameters().size() == 1;
    if (functionDefinition.name() == "operator=="
        || functionDefinition.name() == "operator<"
        || functionDefinition.name() == "operator<="
        || functionDefinition.name() == "operator>"
        || functionDefinition.name() == "operator>=") {
        return functionDefinition.parameters().size() == 2
            && matches(functionDefinition.type(), intrinsics.boolType());
    }
    if (functionDefinition.name() == "operator[]")
        return checkGetter(CheckKind::Index);
    if (functionDefinition.name() == "operator[]=")
        return checkSetter(CheckKind::Index);
    if (functionDefinition.name() == "operator&[]")
        return checkAnder(CheckKind::Index);
    if (functionDefinition.name().startsWith("operator.")) {
        if (functionDefinition.name().endsWith("="))
            return checkSetter(CheckKind::Dot);
        return checkGetter(CheckKind::Dot);
    }
    if (functionDefinition.name().startsWith("operator&."))
        return checkAnder(CheckKind::Dot);
    return false;
}

class Checker : public Visitor {
public:
    Checker(const Intrinsics& intrinsics, Program& program)
        : m_intrinsics(intrinsics)
        , m_program(program)
    {
    }

    ~Checker() = default;

    void visit(Program&) override;

    bool assignTypes();

private:
    bool checkShaderType(const AST::FunctionDefinition&);
    void finishVisitingPropertyAccess(AST::PropertyAccessExpression&, AST::UnnamedType& wrappedBaseType, AST::UnnamedType* extraArgumentType = nullptr);
    bool isBoolType(ResolvingType&);
    struct RecurseInfo {
        ResolvingType& resolvingType;
        Optional<AST::AddressSpace>& addressSpace;
    };
    Optional<RecurseInfo> recurseAndGetInfo(AST::Expression&, bool requiresLValue = false);
    Optional<RecurseInfo> getInfo(AST::Expression&, bool requiresLValue = false);
    Optional<UniqueRef<AST::UnnamedType>> recurseAndWrapBaseType(AST::PropertyAccessExpression&);
    bool recurseAndRequireBoolType(AST::Expression&);
    void assignType(AST::Expression&, UniqueRef<AST::UnnamedType>&&, Optional<AST::AddressSpace> = WTF::nullopt);
    void assignType(AST::Expression&, Ref<ResolvableTypeReference>&&, Optional<AST::AddressSpace> = WTF::nullopt);
    void forwardType(AST::Expression&, ResolvingType&, Optional<AST::AddressSpace> = WTF::nullopt);

    void visit(AST::FunctionDefinition&) override;
    void visit(AST::EnumerationDefinition&) override;
    void visit(AST::TypeReference&) override;
    void visit(AST::VariableDeclaration&) override;
    void visit(AST::AssignmentExpression&) override;
    void visit(AST::ReadModifyWriteExpression&) override;
    void visit(AST::DereferenceExpression&) override;
    void visit(AST::MakePointerExpression&) override;
    void visit(AST::MakeArrayReferenceExpression&) override;
    void visit(AST::DotExpression&) override;
    void visit(AST::IndexExpression&) override;
    void visit(AST::VariableReference&) override;
    void visit(AST::Return&) override;
    void visit(AST::PointerType&) override;
    void visit(AST::ArrayReferenceType&) override;
    void visit(AST::IntegerLiteral&) override;
    void visit(AST::UnsignedIntegerLiteral&) override;
    void visit(AST::FloatLiteral&) override;
    void visit(AST::NullLiteral&) override;
    void visit(AST::BooleanLiteral&) override;
    void visit(AST::EnumerationMemberLiteral&) override;
    void visit(AST::LogicalNotExpression&) override;
    void visit(AST::LogicalExpression&) override;
    void visit(AST::IfStatement&) override;
    void visit(AST::WhileLoop&) override;
    void visit(AST::DoWhileLoop&) override;
    void visit(AST::ForLoop&) override;
    void visit(AST::SwitchStatement&) override;
    void visit(AST::CommaExpression&) override;
    void visit(AST::TernaryExpression&) override;
    void visit(AST::CallExpression&) override;

    HashMap<AST::Expression*, ResolvingType> m_typeMap;
    HashMap<AST::Expression*, Optional<AST::AddressSpace>> m_addressSpaceMap;
    HashSet<String> m_vertexEntryPoints;
    HashSet<String> m_fragmentEntryPoints;
    HashSet<String> m_computeEntryPoints;
    const Intrinsics& m_intrinsics;
    Program& m_program;
};

void Checker::visit(Program& program)
{
    // These visiting functions might add new global statements, so don't use foreach syntax.
    for (size_t i = 0; i < program.typeDefinitions().size(); ++i)
        checkErrorAndVisit(program.typeDefinitions()[i]);
    for (size_t i = 0; i < program.structureDefinitions().size(); ++i)
        checkErrorAndVisit(program.structureDefinitions()[i]);
    for (size_t i = 0; i < program.enumerationDefinitions().size(); ++i)
        checkErrorAndVisit(program.enumerationDefinitions()[i]);
    for (size_t i = 0; i < program.nativeTypeDeclarations().size(); ++i)
        checkErrorAndVisit(program.nativeTypeDeclarations()[i]);

    for (size_t i = 0; i < program.functionDefinitions().size(); ++i)
        checkErrorAndVisit(program.functionDefinitions()[i]);
    for (size_t i = 0; i < program.nativeFunctionDeclarations().size(); ++i)
        checkErrorAndVisit(program.nativeFunctionDeclarations()[i]);
}

bool Checker::assignTypes()
{
    for (auto& keyValuePair : m_typeMap) {
        auto success = WTF::visit(WTF::makeVisitor([&](UniqueRef<AST::UnnamedType>& unnamedType) -> bool {
            keyValuePair.key->setType(unnamedType->clone());
            return true;
        }, [&](Ref<ResolvableTypeReference>& resolvableTypeReference) -> bool {
            if (!resolvableTypeReference->resolvableType().resolvedType()) {
                // FIXME: Instead of trying to commit, it might be better to just return an error instead.
                if (!static_cast<bool>(commit(resolvableTypeReference->resolvableType())))
                    return false;
            }
            keyValuePair.key->setType(resolvableTypeReference->resolvableType().resolvedType()->clone());
            return true;
        }), keyValuePair.value);
        if (!success)
            return false;
    }

    for (auto& keyValuePair : m_addressSpaceMap)
        keyValuePair.key->setAddressSpace(keyValuePair.value);
    return true;
}

bool Checker::checkShaderType(const AST::FunctionDefinition& functionDefinition)
{
    switch (*functionDefinition.entryPointType()) {
    case AST::EntryPointType::Vertex:
        return !m_vertexEntryPoints.add(functionDefinition.name()).isNewEntry;
    case AST::EntryPointType::Fragment:
        return !m_fragmentEntryPoints.add(functionDefinition.name()).isNewEntry;
    case AST::EntryPointType::Compute:
        return !m_computeEntryPoints.add(functionDefinition.name()).isNewEntry;
    }
}

void Checker::visit(AST::FunctionDefinition& functionDefinition)
{
    if (functionDefinition.entryPointType()) {
        if (!checkShaderType(functionDefinition)) {
            setError();
            return;
        }
        auto entryPointItems = gatherEntryPointItems(m_intrinsics, functionDefinition);
        if (!entryPointItems) {
            setError();
            return;
        }
        if (!checkSemantics(entryPointItems->inputs, entryPointItems->outputs, functionDefinition.entryPointType(), m_intrinsics)) {
            setError();
            return;
        }
    }
    if (!checkOperatorOverload(functionDefinition, m_intrinsics, m_program.nameContext())) {
        setError();
        return;
    }

    checkErrorAndVisit(functionDefinition);
}

static Optional<UniqueRef<AST::UnnamedType>> matchAndCommit(ResolvingType& left, ResolvingType& right)
{
    return WTF::visit(WTF::makeVisitor([&](UniqueRef<AST::UnnamedType>& left) -> Optional<UniqueRef<AST::UnnamedType>> {
        return WTF::visit(WTF::makeVisitor([&](UniqueRef<AST::UnnamedType>& right) -> Optional<UniqueRef<AST::UnnamedType>> {
            if (matches(left, right))
                return left->clone();
            return WTF::nullopt;
        }, [&](Ref<ResolvableTypeReference>& right) -> Optional<UniqueRef<AST::UnnamedType>> {
            return matchAndCommit(left, right->resolvableType());
        }), right);
    }, [&](Ref<ResolvableTypeReference>& left) -> Optional<UniqueRef<AST::UnnamedType>> {
        return WTF::visit(WTF::makeVisitor([&](UniqueRef<AST::UnnamedType>& right) -> Optional<UniqueRef<AST::UnnamedType>> {
            return matchAndCommit(right, left->resolvableType());
        }, [&](Ref<ResolvableTypeReference>& right) -> Optional<UniqueRef<AST::UnnamedType>> {
            return matchAndCommit(left->resolvableType(), right->resolvableType());
        }), right);
    }), left);
}

static Optional<UniqueRef<AST::UnnamedType>> matchAndCommit(ResolvingType& resolvingType, AST::UnnamedType& unnamedType)
{
    return WTF::visit(WTF::makeVisitor([&](UniqueRef<AST::UnnamedType>& resolvingType) -> Optional<UniqueRef<AST::UnnamedType>> {
        if (matches(unnamedType, resolvingType))
            return unnamedType.clone();
        return WTF::nullopt;
    }, [&](Ref<ResolvableTypeReference>& resolvingType) -> Optional<UniqueRef<AST::UnnamedType>> {
        return matchAndCommit(unnamedType, resolvingType->resolvableType());
    }), resolvingType);
}

static Optional<UniqueRef<AST::UnnamedType>> matchAndCommit(ResolvingType& resolvingType, AST::NamedType& namedType)
{
    return WTF::visit(WTF::makeVisitor([&](UniqueRef<AST::UnnamedType>& resolvingType) -> Optional<UniqueRef<AST::UnnamedType>> {
        if (matches(resolvingType, namedType))
            return resolvingType->clone();
        return WTF::nullopt;
    }, [&](Ref<ResolvableTypeReference>& resolvingType) -> Optional<UniqueRef<AST::UnnamedType>> {
        return matchAndCommit(namedType, resolvingType->resolvableType());
    }), resolvingType);
}

void Checker::visit(AST::EnumerationDefinition& enumerationDefinition)
{
    auto* baseType = ([&]() -> AST::NativeTypeDeclaration* {
        checkErrorAndVisit(enumerationDefinition.type());
        auto& baseType = enumerationDefinition.type().unifyNode();
        if (!is<AST::NamedType>(baseType))
            return nullptr;
        auto& namedType = downcast<AST::NamedType>(baseType);
        if (!is<AST::NativeTypeDeclaration>(namedType))
            return nullptr;
        auto& nativeTypeDeclaration = downcast<AST::NativeTypeDeclaration>(namedType);
        if (!nativeTypeDeclaration.isInt())
            return nullptr;
        return &nativeTypeDeclaration;
    })();
    if (!baseType) {
        setError();
        return;
    }

    auto enumerationMembers = enumerationDefinition.enumerationMembers();

    for (auto& member : enumerationMembers) {
        if (!member.get().value())
            continue;

        bool success = false;
        member.get().value()->visit(WTF::makeVisitor([&](AST::Expression& value) {
            auto valueInfo = recurseAndGetInfo(value);
            if (!valueInfo)
                return;
            success = static_cast<bool>(matchAndCommit(valueInfo->resolvingType, *baseType));
        }));
        if (!success) {
            setError();
            return;
        }
    }

    int64_t nextValue = 0;
    for (auto& member : enumerationMembers) {
        if (member.get().value()) {
            int64_t value;
            member.get().value()->visit(WTF::makeVisitor([&](AST::IntegerLiteral& integerLiteral) {
                value = integerLiteral.valueForSelectedType();
            }, [&](AST::UnsignedIntegerLiteral& unsignedIntegerLiteral) {
                value = unsignedIntegerLiteral.valueForSelectedType();
            }, [&](auto&) {
                ASSERT_NOT_REACHED();
            }));
            nextValue = baseType->successor()(value);
        } else {
            if (nextValue > std::numeric_limits<int>::max()) {
                ASSERT(nextValue <= std::numeric_limits<unsigned>::max());
                member.get().setValue(AST::ConstantExpression(AST::UnsignedIntegerLiteral(Lexer::Token(member.get().origin()), static_cast<unsigned>(nextValue))));
            }
            ASSERT(nextValue >= std::numeric_limits<int>::min());
            member.get().setValue(AST::ConstantExpression(AST::IntegerLiteral(Lexer::Token(member.get().origin()), static_cast<int>(nextValue))));
            nextValue = baseType->successor()(nextValue);
        }
    }

    auto getValue = [&](AST::EnumerationMember& member) -> int64_t {
        int64_t value;
        ASSERT(member.value());
        member.value()->visit(WTF::makeVisitor([&](AST::IntegerLiteral& integerLiteral) {
            value = integerLiteral.value();
        }, [&](AST::UnsignedIntegerLiteral& unsignedIntegerLiteral) {
            value = unsignedIntegerLiteral.value();
        }, [&](auto&) {
            ASSERT_NOT_REACHED();
        }));
        return value;
    };

    for (size_t i = 0; i < enumerationMembers.size(); ++i) {
        auto value = getValue(enumerationMembers[i].get());
        for (size_t j = i + 1; j < enumerationMembers.size(); ++j) {
            auto otherValue = getValue(enumerationMembers[j].get());
            if (value == otherValue) {
                setError();
                return;
            }
        }
    }

    bool foundZero = false;
    for (auto& member : enumerationMembers) {
        if (!getValue(member.get())) {
            foundZero = true;
            break;
        }
    }
    if (!foundZero) {
        setError();
        return;
    }
}

void Checker::visit(AST::TypeReference& typeReference)
{
    ASSERT(typeReference.resolvedType());

    checkErrorAndVisit(typeReference);
}

auto Checker::recurseAndGetInfo(AST::Expression& expression, bool requiresLValue) -> Optional<RecurseInfo>
{
    checkErrorAndVisit(expression);
    if (!error())
        return WTF::nullopt;
    return getInfo(expression, requiresLValue);
}

auto Checker::getInfo(AST::Expression& expression, bool requiresLValue) -> Optional<RecurseInfo>
{
    auto typeIterator = m_typeMap.find(&expression);
    ASSERT(typeIterator != m_typeMap.end());

    auto addressSpaceIterator = m_addressSpaceMap.find(&expression);
    ASSERT(addressSpaceIterator != m_addressSpaceMap.end());
    if (requiresLValue && !addressSpaceIterator->value) {
        setError();
        return WTF::nullopt;
    }
    return {{ typeIterator->value, addressSpaceIterator->value }};
}

void Checker::visit(AST::VariableDeclaration& variableDeclaration)
{
    // ReadModifyWriteExpressions are the only place where anonymous variables exist,
    // and that doesn't recurse on the anonymous variables, so we can assume the variable has a type.
    checkErrorAndVisit(*variableDeclaration.type());
    if (variableDeclaration.initializer()) {
        auto& lhsType = *variableDeclaration.type();
        auto initializerInfo = recurseAndGetInfo(*variableDeclaration.initializer());
        if (!initializerInfo)
            return;
        if (!matchAndCommit(initializerInfo->resolvingType, lhsType)) {
            setError();
            return;
        }
    }
}

void Checker::assignType(AST::Expression& expression, UniqueRef<AST::UnnamedType>&& unnamedType, Optional<AST::AddressSpace> addressSpace)
{
    auto addResult = m_typeMap.add(&expression, WTFMove(unnamedType));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    auto addressSpaceAddResult = m_addressSpaceMap.add(&expression, addressSpace);
    ASSERT_UNUSED(addressSpaceAddResult, addressSpaceAddResult.isNewEntry);
}

void Checker::assignType(AST::Expression& expression, Ref<ResolvableTypeReference>&& resolvableTypeReference, Optional<AST::AddressSpace> addressSpace)
{
    auto addResult = m_typeMap.add(&expression, WTFMove(resolvableTypeReference));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    auto addressSpaceAddResult = m_addressSpaceMap.add(&expression, addressSpace);
    ASSERT_UNUSED(addressSpaceAddResult, addressSpaceAddResult.isNewEntry);
}

void Checker::visit(AST::AssignmentExpression& assignmentExpression)
{
    auto leftInfo = recurseAndGetInfo(assignmentExpression.left(), true);
    if (!leftInfo)
        return;

    auto rightInfo = recurseAndGetInfo(assignmentExpression.right());
    if (!rightInfo)
        return;

    auto resultType = matchAndCommit(leftInfo->resolvingType, rightInfo->resolvingType);
    if (!resultType) {
        setError();
        return;
    }

    assignType(assignmentExpression, WTFMove(*resultType));
}

void Checker::forwardType(AST::Expression& expression, ResolvingType& resolvingType, Optional<AST::AddressSpace> addressSpace)
{
    WTF::visit(WTF::makeVisitor([&](UniqueRef<AST::UnnamedType>& result) {
        auto addResult = m_typeMap.add(&expression, result->clone());
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }, [&](Ref<ResolvableTypeReference>& result) {
        auto addResult = m_typeMap.add(&expression, result.copyRef());
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }), resolvingType);
    auto addressSpaceAddResult = m_addressSpaceMap.add(&expression, addressSpace);
    ASSERT_UNUSED(addressSpaceAddResult, addressSpaceAddResult.isNewEntry);
}

void Checker::visit(AST::ReadModifyWriteExpression& readModifyWriteExpression)
{
    auto lValueInfo = recurseAndGetInfo(readModifyWriteExpression.lValue(), true);
    if (!lValueInfo)
        return;

    // FIXME: Figure out what to do with the ReadModifyWriteExpression's AnonymousVariables.

    auto newValueInfo = recurseAndGetInfo(*readModifyWriteExpression.newValueExpression());
    if (!newValueInfo)
        return;

    if (!matchAndCommit(lValueInfo->resolvingType, newValueInfo->resolvingType)) {
        setError();
        return;
    }

    auto resultInfo = recurseAndGetInfo(*readModifyWriteExpression.resultExpression());
    if (!resultInfo)
        return;

    forwardType(readModifyWriteExpression, resultInfo->resolvingType);
}

static AST::UnnamedType* getUnnamedType(ResolvingType& resolvingType)
{
    return WTF::visit(WTF::makeVisitor([](UniqueRef<AST::UnnamedType>& type) -> AST::UnnamedType* {
        return &type;
    }, [](Ref<ResolvableTypeReference>& type) -> AST::UnnamedType* {
        // FIXME: If the type isn't committed, should we just commit() it now?
        return type->resolvableType().resolvedType();
    }), resolvingType);
}

void Checker::visit(AST::DereferenceExpression& dereferenceExpression)
{
    auto pointerInfo = recurseAndGetInfo(dereferenceExpression.pointer());
    if (!pointerInfo)
        return;

    auto* unnamedType = getUnnamedType(pointerInfo->resolvingType);

    auto* pointerType = ([&](AST::UnnamedType* unnamedType) -> AST::PointerType* {
        if (!unnamedType)
            return nullptr;
        auto& unifyNode = unnamedType->unifyNode();
        if (!is<AST::UnnamedType>(unifyNode))
            return nullptr;
        auto& unnamedUnifyType = downcast<AST::UnnamedType>(unifyNode);
        if (!is<AST::PointerType>(unnamedUnifyType))
            return nullptr;
        return &downcast<AST::PointerType>(unnamedUnifyType);
    })(unnamedType);
    if (!pointerType) {
        setError();
        return;
    }

    assignType(dereferenceExpression, pointerType->clone(), pointerType->addressSpace());
}

void Checker::visit(AST::MakePointerExpression& makePointerExpression)
{
    auto lValueInfo = recurseAndGetInfo(makePointerExpression.lValue(), true);
    if (!lValueInfo)
        return;

    auto* lValueType = getUnnamedType(lValueInfo->resolvingType);
    if (!lValueType) {
        setError();
        return;
    }

    assignType(makePointerExpression, makeUniqueRef<AST::PointerType>(Lexer::Token(makePointerExpression.origin()), *lValueInfo->addressSpace, lValueType->clone()));
}

void Checker::visit(AST::MakeArrayReferenceExpression& makeArrayReferenceExpression)
{
    auto lValueInfo = recurseAndGetInfo(makeArrayReferenceExpression.lValue());
    if (!lValueInfo)
        return;

    auto* lValueType = getUnnamedType(lValueInfo->resolvingType);
    if (!lValueType) {
        setError();
        return;
    }

    auto& unifyNode = lValueType->unifyNode();
    if (is<AST::UnnamedType>(unifyNode)) {
        auto& unnamedType = downcast<AST::UnnamedType>(unifyNode);
        if (is<AST::PointerType>(unnamedType)) {
            auto& pointerType = downcast<AST::PointerType>(unnamedType);
            // FIXME: Save the fact that we're not targetting the item; we're targetting the item's inner element.
            assignType(makeArrayReferenceExpression, makeUniqueRef<AST::ArrayReferenceType>(Lexer::Token(makeArrayReferenceExpression.origin()), pointerType.addressSpace(), pointerType.elementType().clone()));
            return;
        }

        if (!lValueInfo->addressSpace) {
            setError();
            return;
        }

        if (is<AST::ArrayType>(unnamedType)) {
            auto& arrayType = downcast<AST::ArrayType>(unnamedType);
            // FIXME: Save the number of elements.
            assignType(makeArrayReferenceExpression, makeUniqueRef<AST::ArrayReferenceType>(Lexer::Token(makeArrayReferenceExpression.origin()), *lValueInfo->addressSpace, arrayType.type().clone()));
            return;
        }
    }

    if (!lValueInfo->addressSpace) {
        setError();
        return;
    }

    assignType(makeArrayReferenceExpression, makeUniqueRef<AST::ArrayReferenceType>(Lexer::Token(makeArrayReferenceExpression.origin()), *lValueInfo->addressSpace, lValueType->clone()));
}

void Checker::finishVisitingPropertyAccess(AST::PropertyAccessExpression& propertyAccessExpression, AST::UnnamedType& wrappedBaseType, AST::UnnamedType* extraArgumentType)
{
    Optional<std::reference_wrapper<AST::NamedType>> castReturnType;
    using OverloadResolution = std::tuple<AST::FunctionDeclaration*, AST::UnnamedType*>;

    AST::FunctionDeclaration* getFunction;
    AST::UnnamedType* getReturnType;
    std::tie(getFunction, getReturnType) = ([&]() -> OverloadResolution {
        ResolvingType getArgumentType1(wrappedBaseType.clone());
        Optional<ResolvingType> getArgumentType2;
        if (extraArgumentType)
            getArgumentType2 = ResolvingType(extraArgumentType->clone());

        Vector<std::reference_wrapper<ResolvingType>> getArgumentTypes;
        getArgumentTypes.append(getArgumentType1);
        if (getArgumentType2)
            getArgumentTypes.append(*getArgumentType2);

        auto* getFunction = resolveFunctionOverloadImpl(propertyAccessExpression.possibleGetOverloads(), getArgumentTypes, castReturnType);
        if (!getFunction)
            return std::make_pair(nullptr, nullptr);
        return std::make_pair(getFunction, &getFunction->type());
    })();

    AST::FunctionDeclaration* andFunction;
    AST::UnnamedType* andReturnType;
    std::tie(andFunction, andReturnType) = ([&]() -> OverloadResolution {
        auto computeAndArgumentType = [&](AST::UnnamedType& unnamedType) -> Optional<ResolvingType> {
            if (is<AST::ArrayReferenceType>(unnamedType))
                return { unnamedType.clone() };
            if (is<AST::ArrayType>(unnamedType))
                return { makeUniqueRef<AST::ArrayReferenceType>(Lexer::Token(propertyAccessExpression.origin()), AST::AddressSpace::Thread, downcast<AST::ArrayType>(unnamedType).type().clone()) };
            if (is<AST::PointerType>(unnamedType))
                return WTF::nullopt;
            return { makeUniqueRef<AST::PointerType>(Lexer::Token(propertyAccessExpression.origin()), AST::AddressSpace::Thread, downcast<AST::ArrayType>(unnamedType).type().clone()) };
        };
        auto computeAndReturnType = [&](AST::UnnamedType& unnamedType) -> AST::UnnamedType* {
            if (is<AST::PointerType>(unnamedType))
                return &downcast<AST::PointerType>(unnamedType).elementType();
            return nullptr;
        };

        auto andArgumentType1 = computeAndArgumentType(wrappedBaseType);
        if (!andArgumentType1)
            return std::make_pair(nullptr, nullptr);
        Optional<ResolvingType> andArgumentType2;
        if (extraArgumentType)
            andArgumentType2 = ResolvingType(extraArgumentType->clone());

        Vector<std::reference_wrapper<ResolvingType>> andArgumentTypes;
        andArgumentTypes.append(*andArgumentType1);
        if (andArgumentType2)
            andArgumentTypes.append(*andArgumentType2);

        auto* andFunction = resolveFunctionOverloadImpl(propertyAccessExpression.possibleAndOverloads(), andArgumentTypes, castReturnType);
        if (!andFunction)
            return std::make_pair(nullptr, nullptr);
        return std::make_pair(andFunction, computeAndReturnType(andFunction->type()));
    })();

    if (!getReturnType && !andReturnType) {
        setError();
        return;
    }

    if (getReturnType && andReturnType && !matches(*getReturnType, *andReturnType)) {
        setError();
        return;
    }

    AST::FunctionDeclaration* setFunction;
    AST::UnnamedType* setReturnType;
    std::tie(setFunction, setReturnType) = ([&]() -> OverloadResolution {
        ResolvingType setArgument1Type(wrappedBaseType.clone());
        Optional<ResolvingType> setArgumentType2;
        if (extraArgumentType)
            setArgumentType2 = ResolvingType(extraArgumentType->clone());
        ResolvingType setArgument3Type(getReturnType ? getReturnType->clone() : andReturnType->clone());

        Vector<std::reference_wrapper<ResolvingType>> setArgumentTypes;
        setArgumentTypes.append(setArgument1Type);
        if (setArgumentType2)
            setArgumentTypes.append(*setArgumentType2);
        setArgumentTypes.append(setArgument3Type);

        auto* setFunction = resolveFunctionOverloadImpl(propertyAccessExpression.possibleSetOverloads(), setArgumentTypes, castReturnType);
        if (!setFunction)
            return std::make_pair(nullptr, nullptr);
        return std::make_pair(setFunction, &setFunction->type());
    })();

    if (setFunction) {
        if (!matches(setFunction->type(), wrappedBaseType)) {
            setError();
            return;
        }
    }

    Optional<AST::AddressSpace> addressSpace;
    if (getReturnType || andReturnType) {
        // FIXME: The reference compiler has "else if (!node.base.isLValue && !baseType.isArrayRef)",
        // but I don't understand why it exists. I haven't written it here, and I'll investigate
        // if we can remove it from the reference compiler.
        if (is<AST::ReferenceType>(wrappedBaseType))
            addressSpace = downcast<AST::ReferenceType>(wrappedBaseType).addressSpace();
        else {
            auto addressSpaceIterator = m_addressSpaceMap.find(&propertyAccessExpression.base());
            ASSERT(addressSpaceIterator != m_addressSpaceMap.end());
            if (addressSpaceIterator->value)
                addressSpace = *addressSpaceIterator->value;
            else {
                setError();
                return;
            }
        }
    }

    // FIXME: Generate the call expressions

    assignType(propertyAccessExpression, getReturnType ? getReturnType->clone() : andReturnType->clone(), addressSpace);
}

Optional<UniqueRef<AST::UnnamedType>> Checker::recurseAndWrapBaseType(AST::PropertyAccessExpression& propertyAccessExpression)
{
    auto baseInfo = recurseAndGetInfo(propertyAccessExpression.base());
    if (!baseInfo)
        return WTF::nullopt;

    auto* baseType = getUnnamedType(baseInfo->resolvingType);
    if (!baseType) {
        setError();
        return WTF::nullopt;
    }
    auto& baseUnifyNode = baseType->unifyNode();
    if (is<AST::UnnamedType>(baseUnifyNode))
        return downcast<AST::UnnamedType>(baseUnifyNode).clone();
    ASSERT(is<AST::NamedType>(baseUnifyNode));
    return { AST::TypeReference::wrap(Lexer::Token(propertyAccessExpression.origin()), downcast<AST::NamedType>(baseUnifyNode)) };
}

void Checker::visit(AST::DotExpression& dotExpression)
{
    auto baseType = recurseAndWrapBaseType(dotExpression);
    if (!baseType)
        return;

    finishVisitingPropertyAccess(dotExpression, *baseType);
}

void Checker::visit(AST::IndexExpression& indexExpression)
{
    auto baseType = recurseAndWrapBaseType(indexExpression);
    if (!baseType)
        return;

    auto indexInfo = recurseAndGetInfo(indexExpression.indexExpression());
    if (!indexInfo)
        return;
    auto indexExpressionType = getUnnamedType(indexInfo->resolvingType);
    if (!indexExpressionType) {
        setError();
        return;
    }

    finishVisitingPropertyAccess(indexExpression, WTFMove(*baseType), indexExpressionType);
}

void Checker::visit(AST::VariableReference& variableReference)
{
    ASSERT(variableReference.variable());
    ASSERT(variableReference.variable()->type());
    
    Optional<AST::AddressSpace> addressSpace;
    if (!variableReference.variable()->isAnonymous())
        addressSpace = AST::AddressSpace::Thread;
    assignType(variableReference, variableReference.variable()->type()->clone(), addressSpace);
}

void Checker::visit(AST::Return& returnStatement)
{
    ASSERT(returnStatement.function());
    if (returnStatement.value()) {
        auto valueInfo = recurseAndGetInfo(*returnStatement.value());
        if (!valueInfo)
            return;
        if (!matchAndCommit(valueInfo->resolvingType, returnStatement.function()->type()))
            setError();
        return;
    }

    if (!matches(returnStatement.function()->type(), m_intrinsics.voidType()))
        setError();
}

void Checker::visit(AST::PointerType&)
{
    // Following pointer types can cause infinite loops because of data structures
    // like linked lists.
    // FIXME: Make sure this function should be empty
}

void Checker::visit(AST::ArrayReferenceType&)
{
    // Following array reference types can cause infinite loops because of data
    // structures like linked lists.
    // FIXME: Make sure this function should be empty
}

void Checker::visit(AST::IntegerLiteral& integerLiteral)
{
    assignType(integerLiteral, adoptRef(*new ResolvableTypeReference(integerLiteral.type())));
}

void Checker::visit(AST::UnsignedIntegerLiteral& unsignedIntegerLiteral)
{
    assignType(unsignedIntegerLiteral, adoptRef(*new ResolvableTypeReference(unsignedIntegerLiteral.type())));
}

void Checker::visit(AST::FloatLiteral& floatLiteral)
{
    assignType(floatLiteral, adoptRef(*new ResolvableTypeReference(floatLiteral.type())));
}

void Checker::visit(AST::NullLiteral& nullLiteral)
{
    assignType(nullLiteral, adoptRef(*new ResolvableTypeReference(nullLiteral.type())));
}

void Checker::visit(AST::BooleanLiteral& booleanLiteral)
{
    assignType(booleanLiteral, AST::TypeReference::wrap(Lexer::Token(booleanLiteral.origin()), m_intrinsics.boolType()));
}

void Checker::visit(AST::EnumerationMemberLiteral& enumerationMemberLiteral)
{
    ASSERT(enumerationMemberLiteral.enumerationDefinition());
    auto& enumerationDefinition = *enumerationMemberLiteral.enumerationDefinition();
    assignType(enumerationMemberLiteral, AST::TypeReference::wrap(Lexer::Token(enumerationMemberLiteral.origin()), enumerationDefinition));
}

bool Checker::isBoolType(ResolvingType& resolvingType)
{
    return WTF::visit(WTF::makeVisitor([&](UniqueRef<AST::UnnamedType>& left) -> bool {
        return matches(left, m_intrinsics.boolType());
    }, [&](Ref<ResolvableTypeReference>& left) -> bool {
        return static_cast<bool>(matchAndCommit(m_intrinsics.boolType(), left->resolvableType()));
    }), resolvingType);
}

bool Checker::recurseAndRequireBoolType(AST::Expression& expression)
{
    auto expressionInfo = recurseAndGetInfo(expression);
    if (!expressionInfo)
        return false;
    if (!isBoolType(expressionInfo->resolvingType)) {
        setError();
        return false;
    }
    return true;
}

void Checker::visit(AST::LogicalNotExpression& logicalNotExpression)
{
    if (!recurseAndRequireBoolType(logicalNotExpression.operand()))
        return;
    assignType(logicalNotExpression, AST::TypeReference::wrap(Lexer::Token(logicalNotExpression.origin()), m_intrinsics.boolType()));
}

void Checker::visit(AST::LogicalExpression& logicalExpression)
{
    if (!recurseAndRequireBoolType(logicalExpression.left()))
        return;
    if (!recurseAndRequireBoolType(logicalExpression.right()))
        return;
    assignType(logicalExpression, AST::TypeReference::wrap(Lexer::Token(logicalExpression.origin()), m_intrinsics.boolType()));
}

void Checker::visit(AST::IfStatement& ifStatement)
{
    if (!recurseAndRequireBoolType(ifStatement.conditional()))
        return;
    checkErrorAndVisit(ifStatement.body());
    if (ifStatement.elseBody())
        checkErrorAndVisit(*ifStatement.elseBody());
}

void Checker::visit(AST::WhileLoop& whileLoop)
{
    if (!recurseAndRequireBoolType(whileLoop.conditional()))
        return;
    checkErrorAndVisit(whileLoop.body());
}

void Checker::visit(AST::DoWhileLoop& doWhileLoop)
{
    checkErrorAndVisit(doWhileLoop.body());
    recurseAndRequireBoolType(doWhileLoop.conditional());
}

void Checker::visit(AST::ForLoop& forLoop)
{
    WTF::visit(WTF::makeVisitor([&](AST::VariableDeclarationsStatement& variableDeclarationsStatement) {
        checkErrorAndVisit(variableDeclarationsStatement);
    }, [&](UniqueRef<AST::Expression>& expression) {
        checkErrorAndVisit(expression);
    }), forLoop.initialization());
    if (error())
        return;
    if (forLoop.condition()) {
        if (!recurseAndRequireBoolType(*forLoop.condition()))
            return;
    }
    if (forLoop.increment())
        checkErrorAndVisit(*forLoop.increment());
    checkErrorAndVisit(forLoop.body());
}

void Checker::visit(AST::SwitchStatement& switchStatement)
{
    auto* valueType = ([&]() -> AST::NamedType* {
        auto valueInfo = recurseAndGetInfo(switchStatement.value());
        if (!valueInfo)
            return nullptr;
        auto* valueType = getUnnamedType(valueInfo->resolvingType);
        if (!valueType)
            return nullptr;
        auto& valueUnifyNode = valueType->unifyNode();
        if (!is<AST::NamedType>(valueUnifyNode))
            return nullptr;
        auto& valueNamedUnifyNode = downcast<AST::NamedType>(valueUnifyNode);
        if (!(is<AST::NativeTypeDeclaration>(valueNamedUnifyNode) && downcast<AST::NativeTypeDeclaration>(valueNamedUnifyNode).isInt())
            && !is<AST::EnumerationDefinition>(valueNamedUnifyNode))
            return nullptr;
        return &valueNamedUnifyNode;
    })();
    if (!valueType) {
        setError();
        return;
    }

    bool hasDefault = false;
    for (auto& switchCase : switchStatement.switchCases()) {
        checkErrorAndVisit(switchCase.block());
        if (!switchCase.value()) {
            hasDefault = true;
            continue;
        }
        bool success;
        switchCase.value()->visit(WTF::makeVisitor([&](AST::IntegerLiteral& integerLiteral) {
            success = static_cast<bool>(matchAndCommit(*valueType, integerLiteral.type()));
        }, [&](AST::UnsignedIntegerLiteral& unsignedIntegerLiteral) {
            success = static_cast<bool>(matchAndCommit(*valueType, unsignedIntegerLiteral.type()));
        }, [&](AST::FloatLiteral& floatLiteral) {
            success = static_cast<bool>(matchAndCommit(*valueType, floatLiteral.type()));
        }, [&](AST::NullLiteral& nullLiteral) {
            success = static_cast<bool>(matchAndCommit(*valueType, nullLiteral.type()));
        }, [&](AST::BooleanLiteral&) {
            success = matches(*valueType, m_intrinsics.boolType());
        }, [&](AST::EnumerationMemberLiteral& enumerationMemberLiteral) {
            ASSERT(enumerationMemberLiteral.enumerationDefinition());
            success = matches(*valueType, *enumerationMemberLiteral.enumerationDefinition());
        }));
        if (!success) {
            setError();
            return;
        }
    }

    for (size_t i = 0; i < switchStatement.switchCases().size(); ++i) {
        auto& firstCase = switchStatement.switchCases()[i];
        for (size_t j = i + 1; j < switchStatement.switchCases().size(); ++j) {
            auto& secondCase = switchStatement.switchCases()[j];
            
            if (static_cast<bool>(firstCase.value()) != static_cast<bool>(secondCase.value()))
                continue;

            if (!static_cast<bool>(firstCase.value())) {
                setError();
                return;
            }

            bool success = true;
            firstCase.value()->visit(WTF::makeVisitor([&](AST::IntegerLiteral& firstIntegerLiteral) {
                secondCase.value()->visit(WTF::makeVisitor([&](AST::IntegerLiteral& secondIntegerLiteral) {
                    success = firstIntegerLiteral.value() != secondIntegerLiteral.value();
                }, [&](AST::UnsignedIntegerLiteral& secondUnsignedIntegerLiteral) {
                    success = static_cast<int64_t>(firstIntegerLiteral.value()) != static_cast<int64_t>(secondUnsignedIntegerLiteral.value());
                }, [](auto&) {
                }));
            }, [&](AST::UnsignedIntegerLiteral& firstUnsignedIntegerLiteral) {
                secondCase.value()->visit(WTF::makeVisitor([&](AST::IntegerLiteral& secondIntegerLiteral) {
                    success = static_cast<int64_t>(firstUnsignedIntegerLiteral.value()) != static_cast<int64_t>(secondIntegerLiteral.value());
                }, [&](AST::UnsignedIntegerLiteral& secondUnsignedIntegerLiteral) {
                    success = firstUnsignedIntegerLiteral.value() != secondUnsignedIntegerLiteral.value();
                }, [](auto&) {
                }));
            }, [&](AST::EnumerationMemberLiteral& firstEnumerationMemberLiteral) {
                secondCase.value()->visit(WTF::makeVisitor([&](AST::IntegerLiteral&) {
                }, [&](AST::EnumerationMemberLiteral& secondEnumerationMemberLiteral) {
                    ASSERT(firstEnumerationMemberLiteral.enumerationMember());
                    ASSERT(secondEnumerationMemberLiteral.enumerationMember());
                    success = firstEnumerationMemberLiteral.enumerationMember() != secondEnumerationMemberLiteral.enumerationMember();
                }, [](auto&) {
                }));
            }, [](auto&) {
            }));
        }
    }

    if (!hasDefault) {
        if (is<AST::NativeTypeDeclaration>(*valueType)) {
            HashSet<int64_t> values;
            bool zeroValueExists;
            for (auto& switchCase : switchStatement.switchCases()) {
                int64_t value;
                switchCase.value()->visit(WTF::makeVisitor([&](AST::IntegerLiteral& integerLiteral) {
                    value = integerLiteral.valueForSelectedType();
                }, [&](AST::UnsignedIntegerLiteral& unsignedIntegerLiteral) {
                    value = unsignedIntegerLiteral.valueForSelectedType();
                }, [](auto&) {
                    ASSERT_NOT_REACHED();
                }));
                if (!value)
                    zeroValueExists = true;
                else
                    values.add(value);
            }
            bool success = true;
            downcast<AST::NativeTypeDeclaration>(*valueType).iterateAllValues([&](int64_t value) -> bool {
                if (!value) {
                    if (!zeroValueExists) {
                        success = false;
                        return true;
                    }
                    return false;
                }
                if (!values.contains(value)) {
                    success = false;
                    return true;
                }
                return false;
            });
            if (!success) {
                setError();
                return;
            }
        } else {
            ASSERT(is<AST::EnumerationDefinition>(*valueType));
            HashSet<AST::EnumerationMember*> values;
            for (auto& switchCase : switchStatement.switchCases()) {
                switchCase.value()->visit(WTF::makeVisitor([&](AST::EnumerationMemberLiteral& enumerationMemberLiteral) {
                    ASSERT(enumerationMemberLiteral.enumerationMember());
                    values.add(enumerationMemberLiteral.enumerationMember());
                }, [](auto&) {
                    ASSERT_NOT_REACHED();
                }));
            }
            for (auto& enumerationMember : downcast<AST::EnumerationDefinition>(*valueType).enumerationMembers()) {
                if (!values.contains(&enumerationMember.get())) {
                    setError();
                    return;
                }
            }
        }
    }
}

void Checker::visit(AST::CommaExpression& commaExpression)
{
    ASSERT(commaExpression.list().size() > 0);
    Visitor::visit(commaExpression);
    if (error())
        return;
    auto lastInfo = getInfo(commaExpression.list().last());
    forwardType(commaExpression, lastInfo->resolvingType);
}

void Checker::visit(AST::TernaryExpression& ternaryExpression)
{
    auto predicateInfo = recurseAndRequireBoolType(ternaryExpression.predicate());
    if (!predicateInfo)
        return;

    auto bodyInfo = recurseAndGetInfo(ternaryExpression.bodyExpression());
    auto elseInfo = recurseAndGetInfo(ternaryExpression.elseExpression());
    
    auto resultType = matchAndCommit(bodyInfo->resolvingType, elseInfo->resolvingType);
    if (!resultType) {
        setError();
        return;
    }

    assignType(ternaryExpression, WTFMove(*resultType));
}

void Checker::visit(AST::CallExpression& callExpression)
{
    Vector<std::reference_wrapper<ResolvingType>> types;
    types.reserveInitialCapacity(callExpression.arguments().size());
    for (auto& argument : callExpression.arguments()) {
        auto argumentInfo = recurseAndGetInfo(argument);
        if (!argumentInfo)
            return;
        types.uncheckedAppend(argumentInfo->resolvingType);
    }
    if (callExpression.castReturnType()) {
        checkErrorAndVisit(callExpression.castReturnType()->get());
        if (error())
            return;
    }

    ASSERT(callExpression.hasOverloads());
    auto* function = resolveFunctionOverloadImpl(*callExpression.overloads(), types, callExpression.castReturnType());
    if (!function) {
        if (auto newFunction = resolveByInstantiation(callExpression, types, m_intrinsics)) {
            m_program.append(WTFMove(*newFunction));
            function = &m_program.nativeFunctionDeclarations().last();
        }
    }

    if (!function) {
        setError();
        return;
    }

    for (size_t i = 0; i < function->parameters().size(); ++i) {
        if (!matchAndCommit(types[i].get(), *function->parameters()[i].type())) {
            setError();
            return;
        }
    }

    callExpression.setFunction(*function);

    assignType(callExpression, function->type().clone());
}

bool check(Program& program)
{
    Checker checker(program.intrinsics(), program);
    checker.checkErrorAndVisit(program);
    if (checker.error())
        return false;
    return checker.assignTypes();
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
