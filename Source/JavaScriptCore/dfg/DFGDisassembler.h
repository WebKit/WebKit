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

#ifndef DFGDisassembler_h
#define DFGDisassembler_h

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

#include "DFGCommon.h"
#include "LinkBuffer.h"
#include "MacroAssembler.h"
#include <wtf/Vector.h>

namespace JSC { namespace DFG {

class Graph;

class Disassembler {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Disassembler(Graph&);
    
    void setStartOfCode(MacroAssembler::Label label) { m_startOfCode = label; }
    void setForBlock(BlockIndex blockIndex, MacroAssembler::Label label)
    {
        m_labelForBlockIndex[blockIndex] = label;
    }
    void setForNode(NodeIndex nodeIndex, MacroAssembler::Label label)
    {
        m_labelForNodeIndex[nodeIndex] = label;
    }
    void setEndOfMainPath(MacroAssembler::Label label)
    {
        m_endOfMainPath = label;
    }
    void setEndOfCode(MacroAssembler::Label label)
    {
        m_endOfCode = label;
    }
    
    void dump(LinkBuffer&);
    
private:
    void dumpDisassembly(const char* prefix, LinkBuffer&, MacroAssembler::Label& previousLabel, MacroAssembler::Label currentLabel, NodeIndex context);
    
    Graph& m_graph;
    MacroAssembler::Label m_startOfCode;
    Vector<MacroAssembler::Label> m_labelForBlockIndex;
    Vector<MacroAssembler::Label> m_labelForNodeIndex;
    MacroAssembler::Label m_endOfMainPath;
    MacroAssembler::Label m_endOfCode;
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGDisassembler_h
