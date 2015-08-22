/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WASMFunctionParser.h"

#if ENABLE(WEBASSEMBLY)

#include "JSWASMModule.h"
#include "WASMFunctionSyntaxChecker.h"

#define PROPAGATE_ERROR() do { if (!m_errorMessage.isNull()) return 0; } while (0)
#define FAIL_WITH_MESSAGE(errorMessage) do {  m_errorMessage = errorMessage; return 0; } while (0)
#define READ_COMPACT_INT32_OR_FAIL(result, errorMessage) do { if (!m_reader.readCompactInt32(result)) FAIL_WITH_MESSAGE(errorMessage); } while (0)
#define READ_COMPACT_UINT32_OR_FAIL(result, errorMessage) do { if (!m_reader.readCompactUInt32(result)) FAIL_WITH_MESSAGE(errorMessage); } while (0)
#define READ_OP_STATEMENT_OR_FAIL(hasImmediate, op, opWithImmediate, immediate, errorMessage) do { if (!m_reader.readOpStatement(hasImmediate, op, opWithImmediate, immediate)) FAIL_WITH_MESSAGE(errorMessage); } while (0)
#define READ_OP_EXPRESSION_I32_OR_FAIL(hasImmediate, op, opWithImmediate, immediate, errorMessage) do { if (!m_reader.readOpExpressionI32(hasImmediate, op, opWithImmediate, immediate)) FAIL_WITH_MESSAGE(errorMessage); } while (0)
#define READ_VARIABLE_TYPES_OR_FAIL(hasImmediate, variableTypes, variableTypesWithImmediate, immediate, errorMessage) do { if (!m_reader.readVariableTypes(hasImmediate, variableTypes, variableTypesWithImmediate, immediate)) FAIL_WITH_MESSAGE(errorMessage); } while (0)
#define READ_SWITCH_CASE_OR_FAIL(result, errorMessage) do { if (!m_reader.readSwitchCase(result)) FAIL_WITH_MESSAGE(errorMessage); } while (0)
#define FAIL_IF_FALSE(condition, errorMessage) do { if (!(condition)) FAIL_WITH_MESSAGE(errorMessage); } while (0)

#define UNUSED 0

namespace JSC {

bool WASMFunctionParser::checkSyntax(JSWASMModule* module, const SourceCode& source, size_t functionIndex, unsigned startOffsetInSource, unsigned& endOffsetInSource, String& errorMessage)
{
    WASMFunctionParser parser(module, source, functionIndex);
    WASMFunctionSyntaxChecker syntaxChecker;
    parser.m_reader.setOffset(startOffsetInSource);
    parser.parseFunction(syntaxChecker);
    if (!parser.m_errorMessage.isNull()) {
        errorMessage = parser.m_errorMessage;
        return false;
    }
    endOffsetInSource = parser.m_reader.offset();
    return true;
}

template <class Context>
bool WASMFunctionParser::parseFunction(Context& context)
{
    const WASMSignature& signature = m_module->signatures()[m_module->functionDeclarations()[m_functionIndex].signatureIndex];

    m_returnType = signature.returnType;

    parseLocalVariables();
    PROPAGATE_ERROR();

    const Vector<WASMType>& arguments = signature.arguments;
    for (size_t i = 0; i < arguments.size(); ++i)
        m_localTypes.append(arguments[i]);
    for (uint32_t i = 0; i < m_numberOfI32LocalVariables; ++i)
        m_localTypes.append(WASMType::I32);
    for (uint32_t i = 0; i < m_numberOfF32LocalVariables; ++i)
        m_localTypes.append(WASMType::F32);
    for (uint32_t i = 0; i < m_numberOfF64LocalVariables; ++i)
        m_localTypes.append(WASMType::F64);

    parseBlockStatement(context);
    return true;
}

bool WASMFunctionParser::parseLocalVariables()
{
    m_numberOfI32LocalVariables = 0;
    m_numberOfF32LocalVariables = 0;
    m_numberOfF64LocalVariables = 0;

    bool hasImmediate;
    WASMVariableTypes variableTypes;
    WASMVariableTypesWithImmediate variableTypesWithImmediate;
    uint8_t immediate;
    READ_VARIABLE_TYPES_OR_FAIL(hasImmediate, variableTypes, variableTypesWithImmediate, immediate, "Cannot read the types of local variables.");
    if (!hasImmediate) {
        if (static_cast<uint8_t>(variableTypes) & static_cast<uint8_t>(WASMVariableTypes::I32))
            READ_COMPACT_UINT32_OR_FAIL(m_numberOfI32LocalVariables, "Cannot read the number of int32 local variables.");
        if (static_cast<uint8_t>(variableTypes) & static_cast<uint8_t>(WASMVariableTypes::F32))
            READ_COMPACT_UINT32_OR_FAIL(m_numberOfF32LocalVariables, "Cannot read the number of float32 local variables.");
        if (static_cast<uint8_t>(variableTypes) & static_cast<uint8_t>(WASMVariableTypes::F64))
            READ_COMPACT_UINT32_OR_FAIL(m_numberOfF64LocalVariables, "Cannot read the number of float64 local variables.");
    } else
        m_numberOfI32LocalVariables = immediate;
    return true;
}

template <class Context>
ContextStatement WASMFunctionParser::parseStatement(Context& context)
{
    bool hasImmediate;
    WASMOpStatement op;
    WASMOpStatementWithImmediate opWithImmediate;
    uint8_t immediate;
    READ_OP_STATEMENT_OR_FAIL(hasImmediate, op, opWithImmediate, immediate, "Cannot read the statement opcode.");
    if (!hasImmediate) {
        switch (op) {
        case WASMOpStatement::SetLocal:
            parseSetLocalStatement(context);
            break;
        case WASMOpStatement::Return:
            parseReturnStatement(context);
            break;
        case WASMOpStatement::Block:
            parseBlockStatement(context);
            break;
        case WASMOpStatement::If:
            parseIfStatement(context);
            break;
        case WASMOpStatement::IfElse:
            parseIfElseStatement(context);
            break;
        case WASMOpStatement::While:
            parseWhileStatement(context);
            break;
        case WASMOpStatement::Do:
            parseDoStatement(context);
            break;
        case WASMOpStatement::Label:
            parseLabelStatement(context);
            break;
        case WASMOpStatement::Break:
            parseBreakStatement(context);
            break;
        case WASMOpStatement::BreakLabel:
            parseBreakLabelStatement(context);
            break;
        case WASMOpStatement::Continue:
            parseContinueStatement(context);
            break;
        case WASMOpStatement::ContinueLabel:
            parseContinueLabelStatement(context);
            break;
        case WASMOpStatement::Switch:
            parseSwitchStatement(context);
            break;
        case WASMOpStatement::SetGlobal:
        case WASMOpStatement::I32Store8:
        case WASMOpStatement::I32StoreWithOffset8:
        case WASMOpStatement::I32Store16:
        case WASMOpStatement::I32StoreWithOffset16:
        case WASMOpStatement::I32Store32:
        case WASMOpStatement::I32StoreWithOffset32:
        case WASMOpStatement::F32Store:
        case WASMOpStatement::F32StoreWithOffset:
        case WASMOpStatement::F64Store:
        case WASMOpStatement::F64StoreWithOffset:
        case WASMOpStatement::CallInternal:
        case WASMOpStatement::CallIndirect:
        case WASMOpStatement::CallImport:
            // FIXME: Implement these instructions.
            FAIL_WITH_MESSAGE("Unsupported instruction.");
        default:
            ASSERT_NOT_REACHED();
        }
    } else {
        switch (opWithImmediate) {
        case WASMOpStatementWithImmediate::SetLocal:
            parseSetLocalStatement(context, immediate);
            break;
        case WASMOpStatementWithImmediate::SetGlobal:
            // FIXME: Implement this instruction.
            FAIL_WITH_MESSAGE("Unsupported instruction.");
        default:
            ASSERT_NOT_REACHED();
        }
    }
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseSetLocalStatement(Context& context, uint32_t localIndex)
{
    FAIL_IF_FALSE(localIndex < m_localTypes.size(), "The local variable index is incorrect.");
    WASMType type = m_localTypes[localIndex];
    parseExpression(context, WASMExpressionType(type));
    // FIXME: Implement this instruction.
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseSetLocalStatement(Context& context)
{
    uint32_t localIndex;
    READ_COMPACT_UINT32_OR_FAIL(localIndex, "Cannot read the local index.");
    return parseSetLocalStatement(context, localIndex);
}

template <class Context>
ContextStatement WASMFunctionParser::parseReturnStatement(Context& context)
{
    if (m_returnType != WASMExpressionType::Void)
        parseExpression(context, m_returnType);
    // FIXME: Implement this instruction.
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseBlockStatement(Context& context)
{
    uint32_t numberOfStatements;
    READ_COMPACT_UINT32_OR_FAIL(numberOfStatements, "Cannot read the number of statements.");
    for (uint32_t i = 0; i < numberOfStatements; ++i) {
        parseStatement(context);
        PROPAGATE_ERROR();
    }
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseIfStatement(Context& context)
{
    parseExpressionI32(context);
    PROPAGATE_ERROR();
    parseStatement(context);
    // FIXME: Implement this instruction.
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseIfElseStatement(Context& context)
{
    parseExpressionI32(context);
    PROPAGATE_ERROR();
    parseStatement(context);
    PROPAGATE_ERROR();
    parseStatement(context);
    // FIXME: Implement this instruction.
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseWhileStatement(Context& context)
{
    parseExpressionI32(context);
    PROPAGATE_ERROR();

    m_breakScopeDepth++;
    m_continueScopeDepth++;
    parseStatement(context);
    PROPAGATE_ERROR();
    m_continueScopeDepth--;
    m_breakScopeDepth--;
    // FIXME: Implement this instruction.
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseDoStatement(Context& context)
{
    m_breakScopeDepth++;
    m_continueScopeDepth++;
    parseStatement(context);
    PROPAGATE_ERROR();
    m_continueScopeDepth--;
    m_breakScopeDepth--;

    parseExpressionI32(context);
    // FIXME: Implement this instruction.
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseLabelStatement(Context& context)
{
    m_labelDepth++;
    parseStatement(context);
    PROPAGATE_ERROR();
    m_labelDepth--;
    // FIXME: Implement this instruction.
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseBreakStatement(Context&)
{
    FAIL_IF_FALSE(m_breakScopeDepth, "'break' is only valid inside a switch or loop statement.");
    // FIXME: Implement this instruction.
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseBreakLabelStatement(Context&)
{
    uint32_t labelIndex;
    READ_COMPACT_UINT32_OR_FAIL(labelIndex, "Cannot read the label index.");
    FAIL_IF_FALSE(labelIndex < m_labelDepth, "The label index is incorrect.");
    // FIXME: Implement this instruction.
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseContinueStatement(Context&)
{
    FAIL_IF_FALSE(m_continueScopeDepth, "'continue' is only valid inside a loop statement.");
    // FIXME: Implement this instruction.
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseContinueLabelStatement(Context&)
{
    uint32_t labelIndex;
    READ_COMPACT_UINT32_OR_FAIL(labelIndex, "Cannot read the label index.");
    FAIL_IF_FALSE(labelIndex < m_labelDepth, "The label index is incorrect.");
    // FIXME: Implement this instruction.
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseSwitchStatement(Context& context)
{
    uint32_t numberOfCases;
    READ_COMPACT_UINT32_OR_FAIL(numberOfCases, "Cannot read the number of cases.");
    parseExpressionI32(context);
    PROPAGATE_ERROR();

    m_breakScopeDepth++;
    for (uint32_t i = 0; i < numberOfCases; ++i) {
        WASMSwitchCase switchCase;
        READ_SWITCH_CASE_OR_FAIL(switchCase, "Cannot read the switch case.");
        switch (switchCase) {
        case WASMSwitchCase::CaseWithNoStatements:
        case WASMSwitchCase::CaseWithStatement:
        case WASMSwitchCase::CaseWithBlockStatement: {
            uint32_t value;
            READ_COMPACT_INT32_OR_FAIL(value, "Cannot read the value of the switch case.");
            if (switchCase == WASMSwitchCase::CaseWithStatement) {
                parseStatement(context);
                PROPAGATE_ERROR();
            } else if (switchCase == WASMSwitchCase::CaseWithBlockStatement) {
                parseBlockStatement(context);
                PROPAGATE_ERROR();
            }
            break;
        }
        case WASMSwitchCase::DefaultWithNoStatements:
        case WASMSwitchCase::DefaultWithStatement:
        case WASMSwitchCase::DefaultWithBlockStatement: {
            FAIL_IF_FALSE(i == numberOfCases - 1, "The default case must be the last case.");
            if (switchCase == WASMSwitchCase::DefaultWithStatement) {
                parseStatement(context);
                PROPAGATE_ERROR();
            } else if (switchCase == WASMSwitchCase::DefaultWithBlockStatement) {
                parseBlockStatement(context);
                PROPAGATE_ERROR();
            }
            break;
        }
        default:
            ASSERT_NOT_REACHED();
        }
    }
    m_breakScopeDepth--;
    // FIXME: Implement this instruction.
    return UNUSED;
}

template <class Context>
ContextExpression WASMFunctionParser::parseExpression(Context& context, WASMExpressionType expressionType)
{
    switch (expressionType) {
    case WASMExpressionType::I32:
        return parseExpressionI32(context);
    case WASMExpressionType::F32:
    case WASMExpressionType::F64:
    case WASMExpressionType::Void:
        // FIXME: Implement these instructions.
        FAIL_WITH_MESSAGE("Unsupported instruction.");
    default:
        ASSERT_NOT_REACHED();
    }
    return 0;
}

template <class Context>
ContextExpression WASMFunctionParser::parseExpressionI32(Context& context)
{
    bool hasImmediate;
    WASMOpExpressionI32 op;
    WASMOpExpressionI32WithImmediate opWithImmediate;
    uint8_t immediate;
    READ_OP_EXPRESSION_I32_OR_FAIL(hasImmediate, op, opWithImmediate, immediate, "Cannot read the int32 expression opcode.");
    if (!hasImmediate) {
        switch (op) {
        case WASMOpExpressionI32::Immediate:
            return parseImmediateExpressionI32(context);
        case WASMOpExpressionI32::ConstantPoolIndex:
        case WASMOpExpressionI32::GetLocal:
        case WASMOpExpressionI32::GetGlobal:
        case WASMOpExpressionI32::SetLocal:
        case WASMOpExpressionI32::SetGlobal:
        case WASMOpExpressionI32::SLoad8:
        case WASMOpExpressionI32::SLoadWithOffset8:
        case WASMOpExpressionI32::ULoad8:
        case WASMOpExpressionI32::ULoadWithOffset8:
        case WASMOpExpressionI32::SLoad16:
        case WASMOpExpressionI32::SLoadWithOffset16:
        case WASMOpExpressionI32::ULoad16:
        case WASMOpExpressionI32::ULoadWithOffset16:
        case WASMOpExpressionI32::Load32:
        case WASMOpExpressionI32::LoadWithOffset32:
        case WASMOpExpressionI32::Store8:
        case WASMOpExpressionI32::StoreWithOffset8:
        case WASMOpExpressionI32::Store16:
        case WASMOpExpressionI32::StoreWithOffset16:
        case WASMOpExpressionI32::Store32:
        case WASMOpExpressionI32::StoreWithOffset32:
        case WASMOpExpressionI32::CallInternal:
        case WASMOpExpressionI32::CallIndirect:
        case WASMOpExpressionI32::CallImport:
        case WASMOpExpressionI32::Conditional:
        case WASMOpExpressionI32::Comma:
        case WASMOpExpressionI32::FromF32:
        case WASMOpExpressionI32::FromF64:
        case WASMOpExpressionI32::Negate:
        case WASMOpExpressionI32::Add:
        case WASMOpExpressionI32::Sub:
        case WASMOpExpressionI32::Mul:
        case WASMOpExpressionI32::SDiv:
        case WASMOpExpressionI32::UDiv:
        case WASMOpExpressionI32::SMod:
        case WASMOpExpressionI32::UMod:
        case WASMOpExpressionI32::BitNot:
        case WASMOpExpressionI32::BitOr:
        case WASMOpExpressionI32::BitAnd:
        case WASMOpExpressionI32::BitXor:
        case WASMOpExpressionI32::LeftShift:
        case WASMOpExpressionI32::ArithmeticRightShift:
        case WASMOpExpressionI32::LogicalRightShift:
        case WASMOpExpressionI32::CountLeadingZeros:
        case WASMOpExpressionI32::LogicalNot:
        case WASMOpExpressionI32::EqualI32:
        case WASMOpExpressionI32::EqualF32:
        case WASMOpExpressionI32::EqualF64:
        case WASMOpExpressionI32::NotEqualI32:
        case WASMOpExpressionI32::NotEqualF32:
        case WASMOpExpressionI32::NotEqualF64:
        case WASMOpExpressionI32::SLessThanI32:
        case WASMOpExpressionI32::ULessThanI32:
        case WASMOpExpressionI32::LessThanF32:
        case WASMOpExpressionI32::LessThanF64:
        case WASMOpExpressionI32::SLessThanOrEqualI32:
        case WASMOpExpressionI32::ULessThanOrEqualI32:
        case WASMOpExpressionI32::LessThanOrEqualF32:
        case WASMOpExpressionI32::LessThanOrEqualF64:
        case WASMOpExpressionI32::SGreaterThanI32:
        case WASMOpExpressionI32::UGreaterThanI32:
        case WASMOpExpressionI32::GreaterThanF32:
        case WASMOpExpressionI32::GreaterThanF64:
        case WASMOpExpressionI32::SGreaterThanOrEqualI32:
        case WASMOpExpressionI32::UGreaterThanOrEqualI32:
        case WASMOpExpressionI32::GreaterThanOrEqualF32:
        case WASMOpExpressionI32::GreaterThanOrEqualF64:
        case WASMOpExpressionI32::SMin:
        case WASMOpExpressionI32::UMin:
        case WASMOpExpressionI32::SMax:
        case WASMOpExpressionI32::UMax:
        case WASMOpExpressionI32::Abs:
            // FIXME: Implement these instructions.
            FAIL_WITH_MESSAGE("Unsupported instruction.");
        default:
            ASSERT_NOT_REACHED();
        }
    } else {
        switch (opWithImmediate) {
        case WASMOpExpressionI32WithImmediate::Immediate:
            return parseImmediateExpressionI32(context, immediate);
        case WASMOpExpressionI32WithImmediate::ConstantPoolIndex:
        case WASMOpExpressionI32WithImmediate::GetLocal:
            // FIXME: Implement these instructions.
            FAIL_WITH_MESSAGE("Unsupported instruction.");
        default:
            ASSERT_NOT_REACHED();
        }
    }
    return 0;
}

template <class Context>
ContextExpression WASMFunctionParser::parseImmediateExpressionI32(Context&, uint32_t)
{
    // FIXME: Implement this instruction.
    return UNUSED;
}

template <class Context>
ContextExpression WASMFunctionParser::parseImmediateExpressionI32(Context& context)
{
    uint32_t immediate;
    READ_COMPACT_UINT32_OR_FAIL(immediate, "Cannot read the immediate.");
    return parseImmediateExpressionI32(context, immediate);
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
