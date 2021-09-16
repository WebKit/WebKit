/*
 * Copyright (C) 2012-2021 Apple Inc. All Rights Reserved.
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

#include "BytecodeLivenessAnalysis.h"
#include "BytecodeStructs.h"
#include "ClassInfo.h"
#include "ExecutableInfo.h"
#include "InstructionStream.h"
#include "JSCJSValueInlines.h"
#include "UnlinkedMetadataTableInlines.h"
#include <wtf/DataLog.h>

namespace JSC {

const ClassInfo UnlinkedCodeBlock::s_info = { "UnlinkedCodeBlock", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(UnlinkedCodeBlock) };

UnlinkedCodeBlock::UnlinkedCodeBlock(VM& vm, Structure* structure, CodeType codeType, const ExecutableInfo& info, OptionSet<CodeGenerationMode> codeGenerationMode)
    : Base(vm, structure)
    , m_numVars(0)
    , m_usesCallEval(false)
    , m_numCalleeLocals(0)
    , m_isConstructor(info.isConstructor())
    , m_numParameters(0)
    , m_hasCapturedVariables(false)
    , m_isBuiltinFunction(info.isBuiltinFunction())
    , m_superBinding(static_cast<unsigned>(info.superBinding()))
    , m_scriptMode(static_cast<unsigned>(info.scriptMode()))
    , m_isArrowFunctionContext(info.isArrowFunctionContext())
    , m_isClassContext(info.isClassContext())
    , m_hasTailCalls(false)
    , m_constructorKind(static_cast<unsigned>(info.constructorKind()))
    , m_derivedContextType(static_cast<unsigned>(info.derivedContextType()))
    , m_evalContextType(static_cast<unsigned>(info.evalContextType()))
    , m_codeType(static_cast<unsigned>(codeType))
    , m_didOptimize(static_cast<unsigned>(TriState::Indeterminate))
    , m_age(0)
    , m_hasCheckpoints(false)
    , m_parseMode(info.parseMode())
    , m_codeGenerationMode(codeGenerationMode)
    , m_metadata(UnlinkedMetadataTable::create())
{
    ASSERT(m_constructorKind == static_cast<unsigned>(info.constructorKind()));
    ASSERT(m_codeType == static_cast<unsigned>(codeType));
    ASSERT(m_didOptimize == static_cast<unsigned>(TriState::Indeterminate));
    if (info.needsClassFieldInitializer() == NeedsClassFieldInitializer::Yes) {
        Locker locker { cellLock() };
        createRareDataIfNecessary(locker);
        m_rareData->m_needsClassFieldInitializer = static_cast<unsigned>(NeedsClassFieldInitializer::Yes);
    }
    if (info.privateBrandRequirement() == PrivateBrandRequirement::Needed) {
        Locker locker { cellLock() };
        createRareDataIfNecessary(locker);
        m_rareData->m_privateBrandRequirement = static_cast<unsigned>(PrivateBrandRequirement::Needed);
    }
}

template<typename Visitor>
void UnlinkedCodeBlock::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    UnlinkedCodeBlock* thisObject = jsCast<UnlinkedCodeBlock*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    Locker locker { thisObject->cellLock() };
    if (visitor.isFirstVisit())
        thisObject->m_age = std::min<unsigned>(static_cast<unsigned>(thisObject->m_age) + 1, maxAge);
    for (auto& barrier : thisObject->m_functionDecls)
        visitor.append(barrier);
    for (auto& barrier : thisObject->m_functionExprs)
        visitor.append(barrier);
    visitor.appendValues(thisObject->m_constantRegisters.data(), thisObject->m_constantRegisters.size());
    size_t extraMemory = thisObject->m_metadata->sizeInBytes();
    if (thisObject->m_instructions)
        extraMemory += thisObject->m_instructions->sizeInBytes();
    if (thisObject->hasRareData())
        extraMemory += thisObject->m_rareData->sizeInBytes(locker);

    extraMemory += thisObject->m_jumpTargets.byteSize();
    extraMemory += thisObject->m_identifiers.byteSize();
    extraMemory += thisObject->m_constantRegisters.byteSize();
    extraMemory += thisObject->m_constantsSourceCodeRepresentation.byteSize();
    extraMemory += thisObject->m_functionDecls.byteSize();
    extraMemory += thisObject->m_functionExprs.byteSize();

    visitor.reportExtraMemoryVisited(extraMemory);
}

DEFINE_VISIT_CHILDREN(UnlinkedCodeBlock);

size_t UnlinkedCodeBlock::estimatedSize(JSCell* cell, VM& vm)
{
    UnlinkedCodeBlock* thisObject = jsCast<UnlinkedCodeBlock*>(cell);
    size_t extraSize = thisObject->m_metadata->sizeInBytes();
    if (thisObject->m_instructions)
        extraSize += thisObject->m_instructions->sizeInBytes();
    return Base::estimatedSize(cell, vm) + extraSize;
}

size_t UnlinkedCodeBlock::RareData::sizeInBytes(const AbstractLocker&) const
{
    size_t size = sizeof(RareData);
    size += m_exceptionHandlers.byteSize();
    size += m_unlinkedSwitchJumpTables.byteSize();
    size += m_unlinkedStringSwitchJumpTables.byteSize();
    size += m_expressionInfoFatPositions.byteSize();
    size += m_typeProfilerInfoMap.capacity() * sizeof(decltype(m_typeProfilerInfoMap)::KeyValuePairType);
    size += m_opProfileControlFlowBytecodeOffsets.byteSize();
    size += m_bitVectors.byteSize();
    // FIXME: account for each bit vector.
    size += m_constantIdentifierSets.byteSize();
    for (const auto& identifierSet : m_constantIdentifierSets)
        size += identifierSet.capacity() * sizeof(std::remove_reference_t<decltype(identifierSet)>::ValueType);
    return size;
}

int UnlinkedCodeBlock::lineNumberForBytecodeIndex(BytecodeIndex bytecodeIndex)
{
    ASSERT(bytecodeIndex.offset() < instructions().size());
    int divot { 0 };
    int startOffset { 0 };
    int endOffset { 0 };
    unsigned line { 0 };
    unsigned column { 0 };
    expressionRangeForBytecodeIndex(bytecodeIndex, divot, startOffset, endOffset, line, column);
    return line;
}

inline void UnlinkedCodeBlock::getLineAndColumn(const ExpressionRangeInfo& info,
    unsigned& line, unsigned& column) const
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
static void dumpLineColumnEntry(size_t index, const InstructionStream& instructionStream, unsigned instructionOffset, unsigned line, unsigned column)
{
    const auto instruction = instructionStream.at(instructionOffset);
    const char* event = "";
    if (instruction->is<OpDebug>()) {
        switch (instruction->as<OpDebug>().m_debugHookType) {
        case WillExecuteProgram: event = " WillExecuteProgram"; break;
        case DidExecuteProgram: event = " DidExecuteProgram"; break;
        case DidEnterCallFrame: event = " DidEnterCallFrame"; break;
        case DidReachDebuggerStatement: event = " DidReachDebuggerStatement"; break;
        case WillLeaveCallFrame: event = " WillLeaveCallFrame"; break;
        case WillExecuteStatement: event = " WillExecuteStatement"; break;
        case WillExecuteExpression: event = " WillExecuteExpression"; break;
        }
    }
    dataLogF("  [%zu] pc %u @ line %u col %u : %s%s\n", index, instructionOffset, line, column, instruction->name(), event);
}

void UnlinkedCodeBlock::dumpExpressionRangeInfo()
{
    FixedVector<ExpressionRangeInfo>& expressionInfo = m_expressionInfo;

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

void UnlinkedCodeBlock::expressionRangeForBytecodeIndex(BytecodeIndex bytecodeIndex,
    int& divot, int& startOffset, int& endOffset, unsigned& line, unsigned& column) const
{
    ASSERT(bytecodeIndex.offset() < instructions().size());

    if (!m_expressionInfo.size()) {
        startOffset = 0;
        endOffset = 0;
        divot = 0;
        line = 0;
        column = 0;
        return;
    }

    const FixedVector<ExpressionRangeInfo>& expressionInfo = m_expressionInfo;

    int low = 0;
    int high = expressionInfo.size();
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (expressionInfo[mid].instructionOffset <= bytecodeIndex.offset())
            low = mid + 1;
        else
            high = mid;
    }

    if (!low)
        low = 1;

    const ExpressionRangeInfo& info = expressionInfo[low - 1];
    startOffset = info.startOffset;
    endOffset = info.endOffset;
    divot = info.divotPoint;
    getLineAndColumn(info, line, column);
}

bool UnlinkedCodeBlock::typeProfilerExpressionInfoForBytecodeOffset(unsigned bytecodeOffset, unsigned& startDivot, unsigned& endDivot)
{
    static constexpr bool verbose = false;
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

UnlinkedCodeBlock::~UnlinkedCodeBlock()
{
}

const InstructionStream& UnlinkedCodeBlock::instructions() const
{
    ASSERT(m_instructions.get());
    return *m_instructions;
}

UnlinkedHandlerInfo* UnlinkedCodeBlock::handlerForBytecodeIndex(BytecodeIndex bytecodeIndex, RequiredHandler requiredHandler)
{
    return handlerForIndex(bytecodeIndex.offset(), requiredHandler);
}

UnlinkedHandlerInfo* UnlinkedCodeBlock::handlerForIndex(unsigned index, RequiredHandler requiredHandler)
{
    if (!m_rareData)
        return nullptr;
    return UnlinkedHandlerInfo::handlerForIndex<UnlinkedHandlerInfo>(m_rareData->m_exceptionHandlers, index, requiredHandler);
}

void UnlinkedCodeBlock::dump(PrintStream&) const
{
}

BytecodeLivenessAnalysis& UnlinkedCodeBlock::livenessAnalysisSlow(CodeBlock* codeBlock)
{
    RELEASE_ASSERT(codeBlock->unlinkedCodeBlock() == this);

    {
        ConcurrentJSLocker locker(m_lock);
        if (!m_liveness) {
            // There is a chance two compiler threads raced to the slow path.
            // Grabbing the lock above defends against computing liveness twice.
            m_liveness = makeUnique<BytecodeLivenessAnalysis>(codeBlock);
        }
    }
    
    return *m_liveness;
}

int UnlinkedCodeBlock::outOfLineJumpOffset(InstructionStream::Offset bytecodeOffset)
{
    ASSERT(m_outOfLineJumpTargets.contains(bytecodeOffset));
    return m_outOfLineJumpTargets.get(bytecodeOffset);
}

} // namespace JSC
