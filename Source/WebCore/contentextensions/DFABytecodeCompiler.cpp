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
#include "DFABytecodeCompiler.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "ContentExtensionRule.h"
#include "DFA.h"

namespace WebCore {
    
namespace ContentExtensions {

template <typename IntType>
inline void append(Vector<DFABytecode>& bytecode, IntType value)
{
    bytecode.resize(bytecode.size() + sizeof(IntType));
    *reinterpret_cast<IntType*>(&bytecode[bytecode.size() - sizeof(IntType)]) = value;
}

inline void set32Bits(Vector<DFABytecode>& bytecode, unsigned index, unsigned value)
{
    *reinterpret_cast<unsigned*>(&bytecode[index]) = value;
}

void DFABytecodeCompiler::emitAppendAction(unsigned action)
{
    append<DFABytecodeInstruction>(m_bytecode, DFABytecodeInstruction::AppendAction);
    append<unsigned>(m_bytecode, action);
}

void DFABytecodeCompiler::emitTestFlagsAndAppendAction(uint16_t flags, unsigned action)
{
    ASSERT(flags);
    append<DFABytecodeInstruction>(m_bytecode, DFABytecodeInstruction::TestFlagsAndAppendAction);
    append<uint16_t>(m_bytecode, flags);
    append<unsigned>(m_bytecode, action);
}

void DFABytecodeCompiler::emitJump(unsigned destinationNodeIndex)
{
    append<DFABytecodeInstruction>(m_bytecode, DFABytecodeInstruction::Jump);
    m_linkRecords.append(std::make_pair(m_bytecode.size(), destinationNodeIndex));
    append<unsigned>(m_bytecode, 0); // This value will be set when linking.
}

void DFABytecodeCompiler::emitCheckValue(uint8_t value, unsigned destinationNodeIndex)
{
    append<DFABytecodeInstruction>(m_bytecode, DFABytecodeInstruction::CheckValue);
    append<uint8_t>(m_bytecode, value);
    m_linkRecords.append(std::make_pair(m_bytecode.size(), destinationNodeIndex));
    append<unsigned>(m_bytecode, 0); // This value will be set when linking.
}

void DFABytecodeCompiler::emitTerminate()
{
    append<DFABytecodeInstruction>(m_bytecode, DFABytecodeInstruction::Terminate);
}

void DFABytecodeCompiler::compileNode(unsigned index)
{
    const DFANode& node = m_dfa.nodeAt(index);

    // Record starting index for linking.
    m_nodeStartOffsets[index] = m_bytecode.size();

    for (uint64_t action : node.actions) {
        // High bits are used to store flags. See compileRuleList.
        if (action & 0xFFFF00000000)
            emitTestFlagsAndAppendAction(static_cast<uint16_t>(action >> 32), static_cast<unsigned>(action));
        else
            emitAppendAction(static_cast<unsigned>(action));
    }
    
    for (const auto& transition : node.transitions)
        emitCheckValue(transition.key, transition.value);
    
    if (node.hasFallbackTransition)
        emitJump(node.fallbackTransition);
    else
        emitTerminate();
}
    
void DFABytecodeCompiler::compile()
{
    ASSERT(!m_bytecode.size());
    m_nodeStartOffsets.resize(m_dfa.size());
    
    // Make sure the root is always at the beginning of the bytecode.
    compileNode(m_dfa.root());
    for (unsigned i = 0; i < m_dfa.size(); i++) {
        if (i != m_dfa.root())
            compileNode(i);
    }

    // Link.
    for (const auto& linkRecord : m_linkRecords)
        set32Bits(m_bytecode, linkRecord.first, m_nodeStartOffsets[linkRecord.second]);
}
    
} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
