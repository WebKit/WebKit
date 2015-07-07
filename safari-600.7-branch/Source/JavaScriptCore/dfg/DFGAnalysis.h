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

#ifndef DFGAnalysis_h
#define DFGAnalysis_h

#if ENABLE(DFG_JIT)

namespace JSC { namespace DFG {

class Graph;

// Use this as a mixin for DFG analyses. The analysis itself implements a public
// compute(Graph&) method. Clients call computeIfNecessary() when they want
// results.

template<typename T>
class Analysis {
public:
    Analysis()
        : m_valid(false)
    {
    }
    
    void invalidate()
    {
        m_valid = false;
    }
    
    void computeIfNecessary(Graph& graph)
    {
        if (m_valid)
            return;
        // Set to true early, since the analysis may choose to call its own methods in
        // compute() and it may want to ASSERT() validity in those methods.
        m_valid = true;
        static_cast<T*>(this)->compute(graph);
    }
    
    bool isValid() const { return m_valid; }

private:
    bool m_valid;
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGAnalysis_h

