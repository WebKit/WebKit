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

#if ENABLE(WHLSL_COMPILER)

#include "WHLSLAST.h"
#include "WHLSLProgram.h"
#include "WHLSLReplaceWith.h"
#include "WHLSLVisitor.h"

namespace WebCore {

namespace WHLSL {

class PropertyResolver : public Visitor {
    void handleLeftHandSideBase(UniqueRef<AST::Expression> base, UniqueRef<AST::Expression>& slot, Vector<UniqueRef<AST::Expression>>& expressions)
    {
        if (!base->mayBeEffectful()) {
            slot = WTFMove(base);
            return;
        }

        auto leftAddressSpace = base->typeAnnotation().leftAddressSpace();
        RELEASE_ASSERT(leftAddressSpace);
        CodeLocation codeLocation = base->codeLocation();
        Ref<AST::UnnamedType> baseType = base->resolvedType();
        Ref<AST::PointerType> pointerType = AST::PointerType::create(codeLocation, *leftAddressSpace, baseType.copyRef());

        UniqueRef<AST::VariableDeclaration> pointerVariable = makeUniqueRef<AST::VariableDeclaration>(codeLocation, AST::Qualifiers { }, pointerType.ptr(), String(), nullptr, nullptr);

        auto makeVariableReference = [&] {
            auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(pointerVariable));
            variableReference->setType(pointerType.copyRef());
            variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread });
            return variableReference;
        };

        {
            auto pointerOfBase = makeUniqueRef<AST::MakePointerExpression>(codeLocation, WTFMove(base), AST::AddressEscapeMode::DoesNotEscape);
            pointerOfBase->setType(pointerType.copyRef());
            pointerOfBase->setTypeAnnotation(AST::RightValue());

            auto assignment = makeUniqueRef<AST::AssignmentExpression>(codeLocation, makeVariableReference(), WTFMove(pointerOfBase));
            assignment->setType(pointerType.copyRef());
            assignment->setTypeAnnotation(AST::RightValue());

            expressions.append(WTFMove(assignment));
        }

        {
            auto dereference = makeUniqueRef<AST::DereferenceExpression>(codeLocation, makeVariableReference());
            dereference->setType(baseType.copyRef());
            dereference->setTypeAnnotation(AST::LeftValue { *leftAddressSpace });

            slot = WTFMove(dereference);
        }

        m_variables.append(WTFMove(pointerVariable));
    }

    void handlePropertyAccess(AST::PropertyAccessExpression& propertyAccess, Vector<UniqueRef<AST::Expression>>& expressions)
    {
        AST::PropertyAccessExpression* currentPtr = &propertyAccess;
        // a.b[c].d will go into this array as [.d, [c], .b]
        Vector<std::reference_wrapper<AST::PropertyAccessExpression>> chain;

        while (true) {
            AST::PropertyAccessExpression& current = *currentPtr;
            chain.append(current);
            if (is<AST::IndexExpression>(current))
                checkErrorAndVisit(downcast<AST::IndexExpression>(current).indexExpression());
            if (!is<AST::PropertyAccessExpression>(current.base()))
                break;
            currentPtr = &downcast<AST::PropertyAccessExpression>(current.base());
        }

        AST::PropertyAccessExpression& current = *currentPtr;

        checkErrorAndVisit(current.base());

        CodeLocation baseCodeLocation = current.base().codeLocation();
        
        if (current.base().typeAnnotation().isRightValue()) {
            UniqueRef<AST::VariableDeclaration> copy = makeUniqueRef<AST::VariableDeclaration>(baseCodeLocation, AST::Qualifiers { }, &current.base().resolvedType(), String(), nullptr, nullptr);
            Ref<AST::UnnamedType> baseType = current.base().resolvedType();
            
            auto makeVariableReference = [&] {
                auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(copy));
                variableReference->setType(baseType.copyRef());
                variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread });
                return variableReference;
            };

            auto assignment = makeUniqueRef<AST::AssignmentExpression>(baseCodeLocation, makeVariableReference(), current.takeBase());
            assignment->setType(baseType.copyRef());
            assignment->setTypeAnnotation(AST::RightValue());

            expressions.append(WTFMove(assignment));
            
            current.baseReference() = makeVariableReference();

            m_variables.append(WTFMove(copy));
        } else
            handleLeftHandSideBase(current.takeBase(), current.baseReference(), expressions);

        for (size_t i = chain.size(); i--; ) {
            auto& access = chain[i].get();
            if (is<AST::IndexExpression>(access) && downcast<AST::IndexExpression>(access).indexExpression().mayBeEffectful()) {
                auto& indexExpression = downcast<AST::IndexExpression>(access);

                Ref<AST::UnnamedType> indexType = indexExpression.indexExpression().resolvedType();

                UniqueRef<AST::VariableDeclaration> indexVariable = makeUniqueRef<AST::VariableDeclaration>(access.codeLocation(), AST::Qualifiers { }, indexType.ptr(), String(), nullptr, nullptr);

                auto makeVariableReference = [&] {
                    auto variableReference = makeUniqueRef<AST::VariableReference>(AST::VariableReference::wrap(indexVariable));
                    variableReference->setType(indexType.copyRef());
                    variableReference->setTypeAnnotation(AST::LeftValue { AST::AddressSpace::Thread });
                    return variableReference;
                };

                {
                    auto assignment = makeUniqueRef<AST::AssignmentExpression>(baseCodeLocation, makeVariableReference(), indexExpression.takeIndex());
                    assignment->setType(indexType.copyRef());
                    assignment->setTypeAnnotation(AST::RightValue());

                    expressions.append(WTFMove(assignment));
                }

                indexExpression.indexReference() = makeVariableReference();

                m_variables.append(WTFMove(indexVariable));
            }
        }
    }

    void handlePropertyAccess(AST::PropertyAccessExpression& propertyAccess)
    {
        Vector<UniqueRef<AST::Expression>> expressions;

        handlePropertyAccess(propertyAccess, expressions);

        Ref<AST::UnnamedType> accessType = propertyAccess.resolvedType();

        AST::CommaExpression* comma;
        CodeLocation codeLocation = propertyAccess.codeLocation();
        if (is<AST::IndexExpression>(propertyAccess)) {
            auto& indexExpression = downcast<AST::IndexExpression>(propertyAccess);

            auto newIndexExpression = makeUniqueRef<AST::IndexExpression>(codeLocation, indexExpression.takeBase(), indexExpression.takeIndex());
            newIndexExpression->setType(indexExpression.resolvedType());
            newIndexExpression->setTypeAnnotation(AST::TypeAnnotation(indexExpression.typeAnnotation()));

            expressions.append(WTFMove(newIndexExpression));

            comma = AST::replaceWith<AST::CommaExpression>(indexExpression, codeLocation, WTFMove(expressions));
        } else {
            RELEASE_ASSERT(is<AST::DotExpression>(propertyAccess));
            auto& dotExpression = downcast<AST::DotExpression>(propertyAccess);

            auto newDotExpression = makeUniqueRef<AST::DotExpression>(codeLocation, dotExpression.takeBase(), String(dotExpression.fieldName()));
            newDotExpression->setType(dotExpression.resolvedType());
            newDotExpression->setTypeAnnotation(AST::TypeAnnotation(dotExpression.typeAnnotation()));

            expressions.append(WTFMove(newDotExpression));

            comma = AST::replaceWith<AST::CommaExpression>(dotExpression, codeLocation, WTFMove(expressions));
        }

        comma->setType(WTFMove(accessType));
        comma->setTypeAnnotation(AST::RightValue());
    }

public:
    void visit(AST::DotExpression& dotExpression) override
    {
        handlePropertyAccess(dotExpression);
    }

    void visit(AST::IndexExpression& indexExpression) override
    {
        handlePropertyAccess(indexExpression);
    }

    void visit(AST::ReadModifyWriteExpression& readModifyWrite) override
    {
        checkErrorAndVisit(readModifyWrite.newValueExpression());
        checkErrorAndVisit(readModifyWrite.resultExpression());

        Vector<UniqueRef<AST::Expression>> expressions;

        CodeLocation codeLocation = readModifyWrite.codeLocation();

        Ref<AST::UnnamedType> type = readModifyWrite.resolvedType();

        if (is<AST::PropertyAccessExpression>(readModifyWrite.leftValue()))
            handlePropertyAccess(downcast<AST::PropertyAccessExpression>(readModifyWrite.leftValue()), expressions);
        else
            handleLeftHandSideBase(readModifyWrite.takeLeftValue(), readModifyWrite.leftValueReference(), expressions);

        {
            UniqueRef<AST::ReadModifyWriteExpression> newReadModifyWrite = makeUniqueRef<AST::ReadModifyWriteExpression>(
                readModifyWrite.codeLocation(), readModifyWrite.takeLeftValue(), readModifyWrite.takeOldValue(), readModifyWrite.takeNewValue());
            newReadModifyWrite->setNewValueExpression(readModifyWrite.takeNewValueExpression());
            newReadModifyWrite->setResultExpression(readModifyWrite.takeResultExpression());
            newReadModifyWrite->setType(type.copyRef());
            newReadModifyWrite->setTypeAnnotation(AST::TypeAnnotation(readModifyWrite.typeAnnotation()));

            expressions.append(WTFMove(newReadModifyWrite));
        }

        auto* comma = AST::replaceWith<AST::CommaExpression>(readModifyWrite, codeLocation, WTFMove(expressions));
        comma->setType(WTFMove(type));
        comma->setTypeAnnotation(AST::RightValue());
    }

    void visit(AST::FunctionDefinition& functionDefinition) override
    {
        RELEASE_ASSERT(m_variables.isEmpty());

        checkErrorAndVisit(static_cast<AST::FunctionDeclaration&>(functionDefinition));
        checkErrorAndVisit(functionDefinition.block());

        if (!m_variables.isEmpty()) {
            functionDefinition.block().statements().insert(0, 
                makeUniqueRef<AST::VariableDeclarationsStatement>(functionDefinition.block().codeLocation(), WTFMove(m_variables)));
        }
    }

private:
    AST::VariableDeclarations m_variables;
};

void resolveProperties(Program& program)
{
    // The goal of this phase is two allow two things:
    // 1. For property access expressions, metal codegen should be allowed to evaluate
    // the base, and if it's an index expression, the index, as many times as needed.
    // So this patch ensures that if Metal evaluates such things, effects aren't performed
    // more than once.
    //
    // 2. For ReadModifyWrite expressions, metal codegen should be able to evaluate the
    // leftValueExpression as many times as it'd like without performing the effects of
    // leftValueExpression more than once.
    //
    // We do these things because it's convenient for metal codegen to be able to rely on
    // this with the way it structures the generated code.
    PropertyResolver resolver;
    for (auto& function : program.functionDefinitions())
        resolver.visit(function);
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WHLSL_COMPILER)
