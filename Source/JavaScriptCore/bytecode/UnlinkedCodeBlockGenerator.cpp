/*
 * Copyright (C) 2019-2024 Apple Inc. All Rights Reserved.
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

#include "BytecodeRewriter.h"
#include "ExpressionInfoInlines.h"
#include "InstructionStream.h"
#include "JSCJSValueInlines.h"
#include "PreciseJumpTargets.h"
#include "UnlinkedMetadataTableInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(UnlinkedCodeBlockGenerator);

void UnlinkedCodeBlockGenerator::addExpressionInfo(unsigned instructionOffset, unsigned divot, unsigned startOffset, unsigned endOffset, LineColumn lineColumn)
{
    m_expressionInfoEncoder.encode(instructionOffset, divot, startOffset, endOffset, lineColumn);
}

void UnlinkedCodeBlockGenerator::addTypeProfilerExpressionInfo(unsigned instructionOffset, unsigned startDivot, unsigned endDivot)
{
    UnlinkedCodeBlock::RareData::TypeProfilerExpressionRange range;
    range.m_startDivot = startDivot;
    range.m_endDivot = endDivot;
    m_typeProfilerInfoMap.set(instructionOffset, range);
}

void UnlinkedCodeBlockGenerator::finalize(std::unique_ptr<JSInstructionStream> instructions)
{
    ASSERT(instructions);
    {
        Locker locker { m_codeBlock->cellLock() };
        m_codeBlock->m_instructions = WTFMove(instructions);
        m_codeBlock->allocateSharedProfiles(m_numBinaryArithProfiles, m_numUnaryArithProfiles);
        m_codeBlock->m_metadata->finalize();

        m_codeBlock->m_jumpTargets = WTFMove(m_jumpTargets);
        m_codeBlock->m_identifiers = WTFMove(m_identifiers);
        m_codeBlock->m_constantRegisters = WTFMove(m_constantRegisters);
        m_codeBlock->m_constantsSourceCodeRepresentation = WTFMove(m_constantsSourceCodeRepresentation);
        m_codeBlock->m_functionDecls = WTFMove(m_functionDecls);
        m_codeBlock->m_functionExprs = WTFMove(m_functionExprs);
        m_codeBlock->m_expressionInfo = m_expressionInfoEncoder.createExpressionInfo();

        m_codeBlock->m_outOfLineJumpTargets = WTFMove(m_outOfLineJumpTargets);

        if (!m_codeBlock->m_rareData) {
            if (!m_exceptionHandlers.isEmpty()
                || !m_unlinkedSwitchJumpTables.isEmpty()
                || !m_unlinkedStringSwitchJumpTables.isEmpty()
                || !m_typeProfilerInfoMap.isEmpty()
                || !m_opProfileControlFlowBytecodeOffsets.isEmpty()
                || !m_bitVectors.isEmpty()
                || !m_constantIdentifierSets.isEmpty())
                m_codeBlock->createRareDataIfNecessary(locker);
        }
        if (m_codeBlock->m_rareData) {
            m_codeBlock->m_rareData->m_exceptionHandlers = WTFMove(m_exceptionHandlers);
            m_codeBlock->m_rareData->m_unlinkedSwitchJumpTables = WTFMove(m_unlinkedSwitchJumpTables);
            m_codeBlock->m_rareData->m_unlinkedStringSwitchJumpTables = WTFMove(m_unlinkedStringSwitchJumpTables);
            m_codeBlock->m_rareData->m_typeProfilerInfoMap = WTFMove(m_typeProfilerInfoMap);
            m_codeBlock->m_rareData->m_opProfileControlFlowBytecodeOffsets = WTFMove(m_opProfileControlFlowBytecodeOffsets);
            m_codeBlock->m_rareData->m_bitVectors = WTFMove(m_bitVectors);
            m_codeBlock->m_rareData->m_constantIdentifierSets = WTFMove(m_constantIdentifierSets);
        }

        if (UNLIKELY(Options::returnEarlyFromInfiniteLoopsForFuzzing()))
            m_codeBlock->initializeLoopHintExecutionCounter();
    }
    m_vm.writeBarrier(m_codeBlock.get());
    m_vm.heap.reportExtraMemoryAllocated(m_codeBlock.get(), m_codeBlock->m_instructions->sizeInBytes() + m_codeBlock->metadataSizeInBytes());
}

UnlinkedHandlerInfo* UnlinkedCodeBlockGenerator::handlerForBytecodeIndex(BytecodeIndex bytecodeIndex, RequiredHandler requiredHandler)
{
    return handlerForIndex(bytecodeIndex.offset(), requiredHandler);
}

UnlinkedHandlerInfo* UnlinkedCodeBlockGenerator::handlerForIndex(unsigned index, RequiredHandler requiredHandler)
{
    return UnlinkedHandlerInfo::handlerForIndex<UnlinkedHandlerInfo>(m_exceptionHandlers, index, requiredHandler);
}

void UnlinkedCodeBlockGenerator::applyModification(BytecodeRewriter& rewriter, JSInstructionStreamWriter& instructions)
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

    Vector<unsigned> bytecodeOffsetAdjustments;
    rewriter.forEachLabelPoint([&] (int32_t bytecodeOffset) {
        bytecodeOffsetAdjustments.append(bytecodeOffset);
    });
    m_expressionInfoEncoder.remap(WTFMove(bytecodeOffsetAdjustments), [&] (int32_t bytecodeOffset) {
        return rewriter.adjustAbsoluteOffset(bytecodeOffset);
    });

    // Then, modify the unlinked instructions.
    rewriter.applyModification();

    // And recompute the jump target based on the modified unlinked instructions.
    m_jumpTargets.clear();
    recomputePreciseJumpTargets(this, instructions, m_jumpTargets);
}

void UnlinkedCodeBlockGenerator::addOutOfLineJumpTarget(JSInstructionStream::Offset bytecodeOffset, int target)
{
    RELEASE_ASSERT(target);
    m_outOfLineJumpTargets.set(bytecodeOffset, target);
}

int UnlinkedCodeBlockGenerator::outOfLineJumpOffset(JSInstructionStream::Offset bytecodeOffset)
{
    ASSERT(m_outOfLineJumpTargets.contains(bytecodeOffset));
    return m_outOfLineJumpTargets.get(bytecodeOffset);
}

void UnlinkedCodeBlockGenerator::dump(PrintStream&) const
{
}

} // namespace JSC
