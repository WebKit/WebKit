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
    void compileNode(uint32_t, bool root);
    void compileNodeTransitions(const DFANode&);
    void compileCheckForRange(const Range&);

    void emitAppendAction(uint64_t);
    void emitJump(uint32_t destinationNodeIndex);
    void emitCheckValue(uint8_t value, uint32_t destinationNodeIndex, bool caseSensitive);
    void emitCheckValueRange(uint8_t lowValue, uint8_t highValue, uint32_t destinationNodeIndex, bool caseSensitive);
    void emitTerminate();

    Vector<DFABytecode>& m_bytecode;
    const DFA& m_dfa;
    
    Vector<uint32_t> m_nodeStartOffsets;
    
    // The first value is the index in the bytecode buffer where the jump is to be written.
    // The second value is the index of the node to jump to.
    Vector<std::pair<uint32_t, uint32_t>> m_linkRecords;
};

} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)

#endif // DFABytecodeCompiler_h
