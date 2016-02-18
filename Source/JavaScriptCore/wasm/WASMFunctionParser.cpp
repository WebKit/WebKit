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

#include "JSCJSValueInlines.h"
#include "JSWASMModule.h"
#include "WASMFunctionCompiler.h"
#include "WASMFunctionB3IRGenerator.h"
#include "WASMFunctionSyntaxChecker.h"

#define PROPAGATE_ERROR() do { if (!m_errorMessage.isNull()) return 0; } while (0)
#define FAIL_WITH_MESSAGE(errorMessage) do {  m_errorMessage = errorMessage; return 0; } while (0)
#define READ_FLOAT_OR_FAIL(result, errorMessage) do { if (!m_reader.readFloat(result)) FAIL_WITH_MESSAGE(errorMessage); } while (0)
#define READ_DOUBLE_OR_FAIL(result, errorMessage) do { if (!m_reader.readDouble(result)) FAIL_WITH_MESSAGE(errorMessage); } while (0)
#define READ_COMPACT_INT32_OR_FAIL(result, errorMessage) do { if (!m_reader.readCompactInt32(result)) FAIL_WITH_MESSAGE(errorMessage); } while (0)
#define READ_COMPACT_UINT32_OR_FAIL(result, errorMessage) do { if (!m_reader.readCompactUInt32(result)) FAIL_WITH_MESSAGE(errorMessage); } while (0)
#define READ_EXPRESSION_TYPE_OR_FAIL(result, errorMessage) do { if (!m_reader.readExpressionType(result)) FAIL_WITH_MESSAGE(errorMessage); } while (0)
#define READ_OP_STATEMENT_OR_FAIL(hasImmediate, op, opWithImmediate, immediate, errorMessage) do { if (!m_reader.readOpStatement(hasImmediate, op, opWithImmediate, immediate)) FAIL_WITH_MESSAGE(errorMessage); } while (0)
#define READ_OP_EXPRESSION_I32_OR_FAIL(hasImmediate, op, opWithImmediate, immediate, errorMessage) do { if (!m_reader.readOpExpressionI32(hasImmediate, op, opWithImmediate, immediate)) FAIL_WITH_MESSAGE(errorMessage); } while (0)
#define READ_OP_EXPRESSION_F32_OR_FAIL(hasImmediate, op, opWithImmediate, immediate, errorMessage) do { if (!m_reader.readOpExpressionF32(hasImmediate, op, opWithImmediate, immediate)) FAIL_WITH_MESSAGE(errorMessage); } while (0)
#define READ_OP_EXPRESSION_F64_OR_FAIL(hasImmediate, op, opWithImmediate, immediate, errorMessage) do { if (!m_reader.readOpExpressionF64(hasImmediate, op, opWithImmediate, immediate)) FAIL_WITH_MESSAGE(errorMessage); } while (0)
#define READ_OP_EXPRESSION_VOID_OR_FAIL(op, errorMessage) do { if (!m_reader.readOpExpressionVoid(op)) FAIL_WITH_MESSAGE(errorMessage); } while (0)
#define READ_VARIABLE_TYPES_OR_FAIL(hasImmediate, variableTypes, variableTypesWithImmediate, immediate, errorMessage) do { if (!m_reader.readVariableTypes(hasImmediate, variableTypes, variableTypesWithImmediate, immediate)) FAIL_WITH_MESSAGE(errorMessage); } while (0)
#define READ_SWITCH_CASE_OR_FAIL(result, errorMessage) do { if (!m_reader.readSwitchCase(result)) FAIL_WITH_MESSAGE(errorMessage); } while (0)
#define FAIL_IF_FALSE(condition, errorMessage) do { if (!(condition)) FAIL_WITH_MESSAGE(errorMessage); } while (0)

#define UNUSED 0

namespace JSC {

static String nameOfType(WASMType type)
{
    switch (type) {
    case WASMType::I32:
        return "int32";
    case WASMType::F32:
        return "float32";
    case WASMType::F64:
        return "float64";
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

bool WASMFunctionParser::checkSyntax(JSWASMModule* module, const SourceCode& source, size_t functionIndex, unsigned startOffsetInSource, unsigned& endOffsetInSource, unsigned& stackHeight, String& errorMessage)
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
    stackHeight = syntaxChecker.stackHeight();
    return true;
}

void WASMFunctionParser::compile(VM& vm, CodeBlock* codeBlock, JSWASMModule* module, const SourceCode& source, size_t functionIndex)
{
    WASMFunctionParser parser(module, source, functionIndex);
    WASMFunctionCompiler compiler(vm, codeBlock, module, module->functionStackHeights()[functionIndex]);
    parser.m_reader.setOffset(module->functionStartOffsetsInSource()[functionIndex]);
    parser.parseFunction(compiler);
    ASSERT(parser.m_errorMessage.isNull());
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

    context.startFunction(arguments, m_numberOfI32LocalVariables, m_numberOfF32LocalVariables, m_numberOfF64LocalVariables);

    parseBlockStatement(context);
    PROPAGATE_ERROR();

    context.endFunction();
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
            parseSetLocal(context, WASMOpKind::Statement, WASMExpressionType::Void);
            break;
        case WASMOpStatement::SetGlobal:
            parseSetGlobal(context, WASMOpKind::Statement, WASMExpressionType::Void);
            break;
        case WASMOpStatement::I32Store8:
            parseStore(context, WASMOpKind::Statement, WASMExpressionType::I32, WASMMemoryType::I8, MemoryAccessOffsetMode::NoOffset);
            break;
        case WASMOpStatement::I32StoreWithOffset8:
            parseStore(context, WASMOpKind::Statement, WASMExpressionType::I32, WASMMemoryType::I8, MemoryAccessOffsetMode::WithOffset);
            break;
        case WASMOpStatement::I32Store16:
            parseStore(context, WASMOpKind::Statement, WASMExpressionType::I32, WASMMemoryType::I16, MemoryAccessOffsetMode::NoOffset);
            break;
        case WASMOpStatement::I32StoreWithOffset16:
            parseStore(context, WASMOpKind::Statement, WASMExpressionType::I32, WASMMemoryType::I16, MemoryAccessOffsetMode::WithOffset);
            break;
        case WASMOpStatement::I32Store32:
            parseStore(context, WASMOpKind::Statement, WASMExpressionType::I32, WASMMemoryType::I32, MemoryAccessOffsetMode::NoOffset);
            break;
        case WASMOpStatement::I32StoreWithOffset32:
            parseStore(context, WASMOpKind::Statement, WASMExpressionType::I32, WASMMemoryType::I32, MemoryAccessOffsetMode::WithOffset);
            break;
        case WASMOpStatement::F32Store:
            parseStore(context, WASMOpKind::Statement, WASMExpressionType::F32, WASMMemoryType::F32, MemoryAccessOffsetMode::NoOffset);
            break;
        case WASMOpStatement::F32StoreWithOffset:
            parseStore(context, WASMOpKind::Statement, WASMExpressionType::F32, WASMMemoryType::F32, MemoryAccessOffsetMode::WithOffset);
            break;
        case WASMOpStatement::F64Store:
            parseStore(context, WASMOpKind::Statement, WASMExpressionType::F64, WASMMemoryType::F64, MemoryAccessOffsetMode::NoOffset);
            break;
        case WASMOpStatement::F64StoreWithOffset:
            parseStore(context, WASMOpKind::Statement, WASMExpressionType::F64, WASMMemoryType::F64, MemoryAccessOffsetMode::WithOffset);
            break;
        case WASMOpStatement::CallInternal:
            parseCallInternal(context, WASMOpKind::Statement, WASMExpressionType::Void);
            break;
        case WASMOpStatement::CallIndirect:
            parseCallIndirect(context, WASMOpKind::Statement, WASMExpressionType::Void);
            break;
        case WASMOpStatement::CallImport:
            parseCallImport(context, WASMOpKind::Statement, WASMExpressionType::Void);
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
        default:
            ASSERT_NOT_REACHED();
        }
    } else {
        switch (opWithImmediate) {
        case WASMOpStatementWithImmediate::SetLocal:
            parseSetLocal(context, WASMOpKind::Statement, WASMExpressionType::Void, immediate);
            break;
        case WASMOpStatementWithImmediate::SetGlobal:
            parseSetGlobal(context, WASMOpKind::Statement, WASMExpressionType::Void, immediate);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseReturnStatement(Context& context)
{
    ContextExpression expression = 0;
    if (m_returnType != WASMExpressionType::Void) {
        expression = parseExpression(context, m_returnType);
        PROPAGATE_ERROR();
    }
    context.buildReturn(expression, m_returnType);
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
    ContextJumpTarget end;

    ContextExpression expression = parseExpressionI32(context);
    PROPAGATE_ERROR();

    context.jumpToTargetIf(Context::JumpCondition::Zero, expression, end);

    parseStatement(context);
    PROPAGATE_ERROR();

    context.linkTarget(end);
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseIfElseStatement(Context& context)
{
    ContextJumpTarget elseTarget;
    ContextJumpTarget end;

    ContextExpression expression = parseExpressionI32(context);
    PROPAGATE_ERROR();

    context.jumpToTargetIf(Context::JumpCondition::Zero, expression, elseTarget);

    parseStatement(context);
    PROPAGATE_ERROR();

    context.jumpToTarget(end);
    context.linkTarget(elseTarget);

    parseStatement(context);
    PROPAGATE_ERROR();

    context.linkTarget(end);
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseWhileStatement(Context& context)
{
    context.startLoop();
    context.linkTarget(context.continueTarget());

    ContextExpression expression = parseExpressionI32(context);
    PROPAGATE_ERROR();

    context.jumpToTargetIf(Context::JumpCondition::Zero, expression, context.breakTarget());

    m_breakScopeDepth++;
    m_continueScopeDepth++;
    parseStatement(context);
    PROPAGATE_ERROR();
    m_continueScopeDepth--;
    m_breakScopeDepth--;

    context.jumpToTarget(context.continueTarget());

    context.linkTarget(context.breakTarget());
    context.endLoop();
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseDoStatement(Context& context)
{
    context.startLoop();

    ContextJumpTarget topOfLoop;
    context.linkTarget(topOfLoop);

    m_breakScopeDepth++;
    m_continueScopeDepth++;
    parseStatement(context);
    PROPAGATE_ERROR();
    m_continueScopeDepth--;
    m_breakScopeDepth--;

    context.linkTarget(context.continueTarget());

    ContextExpression expression = parseExpressionI32(context);
    PROPAGATE_ERROR();

    context.jumpToTargetIf(Context::JumpCondition::NonZero, expression, topOfLoop);

    context.linkTarget(context.breakTarget());
    context.endLoop();
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseLabelStatement(Context& context)
{
    context.startLabel();
    m_labelDepth++;
    parseStatement(context);
    PROPAGATE_ERROR();
    m_labelDepth--;
    context.endLabel();
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseBreakStatement(Context& context)
{
    FAIL_IF_FALSE(m_breakScopeDepth, "'break' is only valid inside a switch or loop statement.");
    context.jumpToTarget(context.breakTarget());
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseBreakLabelStatement(Context& context)
{
    uint32_t labelIndex;
    READ_COMPACT_UINT32_OR_FAIL(labelIndex, "Cannot read the label index.");
    FAIL_IF_FALSE(labelIndex < m_labelDepth, "The label index is incorrect.");
    context.jumpToTarget(context.breakLabelTarget(labelIndex));
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseContinueStatement(Context& context)
{
    FAIL_IF_FALSE(m_continueScopeDepth, "'continue' is only valid inside a loop statement.");
    context.jumpToTarget(context.continueTarget());
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseContinueLabelStatement(Context& context)
{
    uint32_t labelIndex;
    READ_COMPACT_UINT32_OR_FAIL(labelIndex, "Cannot read the label index.");
    FAIL_IF_FALSE(labelIndex < m_labelDepth, "The label index is incorrect.");
    context.jumpToTarget(context.continueLabelTarget(labelIndex));
    return UNUSED;
}

template <class Context>
ContextStatement WASMFunctionParser::parseSwitchStatement(Context& context)
{
    context.startSwitch();
    uint32_t numberOfCases;
    READ_COMPACT_UINT32_OR_FAIL(numberOfCases, "Cannot read the number of cases.");
    ContextExpression expression = parseExpressionI32(context);
    PROPAGATE_ERROR();

    ContextJumpTarget compare;
    context.jumpToTarget(compare);

    Vector<int64_t> cases;
    Vector<ContextJumpTarget> targets;
    cases.reserveInitialCapacity(numberOfCases);
    targets.reserveInitialCapacity(numberOfCases);
    bool hasDefault = false;
    ContextJumpTarget defaultTarget;

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
            cases.uncheckedAppend(value);
            ContextJumpTarget target;
            context.linkTarget(target);
            targets.uncheckedAppend(target);
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
            hasDefault = true;
            context.linkTarget(defaultTarget);
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
    if (!hasDefault)
        context.linkTarget(defaultTarget);

    m_breakScopeDepth--;

    context.jumpToTarget(context.breakTarget());
    context.linkTarget(compare);

    context.buildSwitch(expression, cases, targets, defaultTarget);

    context.linkTarget(context.breakTarget());
    context.endSwitch();
    return UNUSED;
}

template <class Context>
ContextExpression WASMFunctionParser::parseExpression(Context& context, WASMExpressionType expressionType)
{
    switch (expressionType) {
    case WASMExpressionType::I32:
        return parseExpressionI32(context);
    case WASMExpressionType::F32:
        return parseExpressionF32(context);
    case WASMExpressionType::F64:
        return parseExpressionF64(context);
    case WASMExpressionType::Void:
        return parseExpressionVoid(context);
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
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
        case WASMOpExpressionI32::ConstantPoolIndex:
            return parseConstantPoolIndexExpressionI32(context);
        case WASMOpExpressionI32::Immediate:
            return parseImmediateExpressionI32(context);
        case WASMOpExpressionI32::GetLocal:
            return parseGetLocalExpression(context, WASMType::I32);
        case WASMOpExpressionI32::GetGlobal:
            return parseGetGlobalExpression(context, WASMType::I32);
        case WASMOpExpressionI32::SetLocal:
            return parseSetLocal(context, WASMOpKind::Expression, WASMExpressionType::I32);
        case WASMOpExpressionI32::SetGlobal:
            return parseSetGlobal(context, WASMOpKind::Expression, WASMExpressionType::I32);
        case WASMOpExpressionI32::SLoad8:
            return parseLoad(context, WASMExpressionType::I32, WASMMemoryType::I8, MemoryAccessOffsetMode::NoOffset, MemoryAccessConversion::SignExtend);
        case WASMOpExpressionI32::SLoadWithOffset8:
            return parseLoad(context, WASMExpressionType::I32, WASMMemoryType::I8, MemoryAccessOffsetMode::WithOffset, MemoryAccessConversion::SignExtend);
        case WASMOpExpressionI32::ULoad8:
            return parseLoad(context, WASMExpressionType::I32, WASMMemoryType::I8, MemoryAccessOffsetMode::NoOffset, MemoryAccessConversion::ZeroExtend);
        case WASMOpExpressionI32::ULoadWithOffset8:
            return parseLoad(context, WASMExpressionType::I32, WASMMemoryType::I8, MemoryAccessOffsetMode::WithOffset, MemoryAccessConversion::ZeroExtend);
        case WASMOpExpressionI32::SLoad16:
            return parseLoad(context, WASMExpressionType::I32, WASMMemoryType::I16, MemoryAccessOffsetMode::NoOffset, MemoryAccessConversion::SignExtend);
        case WASMOpExpressionI32::SLoadWithOffset16:
            return parseLoad(context, WASMExpressionType::I32, WASMMemoryType::I16, MemoryAccessOffsetMode::WithOffset, MemoryAccessConversion::SignExtend);
        case WASMOpExpressionI32::ULoad16:
            return parseLoad(context, WASMExpressionType::I32, WASMMemoryType::I16, MemoryAccessOffsetMode::NoOffset, MemoryAccessConversion::ZeroExtend);
        case WASMOpExpressionI32::ULoadWithOffset16:
            return parseLoad(context, WASMExpressionType::I32, WASMMemoryType::I16, MemoryAccessOffsetMode::WithOffset, MemoryAccessConversion::ZeroExtend);
        case WASMOpExpressionI32::Load32:
            return parseLoad(context, WASMExpressionType::I32, WASMMemoryType::I32, MemoryAccessOffsetMode::NoOffset);
        case WASMOpExpressionI32::LoadWithOffset32:
            return parseLoad(context, WASMExpressionType::I32, WASMMemoryType::I32, MemoryAccessOffsetMode::WithOffset);
        case WASMOpExpressionI32::Store8:
            return parseStore(context, WASMOpKind::Expression, WASMExpressionType::I32, WASMMemoryType::I8, MemoryAccessOffsetMode::NoOffset);
        case WASMOpExpressionI32::StoreWithOffset8:
            return parseStore(context, WASMOpKind::Expression, WASMExpressionType::I32, WASMMemoryType::I8, MemoryAccessOffsetMode::WithOffset);
        case WASMOpExpressionI32::Store16:
            return parseStore(context, WASMOpKind::Expression, WASMExpressionType::I32, WASMMemoryType::I16, MemoryAccessOffsetMode::NoOffset);
        case WASMOpExpressionI32::StoreWithOffset16:
            return parseStore(context, WASMOpKind::Expression, WASMExpressionType::I32, WASMMemoryType::I16, MemoryAccessOffsetMode::WithOffset);
        case WASMOpExpressionI32::Store32:
            return parseStore(context, WASMOpKind::Expression, WASMExpressionType::I32, WASMMemoryType::I32, MemoryAccessOffsetMode::NoOffset);
        case WASMOpExpressionI32::StoreWithOffset32:
            return parseStore(context, WASMOpKind::Expression, WASMExpressionType::I32, WASMMemoryType::I32, MemoryAccessOffsetMode::WithOffset);
        case WASMOpExpressionI32::CallInternal:
            return parseCallInternal(context, WASMOpKind::Expression, WASMExpressionType::I32);
        case WASMOpExpressionI32::CallIndirect:
            return parseCallIndirect(context, WASMOpKind::Expression, WASMExpressionType::I32);
        case WASMOpExpressionI32::CallImport:
            return parseCallImport(context, WASMOpKind::Expression, WASMExpressionType::I32);
        case WASMOpExpressionI32::Conditional:
            return parseConditional(context, WASMExpressionType::I32);
        case WASMOpExpressionI32::Comma:
            return parseComma(context, WASMExpressionType::I32);
        case WASMOpExpressionI32::FromF32:
            return parseConvertType(context, WASMExpressionType::F32, WASMExpressionType::I32, WASMTypeConversion::ConvertSigned);
        case WASMOpExpressionI32::FromF64:
            return parseConvertType(context, WASMExpressionType::F64, WASMExpressionType::I32, WASMTypeConversion::ConvertSigned);
        case WASMOpExpressionI32::Negate:
        case WASMOpExpressionI32::BitNot:
        case WASMOpExpressionI32::CountLeadingZeros:
        case WASMOpExpressionI32::LogicalNot:
        case WASMOpExpressionI32::Abs:
            return parseUnaryExpressionI32(context, op);
        case WASMOpExpressionI32::Add:
        case WASMOpExpressionI32::Sub:
        case WASMOpExpressionI32::Mul:
        case WASMOpExpressionI32::SDiv:
        case WASMOpExpressionI32::UDiv:
        case WASMOpExpressionI32::SMod:
        case WASMOpExpressionI32::UMod:
        case WASMOpExpressionI32::BitOr:
        case WASMOpExpressionI32::BitAnd:
        case WASMOpExpressionI32::BitXor:
        case WASMOpExpressionI32::LeftShift:
        case WASMOpExpressionI32::ArithmeticRightShift:
        case WASMOpExpressionI32::LogicalRightShift:
            return parseBinaryExpressionI32(context, op);
        case WASMOpExpressionI32::EqualI32:
        case WASMOpExpressionI32::NotEqualI32:
        case WASMOpExpressionI32::SLessThanI32:
        case WASMOpExpressionI32::ULessThanI32:
        case WASMOpExpressionI32::SLessThanOrEqualI32:
        case WASMOpExpressionI32::ULessThanOrEqualI32:
        case WASMOpExpressionI32::SGreaterThanI32:
        case WASMOpExpressionI32::UGreaterThanI32:
        case WASMOpExpressionI32::SGreaterThanOrEqualI32:
        case WASMOpExpressionI32::UGreaterThanOrEqualI32:
            return parseRelationalI32ExpressionI32(context, op);
        case WASMOpExpressionI32::EqualF32:
        case WASMOpExpressionI32::NotEqualF32:
        case WASMOpExpressionI32::LessThanF32:
        case WASMOpExpressionI32::LessThanOrEqualF32:
        case WASMOpExpressionI32::GreaterThanF32:
        case WASMOpExpressionI32::GreaterThanOrEqualF32:
            return parseRelationalF32ExpressionI32(context, op);
        case WASMOpExpressionI32::EqualF64:
        case WASMOpExpressionI32::NotEqualF64:
        case WASMOpExpressionI32::LessThanF64:
        case WASMOpExpressionI32::LessThanOrEqualF64:
        case WASMOpExpressionI32::GreaterThanF64:
        case WASMOpExpressionI32::GreaterThanOrEqualF64:
            return parseRelationalF64ExpressionI32(context, op);
        case WASMOpExpressionI32::SMin:
        case WASMOpExpressionI32::UMin:
        case WASMOpExpressionI32::SMax:
        case WASMOpExpressionI32::UMax:
            return parseMinOrMaxExpressionI32(context, op);
        default:
            ASSERT_NOT_REACHED();
        }
    } else {
        switch (opWithImmediate) {
        case WASMOpExpressionI32WithImmediate::ConstantPoolIndex:
            return parseConstantPoolIndexExpressionI32(context, immediate);
        case WASMOpExpressionI32WithImmediate::Immediate:
            return parseImmediateExpressionI32(context, immediate);
        case WASMOpExpressionI32WithImmediate::GetLocal:
            return parseGetLocalExpression(context, WASMType::I32, immediate);
        default:
            ASSERT_NOT_REACHED();
        }
    }
    return 0;
}

template <class Context>
ContextExpression WASMFunctionParser::parseConstantPoolIndexExpressionI32(Context& context, uint32_t constantPoolIndex)
{
    FAIL_IF_FALSE(constantPoolIndex < m_module->i32Constants().size(), "The constant pool index is incorrect.");
    return context.buildImmediateI32(m_module->i32Constants()[constantPoolIndex]);
}

template <class Context>
ContextExpression WASMFunctionParser::parseConstantPoolIndexExpressionI32(Context& context)
{
    uint32_t constantPoolIndex;
    READ_COMPACT_UINT32_OR_FAIL(constantPoolIndex, "Cannot read the constant pool index.");
    return parseConstantPoolIndexExpressionI32(context, constantPoolIndex);
}

template <class Context>
ContextExpression WASMFunctionParser::parseImmediateExpressionI32(Context& context, uint32_t immediate)
{
    return context.buildImmediateI32(immediate);
}

template <class Context>
ContextExpression WASMFunctionParser::parseImmediateExpressionI32(Context& context)
{
    uint32_t immediate;
    READ_COMPACT_UINT32_OR_FAIL(immediate, "Cannot read the immediate.");
    return parseImmediateExpressionI32(context, immediate);
}

template <class Context>
ContextExpression WASMFunctionParser::parseUnaryExpressionI32(Context& context, WASMOpExpressionI32 op)
{
    ContextExpression expression = parseExpressionI32(context);
    PROPAGATE_ERROR();
    return context.buildUnaryI32(expression, op);
}

template <class Context>
ContextExpression WASMFunctionParser::parseBinaryExpressionI32(Context& context, WASMOpExpressionI32 op)
{
    ContextExpression left = parseExpressionI32(context);
    PROPAGATE_ERROR();
    ContextExpression right = parseExpressionI32(context);
    PROPAGATE_ERROR();
    return context.buildBinaryI32(left, right, op);
}

template <class Context>
ContextExpression WASMFunctionParser::parseRelationalI32ExpressionI32(Context& context, WASMOpExpressionI32 op)
{
    ContextExpression left = parseExpressionI32(context);
    PROPAGATE_ERROR();
    ContextExpression right = parseExpressionI32(context);
    PROPAGATE_ERROR();
    return context.buildRelationalI32(left, right, op);
}

template <class Context>
ContextExpression WASMFunctionParser::parseRelationalF32ExpressionI32(Context& context, WASMOpExpressionI32 op)
{
    ContextExpression left = parseExpressionF32(context);
    PROPAGATE_ERROR();
    ContextExpression right = parseExpressionF32(context);
    PROPAGATE_ERROR();
    return context.buildRelationalF32(left, right, op);
}

template <class Context>
ContextExpression WASMFunctionParser::parseRelationalF64ExpressionI32(Context& context, WASMOpExpressionI32 op)
{
    ContextExpression left = parseExpressionF64(context);
    PROPAGATE_ERROR();
    ContextExpression right = parseExpressionF64(context);
    PROPAGATE_ERROR();
    return context.buildRelationalF64(left, right, op);
}

template <class Context>
ContextExpression WASMFunctionParser::parseMinOrMaxExpressionI32(Context& context, WASMOpExpressionI32 op)
{
    uint32_t numberOfArguments;
    READ_COMPACT_UINT32_OR_FAIL(numberOfArguments, "Cannot read the number of arguments to min/max.");
    FAIL_IF_FALSE(numberOfArguments >= 2, "Min/max must be passed at least 2 arguments.");
    ContextExpression current = parseExpressionI32(context);
    PROPAGATE_ERROR();
    for (uint32_t i = 1; i < numberOfArguments; ++i) {
        ContextExpression expression = parseExpressionI32(context);
        PROPAGATE_ERROR();
        current = context.buildMinOrMaxI32(current, expression, op);
    }
    return current;
}

template <class Context>
ContextExpression WASMFunctionParser::parseExpressionF32(Context& context)
{
    bool hasImmediate;
    WASMOpExpressionF32 op;
    WASMOpExpressionF32WithImmediate opWithImmediate;
    uint8_t immediate;
    READ_OP_EXPRESSION_F32_OR_FAIL(hasImmediate, op, opWithImmediate, immediate, "Cannot read the float32 expression opcode.");
    if (!hasImmediate) {
        switch (op) {
        case WASMOpExpressionF32::ConstantPoolIndex:
            return parseConstantPoolIndexExpressionF32(context);
        case WASMOpExpressionF32::Immediate:
            return parseImmediateExpressionF32(context);
        case WASMOpExpressionF32::GetLocal:
            return parseGetLocalExpression(context, WASMType::F32);
        case WASMOpExpressionF32::GetGlobal:
            return parseGetGlobalExpression(context, WASMType::F32);
        case WASMOpExpressionF32::SetLocal:
            return parseSetLocal(context, WASMOpKind::Expression, WASMExpressionType::F32);
        case WASMOpExpressionF32::SetGlobal:
            return parseSetGlobal(context, WASMOpKind::Expression, WASMExpressionType::F32);
        case WASMOpExpressionF32::Load:
            return parseLoad(context, WASMExpressionType::F32, WASMMemoryType::F32, MemoryAccessOffsetMode::NoOffset);
        case WASMOpExpressionF32::LoadWithOffset:
            return parseLoad(context, WASMExpressionType::F32, WASMMemoryType::F32, MemoryAccessOffsetMode::WithOffset);
        case WASMOpExpressionF32::Store:
            return parseStore(context, WASMOpKind::Expression, WASMExpressionType::F32, WASMMemoryType::F32, MemoryAccessOffsetMode::NoOffset);
        case WASMOpExpressionF32::StoreWithOffset:
            return parseStore(context, WASMOpKind::Expression, WASMExpressionType::F32, WASMMemoryType::F32, MemoryAccessOffsetMode::WithOffset);
        case WASMOpExpressionF32::CallInternal:
            return parseCallInternal(context, WASMOpKind::Expression, WASMExpressionType::F32);
        case WASMOpExpressionF32::CallIndirect:
            return parseCallIndirect(context, WASMOpKind::Expression, WASMExpressionType::F32);
        case WASMOpExpressionF32::Conditional:
            return parseConditional(context, WASMExpressionType::F32);
        case WASMOpExpressionF32::Comma:
            return parseComma(context, WASMExpressionType::F32);
        case WASMOpExpressionF32::FromS32:
            return parseConvertType(context, WASMExpressionType::I32, WASMExpressionType::F32, WASMTypeConversion::ConvertSigned);
        case WASMOpExpressionF32::FromU32:
            return parseConvertType(context, WASMExpressionType::I32, WASMExpressionType::F32, WASMTypeConversion::ConvertUnsigned);
        case WASMOpExpressionF32::FromF64:
            return parseConvertType(context, WASMExpressionType::F64, WASMExpressionType::F32, WASMTypeConversion::Demote);
        case WASMOpExpressionF32::Negate:
        case WASMOpExpressionF32::Abs:
        case WASMOpExpressionF32::Ceil:
        case WASMOpExpressionF32::Floor:
        case WASMOpExpressionF32::Sqrt:
            return parseUnaryExpressionF32(context, op);
        case WASMOpExpressionF32::Add:
        case WASMOpExpressionF32::Sub:
        case WASMOpExpressionF32::Mul:
        case WASMOpExpressionF32::Div:
            return parseBinaryExpressionF32(context, op);
        default:
            ASSERT_NOT_REACHED();
        }
    } else {
        switch (opWithImmediate) {
        case WASMOpExpressionF32WithImmediate::ConstantPoolIndex:
            return parseConstantPoolIndexExpressionF32(context, immediate);
        case WASMOpExpressionF32WithImmediate::GetLocal:
            return parseGetLocalExpression(context, WASMType::F32, immediate);
        default:
            ASSERT_NOT_REACHED();
        }
    }
    return 0;
}

template <class Context>
ContextExpression WASMFunctionParser::parseConstantPoolIndexExpressionF32(Context& context, uint32_t constantIndex)
{
    FAIL_IF_FALSE(constantIndex < m_module->f32Constants().size(), "The constant pool index is incorrect.");
    return context.buildImmediateF32(m_module->f32Constants()[constantIndex]);
}

template <class Context>
ContextExpression WASMFunctionParser::parseConstantPoolIndexExpressionF32(Context& context)
{
    uint32_t constantIndex;
    READ_COMPACT_UINT32_OR_FAIL(constantIndex, "Cannot read the constant pool index.");
    return parseConstantPoolIndexExpressionF32(context, constantIndex);
}

template <class Context>
ContextExpression WASMFunctionParser::parseImmediateExpressionF32(Context& context)
{
    float immediate;
    READ_FLOAT_OR_FAIL(immediate, "Cannot read the immediate.");
    return context.buildImmediateF32(immediate);
}

template <class Context>
ContextExpression WASMFunctionParser::parseUnaryExpressionF32(Context& context, WASMOpExpressionF32 op)
{
    ContextExpression expression = parseExpressionF32(context);
    PROPAGATE_ERROR();
    return context.buildUnaryF32(expression, op);
}

template <class Context>
ContextExpression WASMFunctionParser::parseBinaryExpressionF32(Context& context, WASMOpExpressionF32 op)
{
    ContextExpression left = parseExpressionF32(context);
    PROPAGATE_ERROR();
    ContextExpression right = parseExpressionF32(context);
    PROPAGATE_ERROR();
    return context.buildBinaryF32(left, right, op);
}

template <class Context>
ContextExpression WASMFunctionParser::parseExpressionF64(Context& context)
{
    bool hasImmediate;
    WASMOpExpressionF64 op;
    WASMOpExpressionF64WithImmediate opWithImmediate;
    uint8_t immediate;
    READ_OP_EXPRESSION_F64_OR_FAIL(hasImmediate, op, opWithImmediate, immediate, "Cannot read the float64 expression opcode.");
    if (!hasImmediate) {
        switch (op) {
        case WASMOpExpressionF64::ConstantPoolIndex:
            return parseConstantPoolIndexExpressionF64(context);
        case WASMOpExpressionF64::Immediate:
            return parseImmediateExpressionF64(context);
        case WASMOpExpressionF64::GetLocal:
            return parseGetLocalExpression(context, WASMType::F64);
        case WASMOpExpressionF64::GetGlobal:
            return parseGetGlobalExpression(context, WASMType::F64);
        case WASMOpExpressionF64::SetLocal:
            return parseSetLocal(context, WASMOpKind::Expression, WASMExpressionType::F64);
        case WASMOpExpressionF64::SetGlobal:
            return parseSetGlobal(context, WASMOpKind::Expression, WASMExpressionType::F64);
        case WASMOpExpressionF64::Load:
            return parseLoad(context, WASMExpressionType::F64, WASMMemoryType::F64, MemoryAccessOffsetMode::NoOffset);
        case WASMOpExpressionF64::LoadWithOffset:
            return parseLoad(context, WASMExpressionType::F64, WASMMemoryType::F64, MemoryAccessOffsetMode::WithOffset);
        case WASMOpExpressionF64::Store:
            return parseStore(context, WASMOpKind::Expression, WASMExpressionType::F64, WASMMemoryType::F64, MemoryAccessOffsetMode::NoOffset);
        case WASMOpExpressionF64::StoreWithOffset:
            return parseStore(context, WASMOpKind::Expression, WASMExpressionType::F64, WASMMemoryType::F64, MemoryAccessOffsetMode::WithOffset);
        case WASMOpExpressionF64::CallInternal:
            return parseCallInternal(context, WASMOpKind::Expression, WASMExpressionType::F64);
        case WASMOpExpressionF64::CallImport:
            return parseCallImport(context, WASMOpKind::Expression, WASMExpressionType::F64);
        case WASMOpExpressionF64::CallIndirect:
            return parseCallIndirect(context, WASMOpKind::Expression, WASMExpressionType::F64);
        case WASMOpExpressionF64::Conditional:
            return parseConditional(context, WASMExpressionType::F64);
        case WASMOpExpressionF64::Comma:
            return parseComma(context, WASMExpressionType::F64);
        case WASMOpExpressionF64::FromS32:
            return parseConvertType(context, WASMExpressionType::I32, WASMExpressionType::F64, WASMTypeConversion::ConvertSigned);
        case WASMOpExpressionF64::FromU32:
            return parseConvertType(context, WASMExpressionType::I32, WASMExpressionType::F64, WASMTypeConversion::ConvertUnsigned);
        case WASMOpExpressionF64::FromF32:
            return parseConvertType(context, WASMExpressionType::F32, WASMExpressionType::F64, WASMTypeConversion::Promote);
        case WASMOpExpressionF64::Negate:
        case WASMOpExpressionF64::Abs:
        case WASMOpExpressionF64::Ceil:
        case WASMOpExpressionF64::Floor:
        case WASMOpExpressionF64::Sqrt:
        case WASMOpExpressionF64::Cos:
        case WASMOpExpressionF64::Sin:
        case WASMOpExpressionF64::Tan:
        case WASMOpExpressionF64::ACos:
        case WASMOpExpressionF64::ASin:
        case WASMOpExpressionF64::ATan:
        case WASMOpExpressionF64::Exp:
        case WASMOpExpressionF64::Ln:
            return parseUnaryExpressionF64(context, op);
        case WASMOpExpressionF64::Add:
        case WASMOpExpressionF64::Sub:
        case WASMOpExpressionF64::Mul:
        case WASMOpExpressionF64::Div:
        case WASMOpExpressionF64::Mod:
        case WASMOpExpressionF64::ATan2:
        case WASMOpExpressionF64::Pow:
            return parseBinaryExpressionF64(context, op);
        case WASMOpExpressionF64::Min:
        case WASMOpExpressionF64::Max:
            return parseMinOrMaxExpressionF64(context, op);
        default:
            ASSERT_NOT_REACHED();
        }
    } else {
        switch (opWithImmediate) {
        case WASMOpExpressionF64WithImmediate::ConstantPoolIndex:
            return parseConstantPoolIndexExpressionF64(context, immediate);
        case WASMOpExpressionF64WithImmediate::GetLocal:
            return parseGetLocalExpression(context, WASMType::F64, immediate);
        default:
            ASSERT_NOT_REACHED();
        }
    }
    return 0;
}

template <class Context>
ContextExpression WASMFunctionParser::parseConstantPoolIndexExpressionF64(Context& context, uint32_t constantIndex)
{
    FAIL_IF_FALSE(constantIndex < m_module->f64Constants().size(), "The constant index is incorrect.");
    return context.buildImmediateF64(m_module->f64Constants()[constantIndex]);
}

template <class Context>
ContextExpression WASMFunctionParser::parseConstantPoolIndexExpressionF64(Context& context)
{
    uint32_t constantIndex;
    READ_COMPACT_UINT32_OR_FAIL(constantIndex, "Cannot read the constant index.");
    return parseConstantPoolIndexExpressionF64(context, constantIndex);
}

template <class Context>
ContextExpression WASMFunctionParser::parseImmediateExpressionF64(Context& context)
{
    double immediate;
    READ_DOUBLE_OR_FAIL(immediate, "Cannot read the immediate.");
    return context.buildImmediateF64(immediate);
}

template <class Context>
ContextExpression WASMFunctionParser::parseUnaryExpressionF64(Context& context, WASMOpExpressionF64 op)
{
    ContextExpression expression = parseExpressionF64(context);
    PROPAGATE_ERROR();
    return context.buildUnaryF64(expression, op);
}

template <class Context>
ContextExpression WASMFunctionParser::parseBinaryExpressionF64(Context& context, WASMOpExpressionF64 op)
{
    ContextExpression left = parseExpressionF64(context);
    PROPAGATE_ERROR();
    ContextExpression right = parseExpressionF64(context);
    PROPAGATE_ERROR();
    return context.buildBinaryF64(left, right, op);
}

template <class Context>
ContextExpression WASMFunctionParser::parseMinOrMaxExpressionF64(Context& context, WASMOpExpressionF64 op)
{
    uint32_t numberOfArguments;
    READ_COMPACT_UINT32_OR_FAIL(numberOfArguments, "Cannot read the number of arguments to min/max.");
    FAIL_IF_FALSE(numberOfArguments >= 2, "Min/max must be passed at least 2 arguments.");
    ContextExpression current = parseExpressionF64(context);
    PROPAGATE_ERROR();
    for (uint32_t i = 1; i < numberOfArguments; ++i) {
        ContextExpression expression = parseExpressionF64(context);
        PROPAGATE_ERROR();
        current = context.buildMinOrMaxF64(current, expression, op);
    }
    return current;
}

template <class Context>
ContextExpression WASMFunctionParser::parseExpressionVoid(Context& context)
{
    WASMOpExpressionVoid op;
    READ_OP_EXPRESSION_VOID_OR_FAIL(op, "Cannot read the void expression opcode.");
    switch (op) {
    case WASMOpExpressionVoid::CallInternal:
        return parseCallInternal(context, WASMOpKind::Expression, WASMExpressionType::Void);
    case WASMOpExpressionVoid::CallIndirect:
        return parseCallIndirect(context, WASMOpKind::Expression, WASMExpressionType::Void);
    case WASMOpExpressionVoid::CallImport:
        return parseCallImport(context, WASMOpKind::Expression, WASMExpressionType::Void);
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

template <class Context>
ContextExpression WASMFunctionParser::parseGetLocalExpression(Context& context, WASMType type, uint32_t localIndex)
{
    FAIL_IF_FALSE(localIndex < m_localTypes.size(), "The local index is incorrect.");
    FAIL_IF_FALSE(m_localTypes[localIndex] == type, "Expected a local of type " + nameOfType(type) + '.');
    return context.buildGetLocal(localIndex, type);
}

template <class Context>
ContextExpression WASMFunctionParser::parseGetLocalExpression(Context& context, WASMType type)
{
    uint32_t localIndex;
    READ_COMPACT_UINT32_OR_FAIL(localIndex, "Cannot read the local index.");
    return parseGetLocalExpression(context, type, localIndex);
}

template <class Context>
ContextExpression WASMFunctionParser::parseGetGlobalExpression(Context& context, WASMType type)
{
    uint32_t globalIndex;
    READ_COMPACT_UINT32_OR_FAIL(globalIndex, "Cannot read the global index.");
    FAIL_IF_FALSE(globalIndex < m_module->globalVariableTypes().size(), "The global index is incorrect.");
    FAIL_IF_FALSE(m_module->globalVariableTypes()[globalIndex] == type, "Expected a global of type " + nameOfType(type) + '.');
    return context.buildGetGlobal(globalIndex, type);
}

template <class Context>
ContextExpression WASMFunctionParser::parseSetLocal(Context& context, WASMOpKind opKind, WASMExpressionType expressionType, uint32_t localIndex)
{
    FAIL_IF_FALSE(localIndex < m_localTypes.size(), "The local variable index is incorrect.");
    WASMType type = m_localTypes[localIndex];
    if (opKind == WASMOpKind::Expression)
        FAIL_IF_FALSE(expressionType == WASMExpressionType(type), "The type doesn't match.");
    ContextExpression expression = parseExpression(context, WASMExpressionType(type));
    PROPAGATE_ERROR();
    return context.buildSetLocal(opKind, localIndex, expression, type);
}

template <class Context>
ContextExpression WASMFunctionParser::parseSetLocal(Context& context, WASMOpKind opKind, WASMExpressionType expressionType)
{
    uint32_t localIndex;
    READ_COMPACT_UINT32_OR_FAIL(localIndex, "Cannot read the local index.");
    return parseSetLocal(context, opKind, expressionType, localIndex);
}

template <class Context>
ContextExpression WASMFunctionParser::parseSetGlobal(Context& context, WASMOpKind opKind, WASMExpressionType expressionType, uint32_t globalIndex)
{
    FAIL_IF_FALSE(globalIndex < m_module->globalVariableTypes().size(), "The global index is incorrect.");
    WASMType type = m_module->globalVariableTypes()[globalIndex];
    if (opKind == WASMOpKind::Expression)
        FAIL_IF_FALSE(expressionType == WASMExpressionType(type), "The type doesn't match.");
    ContextExpression expression = parseExpression(context, WASMExpressionType(type));
    PROPAGATE_ERROR();
    return context.buildSetGlobal(opKind, globalIndex, expression, type);
}

template <class Context>
ContextExpression WASMFunctionParser::parseSetGlobal(Context& context, WASMOpKind opKind, WASMExpressionType expressionType)
{
    uint32_t globalIndex;
    READ_COMPACT_UINT32_OR_FAIL(globalIndex, "Cannot read the global index.");
    return parseSetGlobal(context, opKind, expressionType, globalIndex);
}

template <class Context>
ContextMemoryAddress WASMFunctionParser::parseMemoryAddress(Context& context, MemoryAccessOffsetMode offsetMode)
{
    uint32_t offset = 0;
    if (offsetMode == MemoryAccessOffsetMode::WithOffset)
        READ_COMPACT_UINT32_OR_FAIL(offset, "Cannot read the address offset.");
    ContextExpression index = parseExpressionI32(context);
    PROPAGATE_ERROR();
    return ContextMemoryAddress(index, offset);
}

template <class Context>
ContextExpression WASMFunctionParser::parseLoad(Context& context, WASMExpressionType expressionType, WASMMemoryType memoryType, MemoryAccessOffsetMode offsetMode, MemoryAccessConversion conversion)
{
    FAIL_IF_FALSE(m_module->arrayBuffer(), "An ArrayBuffer is not provided.");
    const ContextMemoryAddress& memoryAddress = parseMemoryAddress(context, offsetMode);
    PROPAGATE_ERROR();
    return context.buildLoad(memoryAddress, expressionType, memoryType, conversion);
}

template <class Context>
ContextExpression WASMFunctionParser::parseStore(Context& context, WASMOpKind opKind, WASMExpressionType expressionType, WASMMemoryType memoryType, MemoryAccessOffsetMode offsetMode)
{
    FAIL_IF_FALSE(m_module->arrayBuffer(), "An ArrayBuffer is not provided.");
    const ContextMemoryAddress& memoryAddress = parseMemoryAddress(context, offsetMode);
    PROPAGATE_ERROR();

    ContextExpression value = parseExpression(context, expressionType);
    PROPAGATE_ERROR();
    return context.buildStore(opKind, memoryAddress, expressionType, memoryType, value);
}

template <class Context>
ContextExpressionList WASMFunctionParser::parseCallArguments(Context& context, const Vector<WASMType>& arguments)
{
    ContextExpressionList argumentList;
    for (size_t i = 0; i < arguments.size(); ++i) {
        ContextExpression expression = parseExpression(context, WASMExpressionType(arguments[i]));
        PROPAGATE_ERROR();
        context.appendExpressionList(argumentList, expression);
    }
    return argumentList;
}

template <class Context>
ContextExpression WASMFunctionParser::parseCallInternal(Context& context, WASMOpKind opKind, WASMExpressionType returnType)
{
    uint32_t functionIndex;
    READ_COMPACT_UINT32_OR_FAIL(functionIndex, "Cannot read the function index.");
    FAIL_IF_FALSE(functionIndex < m_module->functionDeclarations().size(), "The function index is incorrect.");
    const WASMSignature& signature = m_module->signatures()[m_module->functionDeclarations()[functionIndex].signatureIndex];
    if (opKind == WASMOpKind::Expression)
        FAIL_IF_FALSE(signature.returnType == returnType, "Wrong return type.");

    ContextExpressionList argumentList = parseCallArguments(context, signature.arguments);
    PROPAGATE_ERROR();
    return context.buildCallInternal(functionIndex, argumentList, signature, returnType);
}

template <class Context>
ContextExpression WASMFunctionParser::parseCallIndirect(Context& context, WASMOpKind opKind, WASMExpressionType returnType)
{
    uint32_t functionPointerTableIndex;
    READ_COMPACT_UINT32_OR_FAIL(functionPointerTableIndex, "Cannot read the function pointer table index.");
    FAIL_IF_FALSE(functionPointerTableIndex < m_module->functionPointerTables().size(), "The function pointer table index is incorrect.");
    const WASMFunctionPointerTable& functionPointerTable = m_module->functionPointerTables()[functionPointerTableIndex];
    const WASMSignature& signature = m_module->signatures()[functionPointerTable.signatureIndex];
    if (opKind == WASMOpKind::Expression)
        FAIL_IF_FALSE(signature.returnType == returnType, "Wrong return type.");

    ContextExpression index = parseExpressionI32(context);
    PROPAGATE_ERROR();

    ContextExpressionList argumentList = parseCallArguments(context, signature.arguments);
    PROPAGATE_ERROR();
    return context.buildCallIndirect(functionPointerTableIndex, index, argumentList, signature, returnType);
}

template <class Context>
ContextExpression WASMFunctionParser::parseCallImport(Context& context, WASMOpKind opKind, WASMExpressionType returnType)
{
    uint32_t functionImportSignatureIndex;
    READ_COMPACT_UINT32_OR_FAIL(functionImportSignatureIndex, "Cannot read the function import signature index.");
    FAIL_IF_FALSE(functionImportSignatureIndex < m_module->functionImportSignatures().size(), "The function import signature index is incorrect.");
    const WASMFunctionImportSignature& functionImportSignature = m_module->functionImportSignatures()[functionImportSignatureIndex];
    const WASMSignature& signature = m_module->signatures()[functionImportSignature.signatureIndex];
    if (opKind == WASMOpKind::Expression)
        FAIL_IF_FALSE(signature.returnType == returnType, "Wrong return type.");

    ContextExpressionList argumentList = parseCallArguments(context, signature.arguments);
    PROPAGATE_ERROR();
    return context.buildCallImport(functionImportSignature.functionImportIndex, argumentList, signature, returnType);
}

template <class Context>
ContextExpression WASMFunctionParser::parseConditional(Context& context, WASMExpressionType expressionType)
{
    ContextJumpTarget elseTarget;
    ContextJumpTarget end;

    ContextExpression condition = parseExpressionI32(context);
    PROPAGATE_ERROR();

    context.jumpToTargetIf(Context::JumpCondition::Zero, condition, elseTarget);

    parseExpression(context, expressionType);
    PROPAGATE_ERROR();
    
    context.jumpToTarget(end);
    context.linkTarget(elseTarget);

    // We use discard() here to decrement the stack top in the baseline JIT.
    context.discard(UNUSED);
    parseExpression(context, expressionType);
    PROPAGATE_ERROR();
    
    context.linkTarget(end);
    return UNUSED;
}

template <class Context>
ContextExpression WASMFunctionParser::parseComma(Context& context, WASMExpressionType expressionType)
{
    WASMExpressionType leftExpressionType;
    READ_EXPRESSION_TYPE_OR_FAIL(leftExpressionType, "Cannot read the expression type.");
    ContextExpression leftExpression = parseExpression(context, leftExpressionType);
    PROPAGATE_ERROR();
    if (leftExpressionType != WASMExpressionType::Void)
        context.discard(leftExpression);
    return parseExpression(context, expressionType);
}

template <class Context>
ContextExpression WASMFunctionParser::parseConvertType(Context& context, WASMExpressionType fromType, WASMExpressionType toType, WASMTypeConversion conversion)
{
    ContextExpression expression = parseExpression(context, fromType);
    PROPAGATE_ERROR();

    return context.buildConvertType(expression, fromType, toType, conversion);
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
