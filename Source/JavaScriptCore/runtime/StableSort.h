/*
 * Copyright (C) 2024-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2018-2026 the V8 project authors. All rights reserved.
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
/*
 * Copyright (c) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
 * 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018 Python Software Foundation;
 * All Rights Reserved
 *
 * Part of the implementation, e.g. galloping merge, is coming from Python's TimSort.
 *
 * https://github.com/python/cpython/blob/master/Objects/listobject.c
 *
 * Detailed analysis and a description of the algorithm can be found at:
 *
 * https://github.com/python/cpython/blob/master/Objects/listsort.txt
 */

#pragma once

#include <JavaScriptCore/ArgList.h>
#include <JavaScriptCore/ThrowScope.h>
#include <numeric>
#include <wtf/Int128.h>
#include <wtf/StdLibExtras.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

static ALWAYS_INLINE bool coerceComparatorResultToBoolean(JSGlobalObject* globalObject, JSValue comparatorResult)
{
    if (comparatorResult.isInt32()) [[likely]]
        return comparatorResult.asInt32() < 0;

    // See https://bugs.webkit.org/show_bug.cgi?id=47825 on boolean special-casing
    if (comparatorResult.isBoolean())
        return !comparatorResult.asBoolean();

    return comparatorResult.toNumber(globalObject) < 0;
}

template<typename ElementType, typename Functor>
static ALWAYS_INLINE void arrayInsertionSort(VM& vm, std::span<ElementType> span, const Functor& comparator, size_t sortedHeader = 0)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* array = span.data();
    size_t length = span.size();
    for (size_t i = sortedHeader + 1; i < length; ++i) {
        auto value = array[i];

        // [l, r)
        size_t left = 0;
        size_t right = i;
        for (; left < right;) {
            size_t m = std::midpoint(left, right);
            auto target = array[m];
            bool result = comparator(value, target);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, void());
            if (!result)
                left = m + 1;
            else
                right = m;
        }
        ElementType t = value;
        for (size_t j = left; j < i; ++j)
            std::swap(array[j], t);
        array[i] = t;
    }
}

// Detect ascending or strictly-descending run starting at begin.
// If descending, reverse in-place to make ascending (stable: uses strict < for descending).
// Returns end index (inclusive) of the normalized ascending run.
template<typename ElementType, typename Functor>
static ALWAYS_INLINE size_t extendAndNormalizeRun(VM& vm, std::span<ElementType> span, size_t begin, const Functor& comparator)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    size_t end = begin;
    size_t numElements = span.size();
    if (end + 1 >= numElements)
        return end;

    // Check if the run starts descending: a[begin+1] < a[begin] (strict for stability).
    bool descending = comparator(span[end + 1], span[end]);
    RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, end);

    if (descending) {
        // Extend strictly descending run.
        ++end;
        while (end + 1 < numElements) {
            bool result = comparator(span[end + 1], span[end]);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, end);
            if (!result)
                break;
            ++end;
        }
        std::ranges::reverse(span.subspan(begin, end + 1 - begin));
    } else {
        // Extend ascending run (non-strictly: a[i+1] >= a[i]).
        ++end;
        while (end + 1 < numElements) {
            bool result = comparator(span[end + 1], span[end]);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, end);
            if (result)
                break;
            ++end;
        }
    }

    return end;
}

// gallopLeft: Find the leftmost position where key should be inserted (before equal elements).
// Returns k in [0, length] such that base[k-1] < key <= base[k].
// hint is a starting index for the exponential search.
template<typename ElementType, typename Functor>
static ALWAYS_INLINE size_t gallopLeft(VM& vm, const ElementType& key, const ElementType* base, size_t length, size_t hint, const Functor& comparator)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(hint < length);

    size_t lastOffset = 0;
    size_t offset = 1;

    // base[hint] < key => gallop right
    bool hintLessThanKey = comparator(base[hint], key);
    RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, 0);

    if (hintLessThanKey) {
        // Gallop right: a[hint] < key, so search in (hint, length).
        size_t maxOffset = length - hint;
        while (offset < maxOffset) {
            bool result = comparator(base[hint + offset], key);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, 0);
            if (!result)
                break;
            lastOffset = offset;
            size_t nextOffset = (offset << 1) + 1;
            offset = nextOffset > offset ? std::min(nextOffset, maxOffset) : maxOffset;
        }
        lastOffset += hint;
        offset += hint;
    } else {
        // Gallop left: a[hint] >= key, so search in [0, hint).
        size_t maxOffset = hint + 1;
        while (offset < maxOffset) {
            bool result = comparator(base[hint - offset], key);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, 0);
            if (result)
                break;
            lastOffset = offset;
            size_t nextOffset = (offset << 1) + 1;
            offset = nextOffset > offset ? std::min(nextOffset, maxOffset) : maxOffset;
        }
        size_t tmp = lastOffset;
        lastOffset = hint - offset;
        offset = hint - tmp;
    }

    // Now base[lastOffset] < key <= base[offset], binary search in (lastOffset, offset].
    ++lastOffset;
    while (lastOffset < offset) {
        size_t m = lastOffset + ((offset - lastOffset) >> 1);
        bool result = comparator(base[m], key);
        RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, 0);
        if (result)
            lastOffset = m + 1;
        else
            offset = m;
    }

    return offset;
}

// gallopRight: Find the rightmost position where key should be inserted (after equal elements).
// Returns k in [0, length] such that base[k-1] <= key < base[k].
template<typename ElementType, typename Functor>
static ALWAYS_INLINE size_t gallopRight(VM& vm, const ElementType& key, const ElementType* base, size_t length, size_t hint, const Functor& comparator)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(hint < length);

    size_t lastOffset = 0;
    size_t offset = 1;

    // key < base[hint] => gallop left
    bool keyLessThanHint = comparator(key, base[hint]);
    RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, 0);

    if (keyLessThanHint) {
        // Gallop left: key < a[hint], so search in [0, hint).
        size_t maxOffset = hint + 1;
        while (offset < maxOffset) {
            bool result = comparator(key, base[hint - offset]);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, 0);
            if (!result)
                break;
            lastOffset = offset;
            size_t nextOffset = (offset << 1) + 1;
            offset = nextOffset > offset ? std::min(nextOffset, maxOffset) : maxOffset;
        }
        // Translate back to positive offsets from base.
        size_t tmp = lastOffset;
        lastOffset = hint - offset;
        offset = hint - tmp;
    } else {
        // Gallop right: key >= a[hint], so search in (hint, length).
        size_t maxOffset = length - hint;
        while (offset < maxOffset) {
            bool result = comparator(key, base[hint + offset]);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, 0);
            if (result)
                break;
            lastOffset = offset;
            size_t nextOffset = (offset << 1) + 1;
            offset = nextOffset > offset ? std::min(nextOffset, maxOffset) : maxOffset;
        }
        // Translate to absolute offsets.
        lastOffset += hint;
        offset += hint;
    }

    // Now base[lastOffset] <= key < base[offset], binary search in (lastOffset, offset].
    ++lastOffset;
    while (lastOffset < offset) {
        size_t m = lastOffset + ((offset - lastOffset) >> 1);
        bool result = comparator(key, base[m]);
        RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, 0);
        if (result)
            offset = m;
        else
            lastOffset = m + 1;
    }

    return offset;
}

// Gallop and Simple are code size-time tradeoffs. Gallop is faster and is more code, while Simple is
// slower and is less code. Currently galloping is used for Array sorting, while Simple is used for
// TypedArray sorting. Array sorting is expected to be fast, while TypedArray sorting is relatively
// rare and would incur a significant number of template instantiations across all the TypedArray
// kinds.
enum class MergeStrategy { Galloping, Simple };

template<typename ElementType, typename Functor>
static ALWAYS_INLINE void mergeRunsSimple(VM& vm, std::span<ElementType> dst, std::span<const ElementType> src, size_t srcIndex1, size_t srcEnd1, size_t srcIndex2, size_t srcEnd2, const Functor& comparator)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    size_t left = srcIndex1;
    size_t leftEnd = srcEnd1;
    size_t right = srcIndex2;
    size_t rightEnd = srcEnd2;

    ASSERT(leftEnd <= right);
    ASSERT(rightEnd <= src.size());

    for (size_t dstIndex = left; dstIndex < rightEnd; ++dstIndex) {
        if (right < rightEnd) {
            if (left >= leftEnd) {
                dst[dstIndex] = src[right++];
                continue;
            }
            bool result = comparator(src[right], src[left]);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, void());
            if (result) {
                dst[dstIndex] = src[right++];
                continue;
            }
        }
        dst[dstIndex] = src[left++];
    }
}

// webkit.org/b/313694 : Workaround GCC 13 slowness building this.
#if COMPILER(GCC) && GCC_VERSION < 140000
#define MAYBE_ALWAYS_INLINE inline
#else
#define MAYBE_ALWAYS_INLINE ALWAYS_INLINE
#endif

static constexpr size_t minGallopThreshold = 7;

template<typename ElementType, typename Functor>
static MAYBE_ALWAYS_INLINE void mergePowersortRuns(VM& vm, std::span<ElementType> dst, std::span<const ElementType> src, size_t srcIndex1, size_t srcEnd1, size_t srcIndex2, size_t srcEnd2, const Functor& comparator, size_t& minGallop)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(srcEnd1 <= srcIndex2);
    ASSERT(srcEnd2 <= src.size());

    size_t leftLength = srcEnd1 - srcIndex1;
    size_t rightLength = srcEnd2 - srcIndex2;

    // Either run is empty. Just copy entire two consecutive runs into destination.
    if (!leftLength || !rightLength) {
        WTF::copyElements(dst.subspan(srcIndex1, srcEnd2 - srcIndex1), src.subspan(srcIndex1, srcEnd2 - srcIndex1));
        return;
    }

    // Pre-merge trim: skip leading elements of left that are already in place.
    // If the right run's first element can be inserted into skipLeft position,
    // [srcIndex, skipLeft) sorted ones are lower than any of right run's element.
    // Thus copy them into the destination's lowest area.
    size_t skipLeft = gallopRight(vm, src[srcIndex2], src.data() + srcIndex1, leftLength, 0, comparator);
    RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, void());

    // Copy the already-in-place leading elements.
    if (skipLeft)
        WTF::copyElements(dst.subspan(srcIndex1, skipLeft), src.subspan(srcIndex1, skipLeft));

    size_t left = srcIndex1 + skipLeft;
    leftLength -= skipLeft;

    if (!leftLength) {
        // All of left is <= first element of right; just copy right.
        WTF::copyElements(dst.subspan(srcIndex2, rightLength), src.subspan(srcIndex2, rightLength));
        return;
    }

    // Pre-merge trim: skip trailing elements of right that are already in place.
    // If the left run's last element can be inserted into a position,
    // elements after that in the right run should be in the destination's highest area.
    size_t skipRight = rightLength - gallopLeft(vm, src[srcEnd1 - 1], src.data() + srcIndex2, rightLength, rightLength - 1, comparator);
    RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, void());

    // Copy the already-in-place trailing elements.
    if (skipRight)
        WTF::copyElements(dst.subspan(srcEnd2 - skipRight, skipRight), src.subspan(srcEnd2 - skipRight, skipRight));

    const size_t rightEnd = srcEnd2 - skipRight;
    rightLength -= skipRight;

    if (!rightLength) {
        // All of right is >= last element of left; just copy left.
        WTF::copyElements(dst.subspan(left, leftLength), src.subspan(left, leftLength));
        return;
    }

    ASSERT(leftLength);
    ASSERT(rightLength);

    // Merge with galloping mode. After pre-merge trim, new range is [left, leftEnd) and [right, rightEnd).
    const size_t leftEnd = srcEnd1;
    size_t right = srcIndex2;
    size_t dstIndex = left;
    size_t leftWins = 0;
    size_t rightWins = 0;

    for (;;) {
        // Linear merge until one side wins minGallop times consecutively.
        leftWins = 0;
        rightWins = 0;

        while (leftWins < minGallop && rightWins < minGallop) {
            if (right < rightEnd && left < leftEnd) {
                bool result = comparator(src[right], src[left]);
                RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, void());
                if (result) {
                    dst[dstIndex++] = src[right++];
                    ++rightWins;
                    leftWins = 0;
                } else {
                    dst[dstIndex++] = src[left++];
                    ++leftWins;
                    rightWins = 0;
                }
            } else {
                // One side of run gets exhausted. Let's just copy the rest.
                goto mergeFinish;
            }
        }

        // Galloping mode: one side is winning consistently.
        // Increase minGallop to make it harder to re-enter galloping (penalize leaving).
        ++minGallop;

        do {
            // Decrease minGallop while galloping is productive.
            if (minGallop > 1)
                --minGallop;

            if (left >= leftEnd || right >= rightEnd)
                goto mergeFinish;

            // Gallop in left run for right's current element.
            {
                size_t k = gallopRight(vm, src[right], src.data() + left, leftEnd - left, 0, comparator);
                RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, void());
                leftWins = k;
                if (k) {
                    // k elements in the left are lower than right elements. Just copy them.
                    WTF::copyElements(dst.subspan(dstIndex, k), src.subspan(left, k));
                    dstIndex += k;
                    left += k;
                }
                dst[dstIndex++] = src[right++];

                if (left >= leftEnd || right >= rightEnd)
                    goto mergeFinish;
            }

            // Gallop in right run for left's current element.
            {
                size_t k = gallopLeft(vm, src[left], src.data() + right, rightEnd - right, 0, comparator);
                RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, void());
                rightWins = k;
                if (k) {
                    // k elements in the right are lower than left elements. Just copy them.
                    WTF::copyElements(dst.subspan(dstIndex, k), src.subspan(right, k));
                    dstIndex += k;
                    right += k;
                }
                dst[dstIndex++] = src[left++];

                if (left >= leftEnd || right >= rightEnd)
                    goto mergeFinish;
            }
        } while (leftWins >= minGallopThreshold || rightWins >= minGallopThreshold);

        // Leaving galloping mode; penalize.
        ++minGallop;
    }

mergeFinish:
    // Copy remaining elements.
    while (left < leftEnd)
        dst[dstIndex++] = src[left++];
    while (right < rightEnd)
        dst[dstIndex++] = src[right++];
}

// J. Ian Munro and Sebastian Wild. Nearly-Optimal Mergesorts: Fast, Practical Sorting Methods That
// Optimally Adapt to Existing Runs. In 26th Annual European Symposium on Algorithms (ESA 2018).
// Leibniz International Proceedings in Informatics (LIPIcs), Volume 112, pp. 63:1-63:16, Schloss
// Dagstuhl – Leibniz-Zentrum für Informatik (2018) https://doi.org/10.4230/LIPIcs.ESA.2018.63
// https://arxiv.org/abs/1805.04154

struct SortedRun {
    size_t m_begin;
    size_t m_end;
};

template<MergeStrategy mergeStrategy, typename ElementType, typename Functor, size_t forceRunLength = 64>
static ALWAYS_INLINE std::span<ElementType> arrayStableSort(VM& vm, std::span<ElementType> src, std::span<ElementType> workingSet, const Functor& comparator)
{
    constexpr size_t extendRunCutoff = 8;

    auto scope = DECLARE_THROW_SCOPE(vm);

    const size_t numElements = src.size();

    if (!numElements)
        return src;

    // If the array is small, Powersort probably isn't worth it. Just insertion sort.
    if (numElements < extendRunCutoff) {
        scope.release();
        arrayInsertionSort(vm, src.subspan(0, src.size()), comparator);
        return src;
    }

    // power takes in [left, middle-1] and [middle, right]
    auto power = [](size_t left, size_t middle, size_t right, size_t n) -> unsigned {
        UInt128 n1 = middle - left;
        UInt128 n2 = right - middle + 1;
        // a and b are 2*midpoints of the two ranges, so always within [0, 2n)
        UInt128 a = left * 2 + n1;
        UInt128 b = middle * 2 + n2;
        // n <= 2^64, so n << 62 <= 2^126
        // n << 62 must be <= 2^126, so a << 62 must be < 2^127. thus, we don't end up with overflow
        a <<= 62;
        b <<= 62;

        // a is within [0, 2n), so a << 62 is within [0, 2^63 n). Thus, (when we calculate a / n, it must be within [0, 2^63)
        UInt128 differingBits = (a / n) ^ (b / n);
        ASSERT(!(differingBits >> 64));
        return clz(static_cast<uint64_t>(differingBits));
    };

    struct PowersortStackEntry {
        SortedRun run;
        unsigned power;
    };

    WTF::Vector<PowersortStackEntry, 64> powerstack;
    // floor(lg(n)) + 1
    powerstack.reserveCapacity(8 * sizeof(numElements) - WTF::clz(numElements));

    [[maybe_unused]] size_t minGallop = minGallopThreshold;

    SortedRun run1 { 0, 0 };

    // Detect ascending or descending run, normalize to ascending.
    run1.m_end = extendAndNormalizeRun(vm, src, run1.m_begin, comparator);
    RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, src);

    if (run1.m_end - run1.m_begin < extendRunCutoff) {
        // If the run is too short, insertion sort a bit
        auto size = std::min(forceRunLength, numElements - run1.m_begin);
        arrayInsertionSort(vm, src.subspan(run1.m_begin, size), comparator, run1.m_end - run1.m_begin);
        RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, src);
        run1.m_end = run1.m_begin + size - 1;
    }

    // See if we can extend the run any more.
    while (run1.m_end + 1 < numElements) {
        bool result = comparator(src[run1.m_end + 1], src[run1.m_end]);
        RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, src);
        if (result)
            break;
        ++run1.m_end;
    }

    while (run1.m_end + 1 < numElements) {
        SortedRun run2 { run1.m_end + 1, run1.m_end + 1 };

        // Detect ascending or descending run, normalize to ascending.
        run2.m_end = extendAndNormalizeRun(vm, src, run2.m_begin, comparator);
        RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, src);

        if (run2.m_end - run2.m_begin < extendRunCutoff) {
            // If the run is too short, insertion sort a bit
            auto size = std::min(forceRunLength, numElements - run2.m_begin);
            arrayInsertionSort(vm, src.subspan(run2.m_begin, size), comparator, run2.m_end - run2.m_begin);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, src);
            run2.m_end = run2.m_begin + size - 1;
        }

        // See if we can extend the run any more.
        while (run2.m_end + 1 < numElements) {
            bool result = comparator(src[run2.m_end + 1], src[run2.m_end]);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, src);
            if (result)
                break;
            ++run2.m_end;
        }

        unsigned p = power(run1.m_begin, run2.m_begin, run2.m_end, numElements);
        while (!powerstack.isEmpty() && powerstack.last().power > p) {
            auto rangeToMerge = powerstack.takeLast().run;
            ASSERT(rangeToMerge.m_end == run1.m_begin - 1);

            if constexpr (mergeStrategy == MergeStrategy::Galloping)
                mergePowersortRuns(vm, workingSet, spanConstCast<const ElementType>(src), rangeToMerge.m_begin, rangeToMerge.m_end + 1, run1.m_begin, run1.m_end + 1, comparator, minGallop);
            else
                mergeRunsSimple(vm, workingSet, spanConstCast<const ElementType>(src), rangeToMerge.m_begin, rangeToMerge.m_end + 1, run1.m_begin, run1.m_end + 1, comparator);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, src);
            WTF::copyElements(src.subspan(rangeToMerge.m_begin, run1.m_end + 1 - rangeToMerge.m_begin), spanConstCast<const ElementType>(workingSet).subspan(rangeToMerge.m_begin, run1.m_end + 1 - rangeToMerge.m_begin));
            run1.m_begin = rangeToMerge.m_begin;
        }

        powerstack.append({ run1, p });
        run1 = run2;
    }

    while (!powerstack.isEmpty()) {
        auto rangeToMerge = powerstack.takeLast().run;
        ASSERT(rangeToMerge.m_end == run1.m_begin - 1);

        if constexpr (mergeStrategy == MergeStrategy::Galloping)
            mergePowersortRuns(vm, workingSet, spanConstCast<const ElementType>(src), rangeToMerge.m_begin, rangeToMerge.m_end + 1, run1.m_begin, run1.m_end + 1, comparator, minGallop);
        else
            mergeRunsSimple(vm, workingSet, spanConstCast<const ElementType>(src), rangeToMerge.m_begin, rangeToMerge.m_end + 1, run1.m_begin, run1.m_end + 1, comparator);
        RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, src);
        WTF::copyElements(src.subspan(rangeToMerge.m_begin, run1.m_end + 1 - rangeToMerge.m_begin), spanConstCast<const ElementType>(workingSet).subspan(rangeToMerge.m_begin, run1.m_end + 1 - rangeToMerge.m_begin));
        run1.m_begin = rangeToMerge.m_begin;
    }

    return src;
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
