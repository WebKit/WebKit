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
#include "WHLSLExpression.h"

#if ENABLE(WHLSL_COMPILER)

#include "WHLSLAST.h"

namespace WebCore {

namespace WHLSL {

namespace AST {

void Expression::destroy(Expression& expression)
{
    switch (expression.kind()) {
    case Expression::Kind::Assignment:
        delete &downcast<AssignmentExpression>(expression);
        break;
    case Expression::Kind::BooleanLiteral:
        delete &downcast<BooleanLiteral>(expression);
        break;
    case Expression::Kind::Call:
        delete &downcast<CallExpression>(expression);
        break;
    case Expression::Kind::Comma:
        delete &downcast<CommaExpression>(expression);
        break;
    case Expression::Kind::Dereference:
        delete &downcast<DereferenceExpression>(expression);
        break;
    case Expression::Kind::FloatLiteral:
        delete &downcast<FloatLiteral>(expression);
        break;
    case Expression::Kind::IntegerLiteral:
        delete &downcast<IntegerLiteral>(expression);
        break;
    case Expression::Kind::Logical:
        delete &downcast<LogicalExpression>(expression);
        break;
    case Expression::Kind::LogicalNot:
        delete &downcast<LogicalNotExpression>(expression);
        break;
    case Expression::Kind::MakeArrayReference:
        delete &downcast<MakeArrayReferenceExpression>(expression);
        break;
    case Expression::Kind::MakePointer:
        delete &downcast<MakePointerExpression>(expression);
        break;
    case Expression::Kind::Dot:
        delete &downcast<DotExpression>(expression);
        break;
    case Expression::Kind::GlobalVariableReference:
        delete &downcast<GlobalVariableReference>(expression);
        break;
    case Expression::Kind::Index:
        delete &downcast<IndexExpression>(expression);
        break;
    case Expression::Kind::ReadModifyWrite:
        delete &downcast<ReadModifyWriteExpression>(expression);
        break;
    case Expression::Kind::Ternary:
        delete &downcast<TernaryExpression>(expression);
        break;
    case Expression::Kind::UnsignedIntegerLiteral:
        delete &downcast<UnsignedIntegerLiteral>(expression);
        break;
    case Expression::Kind::EnumerationMemberLiteral:
        delete &downcast<EnumerationMemberLiteral>(expression);
        break;
    case Expression::Kind::VariableReference:
        delete &downcast<VariableReference>(expression);
        break;
    }
}

void Expression::destruct(Expression& expression)
{
    switch (expression.kind()) {
    case Expression::Kind::Assignment:
        downcast<AssignmentExpression>(expression).~AssignmentExpression();
        break;
    case Expression::Kind::BooleanLiteral:
        downcast<BooleanLiteral>(expression).~BooleanLiteral();
        break;
    case Expression::Kind::Call:
        downcast<CallExpression>(expression).~CallExpression();
        break;
    case Expression::Kind::Comma:
        downcast<CommaExpression>(expression).~CommaExpression();
        break;
    case Expression::Kind::Dereference:
        downcast<DereferenceExpression>(expression).~DereferenceExpression();
        break;
    case Expression::Kind::FloatLiteral:
        downcast<FloatLiteral>(expression).~FloatLiteral();
        break;
    case Expression::Kind::IntegerLiteral:
        downcast<IntegerLiteral>(expression).~IntegerLiteral();
        break;
    case Expression::Kind::Logical:
        downcast<LogicalExpression>(expression).~LogicalExpression();
        break;
    case Expression::Kind::LogicalNot:
        downcast<LogicalNotExpression>(expression).~LogicalNotExpression();
        break;
    case Expression::Kind::MakeArrayReference:
        downcast<MakeArrayReferenceExpression>(expression).~MakeArrayReferenceExpression();
        break;
    case Expression::Kind::MakePointer:
        downcast<MakePointerExpression>(expression).~MakePointerExpression();
        break;
    case Expression::Kind::Dot:
        downcast<DotExpression>(expression).~DotExpression();
        break;
    case Expression::Kind::GlobalVariableReference:
        downcast<GlobalVariableReference>(expression).~GlobalVariableReference();
        break;
    case Expression::Kind::Index:
        downcast<IndexExpression>(expression).~IndexExpression();
        break;
    case Expression::Kind::ReadModifyWrite:
        downcast<ReadModifyWriteExpression>(expression).~ReadModifyWriteExpression();
        break;
    case Expression::Kind::Ternary:
        downcast<TernaryExpression>(expression).~TernaryExpression();
        break;
    case Expression::Kind::UnsignedIntegerLiteral:
        downcast<UnsignedIntegerLiteral>(expression).~UnsignedIntegerLiteral();
        break;
    case Expression::Kind::EnumerationMemberLiteral:
        downcast<EnumerationMemberLiteral>(expression).~EnumerationMemberLiteral();
        break;
    case Expression::Kind::VariableReference:
        downcast<VariableReference>(expression).~VariableReference();
        break;
    }
}

bool Expression::mayBeEffectful() const
{
    auto& expression = const_cast<Expression&>(*this);

    switch (expression.kind()) {
    case Expression::Kind::BooleanLiteral:
    case Expression::Kind::FloatLiteral:
    case Expression::Kind::IntegerLiteral:
    case Expression::Kind::UnsignedIntegerLiteral:
    case Expression::Kind::EnumerationMemberLiteral:
    case Expression::Kind::GlobalVariableReference:
    case Expression::Kind::VariableReference:
        return false;

    case Expression::Kind::Dereference:
        return downcast<DereferenceExpression>(expression).pointer().mayBeEffectful();

    case Expression::Kind::Logical:
        return downcast<LogicalExpression>(expression).left().mayBeEffectful() || downcast<LogicalExpression>(expression).right().mayBeEffectful();

    case Expression::Kind::LogicalNot:
        return downcast<LogicalNotExpression>(expression).operand().mayBeEffectful();

    case Expression::Kind::MakeArrayReference:
        return downcast<MakeArrayReferenceExpression>(expression).leftValue().mayBeEffectful();

    case Expression::Kind::MakePointer:
        return downcast<MakePointerExpression>(expression).leftValue().mayBeEffectful();

    case Expression::Kind::Dot:
        return downcast<DotExpression>(expression).base().mayBeEffectful();

    case Expression::Kind::Index:
        return downcast<IndexExpression>(expression).base().mayBeEffectful() || downcast<IndexExpression>(expression).indexExpression().mayBeEffectful();

    default:
        break;
    }

    return true;
}

} // namespace AST

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WHLSL_COMPILER)
