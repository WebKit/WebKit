/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef DFGNaturalLoops_h
#define DFGNaturalLoops_h

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

#include "DFGAnalysis.h"
#include "DFGBasicBlock.h"
#include "DFGCommon.h"

namespace JSC { namespace DFG {

class NaturalLoop {
public:
    NaturalLoop()
        : m_header(0)
    {
    }
    
    NaturalLoop(BasicBlock* header)
        : m_header(header)
    {
    }
    
    void addBlock(BasicBlock* block) { m_body.append(block); }
    
    BasicBlock* header() const { return m_header; }
    
    unsigned size() const { return m_body.size(); }
    BasicBlock* at(unsigned i) const { return m_body[i]; }
    BasicBlock* operator[](unsigned i) const { return at(i); }
    
    bool contains(BasicBlock* block) const
    {
        for (unsigned i = m_body.size(); i--;) {
            if (m_body[i] == block)
                return true;
        }
        ASSERT(block != header()); // Header should be contained.
        return false;
    }
    
    void dump(PrintStream&) const;
private:
    BasicBlock* m_header;
    Vector<BasicBlock*, 4> m_body;
};

class NaturalLoops : public Analysis<NaturalLoops> {
public:
    NaturalLoops();
    ~NaturalLoops();
    
    void compute(Graph&);
    
    unsigned numLoops() const
    {
        ASSERT(isValid());
        return m_loops.size();
    }
    const NaturalLoop& loop(unsigned i) const
    {
        ASSERT(isValid());
        return m_loops[i];
    }
    
    // Return either null if the block isn't a loop header, or the
    // loop it belongs to.
    const NaturalLoop* headerOf(BasicBlock* block) const
    {
        for (unsigned i = m_loops.size(); i--;) {
            if (m_loops[i].header() == block)
                return &m_loops[i];
        }
        return 0;
    }
    
    // Return the indices of all loops this belongs to.
    Vector<const NaturalLoop*> loopsOf(BasicBlock*) const;

    void dump(PrintStream&) const;
private:
    // If we ever had a scalability problem in our natural loop finder, we could
    // use some HashMap's here. But it just feels a heck of a lot less convenient.
    Vector<NaturalLoop, 4> m_loops;
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGNaturalLoops_h

