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
#include "WHLSLPropertyResolver.h"

#if ENABLE(WEBGPU)

#include "WHLSLArrayType.h"
#include "WHLSLAssignmentExpression.h"
#include "WHLSLCallExpression.h"
#include "WHLSLCommaExpression.h"
#include "WHLSLDereferenceExpression.h"
#include "WHLSLDotExpression.h"
#include "WHLSLFunctionDeclaration.h"
#include "WHLSLFunctionDefinition.h"
#include "WHLSLIndexExpression.h"
#include "WHLSLMakeArrayReferenceExpression.h"
#include "WHLSLMakePointerExpression.h"
#include "WHLSLPointerType.h"
#include "WHLSLReadModifyWriteExpression.h"
#include "WHLSLReplaceWith.h"
#include "WHLSLVariableDeclaration.h"
#include "WHLSLVariableReference.h"
#include "WHLSLVisitor.h"

namespace WebCore {

namespace WHLSL {

class PropertyResolver : public Visitor {
public:
private:
    void visit(AST::FunctionDefinition&) override;
    void visit(AST::DotExpression&) override;
    void visit(AST::IndexExpression&) override;
    void visit(AST::AssignmentExpression&) override;
    void visit(AST::ReadModifyWriteExpression&) override;

    void simplifyRightValue(AST::PropertyAccessExpression&);
    bool simplifyAbstractLeftValue(AST::AssignmentExpression&, AST::DotExpression&, UniqueRef<AST::Expression>&& right);
    void simplifyLeftValue(AST::Expression&);

    AST::VariableDeclarations m_variableDeclarations;
};

void PropertyResolver::visit(AST::DotExpression& dotExpression)
{
    // Unless we're inside an AssignmentExpression or a ReadModifyWriteExpression, we're a right value.
    simplifyRightValue(dotExpression);
}

void PropertyResolver::visit(AST::IndexExpression& indexExpression)
{
    checkErrorAndVisit(indexExpression.indexExpression());
    // Unless we're inside an AssignmentExpression or a ReadModifyWriteExpression, we're a right value.
    simplifyRightValue(indexExpression);
}

void PropertyResolver::visit(AST::FunctionDefinition& functionDefinition)
{
    Visitor::visit(functionDefinition);
    if (!m_variableDeclarations.isEmpty())
        functionDefinition.block().statements().insert(0, makeUniqueRef<AST::VariableDeclarationsStatement>(functionDefinition.codeLocation(), WTFMove(m_variableDeclarations)));
}

enum class WhichAnder {
    ThreadAnder,
    Ander
};

struct AnderCallArgumentResult {
    UniqueRef<AST::Expression> expression;
    Optional<UniqueRef<AST::VariableDeclaration>> variableDeclaration;
    WhichAnder whichAnder;
};

template <typename ExpressionConstructor, typename TypeConstructor>
static Optional<AnderCallArgumentResult> wrapAnderCallArgument(UniqueRef<AST::Expression>& expression, Ref<AST::UnnamedType> baseType, AST::FunctionDeclaration* anderFunction, AST::FunctionDeclaration* threadAnderFunction)
{
    auto functionEscapeMode = [] (AST::FunctionDeclaration& functionDeclaration) {
        if (functionDeclaration.isNativeFunctionDeclaration() || functionDeclaration.parsingMode() == ParsingMode::StandardLibrary)
            return AST::AddressEscapeMode::DoesNotEscape;
        return AST::AddressEscapeMode::Escapes;
    };

    auto location = expression->codeLocation();
    if (auto addressSpace = expression->typeAnnotation().leftAddressSpace()) {
        if (!anderFunction)
            return WTF::nullopt;
        auto makeArrayReference = makeUniqueRef<ExpressionConstructor>(location, WTFMove(expression), functionEscapeMode(*anderFunction));
        makeArrayReference->setType(TypeConstructor::create(location, *addressSpace, WTFMove(baseType)));
        makeArrayReference->setTypeAnnotation(AST::RightValue());
        return {{ WTFMove(makeArrayReference), WTF::nullopt, WhichAnder::Ander }};
    }
    if (threadAnderFunction) {
        auto variableDeclaration = makeUniqueRef<AST::VariableDeclaration>(location, AST::Qualifiers(), baseType.copyRef(), String(), nullptr, nullptr);

        auto variableReference1 = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(variableDeclaration));
        variableReference1->setType(baseType.copyRef());
        variableReference1->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread });

        auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(location, WTFMove(variableReference1), WTFMove(expression));
        assignmentExpression->setType(baseType.copyRef());
        assignmentExpression->setTypeAnnotation(AST::RightValue());

        auto variableReference2 = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(variableDeclaration));
        variableReference2->setType(baseType.copyRef());
        variableReference2->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread });

        auto expression = makeUniqueRef<ExpressionConstructor>(location, WTFMove(variableReference2), functionEscapeMode(*threadAnderFunction));
        auto resultType = TypeConstructor::create(location, AST::AddressSpace::Thread, WTFMove(baseType));
        expression->setType(resultType.copyRef());
        expression->setTypeAnnotation(AST::RightValue());

        Vector<UniqueRef<AST::Expression>> expressions;
        expressions.append(WTFMove(assignmentExpression));
        expressions.append(WTFMove(expression));
        auto commaExpression = makeUniqueRef<AST::CommaExpression>(location, WTFMove(expressions));
        commaExpression->setType(WTFMove(resultType));
        commaExpression->setTypeAnnotation(AST::RightValue());
        return {{ WTFMove(commaExpression), { WTFMove(variableDeclaration) }, WhichAnder::ThreadAnder}};
    }
    return WTF::nullopt;
}

static Optional<AnderCallArgumentResult> anderCallArgument(UniqueRef<AST::Expression>& expression, AST::FunctionDeclaration* anderFunction, AST::FunctionDeclaration* threadAnderFunction)
{
    if (!anderFunction && !threadAnderFunction)
        return WTF::nullopt;
    auto& unifyNode = expression->resolvedType().unifyNode();
    if (is<AST::UnnamedType>(unifyNode)) {
        auto& unnamedType = downcast<AST::UnnamedType>(unifyNode);
        ASSERT(!is<AST::PointerType>(unnamedType));
        if (is<AST::ArrayReferenceType>(unnamedType))
            return {{ WTFMove(expression), WTF::nullopt, WhichAnder::Ander }};
        if (is<AST::ArrayType>(unnamedType))
            return wrapAnderCallArgument<AST::MakeArrayReferenceExpression, AST::ArrayReferenceType>(expression, downcast<AST::ArrayType>(unnamedType).type(), anderFunction, threadAnderFunction);
    }
    return wrapAnderCallArgument<AST::MakePointerExpression, AST::PointerType>(expression, expression->resolvedType(), anderFunction, threadAnderFunction);
}

static UniqueRef<AST::Expression> setterCall(AST::PropertyAccessExpression& propertyAccessExpression, AST::FunctionDeclaration* relevantAnder, UniqueRef<AST::Expression>&& newValue, const std::function<UniqueRef<AST::Expression>()>& leftValueFactory, AST::VariableDeclaration* indexVariable)
{
    auto maybeAddIndexArgument = [&](Vector<UniqueRef<AST::Expression>>& arguments) {
        if (!indexVariable)
            return;
        auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(*indexVariable));
        ASSERT(indexVariable->type());
        variableReference->setType(*indexVariable->type());
        variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?
        arguments.append(WTFMove(variableReference));
    };

    auto location = propertyAccessExpression.codeLocation();
    if (relevantAnder) {
        // *operator&.foo(&v) = newValue
        auto leftValue = leftValueFactory();
        auto argument = anderCallArgument(leftValue, relevantAnder, relevantAnder);
        ASSERT(argument);
        ASSERT(!argument->variableDeclaration);
        ASSERT(argument->whichAnder == WhichAnder::Ander);
        Vector<UniqueRef<AST::Expression>> arguments;
        arguments.append(WTFMove(argument->expression));
        maybeAddIndexArgument(arguments);

        auto callExpression = makeUniqueRef<AST::CallExpression>(location, String(relevantAnder->name()), WTFMove(arguments));
        callExpression->setType(relevantAnder->type());
        callExpression->setTypeAnnotation(AST::RightValue());
        callExpression->setFunction(*relevantAnder);

        auto dereferenceExpression = makeUniqueRef<AST::DereferenceExpression>(location, WTFMove(callExpression));
        dereferenceExpression->setType(downcast<AST::PointerType>(relevantAnder->type()).elementType());
        dereferenceExpression->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread });

        auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(location, WTFMove(dereferenceExpression), WTFMove(newValue));
        assignmentExpression->setType(downcast<AST::PointerType>(relevantAnder->type()).elementType());
        assignmentExpression->setTypeAnnotation(AST::RightValue());

        return UniqueRef<AST::Expression>(WTFMove(assignmentExpression));
    }

    // v = operator.foo=(v, newValue)
    ASSERT(propertyAccessExpression.setterFunction());

    Vector<UniqueRef<AST::Expression>> arguments;
    arguments.append(leftValueFactory());
    maybeAddIndexArgument(arguments);
    arguments.append(WTFMove(newValue));

    auto callExpression = makeUniqueRef<AST::CallExpression>(location, String(propertyAccessExpression.setterFunction()->name()), WTFMove(arguments));
    callExpression->setType(propertyAccessExpression.setterFunction()->type());
    callExpression->setTypeAnnotation(AST::RightValue());
    callExpression->setFunction(*propertyAccessExpression.setterFunction());

    auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(location, leftValueFactory(), WTFMove(callExpression));
    assignmentExpression->setType(propertyAccessExpression.setterFunction()->type());
    assignmentExpression->setTypeAnnotation(AST::RightValue());

    return UniqueRef<AST::Expression>(WTFMove(assignmentExpression));
}

static UniqueRef<AST::Expression> getterCall(AST::PropertyAccessExpression& propertyAccessExpression, AST::FunctionDeclaration* relevantAnder, const std::function<UniqueRef<AST::Expression>()>& leftValueFactory, AST::VariableDeclaration* indexVariable)
{
    auto maybeAddIndexArgument = [&](Vector<UniqueRef<AST::Expression>>& arguments) {
        if (!indexVariable)
            return;
        auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(*indexVariable));
        ASSERT(indexVariable->type());
        variableReference->setType(*indexVariable->type());
        variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?
        arguments.append(WTFMove(variableReference));
    };

    auto location = propertyAccessExpression.codeLocation();

    if (relevantAnder) {
        // *operator&.foo(&v)
        auto leftValue = leftValueFactory();
        auto argument = anderCallArgument(leftValue, relevantAnder, relevantAnder);
        ASSERT(argument);
        ASSERT(!argument->variableDeclaration);
        ASSERT(argument->whichAnder == WhichAnder::Ander);
        Vector<UniqueRef<AST::Expression>> arguments;
        arguments.append(WTFMove(argument->expression));
        maybeAddIndexArgument(arguments);

        auto callExpression = makeUniqueRef<AST::CallExpression>(location, String(relevantAnder->name()), WTFMove(arguments));
        callExpression->setType(relevantAnder->type());
        callExpression->setTypeAnnotation(AST::RightValue());
        callExpression->setFunction(*relevantAnder);

        auto dereferenceExpression = makeUniqueRef<AST::DereferenceExpression>(location, WTFMove(callExpression));
        dereferenceExpression->setType(downcast<AST::PointerType>(relevantAnder->type()).elementType());
        dereferenceExpression->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread });

        return UniqueRef<AST::Expression>(WTFMove(dereferenceExpression));
    }

    // operator.foo(v)
    ASSERT(propertyAccessExpression.getterFunction());

    Vector<UniqueRef<AST::Expression>> arguments;
    arguments.append(leftValueFactory());
    maybeAddIndexArgument(arguments);

    auto callExpression = makeUniqueRef<AST::CallExpression>(location, String(propertyAccessExpression.getterFunction()->name()), WTFMove(arguments));
    callExpression->setType(propertyAccessExpression.getterFunction()->type());
    callExpression->setTypeAnnotation(AST::RightValue());
    callExpression->setFunction(*propertyAccessExpression.getterFunction());

    return UniqueRef<AST::Expression>(WTFMove(callExpression));
}

struct ModifyResult {
    AST::Expression& innerLeftValue;
    Vector<UniqueRef<AST::Expression>> expressions;
    Vector<UniqueRef<AST::VariableDeclaration>> variableDeclarations;
};
struct ModificationResult {
    Vector<UniqueRef<AST::Expression>> expressions;
    UniqueRef<AST::Expression> result;
};
static ModifyResult modify(AST::PropertyAccessExpression& propertyAccessExpression, std::function<ModificationResult(UniqueRef<AST::Expression>&&)> modification)
{
    // Consider a.b.c.d++;
    // This would get transformed into:
    //
    // Step 1:
    // p = &a;
    //
    // Step 2:
    // q = operator.b(*p);
    // r = operator.c(q);
    //
    // Step 3:
    // oldValue = operator.d(r);
    // newValue = ...;
    //
    // Step 4:
    // r = operator.d=(r, newValue);
    // q = operator.c=(q, r);
    //
    // Step 5:
    // *p = operator.b=(*p, q);

    // If the expression is a.b.c.d = e, Step 3 disappears and "newValue" in step 4 becomes "e".
    

    // Find the ".b" ".c" and ".d" expressions. They end up in the order [".d", ".c", ".b"].
    Vector<std::reference_wrapper<AST::PropertyAccessExpression>> chain;
    AST::PropertyAccessExpression* iterator = &propertyAccessExpression;
    while (true) {
        chain.append(*iterator);
        if (iterator->base().typeAnnotation().leftAddressSpace())
            break;
        ASSERT(!iterator->base().typeAnnotation().isRightValue());
        iterator = &downcast<AST::PropertyAccessExpression>(iterator->base());
    }
    auto leftExpression = iterator->takeBase();
    AST::Expression& innerLeftExpression = leftExpression;

    // Create "p" variable.
    auto pointerVariable = makeUniqueRef<AST::VariableDeclaration>(leftExpression->codeLocation(), AST::Qualifiers(), AST::PointerType::create(leftExpression->codeLocation(), *leftExpression->typeAnnotation().leftAddressSpace(), leftExpression->resolvedType()), String(), nullptr, nullptr);

    // Create "q" and "r" variables.
    Vector<UniqueRef<AST::VariableDeclaration>> intermediateVariables;
    intermediateVariables.reserveInitialCapacity(chain.size() - 1);
    for (size_t i = 1; i < chain.size(); ++i) {
        auto& propertyAccessExpression = static_cast<AST::PropertyAccessExpression&>(chain[i]);
        intermediateVariables.uncheckedAppend(makeUniqueRef<AST::VariableDeclaration>(propertyAccessExpression.codeLocation(), AST::Qualifiers(), &propertyAccessExpression.resolvedType(), String(), nullptr, nullptr));
    }

    // Consider a[foo()][b] = c;
    // Naively, This would get expanded to:
    //
    // temp = operator[](a, foo());
    // temp = operator[]=(temp, b, c);
    // a = operator[]=(a, foo(), temp);
    //
    // However, if we did this, we would have to run foo() twice, which would be incorrect.
    // Instead, we need to save foo() and b into more temporary variables.
    // These temporary variables are parallel to "chain" above, with nullopt referring to a DotExpression (which doesn't have an index value to save to a variable).
    //
    // Instead, this gets expanded to:
    //
    // p = &a;
    // temp = foo();
    // q = operator[](*p, temp);
    // temp2 = b;
    // q = operator[]=(q, temp2, c);
    // *p = operator[]=(*p, temp, q);

    Vector<Optional<UniqueRef<AST::VariableDeclaration>>> indexVariables;
    indexVariables.reserveInitialCapacity(chain.size());
    for (AST::PropertyAccessExpression& propertyAccessExpression : chain) {
        if (!is<AST::IndexExpression>(propertyAccessExpression)) {
            indexVariables.append(WTF::nullopt);
            continue;
        }
        auto& indexExpression = downcast<AST::IndexExpression>(propertyAccessExpression);
        indexVariables.uncheckedAppend(makeUniqueRef<AST::VariableDeclaration>(propertyAccessExpression.codeLocation(), AST::Qualifiers(), &indexExpression.indexExpression().resolvedType(), String(), nullptr, nullptr));
    }

    Vector<UniqueRef<AST::Expression>> expressions;

    // Step 1:
    {
        auto makePointerExpression = makeUniqueRef<AST::MakePointerExpression>(innerLeftExpression.codeLocation(), WTFMove(leftExpression), AST::AddressEscapeMode::DoesNotEscape);
        makePointerExpression->setType(AST::PointerType::create(innerLeftExpression.codeLocation(), *innerLeftExpression.typeAnnotation().leftAddressSpace(), innerLeftExpression.resolvedType()));
        makePointerExpression->setTypeAnnotation(AST::RightValue());

        auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(pointerVariable));
        ASSERT(pointerVariable->type());
        variableReference->setType(*pointerVariable->type());
        variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

        auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(innerLeftExpression.codeLocation(), WTFMove(variableReference), WTFMove(makePointerExpression));
        assignmentExpression->setType(*pointerVariable->type());
        assignmentExpression->setTypeAnnotation(AST::RightValue());

        expressions.append(WTFMove(assignmentExpression));
    }

    // Step 2:
    AST::VariableDeclaration* previous = nullptr;
    auto previousLeftValue = [&]() -> UniqueRef<AST::Expression> {
        if (previous) {
            auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(*previous));
            ASSERT(previous->type());
            variableReference->setType(*previous->type());
            variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?
            return variableReference;
        }

        auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(pointerVariable));
        ASSERT(pointerVariable->type());
        variableReference->setType(*pointerVariable->type());
        variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

        auto dereferenceExpression = makeUniqueRef<AST::DereferenceExpression>(propertyAccessExpression.codeLocation(), WTFMove(variableReference));
        ASSERT(pointerVariable->type());
        dereferenceExpression->setType(downcast<AST::PointerType>(*pointerVariable->type()).elementType());
        dereferenceExpression->setTypeAnnotation(AST::LeftValue { downcast<AST::PointerType>(*pointerVariable->type()).addressSpace() });
        return dereferenceExpression;
    };
    auto appendIndexAssignment = [&](AST::PropertyAccessExpression& propertyAccessExpression, Optional<UniqueRef<AST::VariableDeclaration>>& indexVariable) {
        if (!indexVariable)
            return;

        auto& indexExpression = downcast<AST::IndexExpression>(propertyAccessExpression);

        auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(*indexVariable));
        ASSERT(indexVariable->get().type());
        variableReference->setType(*indexVariable->get().type());
        variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

        auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(propertyAccessExpression.codeLocation(), WTFMove(variableReference), indexExpression.takeIndex());
        assignmentExpression->setType(*indexVariable->get().type());
        assignmentExpression->setTypeAnnotation(AST::RightValue());

        expressions.append(WTFMove(assignmentExpression));
    };
    for (size_t i = chain.size(); --i; ) {
        AST::PropertyAccessExpression& propertyAccessExpression = chain[i];
        AST::VariableDeclaration& variableDeclaration = intermediateVariables[i - 1];
        Optional<UniqueRef<AST::VariableDeclaration>>& indexVariable = indexVariables[i];

        appendIndexAssignment(propertyAccessExpression, indexVariable);

        AST::FunctionDeclaration* relevantAnder = i == chain.size() - 1 ? propertyAccessExpression.anderFunction() : propertyAccessExpression.threadAnderFunction();
        auto callExpression = getterCall(propertyAccessExpression, relevantAnder, previousLeftValue, indexVariable ? &*indexVariable : nullptr);

        auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(variableDeclaration));
        ASSERT(variableDeclaration.type());
        variableReference->setType(*variableDeclaration.type());
        variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

        auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(propertyAccessExpression.codeLocation(), WTFMove(variableReference), WTFMove(callExpression));
        assignmentExpression->setType(*variableDeclaration.type());
        assignmentExpression->setTypeAnnotation(AST::RightValue());

        expressions.append(WTFMove(assignmentExpression));

        previous = &variableDeclaration;
    }
    appendIndexAssignment(chain[0], indexVariables[0]);
    AST::FunctionDeclaration* relevantAnder = chain.size() == 1 ? propertyAccessExpression.anderFunction() : propertyAccessExpression.threadAnderFunction();
    auto lastGetterCallExpression = getterCall(chain[0], relevantAnder, previousLeftValue, indexVariables[0] ? &*(indexVariables[0]) : nullptr);

    // Step 3:
    auto modificationResult = modification(WTFMove(lastGetterCallExpression));
    for (size_t i = 0; i < modificationResult.expressions.size(); ++i)
        expressions.append(WTFMove(modificationResult.expressions[i]));

    // Step 4:
    UniqueRef<AST::Expression> rightValue = WTFMove(modificationResult.result);
    for (size_t i = 0; i < chain.size() - 1; ++i) {
        AST::PropertyAccessExpression& propertyAccessExpression = chain[i];
        AST::VariableDeclaration& variableDeclaration = intermediateVariables[i];
        Optional<UniqueRef<AST::VariableDeclaration>>& indexVariable = indexVariables[i];

        auto assignmentExpression = setterCall(propertyAccessExpression, propertyAccessExpression.threadAnderFunction(), WTFMove(rightValue), [&]() -> UniqueRef<AST::Expression> {
            auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(variableDeclaration));
            ASSERT(variableDeclaration.type());
            variableReference->setType(*variableDeclaration.type());
            variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?
            return variableReference;
        }, indexVariable ? &*indexVariable : nullptr);

        expressions.append(WTFMove(assignmentExpression));

        rightValue = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(variableDeclaration));
        ASSERT(variableDeclaration.type());
        rightValue->setType(*variableDeclaration.type());
        rightValue->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?
    }

    // Step 5:
    {
        AST::PropertyAccessExpression& propertyAccessExpression = chain[chain.size() - 1];
        auto assignmentExpression = setterCall(propertyAccessExpression, propertyAccessExpression.anderFunction(), WTFMove(rightValue), [&]() -> UniqueRef<AST::Expression> {
            auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(pointerVariable));
            ASSERT(pointerVariable->type());
            variableReference->setType(*pointerVariable->type());
            variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto dereferenceExpression = makeUniqueRef<AST::DereferenceExpression>(propertyAccessExpression.codeLocation(), WTFMove(variableReference));
            ASSERT(pointerVariable->type());
            dereferenceExpression->setType(downcast<AST::PointerType>(*pointerVariable->type()).elementType());
            dereferenceExpression->setTypeAnnotation(AST::LeftValue { downcast<AST::PointerType>(*pointerVariable->type()).addressSpace() });
            return dereferenceExpression;
        }, indexVariables[indexVariables.size() - 1] ? &*(indexVariables[indexVariables.size() - 1]) : nullptr);

        expressions.append(WTFMove(assignmentExpression));
    }

    Vector<UniqueRef<AST::VariableDeclaration>> variableDeclarations;
    variableDeclarations.append(WTFMove(pointerVariable));
    for (auto& intermediateVariable : intermediateVariables)
        variableDeclarations.append(WTFMove(intermediateVariable));
    for (auto& indexVariable : indexVariables) {
        if (indexVariable)
            variableDeclarations.append(WTFMove(*indexVariable));
    }

    return { innerLeftExpression, WTFMove(expressions), WTFMove(variableDeclarations) };
}

void PropertyResolver::visit(AST::AssignmentExpression& assignmentExpression)
{
    if (assignmentExpression.left().typeAnnotation().leftAddressSpace()) {
        simplifyLeftValue(assignmentExpression.left());
        checkErrorAndVisit(assignmentExpression.right());
        return;
    }
    ASSERT(!assignmentExpression.left().typeAnnotation().isRightValue());

    Ref<AST::UnnamedType> type = assignmentExpression.right().resolvedType();

    checkErrorAndVisit(assignmentExpression.right());

    auto modifyResult = modify(downcast<AST::PropertyAccessExpression>(assignmentExpression.left()), [&](UniqueRef<AST::Expression>&&) -> ModificationResult {
        return { Vector<UniqueRef<AST::Expression>>(), assignmentExpression.takeRight() };
    });

    simplifyLeftValue(modifyResult.innerLeftValue);

    auto location = assignmentExpression.codeLocation();
    auto* commaExpression = AST::replaceWith<AST::CommaExpression>(assignmentExpression, location, WTFMove(modifyResult.expressions));
    commaExpression->setType(WTFMove(type));
    commaExpression->setTypeAnnotation(AST::RightValue());

    for (auto& variableDeclaration : modifyResult.variableDeclarations)
        m_variableDeclarations.append(WTFMove(variableDeclaration));
}

void PropertyResolver::visit(AST::ReadModifyWriteExpression& readModifyWriteExpression)
{
    checkErrorAndVisit(readModifyWriteExpression.newValueExpression());
    if (hasError())
        return;

    auto location = readModifyWriteExpression.codeLocation();
    auto leftValueLocation = readModifyWriteExpression.leftValue().codeLocation();

    if (readModifyWriteExpression.leftValue().typeAnnotation().leftAddressSpace()) {
        // Consider a++;
        // This would get transformed into:
        //
        // p = &a;
        // oldValue = *p;
        // newValue = ...;
        // *p = newValue;

        simplifyLeftValue(readModifyWriteExpression.leftValue());

        Ref<AST::UnnamedType> baseType = readModifyWriteExpression.leftValue().resolvedType();
        Ref<AST::UnnamedType> pointerType = AST::PointerType::create(leftValueLocation, *readModifyWriteExpression.leftValue().typeAnnotation().leftAddressSpace(), baseType.copyRef());

        auto pointerVariable = makeUniqueRef<AST::VariableDeclaration>(leftValueLocation, AST::Qualifiers(), pointerType.copyRef(), String(), nullptr, nullptr);

        Vector<UniqueRef<AST::Expression>> expressions;

        {
            auto makePointerExpression = makeUniqueRef<AST::MakePointerExpression>(leftValueLocation, readModifyWriteExpression.takeLeftValue(), AST::AddressEscapeMode::DoesNotEscape);
            makePointerExpression->setType(pointerType.copyRef());
            makePointerExpression->setTypeAnnotation(AST::RightValue());

            auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(pointerVariable));
            variableReference->setType(pointerType.copyRef());
            variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(leftValueLocation, WTFMove(variableReference), WTFMove(makePointerExpression));
            assignmentExpression->setType(pointerType.copyRef());
            assignmentExpression->setTypeAnnotation(AST::RightValue());

            expressions.append(WTFMove(assignmentExpression));
        }

        {
            auto variableReference1 = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(pointerVariable));
            variableReference1->setType(pointerType.copyRef());
            variableReference1->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto dereferenceExpression = makeUniqueRef<AST::DereferenceExpression>(location, WTFMove(variableReference1));
            dereferenceExpression->setType(baseType.copyRef());
            dereferenceExpression->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto variableReference2 = readModifyWriteExpression.oldVariableReference();
            variableReference2->setType(baseType.copyRef());
            variableReference2->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(location, WTFMove(variableReference2), WTFMove(dereferenceExpression));
            assignmentExpression->setType(baseType.copyRef());
            assignmentExpression->setTypeAnnotation(AST::RightValue());

            expressions.append(WTFMove(assignmentExpression));
        }

        {
            auto variableReference = readModifyWriteExpression.newVariableReference();
            variableReference->setType(baseType.copyRef());
            variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto newValueExpression = readModifyWriteExpression.takeNewValueExpression();
            auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(location, WTFMove(variableReference), WTFMove(newValueExpression));
            assignmentExpression->setType(baseType.copyRef());
            assignmentExpression->setTypeAnnotation(AST::RightValue());

            expressions.append(WTFMove(assignmentExpression));
        }

        {
            auto variableReference1 = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(pointerVariable));
            variableReference1->setType(pointerType.copyRef());
            variableReference1->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto dereferenceExpression = makeUniqueRef<AST::DereferenceExpression>(location, WTFMove(variableReference1));
            dereferenceExpression->setType(baseType.copyRef());
            dereferenceExpression->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto variableReference2 = readModifyWriteExpression.newVariableReference();
            variableReference2->setType(baseType.copyRef());
            variableReference2->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(location, WTFMove(dereferenceExpression), WTFMove(variableReference2));
            assignmentExpression->setType(baseType.copyRef());
            assignmentExpression->setTypeAnnotation(AST::RightValue());

            expressions.append(WTFMove(assignmentExpression));
        }

        auto resultExpression = readModifyWriteExpression.takeResultExpression();
        Ref<AST::UnnamedType> type = resultExpression->resolvedType();
        expressions.append(WTFMove(resultExpression));

        UniqueRef<AST::VariableDeclaration> oldVariableDeclaration = readModifyWriteExpression.takeOldValue();
        UniqueRef<AST::VariableDeclaration> newVariableDeclaration = readModifyWriteExpression.takeNewValue();

        auto* commaExpression = AST::replaceWith<AST::CommaExpression>(readModifyWriteExpression, location, WTFMove(expressions));
        commaExpression->setType(WTFMove(type));
        commaExpression->setTypeAnnotation(AST::RightValue());

        m_variableDeclarations.append(WTFMove(pointerVariable));
        m_variableDeclarations.append(WTFMove(oldVariableDeclaration));
        m_variableDeclarations.append(WTFMove(newVariableDeclaration));
        return;
    }

    ASSERT(!readModifyWriteExpression.leftValue().typeAnnotation().isRightValue());
    if (!is<AST::PropertyAccessExpression>(readModifyWriteExpression.leftValue())) {
        setError(Error("Base of read modify write expression is a right-value.", readModifyWriteExpression.leftValue().codeLocation()));
        return;
    }
    auto modifyResult = modify(downcast<AST::PropertyAccessExpression>(readModifyWriteExpression.leftValue()), [&](UniqueRef<AST::Expression>&& lastGetterCallExpression) -> ModificationResult {
        Vector<UniqueRef<AST::Expression>> expressions;

        {
            auto variableReference = readModifyWriteExpression.oldVariableReference();
            variableReference->setType(readModifyWriteExpression.leftValue().resolvedType());
            variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(leftValueLocation, WTFMove(variableReference), WTFMove(lastGetterCallExpression));
            assignmentExpression->setType(readModifyWriteExpression.leftValue().resolvedType());
            assignmentExpression->setTypeAnnotation(AST::RightValue());

            expressions.append(WTFMove(assignmentExpression));
        }

        {
            auto variableReference = readModifyWriteExpression.newVariableReference();
            variableReference->setType(readModifyWriteExpression.leftValue().resolvedType());
            variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto newValueExpression = readModifyWriteExpression.takeNewValueExpression();
            auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(leftValueLocation, WTFMove(variableReference), WTFMove(newValueExpression));
            assignmentExpression->setType(readModifyWriteExpression.leftValue().resolvedType());
            assignmentExpression->setTypeAnnotation(AST::RightValue());

            expressions.append(WTFMove(assignmentExpression));
        }

        auto variableReference = readModifyWriteExpression.newVariableReference();
        variableReference->setType(readModifyWriteExpression.leftValue().resolvedType());
        variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

        return { WTFMove(expressions),  WTFMove(variableReference) };
    });

    simplifyLeftValue(modifyResult.innerLeftValue);

    auto resultExpression = readModifyWriteExpression.takeResultExpression();
    Ref<AST::UnnamedType> type = resultExpression->resolvedType();
    modifyResult.expressions.append(WTFMove(resultExpression));

    UniqueRef<AST::VariableDeclaration> oldVariableDeclaration = readModifyWriteExpression.takeOldValue();
    UniqueRef<AST::VariableDeclaration> newVariableDeclaration = readModifyWriteExpression.takeNewValue();

    auto* commaExpression = AST::replaceWith<AST::CommaExpression>(readModifyWriteExpression, location, WTFMove(modifyResult.expressions));
    commaExpression->setType(WTFMove(type));
    commaExpression->setTypeAnnotation(AST::RightValue());

    for (auto& variableDeclaration : modifyResult.variableDeclarations)
        m_variableDeclarations.append(WTFMove(variableDeclaration));
    m_variableDeclarations.append(WTFMove(oldVariableDeclaration));
    m_variableDeclarations.append(WTFMove(newVariableDeclaration));
}

static Optional<AnderCallArgumentResult> anderCallArgument(AST::PropertyAccessExpression& propertyAccessExpression)
{
    return anderCallArgument(propertyAccessExpression.baseReference(), propertyAccessExpression.anderFunction(), propertyAccessExpression.threadAnderFunction());
}

void PropertyResolver::simplifyRightValue(AST::PropertyAccessExpression& propertyAccessExpression)
{
    auto location = propertyAccessExpression.codeLocation();

    checkErrorAndVisit(propertyAccessExpression.base());

    if (auto argument = anderCallArgument(propertyAccessExpression)) {
        auto* anderFunction = argument->whichAnder == WhichAnder::ThreadAnder ? propertyAccessExpression.threadAnderFunction() : propertyAccessExpression.anderFunction();
        ASSERT(anderFunction);
        Vector<UniqueRef<AST::Expression>> arguments;
        arguments.append(WTFMove(argument->expression));
        if (is<AST::IndexExpression>(propertyAccessExpression))
            arguments.append(downcast<AST::IndexExpression>(propertyAccessExpression).takeIndex());
        auto callExpression = makeUniqueRef<AST::CallExpression>(location, String(anderFunction->name()), WTFMove(arguments));
        callExpression->setType(anderFunction->type());
        callExpression->setTypeAnnotation(AST::RightValue());
        callExpression->setFunction(*anderFunction);

        auto* dereferenceExpression = AST::replaceWith<AST::DereferenceExpression>(propertyAccessExpression, location, WTFMove(callExpression));
        dereferenceExpression->setType(downcast<AST::PointerType>(anderFunction->type()).elementType());
        dereferenceExpression->setTypeAnnotation(AST::LeftValue { downcast<AST::PointerType>(anderFunction->type()).addressSpace() });

        if (auto& variableDeclaration = argument->variableDeclaration)
            m_variableDeclarations.append(WTFMove(*variableDeclaration));

        return;
    }

    ASSERT(propertyAccessExpression.getterFunction());
    auto& getterFunction = *propertyAccessExpression.getterFunction();
    Vector<UniqueRef<AST::Expression>> arguments;
    arguments.append(propertyAccessExpression.takeBase());
    AST::CallExpression* callExpression;
    if (is<AST::IndexExpression>(propertyAccessExpression)) {
        auto& indexExpression = downcast<AST::IndexExpression>(propertyAccessExpression);
        arguments.append(indexExpression.takeIndex());
        callExpression = AST::replaceWith<AST::CallExpression>(indexExpression, location, String(getterFunction.name()), WTFMove(arguments));
    } else {
        ASSERT(is<AST::DotExpression>(propertyAccessExpression));
        auto& dotExpression = downcast<AST::DotExpression>(propertyAccessExpression);
        callExpression = AST::replaceWith<AST::CallExpression>(dotExpression, location, String(getterFunction.name()), WTFMove(arguments));
    }
    callExpression->setFunction(getterFunction);
    callExpression->setType(getterFunction.type());
    callExpression->setTypeAnnotation(AST::RightValue());
}

class LeftValueSimplifier : public Visitor {
private:
    void visit(AST::DotExpression&) override;
    void visit(AST::IndexExpression&) override;
    void visit(AST::DereferenceExpression&) override;

    void finishVisiting(AST::PropertyAccessExpression&);
};

void LeftValueSimplifier::finishVisiting(AST::PropertyAccessExpression& propertyAccessExpression)
{
    ASSERT(propertyAccessExpression.base().typeAnnotation().leftAddressSpace());
    ASSERT(propertyAccessExpression.anderFunction());

    Visitor::visit(propertyAccessExpression.base());

    auto location = propertyAccessExpression.codeLocation();
    auto* anderFunction = propertyAccessExpression.anderFunction();

    auto argument = anderCallArgument(propertyAccessExpression);
    ASSERT(argument);
    ASSERT(!argument->variableDeclaration);
    ASSERT(argument->whichAnder == WhichAnder::Ander);

    Vector<UniqueRef<AST::Expression>> arguments;
    arguments.append(WTFMove(argument->expression));
    if (is<AST::IndexExpression>(propertyAccessExpression))
        arguments.append(downcast<AST::IndexExpression>(propertyAccessExpression).takeIndex());
    auto callExpression = makeUniqueRef<AST::CallExpression>(location, String(anderFunction->name()), WTFMove(arguments));
    callExpression->setType(anderFunction->type());
    callExpression->setTypeAnnotation(AST::RightValue());
    callExpression->setFunction(*anderFunction);

    auto* dereferenceExpression = AST::replaceWith<AST::DereferenceExpression>(propertyAccessExpression, location, WTFMove(callExpression));
    dereferenceExpression->setType(downcast<AST::PointerType>(anderFunction->type()).elementType());
    dereferenceExpression->setTypeAnnotation(AST::LeftValue { downcast<AST::PointerType>(anderFunction->type()).addressSpace() });
}

void LeftValueSimplifier::visit(AST::DotExpression& dotExpression)
{
    Visitor::visit(dotExpression);
    finishVisiting(dotExpression);
}

void LeftValueSimplifier::visit(AST::IndexExpression& indexExpression)
{
    PropertyResolver().Visitor::visit(indexExpression.indexExpression());
    finishVisiting(indexExpression);
}

void LeftValueSimplifier::visit(AST::DereferenceExpression& dereferenceExpression)
{
    // Dereference expressions are the only expressions where the children might be more-right than we are.
    // For example, a dereference expression may be a left value but its child may be a call expression which is a right value.
    // LeftValueSimplifier doesn't handle right values, so we instead need to use PropertyResolver.
    PropertyResolver().Visitor::visit(dereferenceExpression);
}

void PropertyResolver::simplifyLeftValue(AST::Expression& expression)
{
    LeftValueSimplifier().Visitor::visit(expression);
}

void resolveProperties(Program& program)
{
    PropertyResolver().Visitor::visit(program);
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
