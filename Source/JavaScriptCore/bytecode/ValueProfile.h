/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ValueProfile_h
#define ValueProfile_h

#include "WriteBarrier.h"

namespace JSC {

#if ENABLE(VALUE_PROFILER)
struct ValueProfile {
    static const unsigned logNumberOfBuckets = 3; // 8 buckets
    static const unsigned numberOfBuckets = 1 << logNumberOfBuckets;
    static const unsigned bucketIndexMask = numberOfBuckets - 1;
    static const unsigned certainty = numberOfBuckets * numberOfBuckets;
    static const unsigned majority = certainty / 2;
    
    ValueProfile(int bytecodeOffset)
        : bytecodeOffset(bytecodeOffset)
    {
        for (unsigned i = 0; i < numberOfBuckets; ++i)
            buckets[i].setWithoutWriteBarrier(JSValue());
    }
    
    unsigned numberOfSamples() const
    {
        unsigned result = 0;
        for (unsigned i = 0; i < numberOfBuckets; ++i) {
            if (!!buckets[i])
                result++;
        }
        return result;
    }
    
    static unsigned computeProbability(unsigned counts, unsigned numberOfSamples)
    {
        if (!numberOfSamples)
            return 0;
        return counts * certainty / numberOfSamples;
    }
        
    unsigned numberOfInt32s() const
    {
        unsigned result = 0;
        for (unsigned i = 0; i < numberOfBuckets; ++i) {
            if (!!buckets[i] && buckets[i].get().isInt32())
                result++;
        }
        return result;
    }
        
    unsigned numberOfDoubles() const
    {
        unsigned result = 0;
        for (unsigned i = 0; i < numberOfBuckets; ++i) {
            if (!!buckets[i] && buckets[i].get().isDouble())
                result++;
        }
        return result;
    }
        
    unsigned numberOfCells() const
    {
        unsigned result = 0;
        for (unsigned i = 0; i < numberOfBuckets; ++i) {
            if (!!buckets[i] && buckets[i].get().isCell())
                result++;
        }
        return result;
    }
        
    // These methods are not particularly optimized, in that they will each
    // perform two passes over the buckets array. However, they are
    // probably the best bet unless you are sure that you will be making
    // these calls with high frequency.
        
    unsigned probabilityOfInt32() const
    {
        return computeProbability(numberOfInt32s(), numberOfSamples());
    }
        
    unsigned probabilityOfDouble() const
    {
        return computeProbability(numberOfDoubles(), numberOfSamples());
    }
    
    unsigned probabilityOfCell() const
    {
        return computeProbability(numberOfCells(), numberOfSamples());
    }
    
    int bytecodeOffset; // -1 for prologue
    WriteBarrierBase<Unknown> buckets[numberOfBuckets];
};

inline int getValueProfileBytecodeOffset(ValueProfile* valueProfile)
{
    return valueProfile->bytecodeOffset;
}
#endif

}

#endif

