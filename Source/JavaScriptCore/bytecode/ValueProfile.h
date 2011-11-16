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

#include "JSArray.h"
#include "PredictedType.h"
#include "Structure.h"
#include "WriteBarrier.h"

namespace JSC {

#if ENABLE(VALUE_PROFILER)
struct ValueProfile {
    static const unsigned logNumberOfBuckets = 0; // 1 bucket
    static const unsigned numberOfBuckets = 1 << logNumberOfBuckets;
    static const unsigned numberOfSpecFailBuckets = 1;
    static const unsigned bucketIndexMask = numberOfBuckets - 1;
    static const unsigned totalNumberOfBuckets = numberOfBuckets + numberOfSpecFailBuckets;
    static const unsigned certainty = totalNumberOfBuckets * totalNumberOfBuckets;
    static const unsigned majority = certainty / 2;
    
    ValueProfile(int bytecodeOffset)
        : m_bytecodeOffset(bytecodeOffset)
        , m_prediction(PredictNone)
        , m_numberOfSamplesInPrediction(0)
    {
        for (unsigned i = 0; i < totalNumberOfBuckets; ++i)
            m_buckets[i] = JSValue::encode(JSValue());
    }
    
    EncodedJSValue* specFailBucket(unsigned i)
    {
        ASSERT(numberOfBuckets + i < totalNumberOfBuckets);
        return m_buckets + numberOfBuckets + i;
    }
    
    const ClassInfo* classInfo(unsigned bucket) const
    {
        JSValue value = JSValue::decode(m_buckets[bucket]);
        if (!!value) {
            if (!value.isCell())
                return 0;
            return value.asCell()->structure()->classInfo();
        }
        return 0;
    }
    
    unsigned numberOfSamples() const
    {
        unsigned result = 0;
        for (unsigned i = 0; i < totalNumberOfBuckets; ++i) {
            if (!!JSValue::decode(m_buckets[i]))
                result++;
        }
        return result;
    }
    
    unsigned totalNumberOfSamples() const
    {
        return numberOfSamples() + m_numberOfSamplesInPrediction;
    }
    
    bool isLive() const
    {
        for (unsigned i = 0; i < totalNumberOfBuckets; ++i) {
            if (!!JSValue::decode(m_buckets[i]))
                return true;
        }
        return false;
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
        for (unsigned i = 0; i < totalNumberOfBuckets; ++i) {
            if (JSValue::decode(m_buckets[i]).isInt32())
                result++;
        }
        return result;
    }
        
    unsigned numberOfDoubles() const
    {
        unsigned result = 0;
        for (unsigned i = 0; i < totalNumberOfBuckets; ++i) {
            if (JSValue::decode(m_buckets[i]).isDouble())
                result++;
        }
        return result;
    }
        
    unsigned numberOfCells() const
    {
        unsigned result = 0;
        for (unsigned i = 0; i < totalNumberOfBuckets; ++i) {
            if (!!classInfo(i))
                result++;
        }
        return result;
    }
    
    unsigned numberOfObjects() const
    {
        unsigned result = 0;
        for (unsigned i = 0; i < totalNumberOfBuckets; ++i) {
            const ClassInfo* ci = classInfo(i);
            if (!!ci && ci->isSubClassOf(&JSObject::s_info))
                result++;
        }
        return result;
    }
    
    unsigned numberOfFinalObjects() const
    {
        unsigned result = 0;
        for (unsigned i = 0; i < totalNumberOfBuckets; ++i) {
            if (classInfo(i) == &JSFinalObject::s_info)
                result++;
        }
        return result;
    }
    
    unsigned numberOfStrings() const
    {
        unsigned result = 0;
        for (unsigned i = 0; i < totalNumberOfBuckets; ++i) {
            if (classInfo(i) == &JSString::s_info)
                result++;
        }
        return result;
    }
    
    unsigned numberOfArrays() const
    {
        unsigned result = 0;
        for (unsigned i = 0; i < totalNumberOfBuckets; ++i) {
            if (classInfo(i) == &JSArray::s_info)
                result++;
        }
        return result;
    }
    
    unsigned numberOfBooleans() const
    {
        unsigned result = 0;
        for (unsigned i = 0; i < totalNumberOfBuckets; ++i) {
            if (JSValue::decode(m_buckets[i]).isBoolean())
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
    
    unsigned probabilityOfObject() const
    {
        return computeProbability(numberOfObjects(), numberOfSamples());
    }
    
    unsigned probabilityOfFinalObject() const
    {
        return computeProbability(numberOfFinalObjects(), numberOfSamples());
    }
    
    unsigned probabilityOfArray() const
    {
        return computeProbability(numberOfArrays(), numberOfSamples());
    }
    
    unsigned probabilityOfString() const
    {
        return computeProbability(numberOfStrings(), numberOfSamples());
    }
    
    unsigned probabilityOfBoolean() const
    {
        return computeProbability(numberOfBooleans(), numberOfSamples());
    }

#ifndef NDEBUG
    void dump(FILE* out)
    {
        fprintf(out,
                "samples = %u, int32 = %u (%u), double = %u (%u), cell = %u (%u), object = %u (%u), final object = %u (%u), array = %u (%u), string = %u (%u), boolean = %u (%u), prediction = %s, samples in prediction = %u",
                numberOfSamples(),
                probabilityOfInt32(), numberOfInt32s(),
                probabilityOfDouble(), numberOfDoubles(),
                probabilityOfCell(), numberOfCells(),
                probabilityOfObject(), numberOfObjects(),
                probabilityOfFinalObject(), numberOfFinalObjects(),
                probabilityOfArray(), numberOfArrays(),
                probabilityOfString(), numberOfStrings(),
                probabilityOfBoolean(), numberOfBooleans(),
                predictionToString(m_prediction), m_numberOfSamplesInPrediction);
        bool first = true;
        for (unsigned i = 0; i < totalNumberOfBuckets; ++i) {
            JSValue value = JSValue::decode(m_buckets[i]);
            if (!!value) {
                if (first) {
                    fprintf(out, ": ");
                    first = false;
                } else
                    fprintf(out, ", ");
                fprintf(out, "%s", value.description());
            }
        }
    }
#endif
    
    struct Statistics {
        unsigned samples;
        unsigned int32s;
        unsigned doubles;
        unsigned cells;
        unsigned objects;
        unsigned finalObjects;
        unsigned arrays;
        unsigned strings;
        unsigned booleans;
        
        Statistics()
        {
            bzero(this, sizeof(Statistics));
        }
    };
    
    // Method for incrementing all relevant statistics for a ClassInfo, except for
    // incrementing the number of samples, which the caller is responsible for
    // doing.
    static void computeStatistics(const ClassInfo*, Statistics&);

    // Optimized method for getting all counts at once.
    void computeStatistics(Statistics&) const;
    
    // Updates the prediction and returns the new one.
    PredictedType computeUpdatedPrediction();
    
    int m_bytecodeOffset; // -1 for prologue
    
    PredictedType m_prediction;
    unsigned m_numberOfSamplesInPrediction;
    
    EncodedJSValue m_buckets[totalNumberOfBuckets];
};

inline int getValueProfileBytecodeOffset(ValueProfile* valueProfile)
{
    return valueProfile->m_bytecodeOffset;
}

// This is a mini value profile to catch pathologies. It is a counter that gets
// incremented when we take the slow path on any instruction.
struct RareCaseProfile {
    RareCaseProfile(int bytecodeOffset)
        : m_bytecodeOffset(bytecodeOffset)
        , m_counter(0)
    {
    }
    
    int m_bytecodeOffset;
    uint32_t m_counter;
};

inline int getRareCaseProfileBytecodeOffset(RareCaseProfile* rareCaseProfile)
{
    return rareCaseProfile->m_bytecodeOffset;
}
#endif

}

#endif

