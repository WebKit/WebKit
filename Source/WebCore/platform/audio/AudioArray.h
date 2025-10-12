/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#ifndef AudioArray_h
#define AudioArray_h

#include <span>
#include <string.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/MallocSpan.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

template<typename T>
class AudioArray {
    WTF_MAKE_TZONE_ALLOCATED_TEMPLATE(AudioArray);
public:
    AudioArray() = default;
    explicit AudioArray(size_t n)
    {
        resize(n);
    }

    ~AudioArray() = default;

    // It's OK to call resize() multiple times, but data will *not* be copied from an initial allocation
    // if re-allocated. Allocations are zero-initialized.
    void resize(Checked<size_t> n)
    {
        if (n == size())
            return;

        Checked<size_t> initialSize = sizeof(T) * n;
#if USE(GSTREAMER)
        m_allocation = MallocSpan<T>::zeroedMalloc(initialSize);
#else
        // Accelerate.framework behaves differently based on input vector alignment. And each implementation
        // has very small difference in output! We ensure 32byte alignment so that we will always take the most
        // optimized implementation if possible, which makes the result deterministic.
        constexpr size_t alignment = 32;

        m_allocation = MallocSpan<T, FastAlignedMalloc>::alignedMalloc(alignment, initialSize);
        zero();
#endif
    }

    std::span<T> span() LIFETIME_BOUND { return m_allocation.mutableSpan(); }
    std::span<const T> span() const LIFETIME_BOUND { return m_allocation.span(); }
    T* data() LIFETIME_BOUND { return span().data(); }
    const T* data() const LIFETIME_BOUND { return span().data(); }
    size_t size() const { return span().size(); }
    bool isEmpty() const { return span().empty(); }

    T& at(size_t i) LIFETIME_BOUND { return m_allocation[i]; }
    const T& at(size_t i) const LIFETIME_BOUND { return m_allocation[i]; }
    T& operator[](size_t i) LIFETIME_BOUND { return at(i); }
    const T& operator[](size_t i) const LIFETIME_BOUND { return at(i); }

    void zero() { zeroSpan(span()); }

    void zeroRange(unsigned start, unsigned end)
    {
        bool isSafe = (start <= end) && (end <= size());
        ASSERT(isSafe);
        if (!isSafe)
            return;

        // This expression cannot overflow because end - start cannot be
        // greater than m_size, which is safe due to the check in resize().
        zeroSpan(span().subspan(start, end - start));
    }

    void copyToRange(std::span<const T> sourceData, unsigned start, unsigned end)
    {
        bool isSafe = (start <= end) && (end <= size());
        ASSERT(isSafe);
        if (!isSafe)
            return;

        // This expression cannot overflow because end - start cannot be
        // greater than m_size, which is safe due to the check in resize().
        memcpySpan(this->span().subspan(start), sourceData.first(end - start));
    }

    bool containsConstantValue() const
    {
        if (size() <= 1)
            return true;
        float constant = m_allocation[0];
        for (auto& value : span().subspan(1)) {
            if (value != constant)
                return false;
        }
        return true;
    }

private:
#if USE(GSTREAMER)
    MallocSpan<T> m_allocation;
#else
    MallocSpan<T, FastAlignedMalloc> m_allocation;
#endif
};

WTF_MAKE_TZONE_ALLOCATED_TEMPLATE_IMPL(template<typename T>, AudioArray<T>);

typedef AudioArray<float> AudioFloatArray;
typedef AudioArray<double> AudioDoubleArray;

} // WebCore

#endif // AudioArray_h
