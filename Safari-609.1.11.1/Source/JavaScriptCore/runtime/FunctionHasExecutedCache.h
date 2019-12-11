/*
 * Copyright (C) 2014 Apple Inc. All Rights Reserved.
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

#include <wtf/HashMethod.h>
#include <wtf/StdUnorderedMap.h>
#include <wtf/Vector.h>

namespace JSC {

class FunctionHasExecutedCache {
public:
    struct FunctionRange {
        FunctionRange() {}
        bool operator==(const FunctionRange& other) const 
        {
            return m_start == other.m_start && m_end == other.m_end;
        }
        unsigned hash() const
        {
            return m_start * m_end;
        }

        unsigned m_start;
        unsigned m_end;
    };

    bool hasExecutedAtOffset(intptr_t id, unsigned offset);
    void insertUnexecutedRange(intptr_t id, unsigned start, unsigned end);
    void removeUnexecutedRange(intptr_t id, unsigned start, unsigned end);
    Vector<std::tuple<bool, unsigned, unsigned>> getFunctionRanges(intptr_t id);

private:
    using RangeMap = StdUnorderedMap<FunctionRange, bool, HashMethod<FunctionRange>>;
    using SourceIDToRangeMap = StdUnorderedMap<intptr_t, RangeMap>;
    SourceIDToRangeMap m_rangeMap;
};

} // namespace JSC
