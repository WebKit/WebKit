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

#ifndef DFABytecodeCompiler_h
#define DFABytecodeCompiler_h

#if ENABLE(CONTENT_EXTENSIONS)

#include "DFABytecode.h"
#include <wtf/Vector.h>

namespace WebCore {

namespace ContentExtensions {

struct DFA;
class DFANode;

class WEBCORE_EXPORT DFABytecodeCompiler {
public:
    DFABytecodeCompiler(const DFA& dfa, Vector<DFABytecode>& bytecode)
        : m_bytecode(bytecode)
        , m_dfa(dfa)
    {
    }
    
    void compile();

private:
    struct Range {
        Range(uint8_t min, uint8_t max, uint32_t destination, bool caseSensitive)
            : min(min)
            , max(max)
            , destination(destination)
            , caseSensitive(caseSensitive)
        {
        }
        uint8_t min;
        uint8_t max;
        uint32_t destination;
        bool caseSensitive;
    };
    Vector<Range> ranges(const DFANode&);
    
    unsigned compiledNodeMaxBytecodeSize(uint32_t index);
    void compileNode(uint32_t index, bool root);
    unsigned nodeTransitionsMaxBytecodeSize(const DFANode&);
    void compileNodeTransitions(uint32_t nodeIndex);
    unsigned checkForRangeMaxBytecodeSize(const Range&);
    void compileCheckForRange(uint32_t nodeIndex, const Range&);
    int32_t longestPossibleJump(uint32_t jumpLocation, uint32_t sourceNodeIndex, uint32_t destinationNodeIndex);

    void emitAppendAction(uint64_t);
    void emitJump(uint32_t sourceNodeIndex, uint32_t destinationNodeIndex);
    void emitCheckValue(uint8_t value, uint32_t sourceNodeIndex, uint32_t destinationNodeIndex, bool caseSensitive);
    void emitCheckValueRange(uint8_t lowValue, uint8_t highValue, uint32_t sourceNodeIndex, uint32_t destinationNodeIndex, bool caseSensitive);
    void emitTerminate();

    Vector<DFABytecode>& m_bytecode;
    const DFA& m_dfa;
    
    Vector<uint32_t> m_maxNodeStartOffsets;
    Vector<uint32_t> m_nodeStartOffsets;
    
    struct LinkRecord {
        DFABytecodeJumpSize jumpSize;
        int32_t longestPossibleJump;
        uint32_t instructionLocation;
        uint32_t jumpLocation;
        uint32_t destinationNodeIndex;
    };
    Vector<LinkRecord> m_linkRecords;
};

} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)

#endif // DFABytecodeCompiler_h
