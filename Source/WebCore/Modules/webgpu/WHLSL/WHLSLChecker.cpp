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
#include "WHLSLEntryPointType.h"
#include "WHLSLForLoop.h"
#include "WHLSLGatherEntryPointItems.h"
#include "WHLSLIfStatement.h"
#include "WHLSLIndexExpression.h"
#include "WHLSLInferTypes.h"
#include "WHLSLLogicalExpression.h"
#include "WHLSLLogicalNotExpression.h"
#include "WHLSLMakeArrayReferenceExpression.h"
#include "WHLSLMakePointerExpression.h"
#include "WHLSLNameContext.h"
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
        checkErrorAndVisit(typeReference.resolvedType());
    }
};

static AST::NativeFunctionDeclaration resolveWithOperatorAnderIndexer(AST::CodeLocation location, AST::ArrayReferenceType& firstArgument, const Intrinsics& intrinsics)
{
    const bool isOperator = true;
    auto returnType = makeUniqueRef<AST::PointerType>(location, firstArgument.addressSpace(), firstArgument.elementType().clone());
    AST::VariableDeclarations parameters;
    parameters.append(makeUniqueRef<AST::VariableDeclaration>(location, AST::Qualifiers(), firstArgument.clone(), String(), nullptr, nullptr));
    parameters.append(makeUniqueRef<AST::VariableDeclaration>(location, AST::Qualifiers(), UniqueRef<AST::UnnamedType>(AST::TypeReference::wrap(location, intrinsics.uintType())), String(), nullptr, nullptr));
    return AST::NativeFunctionDeclaration(AST::FunctionDeclaration(location, AST::AttributeBlock(), WTF::nullopt, WTFMove(returnType), String("operator&[]", String::ConstructFromLiteral), WTFMove(parameters), nullptr, isOperator));
}

static AST::NativeFunctionDeclaration resolveWithOperatorLength(AST::CodeLocation location, AST::UnnamedType& firstArgument, const Intrinsics& intrinsics)
{
    const bool isOperator = true;
    auto returnType = AST::TypeReference::wrap(location, intrinsics.uintType());
    AST::VariableDeclarations parameters;
    parameters.append(makeUniqueRef<AST::VariableDeclaration>(location, AST::Qualifiers(), firstArgument.clone(), String(), nullptr, nullptr));
    return AST::NativeFunctionDeclaration(AST::FunctionDeclaration(location, AST::AttributeBlock(), WTF::nullopt, WTFMove(returnType), String("operator.length", String::ConstructFromLiteral), WTFMove(parameters), nullptr, isOperator));
}

static AST::NativeFunctionDeclaration resolveWithReferenceComparator(AST::CodeLocation location, ResolvingType& firstArgument, ResolvingType& secondArgument, const Intrinsics& intrinsics)
{
    const bool isOperator = true;
    auto returnType = AST::TypeReference::wrap(location, intrinsics.boolType());
    auto argumentType = firstArgument.visit(WTF::makeVisitor([](UniqueRef<AST::UnnamedType>& unnamedType) -> UniqueRef<AST::UnnamedType> {
        return unnamedType->clone();
    }, [&](RefPtr<ResolvableTypeReference>&) -> UniqueRef<AST::UnnamedType> {
        return secondArgument.visit(WTF::makeVisitor([](UniqueRef<AST::UnnamedType>& unnamedType) -> UniqueRef<AST::UnnamedType> {
            return unnamedType->clone();
        }, [&](RefPtr<ResolvableTypeReference>&) -> UniqueRef<AST::UnnamedType> {
            // We encountered "null == null".
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198162 This can probably be generalized, using the "preferred type" infrastructure used by generic literals
            ASSERT_NOT_REACHED();
            return AST::TypeReference::wrap(location, intrinsics.intType());
        }));
    }));
    AST::VariableDeclarations parameters;
    parameters.append(makeUniqueRef<AST::VariableDeclaration>(location, AST::Qualifiers(), argumentType->clone(), String(), nullptr, nullptr));
    parameters.append(makeUniqueRef<AST::VariableDeclaration>(location, AST::Qualifiers(), UniqueRef<AST::UnnamedType>(WTFMove(argumentType)), String(), nullptr, nullptr));
    return AST::NativeFunctionDeclaration(AST::FunctionDeclaration(location, AST::AttributeBlock(), WTF::nullopt, WTFMove(returnType), String("operator==", String::ConstructFromLiteral), WTFMove(parameters), nullptr, isOperator));
}

enum class Acceptability {
    Yes,
    Maybe,
    No
};

static Optional<AST::NativeFunctionDeclaration> resolveByInstantiation(const String& name, AST::CodeLocation location, const Vector<std::reference_wrapper<ResolvingType>>& types, const Intrinsics& intrinsics)
{
    if (name == "operator&[]" && types.size() == 2) {
        auto* firstArgumentArrayRef = types[0].get().visit(WTF::makeVisitor([](UniqueRef<AST::UnnamedType>& unnamedType) -> AST::ArrayReferenceType* {
            if (is<AST::ArrayReferenceType>(static_cast<AST::UnnamedType&>(unnamedType)))
                return &downcast<AST::ArrayReferenceType>(static_cast<AST::UnnamedType&>(unnamedType));
            return nullptr;
        }, [](RefPtr<ResolvableTypeReference>&) -> AST::ArrayReferenceType* {
            return nullptr;
        }));
        bool secondArgumentIsUint = types[1].get().visit(WTF::makeVisitor([&](UniqueRef<AST::UnnamedType>& unnamedType) -> bool {
            return matches(unnamedType, intrinsics.uintType());
        }, [&](RefPtr<ResolvableTypeReference>& resolvableTypeReference) -> bool {
            return resolvableTypeReference->resolvableType().canResolve(intrinsics.uintType());
        }));
        if (firstArgumentArrayRef && secondArgumentIsUint)
            return resolveWithOperatorAnderIndexer(location, *firstArgumentArrayRef, intrinsics);
    } else if (name == "operator.length" && types.size() == 1) {
        auto* firstArgumentReference = types[0].get().visit(WTF::makeVisitor([](UniqueRef<AST::UnnamedType>& unnamedType) -> AST::UnnamedType* {
            if (is<AST::ArrayReferenceType>(static_cast<AST::UnnamedType&>(unnamedType)) || is<AST::ArrayType>(static_cast<AST::UnnamedType&>(unnamedType)))
                return &unnamedType;
            return nullptr;
        }, [](RefPtr<ResolvableTypeReference>&) -> AST::UnnamedType* {
            return nullptr;
        }));
        if (firstArgumentReference)
            return resolveWithOperatorLength(location, *firstArgumentReference, intrinsics);
    } else if (name == "operator==" && types.size() == 2) {
        auto acceptability = [](ResolvingType& resolvingType) -> Acceptability {
            return resolvingType.visit(WTF::makeVisitor([](UniqueRef<AST::UnnamedType>& unnamedType) -> Acceptability {
                auto& unifyNode = unnamedType->unifyNode();
                return is<AST::UnnamedType>(unifyNode) && is<AST::ReferenceType>(downcast<AST::UnnamedType>(unifyNode)) ? Acceptability::Yes : Acceptability::No;
            }, [](RefPtr<ResolvableTypeReference>& resolvableTypeReference) -> Acceptability {
                return is<AST::NullLiteralType>(resolvableTypeReference->resolvableType()) ? Acceptability::Maybe : Acceptability::No;
            }));
        };
        auto leftAcceptability = acceptability(types[0].get());
        auto rightAcceptability = acceptability(types[1].get());
        bool success = false;
        if (leftAcceptability == Acceptability::Yes && rightAcceptability == Acceptability::Yes) {
            auto& unnamedType1 = *types[0].get().getUnnamedType();
            auto& unnamedType2 = *types[1].get().getUnnamedType();
            success = matches(unnamedType1, unnamedType2);
        } else if ((leftAcceptability == Acceptability::Maybe && rightAcceptability == Acceptability::Yes)
            || (leftAcceptability == Acceptability::Yes && rightAcceptability == Acceptability::Maybe))
            success = true;
        if (success)
            return resolveWithReferenceComparator(location, types[0].get(), types[1].get(), intrinsics);
    }
    return WTF::nullopt;
}

static AST::FunctionDeclaration* resolveFunction(Program& program, Vector<std::reference_wrapper<AST::FunctionDeclaration>, 1>* possibleOverloads, Vector<std::reference_wrapper<ResolvingType>>& types, const String& name, AST::CodeLocation location, const Intrinsics& intrinsics, AST::NamedType* castReturnType = nullptr)
{
    if (possibleOverloads) {
        if (AST::FunctionDeclaration* function = resolveFunctionOverload(*possibleOverloads, types, castReturnType))
            return function;
    }

    if (auto newFunction = resolveByInstantiation(name, location, types, intrinsics)) {
        program.append(WTFMove(*newFunction));
        return &program.nativeFunctionDeclarations().last();
    }

    return nullptr;
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
        auto& firstParameterUnifyNode = (*functionDefinition.parameters()[0]->type())->unifyNode();
        if (is<AST::UnnamedType>(firstParameterUnifyNode)) {
            auto& unnamedType = downcast<AST::UnnamedType>(firstParameterUnifyNode);
            if (is<AST::PointerType>(unnamedType) || is<AST::ArrayReferenceType>(unnamedType) || is<AST::ArrayType>(unnamedType))
                return false;
        }
        if (kind == CheckKind::Index) {
            auto& secondParameterUnifyNode = (*functionDefinition.parameters()[1]->type())->unifyNode();
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
        auto& firstArgumentUnifyNode = (*functionDefinition.parameters()[0]->type())->unifyNode();
        if (is<AST::UnnamedType>(firstArgumentUnifyNode)) {
            auto& unnamedType = downcast<AST::UnnamedType>(firstArgumentUnifyNode);
            if (is<AST::PointerType>(unnamedType) || is<AST::ArrayReferenceType>(unnamedType) || is<AST::ArrayType>(unnamedType))
                return false;
        }
        if (kind == CheckKind::Index) {
            auto& secondParameterUnifyNode = (*functionDefinition.parameters()[1]->type())->unifyNode();
            if (!is<AST::NamedType>(secondParameterUnifyNode))
                return false;
            auto& namedType = downcast<AST::NamedType>(secondParameterUnifyNode);
            if (!is<AST::NativeTypeDeclaration>(namedType))
                return false;
            auto& nativeTypeDeclaration = downcast<AST::NativeTypeDeclaration>(namedType);
            if (!nativeTypeDeclaration.isInt())
                return false;
        }
        if (!matches(functionDefinition.type(), *functionDefinition.parameters()[0]->type()))
            return false;
        auto& valueType = *functionDefinition.parameters()[numExpectedParameters - 1]->type();
        auto getterName = functionDefinition.name().substring(0, functionDefinition.name().length() - 1);
        auto* getterFuncs = nameContext.getFunctions(getterName);
        if (!getterFuncs)
            return false;
        Vector<ResolvingType> argumentTypes;
        Vector<std::reference_wrapper<ResolvingType>> argumentTypeReferences;
        for (size_t i = 0; i < numExpectedParameters - 1; ++i)
            argumentTypes.append((*functionDefinition.parameters()[i]->type())->clone());
        for (auto& argumentType : argumentTypes)
            argumentTypeReferences.append(argumentType);
        auto* overload = resolveFunctionOverload(*getterFuncs, argumentTypeReferences);
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
            auto& unifyNode = (*functionDefinition.parameters()[0]->type())->unifyNode();
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
            && matches(*functionDefinition.parameters()[0]->type(), functionDefinition.type());
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
        || functionDefinition.name() == "operator>>")
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

    virtual ~Checker() = default;

    void visit(Program&) override;

    bool assignTypes();

private:
    bool checkShaderType(const AST::FunctionDefinition&);
    bool isBoolType(ResolvingType&);
    struct RecurseInfo {
        ResolvingType& resolvingType;
        const AST::TypeAnnotation typeAnnotation;
    };
    Optional<RecurseInfo> recurseAndGetInfo(AST::Expression&, bool requiresLeftValue = false);
    Optional<RecurseInfo> getInfo(AST::Expression&, bool requiresLeftValue = false);
    Optional<UniqueRef<AST::UnnamedType>> recurseAndWrapBaseType(AST::PropertyAccessExpression&);
    bool recurseAndRequireBoolType(AST::Expression&);
    void assignType(AST::Expression&, UniqueRef<AST::UnnamedType>&&, AST::TypeAnnotation);
    void assignType(AST::Expression&, RefPtr<ResolvableTypeReference>&&, AST::TypeAnnotation);
    void forwardType(AST::Expression&, ResolvingType&, AST::TypeAnnotation);

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

    void finishVisiting(AST::PropertyAccessExpression&, ResolvingType* additionalArgumentType = nullptr);

    HashMap<AST::Expression*, std::unique_ptr<ResolvingType>> m_typeMap;
    HashSet<String> m_vertexEntryPoints;
    HashSet<String> m_fragmentEntryPoints;
    HashSet<String> m_computeEntryPoints;
    const Intrinsics& m_intrinsics;
    Program& m_program;
    AST::FunctionDefinition* m_currentFunction { nullptr };
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
        auto success = keyValuePair.value->visit(WTF::makeVisitor([&](UniqueRef<AST::UnnamedType>& unnamedType) -> bool {
            keyValuePair.key->setType(unnamedType->clone());
            return true;
        }, [&](RefPtr<ResolvableTypeReference>& resolvableTypeReference) -> bool {
            if (!resolvableTypeReference->resolvableType().maybeResolvedType()) {
                if (!static_cast<bool>(commit(resolvableTypeReference->resolvableType())))
                    return false;
            }
            keyValuePair.key->setType(resolvableTypeReference->resolvableType().resolvedType().clone());
            return true;
        }));
        if (!success)
            return false;
    }

    return true;
}

bool Checker::checkShaderType(const AST::FunctionDefinition& functionDefinition)
{
    switch (*functionDefinition.entryPointType()) {
    case AST::EntryPointType::Vertex:
        return static_cast<bool>(m_vertexEntryPoints.add(functionDefinition.name()));
    case AST::EntryPointType::Fragment:
        return static_cast<bool>(m_fragmentEntryPoints.add(functionDefinition.name()));
    case AST::EntryPointType::Compute:
        return static_cast<bool>(m_computeEntryPoints.add(functionDefinition.name()));
    }
}

void Checker::visit(AST::FunctionDefinition& functionDefinition)
{
    m_currentFunction = &functionDefinition;
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

    Visitor::visit(functionDefinition);
}

static Optional<UniqueRef<AST::UnnamedType>> matchAndCommit(ResolvingType& left, ResolvingType& right)
{
    return left.visit(WTF::makeVisitor([&](UniqueRef<AST::UnnamedType>& left) -> Optional<UniqueRef<AST::UnnamedType>> {
        return right.visit(WTF::makeVisitor([&](UniqueRef<AST::UnnamedType>& right) -> Optional<UniqueRef<AST::UnnamedType>> {
            if (matches(left, right))
                return left->clone();
            return WTF::nullopt;
        }, [&](RefPtr<ResolvableTypeReference>& right) -> Optional<UniqueRef<AST::UnnamedType>> {
            return matchAndCommit(left, right->resolvableType());
        }));
    }, [&](RefPtr<ResolvableTypeReference>& left) -> Optional<UniqueRef<AST::UnnamedType>> {
        return right.visit(WTF::makeVisitor([&](UniqueRef<AST::UnnamedType>& right) -> Optional<UniqueRef<AST::UnnamedType>> {
            return matchAndCommit(right, left->resolvableType());
        }, [&](RefPtr<ResolvableTypeReference>& right) -> Optional<UniqueRef<AST::UnnamedType>> {
            return matchAndCommit(left->resolvableType(), right->resolvableType());
        }));
    }));
}

static Optional<UniqueRef<AST::UnnamedType>> matchAndCommit(ResolvingType& resolvingType, AST::UnnamedType& unnamedType)
{
    return resolvingType.visit(WTF::makeVisitor([&](UniqueRef<AST::UnnamedType>& resolvingType) -> Optional<UniqueRef<AST::UnnamedType>> {
        if (matches(unnamedType, resolvingType))
            return unnamedType.clone();
        return WTF::nullopt;
    }, [&](RefPtr<ResolvableTypeReference>& resolvingType) -> Optional<UniqueRef<AST::UnnamedType>> {
        return matchAndCommit(unnamedType, resolvingType->resolvableType());
    }));
}

static Optional<UniqueRef<AST::UnnamedType>> matchAndCommit(ResolvingType& resolvingType, AST::NamedType& namedType)
{
    return resolvingType.visit(WTF::makeVisitor([&](UniqueRef<AST::UnnamedType>& resolvingType) -> Optional<UniqueRef<AST::UnnamedType>> {
        if (matches(resolvingType, namedType))
            return resolvingType->clone();
        return WTF::nullopt;
    }, [&](RefPtr<ResolvableTypeReference>& resolvingType) -> Optional<UniqueRef<AST::UnnamedType>> {
        return matchAndCommit(namedType, resolvingType->resolvableType());
    }));
}

static Optional<UniqueRef<AST::UnnamedType>> commit(ResolvingType& resolvingType)
{
    return resolvingType.visit(WTF::makeVisitor([&](UniqueRef<AST::UnnamedType>& unnamedType) -> Optional<UniqueRef<AST::UnnamedType>> {
        return unnamedType->clone();
    }, [&](RefPtr<ResolvableTypeReference>& resolvableTypeReference) -> Optional<UniqueRef<AST::UnnamedType>> {
        if (!resolvableTypeReference->resolvableType().maybeResolvedType())
            return commit(resolvableTypeReference->resolvableType());
        return resolvableTypeReference->resolvableType().resolvedType().clone();
    }));
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

    auto matchAndCommitMember = [&](AST::EnumerationMember& member) -> bool {
        return member.value()->visit(WTF::makeVisitor([&](AST::Expression& value) -> bool {
            auto valueInfo = recurseAndGetInfo(value);
            if (!valueInfo)
                return false;
            return static_cast<bool>(matchAndCommit(valueInfo->resolvingType, *baseType));
        }));
    };

    for (auto& member : enumerationMembers) {
        if (!member.get().value())
            continue;

        if (!matchAndCommitMember(member)) {
            setError();
            return;
        }
    }

    int64_t nextValue = 0;
    for (auto& member : enumerationMembers) {
        if (member.get().value()) {
            auto value = member.get().value()->visit(WTF::makeVisitor([&](AST::IntegerLiteral& integerLiteral) -> int64_t {
                return integerLiteral.valueForSelectedType();
            }, [&](AST::UnsignedIntegerLiteral& unsignedIntegerLiteral) -> int64_t {
                return unsignedIntegerLiteral.valueForSelectedType();
            }, [&](auto&) -> int64_t {
                ASSERT_NOT_REACHED();
                return 0;
            }));
            nextValue = baseType->successor()(value);
        } else {
            if (nextValue > std::numeric_limits<int>::max()) {
                ASSERT(nextValue <= std::numeric_limits<unsigned>::max());
                member.get().setValue(AST::ConstantExpression(AST::UnsignedIntegerLiteral(member.get().codeLocation(), static_cast<unsigned>(nextValue))));
            }
            ASSERT(nextValue >= std::numeric_limits<int>::min());
            member.get().setValue(AST::ConstantExpression(AST::IntegerLiteral(member.get().codeLocation(), static_cast<int>(nextValue))));

            if (!matchAndCommitMember(member)) {
                setError();
                return;
            }

            nextValue = baseType->successor()(nextValue);
        }
    }

    auto getValue = [&](AST::EnumerationMember& member) -> int64_t {
        ASSERT(member.value());
        auto value = member.value()->visit(WTF::makeVisitor([&](AST::IntegerLiteral& integerLiteral) -> int64_t {
            return integerLiteral.value();
        }, [&](AST::UnsignedIntegerLiteral& unsignedIntegerLiteral) -> int64_t {
            return unsignedIntegerLiteral.value();
        }, [&](auto&) -> int64_t {
            ASSERT_NOT_REACHED();
            return 0;
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
    ASSERT(typeReference.maybeResolvedType());

    for (auto& typeArgument : typeReference.typeArguments())
        checkErrorAndVisit(typeArgument);
}

auto Checker::recurseAndGetInfo(AST::Expression& expression, bool requiresLeftValue) -> Optional<RecurseInfo>
{
    Visitor::visit(expression);
    if (error())
        return WTF::nullopt;
    return getInfo(expression, requiresLeftValue);
}

auto Checker::getInfo(AST::Expression& expression, bool requiresLeftValue) -> Optional<RecurseInfo>
{
    auto typeIterator = m_typeMap.find(&expression);
    ASSERT(typeIterator != m_typeMap.end());

    const auto& typeAnnotation = expression.typeAnnotation();
    if (requiresLeftValue && typeAnnotation.isRightValue()) {
        setError();
        return WTF::nullopt;
    }
    return {{ *typeIterator->value, typeAnnotation }};
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

void Checker::assignType(AST::Expression& expression, UniqueRef<AST::UnnamedType>&& unnamedType, AST::TypeAnnotation typeAnnotation = AST::RightValue())
{
    auto addResult = m_typeMap.add(&expression, std::make_unique<ResolvingType>(WTFMove(unnamedType)));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    expression.setTypeAnnotation(WTFMove(typeAnnotation));
}

void Checker::assignType(AST::Expression& expression, RefPtr<ResolvableTypeReference>&& resolvableTypeReference, AST::TypeAnnotation typeAnnotation = AST::RightValue())
{
    auto addResult = m_typeMap.add(&expression, std::make_unique<ResolvingType>(WTFMove(resolvableTypeReference)));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    expression.setTypeAnnotation(WTFMove(typeAnnotation));
}

void Checker::forwardType(AST::Expression& expression, ResolvingType& resolvingType, AST::TypeAnnotation typeAnnotation = AST::RightValue())
{
    resolvingType.visit(WTF::makeVisitor([&](UniqueRef<AST::UnnamedType>& result) {
        auto addResult = m_typeMap.add(&expression, std::make_unique<ResolvingType>(result->clone()));
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }, [&](RefPtr<ResolvableTypeReference>& result) {
        auto addResult = m_typeMap.add(&expression, std::make_unique<ResolvingType>(result.copyRef()));
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }));
    expression.setTypeAnnotation(WTFMove(typeAnnotation));
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

void Checker::visit(AST::ReadModifyWriteExpression& readModifyWriteExpression)
{
    auto leftValueInfo = recurseAndGetInfo(readModifyWriteExpression.leftValue(), true);
    if (!leftValueInfo)
        return;

    readModifyWriteExpression.oldValue().setType(leftValueInfo->resolvingType.getUnnamedType()->clone());

    auto newValueInfo = recurseAndGetInfo(readModifyWriteExpression.newValueExpression());
    if (!newValueInfo)
        return;

    if (Optional<UniqueRef<AST::UnnamedType>> matchedType = matchAndCommit(leftValueInfo->resolvingType, newValueInfo->resolvingType))
        readModifyWriteExpression.newValue().setType(WTFMove(matchedType.value()));
    else {
        setError();
        return;
    }

    auto resultInfo = recurseAndGetInfo(readModifyWriteExpression.resultExpression());
    if (!resultInfo)
        return;

    forwardType(readModifyWriteExpression, resultInfo->resolvingType);
}

static AST::UnnamedType* getUnnamedType(ResolvingType& resolvingType)
{
    return resolvingType.visit(WTF::makeVisitor([](UniqueRef<AST::UnnamedType>& type) -> AST::UnnamedType* {
        return &type;
    }, [](RefPtr<ResolvableTypeReference>& type) -> AST::UnnamedType* {
        // FIXME: If the type isn't committed, should we just commit() it now?
        return type->resolvableType().maybeResolvedType();
    }));
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

    assignType(dereferenceExpression, pointerType->elementType().clone(), AST::LeftValue { pointerType->addressSpace() });
}

void Checker::visit(AST::MakePointerExpression& makePointerExpression)
{
    auto leftValueInfo = recurseAndGetInfo(makePointerExpression.leftValue(), true);
    if (!leftValueInfo)
        return;

    auto leftAddressSpace = leftValueInfo->typeAnnotation.leftAddressSpace();
    if (!leftAddressSpace) {
        setError();
        return;
    }

    auto* leftValueType = getUnnamedType(leftValueInfo->resolvingType);
    if (!leftValueType) {
        setError();
        return;
    }

    assignType(makePointerExpression, makeUniqueRef<AST::PointerType>(makePointerExpression.codeLocation(), *leftAddressSpace, leftValueType->clone()));
}

void Checker::visit(AST::MakeArrayReferenceExpression& makeArrayReferenceExpression)
{
    auto leftValueInfo = recurseAndGetInfo(makeArrayReferenceExpression.leftValue());
    if (!leftValueInfo)
        return;

    auto* leftValueType = getUnnamedType(leftValueInfo->resolvingType);
    if (!leftValueType) {
        setError();
        return;
    }

    auto& unifyNode = leftValueType->unifyNode();
    if (is<AST::UnnamedType>(unifyNode)) {
        auto& unnamedType = downcast<AST::UnnamedType>(unifyNode);
        if (is<AST::PointerType>(unnamedType)) {
            auto& pointerType = downcast<AST::PointerType>(unnamedType);
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198163 Save the fact that we're not targetting the item; we're targetting the item's inner element.
            assignType(makeArrayReferenceExpression, makeUniqueRef<AST::ArrayReferenceType>(makeArrayReferenceExpression.codeLocation(), pointerType.addressSpace(), pointerType.elementType().clone()));
            return;
        }

        auto leftAddressSpace = leftValueInfo->typeAnnotation.leftAddressSpace();
        if (!leftAddressSpace) {
            setError();
            return;
        }

        if (is<AST::ArrayType>(unnamedType)) {
            auto& arrayType = downcast<AST::ArrayType>(unnamedType);
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198163 Save the number of elements.
            assignType(makeArrayReferenceExpression, makeUniqueRef<AST::ArrayReferenceType>(makeArrayReferenceExpression.codeLocation(), *leftAddressSpace, arrayType.type().clone()));
            return;
        }
    }

    auto leftAddressSpace = leftValueInfo->typeAnnotation.leftAddressSpace();
    if (!leftAddressSpace) {
        setError();
        return;
    }

    assignType(makeArrayReferenceExpression, makeUniqueRef<AST::ArrayReferenceType>(makeArrayReferenceExpression.codeLocation(), *leftAddressSpace, leftValueType->clone()));
}

static Optional<UniqueRef<AST::UnnamedType>> argumentTypeForAndOverload(AST::UnnamedType& baseType, AST::AddressSpace addressSpace)
{
    auto& unifyNode = baseType.unifyNode();
    if (is<AST::NamedType>(unifyNode)) {
        auto& namedType = downcast<AST::NamedType>(unifyNode);
        return { makeUniqueRef<AST::PointerType>(namedType.codeLocation(), addressSpace, AST::TypeReference::wrap(namedType.codeLocation(), namedType)) };
    }

    auto& unnamedType = downcast<AST::UnnamedType>(unifyNode);

    if (is<AST::ArrayReferenceType>(unnamedType))
        return unnamedType.clone();

    if (is<AST::ArrayType>(unnamedType))
        return { makeUniqueRef<AST::ArrayReferenceType>(unnamedType.codeLocation(), addressSpace, downcast<AST::ArrayType>(unnamedType).type().clone()) };

    if (is<AST::PointerType>(unnamedType))
        return WTF::nullopt;

    return { makeUniqueRef<AST::PointerType>(unnamedType.codeLocation(), addressSpace, unnamedType.clone()) };
}

void Checker::finishVisiting(AST::PropertyAccessExpression& propertyAccessExpression, ResolvingType* additionalArgumentType)
{
    auto baseInfo = recurseAndGetInfo(propertyAccessExpression.base());
    if (!baseInfo)
        return;
    auto baseUnnamedType = commit(baseInfo->resolvingType);
    if (!baseUnnamedType)
        return;

    AST::FunctionDeclaration* getterFunction = nullptr;
    AST::UnnamedType* getterReturnType = nullptr;
    {
        Vector<std::reference_wrapper<ResolvingType>> getterArgumentTypes { baseInfo->resolvingType };
        if (additionalArgumentType)
            getterArgumentTypes.append(*additionalArgumentType);
        auto getterName = propertyAccessExpression.getterFunctionName();
        auto* getterFunctions = m_program.nameContext().getFunctions(getterName);
        getterFunction = resolveFunction(m_program, getterFunctions, getterArgumentTypes, getterName, propertyAccessExpression.codeLocation(), m_intrinsics);
        if (getterFunction)
            getterReturnType = &getterFunction->type();
    }

    AST::FunctionDeclaration* anderFunction = nullptr;
    AST::UnnamedType* anderReturnType = nullptr;
    auto leftAddressSpace = baseInfo->typeAnnotation.leftAddressSpace();
    if (leftAddressSpace) {
        if (auto argumentTypeForAndOverload = WHLSL::argumentTypeForAndOverload(*baseUnnamedType, *leftAddressSpace)) {
            ResolvingType argumentType = { WTFMove(*argumentTypeForAndOverload) };
            Vector<std::reference_wrapper<ResolvingType>> anderArgumentTypes { argumentType };
            if (additionalArgumentType)
                anderArgumentTypes.append(*additionalArgumentType);
            auto anderName = propertyAccessExpression.anderFunctionName();
            auto* anderFunctions = m_program.nameContext().getFunctions(anderName);
            anderFunction = resolveFunction(m_program, anderFunctions, anderArgumentTypes, anderName, propertyAccessExpression.codeLocation(), m_intrinsics);
            if (anderFunction)
                anderReturnType = &downcast<AST::PointerType>(anderFunction->type()).elementType(); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198164 Enforce the return of anders will always be a pointer
        }
    }

    AST::FunctionDeclaration* threadAnderFunction = nullptr;
    AST::UnnamedType* threadAnderReturnType = nullptr;
    if (auto argumentTypeForAndOverload = WHLSL::argumentTypeForAndOverload(*baseUnnamedType, AST::AddressSpace::Thread)) {
        ResolvingType argumentType = { makeUniqueRef<AST::PointerType>(propertyAccessExpression.codeLocation(), AST::AddressSpace::Thread, baseUnnamedType->get().clone()) };
        Vector<std::reference_wrapper<ResolvingType>> threadAnderArgumentTypes { argumentType };
        if (additionalArgumentType)
            threadAnderArgumentTypes.append(*additionalArgumentType);
        auto anderName = propertyAccessExpression.anderFunctionName();
        auto* anderFunctions = m_program.nameContext().getFunctions(anderName);
        threadAnderFunction = resolveFunction(m_program, anderFunctions, threadAnderArgumentTypes, anderName, propertyAccessExpression.codeLocation(), m_intrinsics);
        if (threadAnderFunction)
            threadAnderReturnType = &downcast<AST::PointerType>(threadAnderFunction->type()).elementType(); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198164 Enforce the return of anders will always be a pointer
    }

    if (leftAddressSpace && !anderFunction && !getterFunction) {
        setError();
        return;
    }

    if (!leftAddressSpace && !threadAnderFunction && !getterFunction) {
        setError();
        return;
    }

    if (threadAnderFunction && getterFunction) {
        setError();
        return;
    }

    if (anderFunction && threadAnderFunction && !matches(*anderReturnType, *threadAnderReturnType)) {
        setError();
        return;
    }

    if (getterFunction && anderFunction && !matches(*getterReturnType, *anderReturnType)) {
        setError();
        return;
    }

    if (getterFunction && threadAnderFunction && !matches(*getterReturnType, *threadAnderReturnType)) {
        setError();
        return;
    }

    AST::UnnamedType* fieldType = getterReturnType ? getterReturnType : anderReturnType ? anderReturnType : threadAnderReturnType;

    AST::FunctionDeclaration* setterFunction = nullptr;
    AST::UnnamedType* setterReturnType = nullptr;
    {
        ResolvingType fieldResolvingType(fieldType->clone());
        Vector<std::reference_wrapper<ResolvingType>> setterArgumentTypes { baseInfo->resolvingType };
        if (additionalArgumentType)
            setterArgumentTypes.append(*additionalArgumentType);
        setterArgumentTypes.append(fieldResolvingType);
        auto setterName = propertyAccessExpression.setterFunctionName();
        auto* setterFunctions = m_program.nameContext().getFunctions(setterName);
        setterFunction = resolveFunction(m_program, setterFunctions, setterArgumentTypes, setterName, propertyAccessExpression.codeLocation(), m_intrinsics);
        if (setterFunction)
            setterReturnType = &setterFunction->type();
    }

    if (setterFunction && !getterFunction) {
        setError();
        return;
    }

    propertyAccessExpression.setGetterFunction(getterFunction);
    propertyAccessExpression.setAnderFunction(anderFunction);
    propertyAccessExpression.setThreadAnderFunction(threadAnderFunction);
    propertyAccessExpression.setSetterFunction(setterFunction);

    AST::TypeAnnotation typeAnnotation = AST::RightValue();
    if (auto leftAddressSpace = baseInfo->typeAnnotation.leftAddressSpace()) {
        if (anderFunction)
            typeAnnotation = AST::LeftValue { *leftAddressSpace };
        else if (setterFunction)
            typeAnnotation = AST::AbstractLeftValue();
    } else if (!baseInfo->typeAnnotation.isRightValue() && (setterFunction || threadAnderFunction))
        typeAnnotation = AST::AbstractLeftValue();
    assignType(propertyAccessExpression, fieldType->clone(), WTFMove(typeAnnotation));
}

void Checker::visit(AST::DotExpression& dotExpression)
{
    finishVisiting(dotExpression);
}

void Checker::visit(AST::IndexExpression& indexExpression)
{
    auto baseInfo = recurseAndGetInfo(indexExpression.indexExpression());
    if (!baseInfo)
        return;
    finishVisiting(indexExpression, &baseInfo->resolvingType);
}

void Checker::visit(AST::VariableReference& variableReference)
{
    ASSERT(variableReference.variable());
    ASSERT(variableReference.variable()->type());
    
    assignType(variableReference, variableReference.variable()->type()->clone(), AST::LeftValue { AST::AddressSpace::Thread });
}

void Checker::visit(AST::Return& returnStatement)
{
    if (returnStatement.value()) {
        auto valueInfo = recurseAndGetInfo(*returnStatement.value());
        if (!valueInfo)
            return;
        if (!matchAndCommit(valueInfo->resolvingType, m_currentFunction->type()))
            setError();
        return;
    }

    if (!matches(m_currentFunction->type(), m_intrinsics.voidType()))
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
    assignType(booleanLiteral, AST::TypeReference::wrap(booleanLiteral.codeLocation(), m_intrinsics.boolType()));
}

void Checker::visit(AST::EnumerationMemberLiteral& enumerationMemberLiteral)
{
    ASSERT(enumerationMemberLiteral.enumerationDefinition());
    auto& enumerationDefinition = *enumerationMemberLiteral.enumerationDefinition();
    assignType(enumerationMemberLiteral, AST::TypeReference::wrap(enumerationMemberLiteral.codeLocation(), enumerationDefinition));
}

bool Checker::isBoolType(ResolvingType& resolvingType)
{
    return resolvingType.visit(WTF::makeVisitor([&](UniqueRef<AST::UnnamedType>& left) -> bool {
        return matches(left, m_intrinsics.boolType());
    }, [&](RefPtr<ResolvableTypeReference>& left) -> bool {
        return static_cast<bool>(matchAndCommit(m_intrinsics.boolType(), left->resolvableType()));
    }));
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
    assignType(logicalNotExpression, AST::TypeReference::wrap(logicalNotExpression.codeLocation(), m_intrinsics.boolType()));
}

void Checker::visit(AST::LogicalExpression& logicalExpression)
{
    if (!recurseAndRequireBoolType(logicalExpression.left()))
        return;
    if (!recurseAndRequireBoolType(logicalExpression.right()))
        return;
    assignType(logicalExpression, AST::TypeReference::wrap(logicalExpression.codeLocation(), m_intrinsics.boolType()));
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
    WTF::visit(WTF::makeVisitor([&](UniqueRef<AST::Statement>& statement) {
        checkErrorAndVisit(statement);
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
        auto success = switchCase.value()->visit(WTF::makeVisitor([&](AST::IntegerLiteral& integerLiteral) -> bool {
            return static_cast<bool>(matchAndCommit(*valueType, integerLiteral.type()));
        }, [&](AST::UnsignedIntegerLiteral& unsignedIntegerLiteral) -> bool {
            return static_cast<bool>(matchAndCommit(*valueType, unsignedIntegerLiteral.type()));
        }, [&](AST::FloatLiteral& floatLiteral) -> bool {
            return static_cast<bool>(matchAndCommit(*valueType, floatLiteral.type()));
        }, [&](AST::NullLiteral& nullLiteral) -> bool {
            return static_cast<bool>(matchAndCommit(*valueType, nullLiteral.type()));
        }, [&](AST::BooleanLiteral&) -> bool {
            return matches(*valueType, m_intrinsics.boolType());
        }, [&](AST::EnumerationMemberLiteral& enumerationMemberLiteral) -> bool {
            ASSERT(enumerationMemberLiteral.enumerationDefinition());
            return matches(*valueType, *enumerationMemberLiteral.enumerationDefinition());
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

            auto success = firstCase.value()->visit(WTF::makeVisitor([&](AST::IntegerLiteral& firstIntegerLiteral) -> bool {
                return secondCase.value()->visit(WTF::makeVisitor([&](AST::IntegerLiteral& secondIntegerLiteral) -> bool {
                    return firstIntegerLiteral.value() != secondIntegerLiteral.value();
                }, [&](AST::UnsignedIntegerLiteral& secondUnsignedIntegerLiteral) -> bool {
                    return static_cast<int64_t>(firstIntegerLiteral.value()) != static_cast<int64_t>(secondUnsignedIntegerLiteral.value());
                }, [](auto&) -> bool {
                    return true;
                }));
            }, [&](AST::UnsignedIntegerLiteral& firstUnsignedIntegerLiteral) -> bool {
                return secondCase.value()->visit(WTF::makeVisitor([&](AST::IntegerLiteral& secondIntegerLiteral) -> bool {
                    return static_cast<int64_t>(firstUnsignedIntegerLiteral.value()) != static_cast<int64_t>(secondIntegerLiteral.value());
                }, [&](AST::UnsignedIntegerLiteral& secondUnsignedIntegerLiteral) -> bool {
                    return firstUnsignedIntegerLiteral.value() != secondUnsignedIntegerLiteral.value();
                }, [](auto&) -> bool {
                    return true;
                }));
            }, [&](AST::EnumerationMemberLiteral& firstEnumerationMemberLiteral) -> bool {
                return secondCase.value()->visit(WTF::makeVisitor([&](AST::EnumerationMemberLiteral& secondEnumerationMemberLiteral) -> bool {
                    ASSERT(firstEnumerationMemberLiteral.enumerationMember());
                    ASSERT(secondEnumerationMemberLiteral.enumerationMember());
                    return firstEnumerationMemberLiteral.enumerationMember() != secondEnumerationMemberLiteral.enumerationMember();
                }, [](auto&) -> bool {
                    return true;
                }));
            }, [](auto&) -> bool {
                return true;
            }));
            if (!success) {
                setError();
                return;
            }
        }
    }

    if (!hasDefault) {
        if (is<AST::NativeTypeDeclaration>(*valueType)) {
            HashSet<int64_t> values;
            bool zeroValueExists;
            for (auto& switchCase : switchStatement.switchCases()) {
                auto value = switchCase.value()->visit(WTF::makeVisitor([&](AST::IntegerLiteral& integerLiteral) -> int64_t {
                    return integerLiteral.valueForSelectedType();
                }, [&](AST::UnsignedIntegerLiteral& unsignedIntegerLiteral) -> int64_t {
                    return unsignedIntegerLiteral.valueForSelectedType();
                }, [](auto&) -> int64_t {
                    ASSERT_NOT_REACHED();
                    return 0;
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
    // Don't recurse on the castReturnType, because it's guaranteed to be a NamedType, which will get visited later.
    // We don't want to recurse to the same node twice.

    NameContext& nameContext = m_program.nameContext();
    auto* functions = nameContext.getFunctions(callExpression.name());
    if (!functions) {
        if (auto* types = nameContext.getTypes(callExpression.name())) {
            if (types->size() == 1) {
                if ((functions = nameContext.getFunctions("operator cast"_str)))
                    callExpression.setCastData((*types)[0].get());
            }
        }
    }
    if (!functions) {
        setError();
        return;
    }

    auto* function = resolveFunction(m_program, functions, types, callExpression.name(), callExpression.codeLocation(), m_intrinsics, callExpression.castReturnType());
    if (!function) {
        setError();
        return;
    }

    for (size_t i = 0; i < function->parameters().size(); ++i) {
        if (!matchAndCommit(types[i].get(), *function->parameters()[i]->type())) {
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
