/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#include "TimeRanges.h"

using namespace WebCore;

TimeRanges::TimeRanges(float start, float end)
{
    add(start, end);
}

float TimeRanges::start(unsigned index, ExceptionCode& ec) const 
{ 
    if (index >= length()) {
        ec = INDEX_SIZE_ERR;
        return 0;
    }
    return m_ranges[index].m_start;
}

float TimeRanges::end(unsigned index, ExceptionCode& ec) const 
{ 
    if (index >= length()) {
        ec = INDEX_SIZE_ERR;
        return 0;
    }
    return m_ranges[index].m_end;
}

void TimeRanges::add(float start, float end) 
{ 
    m_ranges.append(Range(start, end));
    // FIXME normalize
}

bool TimeRanges::contain(float time) const
{ 
    ExceptionCode unused;
    for (unsigned n = 0; n < length(); n++) {
        if (time >= start(n, unused) && time <= end(n, unused))
            return true;
    }
    return false;
}
