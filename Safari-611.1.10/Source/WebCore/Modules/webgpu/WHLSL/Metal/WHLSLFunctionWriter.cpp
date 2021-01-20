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

#if ENABLE(WHLSL_COMPILER)

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

struct Variable {
    MangledVariableName name;
    MangledTypeName type;
};

class FunctionDefinitionWriter : public Visitor {
    class HoistedVariableCollector : public Visitor {
        public:
        HoistedVariableCollector(FunctionDefinitionWriter& functionDefinitionWriter)
            : functionDefinitionWriter(functionDefinitionWriter)
        {
        }

        void visit(AST::CallExpression& callExpression) override
        {
            Vector<Variable> variables;
            size_t size = callExpression.arguments().size();
            bool isVoid = matches(callExpression.resolvedType(), functionDefinitionWriter.m_intrinsics.voidType());
            if (!isVoid)
                ++size;
            variables.reserveInitialCapacity(size);

            for (auto& argument : callExpression.arguments()) {
                auto type = functionDefinitionWriter.m_typeNamer.mangledNameForType(argument->resolvedType());
                auto name = functionDefinitionWriter.generateNextVariableName();
                variables.uncheckedAppend(Variable { name, type });
            }

            if (!isVoid)
                variables.uncheckedAppend(Variable { functionDefinitionWriter.generateNextVariableName(), functionDefinitionWriter.m_typeNamer.mangledNameForType(callExpression.resolvedType()) });

            toHoist.add(&callExpression, WTFMove(variables));

            Visitor::visit(callExpression);
        }

        void visit(AST::ReadModifyWriteExpression& readModifyWrite) override
        {
            Vector<Variable> variables;
            variables.append(Variable { functionDefinitionWriter.generateNextVariableName(), functionDefinitionWriter.m_typeNamer.mangledNameForType(*readModifyWrite.oldValue().type()) });
            variables.append(Variable { functionDefinitionWriter.generateNextVariableName(), functionDefinitionWriter.m_typeNamer.mangledNameForType(*readModifyWrite.newValue().type()) });

            toHoist.add(&readModifyWrite, WTFMove(variables));

            Visitor::visit(readModifyWrite);
        }

        FunctionDefinitionWriter& functionDefinitionWriter;
        HashMap<AST::Expression*, Vector<Variable>> toHoist;
    };

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
    void visit(AST::BooleanLiteral&) override;
    void visit(AST::EnumerationMemberLiteral&) override;
    void visit(AST::Expression&) override;
    void visit(AST::GlobalVariableReference&) override;
    void visit(AST::DotExpression&) override;
    void visit(AST::IndexExpression&) override;
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

    std::unique_ptr<EntryPointScaffolding> m_entryPointScaffolding;
    Layout& m_layout;
    unsigned m_variableCount { 0 };
    Optional<MangledVariableName> m_breakOutOfCurrentLoopEarlyVariable;
    Indentation<4> m_indent { 0 };
    HashMap<AST::Expression*, Vector<Variable>> m_hoistedVariables;
};

void FunctionDefinitionWriter::visit(AST::NativeFunctionDeclaration&)
{
    // We inline native function calls.
}

void FunctionDefinitionWriter::visit(AST::FunctionDefinition& functionDefinition)
{

    {
        HoistedVariableCollector collector(*this);
        collector.Visitor::visit(functionDefinition);
        m_hoistedVariables = WTFMove(collector.toHoist);
    }

    auto defineHoistedVariables = [&] {
        for (const auto& vector : m_hoistedVariables.values()) {
            for (auto variable : vector)
                m_stringBuilder.append(m_indent, variable.type, ' ', variable.name, ";\n");
        }
    };

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

            defineHoistedVariables();

            checkErrorAndVisit(functionDefinition.block());
        }
        m_stringBuilder.append("}\n\n");

        m_entryPointScaffolding = nullptr;
    } else {
        ASSERT(m_entryPointScaffolding == nullptr);
        m_stringBuilder.append(m_indent, m_typeNamer.mangledNameForType(functionDefinition.type()), ' ', iterator->value, '(');
        for (size_t i = 0; i < functionDefinition.parameters().size(); ++i) {
            if (i)
                m_stringBuilder.append(", ");
            auto& parameter = functionDefinition.parameters()[i];
            auto parameterName = generateNextVariableName();
            auto addResult = m_variableMapping.add(&parameter, parameterName);
            ASSERT_UNUSED(addResult, addResult.isNewEntry);
            m_stringBuilder.append(m_typeNamer.mangledNameForType(*parameter->type()), ' ', parameterName);
        }
        m_stringBuilder.append(")\n");

        m_stringBuilder.append("{\n");

        defineHoistedVariables();

        checkErrorAndVisit(functionDefinition.block());
        m_stringBuilder.append("}\n");
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
    m_stringBuilder.append(m_indent);
    checkErrorAndVisit(effectfulExpressionStatement.effectfulExpression());
    m_stringBuilder.append(";\n");
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
            m_stringBuilder.append(
                m_indent, "if (!(");
            checkErrorAndVisit(*conditionExpression);
            m_stringBuilder.append(
                "))\n",
                "    break;\n");
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
            m_stringBuilder.append("(");
            checkErrorAndVisit(*increment);
            m_stringBuilder.append(");\n");
        }

        if (loopConditionLocation == LoopConditionLocation::AfterBody && conditionExpression) {
            m_stringBuilder.append(
                m_indent, "if (!(");
            checkErrorAndVisit(*conditionExpression);
            m_stringBuilder.append(
                "))\n",
                "    break;\n");
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
    m_stringBuilder.append(m_indent, "if (");
    checkErrorAndVisit(ifStatement.conditional());
    m_stringBuilder.append(") {\n");

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
        auto tempReturnName = generateNextVariableName(); 
        m_stringBuilder.append(m_indent, m_typeNamer.mangledNameForType(returnStatement.value()->resolvedType()), ' ', tempReturnName, " = "); 
        checkErrorAndVisit(*returnStatement.value());
        m_stringBuilder.append(";\n");

        if (m_entryPointScaffolding) {
            auto variableName = generateNextVariableName();
            m_entryPointScaffolding->emitPack(m_stringBuilder, tempReturnName, variableName, m_indent);
            m_stringBuilder.append(m_indent, "return ", variableName, ";\n");
        } else
            m_stringBuilder.append(m_indent, "return ", tempReturnName, ";\n");
    } else
        m_stringBuilder.append(m_indent, "return;\n");
}

void FunctionDefinitionWriter::visit(AST::SwitchStatement& switchStatement)
{
    m_stringBuilder.append(m_indent, "switch (");
    checkErrorAndVisit(switchStatement.value());
    m_stringBuilder.append(") {");

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
    auto mangledTypeName = m_typeNamer.mangledNameForType(integerLiteral.resolvedType());
    m_stringBuilder.append("static_cast<", mangledTypeName, ">(", integerLiteral.value(), ")");
}

void FunctionDefinitionWriter::visit(AST::UnsignedIntegerLiteral& unsignedIntegerLiteral)
{
    auto mangledTypeName = m_typeNamer.mangledNameForType(unsignedIntegerLiteral.resolvedType());
    m_stringBuilder.append("static_cast<", mangledTypeName, ">(", unsignedIntegerLiteral.value(), ")");
}

void FunctionDefinitionWriter::visit(AST::FloatLiteral& floatLiteral)
{
    auto mangledTypeName = m_typeNamer.mangledNameForType(floatLiteral.resolvedType());
    m_stringBuilder.append("static_cast<", mangledTypeName, ">(", floatLiteral.value(), ")");
}

void FunctionDefinitionWriter::visit(AST::BooleanLiteral& booleanLiteral)
{
    auto mangledTypeName = m_typeNamer.mangledNameForType(booleanLiteral.resolvedType());
    m_stringBuilder.append("static_cast<", mangledTypeName, ">(", booleanLiteral.value() ? "true" : "false", ")");
}

void FunctionDefinitionWriter::visit(AST::EnumerationMemberLiteral& enumerationMemberLiteral)
{
    ASSERT(enumerationMemberLiteral.enumerationDefinition());
    ASSERT(enumerationMemberLiteral.enumerationDefinition());
    auto mangledTypeName = m_typeNamer.mangledNameForType(enumerationMemberLiteral.resolvedType());
    m_stringBuilder.append(mangledTypeName, "::", m_typeNamer.mangledNameForEnumerationMember(*enumerationMemberLiteral.enumerationMember()));
}

void FunctionDefinitionWriter::visit(AST::Expression& expression)
{
    Visitor::visit(expression);
}

void FunctionDefinitionWriter::visit(AST::GlobalVariableReference& globalVariableReference)
{
    MangledStructureElementName mangledFieldName = m_typeNamer.mangledNameForStructureElement(globalVariableReference.structField());
    m_stringBuilder.append('(');
    checkErrorAndVisit(globalVariableReference.base());
    m_stringBuilder.append(")->", mangledFieldName);
}

void FunctionDefinitionWriter::visit(AST::DotExpression& dotExpression)
{
    auto& type = dotExpression.base().resolvedType().unifyNode();

    if (is<AST::StructureDefinition>(type)) {
        auto& structureDefinition = downcast<AST::StructureDefinition>(type);
        auto* structureElement = structureDefinition.find(dotExpression.fieldName());
        ASSERT(structureElement);
        auto elementName = m_typeNamer.mangledNameForStructureElement(*structureElement);

        m_stringBuilder.append('(');
        checkErrorAndVisit(dotExpression.base());
        m_stringBuilder.append(").", elementName);
    } else {
        String elementName = dotExpression.fieldName();
        if (elementName == "length" && (is<AST::ArrayReferenceType>(type) || is<AST::ArrayType>(type) || (is<AST::NativeTypeDeclaration>(type) && downcast<AST::NativeTypeDeclaration>(type).isVector()))) {
            if (is<AST::ArrayReferenceType>(type)) {
                m_stringBuilder.append('(');
                checkErrorAndVisit(dotExpression.base());
                m_stringBuilder.append(").length");
            } else if (is<AST::ArrayType>(type)) {
                m_stringBuilder.append('(');
                checkErrorAndVisit(dotExpression.base());
                m_stringBuilder.append(", ", downcast<AST::ArrayType>(type).numElements(), ")");
            } else {
                m_stringBuilder.append('(');
                checkErrorAndVisit(dotExpression.base());
                m_stringBuilder.append(", ", downcast<AST::NativeTypeDeclaration>(type).vectorSize(), ")");
            }
        } else {
            m_stringBuilder.append('(');
            checkErrorAndVisit(dotExpression.base());
            m_stringBuilder.append(").", elementName);
        }
    }
}

void FunctionDefinitionWriter::visit(AST::IndexExpression& indexExpression)
{
    auto& type = indexExpression.base().resolvedType().unifyNode();
    if (is<AST::ArrayReferenceType>(type)) {
        m_stringBuilder.append('(');
        checkErrorAndVisit(indexExpression.base());
        m_stringBuilder.append(").pointer[(");
        checkErrorAndVisit(indexExpression.indexExpression());
        m_stringBuilder.append(") < (");
        checkErrorAndVisit(indexExpression.base());
        m_stringBuilder.append(").length ? ");
        checkErrorAndVisit(indexExpression.indexExpression());
        m_stringBuilder.append(" : 0]");
    } else if (is<AST::ArrayType>(type)) {
        m_stringBuilder.append('(');
        checkErrorAndVisit(indexExpression.base());
        m_stringBuilder.append(").data()[(");
        checkErrorAndVisit(indexExpression.indexExpression());
        m_stringBuilder.append(") < ", downcast<AST::ArrayType>(type).numElements(), " ? ");
        checkErrorAndVisit(indexExpression.indexExpression());
        m_stringBuilder.append(" : 0]");
    } else if (is<AST::NativeTypeDeclaration>(type)) {
        auto& nativeType = downcast<AST::NativeTypeDeclaration>(type);
        unsigned size;
        if (nativeType.isMatrix())
            size = nativeType.numberOfMatrixColumns();
        else if (nativeType.isVector())
            size = nativeType.vectorSize();
        else
            RELEASE_ASSERT_NOT_REACHED();

        m_stringBuilder.append('(');
        checkErrorAndVisit(indexExpression.base());
        m_stringBuilder.append(")[(");
        checkErrorAndVisit(indexExpression.indexExpression());
        m_stringBuilder.append(" < ", size, ") ? (");
        checkErrorAndVisit(indexExpression.indexExpression());
        m_stringBuilder.append(") : 0]");
    } else
        RELEASE_ASSERT_NOT_REACHED(); 
}

void FunctionDefinitionWriter::visit(AST::VariableDeclaration& variableDeclaration)
{
    ASSERT(variableDeclaration.type());
    auto variableName = generateNextVariableName();
    auto addResult = m_variableMapping.add(&variableDeclaration, variableName);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198160 Implement qualifiers.
    if (variableDeclaration.initializer()) {
        m_stringBuilder.append(m_indent, m_typeNamer.mangledNameForType(*variableDeclaration.type()), ' ', variableName, " = ");
        checkErrorAndVisit(*variableDeclaration.initializer());
        m_stringBuilder.append(";\n");
    } else
        m_stringBuilder.append(m_indent, m_typeNamer.mangledNameForType(*variableDeclaration.type()), ' ', variableName, " = { };\n");
}

void FunctionDefinitionWriter::visit(AST::AssignmentExpression& assignmentExpression)
{
    m_stringBuilder.append('(');
    checkErrorAndVisit(assignmentExpression.left());
    m_stringBuilder.append(')');
    m_stringBuilder.append(" = (");
    checkErrorAndVisit(assignmentExpression.right());
    m_stringBuilder.append(')');
}

void FunctionDefinitionWriter::visit(AST::CallExpression& callExpression)
{
    auto iter = m_hoistedVariables.find(&callExpression);
    RELEASE_ASSERT(iter != m_hoistedVariables.end());
    auto& variables = iter->value;
    RELEASE_ASSERT(callExpression.arguments().size() <= variables.size());
    Vector<MangledVariableName> argumentNames;

    MangledVariableName resultName;
    if (!matches(callExpression.resolvedType(), m_intrinsics.voidType()))
        resultName = variables.last().name;

    m_stringBuilder.append('(');
    for (size_t i = 0; i < callExpression.arguments().size(); ++i) {
        argumentNames.append(variables[i].name);
        m_stringBuilder.append(variables[i].name, " = (");
        checkErrorAndVisit(callExpression.arguments()[i]);
        m_stringBuilder.append("), ");
    }

    if (is<AST::NativeFunctionDeclaration>(callExpression.function())) {
        inlineNativeFunction(m_stringBuilder, downcast<AST::NativeFunctionDeclaration>(callExpression.function()), argumentNames, resultName, m_typeNamer);
    } else {
        auto iterator = m_functionMapping.find(&callExpression.function());
        ASSERT(iterator != m_functionMapping.end());
        m_stringBuilder.append(iterator->value, '(');
        for (size_t i = 0; i < callExpression.arguments().size(); ++i) {
            if (i)
                m_stringBuilder.append(", ");
            m_stringBuilder.append(variables[i].name);
        }
        m_stringBuilder.append(')');
    }

    m_stringBuilder.append(')');
}

void FunctionDefinitionWriter::visit(AST::CommaExpression& commaExpression)
{
    m_stringBuilder.append('(');
    bool ranOnce = false;
    for (auto& expression : commaExpression.list()) {
        if (ranOnce)
            m_stringBuilder.append(", ");
        ranOnce = true;
        checkErrorAndVisit(expression);
    }
    m_stringBuilder.append(')');
}

void FunctionDefinitionWriter::visit(AST::DereferenceExpression& dereferenceExpression)
{
    m_stringBuilder.append("*(");
    checkErrorAndVisit(dereferenceExpression.pointer());
    m_stringBuilder.append(')');
}

void FunctionDefinitionWriter::visit(AST::LogicalExpression& logicalExpression)
{
    m_stringBuilder.append("((");
    checkErrorAndVisit(logicalExpression.left());
    m_stringBuilder.append(')');

    switch (logicalExpression.type()) {
    case AST::LogicalExpression::Type::And:
        m_stringBuilder.append(" && ");
        break;
    case AST::LogicalExpression::Type::Or:
        m_stringBuilder.append(" || ");
        break;
    }

    m_stringBuilder.append('(');
    checkErrorAndVisit(logicalExpression.right());
    m_stringBuilder.append("))");
}

void FunctionDefinitionWriter::visit(AST::LogicalNotExpression& logicalNotExpression)
{
    m_stringBuilder.append("!(");
    checkErrorAndVisit(logicalNotExpression.operand());
    m_stringBuilder.append(')');
}

void FunctionDefinitionWriter::visit(AST::MakeArrayReferenceExpression& makeArrayReferenceExpression)
{
    // FIXME: This needs to be made to work. It probably should be using the last leftValue too.
    // https://bugs.webkit.org/show_bug.cgi?id=198838
    auto mangledTypeName = m_typeNamer.mangledNameForType(makeArrayReferenceExpression.resolvedType());
    if (is<AST::PointerType>(makeArrayReferenceExpression.leftValue().resolvedType())) {
        m_stringBuilder.append(mangledTypeName, "{ ");
        checkErrorAndVisit(makeArrayReferenceExpression.leftValue());
        m_stringBuilder.append(", 1 }");
    } else if (is<AST::ArrayType>(makeArrayReferenceExpression.leftValue().resolvedType())) {
        auto& arrayType = downcast<AST::ArrayType>(makeArrayReferenceExpression.leftValue().resolvedType());
        m_stringBuilder.append(mangledTypeName, " { ");
        checkErrorAndVisit(makeArrayReferenceExpression.leftValue());
        m_stringBuilder.append(".data(), ", arrayType.numElements(), " }");
    } else {
        m_stringBuilder.append(mangledTypeName, " { &");
        checkErrorAndVisit(makeArrayReferenceExpression.leftValue());
        m_stringBuilder.append(", 1 }");
    }
}

void FunctionDefinitionWriter::visit(AST::MakePointerExpression& makePointerExpression)
{
    m_stringBuilder.append("&(");
    checkErrorAndVisit(makePointerExpression.leftValue());
    m_stringBuilder.append(')');
}

void FunctionDefinitionWriter::visit(AST::ReadModifyWriteExpression& readModifyWrite)
{
    /*
     *  1. Evaluate m_leftValue
     *  2. Assign the result to m_oldValue
     *  3. Evaluate m_newValueExpression
     *  4. Assign the result to m_newValue
     *  5. Assign the result to m_leftValue
     *  6. Evaluate m_resultExpression
     *  7. Return the result
     */

    auto iter = m_hoistedVariables.find(&readModifyWrite);
    RELEASE_ASSERT(iter != m_hoistedVariables.end());
    auto& variables = iter->value;
    RELEASE_ASSERT(variables.size() == 2);

    MangledVariableName oldValueVariable = variables[0].name;
    MangledVariableName newValueVariable = variables[1].name;

    m_variableMapping.add(&readModifyWrite.oldValue(), oldValueVariable);
    m_variableMapping.add(&readModifyWrite.newValue(), newValueVariable);

    m_stringBuilder.append('(');

    m_stringBuilder.append(oldValueVariable, " = ");
    checkErrorAndVisit(readModifyWrite.leftValue());

    m_stringBuilder.append(", ", newValueVariable, " = ");
    checkErrorAndVisit(readModifyWrite.newValueExpression());

    m_stringBuilder.append(", ");
    checkErrorAndVisit(readModifyWrite.leftValue());
    m_stringBuilder.append(" = ", newValueVariable, ", ");

    checkErrorAndVisit(readModifyWrite.resultExpression());
    m_stringBuilder.append(')');
}

void FunctionDefinitionWriter::visit(AST::TernaryExpression& ternaryExpression)
{
    m_stringBuilder.append('(');
    checkErrorAndVisit(ternaryExpression.predicate());
    m_stringBuilder.append(") ? (");
    checkErrorAndVisit(ternaryExpression.bodyExpression());
    m_stringBuilder.append(") : (");
    checkErrorAndVisit(ternaryExpression.elseExpression());
    m_stringBuilder.append(')');
}

void FunctionDefinitionWriter::visit(AST::VariableReference& variableReference)
{
    ASSERT(variableReference.variable());
    auto iterator = m_variableMapping.find(variableReference.variable());
    ASSERT(iterator != m_variableMapping.end());

    MangledVariableName variableName = iterator->value;
    m_stringBuilder.append(variableName);
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

#endif // ENABLE(WHLSL_COMPILER)
