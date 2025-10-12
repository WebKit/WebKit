/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Vector.h>

namespace WTF {

namespace Xsum {

struct SmallAccumulator final {
    explicit SmallAccumulator();
    explicit SmallAccumulator(
        Vector<int64_t> &&chunk, const int addsUntilPropagate, const int64_t inf,
        const int64_t nan, const size_t sizeCount, const bool hasPosNumber
    );
    ~SmallAccumulator() = default;

    Vector<int64_t> chunk; // Chunks making up small accumulator
    int addsUntilPropagate; // Number of remaining adds before carry
    int64_t inf; // If non-zero, +Inf, -Inf, or NaN
    int64_t nan; // If non-zero, a NaN value with payload
    size_t sizeCount; // number of added values
    bool hasPosNumber; // check if added values have at least one positive number

    int carryPropagate();
    COLD void addInfNan(int64_t ivalue);
    inline void add1NoCarry(double value);
    ALWAYS_INLINE void incrementWhenValueAdded(double value);
};

struct LargeAccumulator final {
    Vector<uint64_t> chunk; // Chunks making up large accumulator
    Vector<int_least16_t> count; // Counts of # adds remaining for chunks, or -1 if not used yet or special
    Vector<uint64_t> chunksUsed; // Bits indicate chunks in use
    uint64_t usedUsed; // Bits indicate chunk_used entries not 0
    SmallAccumulator sacc; // The small accumulator to condense into

    explicit LargeAccumulator();
    ~LargeAccumulator() = default;

    void addLchunkToSmall(int_fast16_t ix);
    COLD void largeAddValueInfNan(int_fast16_t ix, uint64_t uintv);
    void transferToSmall();
};

class XsumInterface {
public:
    virtual ~XsumInterface() = default;

    virtual void addList(const std::span<const double> vec) = 0;
    virtual void add(double value) = 0;
    virtual double compute() = 0;
};

class XsumSmall final : public XsumInterface {
public:
    explicit XsumSmall();
    explicit XsumSmall(SmallAccumulator sacc);
    ~XsumSmall() = default;

    void addList(const std::span<const double> vec) override;
    void add(double value) override;
    double compute() override;

private:
    SmallAccumulator m_smallAccumulator;
};

class XsumLarge final : public XsumInterface {
public:
    explicit XsumLarge();
    ~XsumLarge() = default;

    void addList(const std::span<const double> vec) override;
    void add(double value) override;
    double compute() override;

private:
    LargeAccumulator m_largeAccumulator;
};

} // namespace Xsum

// Threshold for PreciseSum to determine whether to use XsumSmall or XsumLarge
// If the expected array length is greater than PRECISE_SUM_THRESHOLD, use Xsum::XsumLarge;
// otherwise use Xsum::XsumSmall
constexpr uint64_t PRECISE_SUM_THRESHOLD = 1'000;

template<std::derived_from<Xsum::XsumInterface> T = Xsum::XsumSmall>
class PreciseSum final {
public:
    explicit PreciseSum()
        : m_xsum { T { } } { }
    ~PreciseSum() = default;

    void addList(const std::span<const double> vec)
    {
        m_xsum.addList(vec);
    }

    void add(double value)
    {
        m_xsum.add(value);
    }

    double compute()
    {
        return m_xsum.compute();
    }

private:
    T m_xsum;
};

} // namespace WTF

using WTF::PreciseSum;
