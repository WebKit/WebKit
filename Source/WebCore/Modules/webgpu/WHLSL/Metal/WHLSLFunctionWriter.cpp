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
#include "WHLSLFunctionWriter.h"

#if ENABLE(WEBGPU)

#include "WHLSLArrayReferenceType.h"
#include "WHLSLArrayType.h"
#include "WHLSLAssignmentExpression.h"
#include "WHLSLBooleanLiteral.h"
#include "WHLSLBuiltInSemantic.h"
#include "WHLSLCallExpression.h"
#include "WHLSLCommaExpression.h"
#include "WHLSLDereferenceExpression.h"
#include "WHLSLDoWhileLoop.h"
#include "WHLSLEffectfulExpressionStatement.h"
#include "WHLSLEntryPointScaffolding.h"
#include "WHLSLEntryPointType.h"
#include "WHLSLFloatLiteral.h"
#include "WHLSLForLoop.h"
#include "WHLSLFunctionDeclaration.h"
#include "WHLSLFunctionDefinition.h"
#include "WHLSLIfStatement.h"
#include "WHLSLIntegerLiteral.h"
#include "WHLSLLogicalExpression.h"
#include "WHLSLLogicalNotExpression.h"
#include "WHLSLMakeArrayReferenceExpression.h"
#include "WHLSLMakePointerExpression.h"
#include "WHLSLMappedBindings.h"
#include "WHLSLNativeFunctionDeclaration.h"
#include "WHLSLNativeFunctionWriter.h"
#include "WHLSLNativeTypeDeclaration.h"
#include "WHLSLPointerType.h"
#include "WHLSLProgram.h"
#include "WHLSLReturn.h"
#include "WHLSLSwitchCase.h"
#include "WHLSLSwitchStatement.h"
#include "WHLSLTernaryExpression.h"
#include "WHLSLTypeNamer.h"
#include "WHLSLUnsignedIntegerLiteral.h"
#include "WHLSLVariableDeclaration.h"
#include "WHLSLVariableDeclarationsStatement.h"
#include "WHLSLVariableReference.h"
#include "WHLSLVisitor.h"
#include "WHLSLWhileLoop.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

namespace WHLSL {

namespace Metal {

class FunctionDeclarationWriter : public Visitor {
public:
    FunctionDeclarationWriter(TypeNamer& typeNamer, HashMap<AST::FunctionDeclaration*, String>& functionMapping)
        : m_typeNamer(typeNamer)
        , m_functionMapping(functionMapping)
    {
    }

    virtual ~FunctionDeclarationWriter() = default;

    String toString() { return m_stringBuilder.toString(); }

    void visit(AST::FunctionDeclaration&) override;

private:
    TypeNamer& m_typeNamer;
    HashMap<AST::FunctionDeclaration*, String>& m_functionMapping;
    StringBuilder m_stringBuilder;
};

void FunctionDeclarationWriter::visit(AST::FunctionDeclaration& functionDeclaration)
{
    if (functionDeclaration.entryPointType())
        return;

    auto iterator = m_functionMapping.find(&functionDeclaration);
    ASSERT(iterator != m_functionMapping.end());
    m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(functionDeclaration.type()), ' ', iterator->value, '('));
    for (size_t i = 0; i < functionDeclaration.parameters().size(); ++i) {
        if (i)
            m_stringBuilder.append(", ");
        m_stringBuilder.append(m_typeNamer.mangledNameForType(*functionDeclaration.parameters()[i].type()));
    }
    m_stringBuilder.append(");\n");
}

class FunctionDefinitionWriter : public Visitor {
public:
    FunctionDefinitionWriter(Intrinsics& intrinsics, TypeNamer& typeNamer, HashMap<AST::FunctionDeclaration*, String>& functionMapping, Layout& layout)
        : m_intrinsics(intrinsics)
        , m_typeNamer(typeNamer)
        , m_functionMapping(functionMapping)
        , m_layout(layout)
    {
    }

    virtual ~FunctionDefinitionWriter() = default;

    String toString() { return m_stringBuilder.toString(); }

    void visit(AST::NativeFunctionDeclaration&) override;
    void visit(AST::FunctionDefinition&) override;

protected:
    virtual std::unique_ptr<EntryPointScaffolding> createEntryPointScaffolding(AST::FunctionDefinition&) = 0;

    void visit(AST::FunctionDeclaration&) override;
    void visit(AST::Statement&) override;
    void visit(AST::Block&) override;
    void visit(AST::Break&) override;
    void visit(AST::Continue&) override;
    void visit(AST::DoWhileLoop&) override;
    void visit(AST::EffectfulExpressionStatement&) override;
    void visit(AST::Fallthrough&) override;
    void visit(AST::ForLoop&) override;
    void visit(AST::IfStatement&) override;
    void visit(AST::Return&) override;
    void visit(AST::SwitchStatement&) override;
    void visit(AST::SwitchCase&) override;
    void visit(AST::Trap&) override;
    void visit(AST::VariableDeclarationsStatement&) override;
    void visit(AST::WhileLoop&) override;
    void visit(AST::IntegerLiteral&) override;
    void visit(AST::UnsignedIntegerLiteral&) override;
    void visit(AST::FloatLiteral&) override;
    void visit(AST::NullLiteral&) override;
    void visit(AST::BooleanLiteral&) override;
    void visit(AST::EnumerationMemberLiteral&) override;
    void visit(AST::Expression&) override;
    void visit(AST::DotExpression&) override;
    void visit(AST::IndexExpression&) override;
    void visit(AST::PropertyAccessExpression&) override;
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

    String constantExpressionString(AST::ConstantExpression&);

    String generateNextVariableName()
    {
        return makeString("variable", m_variableCount++);
    }

    Intrinsics& m_intrinsics;
    TypeNamer& m_typeNamer;
    HashMap<AST::FunctionDeclaration*, String>& m_functionMapping;
    HashMap<AST::VariableDeclaration*, String> m_variableMapping;
    StringBuilder m_stringBuilder;
    Vector<String> m_stack;
    std::unique_ptr<EntryPointScaffolding> m_entryPointScaffolding;
    Layout& m_layout;
    unsigned m_variableCount { 0 };
};

void FunctionDefinitionWriter::visit(AST::NativeFunctionDeclaration& nativeFunctionDeclaration)
{
    auto iterator = m_functionMapping.find(&nativeFunctionDeclaration);
    ASSERT(iterator != m_functionMapping.end());
    m_stringBuilder.append(writeNativeFunction(nativeFunctionDeclaration, iterator->value, m_typeNamer));
}

void FunctionDefinitionWriter::visit(AST::FunctionDefinition& functionDefinition)
{
    auto iterator = m_functionMapping.find(&functionDefinition);
    ASSERT(iterator != m_functionMapping.end());
    if (functionDefinition.entryPointType()) {
        auto entryPointScaffolding = createEntryPointScaffolding(functionDefinition);
        if (!entryPointScaffolding)
            return;
        m_entryPointScaffolding = WTFMove(entryPointScaffolding);
        m_stringBuilder.append(m_entryPointScaffolding->helperTypes());
        m_stringBuilder.append('\n');
        m_stringBuilder.append(makeString(m_entryPointScaffolding->signature(iterator->value), " {"));
        m_stringBuilder.append(m_entryPointScaffolding->unpack());
        for (size_t i = 0; i < functionDefinition.parameters().size(); ++i) {
            auto addResult = m_variableMapping.add(&functionDefinition.parameters()[i], m_entryPointScaffolding->parameterVariables()[i]);
            ASSERT_UNUSED(addResult, addResult.isNewEntry);
        }
        checkErrorAndVisit(functionDefinition.block());
        ASSERT(m_stack.isEmpty());
        m_stringBuilder.append("}\n");
        m_entryPointScaffolding = nullptr;
    } else {
        ASSERT(m_entryPointScaffolding == nullptr);
        m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(functionDefinition.type()), ' ', iterator->value, '('));
        for (size_t i = 0; i < functionDefinition.parameters().size(); ++i) {
            auto& parameter = functionDefinition.parameters()[i];
            if (i)
                m_stringBuilder.append(", ");
            auto parameterName = generateNextVariableName();
            auto addResult = m_variableMapping.add(&parameter, parameterName);
            ASSERT_UNUSED(addResult, addResult.isNewEntry);
            m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(*parameter.type()), ' ', parameterName));
        }
        m_stringBuilder.append(") {\n");
        checkErrorAndVisit(functionDefinition.block());
        ASSERT(m_stack.isEmpty());
        m_stringBuilder.append("}\n");
    }
}

void FunctionDefinitionWriter::visit(AST::FunctionDeclaration&)
{
    ASSERT_NOT_REACHED();
}

void FunctionDefinitionWriter::visit(AST::Statement& statement)
{
    Visitor::visit(statement);
}

void FunctionDefinitionWriter::visit(AST::Block& block)
{
    m_stringBuilder.append("{\n");
    for (auto& statement : block.statements())
        checkErrorAndVisit(statement);
    m_stringBuilder.append("}\n");
}

void FunctionDefinitionWriter::visit(AST::Break&)
{
    m_stringBuilder.append("break;\n");
}

void FunctionDefinitionWriter::visit(AST::Continue&)
{
    // FIXME: Figure out which loop we're in, and run the increment code
    CRASH();
}

void FunctionDefinitionWriter::visit(AST::DoWhileLoop& doWhileLoop)
{
    m_stringBuilder.append("do {\n");
    checkErrorAndVisit(doWhileLoop.body());
    checkErrorAndVisit(doWhileLoop.conditional());
    m_stringBuilder.append(makeString("if (!", m_stack.takeLast(), ") break;\n"));
    m_stringBuilder.append(makeString("} while(true);\n"));
}

void FunctionDefinitionWriter::visit(AST::EffectfulExpressionStatement& effectfulExpressionStatement)
{
    checkErrorAndVisit(effectfulExpressionStatement.effectfulExpression());
    m_stack.takeLast(); // The statement is already effectful, so we don't need to do anything with the result.
}

void FunctionDefinitionWriter::visit(AST::Fallthrough&)
{
    m_stringBuilder.append("[[clang::fallthrough]];\n"); // FIXME: Make sure this is okay. Alternatively, we could do nothing and just return here instead.
}

void FunctionDefinitionWriter::visit(AST::ForLoop& forLoop)
{
    WTF::visit(WTF::makeVisitor([&](AST::VariableDeclarationsStatement& variableDeclarationsStatement) {
        checkErrorAndVisit(variableDeclarationsStatement);
    }, [&](UniqueRef<AST::Expression>& expression) {
        checkErrorAndVisit(expression);
        m_stack.takeLast(); // We don't need to do anything with the result.
    }), forLoop.initialization());

    m_stringBuilder.append("for ( ; ; ) {\n");
    if (forLoop.condition()) {
        checkErrorAndVisit(*forLoop.condition());
        m_stringBuilder.append(makeString("if (!", m_stack.takeLast(), ") break;\n"));
    }
    checkErrorAndVisit(forLoop.body());
    if (forLoop.increment()) {
        checkErrorAndVisit(*forLoop.increment());
        m_stack.takeLast();
    }
    m_stringBuilder.append("}\n");
}

void FunctionDefinitionWriter::visit(AST::IfStatement& ifStatement)
{
    checkErrorAndVisit(ifStatement.conditional());
    m_stringBuilder.append(makeString("if (", m_stack.takeLast(), ") {\n"));
    checkErrorAndVisit(ifStatement.body());
    if (ifStatement.elseBody()) {
        m_stringBuilder.append("} else {\n");
        checkErrorAndVisit(*ifStatement.elseBody());
    }
    m_stringBuilder.append("}\n");
}

void FunctionDefinitionWriter::visit(AST::Return& returnStatement)
{
    if (returnStatement.value()) {
        checkErrorAndVisit(*returnStatement.value());
        if (m_entryPointScaffolding) {
            auto variableName = generateNextVariableName();
            m_stringBuilder.append(m_entryPointScaffolding->pack(m_stack.takeLast(), variableName));
            m_stringBuilder.append(makeString("return ", variableName, ";\n"));
        } else
            m_stringBuilder.append(makeString("return ", m_stack.takeLast(), ";\n"));
    } else
        m_stringBuilder.append("return;\n");
}

void FunctionDefinitionWriter::visit(AST::SwitchStatement& switchStatement)
{
    checkErrorAndVisit(switchStatement.value());

    m_stringBuilder.append(makeString("switch (", m_stack.takeLast(), ") {"));
    for (auto& switchCase : switchStatement.switchCases())
        checkErrorAndVisit(switchCase);
    m_stringBuilder.append("}\n");
}

void FunctionDefinitionWriter::visit(AST::SwitchCase& switchCase)
{
    if (switchCase.value())
        m_stringBuilder.append(makeString("case ", constantExpressionString(*switchCase.value()), ":\n"));
    else
        m_stringBuilder.append("default:\n");
    checkErrorAndVisit(switchCase.block());
    // FIXME: Figure out whether we need to break or fallthrough.
    CRASH();
}

void FunctionDefinitionWriter::visit(AST::Trap&)
{
    // FIXME: Implement this
    CRASH();
}

void FunctionDefinitionWriter::visit(AST::VariableDeclarationsStatement& variableDeclarationsStatement)
{
    Visitor::visit(variableDeclarationsStatement);
}

void FunctionDefinitionWriter::visit(AST::WhileLoop& whileLoop)
{
    m_stringBuilder.append(makeString("while (true) {\n"));
    checkErrorAndVisit(whileLoop.conditional());
    m_stringBuilder.append(makeString("if (!", m_stack.takeLast(), ") break;\n"));
    checkErrorAndVisit(whileLoop.body());
    m_stringBuilder.append("}\n");
}

void FunctionDefinitionWriter::visit(AST::IntegerLiteral& integerLiteral)
{
    ASSERT(integerLiteral.resolvedType());
    auto variableName = generateNextVariableName();
    auto mangledTypeName = m_typeNamer.mangledNameForType(*integerLiteral.resolvedType());
    m_stringBuilder.append(makeString(mangledTypeName, ' ', variableName, " = static_cast<", mangledTypeName, ">(", integerLiteral.value(), ");\n"));
    m_stack.append(variableName);
}

void FunctionDefinitionWriter::visit(AST::UnsignedIntegerLiteral& unsignedIntegerLiteral)
{
    ASSERT(unsignedIntegerLiteral.resolvedType());
    auto variableName = generateNextVariableName();
    auto mangledTypeName = m_typeNamer.mangledNameForType(*unsignedIntegerLiteral.resolvedType());
    m_stringBuilder.append(makeString(mangledTypeName, ' ', variableName, " = static_cast<", mangledTypeName, ">(", unsignedIntegerLiteral.value(), ");\n"));
    m_stack.append(variableName);
}

void FunctionDefinitionWriter::visit(AST::FloatLiteral& floatLiteral)
{
    ASSERT(floatLiteral.resolvedType());
    auto variableName = generateNextVariableName();
    auto mangledTypeName = m_typeNamer.mangledNameForType(*floatLiteral.resolvedType());
    m_stringBuilder.append(makeString(mangledTypeName, ' ', variableName, " = static_cast<", mangledTypeName, ">(", floatLiteral.value(), ");\n"));
    m_stack.append(variableName);
}

void FunctionDefinitionWriter::visit(AST::NullLiteral& nullLiteral)
{
    ASSERT(nullLiteral.resolvedType());
    auto& unifyNode = nullLiteral.resolvedType()->unifyNode();
    ASSERT(is<AST::UnnamedType>(unifyNode));
    auto& unnamedType = downcast<AST::UnnamedType>(unifyNode);
    bool isArrayReferenceType = is<AST::ArrayReferenceType>(unnamedType);

    auto variableName = generateNextVariableName();
    m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(*nullLiteral.resolvedType()), ' ', variableName, " = "));
    if (isArrayReferenceType)
        m_stringBuilder.append("{ nullptr, 0 }");
    else
        m_stringBuilder.append("nullptr");
    m_stringBuilder.append(";\n");
    m_stack.append(variableName);
}

void FunctionDefinitionWriter::visit(AST::BooleanLiteral& booleanLiteral)
{
    ASSERT(booleanLiteral.resolvedType());
    auto variableName = generateNextVariableName();
    auto mangledTypeName = m_typeNamer.mangledNameForType(*booleanLiteral.resolvedType());
    m_stringBuilder.append(makeString(mangledTypeName, ' ', variableName, " = static_cast<", mangledTypeName, ">(", booleanLiteral.value() ? "true" : "false", ");\n"));
    m_stack.append(variableName);
}

void FunctionDefinitionWriter::visit(AST::EnumerationMemberLiteral& enumerationMemberLiteral)
{
    ASSERT(enumerationMemberLiteral.resolvedType());
    ASSERT(enumerationMemberLiteral.enumerationDefinition());
    ASSERT(enumerationMemberLiteral.enumerationDefinition());
    auto variableName = generateNextVariableName();
    auto mangledTypeName = m_typeNamer.mangledNameForType(*enumerationMemberLiteral.resolvedType());
    m_stringBuilder.append(makeString(mangledTypeName, ' ', variableName, " = ", mangledTypeName, '.', m_typeNamer.mangledNameForEnumerationMember(*enumerationMemberLiteral.enumerationMember()), ";\n"));
    m_stack.append(variableName);
}

void FunctionDefinitionWriter::visit(AST::Expression& expression)
{
    Visitor::visit(expression);
}

void FunctionDefinitionWriter::visit(AST::DotExpression&)
{
    // This should be lowered already.
    ASSERT_NOT_REACHED();
}

void FunctionDefinitionWriter::visit(AST::IndexExpression&)
{
    // This should be lowered already.
    ASSERT_NOT_REACHED();
}

void FunctionDefinitionWriter::visit(AST::PropertyAccessExpression&)
{
    ASSERT_NOT_REACHED();
}

void FunctionDefinitionWriter::visit(AST::VariableDeclaration& variableDeclaration)
{
    ASSERT(variableDeclaration.type());
    if (variableDeclaration.initializer())
        checkErrorAndVisit(*variableDeclaration.initializer());
    else {
        // FIXME: Zero-fill the variable.
        CRASH();
    }
    // FIXME: Implement qualifiers.
    auto variableName = generateNextVariableName();
    auto addResult = m_variableMapping.add(&variableDeclaration, variableName);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(*variableDeclaration.type()), ' ', variableName, " = ", m_stack.takeLast(), ";\n"));
    m_stack.append(variableName);
}

void FunctionDefinitionWriter::visit(AST::AssignmentExpression& assignmentExpression)
{
    checkErrorAndVisit(assignmentExpression.left());
    auto leftName = m_stack.takeLast();
    checkErrorAndVisit(assignmentExpression.right());
    auto rightName = m_stack.takeLast();
    m_stringBuilder.append(makeString(leftName, " = ", rightName, ";\n"));
}

void FunctionDefinitionWriter::visit(AST::CallExpression& callExpression)
{
    Vector<String> argumentNames;
    for (auto& argument : callExpression.arguments()) {
        checkErrorAndVisit(argument);
        argumentNames.append(m_stack.takeLast());
    }
    ASSERT(callExpression.resolvedType());
    ASSERT(callExpression.function());
    auto iterator = m_functionMapping.find(callExpression.function());
    ASSERT(iterator != m_functionMapping.end());
    auto variableName = generateNextVariableName();
    m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(*callExpression.resolvedType()), ' ', variableName, " = ", iterator->value, '('));
    for (size_t i = 0; i < argumentNames.size(); ++i) {
        if (i)
            m_stringBuilder.append(", ");
        m_stringBuilder.append(argumentNames[i]);
    }
    m_stringBuilder.append(");\n");
    m_stack.append(variableName);
}

void FunctionDefinitionWriter::visit(AST::CommaExpression& commaExpression)
{
    String result;
    for (auto& expression : commaExpression.list()) {
        checkErrorAndVisit(expression);
        result = m_stack.takeLast();
    }
    m_stack.append(result);
}

void FunctionDefinitionWriter::visit(AST::DereferenceExpression& dereferenceExpression)
{
    checkErrorAndVisit(dereferenceExpression.pointer());
    auto right = m_stack.takeLast();
    ASSERT(dereferenceExpression.resolvedType());
    auto variableName = generateNextVariableName();
    m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(*dereferenceExpression.resolvedType()), ' ', variableName, " = *", right, ";\n"));
    m_stack.append(variableName);
}

void FunctionDefinitionWriter::visit(AST::LogicalExpression& logicalExpression)
{
    checkErrorAndVisit(logicalExpression.left());
    auto left = m_stack.takeLast();
    checkErrorAndVisit(logicalExpression.right());
    auto right = m_stack.takeLast();
    ASSERT(logicalExpression.resolvedType());
    auto variableName = generateNextVariableName();
    m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(*logicalExpression.resolvedType()), ' ', variableName, " = ", left));
    switch (logicalExpression.type()) {
    case AST::LogicalExpression::Type::And:
        m_stringBuilder.append(" && ");
        break;
    default:
        ASSERT(logicalExpression.type() == AST::LogicalExpression::Type::Or);
        m_stringBuilder.append(" || ");
        break;
    }
    m_stringBuilder.append(makeString(right, ";\n"));
    m_stack.append(variableName);
}

void FunctionDefinitionWriter::visit(AST::LogicalNotExpression& logicalNotExpression)
{
    checkErrorAndVisit(logicalNotExpression.operand());
    auto operand = m_stack.takeLast();
    ASSERT(logicalNotExpression.resolvedType());
    auto variableName = generateNextVariableName();
    m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(*logicalNotExpression.resolvedType()), ' ', variableName, " = !", operand, ";\n"));
    m_stack.append(variableName);
}

void FunctionDefinitionWriter::visit(AST::MakeArrayReferenceExpression& makeArrayReferenceExpression)
{
    checkErrorAndVisit(makeArrayReferenceExpression.lValue());
    auto lValue = m_stack.takeLast();
    ASSERT(makeArrayReferenceExpression.resolvedType());
    auto variableName = generateNextVariableName();
    auto mangledTypeName = m_typeNamer.mangledNameForType(*makeArrayReferenceExpression.resolvedType());
    if (is<AST::PointerType>(*makeArrayReferenceExpression.resolvedType()))
        m_stringBuilder.append(makeString(mangledTypeName, ' ', variableName, " = { ", lValue, ", 1 };\n"));
    else if (is<AST::ArrayType>(*makeArrayReferenceExpression.resolvedType())) {
        auto& arrayType = downcast<AST::ArrayType>(*makeArrayReferenceExpression.resolvedType());
        m_stringBuilder.append(makeString(mangledTypeName, ' ', variableName, " = { &(", lValue, "[0]), ", arrayType.numElements(), " };\n"));
    } else
        m_stringBuilder.append(makeString(mangledTypeName, ' ', variableName, " = { &", lValue, ", 1 };\n"));
    m_stack.append(variableName);
}

void FunctionDefinitionWriter::visit(AST::MakePointerExpression& makePointerExpression)
{
    checkErrorAndVisit(makePointerExpression.lValue());
    auto lValue = m_stack.takeLast();
    ASSERT(makePointerExpression.resolvedType());
    auto variableName = generateNextVariableName();
    m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(*makePointerExpression.resolvedType()), ' ', variableName, " = &", lValue, ";\n"));
    m_stack.append(variableName);
}

void FunctionDefinitionWriter::visit(AST::ReadModifyWriteExpression&)
{
    // This should be lowered already.
    ASSERT_NOT_REACHED();
}

void FunctionDefinitionWriter::visit(AST::TernaryExpression& ternaryExpression)
{
    checkErrorAndVisit(ternaryExpression.predicate());
    auto check = m_stack.takeLast();

    ASSERT(ternaryExpression.resolvedType());
    auto variableName = generateNextVariableName();
    m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(*ternaryExpression.resolvedType()), ' ', variableName, ";\n"));

    m_stringBuilder.append(makeString("if (", check, ") {\n"));
    checkErrorAndVisit(ternaryExpression.bodyExpression());
    m_stringBuilder.append(makeString(variableName, " = ", m_stack.takeLast(), ";\n"));
    m_stringBuilder.append("} else {\n");
    checkErrorAndVisit(ternaryExpression.elseExpression());
    m_stringBuilder.append(makeString(variableName, " = ", m_stack.takeLast(), ";\n"));
    m_stringBuilder.append("}\n");
    m_stack.append(variableName);
}

void FunctionDefinitionWriter::visit(AST::VariableReference& variableReference)
{
    ASSERT(variableReference.variable());
    auto iterator = m_variableMapping.find(variableReference.variable());
    ASSERT(iterator != m_variableMapping.end());
    m_stack.append(iterator->value);
}

String FunctionDefinitionWriter::constantExpressionString(AST::ConstantExpression& constantExpression)
{
    String result;
    constantExpression.visit(WTF::makeVisitor([&](AST::IntegerLiteral& integerLiteral) {
        result = makeString("", integerLiteral.value());
    }, [&](AST::UnsignedIntegerLiteral& unsignedIntegerLiteral) {
        result = makeString("", unsignedIntegerLiteral.value());
    }, [&](AST::FloatLiteral& floatLiteral) {
        result = makeString("", floatLiteral.value());
    }, [&](AST::NullLiteral&) {
        result = "nullptr"_str;
    }, [&](AST::BooleanLiteral& booleanLiteral) {
        result = booleanLiteral.value() ? "true"_str : "false"_str;
    }, [&](AST::EnumerationMemberLiteral& enumerationMemberLiteral) {
        ASSERT(enumerationMemberLiteral.enumerationDefinition());
        ASSERT(enumerationMemberLiteral.enumerationDefinition());
        result = makeString(m_typeNamer.mangledNameForType(*enumerationMemberLiteral.enumerationDefinition()), '.', m_typeNamer.mangledNameForEnumerationMember(*enumerationMemberLiteral.enumerationMember()));
    }));
    return result;
}

class RenderFunctionDefinitionWriter : public FunctionDefinitionWriter {
public:
    RenderFunctionDefinitionWriter(Intrinsics& intrinsics, TypeNamer& typeNamer, HashMap<AST::FunctionDeclaration*, String>& functionMapping, MatchedRenderSemantics&& matchedSemantics, Layout& layout)
        : FunctionDefinitionWriter(intrinsics, typeNamer, functionMapping, layout)
        , m_matchedSemantics(WTFMove(matchedSemantics))
    {
    }

    MappedBindGroups&& takeVertexMappedBindGroups()
    {
        ASSERT(m_vertexMappedBindGroups);
        return WTFMove(*m_vertexMappedBindGroups);
    }

    MappedBindGroups&& takeFragmentMappedBindGroups()
    {
        ASSERT(m_fragmentMappedBindGroups);
        return WTFMove(*m_fragmentMappedBindGroups);
    }

private:
    std::unique_ptr<EntryPointScaffolding> createEntryPointScaffolding(AST::FunctionDefinition&) override;

    MatchedRenderSemantics m_matchedSemantics;
    Optional<MappedBindGroups> m_vertexMappedBindGroups;
    Optional<MappedBindGroups> m_fragmentMappedBindGroups;
};

std::unique_ptr<EntryPointScaffolding> RenderFunctionDefinitionWriter::createEntryPointScaffolding(AST::FunctionDefinition& functionDefinition)
{
    auto generateNextVariableName = [this]() -> String {
        return this->generateNextVariableName();
    };
    if (&functionDefinition == m_matchedSemantics.vertexShader) {
        auto result = std::make_unique<VertexEntryPointScaffolding>(functionDefinition, m_intrinsics, m_typeNamer, m_matchedSemantics.vertexShaderEntryPointItems, m_matchedSemantics.vertexShaderResourceMap, m_layout, WTFMove(generateNextVariableName), m_matchedSemantics.matchedVertexAttributes);
        ASSERT(!m_vertexMappedBindGroups);
        m_vertexMappedBindGroups = result->mappedBindGroups();
        return result;
    }
    if (&functionDefinition == m_matchedSemantics.fragmentShader) {
        auto result = std::make_unique<FragmentEntryPointScaffolding>(functionDefinition, m_intrinsics, m_typeNamer, m_matchedSemantics.fragmentShaderEntryPointItems, m_matchedSemantics.fragmentShaderResourceMap, m_layout, WTFMove(generateNextVariableName), m_matchedSemantics.matchedColorAttachments);
        ASSERT(!m_fragmentMappedBindGroups);
        m_fragmentMappedBindGroups = result->mappedBindGroups();
        return result;
    }
    return nullptr;
}

class ComputeFunctionDefinitionWriter : public FunctionDefinitionWriter {
public:
    ComputeFunctionDefinitionWriter(Intrinsics& intrinsics, TypeNamer& typeNamer, HashMap<AST::FunctionDeclaration*, String>& functionMapping, MatchedComputeSemantics&& matchedSemantics, Layout& layout)
        : FunctionDefinitionWriter(intrinsics, typeNamer, functionMapping, layout)
        , m_matchedSemantics(WTFMove(matchedSemantics))
    {
    }

    MappedBindGroups&& takeMappedBindGroups()
    {
        ASSERT(m_mappedBindGroups);
        return WTFMove(*m_mappedBindGroups);
    }

private:
    std::unique_ptr<EntryPointScaffolding> createEntryPointScaffolding(AST::FunctionDefinition&) override;

    MatchedComputeSemantics m_matchedSemantics;
    Optional<MappedBindGroups> m_mappedBindGroups;
};

std::unique_ptr<EntryPointScaffolding> ComputeFunctionDefinitionWriter::createEntryPointScaffolding(AST::FunctionDefinition& functionDefinition)
{
    auto generateNextVariableName = [this]() -> String {
        return this->generateNextVariableName();
    };
    if (&functionDefinition == m_matchedSemantics.shader) {
        auto result = std::make_unique<ComputeEntryPointScaffolding>(functionDefinition, m_intrinsics, m_typeNamer, m_matchedSemantics.entryPointItems, m_matchedSemantics.resourceMap, m_layout, WTFMove(generateNextVariableName));
        ASSERT(!m_mappedBindGroups);
        m_mappedBindGroups = result->mappedBindGroups();
        return result;
    }
    return nullptr;
}

struct SharedMetalFunctionsResult {
    HashMap<AST::FunctionDeclaration*, String> functionMapping;
    String metalFunctions;
};
static SharedMetalFunctionsResult sharedMetalFunctions(Program& program, TypeNamer& typeNamer)
{
    StringBuilder stringBuilder;

    unsigned numFunctions = 0;
    HashMap<AST::FunctionDeclaration*, String> functionMapping;
    for (auto& nativeFunctionDeclaration : program.nativeFunctionDeclarations()) {
        auto addResult = functionMapping.add(&nativeFunctionDeclaration, makeString("function", numFunctions++));
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }
    for (auto& functionDefinition : program.functionDefinitions()) {
        auto addResult = functionMapping.add(&functionDefinition, makeString("function", numFunctions++));
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }

    {
        FunctionDeclarationWriter functionDeclarationWriter(typeNamer, functionMapping);
        for (auto& nativeFunctionDeclaration : program.nativeFunctionDeclarations())
            functionDeclarationWriter.visit(nativeFunctionDeclaration);
        for (auto& functionDefinition : program.functionDefinitions()) {
            if (!functionDefinition->entryPointType())
                functionDeclarationWriter.visit(functionDefinition);
        }
        stringBuilder.append(functionDeclarationWriter.toString());
    }

    stringBuilder.append('\n');
    return { WTFMove(functionMapping), stringBuilder.toString() };
}

RenderMetalFunctions metalFunctions(Program& program, TypeNamer& typeNamer, MatchedRenderSemantics&& matchedSemantics, Layout& layout)
{
    auto sharedMetalFunctions = Metal::sharedMetalFunctions(program, typeNamer);

    StringBuilder stringBuilder;
    stringBuilder.append(sharedMetalFunctions.metalFunctions);

    RenderFunctionDefinitionWriter functionDefinitionWriter(program.intrinsics(), typeNamer, sharedMetalFunctions.functionMapping, WTFMove(matchedSemantics), layout);
    for (auto& nativeFunctionDeclaration : program.nativeFunctionDeclarations())
        functionDefinitionWriter.visit(nativeFunctionDeclaration);
    for (auto& functionDefinition : program.functionDefinitions())
        functionDefinitionWriter.visit(functionDefinition);
    stringBuilder.append(functionDefinitionWriter.toString());

    RenderMetalFunctions result;
    result.metalSource = stringBuilder.toString();
    result.vertexMappedBindGroups = functionDefinitionWriter.takeVertexMappedBindGroups();
    result.fragmentMappedBindGroups = functionDefinitionWriter.takeFragmentMappedBindGroups();
    return result;
}

ComputeMetalFunctions metalFunctions(Program& program, TypeNamer& typeNamer, MatchedComputeSemantics&& matchedSemantics, Layout& layout)
{
    auto sharedMetalFunctions = Metal::sharedMetalFunctions(program, typeNamer);

    StringBuilder stringBuilder;
    stringBuilder.append(sharedMetalFunctions.metalFunctions);

    ComputeFunctionDefinitionWriter functionDefinitionWriter(program.intrinsics(), typeNamer, sharedMetalFunctions.functionMapping, WTFMove(matchedSemantics), layout);
    for (auto& nativeFunctionDeclaration : program.nativeFunctionDeclarations())
        functionDefinitionWriter.visit(nativeFunctionDeclaration);
    for (auto& functionDefinition : program.functionDefinitions())
        functionDefinitionWriter.visit(functionDefinition);
    stringBuilder.append(functionDefinitionWriter.toString());

    ComputeMetalFunctions result;
    result.metalSource = stringBuilder.toString();
    result.mappedBindGroups = functionDefinitionWriter.takeMappedBindGroups();
    return result;
}

} // namespace Metal

} // namespace WHLSL

} // namespace WebCore

#endif
