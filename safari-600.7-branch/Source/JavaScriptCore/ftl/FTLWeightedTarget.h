/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef FTLWeightedTarget_h
#define FTLWeightedTarget_h

#if ENABLE(FTL_JIT)

#include "FTLAbbreviatedTypes.h"
#include "FTLWeight.h"

namespace JSC { namespace FTL {

class WeightedTarget {
public:
    WeightedTarget()
        : m_target(nullptr)
    {
    }
    
    WeightedTarget(LBasicBlock target, Weight weight)
        : m_target(target)
        , m_weight(weight)
    {
    }
    
    WeightedTarget(LBasicBlock target, float weight)
        : m_target(target)
        , m_weight(weight)
    {
    }
    
    LBasicBlock target() const { return m_target; }
    Weight weight() const { return m_weight; }
    
private:
    LBasicBlock m_target;
    Weight m_weight;
};

// Helpers for creating weighted targets for statically known (or unknown) branch
// profiles.

inline WeightedTarget usually(LBasicBlock block)
{
    return WeightedTarget(block, 1);
}

inline WeightedTarget rarely(LBasicBlock block)
{
    return WeightedTarget(block, 0);
}

// This means we let LLVM figure it out basic on its static estimates. LLVM's static
// estimates are usually pretty darn good, so there's generally nothing wrong with
// using this.
inline WeightedTarget unsure(LBasicBlock block)
{
    return WeightedTarget(block, Weight());
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

#endif // FTLWeightedTarget_h

