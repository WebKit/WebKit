/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <cstdint>
#include <wtf/ExportMacros.h>
#include <wtf/Vector.h>

namespace WTF {

namespace Xsum {

struct SmallAccumulator final {
    explicit SmallAccumulator(const int addsUntilPropagate, const int64_t inf, const int64_t nan);
    ~SmallAccumulator() = default;

    Vector<int64_t> chunk; // Chunks making up small accumulator
    int addsUntilPropagate; // Number of remaining adds before carry
    int64_t inf; // If non-zero, +Inf, -Inf, or NaN
    int64_t nan; // If non-zero, a NaN value with payload
};

} // namespace Xsum

class PreciseSum final {
public:
    explicit PreciseSum();
    ~PreciseSum() = default;

    WTF_EXPORT_PRIVATE void addList(const std::span<const double> vec);
    WTF_EXPORT_PRIVATE void add(double value);
    WTF_EXPORT_PRIVATE double compute();

private:
    Xsum::SmallAccumulator m_smallAccumulator;
    size_t m_sizeCount;
    bool m_hasPosNumber;

    void xsumSmallAddInfNan(int64_t ivalue);
    inline void xsumAdd1NoCarry(double value);
    int xsumCarryPropagate();
    ALWAYS_INLINE void incrementWhenValueAdded(double value);
};

} // namespace WTF

using WTF::PreciseSum;
