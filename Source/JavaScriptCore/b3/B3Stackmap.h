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

#ifndef B3Stackmap_h
#define B3Stackmap_h

#if ENABLE(B3_JIT)

#include "B3ValueRep.h"
#include "CCallHelpers.h"
#include "RegisterSet.h"
#include <wtf/SharedTask.h>

namespace JSC { namespace B3 {

class CheckSpecial;
class StackmapSpecial;
class Value;

class Stackmap {
public:
    struct GenerationParams {
        // This is the Value containing the stackmap.
        Value* value;

        // This is the stackmap.
        Stackmap* stackmap;

        // This tells you the actual value representations that were chosen.
        Vector<ValueRep> reps;

        // This tells you the registers that were used.
        RegisterSet usedRegisters;
    };
    
    typedef void GeneratorFunction(CCallHelpers&, const GenerationParams&);
    typedef SharedTask<GeneratorFunction> Generator;
    
    JS_EXPORT_PRIVATE Stackmap();
    ~Stackmap();

    // Constrain an argument to the Value that uses this Stackmap. In case of a Patchpoint that
    // returns a value, the first argument is the result value. In all other cases, index zero refers
    // to the first argument to that Value, even if that argument is not constrainable. For example,
    // in a CheckAdd value, it would be an error to constrain indices 0 and 1. In a Check value, it
    // would be an error to constrain index 0. But, when the generation callback is called, you can
    // depend on the reps for those indices being filled in.
    void constrain(unsigned index, const ValueRep& rep)
    {
        if (index + 1 >= m_reps.size())
            m_reps.grow(index + 1);
        m_reps[index] = rep;
    }

    void appendConstraint(const ValueRep& rep)
    {
        m_reps.append(rep);
    }

    const Vector<ValueRep>& reps() const { return m_reps; }

    void clobber(const RegisterSet& set)
    {
        m_clobbered.merge(set);
    }

    const RegisterSet& clobbered() const { return m_clobbered; }

    void setGenerator(RefPtr<Generator> generator)
    {
        m_generator = generator;
    }

    template<typename Functor>
    void setGenerator(const Functor& functor)
    {
        m_generator = createSharedTask<GeneratorFunction>(functor);
    }

    void dump(PrintStream&) const;
    
private:
    friend class CheckSpecial;
    friend class PatchpointSpecial;
    friend class StackmapSpecial;
    
    Vector<ValueRep> m_reps;
    RefPtr<Generator> m_generator;
    RegisterSet m_clobbered;
    RegisterSet m_usedRegisters; // Stackmaps could be further duplicated by Air, but that's unlikely, so we just merge the used registers sets if that were to happen.
};

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

#endif // B3Stackmap_h

