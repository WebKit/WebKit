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

#include "NotImplemented.h"
#include "WHLSLAST.h"
#include "WHLSLEntryPointScaffolding.h"
#include "WHLSLInferTypes.h"
#include "WHLSLNativeFunctionWriter.h"
#include "WHLSLProgram.h"
#include "WHLSLTypeNamer.h"
#include "WHLSLVisitor.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/SetForScope.h>
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
        m_stringBuilder.append(m_typeNamer.mangledNameForType(*functionDeclaration.parameters()[i]->type()));
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
        m_stringBuilder.append(makeString(
            "template <typename T>\n"
            "inline void ", memsetZeroFunctionName, "(thread T& value)\n"
            "{\n"
            "    thread char* ptr = static_cast<thread char*>(static_cast<thread void*>(&value));\n"
            "    for (size_t i = 0; i < sizeof(T); ++i)\n"
            "        ptr[i] = 0;\n"
            "}\n"));
    }

    static constexpr const char* memsetZeroFunctionName = "memsetZero";

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
    void visit(AST::GlobalVariableReference&) override;
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

    enum class LoopConditionLocation {
        BeforeBody,
        AfterBody
    };
    void emitLoop(LoopConditionLocation, AST::Expression* conditionExpression, AST::Expression* increment, AST::Statement& body);

    String constantExpressionString(AST::ConstantExpression&);

    String generateNextVariableName()
    {
        return makeString("variable", m_variableCount++);
    }

    struct StackItem {
        String value;
        String leftValue;
    };

    void appendRightValue(AST::Expression&, String value)
    {
        m_stack.append({ WTFMove(value), String() });
    }

    void appendLeftValue(AST::Expression& expression, String value, String leftValue)
    {
        ASSERT_UNUSED(expression, expression.typeAnnotation().leftAddressSpace());
        m_stack.append({ WTFMove(value), WTFMove(leftValue) });
    }

    String takeLastValue()
    {
        ASSERT(m_stack.last().value);
        return m_stack.takeLast().value;
    }

    String takeLastLeftValue()
    {
        ASSERT(m_stack.last().leftValue);
        return m_stack.takeLast().leftValue;
    }

    enum class BreakContext {
        Loop,
        Switch
    };

    Optional<BreakContext> m_currentBreakContext;

    Intrinsics& m_intrinsics;
    TypeNamer& m_typeNamer;
    HashMap<AST::FunctionDeclaration*, String>& m_functionMapping;
    HashMap<AST::VariableDeclaration*, String> m_variableMapping;
    StringBuilder m_stringBuilder;

    Vector<StackItem> m_stack;
    std::unique_ptr<EntryPointScaffolding> m_entryPointScaffolding;
    Layout& m_layout;
    unsigned m_variableCount { 0 };
    String m_breakOutOfCurrentLoopEarlyVariable;
};

void FunctionDefinitionWriter::visit(AST::NativeFunctionDeclaration& nativeFunctionDeclaration)
{
    auto iterator = m_functionMapping.find(&nativeFunctionDeclaration);
    ASSERT(iterator != m_functionMapping.end());
    m_stringBuilder.append(writeNativeFunction(nativeFunctionDeclaration, iterator->value, m_intrinsics, m_typeNamer, memsetZeroFunctionName));
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
        m_stringBuilder.append(makeString(m_entryPointScaffolding->signature(iterator->value), " {\n"));
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
            m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(*parameter->type()), ' ', parameterName));
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
    ASSERT(m_currentBreakContext);
    switch (*m_currentBreakContext) {
    case BreakContext::Switch:
        m_stringBuilder.append("break;\n");
        break;
    case BreakContext::Loop:
        ASSERT(m_breakOutOfCurrentLoopEarlyVariable.length());
        m_stringBuilder.append(makeString(m_breakOutOfCurrentLoopEarlyVariable, " = true;\n"));
        m_stringBuilder.append("break;\n");
        break;
    }
}

void FunctionDefinitionWriter::visit(AST::Continue&)
{
    ASSERT(m_breakOutOfCurrentLoopEarlyVariable.length());
    m_stringBuilder.append("break;\n");
}

void FunctionDefinitionWriter::visit(AST::EffectfulExpressionStatement& effectfulExpressionStatement)
{
    checkErrorAndVisit(effectfulExpressionStatement.effectfulExpression());
    takeLastValue(); // The statement is already effectful, so we don't need to do anything with the result.
}

void FunctionDefinitionWriter::visit(AST::Fallthrough&)
{
    m_stringBuilder.append("[[clang::fallthrough]];\n"); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195808 Make sure this is okay. Alternatively, we could do nothing and just return here instead.
}

void FunctionDefinitionWriter::emitLoop(LoopConditionLocation loopConditionLocation, AST::Expression* conditionExpression, AST::Expression* increment, AST::Statement& body)
{
    SetForScope<String> loopVariableScope(m_breakOutOfCurrentLoopEarlyVariable, generateNextVariableName());

    m_stringBuilder.append(makeString("bool ", m_breakOutOfCurrentLoopEarlyVariable, " = false;\n"));

    m_stringBuilder.append("while (true) {\n");

    if (loopConditionLocation == LoopConditionLocation::BeforeBody && conditionExpression) {
        checkErrorAndVisit(*conditionExpression);
        m_stringBuilder.append(makeString("if (!", takeLastValue(), ") break;\n"));
    }

    m_stringBuilder.append("do {\n");
    SetForScope<Optional<BreakContext>> breakContext(m_currentBreakContext, BreakContext::Loop);
    checkErrorAndVisit(body);
    m_stringBuilder.append("} while(false); \n");
    m_stringBuilder.append(makeString("if (", m_breakOutOfCurrentLoopEarlyVariable, ") break;\n"));

    if (increment) {
        checkErrorAndVisit(*increment);
        // Expression results get pushed to m_stack. We don't use the result
        // of increment, so we dispense of that now.
        takeLastValue();
    }

    if (loopConditionLocation == LoopConditionLocation::AfterBody && conditionExpression) {
        checkErrorAndVisit(*conditionExpression);
        m_stringBuilder.append(makeString("if (!", takeLastValue(), ") break;\n"));
    }

    m_stringBuilder.append("} \n");
}

void FunctionDefinitionWriter::visit(AST::DoWhileLoop& doWhileLoop)
{
    emitLoop(LoopConditionLocation::AfterBody, &doWhileLoop.conditional(), nullptr, doWhileLoop.body());
}

void FunctionDefinitionWriter::visit(AST::WhileLoop& whileLoop)
{
    emitLoop(LoopConditionLocation::BeforeBody, &whileLoop.conditional(), nullptr, whileLoop.body());
}

void FunctionDefinitionWriter::visit(AST::ForLoop& forLoop)
{
    m_stringBuilder.append("{\n");

    WTF::visit(WTF::makeVisitor([&](AST::Statement& statement) {
        checkErrorAndVisit(statement);
    }, [&](UniqueRef<AST::Expression>& expression) {
        checkErrorAndVisit(expression);
        takeLastValue(); // We don't need to do anything with the result.
    }), forLoop.initialization());

    emitLoop(LoopConditionLocation::BeforeBody, forLoop.condition(), forLoop.increment(), forLoop.body());
    m_stringBuilder.append("}\n");
}

void FunctionDefinitionWriter::visit(AST::IfStatement& ifStatement)
{
    checkErrorAndVisit(ifStatement.conditional());
    m_stringBuilder.append(makeString("if (", takeLastValue(), ") {\n"));
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
            m_stringBuilder.append(m_entryPointScaffolding->pack(takeLastValue(), variableName));
            m_stringBuilder.append(makeString("return ", variableName, ";\n"));
        } else
            m_stringBuilder.append(makeString("return ", takeLastValue(), ";\n"));
    } else
        m_stringBuilder.append("return;\n");
}

void FunctionDefinitionWriter::visit(AST::SwitchStatement& switchStatement)
{
    checkErrorAndVisit(switchStatement.value());

    m_stringBuilder.append(makeString("switch (", takeLastValue(), ") {"));
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
    SetForScope<Optional<BreakContext>> breakContext(m_currentBreakContext, BreakContext::Switch);
    checkErrorAndVisit(switchCase.block());
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195812 Figure out whether we need to break or fallthrough.
}

void FunctionDefinitionWriter::visit(AST::Trap&)
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195811 Implement this
    notImplemented();
}

void FunctionDefinitionWriter::visit(AST::VariableDeclarationsStatement& variableDeclarationsStatement)
{
    Visitor::visit(variableDeclarationsStatement);
}

void FunctionDefinitionWriter::visit(AST::IntegerLiteral& integerLiteral)
{
    auto variableName = generateNextVariableName();
    auto mangledTypeName = m_typeNamer.mangledNameForType(integerLiteral.resolvedType());
    m_stringBuilder.append(makeString(mangledTypeName, ' ', variableName, " = static_cast<", mangledTypeName, ">(", integerLiteral.value(), ");\n"));
    appendRightValue(integerLiteral, variableName);
}

void FunctionDefinitionWriter::visit(AST::UnsignedIntegerLiteral& unsignedIntegerLiteral)
{
    auto variableName = generateNextVariableName();
    auto mangledTypeName = m_typeNamer.mangledNameForType(unsignedIntegerLiteral.resolvedType());
    m_stringBuilder.append(makeString(mangledTypeName, ' ', variableName, " = static_cast<", mangledTypeName, ">(", unsignedIntegerLiteral.value(), ");\n"));
    appendRightValue(unsignedIntegerLiteral, variableName);
}

void FunctionDefinitionWriter::visit(AST::FloatLiteral& floatLiteral)
{
    auto variableName = generateNextVariableName();
    auto mangledTypeName = m_typeNamer.mangledNameForType(floatLiteral.resolvedType());
    m_stringBuilder.append(makeString(mangledTypeName, ' ', variableName, " = static_cast<", mangledTypeName, ">(", floatLiteral.value(), ");\n"));
    appendRightValue(floatLiteral, variableName);
}

void FunctionDefinitionWriter::visit(AST::NullLiteral& nullLiteral)
{
    auto& unifyNode = nullLiteral.resolvedType().unifyNode();
    auto& unnamedType = downcast<AST::UnnamedType>(unifyNode);
    bool isArrayReferenceType = is<AST::ArrayReferenceType>(unnamedType);

    auto variableName = generateNextVariableName();
    m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(nullLiteral.resolvedType()), ' ', variableName, " = "));
    if (isArrayReferenceType)
        m_stringBuilder.append("{ nullptr, 0 }");
    else
        m_stringBuilder.append("nullptr");
    m_stringBuilder.append(";\n");
    appendRightValue(nullLiteral, variableName);
}

void FunctionDefinitionWriter::visit(AST::BooleanLiteral& booleanLiteral)
{
    auto variableName = generateNextVariableName();
    auto mangledTypeName = m_typeNamer.mangledNameForType(booleanLiteral.resolvedType());
    m_stringBuilder.append(makeString(mangledTypeName, ' ', variableName, " = static_cast<", mangledTypeName, ">(", booleanLiteral.value() ? "true" : "false", ");\n"));
    appendRightValue(booleanLiteral, variableName);
}

void FunctionDefinitionWriter::visit(AST::EnumerationMemberLiteral& enumerationMemberLiteral)
{
    ASSERT(enumerationMemberLiteral.enumerationDefinition());
    ASSERT(enumerationMemberLiteral.enumerationDefinition());
    auto variableName = generateNextVariableName();
    auto mangledTypeName = m_typeNamer.mangledNameForType(enumerationMemberLiteral.resolvedType());
    m_stringBuilder.append(makeString(mangledTypeName, ' ', variableName, " = ", mangledTypeName, '.', m_typeNamer.mangledNameForEnumerationMember(*enumerationMemberLiteral.enumerationMember()), ";\n"));
    appendRightValue(enumerationMemberLiteral, variableName);
}

void FunctionDefinitionWriter::visit(AST::Expression& expression)
{
    Visitor::visit(expression);
}

void FunctionDefinitionWriter::visit(AST::DotExpression& dotExpression)
{
    // This should be lowered already.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195788 Replace this with ASSERT_NOT_REACHED().
    notImplemented();
    appendRightValue(dotExpression, "dummy");
}

void FunctionDefinitionWriter::visit(AST::GlobalVariableReference& globalVariableReference)
{
    auto valueName = generateNextVariableName();
    auto pointerName = generateNextVariableName();
    auto mangledTypeName = m_typeNamer.mangledNameForType(globalVariableReference.resolvedType());
    checkErrorAndVisit(globalVariableReference.base());
    m_stringBuilder.append(makeString("thread ", mangledTypeName, "* ", pointerName, " = &", takeLastValue(), "->", m_typeNamer.mangledNameForStructureElement(globalVariableReference.structField()), ";\n"));
    m_stringBuilder.append(makeString(mangledTypeName, ' ', valueName, " = ", "*", pointerName, ";\n"));
    appendLeftValue(globalVariableReference, valueName, pointerName);
}

void FunctionDefinitionWriter::visit(AST::IndexExpression& indexExpression)
{
    // This should be lowered already.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195788 Replace this with ASSERT_NOT_REACHED().
    notImplemented();
    appendRightValue(indexExpression, "dummy");
}

void FunctionDefinitionWriter::visit(AST::PropertyAccessExpression& propertyAccessExpression)
{
    // This should be lowered already.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195788 Replace this with ASSERT_NOT_REACHED().
    notImplemented();
    appendRightValue(propertyAccessExpression, "dummy");
}

void FunctionDefinitionWriter::visit(AST::VariableDeclaration& variableDeclaration)
{
    ASSERT(variableDeclaration.type());
    auto variableName = generateNextVariableName();
    auto addResult = m_variableMapping.add(&variableDeclaration, variableName);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198160 Implement qualifiers.
    if (variableDeclaration.initializer()) {
        checkErrorAndVisit(*variableDeclaration.initializer());
        m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(*variableDeclaration.type()), ' ', variableName, " = ", takeLastValue(), ";\n"));
    } else
        m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(*variableDeclaration.type()), ' ', variableName, ";\n"));
}

void FunctionDefinitionWriter::visit(AST::AssignmentExpression& assignmentExpression)
{
    checkErrorAndVisit(assignmentExpression.left());
    auto pointerName = takeLastLeftValue();
    checkErrorAndVisit(assignmentExpression.right());
    auto rightName = takeLastValue();
    m_stringBuilder.append(makeString("if (", pointerName, ") *", pointerName, " = ", rightName, ";\n"));
    appendRightValue(assignmentExpression, rightName);
}

void FunctionDefinitionWriter::visit(AST::CallExpression& callExpression)
{
    Vector<String> argumentNames;
    for (auto& argument : callExpression.arguments()) {
        checkErrorAndVisit(argument);
        argumentNames.append(takeLastValue());
    }
    auto iterator = m_functionMapping.find(&callExpression.function());
    ASSERT(iterator != m_functionMapping.end());
    auto variableName = generateNextVariableName();
    if (!matches(callExpression.resolvedType(), m_intrinsics.voidType()))
        m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(callExpression.resolvedType()), ' ', variableName, " = "));
    m_stringBuilder.append(makeString(iterator->value, '('));
    for (size_t i = 0; i < argumentNames.size(); ++i) {
        if (i)
            m_stringBuilder.append(", ");
        m_stringBuilder.append(argumentNames[i]);
    }
    m_stringBuilder.append(");\n");
    appendRightValue(callExpression, variableName);
}

void FunctionDefinitionWriter::visit(AST::CommaExpression& commaExpression)
{
    String result;
    for (auto& expression : commaExpression.list()) {
        checkErrorAndVisit(expression);
        result = takeLastValue();
    }
    appendRightValue(commaExpression, result);
}

void FunctionDefinitionWriter::visit(AST::DereferenceExpression& dereferenceExpression)
{
    checkErrorAndVisit(dereferenceExpression.pointer());
    auto right = takeLastValue();
    auto variableName = generateNextVariableName();
    auto pointerName = generateNextVariableName();
    m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(dereferenceExpression.pointer().resolvedType()), ' ', pointerName, " = ", right, ";\n"));
    m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(dereferenceExpression.resolvedType()), ' ', variableName, ";\n"));
    m_stringBuilder.append(makeString("if (", pointerName, ") ", variableName, " = *", right, ";\n"));
    m_stringBuilder.append(makeString("else ", memsetZeroFunctionName, '(', variableName, ");\n"));
    appendLeftValue(dereferenceExpression, variableName, pointerName);
}

void FunctionDefinitionWriter::visit(AST::LogicalExpression& logicalExpression)
{
    checkErrorAndVisit(logicalExpression.left());
    auto left = takeLastValue();
    checkErrorAndVisit(logicalExpression.right());
    auto right = takeLastValue();
    auto variableName = generateNextVariableName();
    m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(logicalExpression.resolvedType()), ' ', variableName, " = ", left));
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
    appendRightValue(logicalExpression, variableName);
}

void FunctionDefinitionWriter::visit(AST::LogicalNotExpression& logicalNotExpression)
{
    checkErrorAndVisit(logicalNotExpression.operand());
    auto operand = takeLastValue();
    auto variableName = generateNextVariableName();
    m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(logicalNotExpression.resolvedType()), ' ', variableName, " = !", operand, ";\n"));
    appendRightValue(logicalNotExpression, variableName);
}

void FunctionDefinitionWriter::visit(AST::MakeArrayReferenceExpression& makeArrayReferenceExpression)
{
    checkErrorAndVisit(makeArrayReferenceExpression.leftValue());
    // FIXME: This needs to be made to work. It probably should be using the last leftValue too.
    // https://bugs.webkit.org/show_bug.cgi?id=198838
    auto variableName = generateNextVariableName();

    auto mangledTypeName = m_typeNamer.mangledNameForType(makeArrayReferenceExpression.resolvedType());
    if (is<AST::PointerType>(makeArrayReferenceExpression.leftValue().resolvedType())) {
        auto ptrValue = takeLastValue();
        m_stringBuilder.append(makeString(mangledTypeName, ' ', variableName, ";\n"));
        m_stringBuilder.append(makeString("if (", ptrValue, ") ", variableName, " = { ", ptrValue, ", 1};\n"));
        m_stringBuilder.append(makeString("else ", variableName, " = { nullptr, 0 };\n"));
    } else if (is<AST::ArrayType>(makeArrayReferenceExpression.leftValue().resolvedType())) {
        auto lValue = takeLastLeftValue();
        auto& arrayType = downcast<AST::ArrayType>(makeArrayReferenceExpression.leftValue().resolvedType());
        m_stringBuilder.append(makeString(mangledTypeName, ' ', variableName, " = { ", lValue, "->data(), ", arrayType.numElements(), " };\n"));
    } else {
        auto lValue = takeLastLeftValue();
        m_stringBuilder.append(makeString(mangledTypeName, ' ', variableName, " = { ", lValue, ", 1 };\n"));
    }
    appendRightValue(makeArrayReferenceExpression, variableName);
}

void FunctionDefinitionWriter::visit(AST::MakePointerExpression& makePointerExpression)
{
    checkErrorAndVisit(makePointerExpression.leftValue());
    auto pointer = takeLastLeftValue();
    auto variableName = generateNextVariableName();
    m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(makePointerExpression.resolvedType()), ' ', variableName, " = ", pointer, ";\n"));
    appendRightValue(makePointerExpression, variableName);
}

void FunctionDefinitionWriter::visit(AST::ReadModifyWriteExpression&)
{
    // This should be lowered already.
    ASSERT_NOT_REACHED();
}

void FunctionDefinitionWriter::visit(AST::TernaryExpression& ternaryExpression)
{
    checkErrorAndVisit(ternaryExpression.predicate());
    auto check = takeLastValue();

    auto variableName = generateNextVariableName();
    m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(ternaryExpression.resolvedType()), ' ', variableName, ";\n"));

    m_stringBuilder.append(makeString("if (", check, ") {\n"));
    checkErrorAndVisit(ternaryExpression.bodyExpression());
    m_stringBuilder.append(makeString(variableName, " = ", takeLastValue(), ";\n"));
    m_stringBuilder.append("} else {\n");
    checkErrorAndVisit(ternaryExpression.elseExpression());
    m_stringBuilder.append(makeString(variableName, " = ", takeLastValue(), ";\n"));
    m_stringBuilder.append("}\n");
    appendRightValue(ternaryExpression, variableName);
}

void FunctionDefinitionWriter::visit(AST::VariableReference& variableReference)
{
    ASSERT(variableReference.variable());
    auto iterator = m_variableMapping.find(variableReference.variable());
    ASSERT(iterator != m_variableMapping.end());
    auto pointerName = generateNextVariableName();
    m_stringBuilder.append(makeString("thread ", m_typeNamer.mangledNameForType(variableReference.resolvedType()), "* ", pointerName, " = &", iterator->value, ";\n"));
    appendLeftValue(variableReference, iterator->value, pointerName);
}

String FunctionDefinitionWriter::constantExpressionString(AST::ConstantExpression& constantExpression)
{
    return constantExpression.visit(WTF::makeVisitor([&](AST::IntegerLiteral& integerLiteral) -> String {
        return makeString("", integerLiteral.value());
    }, [&](AST::UnsignedIntegerLiteral& unsignedIntegerLiteral) -> String {
        return makeString("", unsignedIntegerLiteral.value());
    }, [&](AST::FloatLiteral& floatLiteral) -> String {
        return makeString("", floatLiteral.value());
    }, [&](AST::NullLiteral&) -> String {
        return "nullptr"_str;
    }, [&](AST::BooleanLiteral& booleanLiteral) -> String {
        return booleanLiteral.value() ? "true"_str : "false"_str;
    }, [&](AST::EnumerationMemberLiteral& enumerationMemberLiteral) -> String {
        ASSERT(enumerationMemberLiteral.enumerationDefinition());
        ASSERT(enumerationMemberLiteral.enumerationDefinition());
        return makeString(m_typeNamer.mangledNameForType(*enumerationMemberLiteral.enumerationDefinition()), '.', m_typeNamer.mangledNameForEnumerationMember(*enumerationMemberLiteral.enumerationMember()));
    }));
}

class RenderFunctionDefinitionWriter : public FunctionDefinitionWriter {
public:
    RenderFunctionDefinitionWriter(Intrinsics& intrinsics, TypeNamer& typeNamer, HashMap<AST::FunctionDeclaration*, String>& functionMapping, MatchedRenderSemantics&& matchedSemantics, Layout& layout)
        : FunctionDefinitionWriter(intrinsics, typeNamer, functionMapping, layout)
        , m_matchedSemantics(WTFMove(matchedSemantics))
    {
    }

private:
    std::unique_ptr<EntryPointScaffolding> createEntryPointScaffolding(AST::FunctionDefinition&) override;

    MatchedRenderSemantics m_matchedSemantics;
};

std::unique_ptr<EntryPointScaffolding> RenderFunctionDefinitionWriter::createEntryPointScaffolding(AST::FunctionDefinition& functionDefinition)
{
    auto generateNextVariableName = [this]() -> String {
        return this->generateNextVariableName();
    };
    if (&functionDefinition == m_matchedSemantics.vertexShader)
        return std::make_unique<VertexEntryPointScaffolding>(functionDefinition, m_intrinsics, m_typeNamer, m_matchedSemantics.vertexShaderEntryPointItems, m_matchedSemantics.vertexShaderResourceMap, m_layout, WTFMove(generateNextVariableName), m_matchedSemantics.matchedVertexAttributes);
    if (&functionDefinition == m_matchedSemantics.fragmentShader)
        return std::make_unique<FragmentEntryPointScaffolding>(functionDefinition, m_intrinsics, m_typeNamer, m_matchedSemantics.fragmentShaderEntryPointItems, m_matchedSemantics.fragmentShaderResourceMap, m_layout, WTFMove(generateNextVariableName), m_matchedSemantics.matchedColorAttachments);
    return nullptr;
}

class ComputeFunctionDefinitionWriter : public FunctionDefinitionWriter {
public:
    ComputeFunctionDefinitionWriter(Intrinsics& intrinsics, TypeNamer& typeNamer, HashMap<AST::FunctionDeclaration*, String>& functionMapping, MatchedComputeSemantics&& matchedSemantics, Layout& layout)
        : FunctionDefinitionWriter(intrinsics, typeNamer, functionMapping, layout)
        , m_matchedSemantics(WTFMove(matchedSemantics))
    {
    }

private:
    std::unique_ptr<EntryPointScaffolding> createEntryPointScaffolding(AST::FunctionDefinition&) override;

    MatchedComputeSemantics m_matchedSemantics;
};

std::unique_ptr<EntryPointScaffolding> ComputeFunctionDefinitionWriter::createEntryPointScaffolding(AST::FunctionDefinition& functionDefinition)
{
    auto generateNextVariableName = [this]() -> String {
        return this->generateNextVariableName();
    };
    if (&functionDefinition == m_matchedSemantics.shader)
        return std::make_unique<ComputeEntryPointScaffolding>(functionDefinition, m_intrinsics, m_typeNamer, m_matchedSemantics.entryPointItems, m_matchedSemantics.resourceMap, m_layout, WTFMove(generateNextVariableName));
    return nullptr;
}

struct SharedMetalFunctionsResult {
    HashMap<AST::FunctionDeclaration*, String> functionMapping;
    String metalFunctions;
};
static SharedMetalFunctionsResult sharedMetalFunctions(Program& program, TypeNamer& typeNamer, const HashSet<AST::FunctionDeclaration*>& reachableFunctions)
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
        for (auto& nativeFunctionDeclaration : program.nativeFunctionDeclarations()) {
            if (reachableFunctions.contains(&nativeFunctionDeclaration))
                functionDeclarationWriter.visit(nativeFunctionDeclaration);
        }
        for (auto& functionDefinition : program.functionDefinitions()) {
            if (!functionDefinition->entryPointType() && reachableFunctions.contains(&functionDefinition))
                functionDeclarationWriter.visit(functionDefinition);
        }
        stringBuilder.append(functionDeclarationWriter.toString());
    }

    stringBuilder.append('\n');
    return { WTFMove(functionMapping), stringBuilder.toString() };
}

class ReachableFunctionsGatherer : public Visitor {
public:
    void visit(AST::FunctionDeclaration& functionDeclaration) override
    {
        Visitor::visit(functionDeclaration);
        m_reachableFunctions.add(&functionDeclaration);
    }

    void visit(AST::CallExpression& callExpression) override
    {
        Visitor::visit(callExpression);
        if (is<AST::FunctionDefinition>(callExpression.function()))
            checkErrorAndVisit(downcast<AST::FunctionDefinition>(callExpression.function()));
        else
            checkErrorAndVisit(downcast<AST::NativeFunctionDeclaration>(callExpression.function()));
    }

    HashSet<AST::FunctionDeclaration*> takeReachableFunctions() { return WTFMove(m_reachableFunctions); }

private:
    HashSet<AST::FunctionDeclaration*> m_reachableFunctions;
};

RenderMetalFunctions metalFunctions(Program& program, TypeNamer& typeNamer, MatchedRenderSemantics&& matchedSemantics, Layout& layout)
{
    auto& vertexShaderEntryPoint = *matchedSemantics.vertexShader;
    auto& fragmentShaderEntryPoint = *matchedSemantics.fragmentShader;

    ReachableFunctionsGatherer reachableFunctionsGatherer;
    reachableFunctionsGatherer.Visitor::visit(vertexShaderEntryPoint);
    reachableFunctionsGatherer.Visitor::visit(fragmentShaderEntryPoint);
    auto reachableFunctions = reachableFunctionsGatherer.takeReachableFunctions();

    auto sharedMetalFunctions = Metal::sharedMetalFunctions(program, typeNamer, reachableFunctions);

    StringBuilder stringBuilder;
    stringBuilder.append(sharedMetalFunctions.metalFunctions);

    RenderFunctionDefinitionWriter functionDefinitionWriter(program.intrinsics(), typeNamer, sharedMetalFunctions.functionMapping, WTFMove(matchedSemantics), layout);
    for (auto& nativeFunctionDeclaration : program.nativeFunctionDeclarations()) {
        if (reachableFunctions.contains(&nativeFunctionDeclaration))
            functionDefinitionWriter.visit(nativeFunctionDeclaration);
    }
    for (auto& functionDefinition : program.functionDefinitions()) {
        if (reachableFunctions.contains(&functionDefinition))
            functionDefinitionWriter.visit(functionDefinition);
    }
    stringBuilder.append(functionDefinitionWriter.toString());

    RenderMetalFunctions result;
    result.metalSource = stringBuilder.toString();
    result.mangledVertexEntryPointName = sharedMetalFunctions.functionMapping.get(&vertexShaderEntryPoint);
    result.mangledFragmentEntryPointName = sharedMetalFunctions.functionMapping.get(&fragmentShaderEntryPoint);
    return result;
}

ComputeMetalFunctions metalFunctions(Program& program, TypeNamer& typeNamer, MatchedComputeSemantics&& matchedSemantics, Layout& layout)
{
    auto& entryPoint = *matchedSemantics.shader;

    ReachableFunctionsGatherer reachableFunctionsGatherer;
    reachableFunctionsGatherer.Visitor::visit(entryPoint);
    auto reachableFunctions = reachableFunctionsGatherer.takeReachableFunctions();

    auto sharedMetalFunctions = Metal::sharedMetalFunctions(program, typeNamer, reachableFunctions);

    StringBuilder stringBuilder;
    stringBuilder.append(sharedMetalFunctions.metalFunctions);

    ComputeFunctionDefinitionWriter functionDefinitionWriter(program.intrinsics(), typeNamer, sharedMetalFunctions.functionMapping, WTFMove(matchedSemantics), layout);
    for (auto& nativeFunctionDeclaration : program.nativeFunctionDeclarations()) {
        if (reachableFunctions.contains(&nativeFunctionDeclaration))
            functionDefinitionWriter.visit(nativeFunctionDeclaration);
    }
    for (auto& functionDefinition : program.functionDefinitions()) {
        if (reachableFunctions.contains(&functionDefinition))
            functionDefinitionWriter.visit(functionDefinition);
    }
    stringBuilder.append(functionDefinitionWriter.toString());

    ComputeMetalFunctions result;
    result.metalSource = stringBuilder.toString();
    result.mangledEntryPointName = sharedMetalFunctions.functionMapping.get(&entryPoint);
    return result;
}

} // namespace Metal

} // namespace WHLSL

} // namespace WebCore

#endif
