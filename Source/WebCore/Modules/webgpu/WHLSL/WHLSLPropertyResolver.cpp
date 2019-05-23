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

#include "WHLSLAssignmentExpression.h"
#include "WHLSLCallExpression.h"
#include "WHLSLCommaExpression.h"
#include "WHLSLDereferenceExpression.h"
#include "WHLSLDotExpression.h"
#include "WHLSLFunctionDeclaration.h"
#include "WHLSLFunctionDefinition.h"
#include "WHLSLMakePointerExpression.h"
#include "WHLSLPointerType.h"
#include "WHLSLReadModifyWriteExpression.h"
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
    void visit(AST::AssignmentExpression&) override;
    void visit(AST::ReadModifyWriteExpression&) override;

    bool simplifyRightValue(AST::DotExpression&);
    bool simplifyAbstractLeftValue(AST::AssignmentExpression&, AST::DotExpression&, UniqueRef<AST::Expression>&& right);
    void simplifyLeftValue(AST::Expression&);

    AST::VariableDeclarations m_variableDeclarations;
};

void PropertyResolver::visit(AST::DotExpression& dotExpression)
{
    // Unless we're inside an AssignmentExpression or a ReadModifyWriteExpression, we're a right value.
    if (!simplifyRightValue(dotExpression))
        setError();
}

void PropertyResolver::visit(AST::FunctionDefinition& functionDefinition)
{
    Visitor::visit(functionDefinition);
    if (!m_variableDeclarations.isEmpty())
        functionDefinition.block().statements().insert(0, makeUniqueRef<AST::VariableDeclarationsStatement>(Lexer::Token(m_variableDeclarations[0]->origin()), WTFMove(m_variableDeclarations)));
}

static Optional<UniqueRef<AST::Expression>> setterCall(AST::DotExpression& dotExpression, UniqueRef<AST::Expression>&& newValue, const std::function<UniqueRef<AST::Expression>()>& leftValueFactory, const std::function<UniqueRef<AST::Expression>()>& pointerToLeftValueFactory)
{
    if (dotExpression.anderFunction()) {
        // *operator&.foo(&v) = newValue
        if (!dotExpression.threadAnderFunction())
            return WTF::nullopt;
        
        Vector<UniqueRef<AST::Expression>> arguments;
        arguments.append(pointerToLeftValueFactory());
        auto callExpression = makeUniqueRef<AST::CallExpression>(Lexer::Token(dotExpression.origin()), String(dotExpression.threadAnderFunction()->name()), WTFMove(arguments));
        callExpression->setType(dotExpression.threadAnderFunction()->type().clone());
        callExpression->setTypeAnnotation(AST::RightValue());
        callExpression->setFunction(*dotExpression.threadAnderFunction());

        auto dereferenceExpression = makeUniqueRef<AST::DereferenceExpression>(Lexer::Token(dotExpression.origin()), WTFMove(callExpression));
        dereferenceExpression->setType(downcast<AST::PointerType>(dotExpression.threadAnderFunction()->type()).elementType().clone());
        dereferenceExpression->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread });

        auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(Lexer::Token(dotExpression.origin()), WTFMove(dereferenceExpression), WTFMove(newValue));
        assignmentExpression->setType(downcast<AST::PointerType>(dotExpression.threadAnderFunction()->type()).elementType().clone());
        assignmentExpression->setTypeAnnotation(AST::RightValue());

        return UniqueRef<AST::Expression>(WTFMove(assignmentExpression));
    }

    // v = operator.foo=(v, newValue)
    ASSERT(dotExpression.setterFunction());

    Vector<UniqueRef<AST::Expression>> arguments;
    arguments.append(leftValueFactory());
    arguments.append(WTFMove(newValue));
    auto callExpression = makeUniqueRef<AST::CallExpression>(Lexer::Token(dotExpression.origin()), String(dotExpression.setterFunction()->name()), WTFMove(arguments));
    callExpression->setType(dotExpression.setterFunction()->type().clone());
    callExpression->setTypeAnnotation(AST::RightValue());
    callExpression->setFunction(*dotExpression.setterFunction());

    auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(Lexer::Token(dotExpression.origin()), leftValueFactory(), WTFMove(callExpression));
    assignmentExpression->setType(dotExpression.setterFunction()->type().clone());
    assignmentExpression->setTypeAnnotation(AST::RightValue());

    return UniqueRef<AST::Expression>(WTFMove(assignmentExpression));
}

static Optional<UniqueRef<AST::Expression>> getterCall(AST::DotExpression& dotExpression, const std::function<UniqueRef<AST::Expression>()>& leftValueFactory, const std::function<UniqueRef<AST::Expression>()>& pointerToLeftValueFactory)
{
    if (dotExpression.anderFunction()) {
        // *operator&.foo(&v)
        if (!dotExpression.threadAnderFunction())
            return WTF::nullopt;
        
        Vector<UniqueRef<AST::Expression>> arguments;
        arguments.append(pointerToLeftValueFactory());
        auto callExpression = makeUniqueRef<AST::CallExpression>(Lexer::Token(dotExpression.origin()), String(dotExpression.threadAnderFunction()->name()), WTFMove(arguments));
        callExpression->setType(dotExpression.threadAnderFunction()->type().clone());
        callExpression->setTypeAnnotation(AST::RightValue());
        callExpression->setFunction(*dotExpression.threadAnderFunction());

        auto dereferenceExpression = makeUniqueRef<AST::DereferenceExpression>(Lexer::Token(dotExpression.origin()), WTFMove(callExpression));
        dereferenceExpression->setType(downcast<AST::PointerType>(dotExpression.threadAnderFunction()->type()).elementType().clone());
        dereferenceExpression->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread });

        return UniqueRef<AST::Expression>(WTFMove(dereferenceExpression));
    }

    // operator.foo(v)
    ASSERT(dotExpression.getterFunction());
    
    Vector<UniqueRef<AST::Expression>> arguments;
    arguments.append(leftValueFactory());
    auto callExpression = makeUniqueRef<AST::CallExpression>(Lexer::Token(dotExpression.origin()), String(dotExpression.getterFunction()->name()), WTFMove(arguments));
    callExpression->setType(dotExpression.getterFunction()->type().clone());
    callExpression->setTypeAnnotation(AST::RightValue());
    callExpression->setFunction(*dotExpression.getterFunction());

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
static Optional<ModifyResult> modify(AST::DotExpression& dotExpression, std::function<Optional<ModificationResult>(Optional<UniqueRef<AST::Expression>>&&)> modification)
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
    // Step 4:
    // *p = operator.b=(*p, q);

    // If the expression is a.b.c.d = e, Step 3 disappears and "newValue" in step 4 becomes "e".
    

    // Find the ".b" ".c" and ".d" expressions. They end up in the order [".d", ".c", ".b"].
    Vector<std::reference_wrapper<AST::DotExpression>> chain;
    AST::DotExpression* iterator = &dotExpression;
    while (true) {
        chain.append(*iterator);
        if (iterator->base().typeAnnotation().leftAddressSpace())
            break;
        ASSERT(!iterator->base().typeAnnotation().isRightValue());
        ASSERT(is<AST::DotExpression>(iterator->base())); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198163 Make this work with index expressions
        iterator = &downcast<AST::DotExpression>(iterator->base());
    }
    auto leftExpression = iterator->takeBase();
    AST::Expression& innerLeftExpression = leftExpression;

    // Create "p" variable.
    auto pointerVariable = makeUniqueRef<AST::VariableDeclaration>(Lexer::Token(leftExpression->origin()), AST::Qualifiers(), UniqueRef<AST::UnnamedType>(makeUniqueRef<AST::PointerType>(Lexer::Token(leftExpression->origin()), *leftExpression->typeAnnotation().leftAddressSpace(), leftExpression->resolvedType().clone())), String(), WTF::nullopt, WTF::nullopt);

    // Create "q" and "r" variables.
    Vector<UniqueRef<AST::VariableDeclaration>> intermediateVariables;
    intermediateVariables.reserveInitialCapacity(chain.size() - 1);
    for (size_t i = 1; i < chain.size(); ++i) {
        auto& dotExpression = static_cast<AST::DotExpression&>(chain[i]);
        intermediateVariables.uncheckedAppend(makeUniqueRef<AST::VariableDeclaration>(Lexer::Token(dotExpression.origin()), AST::Qualifiers(), dotExpression.resolvedType().clone(), String(), WTF::nullopt, WTF::nullopt));
    }

    Vector<UniqueRef<AST::Expression>> expressions;

    // Step 1:
    {
        auto makePointerExpression = makeUniqueRef<AST::MakePointerExpression>(Lexer::Token(innerLeftExpression.origin()), WTFMove(leftExpression));
        makePointerExpression->setType(makeUniqueRef<AST::PointerType>(Lexer::Token(innerLeftExpression.origin()), *innerLeftExpression.typeAnnotation().leftAddressSpace(), innerLeftExpression.resolvedType().clone()));
        makePointerExpression->setTypeAnnotation(AST::RightValue());

        auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(pointerVariable));
        ASSERT(pointerVariable->type());
        variableReference->setType(pointerVariable->type()->clone());
        variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

        auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(Lexer::Token(innerLeftExpression.origin()), WTFMove(variableReference), WTFMove(makePointerExpression));
        assignmentExpression->setType(pointerVariable->type()->clone());
        assignmentExpression->setTypeAnnotation(AST::RightValue());

        expressions.append(WTFMove(assignmentExpression));
    }

    // Step 2:
    AST::VariableDeclaration* previous = nullptr;
    auto previousLeftValue = [&]() -> UniqueRef<AST::Expression> {
        if (previous) {
            auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(*previous));
            ASSERT(previous->type());
            variableReference->setType(previous->type()->clone());
            variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?
            return variableReference;
        }
    
        auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(pointerVariable));
        ASSERT(pointerVariable->type());
        variableReference->setType(pointerVariable->type()->clone());
        variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

        auto dereferenceExpression = makeUniqueRef<AST::DereferenceExpression>(Lexer::Token(dotExpression.origin()), WTFMove(variableReference));
        ASSERT(pointerVariable->type());
        dereferenceExpression->setType(downcast<AST::PointerType>(*pointerVariable->type()).elementType().clone());
        dereferenceExpression->setTypeAnnotation(AST::LeftValue { downcast<AST::PointerType>(*pointerVariable->type()).addressSpace() });
        return dereferenceExpression;
    };
    auto pointerToPreviousLeftValue = [&]() -> UniqueRef<AST::Expression> {
        if (previous) {
            auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(*previous));
            ASSERT(previous->type());
            variableReference->setType(previous->type()->clone());
            variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto makePointerExpression = makeUniqueRef<AST::MakePointerExpression>(Lexer::Token(dotExpression.origin()), WTFMove(variableReference));
            ASSERT(previous->type());
            makePointerExpression->setType(makeUniqueRef<AST::PointerType>(Lexer::Token(dotExpression.origin()), AST::AddressSpace::Thread, previous->type()->clone()));
            makePointerExpression->setTypeAnnotation(AST::RightValue());
            return makePointerExpression;
        }

        auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(pointerVariable));
        ASSERT(pointerVariable->type());
        variableReference->setType(pointerVariable->type()->clone());
        variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?
        return variableReference;
    };
    for (size_t i = chain.size(); --i; ) {
        AST::DotExpression& dotExpression = chain[i];
        AST::VariableDeclaration& variableDeclaration = intermediateVariables[i - 1];

        auto callExpression = getterCall(dotExpression, previousLeftValue, pointerToPreviousLeftValue);

        if (!callExpression)
            return WTF::nullopt;

        auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(variableDeclaration));
        ASSERT(variableDeclaration.type());
        variableReference->setType(variableDeclaration.type()->clone());
        variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

        auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(Lexer::Token(dotExpression.origin()), WTFMove(variableReference), WTFMove(*callExpression));
        assignmentExpression->setType(variableDeclaration.type()->clone());
        assignmentExpression->setTypeAnnotation(AST::RightValue());

        expressions.append(WTFMove(assignmentExpression));
        
        previous = &variableDeclaration;
    }
    auto lastGetterCallExpression = getterCall(chain[0], previousLeftValue, pointerToPreviousLeftValue);

    // Step 3:
    auto modificationResult = modification(WTFMove(lastGetterCallExpression));
    if (!modificationResult)
        return WTF::nullopt;
    for (size_t i = 0; i < modificationResult->expressions.size(); ++i)
        expressions.append(WTFMove(modificationResult->expressions[i]));

    // Step 4:
    UniqueRef<AST::Expression> rightValue = WTFMove(modificationResult->result);
    auto expressionType = rightValue->resolvedType().clone();
    for (size_t i = 0; i < chain.size() - 1; ++i) {
        AST::DotExpression& dotExpression = chain[i];
        AST::VariableDeclaration& variableDeclaration = intermediateVariables[i];

        auto assignmentExpression = setterCall(dotExpression, WTFMove(rightValue), [&]() -> UniqueRef<AST::Expression> {
            auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(variableDeclaration));
            ASSERT(variableDeclaration.type());
            variableReference->setType(variableDeclaration.type()->clone());
            variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?
            return variableReference;
        }, [&]() -> UniqueRef<AST::Expression> {
            auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(variableDeclaration));
            ASSERT(variableDeclaration.type());
            variableReference->setType(variableDeclaration.type()->clone());
            variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto makePointerExpression = makeUniqueRef<AST::MakePointerExpression>(Lexer::Token(dotExpression.origin()), WTFMove(variableReference));
            ASSERT(variableDeclaration.type());
            makePointerExpression->setType(makeUniqueRef<AST::PointerType>(Lexer::Token(dotExpression.origin()), AST::AddressSpace::Thread, variableDeclaration.type()->clone()));
            makePointerExpression->setTypeAnnotation(AST::RightValue());
            return makePointerExpression;
        });

        if (!assignmentExpression)
            return WTF::nullopt;

        expressions.append(WTFMove(*assignmentExpression));

        rightValue = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(variableDeclaration));
        ASSERT(variableDeclaration.type());
        rightValue->setType(variableDeclaration.type()->clone());
        rightValue->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?
    }

    // Step 5:
    {
        auto assignmentExpression = setterCall(chain[chain.size() - 1], WTFMove(rightValue), [&]() -> UniqueRef<AST::Expression> {
            auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(pointerVariable));
            ASSERT(pointerVariable->type());
            variableReference->setType(pointerVariable->type()->clone());
            variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto dereferenceExpression = makeUniqueRef<AST::DereferenceExpression>(Lexer::Token(dotExpression.origin()), WTFMove(variableReference));
            ASSERT(pointerVariable->type());
            dereferenceExpression->setType(downcast<AST::PointerType>(*pointerVariable->type()).elementType().clone());
            dereferenceExpression->setTypeAnnotation(AST::LeftValue { downcast<AST::PointerType>(*pointerVariable->type()).addressSpace() });
            return dereferenceExpression;
        }, [&]() -> UniqueRef<AST::Expression> {
            auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(pointerVariable));
            ASSERT(pointerVariable->type());
            variableReference->setType(pointerVariable->type()->clone());
            variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?
            return variableReference;
        });

        if (!assignmentExpression)
            return WTF::nullopt;

        expressions.append(WTFMove(*assignmentExpression));
    }

    Vector<UniqueRef<AST::VariableDeclaration>> variableDeclarations;
    variableDeclarations.append(WTFMove(pointerVariable));
    for (auto& intermediateVariable : intermediateVariables)
        variableDeclarations.append(WTFMove(intermediateVariable));

    return {{ innerLeftExpression, WTFMove(expressions), WTFMove(variableDeclarations) }};
}

void PropertyResolver::visit(AST::AssignmentExpression& assignmentExpression)
{
    if (assignmentExpression.left().typeAnnotation().leftAddressSpace()) {
        simplifyLeftValue(assignmentExpression.left());
        checkErrorAndVisit(assignmentExpression.right());
        return;
    }
    ASSERT(!assignmentExpression.left().typeAnnotation().isRightValue());
    if (!is<AST::DotExpression>(assignmentExpression.left())) {
        setError(); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198163 Make this work with index expressions.
        return;
    }

    auto type = assignmentExpression.right().resolvedType().clone();

    checkErrorAndVisit(assignmentExpression.right());

    auto modifyResult = modify(downcast<AST::DotExpression>(assignmentExpression.left()), [&](Optional<UniqueRef<AST::Expression>>&&) -> Optional<ModificationResult> {
        return {{ Vector<UniqueRef<AST::Expression>>(), assignmentExpression.takeRight() }};
    });

    if (!modifyResult) {
        setError();
        return;
    }
    simplifyLeftValue(modifyResult->innerLeftValue);

    Lexer::Token origin = assignmentExpression.origin();
    auto* commaExpression = AST::replaceWith<AST::CommaExpression>(assignmentExpression, WTFMove(origin), WTFMove(modifyResult->expressions));
    commaExpression->setType(WTFMove(type));
    commaExpression->setTypeAnnotation(AST::RightValue());

    for (auto& variableDeclaration : modifyResult->variableDeclarations)
        m_variableDeclarations.append(WTFMove(variableDeclaration));
}

void PropertyResolver::visit(AST::ReadModifyWriteExpression& readModifyWriteExpression)
{
    if (readModifyWriteExpression.leftValue().typeAnnotation().leftAddressSpace()) {
        // Consider a++;
        // This would get transformed into:
        //
        // p = &a;
        // oldValue = *p;
        // newValue = ...;
        // *p = newValue;

        auto baseType = readModifyWriteExpression.leftValue().resolvedType().clone();
        auto pointerType = makeUniqueRef<AST::PointerType>(Lexer::Token(readModifyWriteExpression.leftValue().origin()), *readModifyWriteExpression.leftValue().typeAnnotation().leftAddressSpace(), baseType->clone());

        auto pointerVariable = makeUniqueRef<AST::VariableDeclaration>(Lexer::Token(readModifyWriteExpression.leftValue().origin()), AST::Qualifiers(), UniqueRef<AST::UnnamedType>(pointerType->clone()), String(), WTF::nullopt, WTF::nullopt);

        Vector<UniqueRef<AST::Expression>> expressions;

        {
            auto origin = Lexer::Token(readModifyWriteExpression.leftValue().origin());
            auto makePointerExpression = makeUniqueRef<AST::MakePointerExpression>(Lexer::Token(origin), readModifyWriteExpression.takeLeftValue());
            makePointerExpression->setType(pointerType->clone());
            makePointerExpression->setTypeAnnotation(AST::RightValue());

            auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(pointerVariable));
            variableReference->setType(pointerType->clone());
            variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(WTFMove(origin), WTFMove(variableReference), WTFMove(makePointerExpression));
            assignmentExpression->setType(pointerType->clone());
            assignmentExpression->setTypeAnnotation(AST::RightValue());

            expressions.append(WTFMove(assignmentExpression));
        }

        {
            auto variableReference1 = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(pointerVariable));
            variableReference1->setType(pointerType->clone());
            variableReference1->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto dereferenceExpression = makeUniqueRef<AST::DereferenceExpression>(Lexer::Token(readModifyWriteExpression.origin()), WTFMove(variableReference1));
            dereferenceExpression->setType(baseType->clone());
            dereferenceExpression->setTypeAnnotation(AST::RightValue());

            auto variableReference2 = readModifyWriteExpression.oldVariableReference();
            variableReference2->setType(baseType->clone());
            variableReference2->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(Lexer::Token(readModifyWriteExpression.origin()), WTFMove(variableReference2), WTFMove(dereferenceExpression));
            assignmentExpression->setType(baseType->clone());
            assignmentExpression->setTypeAnnotation(AST::RightValue());

            expressions.append(WTFMove(assignmentExpression));
        }

        {
            auto variableReference = readModifyWriteExpression.newVariableReference();
            variableReference->setType(baseType->clone());
            variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto newValueExpression = readModifyWriteExpression.takeNewValueExpression();
            ASSERT(newValueExpression); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198170 Relax this constraint.
            auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(Lexer::Token(readModifyWriteExpression.origin()), WTFMove(variableReference), WTFMove(*newValueExpression));
            assignmentExpression->setType(baseType->clone());
            assignmentExpression->setTypeAnnotation(AST::RightValue());

            expressions.append(WTFMove(assignmentExpression));
        }

        {
            auto variableReference1 = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(pointerVariable));
            variableReference1->setType(pointerType->clone());
            variableReference1->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto dereferenceExpression = makeUniqueRef<AST::DereferenceExpression>(Lexer::Token(readModifyWriteExpression.origin()), WTFMove(variableReference1));
            dereferenceExpression->setType(baseType->clone());
            dereferenceExpression->setTypeAnnotation(AST::RightValue());

            auto variableReference2 = readModifyWriteExpression.newVariableReference();
            variableReference2->setType(baseType->clone());
            variableReference2->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(Lexer::Token(readModifyWriteExpression.origin()), WTFMove(dereferenceExpression), WTFMove(variableReference2));
            assignmentExpression->setType(baseType->clone());
            assignmentExpression->setTypeAnnotation(AST::RightValue());

            expressions.append(WTFMove(assignmentExpression));
        }

        auto resultExpression = readModifyWriteExpression.takeResultExpression();
        ASSERT(resultExpression); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198170 Be resilient to this being null.
        auto type = (*resultExpression)->resolvedType().clone();
        expressions.append(WTFMove(*resultExpression));

        UniqueRef<AST::VariableDeclaration> oldVariableDeclaration = readModifyWriteExpression.takeOldValue();
        UniqueRef<AST::VariableDeclaration> newVariableDeclaration = readModifyWriteExpression.takeNewValue();

        Lexer::Token origin = readModifyWriteExpression.origin();
        auto* commaExpression = AST::replaceWith<AST::CommaExpression>(readModifyWriteExpression, WTFMove(origin), WTFMove(expressions));
        commaExpression->setType(WTFMove(type));
        commaExpression->setTypeAnnotation(AST::RightValue());

        m_variableDeclarations.append(WTFMove(pointerVariable));
        m_variableDeclarations.append(WTFMove(oldVariableDeclaration));
        m_variableDeclarations.append(WTFMove(newVariableDeclaration));
        return;
    }

    ASSERT(!readModifyWriteExpression.leftValue().typeAnnotation().isRightValue());
    if (!is<AST::DotExpression>(readModifyWriteExpression.leftValue())) {
        setError(); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198163 Make this work with index expressions.
        return;
    }
    auto modifyResult = modify(downcast<AST::DotExpression>(readModifyWriteExpression.leftValue()), [&](Optional<UniqueRef<AST::Expression>>&& lastGetterCallExpression) -> Optional<ModificationResult> {
        Vector<UniqueRef<AST::Expression>> expressions;
        if (!lastGetterCallExpression)
            return WTF::nullopt;

        {
            auto variableReference = readModifyWriteExpression.oldVariableReference();
            variableReference->setType(readModifyWriteExpression.leftValue().resolvedType().clone());
            variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(Lexer::Token(readModifyWriteExpression.leftValue().origin()), WTFMove(variableReference), WTFMove(*lastGetterCallExpression));
            assignmentExpression->setType(readModifyWriteExpression.leftValue().resolvedType().clone());
            assignmentExpression->setTypeAnnotation(AST::RightValue());

            expressions.append(WTFMove(assignmentExpression));
        }

        {
            auto variableReference = readModifyWriteExpression.newVariableReference();
            variableReference->setType(readModifyWriteExpression.leftValue().resolvedType().clone());
            variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread }); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198169 Is this right?

            auto newValueExpression = readModifyWriteExpression.takeNewValueExpression();
            ASSERT(newValueExpression); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198170 Relax this constraint
            auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(Lexer::Token(readModifyWriteExpression.leftValue().origin()), WTFMove(variableReference), WTFMove(*newValueExpression));
            assignmentExpression->setType(readModifyWriteExpression.leftValue().resolvedType().clone());
            assignmentExpression->setTypeAnnotation(AST::RightValue());

            expressions.append(WTFMove(assignmentExpression));
        }

        return {{ WTFMove(expressions), readModifyWriteExpression.newVariableReference() }};
    });

    if (!modifyResult) {
        setError();
        return;
    }
    simplifyLeftValue(modifyResult->innerLeftValue);

    auto resultExpression = readModifyWriteExpression.takeResultExpression();
    ASSERT(resultExpression); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198170 Be resilient to this being null.
    auto type = (*resultExpression)->resolvedType().clone();
    modifyResult->expressions.append(WTFMove(*resultExpression));

    UniqueRef<AST::VariableDeclaration> oldVariableDeclaration = readModifyWriteExpression.takeOldValue();
    UniqueRef<AST::VariableDeclaration> newVariableDeclaration = readModifyWriteExpression.takeNewValue();

    Lexer::Token origin = readModifyWriteExpression.origin();
    auto* commaExpression = AST::replaceWith<AST::CommaExpression>(readModifyWriteExpression, WTFMove(origin), WTFMove(modifyResult->expressions));
    commaExpression->setType(WTFMove(type));
    commaExpression->setTypeAnnotation(AST::RightValue());

    for (auto& variableDeclaration : modifyResult->variableDeclarations)
        m_variableDeclarations.append(WTFMove(variableDeclaration));
    m_variableDeclarations.append(WTFMove(oldVariableDeclaration));
    m_variableDeclarations.append(WTFMove(newVariableDeclaration));
}

bool PropertyResolver::simplifyRightValue(AST::DotExpression& dotExpression)
{
    Lexer::Token origin = dotExpression.origin();

    if (auto* anderFunction = dotExpression.anderFunction()) {
        auto& base = dotExpression.base();
        if (auto leftAddressSpace = base.typeAnnotation().leftAddressSpace()) {
            auto makePointerExpression = makeUniqueRef<AST::MakePointerExpression>(Lexer::Token(origin), dotExpression.takeBase());
            makePointerExpression->setType(makeUniqueRef<AST::PointerType>(Lexer::Token(origin), *leftAddressSpace, base.resolvedType().clone()));
            makePointerExpression->setTypeAnnotation(AST::RightValue());

            Vector<UniqueRef<AST::Expression>> arguments;
            arguments.append(WTFMove(makePointerExpression));
            auto callExpression = makeUniqueRef<AST::CallExpression>(Lexer::Token(origin), String(anderFunction->name()), WTFMove(arguments));
            callExpression->setType(anderFunction->type().clone());
            callExpression->setTypeAnnotation(AST::RightValue());
            callExpression->setFunction(*anderFunction);

            auto* dereferenceExpression = AST::replaceWith<AST::DereferenceExpression>(dotExpression, WTFMove(origin), WTFMove(callExpression));
            dereferenceExpression->setType(downcast<AST::PointerType>(anderFunction->type()).elementType().clone());
            dereferenceExpression->setTypeAnnotation(AST::LeftValue { downcast<AST::PointerType>(anderFunction->type()).addressSpace() });
            return true;
        }

        // We have an ander, but no left value to call it on. Let's save the value into a temporary variable to create a left value.
        // This is effectively inlining the functions the spec says are generated.
        if (!dotExpression.threadAnderFunction())
            return false;

        auto variableDeclaration = makeUniqueRef<AST::VariableDeclaration>(Lexer::Token(origin), AST::Qualifiers(), base.resolvedType().clone(), String(), WTF::nullopt, WTF::nullopt);

        auto variableReference1 = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(variableDeclaration));
        variableReference1->setType(base.resolvedType().clone());
        variableReference1->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread });

        auto assignmentExpression = makeUniqueRef<AST::AssignmentExpression>(Lexer::Token(origin), WTFMove(variableReference1), dotExpression.takeBase());
        assignmentExpression->setType(base.resolvedType().clone());
        assignmentExpression->setTypeAnnotation(AST::RightValue());

        auto variableReference2 = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(variableDeclaration));
        variableReference2->setType(base.resolvedType().clone());
        variableReference2->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread });

        auto makePointerExpression = makeUniqueRef<AST::MakePointerExpression>(Lexer::Token(origin), WTFMove(variableReference2));
        makePointerExpression->setType(makeUniqueRef<AST::PointerType>(Lexer::Token(origin), AST::AddressSpace::Thread, base.resolvedType().clone()));
        makePointerExpression->setTypeAnnotation(AST::RightValue());

        Vector<UniqueRef<AST::Expression>> arguments;
        arguments.append(WTFMove(makePointerExpression));
        auto callExpression = makeUniqueRef<AST::CallExpression>(Lexer::Token(origin), String(anderFunction->name()), WTFMove(arguments));
        callExpression->setType(anderFunction->type().clone());
        callExpression->setTypeAnnotation(AST::RightValue());
        callExpression->setFunction(*anderFunction);

        auto dereferenceExpression = makeUniqueRef<AST::DereferenceExpression>(WTFMove(origin), WTFMove(callExpression));
        dereferenceExpression->setType(downcast<AST::PointerType>(anderFunction->type()).elementType().clone());
        dereferenceExpression->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread });

        Vector<UniqueRef<AST::Expression>> expressions;
        expressions.append(WTFMove(assignmentExpression));
        expressions.append(WTFMove(dereferenceExpression));
        auto* commaExpression = AST::replaceWith<AST::CommaExpression>(dotExpression, WTFMove(origin), WTFMove(expressions));
        commaExpression->setType(downcast<AST::PointerType>(anderFunction->type()).elementType().clone());
        commaExpression->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread });

        m_variableDeclarations.append(WTFMove(variableDeclaration));
        return true;
    }

    ASSERT(dotExpression.getterFunction());
    auto& getterFunction = *dotExpression.getterFunction();
    Vector<UniqueRef<AST::Expression>> arguments;
    arguments.append(dotExpression.takeBase());
    auto* callExpression = AST::replaceWith<AST::CallExpression>(dotExpression, WTFMove(origin), String(getterFunction.name()), WTFMove(arguments));
    callExpression->setFunction(getterFunction);
    callExpression->setType(getterFunction.type().clone());
    callExpression->setTypeAnnotation(AST::RightValue());
    return true;
}

class LeftValueSimplifier : public Visitor {
public:
    void visit(AST::DotExpression&) override;
    void visit(AST::DereferenceExpression&) override;

private:
};

void LeftValueSimplifier::visit(AST::DotExpression& dotExpression)
{
    Visitor::visit(dotExpression);
    ASSERT(dotExpression.base().typeAnnotation().leftAddressSpace());
    ASSERT(dotExpression.anderFunction());

    Lexer::Token origin = dotExpression.origin();
    auto* anderFunction = dotExpression.anderFunction();
    auto& base = dotExpression.base();
    auto leftAddressSpace = *dotExpression.base().typeAnnotation().leftAddressSpace();
    auto makePointerExpression = makeUniqueRef<AST::MakePointerExpression>(Lexer::Token(origin), dotExpression.takeBase());
    makePointerExpression->setType(makeUniqueRef<AST::PointerType>(Lexer::Token(origin), leftAddressSpace, base.resolvedType().clone()));
    makePointerExpression->setTypeAnnotation(AST::RightValue());

    Vector<UniqueRef<AST::Expression>> arguments;
    arguments.append(WTFMove(makePointerExpression));
    auto callExpression = makeUniqueRef<AST::CallExpression>(Lexer::Token(origin), String(anderFunction->name()), WTFMove(arguments));
    callExpression->setType(anderFunction->type().clone());
    callExpression->setTypeAnnotation(AST::RightValue());
    callExpression->setFunction(*anderFunction);

    auto* dereferenceExpression = AST::replaceWith<AST::DereferenceExpression>(dotExpression, WTFMove(origin), WTFMove(callExpression));
    dereferenceExpression->setType(downcast<AST::PointerType>(anderFunction->type()).elementType().clone());
    dereferenceExpression->setTypeAnnotation(AST::LeftValue { downcast<AST::PointerType>(anderFunction->type()).addressSpace() });
}

void LeftValueSimplifier::visit(AST::DereferenceExpression& dereferenceExpression)
{
    // Dereference expressions are the only expressions where the children might be more-right than we are.
    // For example, a dereference expression may be a left value but its child may be a call expression which is a right value.
    // LeftValueSimplifier doesn't handle right values, so we instead need to use PropertyResolver.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198170 What about function call arguments?
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
