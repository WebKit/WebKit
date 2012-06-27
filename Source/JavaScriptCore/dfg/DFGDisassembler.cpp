/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "DFGDisassembler.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"

namespace JSC { namespace DFG {

Disassembler::Disassembler(Graph& graph)
    : m_graph(graph)
{
    m_labelForBlockIndex.resize(graph.m_blocks.size());
    m_labelForNodeIndex.resize(graph.size());
}

void Disassembler::dump(LinkBuffer& linkBuffer)
{
    m_graph.m_dominators.computeIfNecessary(m_graph);
    
    dataLog("Generated JIT code for DFG CodeBlock %p, instruction count = %u:\n", m_graph.m_codeBlock, m_graph.m_codeBlock->instructionCount());
    dataLog("    Code at [%p, %p):\n", linkBuffer.debugAddress(), static_cast<char*>(linkBuffer.debugAddress()) + linkBuffer.debugSize());
    
    const char* prefix = "    ";
    const char* disassemblyPrefix = "        ";
    
    NodeIndex lastNodeIndex = NoNode;
    MacroAssembler::Label previousLabel = m_startOfCode;
    for (size_t blockIndex = 0; blockIndex < m_graph.m_blocks.size(); ++blockIndex) {
        BasicBlock* block = m_graph.m_blocks[blockIndex].get();
        if (!block)
            continue;
        dumpDisassembly(disassemblyPrefix, linkBuffer, previousLabel, m_labelForBlockIndex[blockIndex], lastNodeIndex);
        m_graph.dumpBlockHeader(prefix, blockIndex, Graph::DumpLivePhisOnly);
        NodeIndex lastNodeIndexForDisassembly = block->at(0);
        for (size_t i = 0; i < block->size(); ++i) {
            if (!m_graph[block->at(i)].willHaveCodeGenOrOSR())
                continue;
            MacroAssembler::Label currentLabel;
            if (m_labelForNodeIndex[block->at(i)].isSet())
                currentLabel = m_labelForNodeIndex[block->at(i)];
            else {
                // Dump the last instruction by using the first label of the next block
                // as the end point. This case is hit either during peephole compare
                // optimizations (the Branch won't have its own label) or if we have a
                // forced OSR exit.
                if (blockIndex + 1 < m_graph.m_blocks.size())
                    currentLabel = m_labelForBlockIndex[blockIndex + 1];
                else
                    currentLabel = m_endOfMainPath;
            }
            dumpDisassembly(disassemblyPrefix, linkBuffer, previousLabel, currentLabel, lastNodeIndexForDisassembly);
            m_graph.dumpCodeOrigin(prefix, lastNodeIndex, block->at(i));
            m_graph.dump(prefix, block->at(i));
            lastNodeIndex = block->at(i);
            lastNodeIndexForDisassembly = block->at(i);
        }
    }
    dumpDisassembly(disassemblyPrefix, linkBuffer, previousLabel, m_endOfMainPath, lastNodeIndex);
    dataLog("%s(End Of Main Path)\n", prefix);
    dumpDisassembly(disassemblyPrefix, linkBuffer, previousLabel, m_endOfCode, NoNode);
}

void Disassembler::dumpDisassembly(const char* prefix, LinkBuffer& linkBuffer, MacroAssembler::Label& previousLabel, MacroAssembler::Label currentLabel, NodeIndex context)
{
    size_t prefixLength = strlen(prefix);
    int amountOfNodeWhiteSpace;
    if (context == NoNode)
        amountOfNodeWhiteSpace = 0;
    else
        amountOfNodeWhiteSpace = Graph::amountOfNodeWhiteSpace(m_graph[context]);
    OwnArrayPtr<char> prefixBuffer = adoptArrayPtr(new char[prefixLength + amountOfNodeWhiteSpace + 1]);
    strcpy(prefixBuffer.get(), prefix);
    for (int i = 0; i < amountOfNodeWhiteSpace; ++i)
        prefixBuffer[i + prefixLength] = ' ';
    prefixBuffer[prefixLength + amountOfNodeWhiteSpace] = 0;
    
    CodeLocationLabel start = linkBuffer.locationOf(previousLabel);
    CodeLocationLabel end = linkBuffer.locationOf(currentLabel);
    previousLabel = currentLabel;
    ASSERT(bitwise_cast<uintptr_t>(end.executableAddress()) >= bitwise_cast<uintptr_t>(start.executableAddress()));
    if (tryToDisassemble(start, bitwise_cast<uintptr_t>(end.executableAddress()) - bitwise_cast<uintptr_t>(start.executableAddress()), prefixBuffer.get(), WTF::dataFile()))
        return;
    
    dataLog("%s    disassembly not available for range %p...%p\n", prefixBuffer.get(), start.executableAddress(), end.executableAddress());
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
