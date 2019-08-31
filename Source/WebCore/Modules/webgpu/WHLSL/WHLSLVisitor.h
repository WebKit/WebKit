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

#pragma once

#if ENABLE(WEBGPU)

#include "WHLSLError.h"
#include "WHLSLFunctionAttribute.h"
#include "WHLSLSemantic.h"
#include "WHLSLTypeArgument.h"
#include <wtf/Expected.h>

namespace WebCore {

namespace WHLSL {

class Program;

namespace AST {

class TypeDefinition;
class StructureDefinition;
class EnumerationDefinition;
class FunctionDefinition;
class NativeFunctionDeclaration;
class NativeTypeDeclaration;
class TypeReference;
class PointerType;
class ArrayReferenceType;
class ArrayType;
class StructureElement;
class EnumerationMember;
class FunctionDeclaration;
class ReferenceType;
class ConstantExpression;
class BuiltInSemantic;
class ResourceSemantic;
class SpecializationConstantSemantic;
class StageInOutSemantic;
class IntegerLiteral;
class UnsignedIntegerLiteral;
class FloatLiteral;
class BooleanLiteral;
class EnumerationMemberLiteral;
class NumThreadsFunctionAttribute;
class Block;
class StatementList;
class Statement;
class Break;
class Continue;
class DoWhileLoop;
class Expression;
class DotExpression;
class GlobalVariableReference;
class IndexExpression;
class PropertyAccessExpression;
class EffectfulExpressionStatement;
class Fallthrough;
class ForLoop;
class IfStatement;
class Return;
class SwitchCase;
class SwitchStatement;
class VariableDeclarationsStatement;
class WhileLoop;
class VariableDeclaration;
class AssignmentExpression;
class CallExpression;
class CommaExpression;
class DereferenceExpression;
class LogicalExpression;
class LogicalNotExpression;
class MakeArrayReferenceExpression;
class MakePointerExpression;
class ReadModifyWriteExpression;
class TernaryExpression;
class VariableReference;
class UnnamedType;
class NamedType;

}

class Visitor {
public:
    virtual ~Visitor() = default;

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198171 Add a way to visit a const Program

    virtual void visit(Program&);
    virtual void visit(AST::UnnamedType&);
    virtual void visit(AST::NamedType&);
    virtual void visit(AST::TypeDefinition&);
    virtual void visit(AST::StructureDefinition&);
    virtual void visit(AST::EnumerationDefinition&);
    virtual void visit(AST::FunctionDefinition&);
    virtual void visit(AST::NativeFunctionDeclaration&);
    virtual void visit(AST::NativeTypeDeclaration&);
    virtual void visit(AST::TypeReference&);
    virtual void visit(AST::PointerType&);
    virtual void visit(AST::ArrayReferenceType&);
    virtual void visit(AST::ArrayType&);
    virtual void visit(AST::StructureElement&);
    virtual void visit(AST::EnumerationMember&);
    virtual void visit(AST::FunctionDeclaration&);
    virtual void visit(AST::TypeArgument&);
    virtual void visit(AST::ReferenceType&);
    virtual void visit(AST::Semantic&);
    virtual void visit(AST::ConstantExpression&);
    virtual void visit(AST::AttributeBlock&);
    virtual void visit(AST::BuiltInSemantic&);
    virtual void visit(AST::ResourceSemantic&);
    virtual void visit(AST::SpecializationConstantSemantic&);
    virtual void visit(AST::StageInOutSemantic&);
    virtual void visit(AST::IntegerLiteral&);
    virtual void visit(AST::UnsignedIntegerLiteral&);
    virtual void visit(AST::FloatLiteral&);
    virtual void visit(AST::BooleanLiteral&);
    virtual void visit(AST::IntegerLiteralType&);
    virtual void visit(AST::UnsignedIntegerLiteralType&);
    virtual void visit(AST::FloatLiteralType&);
    virtual void visit(AST::EnumerationMemberLiteral&);
    virtual void visit(AST::FunctionAttribute&);
    virtual void visit(AST::NumThreadsFunctionAttribute&);
    virtual void visit(AST::Block&);
    virtual void visit(AST::StatementList&);
    virtual void visit(AST::Statement&);
    virtual void visit(AST::Break&);
    virtual void visit(AST::Continue&);
    virtual void visit(AST::DoWhileLoop&);
    virtual void visit(AST::Expression&);
    virtual void visit(AST::DotExpression&);
    virtual void visit(AST::GlobalVariableReference&);
    virtual void visit(AST::IndexExpression&);
    virtual void visit(AST::PropertyAccessExpression&);
    virtual void visit(AST::EffectfulExpressionStatement&);
    virtual void visit(AST::Fallthrough&);
    virtual void visit(AST::ForLoop&);
    virtual void visit(AST::IfStatement&);
    virtual void visit(AST::Return&);
    virtual void visit(AST::SwitchCase&);
    virtual void visit(AST::SwitchStatement&);
    virtual void visit(AST::VariableDeclarationsStatement&);
    virtual void visit(AST::WhileLoop&);
    virtual void visit(AST::VariableDeclaration&);
    virtual void visit(AST::AssignmentExpression&);
    virtual void visit(AST::CallExpression&);
    virtual void visit(AST::CommaExpression&);
    virtual void visit(AST::DereferenceExpression&);
    virtual void visit(AST::LogicalExpression&);
    virtual void visit(AST::LogicalNotExpression&);
    virtual void visit(AST::MakeArrayReferenceExpression&);
    virtual void visit(AST::MakePointerExpression&);
    virtual void visit(AST::ReadModifyWriteExpression&);
    virtual void visit(AST::TernaryExpression&);
    virtual void visit(AST::VariableReference&);

    bool hasError() const { return !m_expectedError; }
    Expected<void, Error> result() { return m_expectedError; }

    template<typename T> void checkErrorAndVisit(T& x)
    {
        if (!hasError())
            visit(x);
    }

protected:
    void setError(Error error)
    {
        ASSERT(!hasError());
        m_expectedError = makeUnexpected(error);
    }

private:
    Expected<void, Error> m_expectedError;
};

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
