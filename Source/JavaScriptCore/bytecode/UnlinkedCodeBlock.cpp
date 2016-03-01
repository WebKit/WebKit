/*
 * Copyright (C) 2012, 2013, 2015 Apple Inc. All Rights Reserved.
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
#include "ExecutableInfo.h"
#include "FunctionOverrides.h"
#include "JSString.h"
#include "JSCInlines.h"
#include "Parser.h"
#include "SourceProvider.h"
#include "Structure.h"
#include "SymbolTable.h"
#include "UnlinkedInstructionStream.h"
#include <wtf/DataLog.h>

namespace JSC {

const ClassInfo UnlinkedCodeBlock::s_info = { "UnlinkedCodeBlock", 0, 0, CREATE_METHOD_TABLE(UnlinkedCodeBlock) };
const ClassInfo UnlinkedGlobalCodeBlock::s_info = { "UnlinkedGlobalCodeBlock", &Base::s_info, 0, CREATE_METHOD_TABLE(UnlinkedGlobalCodeBlock) };
const ClassInfo UnlinkedProgramCodeBlock::s_info = { "UnlinkedProgramCodeBlock", &Base::s_info, 0, CREATE_METHOD_TABLE(UnlinkedProgramCodeBlock) };
const ClassInfo UnlinkedModuleProgramCodeBlock::s_info = { "UnlinkedModuleProgramCodeBlock", &Base::s_info, nullptr, CREATE_METHOD_TABLE(UnlinkedModuleProgramCodeBlock) };
const ClassInfo UnlinkedEvalCodeBlock::s_info = { "UnlinkedEvalCodeBlock", &Base::s_info, 0, CREATE_METHOD_TABLE(UnlinkedEvalCodeBlock) };
const ClassInfo UnlinkedFunctionCodeBlock::s_info = { "UnlinkedFunctionCodeBlock", &Base::s_info, 0, CREATE_METHOD_TABLE(UnlinkedFunctionCodeBlock) };

UnlinkedCodeBlock::UnlinkedCodeBlock(VM* vm, Structure* structure, CodeType codeType, const ExecutableInfo& info)
    : Base(*vm, structure)
    , m_numVars(0)
    , m_numCalleeLocals(0)
    , m_numParameters(0)
    , m_globalObjectRegister(VirtualRegister())
    , m_usesEval(info.usesEval())
    , m_isStrictMode(info.isStrictMode())
    , m_isConstructor(info.isConstructor())
    , m_hasCapturedVariables(false)
    , m_isBuiltinFunction(info.isBuiltinFunction())
    , m_constructorKind(static_cast<unsigned>(info.constructorKind()))
    , m_superBinding(static_cast<unsigned>(info.superBinding()))
    , m_derivedContextType(static_cast<unsigned>(info.derivedContextType()))
    , m_isArrowFunctionContext(info.isArrowFunctionContext())
    , m_isClassContext(info.isClassContext())
    , m_firstLine(0)
    , m_lineCount(0)
    , m_endColumn(UINT_MAX)
    , m_parseMode(info.parseMode())
    , m_features(0)
    , m_codeType(codeType)
    , m_arrayProfileCount(0)
    , m_arrayAllocationProfileCount(0)
    , m_objectAllocationProfileCount(0)
    , m_valueProfileCount(0)
    , m_llintCallLinkInfoCount(0)
{
    for (auto& constantRegisterIndex : m_linkTimeConstants)
        constantRegisterIndex = 0;
    ASSERT(m_constructorKind == static_cast<unsigned>(info.constructorKind()));
}

VM* UnlinkedCodeBlock::vm() const
{
    return MarkedBlock::blockFor(this)->vm();
}

void UnlinkedCodeBlock::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    UnlinkedCodeBlock* thisObject = jsCast<UnlinkedCodeBlock*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    for (FunctionExpressionVector::iterator ptr = thisObject->m_functionDecls.begin(), end = thisObject->m_functionDecls.end(); ptr != end; ++ptr)
        visitor.append(ptr);
    for (FunctionExpressionVector::iterator ptr = thisObject->m_functionExprs.begin(), end = thisObject->m_functionExprs.end(); ptr != end; ++ptr)
        visitor.append(ptr);
    visitor.appendValues(thisObject->m_constantRegisters.data(), thisObject->m_constantRegisters.size());
    if (thisObject->m_unlinkedInstructions)
        visitor.reportExtraMemoryVisited(thisObject->m_unlinkedInstructions->sizeInBytes());
    if (thisObject->m_rareData) {
        for (size_t i = 0, end = thisObject->m_rareData->m_regexps.size(); i != end; i++)
            visitor.append(&thisObject->m_rareData->m_regexps[i]);
    }
}

size_t UnlinkedCodeBlock::estimatedSize(JSCell* cell)
{
    UnlinkedCodeBlock* thisObject = jsCast<UnlinkedCodeBlock*>(cell);
    size_t extraSize = thisObject->m_unlinkedInstructions ? thisObject->m_unlinkedInstructions->sizeInBytes() : 0;
    return Base::estimatedSize(cell) + extraSize;
}

int UnlinkedCodeBlock::lineNumberForBytecodeOffset(unsigned bytecodeOffset)
{
    ASSERT(bytecodeOffset < instructions().count());
    int divot;
    int startOffset;
    int endOffset;
    unsigned line;
    unsigned column;
    expressionRangeForBytecodeOffset(bytecodeOffset, divot, startOffset, endOffset, line, column);
    return line;
}

inline void UnlinkedCodeBlock::getLineAndColumn(ExpressionRangeInfo& info,
    unsigned& line, unsigned& column)
{
    switch (info.mode) {
    case ExpressionRangeInfo::FatLineMode:
        info.decodeFatLineMode(line, column);
        break;
    case ExpressionRangeInfo::FatColumnMode:
        info.decodeFatColumnMode(line, column);
        break;
    case ExpressionRangeInfo::FatLineAndColumnMode: {
        unsigned fatIndex = info.position;
        ExpressionRangeInfo::FatPosition& fatPos = m_rareData->m_expressionInfoFatPositions[fatIndex];
        line = fatPos.line;
        column = fatPos.column;
        break;
    }
    } // switch
}

#ifndef NDEBUG
static void dumpLineColumnEntry(size_t index, const UnlinkedInstructionStream& instructionStream, unsigned instructionOffset, unsigned line, unsigned column)
{
    const auto& instructions = instructionStream.unpackForDebugging();
    OpcodeID opcode = instructions[instructionOffset].u.opcode;
    const char* event = "";
    if (opcode == op_debug) {
        switch (instructions[instructionOffset + 1].u.operand) {
        case WillExecuteProgram: event = " WillExecuteProgram"; break;
        case DidExecuteProgram: event = " DidExecuteProgram"; break;
        case DidEnterCallFrame: event = " DidEnterCallFrame"; break;
        case DidReachBreakpoint: event = " DidReachBreakpoint"; break;
        case WillLeaveCallFrame: event = " WillLeaveCallFrame"; break;
        case WillExecuteStatement: event = " WillExecuteStatement"; break;
        }
    }
    dataLogF("  [%zu] pc %u @ line %u col %u : %s%s\n", index, instructionOffset, line, column, opcodeNames[opcode], event);
}

void UnlinkedCodeBlock::dumpExpressionRangeInfo()
{
    Vector<ExpressionRangeInfo>& expressionInfo = m_expressionInfo;

    size_t size = m_expressionInfo.size();
    dataLogF("UnlinkedCodeBlock %p expressionRangeInfo[%zu] {\n", this, size);
    for (size_t i = 0; i < size; i++) {
        ExpressionRangeInfo& info = expressionInfo[i];
        unsigned line;
        unsigned column;
        getLineAndColumn(info, line, column);
        dumpLineColumnEntry(i, instructions(), info.instructionOffset, line, column);
    }
    dataLog("}\n");
}
#endif

void UnlinkedCodeBlock::expressionRangeForBytecodeOffset(unsigned bytecodeOffset,
    int& divot, int& startOffset, int& endOffset, unsigned& line, unsigned& column)
{
    ASSERT(bytecodeOffset < instructions().count());

    if (!m_expressionInfo.size()) {
        startOffset = 0;
        endOffset = 0;
        divot = 0;
        line = 0;
        column = 0;
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

    if (!low)
        low = 1;

    ExpressionRangeInfo& info = expressionInfo[low - 1];
    startOffset = info.startOffset;
    endOffset = info.endOffset;
    divot = info.divotPoint;
    getLineAndColumn(info, line, column);
}

void UnlinkedCodeBlock::addExpressionInfo(unsigned instructionOffset,
    int divot, int startOffset, int endOffset, unsigned line, unsigned column)
{
    if (divot > ExpressionRangeInfo::MaxDivot) {
        // Overflow has occurred, we can only give line number info for errors for this region
        divot = 0;
        startOffset = 0;
        endOffset = 0;
    } else if (startOffset > ExpressionRangeInfo::MaxOffset) {
        // If the start offset is out of bounds we clear both offsets
        // so we only get the divot marker. Error message will have to be reduced
        // to line and charPosition number.
        startOffset = 0;
        endOffset = 0;
    } else if (endOffset > ExpressionRangeInfo::MaxOffset) {
        // The end offset is only used for additional context, and is much more likely
        // to overflow (eg. function call arguments) so we are willing to drop it without
        // dropping the rest of the range.
        endOffset = 0;
    }

    unsigned positionMode =
        (line <= ExpressionRangeInfo::MaxFatLineModeLine && column <= ExpressionRangeInfo::MaxFatLineModeColumn) 
        ? ExpressionRangeInfo::FatLineMode
        : (line <= ExpressionRangeInfo::MaxFatColumnModeLine && column <= ExpressionRangeInfo::MaxFatColumnModeColumn)
        ? ExpressionRangeInfo::FatColumnMode
        : ExpressionRangeInfo::FatLineAndColumnMode;

    ExpressionRangeInfo info;
    info.instructionOffset = instructionOffset;
    info.divotPoint = divot;
    info.startOffset = startOffset;
    info.endOffset = endOffset;

    info.mode = positionMode;
    switch (positionMode) {
    case ExpressionRangeInfo::FatLineMode:
        info.encodeFatLineMode(line, column);
        break;
    case ExpressionRangeInfo::FatColumnMode:
        info.encodeFatColumnMode(line, column);
        break;
    case ExpressionRangeInfo::FatLineAndColumnMode: {
        createRareDataIfNecessary();
        unsigned fatIndex = m_rareData->m_expressionInfoFatPositions.size();
        ExpressionRangeInfo::FatPosition fatPos = { line, column };
        m_rareData->m_expressionInfoFatPositions.append(fatPos);
        info.position = fatIndex;
    }
    } // switch

    m_expressionInfo.append(info);
}

bool UnlinkedCodeBlock::typeProfilerExpressionInfoForBytecodeOffset(unsigned bytecodeOffset, unsigned& startDivot, unsigned& endDivot)
{
    static const bool verbose = false;
    if (!m_rareData) {
        if (verbose)
            dataLogF("Don't have assignment info for offset:%u\n", bytecodeOffset);
        startDivot = UINT_MAX;
        endDivot = UINT_MAX;
        return false;
    }

    auto iter = m_rareData->m_typeProfilerInfoMap.find(bytecodeOffset);
    if (iter == m_rareData->m_typeProfilerInfoMap.end()) {
        if (verbose)
            dataLogF("Don't have assignment info for offset:%u\n", bytecodeOffset);
        startDivot = UINT_MAX;
        endDivot = UINT_MAX;
        return false;
    }
    
    RareData::TypeProfilerExpressionRange& range = iter->value;
    startDivot = range.m_startDivot;
    endDivot = range.m_endDivot;
    return true;
}

void UnlinkedCodeBlock::addTypeProfilerExpressionInfo(unsigned instructionOffset, unsigned startDivot, unsigned endDivot)
{
    createRareDataIfNecessary();
    RareData::TypeProfilerExpressionRange range;
    range.m_startDivot = startDivot;
    range.m_endDivot = endDivot;
    m_rareData->m_typeProfilerInfoMap.set(instructionOffset, range);
}

void UnlinkedProgramCodeBlock::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    UnlinkedProgramCodeBlock* thisObject = jsCast<UnlinkedProgramCodeBlock*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

void UnlinkedModuleProgramCodeBlock::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    UnlinkedModuleProgramCodeBlock* thisObject = jsCast<UnlinkedModuleProgramCodeBlock*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

UnlinkedCodeBlock::~UnlinkedCodeBlock()
{
}

void UnlinkedProgramCodeBlock::destroy(JSCell* cell)
{
    jsCast<UnlinkedProgramCodeBlock*>(cell)->~UnlinkedProgramCodeBlock();
}

void UnlinkedModuleProgramCodeBlock::destroy(JSCell* cell)
{
    jsCast<UnlinkedModuleProgramCodeBlock*>(cell)->~UnlinkedModuleProgramCodeBlock();
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

void UnlinkedCodeBlock::setInstructions(std::unique_ptr<UnlinkedInstructionStream> instructions)
{
    ASSERT(instructions);
    m_unlinkedInstructions = WTFMove(instructions);
    Heap::heap(this)->reportExtraMemoryAllocated(m_unlinkedInstructions->sizeInBytes());
}

const UnlinkedInstructionStream& UnlinkedCodeBlock::instructions() const
{
    ASSERT(m_unlinkedInstructions.get());
    return *m_unlinkedInstructions;
}

}

