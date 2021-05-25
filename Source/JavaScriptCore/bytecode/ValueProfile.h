/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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
    static constexpr unsigned numberOfBuckets = numberOfBucketsArgument;
    static constexpr unsigned numberOfSpecFailBuckets = 1;
    static constexpr unsigned bucketIndexMask = numberOfBuckets - 1;
    static constexpr unsigned totalNumberOfBuckets = numberOfBuckets + numberOfSpecFailBuckets;
    
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
                return nullptr;
            return value.asCell()->structure()->classInfo();
        }
        return nullptr;
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
    static constexpr unsigned logNumberOfBuckets = logNumberOfBucketsArgument;
    
    ValueProfileWithLogNumberOfBuckets()
        : ValueProfileBase<1 << logNumberOfBucketsArgument>()
    {
    }
};

struct ValueProfile : public ValueProfileWithLogNumberOfBuckets<0> {
    ValueProfile() : ValueProfileWithLogNumberOfBuckets<0>() { }
};

struct ValueProfileAndVirtualRegister : public ValueProfile {
    VirtualRegister m_operand;
};

static_assert(sizeof(ValueProfileAndVirtualRegister) >= sizeof(unsigned));
class alignas(ValueProfileAndVirtualRegister) ValueProfileAndVirtualRegisterBuffer final {
    WTF_MAKE_NONCOPYABLE(ValueProfileAndVirtualRegisterBuffer);
public:

    static ValueProfileAndVirtualRegisterBuffer* create(unsigned size)
    {
        void* buffer = VMMalloc::malloc(sizeof(ValueProfileAndVirtualRegisterBuffer) + size * sizeof(ValueProfileAndVirtualRegister));
        return new (buffer) ValueProfileAndVirtualRegisterBuffer(size);
    }

    static void destroy(ValueProfileAndVirtualRegisterBuffer* buffer)
    {
        buffer->~ValueProfileAndVirtualRegisterBuffer();
        VMMalloc::free(buffer);
    }

    template <typename Function>
    void forEach(Function function)
    {
        for (unsigned i = 0; i < m_size; ++i)
            function(data()[i]);
    }

    unsigned size() const { return m_size; }
    ValueProfileAndVirtualRegister* data() const
    {
        return bitwise_cast<ValueProfileAndVirtualRegister*>(this + 1);
    }

private:

    ValueProfileAndVirtualRegisterBuffer(unsigned size)
        : m_size(size)
    {
        // FIXME: ValueProfile has more stuff than we need. We could optimize these value profiles
        // to be more space efficient.
        // https://bugs.webkit.org/show_bug.cgi?id=175413
        for (unsigned i = 0; i < m_size; ++i)
            new (&data()[i]) ValueProfileAndVirtualRegister();
    }

    ~ValueProfileAndVirtualRegisterBuffer()
    {
        for (unsigned i = 0; i < m_size; ++i)
            data()[i].~ValueProfileAndVirtualRegister();
    }

    unsigned m_size;
};

} // namespace JSC
