/*
 * Copyright (C) 2019 Apple Inc. All Rights Reserved.
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
#include "UnlinkedCodeBlockGenerator.h"

#include "BytecodeGenerator.h"
#include "BytecodeLivenessAnalysis.h"
#include "BytecodeRewriter.h"
#include "ClassInfo.h"
#include "CodeCache.h"
#include "ExecutableInfo.h"
#include "FunctionOverrides.h"
#include "InstructionStream.h"
#include "JSCInlines.h"
#include "JSString.h"
#include "Opcode.h"
#include "Parser.h"
#include "PreciseJumpTargetsInlines.h"
#include "SourceProvider.h"
#include "Structure.h"
#include "SymbolTable.h"
#include "UnlinkedEvalCodeBlock.h"
#include "UnlinkedFunctionCodeBlock.h"
#include "UnlinkedMetadataTableInlines.h"
#include "UnlinkedModuleProgramCodeBlock.h"
#include "UnlinkedProgramCodeBlock.h"
#include <wtf/DataLog.h>

namespace JSC {

inline void UnlinkedCodeBlockGenerator::getLineAndColumn(const ExpressionRangeInfo& info, unsigned& line, unsigned& column) const
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
        const ExpressionRangeInfo::FatPosition& fatPos = m_expressionInfoFatPositions[fatIndex];
        line = fatPos.line;
        column = fatPos.column;
        break;
    }
    } // switch
}

void UnlinkedCodeBlockGenerator::addExpressionInfo(unsigned instructionOffset, int divot, int startOffset, int endOffset, unsigned line, unsigned column)
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
        unsigned fatIndex = m_expressionInfoFatPositions.size();
        ExpressionRangeInfo::FatPosition fatPos = { line, column };
        m_expressionInfoFatPositions.append(fatPos);
        info.position = fatIndex;
    }
    } // switch

    m_expressionInfo.append(info);
}

void UnlinkedCodeBlockGenerator::addTypeProfilerExpressionInfo(unsigned instructionOffset, unsigned startDivot, unsigned endDivot)
{
    UnlinkedCodeBlock::RareData::TypeProfilerExpressionRange range;
    range.m_startDivot = startDivot;
    range.m_endDivot = endDivot;
    m_typeProfilerInfoMap.set(instructionOffset, range);
}

void UnlinkedCodeBlockGenerator::finalize(std::unique_ptr<InstructionStream> instructions)
{
    ASSERT(instructions);
    {
        auto locker = holdLock(m_codeBlock->cellLock());
        m_codeBlock->m_instructions = WTFMove(instructions);
        m_codeBlock->m_metadata->finalize();

        m_codeBlock->m_jumpTargets = WTFMove(m_jumpTargets);
        m_codeBlock->m_identifiers = WTFMove(m_identifiers);
        m_codeBlock->m_constantRegisters = WTFMove(m_constantRegisters);
        m_codeBlock->m_constantsSourceCodeRepresentation = WTFMove(m_constantsSourceCodeRepresentation);
        m_codeBlock->m_functionDecls = WTFMove(m_functionDecls);
        m_codeBlock->m_functionExprs = WTFMove(m_functionExprs);
        m_codeBlock->m_expressionInfo = WTFMove(m_expressionInfo);
        m_codeBlock->m_outOfLineJumpTargets = WTFMove(m_outOfLineJumpTargets);

        if (!m_codeBlock->m_rareData) {
            if (!m_exceptionHandlers.isEmpty()
                || !m_switchJumpTables.isEmpty()
                || !m_stringSwitchJumpTables.isEmpty()
                || !m_expressionInfoFatPositions.isEmpty()
                || !m_typeProfilerInfoMap.isEmpty()
                || !m_opProfileControlFlowBytecodeOffsets.isEmpty()
                || !m_bitVectors.isEmpty()
                || !m_constantIdentifierSets.isEmpty())
                m_codeBlock->createRareDataIfNecessary(locker);
        }
        if (m_codeBlock->m_rareData) {
            m_codeBlock->m_rareData->m_exceptionHandlers = WTFMove(m_exceptionHandlers);
            m_codeBlock->m_rareData->m_switchJumpTables = WTFMove(m_switchJumpTables);
            m_codeBlock->m_rareData->m_stringSwitchJumpTables = WTFMove(m_stringSwitchJumpTables);
            m_codeBlock->m_rareData->m_expressionInfoFatPositions = WTFMove(m_expressionInfoFatPositions);
            m_codeBlock->m_rareData->m_typeProfilerInfoMap = WTFMove(m_typeProfilerInfoMap);
            m_codeBlock->m_rareData->m_opProfileControlFlowBytecodeOffsets = WTFMove(m_opProfileControlFlowBytecodeOffsets);
            m_codeBlock->m_rareData->m_bitVectors = WTFMove(m_bitVectors);
            m_codeBlock->m_rareData->m_constantIdentifierSets = WTFMove(m_constantIdentifierSets);
        }
    }
    m_vm.heap.writeBarrier(m_codeBlock.get());
    m_vm.heap.reportExtraMemoryAllocated(m_codeBlock->m_instructions->sizeInBytes() + m_codeBlock->m_metadata->sizeInBytes());
}

UnlinkedHandlerInfo* UnlinkedCodeBlockGenerator::handlerForBytecodeIndex(BytecodeIndex bytecodeIndex, RequiredHandler requiredHandler)
{
    return handlerForIndex(bytecodeIndex.offset(), requiredHandler);
}

UnlinkedHandlerInfo* UnlinkedCodeBlockGenerator::handlerForIndex(unsigned index, RequiredHandler requiredHandler)
{
    return UnlinkedHandlerInfo::handlerForIndex<UnlinkedHandlerInfo>(m_exceptionHandlers, index, requiredHandler);
}

void UnlinkedCodeBlockGenerator::applyModification(BytecodeRewriter& rewriter, InstructionStreamWriter& instructions)
{
    // Before applying the changes, we adjust the jumps based on the original bytecode offset, the offset to the jump target, and
    // the insertion information.

    rewriter.adjustJumpTargets();

    // Then, exception handlers should be adjusted.
    for (UnlinkedHandlerInfo& handler : m_exceptionHandlers) {
        handler.target = rewriter.adjustAbsoluteOffset(handler.target);
        handler.start = rewriter.adjustAbsoluteOffset(handler.start);
        handler.end = rewriter.adjustAbsoluteOffset(handler.end);
    }

    for (size_t i = 0; i < m_opProfileControlFlowBytecodeOffsets.size(); ++i)
        m_opProfileControlFlowBytecodeOffsets[i] = rewriter.adjustAbsoluteOffset(m_opProfileControlFlowBytecodeOffsets[i]);

    if (!m_typeProfilerInfoMap.isEmpty()) {
        HashMap<unsigned, UnlinkedCodeBlock::RareData::TypeProfilerExpressionRange> adjustedTypeProfilerInfoMap;
        for (auto& entry : m_typeProfilerInfoMap)
            adjustedTypeProfilerInfoMap.set(rewriter.adjustAbsoluteOffset(entry.key), entry.value);
        m_typeProfilerInfoMap.swap(adjustedTypeProfilerInfoMap);
    }

    for (size_t i = 0; i < m_expressionInfo.size(); ++i)
        m_expressionInfo[i].instructionOffset = rewriter.adjustAbsoluteOffset(m_expressionInfo[i].instructionOffset);

    // Then, modify the unlinked instructions.
    rewriter.applyModification();

    // And recompute the jump target based on the modified unlinked instructions.
    m_jumpTargets.clear();
    recomputePreciseJumpTargets(this, instructions, m_jumpTargets);
}

void UnlinkedCodeBlockGenerator::addOutOfLineJumpTarget(InstructionStream::Offset bytecodeOffset, int target)
{
    RELEASE_ASSERT(target);
    m_outOfLineJumpTargets.set(bytecodeOffset, target);
}

int UnlinkedCodeBlockGenerator::outOfLineJumpOffset(InstructionStream::Offset bytecodeOffset)
{
    ASSERT(m_outOfLineJumpTargets.contains(bytecodeOffset));
    return m_outOfLineJumpTargets.get(bytecodeOffset);
}

void UnlinkedCodeBlockGenerator::dump(PrintStream&) const
{
}

} // namespace JSC
