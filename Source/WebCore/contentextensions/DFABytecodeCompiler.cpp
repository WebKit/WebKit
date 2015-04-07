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
#include "DFANode.h"

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

void DFABytecodeCompiler::emitCheckValue(uint8_t value, unsigned destinationNodeIndex, bool caseSensitive)
{
    append<DFABytecodeInstruction>(m_bytecode, caseSensitive ? DFABytecodeInstruction::CheckValueCaseSensitive : DFABytecodeInstruction::CheckValueCaseInsensitive);
    append<uint8_t>(m_bytecode, value);
    m_linkRecords.append(std::make_pair(m_bytecode.size(), destinationNodeIndex));
    append<unsigned>(m_bytecode, 0); // This value will be set when linking.
}

void DFABytecodeCompiler::emitCheckValueRange(uint8_t lowValue, uint8_t highValue, unsigned destinationNodeIndex, bool caseSensitive)
{
    ASSERT_WITH_MESSAGE(lowValue < highValue, "The instruction semantic impose lowValue is strictly less than highValue.");

    append<DFABytecodeInstruction>(m_bytecode, caseSensitive ? DFABytecodeInstruction::CheckValueRangeCaseSensitive : DFABytecodeInstruction::CheckValueRangeCaseInsensitive);
    append<uint8_t>(m_bytecode, lowValue);
    append<uint8_t>(m_bytecode, highValue);
    m_linkRecords.append(std::make_pair(m_bytecode.size(), destinationNodeIndex));
    append<unsigned>(m_bytecode, 0); // This value will be set when linking.
}

void DFABytecodeCompiler::emitTerminate()
{
    append<DFABytecodeInstruction>(m_bytecode, DFABytecodeInstruction::Terminate);
}

void DFABytecodeCompiler::compileNode(unsigned index, bool root)
{
    const DFANode& node = m_dfa.nodeAt(index);

    // Record starting index for linking.
    if (!root)
        m_nodeStartOffsets[index] = m_bytecode.size();

    for (uint64_t action : node.actions) {
        // High bits are used to store flags. See compileRuleList.
        if (action & 0xFFFF00000000)
            emitTestFlagsAndAppendAction(static_cast<uint16_t>(action >> 32), static_cast<unsigned>(action));
        else
            emitAppendAction(static_cast<unsigned>(action));
    }
    
    // If we jump to the root, we don't want to re-add its actions to a HashSet.
    // We know we have already added them because the root is always compiled first and we always start interpreting at the beginning.
    if (root)
        m_nodeStartOffsets[index] = m_bytecode.size();
    
    compileNodeTransitions(node);
}

void DFABytecodeCompiler::compileNodeTransitions(const DFANode& node)
{
    unsigned destinations[128];
    const unsigned noDestination = std::numeric_limits<unsigned>::max();
    for (uint16_t i = 0; i < 128; i++) {
        auto it = node.transitions.find(i);
        if (it == node.transitions.end())
            destinations[i] = noDestination;
        else
            destinations[i] = it->value;
    }

    Vector<Range> ranges;
    uint8_t rangeMin;
    bool hasRangeMin = false;
    for (uint8_t i = 0; i < 128; i++) {
        if (hasRangeMin) {
            ASSERT_WITH_MESSAGE(!(node.hasFallbackTransition && node.fallbackTransition == destinations[rangeMin]), "Individual transitions to the fallback transitions should have been eliminated by the optimizer.");
            if (destinations[i] != destinations[rangeMin]) {

                // This is the end of a range. Check if it can be case insensitive.
                uint8_t rangeMax = i - 1;
                bool caseSensitive = true;
                if (rangeMin >= 'A' && rangeMax <= 'Z') {
                    caseSensitive = false;
                    for (uint8_t rangeIndex = rangeMin; rangeIndex <= rangeMax; rangeIndex++) {
                        if (destinations[rangeMin] != destinations[toASCIILower(rangeIndex)]) {
                            caseSensitive = true;
                            break;
                        }
                    }
                }

                if (!caseSensitive) {
                    // If all the lower-case destinations are the same as the upper-case destinations,
                    // then they will be covered by a case-insensitive range and will not need their own range.
                    for (uint8_t rangeIndex = rangeMin; rangeIndex <= rangeMax; rangeIndex++) {
                        ASSERT(destinations[rangeMin] == destinations[toASCIILower(rangeIndex)]);
                        destinations[toASCIILower(rangeIndex)] = noDestination;
                    }
                    ranges.append(Range(toASCIILower(rangeMin), toASCIILower(rangeMax), destinations[rangeMin], caseSensitive));
                } else
                    ranges.append(Range(rangeMin, rangeMax, destinations[rangeMin], caseSensitive));

                if (destinations[i] == noDestination)
                    hasRangeMin = false;
                else
                    rangeMin = i;
            }
        } else {
            if (destinations[i] != noDestination) {
                rangeMin = i;
                hasRangeMin = true;
            }
        }
    }
    if (hasRangeMin) {
        // Ranges are appended after passing the end of them.
        // If a range goes to 127, we will have an uncommitted rangeMin because the loop does not check 128.
        // If a range goes to 127, there will never be values higher than it, so checking for case-insensitive ranges would always fail.
        ranges.append(Range(rangeMin, 127, destinations[rangeMin], true));
    }

    for (const auto& range : ranges)
        compileCheckForRange(range);
    if (node.hasFallbackTransition)
        emitJump(node.fallbackTransition);
    else
        emitTerminate();
}

void DFABytecodeCompiler::compileCheckForRange(const Range& range)
{
    ASSERT_WITH_MESSAGE(range.max < 128, "The DFA engine only supports the ASCII alphabet.");
    ASSERT(range.min <= range.max);

    if (range.min == range.max)
        emitCheckValue(range.min, range.destination, range.caseSensitive);
    else
        emitCheckValueRange(range.min, range.max, range.destination, range.caseSensitive);
}

void DFABytecodeCompiler::compile()
{
    // DFA header.
    unsigned startLocation = m_bytecode.size();
    append<unsigned>(m_bytecode, 0);
    m_nodeStartOffsets.resize(m_dfa.size());
    
    // Make sure the root is always at the beginning of the bytecode.
    compileNode(m_dfa.root(), true);
    for (unsigned i = 0; i < m_dfa.size(); i++) {
        if (i != m_dfa.root())
            compileNode(i, false);
    }

    // Link.
    for (const auto& linkRecord : m_linkRecords)
        set32Bits(m_bytecode, linkRecord.first, m_nodeStartOffsets[linkRecord.second]);
    
    // Set size header.
    set32Bits(m_bytecode, startLocation, m_bytecode.size() - startLocation);
}
    
} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
