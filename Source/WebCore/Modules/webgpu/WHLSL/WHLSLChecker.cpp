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

class AndOverloadTypeKey {
public:
    AndOverloadTypeKey() = default;
    AndOverloadTypeKey(WTF::HashTableDeletedValueType)
    {
        m_type = bitwise_cast<AST::UnnamedType*>(static_cast<uintptr_t>(1));
    }

    AndOverloadTypeKey(AST::UnnamedType& type, AST::AddressSpace addressSpace)
        : m_type(&type)
        , m_addressSpace(addressSpace)
    { }

    bool isEmptyValue() const { return !m_type; }
    bool isHashTableDeletedValue() const { return m_type == bitwise_cast<AST::UnnamedType*>(static_cast<uintptr_t>(1)); }

    unsigned hash() const
    {
        return IntHash<uint8_t>::hash(static_cast<uint8_t>(m_addressSpace)) ^ m_type->hash();
    }

    bool operator==(const AndOverloadTypeKey& other) const
    {
        return m_addressSpace == other.m_addressSpace
            && *m_type == *other.m_type;
    }

    struct Hash {
        static unsigned hash(const AndOverloadTypeKey& key)
        {
            return key.hash();
        }

        static bool equal(const AndOverloadTypeKey& a, const AndOverloadTypeKey& b)
        {
            return a == b;
        }

        static const bool safeToCompareToEmptyOrDeleted = false;
    };

    struct Traits : public WTF::SimpleClassHashTraits<AndOverloadTypeKey> {
        static const bool hasIsEmptyValueFunction = true;
        static bool isEmptyValue(const AndOverloadTypeKey& key) { return key.isEmptyValue(); }
    };

private:
    AST::UnnamedType* m_type { nullptr };
    AST::AddressSpace m_addressSpace;
};

static AST::NativeFunctionDeclaration resolveWithOperatorAnderIndexer(CodeLocation location, AST::ArrayReferenceType& firstArgument, const Intrinsics& intrinsics)
{
    const bool isOperator = true;
    auto returnType = AST::PointerType::create(location, firstArgument.addressSpace(), firstArgument.elementType());
    AST::VariableDeclarations parameters;
    parameters.append(makeUniqueRef<AST::VariableDeclaration>(location, AST::Qualifiers(), &firstArgument, String(), nullptr, nullptr));
    parameters.append(makeUniqueRef<AST::VariableDeclaration>(location, AST::Qualifiers(), AST::TypeReference::wrap(location, intrinsics.uintType()), String(), nullptr, nullptr));
    return AST::NativeFunctionDeclaration(AST::FunctionDeclaration(location, AST::AttributeBlock(), WTF::nullopt, WTFMove(returnType), String("operator&[]", String::ConstructFromLiteral), WTFMove(parameters), nullptr, isOperator, ParsingMode::StandardLibrary));
}

static AST::NativeFunctionDeclaration resolveWithOperatorLength(CodeLocation location, AST::UnnamedType& firstArgument, const Intrinsics& intrinsics)
{
    const bool isOperator = true;
    auto returnType = AST::TypeReference::wrap(location, intrinsics.uintType());
    AST::VariableDeclarations parameters;
    parameters.append(makeUniqueRef<AST::VariableDeclaration>(location, AST::Qualifiers(), &firstArgument, String(), nullptr, nullptr));
    return AST::NativeFunctionDeclaration(AST::FunctionDeclaration(location, AST::AttributeBlock(), WTF::nullopt, WTFMove(returnType), String("operator.length", String::ConstructFromLiteral), WTFMove(parameters), nullptr, isOperator, ParsingMode::StandardLibrary));
}

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
    Maybe,
    No
};

static Optional<AST::NativeFunctionDeclaration> resolveByInstantiation(const String& name, CodeLocation location, const Vector<std::reference_wrapper<ResolvingType>>& types, const Intrinsics& intrinsics)
{
    if (name == "operator&[]" && types.size() == 2) {
        auto* firstArgumentArrayRef = types[0].get().visit(WTF::makeVisitor([](Ref<AST::UnnamedType>& unnamedType) -> AST::ArrayReferenceType* {
            if (is<AST::ArrayReferenceType>(static_cast<AST::UnnamedType&>(unnamedType)))
                return &downcast<AST::ArrayReferenceType>(static_cast<AST::UnnamedType&>(unnamedType));
            return nullptr;
        }, [](RefPtr<ResolvableTypeReference>&) -> AST::ArrayReferenceType* {
            return nullptr;
        }));
        bool secondArgumentIsUint = types[1].get().visit(WTF::makeVisitor([&](Ref<AST::UnnamedType>& unnamedType) -> bool {
            return matches(unnamedType, intrinsics.uintType());
        }, [&](RefPtr<ResolvableTypeReference>& resolvableTypeReference) -> bool {
            return resolvableTypeReference->resolvableType().canResolve(intrinsics.uintType());
        }));
        if (firstArgumentArrayRef && secondArgumentIsUint)
            return resolveWithOperatorAnderIndexer(location, *firstArgumentArrayRef, intrinsics);
    } else if (name == "operator.length" && types.size() == 1) {
        auto* firstArgumentReference = types[0].get().visit(WTF::makeVisitor([](Ref<AST::UnnamedType>& unnamedType) -> AST::UnnamedType* {
            if (is<AST::ArrayReferenceType>(static_cast<AST::UnnamedType&>(unnamedType)) || is<AST::ArrayType>(static_cast<AST::UnnamedType&>(unnamedType)))
                return unnamedType.ptr();
            return nullptr;
        }, [](RefPtr<ResolvableTypeReference>&) -> AST::UnnamedType* {
            return nullptr;
        }));
        if (firstArgumentReference)
            return resolveWithOperatorLength(location, *firstArgumentReference, intrinsics);
    } else if (name == "operator==" && types.size() == 2) {
        auto acceptability = [](ResolvingType& resolvingType) -> Acceptability {
            return resolvingType.visit(WTF::makeVisitor([](Ref<AST::UnnamedType>& unnamedType) -> Acceptability {
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

static bool checkOperatorOverload(const AST::FunctionDefinition& functionDefinition, NameContext& nameContext, AST::NameSpace currentNameSpace)
{
    enum class CheckKind {
        Index,
        Dot
    };

    auto checkGetter = [&](CheckKind kind) -> bool {
        size_t numExpectedParameters = kind == CheckKind::Index ? 2 : 1;
        if (functionDefinition.parameters().size() != numExpectedParameters)
            return false;
        auto& firstParameterUnifyNode = functionDefinition.parameters()[0]->type()->unifyNode();
        if (is<AST::UnnamedType>(firstParameterUnifyNode)) {
            auto& unnamedType = downcast<AST::UnnamedType>(firstParameterUnifyNode);
            if (is<AST::PointerType>(unnamedType) || is<AST::ArrayReferenceType>(unnamedType) || is<AST::ArrayType>(unnamedType))
                return false;
        }
        if (kind == CheckKind::Index) {
            auto& secondParameterUnifyNode = functionDefinition.parameters()[1]->type()->unifyNode();
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
        auto& firstArgumentUnifyNode = functionDefinition.parameters()[0]->type()->unifyNode();
        if (is<AST::UnnamedType>(firstArgumentUnifyNode)) {
            auto& unnamedType = downcast<AST::UnnamedType>(firstArgumentUnifyNode);
            if (is<AST::PointerType>(unnamedType) || is<AST::ArrayReferenceType>(unnamedType) || is<AST::ArrayType>(unnamedType))
                return false;
        }
        if (kind == CheckKind::Index) {
            auto& secondParameterUnifyNode = functionDefinition.parameters()[1]->type()->unifyNode();
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
        auto getterFuncs = nameContext.getFunctions(getterName, currentNameSpace);
        Vector<ResolvingType> argumentTypes;
        Vector<std::reference_wrapper<ResolvingType>> argumentTypeReferences;
        for (size_t i = 0; i < numExpectedParameters - 1; ++i)
            argumentTypes.append(*functionDefinition.parameters()[i]->type());
        for (auto& argumentType : argumentTypes)
            argumentTypeReferences.append(argumentType);
        auto* overload = resolveFunctionOverload(getterFuncs, argumentTypeReferences, currentNameSpace);
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
            auto& unifyNode = functionDefinition.parameters()[0]->type()->unifyNode();
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
    void assignType(AST::Expression&, RefPtr<ResolvableTypeReference>, AST::TypeAnnotation);
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

    AST::FunctionDeclaration* resolveFunction(Vector<std::reference_wrapper<ResolvingType>>& types, const String& name, CodeLocation, AST::NamedType* castReturnType = nullptr);

    RefPtr<AST::UnnamedType> argumentTypeForAndOverload(AST::UnnamedType& baseType, AST::AddressSpace);

    AST::UnnamedType& wrappedFloatType()
    {
        if (!m_wrappedFloatType)
            m_wrappedFloatType = AST::TypeReference::wrap({ }, m_intrinsics.floatType());
        return *m_wrappedFloatType;
    }

    AST::UnnamedType& genericPointerType()
    {
        if (!m_genericPointerType)
            m_genericPointerType = AST::PointerType::create({ }, AST::AddressSpace::Thread, AST::TypeReference::wrap({ }, m_intrinsics.floatType()));
        return *m_genericPointerType;
    }

    AST::UnnamedType& normalizedTypeForFunctionKey(AST::UnnamedType& type)
    {
        auto* unifyNode = &type.unifyNode();
        if (unifyNode == &m_intrinsics.uintType() || unifyNode == &m_intrinsics.intType())
            return wrappedFloatType();

        if (is<AST::ReferenceType>(type))
            return genericPointerType();

        return type;
    }

    RefPtr<AST::TypeReference> m_wrappedFloatType;
    RefPtr<AST::UnnamedType> m_genericPointerType;
    HashMap<AST::Expression*, std::unique_ptr<ResolvingType>> m_typeMap;
    HashSet<String> m_vertexEntryPoints[AST::nameSpaceCount];
    HashSet<String> m_fragmentEntryPoints[AST::nameSpaceCount];
    HashSet<String> m_computeEntryPoints[AST::nameSpaceCount];
    const Intrinsics& m_intrinsics;
    Program& m_program;
    AST::FunctionDefinition* m_currentFunction { nullptr };
    HashMap<FunctionKey, Vector<std::reference_wrapper<AST::FunctionDeclaration>, 1>, FunctionKey::Hash, FunctionKey::Traits> m_functions;
    HashMap<AndOverloadTypeKey, RefPtr<AST::UnnamedType>, AndOverloadTypeKey::Hash, AndOverloadTypeKey::Traits> m_andOverloadTypeMap;
    AST::NameSpace m_currentNameSpace { AST::NameSpace::StandardLibrary };
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
    if (!checkOperatorOverload(functionDefinition, m_program.nameContext(), m_currentNameSpace)) {
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

            if (resolvableTypeReference->resolvableType().isNullLiteralType())
                return &genericPointerType();

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
    }
}

void Checker::assignConcreteType(AST::Expression& expression, Ref<AST::UnnamedType> unnamedType, AST::TypeAnnotation typeAnnotation = AST::RightValue())
{
    auto addResult = m_typeMap.add(&expression, makeUnique<ResolvingType>(WTFMove(unnamedType)));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    expression.setTypeAnnotation(WTFMove(typeAnnotation));
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

    forwardType(readModifyWriteExpression, resultInfo->resolvingType);
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
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198163 Save the fact that we're not targetting the item; we're targetting the item's inner element.
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

RefPtr<AST::UnnamedType> Checker::argumentTypeForAndOverload(AST::UnnamedType& baseType, AST::AddressSpace addressSpace)
{
    AndOverloadTypeKey key { baseType, addressSpace };
    {
        auto iter = m_andOverloadTypeMap.find(key);
        if (iter != m_andOverloadTypeMap.end())
            return iter->value;
    }

    auto createArgumentType = [&] () -> RefPtr<AST::UnnamedType> {
        auto& unifyNode = baseType.unifyNode();
        if (is<AST::NamedType>(unifyNode)) {
            auto& namedType = downcast<AST::NamedType>(unifyNode);
            return { AST::PointerType::create(namedType.codeLocation(), addressSpace, AST::TypeReference::wrap(namedType.codeLocation(), namedType)) };
        }

        auto& unnamedType = downcast<AST::UnnamedType>(unifyNode);

        if (is<AST::ArrayReferenceType>(unnamedType))
            return &unnamedType;

        if (is<AST::ArrayType>(unnamedType))
            return { AST::ArrayReferenceType::create(unnamedType.codeLocation(), addressSpace, downcast<AST::ArrayType>(unnamedType).type()) };

        if (is<AST::PointerType>(unnamedType))
            return nullptr;

        return { AST::PointerType::create(unnamedType.codeLocation(), addressSpace, unnamedType) };
    };

    auto result = createArgumentType();
    m_andOverloadTypeMap.add(key, result);
    return result;
}

void Checker::finishVisiting(AST::PropertyAccessExpression& propertyAccessExpression, ResolvingType* additionalArgumentType)
{
    auto baseInfo = recurseAndGetInfo(propertyAccessExpression.base());
    if (!baseInfo)
        return;
    auto baseUnnamedType = commit(baseInfo->resolvingType);
    if (!baseUnnamedType) {
        setError(Error("Cannot resolve the type of the base of a property access expression.", propertyAccessExpression.codeLocation()));
        return;
    }

    AST::FunctionDeclaration* getterFunction = nullptr;
    RefPtr<AST::UnnamedType> getterReturnType = nullptr;
    {
        Vector<std::reference_wrapper<ResolvingType>> getterArgumentTypes { baseInfo->resolvingType };
        if (additionalArgumentType)
            getterArgumentTypes.append(*additionalArgumentType);
        auto getterName = propertyAccessExpression.getterFunctionName();
        getterFunction = resolveFunction(getterArgumentTypes, getterName, propertyAccessExpression.codeLocation());
        if (hasError())
            return;
        if (getterFunction)
            getterReturnType = &getterFunction->type();
    }

    AST::FunctionDeclaration* anderFunction = nullptr;
    RefPtr<AST::UnnamedType> anderReturnType = nullptr;
    auto leftAddressSpace = baseInfo->typeAnnotation.leftAddressSpace();
    if (leftAddressSpace) {
        if (auto argumentTypeForAndOverload = this->argumentTypeForAndOverload(*baseUnnamedType, *leftAddressSpace)) {
            ResolvingType argumentType = { Ref<AST::UnnamedType>(*argumentTypeForAndOverload) };
            Vector<std::reference_wrapper<ResolvingType>> anderArgumentTypes { argumentType };
            if (additionalArgumentType)
                anderArgumentTypes.append(*additionalArgumentType);
            auto anderName = propertyAccessExpression.anderFunctionName();
            anderFunction = resolveFunction(anderArgumentTypes, anderName, propertyAccessExpression.codeLocation());
            if (hasError())
                return;
            if (anderFunction)
                anderReturnType = &downcast<AST::PointerType>(anderFunction->type()).elementType(); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198164 Enforce the return of anders will always be a pointer
        }
    }

    AST::FunctionDeclaration* threadAnderFunction = nullptr;
    RefPtr<AST::UnnamedType> threadAnderReturnType = nullptr;
    if (auto argumentTypeForAndOverload = this->argumentTypeForAndOverload(*baseUnnamedType, AST::AddressSpace::Thread)) {
        ResolvingType argumentType = { Ref<AST::UnnamedType>(AST::PointerType::create(propertyAccessExpression.codeLocation(), AST::AddressSpace::Thread, *baseUnnamedType)) };
        Vector<std::reference_wrapper<ResolvingType>> threadAnderArgumentTypes { argumentType };
        if (additionalArgumentType)
            threadAnderArgumentTypes.append(*additionalArgumentType);
        auto anderName = propertyAccessExpression.anderFunctionName();
        threadAnderFunction = resolveFunction(threadAnderArgumentTypes, anderName, propertyAccessExpression.codeLocation());
        if (hasError())
            return;
        if (threadAnderFunction)
            threadAnderReturnType = &downcast<AST::PointerType>(threadAnderFunction->type()).elementType(); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198164 Enforce the return of anders will always be a pointer
    }

    if (leftAddressSpace && !anderFunction && !getterFunction) {
        setError(Error("Property access instruction must either have an ander or a getter.", propertyAccessExpression.codeLocation()));
        return;
    }

    if (!leftAddressSpace && !threadAnderFunction && !getterFunction) {
        setError(Error("Property access instruction must either have a thread ander or a getter.", propertyAccessExpression.codeLocation()));
        return;
    }

    if (threadAnderFunction && getterFunction) {
        setError(Error("Cannot have both a thread ander and a getter.", propertyAccessExpression.codeLocation()));
        return;
    }

    if (anderFunction && threadAnderFunction && !matches(*anderReturnType, *threadAnderReturnType)) {
        setError(Error("Return type of ander must match the return type of the thread ander.", propertyAccessExpression.codeLocation()));
        return;
    }

    if (getterFunction && anderFunction && !matches(*getterReturnType, *anderReturnType)) {
        setError(Error("Return type of ander must match the return type of the getter.", propertyAccessExpression.codeLocation()));
        return;
    }

    if (getterFunction && threadAnderFunction && !matches(*getterReturnType, *threadAnderReturnType)) {
        setError(Error("Return type of the thread ander must match the return type of the getter.", propertyAccessExpression.codeLocation()));
        return;
    }

    Ref<AST::UnnamedType> fieldType = getterReturnType ? *getterReturnType : anderReturnType ? *anderReturnType : *threadAnderReturnType;

    AST::FunctionDeclaration* setterFunction = nullptr;
    AST::UnnamedType* setterReturnType = nullptr;
    {
        ResolvingType fieldResolvingType(fieldType.copyRef());
        Vector<std::reference_wrapper<ResolvingType>> setterArgumentTypes { baseInfo->resolvingType };
        if (additionalArgumentType)
            setterArgumentTypes.append(*additionalArgumentType);
        setterArgumentTypes.append(fieldResolvingType);
        auto setterName = propertyAccessExpression.setterFunctionName();
        setterFunction = resolveFunction(setterArgumentTypes, setterName, propertyAccessExpression.codeLocation());
        if (hasError())
            return;
        if (setterFunction)
            setterReturnType = &setterFunction->type();
    }

    if (setterFunction && !getterFunction) {
        setError(Error("Cannot define a setter function without a corresponding getter.", propertyAccessExpression.codeLocation()));
        return;
    }

    propertyAccessExpression.setGetterFunction(getterFunction);
    propertyAccessExpression.setAnderFunction(anderFunction);
    propertyAccessExpression.setThreadAnderFunction(threadAnderFunction);
    propertyAccessExpression.setSetterFunction(setterFunction);

    AST::TypeAnnotation typeAnnotation = AST::RightValue();
    if (auto leftAddressSpace = baseInfo->typeAnnotation.leftAddressSpace()) {
        if (anderFunction)
            typeAnnotation = AST::LeftValue { downcast<AST::ReferenceType>(anderFunction->type()).addressSpace() };
        else if (setterFunction)
            typeAnnotation = AST::AbstractLeftValue();
    } else if (!baseInfo->typeAnnotation.isRightValue() && (setterFunction || threadAnderFunction))
        typeAnnotation = AST::AbstractLeftValue();
    assignConcreteType(propertyAccessExpression, WTFMove(fieldType), WTFMove(typeAnnotation));
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

void Checker::visit(AST::NullLiteral& nullLiteral)
{
    assignType(nullLiteral, adoptRef(*new ResolvableTypeReference(nullLiteral.type())));
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
        }, [&](AST::NullLiteral& nullLiteral) -> bool {
            return static_cast<bool>(matchAndCommit(*valueType, nullLiteral.type()));
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
