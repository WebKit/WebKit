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
            setError(Error("Use of native type is not a POD in entrypoint semantic.", nativeTypeDeclaration.codeLocation()));
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

    void visit(AST::PointerType& pointerType) override
    {
        setError(Error("Illegal use of pointer in entrypoint semantic.", pointerType.codeLocation()));
    }

    void visit(AST::ArrayReferenceType& arrayReferenceType) override
    {
        setError(Error("Illegal use of array reference in entrypoint semantic.", arrayReferenceType.codeLocation()));
    }

    void visit(AST::TypeReference& typeReference) override
    {
        checkErrorAndVisit(typeReference.resolvedType());
    }
};

class FunctionKey {
public:
    FunctionKey() = default;
    FunctionKey(WTF::HashTableDeletedValueType)
    {
        m_castReturnType = bitwise_cast<AST::NamedType*>(static_cast<uintptr_t>(1));
    }

    FunctionKey(String name, Vector<std::reference_wrapper<AST::UnnamedType>> types, AST::NamedType* castReturnType = nullptr)
        : m_name(WTFMove(name))
        , m_types(WTFMove(types))
        , m_castReturnType(castReturnType)
    { }

    bool isEmptyValue() const { return m_name.isNull(); }
    bool isHashTableDeletedValue() const { return m_castReturnType == bitwise_cast<AST::NamedType*>(static_cast<uintptr_t>(1)); }

    unsigned hash() const
    {
        unsigned hash = IntHash<size_t>::hash(m_types.size());
        hash ^= m_name.hash();
        for (size_t i = 0; i < m_types.size(); ++i)
            hash ^= m_types[i].get().hash() + i;

        if (m_castReturnType)
            hash ^= WTF::PtrHash<AST::Type*>::hash(&m_castReturnType->unifyNode());

        return hash;
    }

    bool operator==(const FunctionKey& other) const
    {
        if (m_types.size() != other.m_types.size())
            return false;

        if (m_name != other.m_name)
            return false;

        for (size_t i = 0; i < m_types.size(); ++i) {
            if (!matches(m_types[i].get(), other.m_types[i].get()))
                return false;
        }

        if (static_cast<bool>(m_castReturnType) != static_cast<bool>(other.m_castReturnType))
            return false;

        if (!m_castReturnType)
            return true;

        if (&m_castReturnType->unifyNode() == &other.m_castReturnType->unifyNode())
            return true;

        return false;
    }

    struct Hash {
        static unsigned hash(const FunctionKey& key)
        {
            return key.hash();
        }

        static bool equal(const FunctionKey& a, const FunctionKey& b)
        {
            return a == b;
        }

        static const bool safeToCompareToEmptyOrDeleted = false;
        static const bool emptyValueIsZero = false;
    };

    struct Traits : public WTF::SimpleClassHashTraits<FunctionKey> {
        static const bool hasIsEmptyValueFunction = true;
        static bool isEmptyValue(const FunctionKey& key) { return key.isEmptyValue(); }
    };

private:
    String m_name;
    Vector<std::reference_wrapper<AST::UnnamedType>> m_types;
    AST::NamedType* m_castReturnType;
};

static AST::NativeFunctionDeclaration resolveWithReferenceComparator(CodeLocation location, ResolvingType& firstArgument, ResolvingType& secondArgument, const Intrinsics& intrinsics)
{
    const bool isOperator = true;
    auto returnType = AST::TypeReference::wrap(location, intrinsics.boolType());
    auto argumentType = firstArgument.visit(WTF::makeVisitor([](Ref<AST::UnnamedType>& unnamedType) -> Ref<AST::UnnamedType> {
        return unnamedType.copyRef();
    }, [&](RefPtr<ResolvableTypeReference>&) -> Ref<AST::UnnamedType> {
        return secondArgument.visit(WTF::makeVisitor([](Ref<AST::UnnamedType>& unnamedType) -> Ref<AST::UnnamedType> {
            return unnamedType.copyRef();
        }, [&](RefPtr<ResolvableTypeReference>&) -> Ref<AST::UnnamedType> {
            // We encountered "null == null".
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198162 This can probably be generalized, using the "preferred type" infrastructure used by generic literals
            ASSERT_NOT_REACHED();
            return AST::TypeReference::wrap(location, intrinsics.intType());
        }));
    }));
    AST::VariableDeclarations parameters;
    parameters.append(makeUniqueRef<AST::VariableDeclaration>(location, AST::Qualifiers(), argumentType.copyRef(), String(), nullptr, nullptr));
    parameters.append(makeUniqueRef<AST::VariableDeclaration>(location, AST::Qualifiers(), WTFMove(argumentType), String(), nullptr, nullptr));
    return AST::NativeFunctionDeclaration(AST::FunctionDeclaration(location, AST::AttributeBlock(), WTF::nullopt, WTFMove(returnType), String("operator==", String::ConstructFromLiteral), WTFMove(parameters), nullptr, isOperator, ParsingMode::StandardLibrary));
}

enum class Acceptability {
    Yes,
    No
};

static Optional<AST::NativeFunctionDeclaration> resolveByInstantiation(const String& name, CodeLocation location, const Vector<std::reference_wrapper<ResolvingType>>& types, const Intrinsics& intrinsics)
{
    if (name == "operator==" && types.size() == 2) {
        auto acceptability = [](ResolvingType& resolvingType) -> Acceptability {
            return resolvingType.visit(WTF::makeVisitor([](Ref<AST::UnnamedType>& unnamedType) -> Acceptability {
                auto& unifyNode = unnamedType->unifyNode();
                return is<AST::UnnamedType>(unifyNode) && is<AST::ReferenceType>(downcast<AST::UnnamedType>(unifyNode)) ? Acceptability::Yes : Acceptability::No;
            }, [](RefPtr<ResolvableTypeReference>&) -> Acceptability {
                return Acceptability::No;
            }));
        };
        auto leftAcceptability = acceptability(types[0].get());
        auto rightAcceptability = acceptability(types[1].get());
        bool success = false;
        if (leftAcceptability == Acceptability::Yes && rightAcceptability == Acceptability::Yes) {
            auto& unnamedType1 = *types[0].get().getUnnamedType();
            auto& unnamedType2 = *types[1].get().getUnnamedType();
            success = matches(unnamedType1, unnamedType2);
        } 
        if (success)
            return resolveWithReferenceComparator(location, types[0].get(), types[1].get(), intrinsics);
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
                if (podChecker.hasError())
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

static bool checkOperatorOverload(const AST::FunctionDefinition& functionDefinition)
{
    enum class CheckKind {
        Index,
        Dot
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
    return false;
}

class Checker : public Visitor {
public:
    Checker(const Intrinsics& intrinsics, Program& program)
        : m_intrinsics(intrinsics)
        , m_program(program)
    {
        auto addFunction = [&] (AST::FunctionDeclaration& function) {
            AST::NamedType* castReturnType = nullptr;
            if (function.isCast() && is<AST::NamedType>(function.type().unifyNode()))
                castReturnType = &downcast<AST::NamedType>(function.type().unifyNode());

            Vector<std::reference_wrapper<AST::UnnamedType>> types;
            types.reserveInitialCapacity(function.parameters().size());

            for (auto& param : function.parameters())
                types.uncheckedAppend(normalizedTypeForFunctionKey(*param->type()));

            auto addResult = m_functions.add(FunctionKey { function.name(), WTFMove(types), castReturnType }, Vector<std::reference_wrapper<AST::FunctionDeclaration>, 1>());
            addResult.iterator->value.append(function);
        };

        for (auto& function : m_program.functionDefinitions())
            addFunction(function.get());
        for (auto& function : m_program.nativeFunctionDeclarations())
            addFunction(function.get());
    }

    virtual ~Checker() = default;

    void visit(Program&) override;

    Expected<void, Error> assignTypes();

private:
    bool checkShaderType(const AST::FunctionDefinition&);
    bool isBoolType(ResolvingType&);
    struct RecurseInfo {
        ResolvingType& resolvingType;
        const AST::TypeAnnotation typeAnnotation;
    };
    Optional<RecurseInfo> recurseAndGetInfo(AST::Expression&, bool requiresLeftValue = false);
    Optional<RecurseInfo> getInfo(AST::Expression&, bool requiresLeftValue = false);
    RefPtr<AST::UnnamedType> recurseAndWrapBaseType(AST::PropertyAccessExpression&);
    bool recurseAndRequireBoolType(AST::Expression&);
    void assignConcreteType(AST::Expression&, Ref<AST::UnnamedType>, AST::TypeAnnotation);
    void assignConcreteType(AST::Expression&, AST::NamedType&, AST::TypeAnnotation);
    void assignType(AST::Expression&, RefPtr<ResolvableTypeReference>, AST::TypeAnnotation);
    void forwardType(AST::Expression&, ResolvingType&, AST::TypeAnnotation);

    void visit(AST::FunctionDefinition&) override;
    void visit(AST::FunctionDeclaration&) override;
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

    AST::FunctionDeclaration* resolveFunction(Vector<std::reference_wrapper<ResolvingType>>& types, const String& name, CodeLocation, AST::NamedType* castReturnType = nullptr);

    AST::UnnamedType& wrappedFloatType()
    {
        if (!m_wrappedFloatType)
            m_wrappedFloatType = AST::TypeReference::wrap({ }, m_intrinsics.floatType());
        return *m_wrappedFloatType;
    }

    AST::UnnamedType& wrappedUintType()
    {
        if (!m_wrappedUintType)
            m_wrappedUintType = AST::TypeReference::wrap({ }, m_intrinsics.uintType());
        return *m_wrappedUintType;
    }

    AST::UnnamedType& normalizedTypeForFunctionKey(AST::UnnamedType& type)
    {
        auto* unifyNode = &type.unifyNode();
        if (unifyNode == &m_intrinsics.uintType() || unifyNode == &m_intrinsics.intType())
            return wrappedFloatType();

        return type;
    }

    RefPtr<AST::TypeReference> m_wrappedFloatType;
    RefPtr<AST::TypeReference> m_wrappedUintType;
    HashMap<AST::Expression*, std::unique_ptr<ResolvingType>> m_typeMap;
    HashSet<String> m_vertexEntryPoints[AST::nameSpaceCount];
    HashSet<String> m_fragmentEntryPoints[AST::nameSpaceCount];
    HashSet<String> m_computeEntryPoints[AST::nameSpaceCount];
    const Intrinsics& m_intrinsics;
    Program& m_program;
    AST::FunctionDefinition* m_currentFunction { nullptr };
    HashMap<FunctionKey, Vector<std::reference_wrapper<AST::FunctionDeclaration>, 1>, FunctionKey::Hash, FunctionKey::Traits> m_functions;
    AST::NameSpace m_currentNameSpace { AST::NameSpace::StandardLibrary };
    bool m_isVisitingParameters { false };
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

Expected<void, Error> Checker::assignTypes()
{
    for (auto& keyValuePair : m_typeMap) {
        auto success = keyValuePair.value->visit(WTF::makeVisitor([&](Ref<AST::UnnamedType>& unnamedType) -> bool {
            keyValuePair.key->setType(unnamedType.copyRef());
            return true;
        }, [&](RefPtr<ResolvableTypeReference>& resolvableTypeReference) -> bool {
            if (!resolvableTypeReference->resolvableType().maybeResolvedType()) {
                if (!static_cast<bool>(commit(resolvableTypeReference->resolvableType())))
                    return false;
            }
            keyValuePair.key->setType(resolvableTypeReference->resolvableType().resolvedType());
            return true;
        }));
        if (!success)
            return makeUnexpected(Error("Could not resolve the type of a constant."));
    }

    return { };
}

bool Checker::checkShaderType(const AST::FunctionDefinition& functionDefinition)
{
    auto index = static_cast<unsigned>(m_currentNameSpace);
    switch (*functionDefinition.entryPointType()) {
    case AST::EntryPointType::Vertex:
        return static_cast<bool>(m_vertexEntryPoints[index].add(functionDefinition.name()));
    case AST::EntryPointType::Fragment:
        return static_cast<bool>(m_fragmentEntryPoints[index].add(functionDefinition.name()));
    case AST::EntryPointType::Compute:
        return static_cast<bool>(m_computeEntryPoints[index].add(functionDefinition.name()));
    }
}

void Checker::visit(AST::FunctionDeclaration& functionDeclaration)
{
    m_isVisitingParameters = true;
    Visitor::visit(functionDeclaration);
    m_isVisitingParameters = false;
}

void Checker::visit(AST::FunctionDefinition& functionDefinition)
{
    m_currentNameSpace = functionDefinition.nameSpace();
    m_currentFunction = &functionDefinition;
    if (functionDefinition.entryPointType()) {
        if (!checkShaderType(functionDefinition)) {
            setError(Error("Duplicate entrypoint function.", functionDefinition.codeLocation()));
            return;
        }
        auto entryPointItems = gatherEntryPointItems(m_intrinsics, functionDefinition);
        if (!entryPointItems) {
            setError(entryPointItems.error());
            return;
        }
        if (!checkSemantics(entryPointItems->inputs, entryPointItems->outputs, functionDefinition.entryPointType(), m_intrinsics)) {
            setError(Error("Bad semantics for entrypoint.", functionDefinition.codeLocation()));
            return;
        }
    }
    if (!checkOperatorOverload(functionDefinition)) {
        setError(Error("Operator does not match expected signature.", functionDefinition.codeLocation()));
        return;
    }

    Visitor::visit(functionDefinition);
}

static RefPtr<AST::UnnamedType> matchAndCommit(ResolvingType& left, ResolvingType& right)
{
    return left.visit(WTF::makeVisitor([&](Ref<AST::UnnamedType>& left) -> RefPtr<AST::UnnamedType> {
        return right.visit(WTF::makeVisitor([&](Ref<AST::UnnamedType>& right) -> RefPtr<AST::UnnamedType> {
            if (matches(left, right))
                return left.copyRef();
            return nullptr;
        }, [&](RefPtr<ResolvableTypeReference>& right) -> RefPtr<AST::UnnamedType> {
            return matchAndCommit(left, right->resolvableType());
        }));
    }, [&](RefPtr<ResolvableTypeReference>& left) -> RefPtr<AST::UnnamedType> {
        return right.visit(WTF::makeVisitor([&](Ref<AST::UnnamedType>& right) -> RefPtr<AST::UnnamedType> {
            return matchAndCommit(right, left->resolvableType());
        }, [&](RefPtr<ResolvableTypeReference>& right) -> RefPtr<AST::UnnamedType> {
            return matchAndCommit(left->resolvableType(), right->resolvableType());
        }));
    }));
}

static RefPtr<AST::UnnamedType> matchAndCommit(ResolvingType& resolvingType, AST::UnnamedType& unnamedType)
{
    return resolvingType.visit(WTF::makeVisitor([&](Ref<AST::UnnamedType>& resolvingType) -> RefPtr<AST::UnnamedType> {
        if (matches(unnamedType, resolvingType))
            return &unnamedType;
        return nullptr;
    }, [&](RefPtr<ResolvableTypeReference>& resolvingType) -> RefPtr<AST::UnnamedType> {
        return matchAndCommit(unnamedType, resolvingType->resolvableType());
    }));
}

static bool matchAndCommit(ResolvingType& resolvingType, AST::NamedType& namedType)
{
    return resolvingType.visit(WTF::makeVisitor([&](Ref<AST::UnnamedType>& resolvingType) {
        if (matches(resolvingType, namedType))
            return true;
        return false;
    }, [&](RefPtr<ResolvableTypeReference>& resolvingType) -> bool {
        return matchAndCommit(namedType, resolvingType->resolvableType());
    }));
}

static RefPtr<AST::UnnamedType> commit(ResolvingType& resolvingType)
{
    return resolvingType.visit(WTF::makeVisitor([&](Ref<AST::UnnamedType>& unnamedType) -> RefPtr<AST::UnnamedType> {
        return unnamedType.copyRef();
    }, [&](RefPtr<ResolvableTypeReference>& resolvableTypeReference) -> RefPtr<AST::UnnamedType> {
        if (!resolvableTypeReference->resolvableType().maybeResolvedType())
            return commit(resolvableTypeReference->resolvableType());
        return &resolvableTypeReference->resolvableType().resolvedType();
    }));
}

AST::FunctionDeclaration* Checker::resolveFunction(Vector<std::reference_wrapper<ResolvingType>>& types, const String& name, CodeLocation location, AST::NamedType* castReturnType)
{
    Vector<std::reference_wrapper<AST::UnnamedType>> unnamedTypes;
    unnamedTypes.reserveInitialCapacity(types.size());

    for (auto resolvingType : types) {
        AST::UnnamedType* type = resolvingType.get().visit(WTF::makeVisitor([&](Ref<AST::UnnamedType>& unnamedType) -> AST::UnnamedType* {
            return unnamedType.ptr();
        }, [&](RefPtr<ResolvableTypeReference>& resolvableTypeReference) -> AST::UnnamedType* {
            if (resolvableTypeReference->resolvableType().maybeResolvedType())
                return &resolvableTypeReference->resolvableType().resolvedType();

            if (resolvableTypeReference->resolvableType().isFloatLiteralType()
                || resolvableTypeReference->resolvableType().isIntegerLiteralType()
                || resolvableTypeReference->resolvableType().isUnsignedIntegerLiteralType())
                return &wrappedFloatType();

            return commit(resolvableTypeReference->resolvableType()).get();
        }));

        if (!type) {
            setError(Error("Could not resolve the type of a constant."));
            return nullptr;
        }

        unnamedTypes.uncheckedAppend(normalizedTypeForFunctionKey(*type));
    }

    {
        auto iter = m_functions.find(FunctionKey { name, WTFMove(unnamedTypes), castReturnType });
        if (iter != m_functions.end()) {
            if (AST::FunctionDeclaration* function = resolveFunctionOverload(iter->value, types, castReturnType, m_currentNameSpace))
                return function;
        }
    }

    if (auto newFunction = resolveByInstantiation(name, location, types, m_intrinsics)) {
        m_program.append(WTFMove(*newFunction));
        return &m_program.nativeFunctionDeclarations().last();
    }

    return nullptr;
}

void Checker::visit(AST::EnumerationDefinition& enumerationDefinition)
{
    bool isSigned;
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
        isSigned = nativeTypeDeclaration.isSigned();
        return &nativeTypeDeclaration;
    })();
    if (!baseType) {
        setError(Error("Invalid base type for enum.", enumerationDefinition.codeLocation()));
        return;
    }

    auto enumerationMembers = enumerationDefinition.enumerationMembers();

    for (auto& member : enumerationMembers) {
        int64_t value = member.get().value();
        if (isSigned) {
            if (static_cast<int64_t>(static_cast<int32_t>(value)) != value) {
                setError(Error("Invalid enumeration value.", member.get().codeLocation()));
                return;
            }
        } else {
            if (static_cast<int64_t>(static_cast<uint32_t>(value)) != value) {
                setError(Error("Invalid enumeration value.", member.get().codeLocation()));
                return;
            }
        }
    }

    for (size_t i = 0; i < enumerationMembers.size(); ++i) {
        auto value = enumerationMembers[i].get().value();
        for (size_t j = i + 1; j < enumerationMembers.size(); ++j) {
            auto otherValue = enumerationMembers[j].get().value();
            if (value == otherValue) {
                setError(Error("Cannot declare duplicate enumeration values.", enumerationMembers[j].get().codeLocation()));
                return;
            }
        }
    }

    bool foundZero = false;
    for (auto& member : enumerationMembers) {
        if (!member.get().value()) {
            foundZero = true;
            break;
        }
    }
    if (!foundZero) {
        setError(Error("enum definition must contain a zero value.", enumerationDefinition.codeLocation()));
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
    if (hasError())
        return WTF::nullopt;
    return getInfo(expression, requiresLeftValue);
}

auto Checker::getInfo(AST::Expression& expression, bool requiresLeftValue) -> Optional<RecurseInfo>
{
    auto typeIterator = m_typeMap.find(&expression);
    ASSERT(typeIterator != m_typeMap.end());

    const auto& typeAnnotation = expression.typeAnnotation();
    if (requiresLeftValue && typeAnnotation.isRightValue()) {
        setError(Error("Unexpected rvalue.", expression.codeLocation()));
        return WTF::nullopt;
    }
    return {{ *typeIterator->value, typeAnnotation }};
}

void Checker::visit(AST::VariableDeclaration& variableDeclaration)
{
    // ReadModifyWriteExpressions are the only place where anonymous variables exist,
    // and that doesn't recurse on the anonymous variables, so we can assume the variable has a type.
    checkErrorAndVisit(*variableDeclaration.type());
    if (matches(*variableDeclaration.type(), m_intrinsics.voidType())) {
        setError(Error("Variables can't have void type.", variableDeclaration.codeLocation()));
        return;
    }
    if (variableDeclaration.initializer()) {
        auto& lhsType = *variableDeclaration.type();
        auto initializerInfo = recurseAndGetInfo(*variableDeclaration.initializer());
        if (!initializerInfo)
            return;
        if (!matchAndCommit(initializerInfo->resolvingType, lhsType)) {
            setError(Error("Declared variable type does not match its initializer's type.", variableDeclaration.codeLocation()));
            return;
        }
    } else if (!m_isVisitingParameters && is<AST::ReferenceType>(variableDeclaration.type()->unifyNode())) {
        if (is<AST::PointerType>(variableDeclaration.type()->unifyNode()))
            setError(Error("Must assign to a pointer variable declaration in its initializer.", variableDeclaration.codeLocation()));
        else {
            ASSERT(is<AST::ArrayReferenceType>(variableDeclaration.type()->unifyNode()));
            setError(Error("Must assign to an array reference variable declaration in its initializer.", variableDeclaration.codeLocation()));
        }
        return;
    }
}

void Checker::assignConcreteType(AST::Expression& expression, Ref<AST::UnnamedType> unnamedType, AST::TypeAnnotation typeAnnotation = AST::RightValue())
{
    auto addResult = m_typeMap.add(&expression, makeUnique<ResolvingType>(WTFMove(unnamedType)));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    expression.setTypeAnnotation(WTFMove(typeAnnotation));
}

void Checker::assignConcreteType(AST::Expression& expression, AST::NamedType& type, AST::TypeAnnotation annotation)
{
    auto unnamedType = AST::TypeReference::wrap(type.codeLocation(), type);
    Visitor::visit(unnamedType);
    assignConcreteType(expression, WTFMove(unnamedType), annotation);
}

void Checker::assignType(AST::Expression& expression, RefPtr<ResolvableTypeReference> resolvableTypeReference, AST::TypeAnnotation typeAnnotation = AST::RightValue())
{
    auto addResult = m_typeMap.add(&expression, makeUnique<ResolvingType>(WTFMove(resolvableTypeReference)));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    expression.setTypeAnnotation(WTFMove(typeAnnotation));
}

void Checker::forwardType(AST::Expression& expression, ResolvingType& resolvingType, AST::TypeAnnotation typeAnnotation = AST::RightValue())
{
    resolvingType.visit(WTF::makeVisitor([&](Ref<AST::UnnamedType>& result) {
        auto addResult = m_typeMap.add(&expression, makeUnique<ResolvingType>(result.copyRef()));
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }, [&](RefPtr<ResolvableTypeReference>& result) {
        auto addResult = m_typeMap.add(&expression, makeUnique<ResolvingType>(result.copyRef()));
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
        setError(Error("Left hand side of assignment does not match the type of the right hand side.", assignmentExpression.codeLocation()));
        return;
    }

    assignConcreteType(assignmentExpression, *resultType);
}

void Checker::visit(AST::ReadModifyWriteExpression& readModifyWriteExpression)
{
    auto leftValueInfo = recurseAndGetInfo(readModifyWriteExpression.leftValue(), true);
    if (!leftValueInfo)
        return;

    readModifyWriteExpression.oldValue().setType(*leftValueInfo->resolvingType.getUnnamedType());

    auto newValueInfo = recurseAndGetInfo(readModifyWriteExpression.newValueExpression());
    if (!newValueInfo)
        return;

    if (RefPtr<AST::UnnamedType> matchedType = matchAndCommit(leftValueInfo->resolvingType, newValueInfo->resolvingType))
        readModifyWriteExpression.newValue().setType(*matchedType);
    else {
        setError(Error("Base of the read-modify-write expression does not match the type of the new value.", readModifyWriteExpression.codeLocation()));
        return;
    }

    auto resultInfo = recurseAndGetInfo(readModifyWriteExpression.resultExpression());
    if (!resultInfo)
        return;

    forwardType(readModifyWriteExpression, resultInfo->resolvingType, AST::RightValue());
}

static AST::UnnamedType* getUnnamedType(ResolvingType& resolvingType)
{
    return resolvingType.visit(WTF::makeVisitor([](Ref<AST::UnnamedType>& type) -> AST::UnnamedType* {
        return type.ptr();
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
        setError(Error("Cannot dereference a non-pointer type.", dereferenceExpression.codeLocation()));
        return;
    }

    assignConcreteType(dereferenceExpression, pointerType->elementType(), AST::LeftValue { pointerType->addressSpace() });
}

void Checker::visit(AST::MakePointerExpression& makePointerExpression)
{
    auto leftValueInfo = recurseAndGetInfo(makePointerExpression.leftValue(), true);
    if (!leftValueInfo)
        return;

    auto leftAddressSpace = leftValueInfo->typeAnnotation.leftAddressSpace();
    if (!leftAddressSpace) {
        setError(Error("Cannot take the address of a non lvalue.", makePointerExpression.codeLocation()));
        return;
    }

    auto* leftValueType = getUnnamedType(leftValueInfo->resolvingType);
    if (!leftValueType) {
        setError(Error("Cannot take the address of a value without a type.", makePointerExpression.codeLocation()));
        return;
    }

    assignConcreteType(makePointerExpression, AST::PointerType::create(makePointerExpression.codeLocation(), *leftAddressSpace, *leftValueType));
}

void Checker::visit(AST::MakeArrayReferenceExpression& makeArrayReferenceExpression)
{
    auto leftValueInfo = recurseAndGetInfo(makeArrayReferenceExpression.leftValue());
    if (!leftValueInfo)
        return;

    auto* leftValueType = getUnnamedType(leftValueInfo->resolvingType);
    if (!leftValueType) {
        setError(Error("Cannot make an array reference of a value without a type.", makeArrayReferenceExpression.codeLocation()));
        return;
    }

    auto& unifyNode = leftValueType->unifyNode();
    if (is<AST::UnnamedType>(unifyNode)) {
        auto& unnamedType = downcast<AST::UnnamedType>(unifyNode);
        if (is<AST::PointerType>(unnamedType)) {
            auto& pointerType = downcast<AST::PointerType>(unnamedType);
            assignConcreteType(makeArrayReferenceExpression, AST::ArrayReferenceType::create(makeArrayReferenceExpression.codeLocation(), pointerType.addressSpace(), pointerType.elementType()));
            return;
        }

        auto leftAddressSpace = leftValueInfo->typeAnnotation.leftAddressSpace();
        if (!leftAddressSpace) {
            setError(Error("Cannot make an array reference from a non-left-value.", makeArrayReferenceExpression.codeLocation()));
            return;
        }

        if (is<AST::ArrayType>(unnamedType)) {
            auto& arrayType = downcast<AST::ArrayType>(unnamedType);
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198163 Save the number of elements.
            assignConcreteType(makeArrayReferenceExpression, AST::ArrayReferenceType::create(makeArrayReferenceExpression.codeLocation(), *leftAddressSpace, arrayType.type()));
            return;
        }
    }

    auto leftAddressSpace = leftValueInfo->typeAnnotation.leftAddressSpace();
    if (!leftAddressSpace) {
        setError(Error("Cannot make an array reference from a non-left-value.", makeArrayReferenceExpression.codeLocation()));
        return;
    }

    assignConcreteType(makeArrayReferenceExpression, AST::ArrayReferenceType::create(makeArrayReferenceExpression.codeLocation(), *leftAddressSpace, *leftValueType));
}

void Checker::visit(AST::DotExpression& dotExpression)
{
    auto baseInfo = recurseAndGetInfo(dotExpression.base());
    if (!baseInfo)
        return;

    auto baseUnnamedType = commit(baseInfo->resolvingType);
    if (!baseUnnamedType) {
        setError(Error("Cannot resolve the type of the base of a dot expression.", dotExpression.codeLocation()));
        return;
    }

    auto& type = baseUnnamedType->unifyNode();
    if (is<AST::StructureDefinition>(type)) {
        auto& structure = downcast<AST::StructureDefinition>(type);    
        if (AST::StructureElement* element = structure.find(dotExpression.fieldName()))
            assignConcreteType(dotExpression, element->type(), baseInfo->typeAnnotation);
        else {
            setError(Error(makeString("Field name: '", dotExpression.fieldName(), "' does not exist on structure: ", structure.name()), dotExpression.codeLocation()));
            return;
        }
    } else if (dotExpression.fieldName() == "length") {
        if (is<AST::ArrayReferenceType>(type)
            || is<AST::ArrayType>(type)
            || (is<AST::NativeTypeDeclaration>(type) && downcast<AST::NativeTypeDeclaration>(type).isVector())) {
            assignConcreteType(dotExpression, wrappedUintType(), AST::RightValue());
        } else {
            setError(Error(".length field is only available on arrays, array references, or vectors.", dotExpression.codeLocation()));
            return;
        }
    } else if (is<AST::NativeTypeDeclaration>(type) && downcast<AST::NativeTypeDeclaration>(type).isVector()) {
        if (!m_program.isValidVectorProperty(dotExpression.fieldName())) {
            setError(Error(makeString("'.", dotExpression.fieldName(), "' is not a valid property on a vector."), dotExpression.codeLocation()));
            return;
        }

        auto typeAnnotation = baseInfo->typeAnnotation.isRightValue() ? AST::TypeAnnotation { AST::RightValue() } : AST::TypeAnnotation { AST::AbstractLeftValue() };

        size_t fieldLength = dotExpression.fieldName().length();
        auto& innerType = downcast<AST::NativeTypeDeclaration>(type).vectorTypeArgument();
        if (fieldLength == 1)
            assignConcreteType(dotExpression, innerType, typeAnnotation);
        else {
            if (matches(innerType, m_intrinsics.boolType()))
                assignConcreteType(dotExpression, m_intrinsics.boolVectorTypeForSize(fieldLength), typeAnnotation);
            else if (matches(innerType, m_intrinsics.intType()))
                assignConcreteType(dotExpression, m_intrinsics.intVectorTypeForSize(fieldLength), typeAnnotation);
            else if (matches(innerType, m_intrinsics.uintType()))
                assignConcreteType(dotExpression, m_intrinsics.uintVectorTypeForSize(fieldLength), typeAnnotation);
            else if (matches(innerType, m_intrinsics.floatType()))
                assignConcreteType(dotExpression, m_intrinsics.floatVectorTypeForSize(fieldLength), typeAnnotation);
            else
                RELEASE_ASSERT_NOT_REACHED();
        }
    } else
        setError(Error("Base value of dot expression must be a structure, array, or vector.", dotExpression.codeLocation()));
}

void Checker::visit(AST::IndexExpression& indexExpression)
{
    {
        auto indexInfo = recurseAndGetInfo(indexExpression.indexExpression());
        if (!indexInfo)
            return;

        if (!matchAndCommit(indexInfo->resolvingType, m_intrinsics.uintType())) {
            setError(Error("Index in an index expression must be a uint.", indexExpression.codeLocation()));
            return;
        }
    }

    auto baseInfo = recurseAndGetInfo(indexExpression.base());
    if (!baseInfo)
        return;

    auto baseUnnamedType = commit(baseInfo->resolvingType);
    if (!baseUnnamedType) {
        setError(Error("Cannot resolve the type of the base of an index expression.", indexExpression.codeLocation()));
        return;
    }

    auto& type = baseUnnamedType->unifyNode();
    if (is<AST::ArrayReferenceType>(type)) {
        auto& arrayReferenceType = downcast<AST::ArrayReferenceType>(type);
        assignConcreteType(indexExpression, arrayReferenceType.elementType(), AST::LeftValue { arrayReferenceType.addressSpace() });
    } else if (is<AST::ArrayType>(type))
        assignConcreteType(indexExpression, downcast<AST::ArrayType>(type).type(), baseInfo->typeAnnotation);
    else if (is<AST::NativeTypeDeclaration>(type)) {
        auto& nativeType = downcast<AST::NativeTypeDeclaration>(type);
        auto typeAnnotation = baseInfo->typeAnnotation.isRightValue() ? AST::TypeAnnotation { AST::RightValue() } : AST::TypeAnnotation { AST::AbstractLeftValue() };
        if (nativeType.isVector())
            assignConcreteType(indexExpression, nativeType.vectorTypeArgument(), typeAnnotation);
        else if (nativeType.isMatrix()) {
            auto& innerType = nativeType.matrixTypeArgument();
            unsigned numRows = nativeType.numberOfMatrixRows();
            if (matches(innerType, m_intrinsics.boolType()))
                assignConcreteType(indexExpression, m_intrinsics.boolVectorTypeForSize(numRows), typeAnnotation);
            else if (matches(innerType, m_intrinsics.floatType()))
                assignConcreteType(indexExpression, m_intrinsics.floatVectorTypeForSize(numRows), typeAnnotation);
            else
                RELEASE_ASSERT_NOT_REACHED();
        } else {
            setError(Error("Index expression on unknown type.", indexExpression.codeLocation()));
            return;
        }
    } else {
        setError(Error("Index expression on an unknown base type. Base type must be an array, array reference, vector, or matrix.", indexExpression.codeLocation()));
        return;
    }
}

void Checker::visit(AST::VariableReference& variableReference)
{
    ASSERT(variableReference.variable());
    ASSERT(variableReference.variable()->type());
    
    assignConcreteType(variableReference, *variableReference.variable()->type(), AST::LeftValue { AST::AddressSpace::Thread });
}

void Checker::visit(AST::Return& returnStatement)
{
    if (returnStatement.value()) {
        auto valueInfo = recurseAndGetInfo(*returnStatement.value());
        if (!valueInfo)
            return;
        if (!matchAndCommit(valueInfo->resolvingType, m_currentFunction->type()))
            setError(Error("Type of the return value must match the return type of the function.", returnStatement.codeLocation()));
        return;
    }

    if (!matches(m_currentFunction->type(), m_intrinsics.voidType()))
        setError(Error("Cannot return a value from a void function.", returnStatement.codeLocation()));
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

void Checker::visit(AST::BooleanLiteral& booleanLiteral)
{
    assignConcreteType(booleanLiteral, AST::TypeReference::wrap(booleanLiteral.codeLocation(), m_intrinsics.boolType()));
}

void Checker::visit(AST::EnumerationMemberLiteral& enumerationMemberLiteral)
{
    ASSERT(enumerationMemberLiteral.enumerationDefinition());
    auto& enumerationDefinition = *enumerationMemberLiteral.enumerationDefinition();
    assignConcreteType(enumerationMemberLiteral, AST::TypeReference::wrap(enumerationMemberLiteral.codeLocation(), enumerationDefinition));
}

bool Checker::isBoolType(ResolvingType& resolvingType)
{
    return resolvingType.visit(WTF::makeVisitor([&](Ref<AST::UnnamedType>& left) -> bool {
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
        setError(Error("Expected bool type from expression.", expression.codeLocation()));
        return false;
    }
    return true;
}

void Checker::visit(AST::LogicalNotExpression& logicalNotExpression)
{
    if (!recurseAndRequireBoolType(logicalNotExpression.operand()))
        return;
    assignConcreteType(logicalNotExpression, AST::TypeReference::wrap(logicalNotExpression.codeLocation(), m_intrinsics.boolType()));
}

void Checker::visit(AST::LogicalExpression& logicalExpression)
{
    if (!recurseAndRequireBoolType(logicalExpression.left()))
        return;
    if (!recurseAndRequireBoolType(logicalExpression.right()))
        return;
    assignConcreteType(logicalExpression, AST::TypeReference::wrap(logicalExpression.codeLocation(), m_intrinsics.boolType()));
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
    checkErrorAndVisit(forLoop.initialization());
    if (hasError())
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
        setError(Error("Invalid type for the expression condition of the switch statement.", switchStatement.codeLocation()));
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
        }, [&](AST::BooleanLiteral&) -> bool {
            return matches(*valueType, m_intrinsics.boolType());
        }, [&](AST::EnumerationMemberLiteral& enumerationMemberLiteral) -> bool {
            ASSERT(enumerationMemberLiteral.enumerationDefinition());
            return matches(*valueType, *enumerationMemberLiteral.enumerationDefinition());
        }));
        if (!success) {
            setError(Error("Invalid type for switch case.", switchCase.codeLocation()));
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
                setError(Error("Cannot define multiple default cases in switch statement.", secondCase.codeLocation()));
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
                setError(Error("Cannot define duplicate case statements in a switch.", secondCase.codeLocation()));
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
                setError(Error("Switch cases must be exhaustive or you must define a default case.", switchStatement.codeLocation()));
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
                    setError(Error("Switch cases over an enum must be exhaustive or you must define a default case.", switchStatement.codeLocation()));
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
    if (hasError())
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
        setError(Error("lhs and rhs of a ternary expression must match.", ternaryExpression.codeLocation()));
        return;
    }

    assignConcreteType(ternaryExpression, *resultType);
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

    auto* function = resolveFunction(types, callExpression.name(), callExpression.codeLocation());
    if (hasError())
        return;

    if (!function) {
        NameContext& nameContext = m_program.nameContext();
        auto castTypes = nameContext.getTypes(callExpression.name(), m_currentNameSpace);
        if (castTypes.size() == 1) {
            AST::NamedType& castType = castTypes[0].get();
            function = resolveFunction(types, "operator cast"_str, callExpression.codeLocation(), &castType);
            if (hasError())
                return;
            if (function)
                callExpression.setCastData(castType);
        }
    }

    if (!function) {
        // FIXME: Add better error messages for why we can't resolve to one of the overrides.
        // https://bugs.webkit.org/show_bug.cgi?id=200133
        setError(Error("Cannot resolve function call to a concrete callee. Make sure you are using compatible types.", callExpression.codeLocation()));
        return;
    }

    for (size_t i = 0; i < function->parameters().size(); ++i) {
        if (!matchAndCommit(types[i].get(), *function->parameters()[i]->type())) {
            setError(Error(makeString("Invalid type for parameter number ", i + 1, " in function call."), callExpression.codeLocation()));
            return;
        }
    }

    callExpression.setFunction(*function);

    assignConcreteType(callExpression, function->type());
}

Expected<void, Error> check(Program& program)
{
    Checker checker(program.intrinsics(), program);
    checker.checkErrorAndVisit(program);
    if (checker.hasError())
        return checker.result();
    return checker.assignTypes();
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
