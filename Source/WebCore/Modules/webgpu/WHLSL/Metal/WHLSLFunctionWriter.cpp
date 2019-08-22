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

static void declareFunction(StringBuilder& stringBuilder, AST::FunctionDeclaration& functionDeclaration, TypeNamer& typeNamer, HashMap<AST::FunctionDeclaration*, MangledFunctionName>& functionMapping)
{
    if (functionDeclaration.entryPointType())
        return;

    auto iterator = functionMapping.find(&functionDeclaration);
    ASSERT(iterator != functionMapping.end());
    stringBuilder.append(typeNamer.mangledNameForType(functionDeclaration.type()), ' ', iterator->value, '(');
    for (size_t i = 0; i < functionDeclaration.parameters().size(); ++i) {
        if (i)
            stringBuilder.append(", ");
        stringBuilder.append(typeNamer.mangledNameForType(*functionDeclaration.parameters()[i]->type()));
    }
    stringBuilder.append(");\n");
}

class FunctionDefinitionWriter : public Visitor {
public:
    FunctionDefinitionWriter(StringBuilder& stringBuilder, Intrinsics& intrinsics, TypeNamer& typeNamer, HashMap<AST::FunctionDeclaration*, MangledFunctionName>& functionMapping, Layout& layout)
        : m_stringBuilder(stringBuilder)
        , m_intrinsics(intrinsics)
        , m_typeNamer(typeNamer)
        , m_functionMapping(functionMapping)
        , m_layout(layout)
    {
    }

    virtual ~FunctionDefinitionWriter() = default;

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

    void emitConstantExpressionString(AST::ConstantExpression&);

    MangledVariableName generateNextVariableName() { return { m_variableCount++ }; }

    enum class Nullability : uint8_t {
        NotNull,
        CanBeNull
    };

    struct StackItem {
        MangledVariableName value;
        MangledVariableName leftValue;
        Nullability valueNullability;
        Nullability leftValueNullability;
        std::function<MangledVariableName()> generateLeftValue;
    };

    struct StackValue {
        MangledVariableName value;
        Nullability nullability;
    };

    // This is the important data flow step where we can take the nullability of an lvalue
    // and transfer it into the nullability of an rvalue. This is conveyed in MakePointerExpression
    // and DereferenceExpression. MakePointerExpression will try to produce rvalues which are
    // non-null, and DereferenceExpression will take a non-null rvalue and try to produce
    // a non-null lvalue.
    void appendRightValueWithNullability(AST::Expression&, MangledVariableName value, Nullability nullability)
    {
        m_stack.append({ WTFMove(value), { }, nullability, Nullability::CanBeNull, { } });
    }

    void appendRightValue(AST::Expression& expression, MangledVariableName value)
    {
        appendRightValueWithNullability(expression, WTFMove(value), Nullability::CanBeNull);
    }

    void appendLeftValue(AST::Expression& expression, MangledVariableName value, MangledVariableName leftValue, Nullability nullability, std::function<MangledVariableName()> generateLeftValue = { })
    {
        ASSERT_UNUSED(expression, expression.typeAnnotation().leftAddressSpace());
        ASSERT(leftValue || generateLeftValue);
        m_stack.append({ WTFMove(value), WTFMove(leftValue), Nullability::CanBeNull, nullability, WTFMove(generateLeftValue) });
    }

    MangledVariableName takeLastValue()
    {
        return m_stack.takeLast().value;
    }

    StackValue takeLastValueAndNullability()
    {
        auto last = m_stack.takeLast();
        return { last.value, last.valueNullability };
    }

    StackValue takeLastLeftValue()
    {
        auto last = m_stack.takeLast();
        if (!last.leftValue)
            last.leftValue = last.generateLeftValue();
        return { last.leftValue, last.leftValueNullability };
    }

    enum class BreakContext {
        Loop,
        Switch
    };

    Optional<BreakContext> m_currentBreakContext;

    StringBuilder& m_stringBuilder;
    Intrinsics& m_intrinsics;
    TypeNamer& m_typeNamer;
    HashMap<AST::FunctionDeclaration*, MangledFunctionName>& m_functionMapping;
    HashMap<AST::VariableDeclaration*, MangledVariableName> m_variableMapping;

    Vector<StackItem> m_stack;
    std::unique_ptr<EntryPointScaffolding> m_entryPointScaffolding;
    Layout& m_layout;
    unsigned m_variableCount { 0 };
    Optional<MangledVariableName> m_breakOutOfCurrentLoopEarlyVariable;
    Indentation<4> m_indent { 0 };
};

void FunctionDefinitionWriter::visit(AST::NativeFunctionDeclaration&)
{
    // We inline native function calls.
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
        
        m_entryPointScaffolding->emitHelperTypes(m_stringBuilder, m_indent);
        m_entryPointScaffolding->emitSignature(m_stringBuilder, iterator->value, m_indent);
        m_stringBuilder.append(m_indent, "{\n");
        {
            IndentationScope scope(m_indent);

            m_entryPointScaffolding->emitUnpack(m_stringBuilder, m_indent);
        
            for (size_t i = 0; i < functionDefinition.parameters().size(); ++i) {
                auto addResult = m_variableMapping.add(&functionDefinition.parameters()[i], m_entryPointScaffolding->parameterVariables()[i]);
                ASSERT_UNUSED(addResult, addResult.isNewEntry);
            }
            checkErrorAndVisit(functionDefinition.block());
            ASSERT(m_stack.isEmpty());
        }
        m_stringBuilder.append("}\n\n");

        m_entryPointScaffolding = nullptr;
    } else {
        ASSERT(m_entryPointScaffolding == nullptr);
        m_stringBuilder.append(m_indent, m_typeNamer.mangledNameForType(functionDefinition.type()), ' ', iterator->value, '(');
        for (size_t i = 0; i < functionDefinition.parameters().size(); ++i) {
            auto& parameter = functionDefinition.parameters()[i];
            if (i)
                m_stringBuilder.append(", ");
            auto parameterName = generateNextVariableName();
            auto addResult = m_variableMapping.add(&parameter, parameterName);
            ASSERT_UNUSED(addResult, addResult.isNewEntry);
            m_stringBuilder.append(m_typeNamer.mangledNameForType(*parameter->type()), ' ', parameterName);
        }
        m_stringBuilder.append(")\n");
        checkErrorAndVisit(functionDefinition.block());
        ASSERT(m_stack.isEmpty());
        m_stringBuilder.append('\n');
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
    m_stringBuilder.append(m_indent, "{\n");
    {
        IndentationScope scope(m_indent);
        for (auto& statement : block.statements())
            checkErrorAndVisit(statement);
    }
    m_stringBuilder.append(m_indent, "}\n");
}

void FunctionDefinitionWriter::visit(AST::Break&)
{
    ASSERT(m_currentBreakContext);
    switch (*m_currentBreakContext) {
    case BreakContext::Switch:
        m_stringBuilder.append(m_indent, "break;\n");
        break;
    case BreakContext::Loop:
        ASSERT(m_breakOutOfCurrentLoopEarlyVariable);
        m_stringBuilder.append(
            m_indent, *m_breakOutOfCurrentLoopEarlyVariable, " = true;\n",
            m_indent, "break;\n"
        );
        break;
    }
}

void FunctionDefinitionWriter::visit(AST::Continue&)
{
    ASSERT(m_breakOutOfCurrentLoopEarlyVariable);
    m_stringBuilder.append(m_indent, "break;\n");
}

void FunctionDefinitionWriter::visit(AST::EffectfulExpressionStatement& effectfulExpressionStatement)
{
    checkErrorAndVisit(effectfulExpressionStatement.effectfulExpression());
    takeLastValue(); // The statement is already effectful, so we don't need to do anything with the result.
}

void FunctionDefinitionWriter::visit(AST::Fallthrough&)
{
    m_stringBuilder.append(m_indent, "[[clang::fallthrough]];\n"); // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195808 Make sure this is okay. Alternatively, we could do nothing and just return here instead.
}

void FunctionDefinitionWriter::emitLoop(LoopConditionLocation loopConditionLocation, AST::Expression* conditionExpression, AST::Expression* increment, AST::Statement& body)
{
    SetForScope<Optional<MangledVariableName>> loopVariableScope(m_breakOutOfCurrentLoopEarlyVariable, generateNextVariableName());

    m_stringBuilder.append(
        m_indent, "bool ", *m_breakOutOfCurrentLoopEarlyVariable, " = false;\n",
        m_indent, "while (true) {\n"
    );
    {
        IndentationScope whileScope(m_indent);

        if (loopConditionLocation == LoopConditionLocation::BeforeBody && conditionExpression) {
            checkErrorAndVisit(*conditionExpression);
            m_stringBuilder.append(
                m_indent, "if (!", takeLastValue(), ")\n",
                m_indent, "    break;\n");
        }

        m_stringBuilder.append(m_indent, "do {\n");
        SetForScope<Optional<BreakContext>> breakContext(m_currentBreakContext, BreakContext::Loop);

        {
            IndentationScope doScope(m_indent);
            checkErrorAndVisit(body);
        }
        m_stringBuilder.append(m_indent, "} while(false); \n");

        m_stringBuilder.append(
            m_indent, "if (", *m_breakOutOfCurrentLoopEarlyVariable, ")\n",
            m_indent, "    break;\n");

        if (increment) {
            checkErrorAndVisit(*increment);
            // Expression results get pushed to m_stack. We don't use the result
            // of increment, so we dispense of that now.
            takeLastValue();
        }

        if (loopConditionLocation == LoopConditionLocation::AfterBody && conditionExpression) {
            checkErrorAndVisit(*conditionExpression);
            m_stringBuilder.append(
                m_indent, "if (!", takeLastValue(), ")\n",
                m_indent, "    break;\n");
        }
    }

    m_stringBuilder.append(m_indent, "} \n");
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
    m_stringBuilder.append(m_indent, "{\n");
    {
        IndentationScope scope(m_indent);
        checkErrorAndVisit(forLoop.initialization());
        emitLoop(LoopConditionLocation::BeforeBody, forLoop.condition(), forLoop.increment(), forLoop.body());
    }
    m_stringBuilder.append(m_indent, "}\n");
}

void FunctionDefinitionWriter::visit(AST::IfStatement& ifStatement)
{
    checkErrorAndVisit(ifStatement.conditional());
    m_stringBuilder.append(m_indent, "if (", takeLastValue(), ") {\n");
    {
        IndentationScope ifScope(m_indent);
        checkErrorAndVisit(ifStatement.body());
    }
    if (ifStatement.elseBody()) {
        m_stringBuilder.append(m_indent, "} else {\n");
        {
            IndentationScope elseScope(m_indent);
            checkErrorAndVisit(*ifStatement.elseBody());
        }
    }
    m_stringBuilder.append(m_indent, "}\n");
}

void FunctionDefinitionWriter::visit(AST::Return& returnStatement)
{
    if (returnStatement.value()) {
        checkErrorAndVisit(*returnStatement.value());

        if (m_entryPointScaffolding) {
            auto variableName = generateNextVariableName();
            m_entryPointScaffolding->emitPack(m_stringBuilder, takeLastValue(), variableName, m_indent);
            m_stringBuilder.append(m_indent, "return ", variableName, ";\n");
        } else
            m_stringBuilder.append(m_indent, "return ", takeLastValue(), ";\n");
    } else
        m_stringBuilder.append(m_indent, "return;\n");
}

void FunctionDefinitionWriter::visit(AST::SwitchStatement& switchStatement)
{
    checkErrorAndVisit(switchStatement.value());

    m_stringBuilder.append(m_indent, "switch (", takeLastValue(), ") {");
    {
        IndentationScope switchScope(m_indent);
        for (auto& switchCase : switchStatement.switchCases())
            checkErrorAndVisit(switchCase);
    }
    m_stringBuilder.append(m_indent, "}\n");
}

void FunctionDefinitionWriter::visit(AST::SwitchCase& switchCase)
{
    if (switchCase.value()) {
        m_stringBuilder.append(m_indent, "case ");
        emitConstantExpressionString(*switchCase.value());
        m_stringBuilder.append(":\n");
    } else
        m_stringBuilder.append(m_indent, "default:\n");
    SetForScope<Optional<BreakContext>> breakContext(m_currentBreakContext, BreakContext::Switch);
    checkErrorAndVisit(switchCase.block());
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195812 Figure out whether we need to break or fallthrough.
}

void FunctionDefinitionWriter::visit(AST::VariableDeclarationsStatement& variableDeclarationsStatement)
{
    Visitor::visit(variableDeclarationsStatement);
}

void FunctionDefinitionWriter::visit(AST::IntegerLiteral& integerLiteral)
{
    auto variableName = generateNextVariableName();
    auto mangledTypeName = m_typeNamer.mangledNameForType(integerLiteral.resolvedType());
    m_stringBuilder.append(m_indent, mangledTypeName, ' ', variableName, " = static_cast<", mangledTypeName, ">(", integerLiteral.value(), ");\n");
    appendRightValue(integerLiteral, variableName);
}

void FunctionDefinitionWriter::visit(AST::UnsignedIntegerLiteral& unsignedIntegerLiteral)
{
    auto variableName = generateNextVariableName();
    auto mangledTypeName = m_typeNamer.mangledNameForType(unsignedIntegerLiteral.resolvedType());
    m_stringBuilder.append(m_indent, mangledTypeName, ' ', variableName, " = static_cast<", mangledTypeName, ">(", unsignedIntegerLiteral.value(), ");\n");
    appendRightValue(unsignedIntegerLiteral, variableName);
}

void FunctionDefinitionWriter::visit(AST::FloatLiteral& floatLiteral)
{
    auto variableName = generateNextVariableName();
    auto mangledTypeName = m_typeNamer.mangledNameForType(floatLiteral.resolvedType());
    m_stringBuilder.append(m_indent, mangledTypeName, ' ', variableName, " = static_cast<", mangledTypeName, ">(", floatLiteral.value(), ");\n");
    appendRightValue(floatLiteral, variableName);
}

void FunctionDefinitionWriter::visit(AST::NullLiteral& nullLiteral)
{
    auto& unifyNode = nullLiteral.resolvedType().unifyNode();
    auto& unnamedType = downcast<AST::UnnamedType>(unifyNode);
    bool isArrayReferenceType = is<AST::ArrayReferenceType>(unnamedType);

    auto variableName = generateNextVariableName();
    m_stringBuilder.append(m_indent, m_typeNamer.mangledNameForType(nullLiteral.resolvedType()), ' ', variableName, " = ");
    if (isArrayReferenceType)
        m_stringBuilder.append("{ nullptr, 0 };\n");
    else
        m_stringBuilder.append("nullptr;\n");
    appendRightValue(nullLiteral, variableName);
}

void FunctionDefinitionWriter::visit(AST::BooleanLiteral& booleanLiteral)
{
    auto variableName = generateNextVariableName();
    auto mangledTypeName = m_typeNamer.mangledNameForType(booleanLiteral.resolvedType());
    m_stringBuilder.append(m_indent, mangledTypeName, ' ', variableName, " = static_cast<", mangledTypeName, ">(", booleanLiteral.value() ? "true" : "false", ");\n");
    appendRightValue(booleanLiteral, variableName);
}

void FunctionDefinitionWriter::visit(AST::EnumerationMemberLiteral& enumerationMemberLiteral)
{
    ASSERT(enumerationMemberLiteral.enumerationDefinition());
    ASSERT(enumerationMemberLiteral.enumerationDefinition());
    auto variableName = generateNextVariableName();
    auto mangledTypeName = m_typeNamer.mangledNameForType(enumerationMemberLiteral.resolvedType());
    m_stringBuilder.append(m_indent, mangledTypeName, ' ', variableName, " = ", mangledTypeName, "::", m_typeNamer.mangledNameForEnumerationMember(*enumerationMemberLiteral.enumerationMember()), ";\n");
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
    appendRightValue(dotExpression, generateNextVariableName());
}

void FunctionDefinitionWriter::visit(AST::GlobalVariableReference& globalVariableReference)
{
    auto valueName = generateNextVariableName();
    MangledTypeName mangledTypeName = m_typeNamer.mangledNameForType(globalVariableReference.resolvedType());

    checkErrorAndVisit(globalVariableReference.base());
    MangledVariableName structVariable = takeLastValue();

    MangledStructureElementName mangledFieldName = m_typeNamer.mangledNameForStructureElement(globalVariableReference.structField());

    m_stringBuilder.append(
        m_indent, mangledTypeName, ' ', valueName, " = ", structVariable, "->", mangledFieldName, ";\n");

    appendLeftValue(globalVariableReference, valueName, { }, Nullability::NotNull,
        [this, mangledTypeName, structVariable, mangledFieldName] {
            auto pointerName = generateNextVariableName();
            m_stringBuilder.append(
                m_indent, "thread ", mangledTypeName, "* ", pointerName, " = &", structVariable, "->", mangledFieldName, ";\n");
            return pointerName;
        });
}

void FunctionDefinitionWriter::visit(AST::IndexExpression& indexExpression)
{
    // This should be lowered already.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195788 Replace this with ASSERT_NOT_REACHED().
    notImplemented();
    appendRightValue(indexExpression, generateNextVariableName());
}

void FunctionDefinitionWriter::visit(AST::PropertyAccessExpression& propertyAccessExpression)
{
    // This should be lowered already.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195788 Replace this with ASSERT_NOT_REACHED().
    notImplemented();
    appendRightValue(propertyAccessExpression, generateNextVariableName());
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
        m_stringBuilder.append(m_indent, m_typeNamer.mangledNameForType(*variableDeclaration.type()), ' ', variableName, " = ", takeLastValue(), ";\n");
    } else
        m_stringBuilder.append(m_indent, m_typeNamer.mangledNameForType(*variableDeclaration.type()), ' ', variableName, " = { };\n");
}

void FunctionDefinitionWriter::visit(AST::AssignmentExpression& assignmentExpression)
{
    checkErrorAndVisit(assignmentExpression.left());
    auto [pointerName, nullability] = takeLastLeftValue();
    checkErrorAndVisit(assignmentExpression.right());
    auto [rightName, rightNullability] = takeLastValueAndNullability();

    if (nullability == Nullability::CanBeNull)
        m_stringBuilder.append(
            m_indent, "if (", pointerName, ")\n",
            m_indent, "    *", pointerName, " = ", rightName, ";\n");
    else
        m_stringBuilder.append(m_indent, "*", pointerName, " = ", rightName, ";\n");
    appendRightValueWithNullability(assignmentExpression, rightName, rightNullability);
}

void FunctionDefinitionWriter::visit(AST::CallExpression& callExpression)
{
    Vector<MangledVariableName> argumentNames;
    for (auto& argument : callExpression.arguments()) {
        checkErrorAndVisit(argument);
        argumentNames.append(takeLastValue());
    }

    bool isVoid = matches(callExpression.resolvedType(), m_intrinsics.voidType());
    MangledVariableName returnName;
    if (!isVoid) {
        returnName = generateNextVariableName();
        m_stringBuilder.append(m_indent, m_typeNamer.mangledNameForType(callExpression.resolvedType()), ' ', returnName, ";\n");
    }

    if (is<AST::NativeFunctionDeclaration>(callExpression.function())) {
        auto generateNextVariableName = [this]() -> MangledVariableName {
            return this->generateNextVariableName();
        };

        m_stringBuilder.append('\n');
        inlineNativeFunction(m_stringBuilder, downcast<AST::NativeFunctionDeclaration>(callExpression.function()), returnName, argumentNames, m_intrinsics, m_typeNamer, WTFMove(generateNextVariableName), m_indent);
        m_stringBuilder.append('\n');
    } else {
        m_stringBuilder.append(m_indent);

        auto iterator = m_functionMapping.find(&callExpression.function());
        ASSERT(iterator != m_functionMapping.end());
        if (!isVoid)
            m_stringBuilder.append(returnName, " = ");
        m_stringBuilder.append(iterator->value, '(');
        for (size_t i = 0; i < argumentNames.size(); ++i) {
            if (i)
                m_stringBuilder.append(", ");
            m_stringBuilder.append(argumentNames[i]);
        }
        m_stringBuilder.append(");\n");
    }

    appendRightValue(callExpression, returnName);
}

void FunctionDefinitionWriter::visit(AST::CommaExpression& commaExpression)
{
    Optional<MangledVariableName> result;
    for (auto& expression : commaExpression.list()) {
        checkErrorAndVisit(expression);
        result = takeLastValue();
    }
    ASSERT(result);
    appendRightValue(commaExpression, *result);
}

void FunctionDefinitionWriter::visit(AST::DereferenceExpression& dereferenceExpression)
{
    checkErrorAndVisit(dereferenceExpression.pointer());
    auto [inputPointer, nullability] = takeLastValueAndNullability();
    auto resultValue = generateNextVariableName();
    auto resultType = m_typeNamer.mangledNameForType(dereferenceExpression.resolvedType());

    if (nullability == Nullability::CanBeNull) {
        m_stringBuilder.append(
            m_indent, resultType , ' ', resultValue, " = ", inputPointer, " ? ", '*', inputPointer, " : ", resultType, "{ };\n");
    } else
        m_stringBuilder.append(m_indent, resultValue, " = *", inputPointer, ";\n");

    appendLeftValue(dereferenceExpression, resultValue, inputPointer, nullability);
}

void FunctionDefinitionWriter::visit(AST::LogicalExpression& logicalExpression)
{
    checkErrorAndVisit(logicalExpression.left());
    auto left = takeLastValue();
    checkErrorAndVisit(logicalExpression.right());
    auto right = takeLastValue();
    auto variableName = generateNextVariableName();

    m_stringBuilder.append(
        m_indent, m_typeNamer.mangledNameForType(logicalExpression.resolvedType()), ' ', variableName, " = ", left);
    switch (logicalExpression.type()) {
    case AST::LogicalExpression::Type::And:
        m_stringBuilder.append(" && ");
        break;
    default:
        ASSERT(logicalExpression.type() == AST::LogicalExpression::Type::Or);
        m_stringBuilder.append(" || ");
        break;
    }
    m_stringBuilder.append(right, ";\n");
    appendRightValue(logicalExpression, variableName);
}

void FunctionDefinitionWriter::visit(AST::LogicalNotExpression& logicalNotExpression)
{
    checkErrorAndVisit(logicalNotExpression.operand());
    auto operand = takeLastValue();
    auto variableName = generateNextVariableName();

    m_stringBuilder.append(
        m_indent, m_typeNamer.mangledNameForType(logicalNotExpression.resolvedType()), ' ', variableName, " = !", operand, ";\n");
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
        m_stringBuilder.append(
            m_indent, mangledTypeName, ' ', variableName, " = ", ptrValue, " ? ", mangledTypeName, "{ ", ptrValue, ", 1 } : ", mangledTypeName, "{ nullptr, 0 };\n");
    } else if (is<AST::ArrayType>(makeArrayReferenceExpression.leftValue().resolvedType())) {
        auto lValue = takeLastLeftValue().value;
        auto& arrayType = downcast<AST::ArrayType>(makeArrayReferenceExpression.leftValue().resolvedType());
        m_stringBuilder.append(m_indent, mangledTypeName, ' ', variableName, " = { ", lValue, "->data(), ", arrayType.numElements(), " };\n");
    } else {
        auto lValue = takeLastLeftValue().value;
        m_stringBuilder.append(m_indent, mangledTypeName, ' ', variableName, " = { ", lValue, ", 1 };\n");
    }
    appendRightValue(makeArrayReferenceExpression, variableName);
}

void FunctionDefinitionWriter::visit(AST::MakePointerExpression& makePointerExpression)
{
    checkErrorAndVisit(makePointerExpression.leftValue());
    auto [pointer, nullability] = takeLastLeftValue();
    auto variableName = generateNextVariableName();
    m_stringBuilder.append(m_indent, m_typeNamer.mangledNameForType(makePointerExpression.resolvedType()), ' ', variableName, " = ", pointer, ";\n");
    appendRightValueWithNullability(makePointerExpression, variableName, nullability);
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
    checkErrorAndVisit(ternaryExpression.bodyExpression());
    auto body = takeLastValue();
    checkErrorAndVisit(ternaryExpression.elseExpression());
    auto elseBody = takeLastValue();

    auto variableName = generateNextVariableName();
    m_stringBuilder.append(m_indent, m_typeNamer.mangledNameForType(ternaryExpression.resolvedType()), ' ', variableName, " = ", check, " ? ", body, " : ", elseBody, ";\n");
    appendRightValue(ternaryExpression, variableName);
}

void FunctionDefinitionWriter::visit(AST::VariableReference& variableReference)
{
    ASSERT(variableReference.variable());
    auto iterator = m_variableMapping.find(variableReference.variable());
    ASSERT(iterator != m_variableMapping.end());

    MangledVariableName variableName = iterator->value;

    appendLeftValue(variableReference, variableName, { }, Nullability::NotNull,
        [this, &variableReference, variableName] {
            auto pointerName = generateNextVariableName();
            m_stringBuilder.append(m_indent, "thread ", m_typeNamer.mangledNameForType(variableReference.resolvedType()), "* ", pointerName, " = &", variableName, ";\n");
            return pointerName;
        });
}

void FunctionDefinitionWriter::emitConstantExpressionString(AST::ConstantExpression& constantExpression)
{
    constantExpression.visit(WTF::makeVisitor(
        [&](AST::IntegerLiteral& integerLiteral) {
            m_stringBuilder.append(integerLiteral.value());
        },
        [&](AST::UnsignedIntegerLiteral& unsignedIntegerLiteral) {
            m_stringBuilder.append(unsignedIntegerLiteral.value());
        },
        [&](AST::FloatLiteral& floatLiteral) {
            m_stringBuilder.append(floatLiteral.value());
        },
        [&](AST::NullLiteral&) {
            m_stringBuilder.append("nullptr");
        },
        [&](AST::BooleanLiteral& booleanLiteral) {
            if (booleanLiteral.value())
                m_stringBuilder.append("true");
            else
                m_stringBuilder.append("false");
        },
        [&](AST::EnumerationMemberLiteral& enumerationMemberLiteral) {
            ASSERT(enumerationMemberLiteral.enumerationDefinition());
            ASSERT(enumerationMemberLiteral.enumerationDefinition());
            m_stringBuilder.append(m_typeNamer.mangledNameForType(*enumerationMemberLiteral.enumerationDefinition()), "::", m_typeNamer.mangledNameForEnumerationMember(*enumerationMemberLiteral.enumerationMember()));
        }
    ));
}

class RenderFunctionDefinitionWriter final : public FunctionDefinitionWriter {
public:
    RenderFunctionDefinitionWriter(StringBuilder& stringBuilder, Intrinsics& intrinsics, TypeNamer& typeNamer, HashMap<AST::FunctionDeclaration*, MangledFunctionName>& functionMapping, MatchedRenderSemantics&& matchedSemantics, Layout& layout)
        : FunctionDefinitionWriter(stringBuilder, intrinsics, typeNamer, functionMapping, layout)
        , m_matchedSemantics(WTFMove(matchedSemantics))
    {
    }

private:
    std::unique_ptr<EntryPointScaffolding> createEntryPointScaffolding(AST::FunctionDefinition&) override;

    MatchedRenderSemantics m_matchedSemantics;
};

std::unique_ptr<EntryPointScaffolding> RenderFunctionDefinitionWriter::createEntryPointScaffolding(AST::FunctionDefinition& functionDefinition)
{
    auto generateNextVariableName = [this]() -> MangledVariableName {
        return this->generateNextVariableName();
    };
    if (&functionDefinition == m_matchedSemantics.vertexShader)
        return makeUnique<VertexEntryPointScaffolding>(functionDefinition, m_intrinsics, m_typeNamer, m_matchedSemantics.vertexShaderEntryPointItems, m_matchedSemantics.vertexShaderResourceMap, m_layout, WTFMove(generateNextVariableName), m_matchedSemantics.matchedVertexAttributes);
    if (&functionDefinition == m_matchedSemantics.fragmentShader)
        return makeUnique<FragmentEntryPointScaffolding>(functionDefinition, m_intrinsics, m_typeNamer, m_matchedSemantics.fragmentShaderEntryPointItems, m_matchedSemantics.fragmentShaderResourceMap, m_layout, WTFMove(generateNextVariableName), m_matchedSemantics.matchedColorAttachments);
    return nullptr;
}

class ComputeFunctionDefinitionWriter final : public FunctionDefinitionWriter {
public:
    ComputeFunctionDefinitionWriter(StringBuilder& stringBuilder, Intrinsics& intrinsics, TypeNamer& typeNamer, HashMap<AST::FunctionDeclaration*, MangledFunctionName>& functionMapping, MatchedComputeSemantics&& matchedSemantics, Layout& layout)
        : FunctionDefinitionWriter(stringBuilder, intrinsics, typeNamer, functionMapping, layout)
        , m_matchedSemantics(WTFMove(matchedSemantics))
    {
    }

private:
    std::unique_ptr<EntryPointScaffolding> createEntryPointScaffolding(AST::FunctionDefinition&) override;

    MatchedComputeSemantics m_matchedSemantics;
};

std::unique_ptr<EntryPointScaffolding> ComputeFunctionDefinitionWriter::createEntryPointScaffolding(AST::FunctionDefinition& functionDefinition)
{
    auto generateNextVariableName = [this]() -> MangledVariableName {
        return this->generateNextVariableName();
    };
    if (&functionDefinition == m_matchedSemantics.shader)
        return makeUnique<ComputeEntryPointScaffolding>(functionDefinition, m_intrinsics, m_typeNamer, m_matchedSemantics.entryPointItems, m_matchedSemantics.resourceMap, m_layout, WTFMove(generateNextVariableName));
    return nullptr;
}

static HashMap<AST::FunctionDeclaration*, MangledFunctionName> generateMetalFunctionsMapping(Program& program)
{
    unsigned numFunctions = 0;
    HashMap<AST::FunctionDeclaration*, MangledFunctionName> functionMapping;
    for (auto& functionDefinition : program.functionDefinitions()) {
        auto addResult = functionMapping.add(&functionDefinition, MangledFunctionName { numFunctions++ });
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }

    return functionMapping;
}

static void emitSharedMetalFunctions(StringBuilder& stringBuilder, Program& program, TypeNamer& typeNamer, const HashSet<AST::FunctionDeclaration*>& reachableFunctions, HashMap<AST::FunctionDeclaration*, MangledFunctionName>& functionMapping)
{
    for (auto& functionDefinition : program.functionDefinitions()) {
        if (!functionDefinition->entryPointType() && reachableFunctions.contains(&functionDefinition))
            declareFunction(stringBuilder, functionDefinition, typeNamer, functionMapping);
    }

    stringBuilder.append('\n');
}

class ReachableFunctionsGatherer final : public Visitor {
public:
    void visit(AST::FunctionDeclaration& functionDeclaration) override
    {
        auto result = m_reachableFunctions.add(&functionDeclaration);
        if (result.isNewEntry)
            Visitor::visit(functionDeclaration);
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

RenderMetalFunctionEntryPoints emitMetalFunctions(StringBuilder& stringBuilder, Program& program, TypeNamer& typeNamer, MatchedRenderSemantics&& matchedSemantics, Layout& layout)
{
    auto& vertexShaderEntryPoint = *matchedSemantics.vertexShader;
    auto* fragmentShaderEntryPoint = matchedSemantics.fragmentShader;

    ReachableFunctionsGatherer reachableFunctionsGatherer;
    reachableFunctionsGatherer.Visitor::visit(vertexShaderEntryPoint);
    if (fragmentShaderEntryPoint)
        reachableFunctionsGatherer.Visitor::visit(*fragmentShaderEntryPoint);
    auto reachableFunctions = reachableFunctionsGatherer.takeReachableFunctions();

    auto functionMapping = generateMetalFunctionsMapping(program);
    
    emitSharedMetalFunctions(stringBuilder, program, typeNamer, reachableFunctions, functionMapping);

    RenderFunctionDefinitionWriter functionDefinitionWriter(stringBuilder, program.intrinsics(), typeNamer, functionMapping, WTFMove(matchedSemantics), layout);
    for (auto& functionDefinition : program.functionDefinitions()) {
        if (reachableFunctions.contains(&functionDefinition))
            functionDefinitionWriter.visit(functionDefinition);
    }

    return { functionMapping.get(&vertexShaderEntryPoint), fragmentShaderEntryPoint ? functionMapping.get(fragmentShaderEntryPoint) : MangledFunctionName { 0 } };
}

ComputeMetalFunctionEntryPoints emitMetalFunctions(StringBuilder& stringBuilder, Program& program, TypeNamer& typeNamer, MatchedComputeSemantics&& matchedSemantics, Layout& layout)
{
    auto& entryPoint = *matchedSemantics.shader;

    ReachableFunctionsGatherer reachableFunctionsGatherer;
    reachableFunctionsGatherer.Visitor::visit(entryPoint);
    auto reachableFunctions = reachableFunctionsGatherer.takeReachableFunctions();

    auto functionMapping = generateMetalFunctionsMapping(program);
    emitSharedMetalFunctions(stringBuilder, program, typeNamer, reachableFunctions, functionMapping);

    ComputeFunctionDefinitionWriter functionDefinitionWriter(stringBuilder, program.intrinsics(), typeNamer, functionMapping, WTFMove(matchedSemantics), layout);
    for (auto& functionDefinition : program.functionDefinitions()) {
        if (reachableFunctions.contains(&functionDefinition))
            functionDefinitionWriter.visit(functionDefinition);
    }

    return { functionMapping.get(&entryPoint) };
}

} // namespace Metal

} // namespace WHLSL

} // namespace WebCore

#endif
