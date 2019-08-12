/*
 * Copyright (C) 2011-2013, 2016 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#pragma once

#include "ConcurrentJSLock.h"
#include "SpeculatedType.h"
#include "Structure.h"
#include <wtf/PrintStream.h>
#include <wtf/StringPrintStream.h>

namespace JSC {

template<unsigned numberOfBucketsArgument>
struct ValueProfileBase {
    static const unsigned numberOfBuckets = numberOfBucketsArgument;
    static const unsigned numberOfSpecFailBuckets = 1;
    static const unsigned bucketIndexMask = numberOfBuckets - 1;
    static const unsigned totalNumberOfBuckets = numberOfBuckets + numberOfSpecFailBuckets;
    
    ValueProfileBase()
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
        return numberOfSamples() + isSampledBefore();
    }

    bool isSampledBefore() const { return m_prediction != SpecNone; }
    
    bool isLive() const
    {
        for (unsigned i = 0; i < totalNumberOfBuckets; ++i) {
            if (!!JSValue::decode(m_buckets[i]))
                return true;
        }
        return false;
    }
    
    CString briefDescription(const ConcurrentJSLocker& locker)
    {
        computeUpdatedPrediction(locker);
        
        StringPrintStream out;
        out.print("predicting ", SpeculationDump(m_prediction));
        return out.toCString();
    }
    
    void dump(PrintStream& out)
    {
        out.print("sampled before = ", isSampledBefore(), " live samples = ", numberOfSamples(), " prediction = ", SpeculationDump(m_prediction));
        bool first = true;
        for (unsigned i = 0; i < totalNumberOfBuckets; ++i) {
            JSValue value = JSValue::decode(m_buckets[i]);
            if (!!value) {
                if (first) {
                    out.printf(": ");
                    first = false;
                } else
                    out.printf(", ");
                out.print(value);
            }
        }
    }
    
    // Updates the prediction and returns the new one. Never call this from any thread
    // that isn't executing the code.
    SpeculatedType computeUpdatedPrediction(const ConcurrentJSLocker&)
    {
        for (unsigned i = 0; i < totalNumberOfBuckets; ++i) {
            JSValue value = JSValue::decode(m_buckets[i]);
            if (!value)
                continue;
            
            mergeSpeculation(m_prediction, speculationFromValue(value));
            
            m_buckets[i] = JSValue::encode(JSValue());
        }
        
        return m_prediction;
    }
    
    EncodedJSValue m_buckets[totalNumberOfBuckets];

    SpeculatedType m_prediction { SpecNone };
};

struct MinimalValueProfile : public ValueProfileBase<0> {
    MinimalValueProfile(): ValueProfileBase<0>() { }
};

template<unsigned logNumberOfBucketsArgument>
struct ValueProfileWithLogNumberOfBuckets : public ValueProfileBase<1 << logNumberOfBucketsArgument> {
    static const unsigned logNumberOfBuckets = logNumberOfBucketsArgument;
    
    ValueProfileWithLogNumberOfBuckets()
        : ValueProfileBase<1 << logNumberOfBucketsArgument>()
    {
    }
};

struct ValueProfile : public ValueProfileWithLogNumberOfBuckets<0> {
    ValueProfile() : ValueProfileWithLogNumberOfBuckets<0>() { }
};

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

struct ValueProfileAndOperand : public ValueProfile {
    int m_operand;
};

struct ValueProfileAndOperandBuffer {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    ValueProfileAndOperandBuffer(unsigned size)
        : m_size(size)
    {
        // FIXME: ValueProfile has more stuff than we need. We could optimize these value profiles
        // to be more space efficient.
        // https://bugs.webkit.org/show_bug.cgi?id=175413
        m_buffer = MallocPtr<ValueProfileAndOperand>::malloc(m_size * sizeof(ValueProfileAndOperand));
        for (unsigned i = 0; i < m_size; ++i)
            new (&m_buffer.get()[i]) ValueProfileAndOperand();
    }

    ~ValueProfileAndOperandBuffer()
    {
        for (unsigned i = 0; i < m_size; ++i)
            m_buffer.get()[i].~ValueProfileAndOperand();
    }

    template <typename Function>
    void forEach(Function function)
    {
        for (unsigned i = 0; i < m_size; ++i)
            function(m_buffer.get()[i]);
    }

    unsigned m_size;
    MallocPtr<ValueProfileAndOperand> m_buffer;
};

} // namespace JSC
