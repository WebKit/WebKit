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
    void visit(AST::FunctionDeclaration& functionDeclaration) override
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

private:
    TypeNamer& m_typeNamer;
    HashMap<AST::FunctionDeclaration*, String>& m_functionMapping;
    StringBuilder m_stringBuilder;
};

class FunctionDefinitionWriter : public Visitor {
public:
    FunctionDefinitionWriter(Intrinsics& intrinsics, TypeNamer& typeNamer, HashMap<AST::FunctionDeclaration*, String>& functionMapping)
        : m_intrinsics(intrinsics)
        , m_typeNamer(typeNamer)
        , m_functionMapping(functionMapping)
    {
    }

    virtual ~FunctionDefinitionWriter() = default;

    String toString() { return m_stringBuilder.toString(); }

    void visit(AST::NativeFunctionDeclaration& nativeFunctionDeclaration) override
    {
        auto iterator = m_functionMapping.find(&nativeFunctionDeclaration);
        ASSERT(iterator != m_functionMapping.end());
        m_stringBuilder.append(writeNativeFunction(nativeFunctionDeclaration, iterator->value, m_typeNamer));
    }

    void visit(AST::FunctionDefinition& functionDefinition) override
    {
        auto iterator = m_functionMapping.find(&functionDefinition);
        ASSERT(iterator != m_functionMapping.end());
        if (functionDefinition.entryPointType()) {
            m_entryPointScaffolding = EntryPointScaffolding(functionDefinition, m_intrinsics);
            m_stringBuilder.append(m_entryPointScaffolding->helperTypes());
            m_stringBuilder.append('\n');
            m_stringBuilder.append(makeString(m_entryPointScaffolding->signature(), " {"));
            m_stringBuilder.append(m_entryPointScaffolding->unpack());
            checkErrorAndVisit(functionDefinition.block());
            ASSERT(m_stack.isEmpty());
            m_stringBuilder.append("}\n");
        } else {
            m_entryPointScaffolding = WTF::nullopt;
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

private:
    void visit(AST::FunctionDeclaration&) override
    {
        ASSERT_NOT_REACHED();
    }

    void visit(AST::Statement& statement) override
    {
        Visitor::visit(statement);
    }

    void visit(AST::Block& block) override
    {
        m_stringBuilder.append("{\n");
        for (auto& statement : block.statements())
            checkErrorAndVisit(statement);
        m_stringBuilder.append("}\n");
    }

    void visit(AST::Break&) override
    {
        m_stringBuilder.append("break;\n");
    }

    void visit(AST::Continue&) override
    {
        // FIXME: Figure out which loop we're in, and run the increment code
        CRASH();
    }

    void visit(AST::DoWhileLoop& doWhileLoop) override
    {
        m_stringBuilder.append("do {\n");
        checkErrorAndVisit(doWhileLoop.body());
        checkErrorAndVisit(doWhileLoop.conditional());
        m_stringBuilder.append(makeString("if (!", m_stack.takeLast(), ") break;\n"));
        m_stringBuilder.append(makeString("} while(true);\n"));
    }

    void visit(AST::EffectfulExpressionStatement& effectfulExpressionStatement) override
    {
        checkErrorAndVisit(effectfulExpressionStatement.effectfulExpression());
        m_stack.takeLast(); // The statement is already effectful, so we don't need to do anything with the result.
    }

    void visit(AST::Fallthrough&) override
    {
        m_stringBuilder.append("[[clang::fallthrough]];\n"); // FIXME: Make sure this is okay. Alternatively, we could do nothing and just return here instead.
    }

    void visit(AST::ForLoop& forLoop) override
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

    void visit(AST::IfStatement& ifStatement) override
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

    void visit(AST::Return& returnStatement) override
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

    void visit(AST::SwitchStatement& switchStatement) override
    {
        checkErrorAndVisit(switchStatement.value());

        m_stringBuilder.append(makeString("switch (", m_stack.takeLast(), ") {"));
        for (auto& switchCase : switchStatement.switchCases())
            checkErrorAndVisit(switchCase);
        m_stringBuilder.append("}\n");
    }

    void visit(AST::SwitchCase& switchCase) override
    {
        if (switchCase.value())
            m_stringBuilder.append(makeString("case ", constantExpressionString(*switchCase.value()), ":\n"));
        else
            m_stringBuilder.append("default:\n");
        checkErrorAndVisit(switchCase.block());
        // FIXME: Figure out whether we need to break or fallthrough.
        CRASH();
    }

    void visit(AST::Trap&) override
    {
        // FIXME: Implement this
        CRASH();
    }

    void visit(AST::VariableDeclarationsStatement& variableDeclarationsStatement) override
    {
        Visitor::visit(variableDeclarationsStatement);
    }

    void visit(AST::WhileLoop& whileLoop) override
    {
        m_stringBuilder.append(makeString("while (true) {\n"));
        checkErrorAndVisit(whileLoop.conditional());
        m_stringBuilder.append(makeString("if (!", m_stack.takeLast(), ") break;\n"));
        checkErrorAndVisit(whileLoop.body());
        m_stringBuilder.append("}\n");
    }

    void visit(AST::IntegerLiteral& integerLiteral) override
    {
        ASSERT(integerLiteral.resolvedType());
        auto variableName = generateNextVariableName();
        auto mangledTypeName = m_typeNamer.mangledNameForType(*integerLiteral.resolvedType());
        m_stringBuilder.append(makeString(mangledTypeName, ' ', variableName, " = static_cast<", mangledTypeName, ">(", integerLiteral.value(), ");\n"));
        m_stack.append(variableName);
    }

    void visit(AST::UnsignedIntegerLiteral& unsignedIntegerLiteral) override
    {
        ASSERT(unsignedIntegerLiteral.resolvedType());
        auto variableName = generateNextVariableName();
        auto mangledTypeName = m_typeNamer.mangledNameForType(*unsignedIntegerLiteral.resolvedType());
        m_stringBuilder.append(makeString(mangledTypeName, ' ', variableName, " = static_cast<", mangledTypeName, ">(", unsignedIntegerLiteral.value(), ");\n"));
        m_stack.append(variableName);
    }

    void visit(AST::FloatLiteral& floatLiteral) override
    {
        ASSERT(floatLiteral.resolvedType());
        auto variableName = generateNextVariableName();
        auto mangledTypeName = m_typeNamer.mangledNameForType(*floatLiteral.resolvedType());
        m_stringBuilder.append(makeString(mangledTypeName, ' ', variableName, " = static_cast<", mangledTypeName, ">(", floatLiteral.value(), ");\n"));
        m_stack.append(variableName);
    }

    void visit(AST::NullLiteral& nullLiteral) override
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

    void visit(AST::BooleanLiteral& booleanLiteral) override
    {
        ASSERT(booleanLiteral.resolvedType());
        auto variableName = generateNextVariableName();
        auto mangledTypeName = m_typeNamer.mangledNameForType(*booleanLiteral.resolvedType());
        m_stringBuilder.append(makeString(mangledTypeName, ' ', variableName, " = static_cast<", mangledTypeName, ">(", booleanLiteral.value() ? "true" : "false", ");\n"));
        m_stack.append(variableName);
    }

    void visit(AST::EnumerationMemberLiteral& enumerationMemberLiteral) override
    {
        ASSERT(enumerationMemberLiteral.resolvedType());
        ASSERT(enumerationMemberLiteral.enumerationDefinition());
        ASSERT(enumerationMemberLiteral.enumerationDefinition());
        auto variableName = generateNextVariableName();
        auto mangledTypeName = m_typeNamer.mangledNameForType(*enumerationMemberLiteral.resolvedType());
        m_stringBuilder.append(makeString(mangledTypeName, ' ', variableName, " = ", mangledTypeName, '.', m_typeNamer.mangledNameForEnumerationMember(*enumerationMemberLiteral.enumerationMember()), ";\n"));
        m_stack.append(variableName);
    }

    void visit(AST::Expression& expression) override
    {
        Visitor::visit(expression);
    }

    void visit(AST::DotExpression&) override
    {
        // This should be lowered already.
        ASSERT_NOT_REACHED();
    }

    void visit(AST::IndexExpression&) override
    {
        // This should be lowered already.
        ASSERT_NOT_REACHED();
    }

    void visit(AST::PropertyAccessExpression&) override
    {
        ASSERT_NOT_REACHED();
    }

    void visit(AST::VariableDeclaration& variableDeclaration) override
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

    void visit(AST::AssignmentExpression& assignmentExpression) override
    {
        checkErrorAndVisit(assignmentExpression.left());
        auto leftName = m_stack.takeLast();
        checkErrorAndVisit(assignmentExpression.right());
        auto rightName = m_stack.takeLast();
        m_stringBuilder.append(makeString(leftName, " = ", rightName, ";\n"));
    }

    void visit(AST::CallExpression& callExpression) override
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

    void visit(AST::CommaExpression& commaExpression) override
    {
        String result;
        for (auto& expression : commaExpression.list()) {
            checkErrorAndVisit(expression);
            result = m_stack.takeLast();
        }
        m_stack.append(result);
    }

    void visit(AST::DereferenceExpression& dereferenceExpression) override
    {
        checkErrorAndVisit(dereferenceExpression.pointer());
        auto right = m_stack.takeLast();
        ASSERT(dereferenceExpression.resolvedType());
        auto variableName = generateNextVariableName();
        m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(*dereferenceExpression.resolvedType()), ' ', variableName, " = *", right, ";\n"));
        m_stack.append(variableName);
    }

    void visit(AST::LogicalExpression& logicalExpression) override
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

    void visit(AST::LogicalNotExpression& logicalNotExpression) override
    {
        checkErrorAndVisit(logicalNotExpression.operand());
        auto operand = m_stack.takeLast();
        ASSERT(logicalNotExpression.resolvedType());
        auto variableName = generateNextVariableName();
        m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(*logicalNotExpression.resolvedType()), ' ', variableName, " = !", operand, ";\n"));
        m_stack.append(variableName);
    }

    void visit(AST::MakeArrayReferenceExpression& makeArrayReferenceExpression) override
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

    void visit(AST::MakePointerExpression& makePointerExpression) override
    {
        checkErrorAndVisit(makePointerExpression.lValue());
        auto lValue = m_stack.takeLast();
        ASSERT(makePointerExpression.resolvedType());
        auto variableName = generateNextVariableName();
        m_stringBuilder.append(makeString(m_typeNamer.mangledNameForType(*makePointerExpression.resolvedType()), ' ', variableName, " = &", lValue, ";\n"));
        m_stack.append(variableName);
    }

    void visit(AST::ReadModifyWriteExpression&) override
    {
        // This should be lowered already.
        ASSERT_NOT_REACHED();
    }

    void visit(AST::TernaryExpression& ternaryExpression) override
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

    void visit(AST::VariableReference& variableReference) override
    {
        ASSERT(variableReference.variable());
        auto iterator = m_variableMapping.find(variableReference.variable());
        ASSERT(iterator != m_variableMapping.end());
        m_stack.append(iterator->value);
    }

    String constantExpressionString(AST::ConstantExpression& constantExpression)
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

    String generateNextVariableName()
    {
        return makeString("variable", m_variableCount++);
    }

private:
    Intrinsics& m_intrinsics;
    TypeNamer& m_typeNamer;
    HashMap<AST::FunctionDeclaration*, String>& m_functionMapping;
    HashMap<AST::VariableDeclaration*, String> m_variableMapping;
    StringBuilder m_stringBuilder;
    Vector<String> m_stack;
    Optional<EntryPointScaffolding> m_entryPointScaffolding;
    unsigned m_variableCount { 0 };
};

String metalFunctions(Program& program, TypeNamer& typeNamer)
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

    {
        FunctionDefinitionWriter functionDefinitionWriter(program.intrinsics(), typeNamer, functionMapping);
        for (auto& nativeFunctionDeclaration : program.nativeFunctionDeclarations())
            functionDefinitionWriter.visit(nativeFunctionDeclaration);
        for (auto& functionDefinition : program.functionDefinitions())
            functionDefinitionWriter.visit(functionDefinition);
        stringBuilder.append(functionDefinitionWriter.toString());
    }

    return stringBuilder.toString();
}

} // namespace Metal

} // namespace WHLSL

} // namespace WebCore

#endif
