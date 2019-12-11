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
    bytecode.grow(bytecode.size() + sizeof(IntType));
    *reinterpret_cast<IntType*>(&bytecode[bytecode.size() - sizeof(IntType)]) = value;
}

inline void appendZeroes(Vector<DFABytecode>& bytecode, DFABytecodeJumpSize jumpSize)
{
    switch (jumpSize) {
    case DFABytecodeJumpSize::Int8:
        append<int8_t>(bytecode, 0); // This value will be set when linking.
        break;
    case DFABytecodeJumpSize::Int16:
        append<int16_t>(bytecode, 0); // This value will be set when linking.
        break;
    case DFABytecodeJumpSize::Int24:
        append<uint16_t>(bytecode, 0);
        append<int8_t>(bytecode, 0); // These values will be set when linking.
        break;
    case DFABytecodeJumpSize::Int32:
        append<int32_t>(bytecode, 0); // This value will be set when linking.
        break;
    }
}

template <typename IntType>
inline void setBits(Vector<DFABytecode>& bytecode, uint32_t index, IntType value)
{
    RELEASE_ASSERT(index + sizeof(IntType) <= bytecode.size());
    ASSERT_WITH_MESSAGE(!*reinterpret_cast<IntType*>(&bytecode[index]), "Right now we should only be using setBits to overwrite values that were zero as a placeholder.");
    *reinterpret_cast<IntType*>(&bytecode[index]) = value;
}

static unsigned appendActionBytecodeSize(uint64_t action)
{
    if (action & ActionFlagMask)
        return sizeof(DFABytecodeInstruction) + sizeof(uint16_t) + sizeof(uint32_t);
    return sizeof(DFABytecodeInstruction) + sizeof(uint32_t);
}
    
void DFABytecodeCompiler::emitAppendAction(uint64_t action)
{
    // High bits are used to store flags. See compileRuleList.
    if (action & ActionFlagMask) {
        if (action & IfConditionFlag)
            append<DFABytecodeInstruction>(m_bytecode, DFABytecodeInstruction::TestFlagsAndAppendActionWithIfCondition);
        else
            append<DFABytecodeInstruction>(m_bytecode, DFABytecodeInstruction::TestFlagsAndAppendAction);
        append<uint16_t>(m_bytecode, static_cast<uint16_t>(action >> 32));
        append<uint32_t>(m_bytecode, static_cast<uint32_t>(action));
    } else {
        if (action & IfConditionFlag)
            append<DFABytecodeInstruction>(m_bytecode, DFABytecodeInstruction::AppendActionWithIfCondition);
        else
            append<DFABytecodeInstruction>(m_bytecode, DFABytecodeInstruction::AppendAction);
        append<uint32_t>(m_bytecode, static_cast<uint32_t>(action));
    }
}

int32_t DFABytecodeCompiler::longestPossibleJump(uint32_t instructionLocation, uint32_t sourceNodeIndex, uint32_t destinationNodeIndex)
{
    if (m_nodeStartOffsets[destinationNodeIndex] == std::numeric_limits<uint32_t>::max()) {
        // Jumping to a node that hasn't been compiled yet, we don't know exactly how far forward we will need to jump,
        // so make sure we have enough room for the worst possible case, the farthest possible jump
        // which would be the distance if there were no compacted branches between this jump and its destination.
        ASSERT(instructionLocation >= m_nodeStartOffsets[sourceNodeIndex]);
        ASSERT(m_maxNodeStartOffsets[destinationNodeIndex] > m_maxNodeStartOffsets[sourceNodeIndex]);
        ASSERT(m_nodeStartOffsets[sourceNodeIndex] != std::numeric_limits<uint32_t>::max());
        return m_maxNodeStartOffsets[destinationNodeIndex] - m_maxNodeStartOffsets[sourceNodeIndex] - (m_nodeStartOffsets[sourceNodeIndex] - instructionLocation);
    }
    
    // Jumping to an already compiled node, we already know exactly where we will need to jump to.
    ASSERT(m_nodeStartOffsets[destinationNodeIndex] <= instructionLocation);
    return m_nodeStartOffsets[destinationNodeIndex] - instructionLocation;
}
    
void DFABytecodeCompiler::emitJump(uint32_t sourceNodeIndex, uint32_t destinationNodeIndex)
{
    uint32_t instructionLocation = m_bytecode.size();
    uint32_t jumpLocation = instructionLocation + sizeof(uint8_t);
    int32_t longestPossibleJumpDistance = longestPossibleJump(instructionLocation, sourceNodeIndex, destinationNodeIndex);
    DFABytecodeJumpSize jumpSize = smallestPossibleJumpSize(longestPossibleJumpDistance);
    append<uint8_t>(m_bytecode, static_cast<uint8_t>(DFABytecodeInstruction::Jump) | jumpSize);

    m_linkRecords.append(LinkRecord({jumpSize, longestPossibleJumpDistance, instructionLocation, jumpLocation, destinationNodeIndex}));
    appendZeroes(m_bytecode, jumpSize);
}

void DFABytecodeCompiler::emitCheckValue(uint8_t value, uint32_t sourceNodeIndex, uint32_t destinationNodeIndex, bool caseSensitive)
{
    uint32_t instructionLocation = m_bytecode.size();
    uint32_t jumpLocation = instructionLocation + 2 * sizeof(uint8_t);
    int32_t longestPossibleJumpDistance = longestPossibleJump(instructionLocation, sourceNodeIndex, destinationNodeIndex);
    DFABytecodeJumpSize jumpSize = smallestPossibleJumpSize(longestPossibleJumpDistance);
    DFABytecodeInstruction instruction = caseSensitive ? DFABytecodeInstruction::CheckValueCaseSensitive : DFABytecodeInstruction::CheckValueCaseInsensitive;
    append<uint8_t>(m_bytecode, static_cast<uint8_t>(instruction) | jumpSize);
    append<uint8_t>(m_bytecode, value);
    m_linkRecords.append(LinkRecord({jumpSize, longestPossibleJumpDistance, instructionLocation, jumpLocation, destinationNodeIndex}));
    appendZeroes(m_bytecode, jumpSize);
}

void DFABytecodeCompiler::emitCheckValueRange(uint8_t lowValue, uint8_t highValue, uint32_t sourceNodeIndex, uint32_t destinationNodeIndex, bool caseSensitive)
{
    ASSERT_WITH_MESSAGE(lowValue < highValue, "The instruction semantic impose lowValue is strictly less than highValue.");

    uint32_t instructionLocation = m_bytecode.size();
    uint32_t jumpLocation = instructionLocation + 3 * sizeof(uint8_t);
    int32_t longestPossibleJumpDistance = longestPossibleJump(instructionLocation, sourceNodeIndex, destinationNodeIndex);
    DFABytecodeJumpSize jumpSize = smallestPossibleJumpSize(longestPossibleJumpDistance);
    DFABytecodeInstruction instruction = caseSensitive ? DFABytecodeInstruction::CheckValueRangeCaseSensitive : DFABytecodeInstruction::CheckValueRangeCaseInsensitive;
    append<uint8_t>(m_bytecode, static_cast<uint8_t>(instruction) | jumpSize);
    append<uint8_t>(m_bytecode, lowValue);
    append<uint8_t>(m_bytecode, highValue);
    m_linkRecords.append(LinkRecord({jumpSize, longestPossibleJumpDistance, instructionLocation, jumpLocation, destinationNodeIndex}));
    appendZeroes(m_bytecode, jumpSize);
}

void DFABytecodeCompiler::emitTerminate()
{
    append<DFABytecodeInstruction>(m_bytecode, DFABytecodeInstruction::Terminate);
}

void DFABytecodeCompiler::compileNode(uint32_t index, bool root)
{
    unsigned startSize = m_bytecode.size();
    
    const DFANode& node = m_dfa.nodes[index];
    if (node.isKilled()) {
        ASSERT(m_nodeStartOffsets[index] == std::numeric_limits<uint32_t>::max());
        return;
    }

    // Record starting index for linking.
    if (!root)
        m_nodeStartOffsets[index] = m_bytecode.size();

    for (uint64_t action : node.actions(m_dfa))
        emitAppendAction(action);
    
    // If we jump to the root, we don't want to re-add its actions to a HashSet.
    // We know we have already added them because the root is always compiled first and we always start interpreting at the beginning.
    if (root)
        m_nodeStartOffsets[index] = m_bytecode.size();
    
    compileNodeTransitions(index);
    
    ASSERT_UNUSED(startSize, m_bytecode.size() - startSize <= compiledNodeMaxBytecodeSize(index));
}
    
unsigned DFABytecodeCompiler::compiledNodeMaxBytecodeSize(uint32_t index)
{
    const DFANode& node = m_dfa.nodes[index];
    if (node.isKilled())
        return 0;
    unsigned size = 0;
    for (uint64_t action : node.actions(m_dfa))
        size += appendActionBytecodeSize(action);
    size += nodeTransitionsMaxBytecodeSize(node);
    return size;
}

DFABytecodeCompiler::JumpTable DFABytecodeCompiler::extractJumpTable(Vector<DFABytecodeCompiler::Range>& ranges, unsigned firstRange, unsigned lastRange)
{
    ASSERT(lastRange > firstRange);
    ASSERT(lastRange < ranges.size());

    JumpTable jumpTable;
    jumpTable.min = ranges[firstRange].min;
    jumpTable.max = ranges[lastRange].max;
    jumpTable.caseSensitive = ranges[lastRange].caseSensitive;

    unsigned size = lastRange - firstRange + 1;
    jumpTable.destinations.reserveInitialCapacity(size);
    for (unsigned i = firstRange; i <= lastRange; ++i) {
        const Range& range = ranges[i];

        ASSERT(range.caseSensitive == jumpTable.caseSensitive);
        ASSERT(range.min == range.max);
        ASSERT(range.min >= jumpTable.min);
        ASSERT(range.min <= jumpTable.max);

        jumpTable.destinations.uncheckedAppend(range.destination);
    }

    ranges.remove(firstRange, size);

    return jumpTable;
}

DFABytecodeCompiler::Transitions DFABytecodeCompiler::transitions(const DFANode& node)
{
    Transitions transitions;

    uint32_t destinations[128];
    memset(destinations, 0xff, sizeof(destinations));
    const uint32_t noDestination = std::numeric_limits<uint32_t>::max();

    transitions.useFallbackTransition = node.canUseFallbackTransition(m_dfa);
    if (transitions.useFallbackTransition)
        transitions.fallbackTransitionTarget = node.bestFallbackTarget(m_dfa);

    for (const auto& transition : node.transitions(m_dfa)) {
        uint32_t targetNodeIndex = transition.target();
        if (transitions.useFallbackTransition && transitions.fallbackTransitionTarget == targetNodeIndex)
            continue;

        for (uint16_t i = transition.range().first; i <= transition.range().last; ++i)
            destinations[i] = targetNodeIndex;
    }

    Vector<Range>& ranges = transitions.ranges;
    uint8_t rangeMin;
    bool hasRangeMin = false;
    for (uint8_t i = 0; i < 128; i++) {
        if (hasRangeMin) {
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

    Vector<JumpTable>& jumpTables = transitions.jumpTables;
    unsigned rangePosition = 0;
    unsigned baseRangePosition = std::numeric_limits<unsigned>::max();
    Range* baseRange = nullptr;
    while (rangePosition < ranges.size()) {
        auto& range = ranges[rangePosition];
        if (baseRange) {
            if (range.min != range.max
                || baseRange->caseSensitive != range.caseSensitive
                || ranges[rangePosition - 1].max + 1 != range.min) {
                if (rangePosition - baseRangePosition > 1) {
                    jumpTables.append(extractJumpTable(ranges, baseRangePosition, rangePosition - 1));
                    rangePosition = baseRangePosition;
                }
                baseRangePosition = std::numeric_limits<unsigned>::max();
                baseRange = nullptr;
            }
        } else {
            if (range.min == range.max) {
                baseRangePosition = rangePosition;
                baseRange = &range;
            }
        }
        ++rangePosition;
    }

    if (baseRange && ranges.size() - baseRangePosition > 1)
        jumpTables.append(extractJumpTable(ranges, baseRangePosition, ranges.size() - 1));

    return transitions;
}

unsigned DFABytecodeCompiler::checkForJumpTableMaxBytecodeSize(const JumpTable& jumpTable)
{
    unsigned baselineSize = sizeof(DFABytecodeInstruction::CheckValueRangeCaseInsensitive) + 2 * sizeof(uint8_t);
    unsigned targetsSize = (jumpTable.max - jumpTable.min + 1) * sizeof(uint32_t);
    return baselineSize + targetsSize;
}
    
unsigned DFABytecodeCompiler::checkForRangeMaxBytecodeSize(const Range& range)
{
    if (range.min == range.max)
        return sizeof(DFABytecodeInstruction::CheckValueCaseInsensitive) + sizeof(uint8_t) + sizeof(uint32_t);
    return sizeof(DFABytecodeInstruction::CheckValueRangeCaseInsensitive) + 2 * sizeof(uint8_t) + sizeof(uint32_t);
}

void DFABytecodeCompiler::compileJumpTable(uint32_t nodeIndex, const JumpTable& jumpTable)
{
    unsigned startSize = m_bytecode.size();
    ASSERT_WITH_MESSAGE(jumpTable.max < 128, "The DFA engine only supports the ASCII alphabet.");
    ASSERT(jumpTable.min <= jumpTable.max);

    uint32_t instructionLocation = m_bytecode.size();
    DFABytecodeJumpSize jumpSize = Int8;
    for (uint32_t destinationNodeIndex : jumpTable.destinations) {
        int32_t longestPossibleJumpDistance = longestPossibleJump(instructionLocation, nodeIndex, destinationNodeIndex);
        DFABytecodeJumpSize localJumpSize = smallestPossibleJumpSize(longestPossibleJumpDistance);
        jumpSize = std::max(jumpSize, localJumpSize);
    }

    DFABytecodeInstruction instruction = jumpTable.caseSensitive ? DFABytecodeInstruction::JumpTableCaseSensitive : DFABytecodeInstruction::JumpTableCaseInsensitive;
    append<uint8_t>(m_bytecode, static_cast<uint8_t>(instruction) | jumpSize);
    append<uint8_t>(m_bytecode, jumpTable.min);
    append<uint8_t>(m_bytecode, jumpTable.max);

    for (uint32_t destinationNodeIndex : jumpTable.destinations) {
        int32_t longestPossibleJumpDistance = longestPossibleJump(instructionLocation, nodeIndex, destinationNodeIndex);
        uint32_t jumpLocation = m_bytecode.size();
        m_linkRecords.append(LinkRecord({jumpSize, longestPossibleJumpDistance, instructionLocation, jumpLocation, destinationNodeIndex}));
        appendZeroes(m_bytecode, jumpSize);
    }

    ASSERT_UNUSED(startSize, m_bytecode.size() - startSize <= checkForJumpTableMaxBytecodeSize(jumpTable));
}

void DFABytecodeCompiler::compileCheckForRange(uint32_t nodeIndex, const Range& range)
{
    unsigned startSize = m_bytecode.size();
    ASSERT_WITH_MESSAGE(range.max < 128, "The DFA engine only supports the ASCII alphabet.");
    ASSERT(range.min <= range.max);

    if (range.min == range.max)
        emitCheckValue(range.min, nodeIndex, range.destination, range.caseSensitive);
    else
        emitCheckValueRange(range.min, range.max, nodeIndex, range.destination, range.caseSensitive);
    
    ASSERT_UNUSED(startSize, m_bytecode.size() - startSize <= checkForRangeMaxBytecodeSize(range));
}

unsigned DFABytecodeCompiler::nodeTransitionsMaxBytecodeSize(const DFANode& node)
{
    unsigned size = 0;
    Transitions nodeTransitions = transitions(node);
    for (const auto& jumpTable : nodeTransitions.jumpTables)
        size += checkForJumpTableMaxBytecodeSize(jumpTable);
    for (const auto& range : nodeTransitions.ranges)
        size += checkForRangeMaxBytecodeSize(range);
    if (nodeTransitions.useFallbackTransition)
        size += sizeof(DFABytecodeInstruction::Jump) + sizeof(uint32_t);
    else
        size += instructionSizeWithArguments(DFABytecodeInstruction::Terminate);
    return size;
}

void DFABytecodeCompiler::compileNodeTransitions(uint32_t nodeIndex)
{
    const DFANode& node = m_dfa.nodes[nodeIndex];
    unsigned startSize = m_bytecode.size();

    Transitions nodeTransitions = transitions(node);
    for (const auto& jumpTable : nodeTransitions.jumpTables)
        compileJumpTable(nodeIndex, jumpTable);
    for (const auto& range : nodeTransitions.ranges)
        compileCheckForRange(nodeIndex, range);
    if (nodeTransitions.useFallbackTransition)
        emitJump(nodeIndex, nodeTransitions.fallbackTransitionTarget);
    else
        emitTerminate();

    ASSERT_UNUSED(startSize, m_bytecode.size() - startSize <= nodeTransitionsMaxBytecodeSize(node));
}

void DFABytecodeCompiler::compile()
{
    uint32_t startLocation = m_bytecode.size();
    append<DFAHeader>(m_bytecode, 0); // This will be set when we are finished compiling this DFA.

    m_nodeStartOffsets.resize(m_dfa.nodes.size());
    for (unsigned i = 0; i < m_dfa.nodes.size(); ++i)
        m_nodeStartOffsets[i] = std::numeric_limits<uint32_t>::max();
    
    // Populate m_maxNodeStartOffsets with a worst-case index of where the node would be with no branch compaction.
    // Compacting the branches using 1-4 byte signed jump distances should only make nodes closer together than this.
    ASSERT(m_maxNodeStartOffsets.isEmpty());
    m_maxNodeStartOffsets.clear();
    m_maxNodeStartOffsets.resize(m_dfa.nodes.size());
    unsigned rootActionsSize = 0;
    for (uint64_t action : m_dfa.nodes[m_dfa.root].actions(m_dfa))
        rootActionsSize += appendActionBytecodeSize(action);
    m_maxNodeStartOffsets[m_dfa.root] = sizeof(DFAHeader) + rootActionsSize;
    unsigned nextIndex = sizeof(DFAHeader) + compiledNodeMaxBytecodeSize(m_dfa.root);
    for (uint32_t i = 0; i < m_dfa.nodes.size(); i++) {
        if (i != m_dfa.root) {
            m_maxNodeStartOffsets[i] = nextIndex;
            nextIndex += compiledNodeMaxBytecodeSize(i);
        }
    }
    
    // Make sure the root is always at the beginning of the bytecode.
    compileNode(m_dfa.root, true);
    for (uint32_t i = 0; i < m_dfa.nodes.size(); i++) {
        if (i != m_dfa.root)
            compileNode(i, false);
    }
    
    ASSERT(m_maxNodeStartOffsets.size() == m_nodeStartOffsets.size());
    for (unsigned i = 0; i < m_dfa.nodes.size(); ++i) {
        if (m_nodeStartOffsets[i] != std::numeric_limits<uint32_t>::max())
            ASSERT(m_maxNodeStartOffsets[i] >= m_nodeStartOffsets[i]);
    }

    // Link.
    for (const auto& linkRecord : m_linkRecords) {
        uint32_t destination = m_nodeStartOffsets[linkRecord.destinationNodeIndex];
        RELEASE_ASSERT(destination < static_cast<uint32_t>(std::numeric_limits<int32_t>::max()));
        int32_t distance = destination - linkRecord.instructionLocation;
        ASSERT(abs(distance) <= abs(linkRecord.longestPossibleJump));
        
        switch (linkRecord.jumpSize) {
        case Int8:
            RELEASE_ASSERT(distance == static_cast<int8_t>(distance));
            setBits<int8_t>(m_bytecode, linkRecord.jumpLocation, static_cast<int8_t>(distance));
            break;
        case Int16:
            RELEASE_ASSERT(distance == static_cast<int16_t>(distance));
            setBits<int16_t>(m_bytecode, linkRecord.jumpLocation, static_cast<int16_t>(distance));
            break;
        case Int24:
            RELEASE_ASSERT(distance >= Int24Min && distance <= Int24Max);
            setBits<uint16_t>(m_bytecode, linkRecord.jumpLocation, static_cast<uint16_t>(distance));
            setBits<int8_t>(m_bytecode, linkRecord.jumpLocation + sizeof(int16_t), static_cast<int8_t>(distance >> 16));
            break;
        case Int32:
            setBits<int32_t>(m_bytecode, linkRecord.jumpLocation, distance);
            break;
        }
    }
    
    setBits<DFAHeader>(m_bytecode, startLocation, m_bytecode.size() - startLocation);
}
    
} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
