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
#include "WHLSLASTDumper.h"

#if ENABLE(WEBGPU)

#include "WHLSLAST.h"
#include "WHLSLProgram.h"

namespace WebCore {

namespace WHLSL {

void ASTDumper::visit(Program& program)
{
    m_out.println();

    for (size_t i = 0; i < program.nativeTypeDeclarations().size(); ++i)
        visit(program.nativeTypeDeclarations()[i]);
    if (program.nativeTypeDeclarations().size())
        m_out.print("\n\n");

    for (size_t i = 0; i < program.nativeFunctionDeclarations().size(); ++i)
        visit(program.nativeFunctionDeclarations()[i]);
    if (program.nativeFunctionDeclarations().size())
        m_out.print("\n\n");

    for (size_t i = 0; i < program.typeDefinitions().size(); ++i)
        visit(program.typeDefinitions()[i]);
    if (program.typeDefinitions().size())
        m_out.print("\n\n");

    for (size_t i = 0; i < program.structureDefinitions().size(); ++i)
        visit(program.structureDefinitions()[i]);
    if (program.structureDefinitions().size())
        m_out.print("\n\n");

    for (size_t i = 0; i < program.enumerationDefinitions().size(); ++i)
        visit(program.enumerationDefinitions()[i]);
    if (program.enumerationDefinitions().size())
        m_out.print("\n\n");

    for (size_t i = 0; i < program.functionDefinitions().size(); ++i)
        visit(program.functionDefinitions()[i]);

    m_out.println();
}

void ASTDumper::visit(AST::UnnamedType& unnamedType)
{
    Base::visit(unnamedType);
}

void ASTDumper::visit(AST::NamedType& namedType)
{
    Base::visit(namedType);
}

void ASTDumper::visit(AST::TypeDefinition& typeDefinition)
{
    m_out.print("typedef ", typeDefinition.name(), " = ");
    visit(typeDefinition.type());
    m_out.println(";");
}

void ASTDumper::visit(AST::StructureDefinition& structureDefinition)
{
    m_out.println(m_indent, "struct ", structureDefinition.name(), " {");
    {
        auto indent = bumpIndent();
        for (auto& structureElement : structureDefinition.structureElements())
            visit(structureElement);
    }
    m_out.println(m_indent, "}");
    m_out.println();
}

void ASTDumper::visit(AST::StructureElement& structureElement)
{
    m_out.print(m_indent);
    visit(structureElement.type());
    m_out.print(" ", structureElement.name());
    if (structureElement.semantic())
        visit(*structureElement.semantic());
    m_out.println(";");
}

void ASTDumper::visit(AST::EnumerationDefinition& enumerationDefinition)
{
    m_out.print(m_indent, "enum ");
    visit(enumerationDefinition.type());
    m_out.print(" {");

    {
        auto indent = bumpIndent();
        bool once = false;
        for (auto& enumerationMember : enumerationDefinition.enumerationMembers()) {
            if (once)
                m_out.print(", ");
            m_out.println();
            m_out.print(m_indent);
            visit(enumerationMember);
        }
    }

    m_out.println();
    m_out.println(m_indent, "}");
    m_out.println();
}

void ASTDumper::visit(AST::EnumerationMember& enumerationMember)
{
    m_out.print(enumerationMember.name(), " = ", enumerationMember.value());
}

void ASTDumper::visit(AST::FunctionDefinition& functionDefinition)
{
    m_out.print(m_indent);
    visit(static_cast<AST::FunctionDeclaration&>(functionDefinition));
    visit(functionDefinition.block());
    m_out.print("\n\n");
}

void ASTDumper::visit(AST::NativeFunctionDeclaration& nativeFunctionDeclaration)
{
    m_out.print(m_indent);
    m_out.print("native ");
    visit(static_cast<AST::FunctionDeclaration&>(nativeFunctionDeclaration));
    m_out.println();
}

void ASTDumper::visit(AST::FunctionDeclaration& functionDeclaration)
{
    visit(functionDeclaration.attributeBlock());
    if (functionDeclaration.entryPointType())
        m_out.print(AST::toString(*functionDeclaration.entryPointType()), " ");
    visit(functionDeclaration.type());
    m_out.print(" ", functionDeclaration.name(), "(");
    bool once = false;
    for (auto& parameter : functionDeclaration.parameters()) {
        if (once)
            m_out.print(", ");
        once = true;
        visit(parameter);
    }
    m_out.print(")");
    if (functionDeclaration.semantic())
        visit(*functionDeclaration.semantic());
    m_out.print(" ");
}

void ASTDumper::visit(AST::NativeTypeDeclaration& nativeTypeDeclaration)
{
    m_out.print(m_indent, "native typedef ");
    m_out.print(nativeTypeDeclaration.name());
    if (nativeTypeDeclaration.typeArguments().size()) {
        m_out.print("<");
        bool once = false;
        for (auto& typeArgument : nativeTypeDeclaration.typeArguments()) {
            if (once)
                m_out.print(", ");
            once = true;
            visit(typeArgument);
        }
        m_out.print(">");
    }
    m_out.println(";");
}

void ASTDumper::visit(AST::TypeReference& typeReference)
{
    m_out.print(typeReference.name());

    if (typeReference.typeArguments().size()) {
        bool once = false;
        m_out.print("<");
        for (auto& typeArgument : typeReference.typeArguments()) {
            if (once)
                m_out.print(", ");
            once = true;
            visit(typeArgument);
        }
        m_out.print(">");
    }
}

void ASTDumper::visit(AST::PointerType& pointerType)
{
    visit(static_cast<AST::ReferenceType&>(pointerType));
    m_out.print("*");
}

void ASTDumper::visit(AST::ArrayReferenceType& arrayReferenceType)
{
    visit(static_cast<AST::ReferenceType&>(arrayReferenceType));
    m_out.print("[]");
}

void ASTDumper::visit(AST::ArrayType& arrayType)
{
    visit(arrayType.type());
    m_out.print("[", arrayType.numElements(), "]");
}

void ASTDumper::visit(AST::TypeArgument& typeArgument)
{
    Base::visit(typeArgument);
}

void ASTDumper::visit(AST::ReferenceType& referenceType)
{
    m_out.print(AST::toString(referenceType.addressSpace()), " ");
    visit(referenceType.elementType());
}

void ASTDumper::visit(AST::Semantic& semantic)
{
    m_out.print(" : ");
    WTF::visit(WTF::makeVisitor([&](AST::BuiltInSemantic& builtInSemantic) {
        visit(builtInSemantic);
    }, [&](AST::ResourceSemantic& resourceSemantic) {
        visit(resourceSemantic);
    }, [&](AST::SpecializationConstantSemantic& specializationConstantSemantic) {
        visit(specializationConstantSemantic);
    }, [&](AST::StageInOutSemantic& stageInOutSemantic) {
        visit(stageInOutSemantic);
    }), semantic);
}

void ASTDumper::visit(AST::ConstantExpression& constantExpression)
{
    Base::visit(constantExpression);
}

void ASTDumper::visit(AST::AttributeBlock& attributeBlock)
{
    if (attributeBlock.isEmpty())
        return;

    m_out.print("[");
    for (auto& functionAttribute : attributeBlock)
        visit(functionAttribute);
    m_out.print("] ");
}

void ASTDumper::visit(AST::BuiltInSemantic& builtInSemantic)
{
    m_out.print(builtInSemantic.toString());
}

void ASTDumper::visit(AST::ResourceSemantic& resourceSemantic)
{
    m_out.print(resourceSemantic.toString());
}

void ASTDumper::visit(AST::SpecializationConstantSemantic&)
{
    // FIXME: Handle this when we implement it.
}

void ASTDumper::visit(AST::StageInOutSemantic& stageInOutSemantic)
{
    m_out.print("attribute(", stageInOutSemantic.index(), ")");
}

void ASTDumper::visit(AST::IntegerLiteral& integerLiteral)
{
    m_out.print(integerLiteral.value());
}

void ASTDumper::visit(AST::UnsignedIntegerLiteral& unsignedIntegerLiteral)
{
    m_out.print(unsignedIntegerLiteral.value());
}

void ASTDumper::visit(AST::FloatLiteral& floatLiteral)
{
    m_out.print(floatLiteral.value());
}

void ASTDumper::visit(AST::BooleanLiteral& booleanLiteral)
{
    if (booleanLiteral.value())
        m_out.print("true");
    else
        m_out.print("false");
}

void ASTDumper::visit(AST::IntegerLiteralType&)
{
}

void ASTDumper::visit(AST::UnsignedIntegerLiteralType&)
{
}

void ASTDumper::visit(AST::FloatLiteralType&)
{
}

void ASTDumper::visit(AST::EnumerationMemberLiteral& enumerationMemberLiteral)
{
    m_out.print(enumerationMemberLiteral.left(), ".", enumerationMemberLiteral.right());
}

void ASTDumper::visit(AST::FunctionAttribute& functionAttribute)
{
    WTF::visit(WTF::makeVisitor([&](AST::NumThreadsFunctionAttribute& numThreadsFunctionAttribute) {
        visit(numThreadsFunctionAttribute);
    }), functionAttribute);
}

void ASTDumper::visit(AST::NumThreadsFunctionAttribute& numThreadsAttribute)
{
    m_out.print("numthreads(", numThreadsAttribute.width(), ", ", numThreadsAttribute.height(), ", ", numThreadsAttribute.depth(), ")");
}

void ASTDumper::visit(AST::Block& block)
{
    m_out.println("{");
    {
        auto indent = bumpIndent();
        for (auto& statement : block.statements()) {
            m_out.print(m_indent);
            visit(statement);
            m_out.println(";");
        }
    }
    m_out.print(m_indent, "}");
}

void ASTDumper::visit(AST::Statement& statement)
{
    Base::visit(statement);
}

void ASTDumper::visit(AST::StatementList& statementList)
{
    bool once = false;
    for (auto& statement : statementList.statements()) {
        if (once)
            m_out.print(";\n", m_indent);
        once = true;
        visit(statement);
    }
}

void ASTDumper::visit(AST::Break&)
{
    m_out.print("break");
}

void ASTDumper::visit(AST::Continue&)
{
    m_out.print("continue");
}

void ASTDumper::visit(AST::WhileLoop& whileLoop)
{
    m_out.print("while (");
    visit(whileLoop.conditional());
    m_out.print(")");
    visit(whileLoop.body());
}

void ASTDumper::visit(AST::DoWhileLoop& doWhileLoop)
{
    m_out.print("do ");
    visit(doWhileLoop.body());
    m_out.print(" while(");
    visit(doWhileLoop.conditional());
    m_out.print(")");
}

void ASTDumper::visit(AST::ForLoop& forLoop)
{
    m_out.print("for (");
    visit(forLoop.initialization());
    m_out.print("; ");
    if (forLoop.condition())
        visit(*forLoop.condition());
    m_out.print("; ");
    if (forLoop.increment())
        visit(*forLoop.increment());
    m_out.print(") ");
    visit(forLoop.body());
}

void ASTDumper::visit(AST::Expression& expression)
{
    bool skipParens = is<AST::BooleanLiteral>(expression)
        || is<AST::FloatLiteral>(expression)
        || is<AST::IntegerLiteral>(expression)
        || is<AST::UnsignedIntegerLiteral>(expression)
        || is<AST::EnumerationMemberLiteral>(expression)
        || is<AST::CommaExpression>(expression)
        || is<AST::VariableReference>(expression);

    if (auto* annotation = expression.maybeTypeAnnotation()) {
        if (auto addressSpace = annotation->leftAddressSpace())
            m_out.print("<LV:", AST::toString(*addressSpace));
        else if (annotation->isAbstractLeftValue())
            m_out.print("<ALV");
        else if (annotation->isRightValue())
            m_out.print("<RV");

        m_out.print(", ", expression.resolvedType().toString(), ">");

        skipParens = false;
    }

    if (!skipParens)
        m_out.print("(");
    Base::visit(expression);
    if (!skipParens)
        m_out.print(")");
}

void ASTDumper::visit(AST::DotExpression& dotExpression)
{
    visit(static_cast<AST::PropertyAccessExpression&>(dotExpression));
    m_out.print(".", dotExpression.fieldName());
}

void ASTDumper::visit(AST::GlobalVariableReference& globalVariableReference)
{
    visit(globalVariableReference.base());
    m_out.print("=>", globalVariableReference.structField().name());
}

void ASTDumper::visit(AST::IndexExpression& indexExpression)
{
    visit(static_cast<AST::PropertyAccessExpression&>(indexExpression));
    m_out.print("[");
    visit(indexExpression.indexExpression());
    m_out.print("]");
}

void ASTDumper::visit(AST::PropertyAccessExpression& expression)
{
    Base::visit(expression);
}

void ASTDumper::visit(AST::EffectfulExpressionStatement& effectfulExpressionStatement)
{
    Base::visit(effectfulExpressionStatement);
}

void ASTDumper::visit(AST::Fallthrough&)
{
    m_out.print("fallthrough");
}

void ASTDumper::visit(AST::IfStatement& ifStatement)
{
    m_out.print("if (");
    visit(ifStatement.conditional());
    m_out.print(") ");
    visit(ifStatement.body());
    if (ifStatement.elseBody()) {
        m_out.print(" else ");
        visit(*ifStatement.elseBody());
    }
}

void ASTDumper::visit(AST::Return& returnStatement)
{
    m_out.print("return");
    if (returnStatement.value()) {
        m_out.print(" ");
        visit(*returnStatement.value());
    }
}

void ASTDumper::visit(AST::SwitchStatement& switchStatement)
{
    m_out.print("switch (");
    visit(switchStatement.value());
    m_out.println(") {");
    bool once = false;
    for (auto& switchCase : switchStatement.switchCases()) {
        if (once)
            m_out.println();
        once = true;
        m_out.print(m_indent);
        visit(switchCase);
    }
    m_out.print("\n", m_indent, "}");

}

void ASTDumper::visit(AST::SwitchCase& switchCase)
{
    if (switchCase.value()) {
        m_out.print("case ");
        visit(*switchCase.value());
        m_out.print(": ");
    } else
        m_out.print("default: ");
    visit(switchCase.block());
}

void ASTDumper::visit(AST::VariableDeclarationsStatement& variableDeclarationsStatement)
{
    Base::visit(variableDeclarationsStatement);
}

void ASTDumper::visit(AST::VariableDeclaration& variableDeclaration)
{
    if (variableDeclaration.type()) {
        visit(*variableDeclaration.type());
        m_out.print(" ");
    }

    if (variableDeclaration.name().isEmpty())
        m_out.print("$", RawPointer(&variableDeclaration));
    else
        m_out.print(variableDeclaration.name());

    if (variableDeclaration.semantic())
        visit(*variableDeclaration.semantic());
    if (variableDeclaration.initializer()) {
        m_out.print(" = ");
        visit(*variableDeclaration.initializer());
    }
}

void ASTDumper::visit(AST::AssignmentExpression& assignmentExpression)
{
    visit(assignmentExpression.left());
    m_out.print(" = ");
    visit(assignmentExpression.right());
}

void ASTDumper::visit(AST::CallExpression& callExpression)
{
    m_out.print(callExpression.name(), "(");
    bool once = false;
    for (auto& argument : callExpression.arguments()) {
        if (once)
            m_out.print(", ");
        once = true;
        visit(argument);
    }
    m_out.print(")");
}

void ASTDumper::visit(AST::CommaExpression& commaExpression)
{
    m_out.print("(");
    bool once = false;
    for (auto& expression : commaExpression.list()) {
        if (once)
            m_out.print(", ");
        once = true;
        visit(expression);
    }
    m_out.print(")");
}

void ASTDumper::visit(AST::DereferenceExpression& dereferenceExpression)
{
    m_out.print("*");
    visit(dereferenceExpression.pointer());
}

void ASTDumper::visit(AST::LogicalExpression& logicalExpression)
{
    m_out.print("(");
    visit(logicalExpression.left());
    switch (logicalExpression.type()) {
    case AST::LogicalExpression::Type::And:
        m_out.print(" && ");
        break;
    case AST::LogicalExpression::Type::Or:
        m_out.print(" || ");
        break;
    }
    visit(logicalExpression.right());
    m_out.print(")");
}

void ASTDumper::visit(AST::LogicalNotExpression& logicalNotExpression)
{
    m_out.print("!(");
    visit(logicalNotExpression.operand());
    m_out.print(")");
}

void ASTDumper::visit(AST::MakeArrayReferenceExpression& makeArrayReferenceExpression)
{
    m_out.print("@");
    visit(makeArrayReferenceExpression.leftValue());
}

void ASTDumper::visit(AST::MakePointerExpression& makePointerExpression)
{
    m_out.print("&");
    visit(makePointerExpression.leftValue());
}

void ASTDumper::visit(AST::ReadModifyWriteExpression& readModifyWriteExpression)
{
    auto oldVariable = readModifyWriteExpression.oldVariableReference();
    auto newVariable = readModifyWriteExpression.newVariableReference();

    m_out.print("RMW(");
    visit(oldVariable.get());
    m_out.print(" = ");
    visit(readModifyWriteExpression.leftValue());
    m_out.print(", ");

    visit(newVariable.get());
    m_out.print(" = ");
    visit(readModifyWriteExpression.newValueExpression());
    m_out.print(", ");

    visit(readModifyWriteExpression.leftValue());
    m_out.print(" = ");
    visit(newVariable.get());
    m_out.print(", ");

    visit(readModifyWriteExpression.resultExpression());
    m_out.print(")");
}

void ASTDumper::visit(AST::TernaryExpression& ternaryExpression)
{
    visit(ternaryExpression.predicate());
    m_out.print(" ? ");
    visit(ternaryExpression.bodyExpression());
    m_out.print(" : ");
    visit(ternaryExpression.elseExpression());
}

void ASTDumper::visit(AST::VariableReference& variableReference)
{
    if (variableReference.name().isEmpty())
        m_out.print("$", RawPointer(variableReference.variable()));
    else
        m_out.print(variableReference.name());
}

} // namespace WHLSL

} // namespace WebCore

#endif
