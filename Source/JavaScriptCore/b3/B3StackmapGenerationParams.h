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

#ifndef B3StackmapGenerationParams_h
#define B3StackmapGenerationParams_h

#if ENABLE(B3_JIT)

#include "AirGenerationContext.h"
#include "B3ValueRep.h"
#include "RegisterSet.h"

namespace JSC { namespace B3 {

class CheckSpecial;
class PatchpointSpecial;
class Procedure;
class StackmapValue;

class StackmapGenerationParams {
public:
    // This is the stackmap value that we're generating.
    StackmapValue* value() const { return m_value; }
    
    // This tells you the actual value representations that were chosen. This is usually different
    // from the constraints we supplied.
    const Vector<ValueRep>& reps() const { return m_reps; };

    // Usually we wish to access the reps. We make this easy by making ourselves appear to be a
    // collection of reps.
    unsigned size() const { return m_reps.size(); }
    const ValueRep& at(unsigned index) const { return m_reps[index]; }
    const ValueRep& operator[](unsigned index) const { return at(index); }
    Vector<ValueRep>::const_iterator begin() const { return m_reps.begin(); }
    Vector<ValueRep>::const_iterator end() const { return m_reps.end(); }
    
    // This tells you the registers that were used.
    const RegisterSet& usedRegisters() const;

    // This is provided for convenience; it means that you don't have to capture it if you don't want to.
    Procedure& proc() const;
    
    // The Air::GenerationContext gives you even more power.
    Air::GenerationContext& context() const { return m_context; };

    template<typename Functor>
    void addLatePath(const Functor& functor) const
    {
        context().latePaths.append(
            createSharedTask<Air::GenerationContext::LatePathFunction>(
                [=] (CCallHelpers& jit, Air::GenerationContext&) {
                    functor(jit);
                }));
    }

private:
    friend class CheckSpecial;
    friend class PatchpointSpecial;
    
    StackmapGenerationParams(StackmapValue*, const Vector<ValueRep>& reps, Air::GenerationContext&);

    StackmapValue* m_value;
    Vector<ValueRep> m_reps;
    Air::GenerationContext& m_context;
};

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

#endif // B3StackmapGenerationParams_h

