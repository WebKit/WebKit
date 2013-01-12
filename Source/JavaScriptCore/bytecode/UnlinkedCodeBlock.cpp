/*
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
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

#include "UnlinkedCodeBlock.h"

#include "BytecodeGenerator.h"
#include "ClassInfo.h"
#include "CodeCache.h"
#include "Executable.h"
#include "JSString.h"
#include "Operations.h"
#include "SourceProvider.h"
#include "Structure.h"
#include "SymbolTable.h"

namespace JSC {

const ClassInfo UnlinkedFunctionExecutable::s_info = { "UnlinkedFunctionExecutable", 0, 0, 0, CREATE_METHOD_TABLE(UnlinkedFunctionExecutable) };
const ClassInfo UnlinkedCodeBlock::s_info = { "UnlinkedCodeBlock", 0, 0, 0, CREATE_METHOD_TABLE(UnlinkedCodeBlock) };
const ClassInfo UnlinkedGlobalCodeBlock::s_info = { "UnlinkedGlobalCodeBlock", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(UnlinkedGlobalCodeBlock) };
const ClassInfo UnlinkedProgramCodeBlock::s_info = { "UnlinkedProgramCodeBlock", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(UnlinkedProgramCodeBlock) };
const ClassInfo UnlinkedEvalCodeBlock::s_info = { "UnlinkedEvalCodeBlock", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(UnlinkedEvalCodeBlock) };
const ClassInfo UnlinkedFunctionCodeBlock::s_info = { "UnlinkedFunctionCodeBlock", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(UnlinkedFunctionCodeBlock) };

unsigned UnlinkedCodeBlock::addOrFindConstant(JSValue v)
{
    unsigned numberOfConstants = numberOfConstantRegisters();
    for (unsigned i = 0; i < numberOfConstants; ++i) {
        if (getConstant(FirstConstantRegisterIndex + i) == v)
            return i;
    }
    return addConstant(v);
}

UnlinkedFunctionExecutable::UnlinkedFunctionExecutable(JSGlobalData* globalData, Structure* structure, const SourceCode& source, FunctionBodyNode* node)
    : Base(*globalData, structure)
    , m_numCapturedVariables(node->capturedVariableCount())
    , m_forceUsesArguments(node->usesArguments())
    , m_isInStrictContext(node->isStrictMode())
    , m_hasCapturedVariables(node->hasCapturedVariables())
    , m_name(node->ident())
    , m_inferredName(node->inferredName())
    , m_parameters(node->parameters())
    , m_firstLineOffset(node->firstLine() - source.firstLine())
    , m_lineCount(node->lastLine() - node->firstLine())
    , m_functionStartOffset(node->functionStart() - source.startOffset())
    , m_startOffset(node->source().startOffset() - source.startOffset())
    , m_sourceLength(node->source().length())
    , m_features(node->features())
    , m_functionNameIsInScopeToggle(node->functionNameIsInScopeToggle())
{
}

void UnlinkedFunctionExecutable::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    UnlinkedFunctionExecutable* thisObject = jsCast<UnlinkedFunctionExecutable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, &s_info);
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());
    Base::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_nameValue);
    visitor.append(&thisObject->m_symbolTableForCall);
    visitor.append(&thisObject->m_symbolTableForConstruct);
}

FunctionExecutable* UnlinkedFunctionExecutable::link(JSGlobalData& globalData, const SourceCode& source, size_t lineOffset, size_t sourceOffset)
{
    unsigned firstLine = lineOffset + m_firstLineOffset;
    unsigned startOffset = sourceOffset + m_startOffset;
    SourceCode code(source.provider(), startOffset, startOffset + m_sourceLength, firstLine);
    return FunctionExecutable::create(globalData, code, this, firstLine, firstLine + m_lineCount);
}

UnlinkedFunctionExecutable* UnlinkedFunctionExecutable::fromGlobalCode(const Identifier& name, ExecState* exec, Debugger*, const SourceCode& source, JSObject** exception)
{
    ParserError error;
    CodeCache* codeCache = exec->globalData().codeCache();
    UnlinkedFunctionExecutable* executable = codeCache->getFunctionExecutableFromGlobalCode(exec->globalData(), name, source, error);
    if (error.m_type != ParserError::ErrorNone) {
        *exception = error.toErrorObject(exec->lexicalGlobalObject(), source);
        return 0;
    }

    return executable;
}

UnlinkedFunctionCodeBlock* UnlinkedFunctionExecutable::codeBlockFor(JSGlobalData& globalData, const SourceCode& source, CodeSpecializationKind specializationKind, DebuggerMode debuggerMode, ProfilerMode profilerMode, ParserError& error)
{
    switch (specializationKind) {
    case CodeForCall:
        if (UnlinkedFunctionCodeBlock* codeBlock = m_codeBlockForCall.get()) {
            globalData.codeCache()->usedFunctionCode(globalData, codeBlock);
            return codeBlock;
        }
        break;
    case CodeForConstruct:
        if (UnlinkedFunctionCodeBlock* codeBlock = m_codeBlockForConstruct.get()) {
            globalData.codeCache()->usedFunctionCode(globalData, codeBlock);
            return codeBlock;
        }
        break;
    }

    UnlinkedFunctionCodeBlock* result = globalData.codeCache()->getFunctionCodeBlock(globalData, this, source, specializationKind, debuggerMode, profilerMode, error);
    
    if (error.m_type != ParserError::ErrorNone)
        return 0;

    switch (specializationKind) {
    case CodeForCall:
        m_codeBlockForCall = PassWeak<UnlinkedFunctionCodeBlock>(result);
        m_symbolTableForCall.set(globalData, this, result->symbolTable());
        break;
    case CodeForConstruct:
        m_codeBlockForConstruct = PassWeak<UnlinkedFunctionCodeBlock>(result);
        m_symbolTableForConstruct.set(globalData, this, result->symbolTable());
        break;
    }
    return result;
}

String UnlinkedFunctionExecutable::paramString() const
{
    FunctionParameters& parameters = *m_parameters;
    StringBuilder builder;
    for (size_t pos = 0; pos < parameters.size(); ++pos) {
        if (!builder.isEmpty())
            builder.appendLiteral(", ");
        builder.append(parameters[pos].string());
    }
    return builder.toString();
}

UnlinkedCodeBlock::UnlinkedCodeBlock(JSGlobalData* globalData, Structure* structure, CodeType codeType, const ExecutableInfo& info)
    : Base(*globalData, structure)
    , m_numVars(0)
    , m_numCalleeRegisters(0)
    , m_numParameters(0)
    , m_globalData(globalData)
    , m_argumentsRegister(-1)
    , m_needsFullScopeChain(info.m_needsActivation)
    , m_usesEval(info.m_usesEval)
    , m_isNumericCompareFunction(false)
    , m_isStrictMode(info.m_isStrictMode)
    , m_isConstructor(info.m_isConstructor)
    , m_hasCapturedVariables(false)
    , m_firstLine(0)
    , m_lineCount(0)
    , m_features(0)
    , m_codeType(codeType)
    , m_resolveOperationCount(0)
    , m_putToBaseOperationCount(1)
    , m_arrayProfileCount(0)
    , m_arrayAllocationProfileCount(0)
    , m_valueProfileCount(0)
    , m_llintCallLinkInfoCount(0)
#if ENABLE(BYTECODE_COMMENTS)
    , m_bytecodeCommentIterator(0)
#endif
{

}

void UnlinkedCodeBlock::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    UnlinkedCodeBlock* thisObject = jsCast<UnlinkedCodeBlock*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, &s_info);
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());
    Base::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_symbolTable);
    for (FunctionExpressionVector::iterator ptr = thisObject->m_functionDecls.begin(), end = thisObject->m_functionDecls.end(); ptr != end; ++ptr)
        visitor.append(ptr);
    for (FunctionExpressionVector::iterator ptr = thisObject->m_functionExprs.begin(), end = thisObject->m_functionExprs.end(); ptr != end; ++ptr)
        visitor.append(ptr);
    visitor.appendValues(thisObject->m_constantRegisters.data(), thisObject->m_constantRegisters.size());
    if (thisObject->m_rareData) {
        for (size_t i = 0, end = thisObject->m_rareData->m_regexps.size(); i != end; i++)
            visitor.append(&thisObject->m_rareData->m_regexps[i]);
    }
}

int UnlinkedCodeBlock::lineNumberForBytecodeOffset(unsigned bytecodeOffset)
{
    ASSERT(bytecodeOffset < instructions().size());
    Vector<LineInfo>& lineInfo = m_lineInfo;
    
    int low = 0;
    int high = lineInfo.size();
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (lineInfo[mid].instructionOffset <= bytecodeOffset)
            low = mid + 1;
        else
            high = mid;
    }
    
    if (!low)
        return 0;
    return lineInfo[low - 1].lineNumber;
}

void UnlinkedCodeBlock::expressionRangeForBytecodeOffset(unsigned bytecodeOffset, int& divot, int& startOffset, int& endOffset)
{
    ASSERT(bytecodeOffset < instructions().size());

    if (!m_expressionInfo.size()) {
        startOffset = 0;
        endOffset = 0;
        divot = 0;
        return;
    }

    Vector<ExpressionRangeInfo>& expressionInfo = m_expressionInfo;

    int low = 0;
    int high = expressionInfo.size();
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (expressionInfo[mid].instructionOffset <= bytecodeOffset)
            low = mid + 1;
        else
            high = mid;
    }

    ASSERT(low);
    if (!low) {
        startOffset = 0;
        endOffset = 0;
        divot = 0;
        return;
    }

    startOffset = expressionInfo[low - 1].startOffset;
    endOffset = expressionInfo[low - 1].endOffset;
    divot = expressionInfo[low - 1].divotPoint;
}

void UnlinkedProgramCodeBlock::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    UnlinkedProgramCodeBlock* thisObject = jsCast<UnlinkedProgramCodeBlock*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, &s_info);
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());
    Base::visitChildren(thisObject, visitor);
    for (size_t i = 0, end = thisObject->m_functionDeclarations.size(); i != end; i++)
        visitor.append(&thisObject->m_functionDeclarations[i].second);
}

UnlinkedCodeBlock::~UnlinkedCodeBlock()
{
}

void UnlinkedProgramCodeBlock::destroy(JSCell* cell)
{
    jsCast<UnlinkedProgramCodeBlock*>(cell)->~UnlinkedProgramCodeBlock();
}

void UnlinkedEvalCodeBlock::destroy(JSCell* cell)
{
    jsCast<UnlinkedEvalCodeBlock*>(cell)->~UnlinkedEvalCodeBlock();
}

void UnlinkedFunctionCodeBlock::destroy(JSCell* cell)
{
    jsCast<UnlinkedFunctionCodeBlock*>(cell)->~UnlinkedFunctionCodeBlock();
}

void UnlinkedFunctionExecutable::destroy(JSCell* cell)
{
    jsCast<UnlinkedFunctionExecutable*>(cell)->~UnlinkedFunctionExecutable();
}

}

