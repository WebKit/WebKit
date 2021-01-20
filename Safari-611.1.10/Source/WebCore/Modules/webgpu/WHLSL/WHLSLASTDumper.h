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

#if ENABLE(WHLSL_COMPILER)

#include "WHLSLVisitor.h"

#include <wtf/DataLog.h>
#include <wtf/SetForScope.h>
#include <wtf/StringPrintStream.h>

namespace WebCore {

namespace WHLSL {

class ASTDumper : Visitor {
    using Base = Visitor;
public:
    void visit(Program&) override;

    String toString() { return m_out.toString(); }

    void visit(AST::UnnamedType&) override;
    void visit(AST::NamedType&) override;
    void visit(AST::TypeDefinition&) override;
    void visit(AST::StructureDefinition&) override;
    void visit(AST::EnumerationDefinition&) override;
    void visit(AST::FunctionDefinition&) override;
    void visit(AST::NativeFunctionDeclaration&) override;
    void visit(AST::NativeTypeDeclaration&) override;
    void visit(AST::TypeReference&) override;
    void visit(AST::PointerType&) override;
    void visit(AST::ArrayReferenceType&) override;
    void visit(AST::ArrayType&) override;
    void visit(AST::StructureElement&) override;
    void visit(AST::EnumerationMember&) override;
    void visit(AST::FunctionDeclaration&) override;
    void visit(AST::TypeArgument&) override;
    void visit(AST::ReferenceType&) override;
    void visit(AST::Semantic&) override;
    void visit(AST::ConstantExpression&) override;
    void visit(AST::AttributeBlock&) override;
    void visit(AST::BuiltInSemantic&) override;
    void visit(AST::ResourceSemantic&) override;
    void visit(AST::SpecializationConstantSemantic&) override;
    void visit(AST::StageInOutSemantic&) override;
    void visit(AST::IntegerLiteral&) override;
    void visit(AST::UnsignedIntegerLiteral&) override;
    void visit(AST::FloatLiteral&) override;
    void visit(AST::BooleanLiteral&) override;
    void visit(AST::IntegerLiteralType&) override;
    void visit(AST::UnsignedIntegerLiteralType&) override;
    void visit(AST::FloatLiteralType&) override;
    void visit(AST::EnumerationMemberLiteral&) override;
    void visit(AST::FunctionAttribute&) override;
    void visit(AST::NumThreadsFunctionAttribute&) override;
    void visit(AST::Block&) override;
    void visit(AST::StatementList&) override;
    void visit(AST::Statement&) override;
    void visit(AST::Break&) override;
    void visit(AST::Continue&) override;
    void visit(AST::DoWhileLoop&) override;
    void visit(AST::Expression&) override;
    void visit(AST::DotExpression&) override;
    void visit(AST::GlobalVariableReference&) override;
    void visit(AST::IndexExpression&) override;
    void visit(AST::PropertyAccessExpression&) override;
    void visit(AST::EffectfulExpressionStatement&) override;
    void visit(AST::Fallthrough&) override;
    void visit(AST::ForLoop&) override;
    void visit(AST::IfStatement&) override;
    void visit(AST::Return&) override;
    void visit(AST::SwitchCase&) override;
    void visit(AST::SwitchStatement&) override;
    void visit(AST::VariableDeclarationsStatement&) override;
    void visit(AST::WhileLoop&) override;
    void visit(AST::VariableDeclaration&) override;
    void visit(AST::AssignmentExpression&) override;
    void visit(AST::CallExpression&) override;
    void visit(AST::CommaExpression&) override;
    void visit(AST::DereferenceExpression&) override;
    void visit(AST::LogicalExpression&) override;
    void visit(AST::LogicalNotExpression&) override;
    void visit(AST::MakeArrayReferenceExpression&) override;
    void visit(AST::MakePointerExpression&) override;
    void visit(AST::ReadModifyWriteExpression&) override;
    void visit(AST::TernaryExpression&) override;
    void visit(AST::VariableReference&) override;

private:
    struct Indent {
        Indent(ASTDumper& dumper)
            : m_scope(dumper.m_indent, dumper.m_indent + "   ")
        { }
        SetForScope<String> m_scope;
    };

    Indent bumpIndent() { return Indent(*this); }

    friend struct Indent;

    StringPrintStream m_out;
    String m_indent;
};

template <typename T>
ALWAYS_INLINE void dumpASTNode(PrintStream& out, T& value)
{
    ASTDumper dumper;
    dumper.visit(value);
    out.print(dumper.toString());
}
MAKE_PRINT_ADAPTOR(ExpressionDumper, AST::Expression&, dumpASTNode);
MAKE_PRINT_ADAPTOR(StatementDumper, AST::Statement&, dumpASTNode);
MAKE_PRINT_ADAPTOR(ProgramDumper, Program&, dumpASTNode);
MAKE_PRINT_ADAPTOR(StructureDefinitionDumper, AST::StructureDefinition&, dumpASTNode);
MAKE_PRINT_ADAPTOR(FunctionDefinitionDumper, AST::FunctionDefinition&, dumpASTNode);
MAKE_PRINT_ADAPTOR(NativeFunctionDeclarationDumper, AST::NativeFunctionDeclaration&, dumpASTNode);
MAKE_PRINT_ADAPTOR(TypeDumper, AST::UnnamedType&, dumpASTNode);


static ALWAYS_INLINE void dumpAST(Program& program)
{
    dataLogLn(ProgramDumper(program));
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WHLSL_COMPILER)
