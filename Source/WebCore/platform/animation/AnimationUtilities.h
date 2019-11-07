/*
 * Copyright (C) 2011-2019 Apple Inc.  All rights reserved.
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

#include "IntPoint.h"
#include "LayoutPoint.h"

namespace WebCore {

inline int blend(int from, int to, double progress)
{  
    return static_cast<int>(lround(static_cast<double>(from) + static_cast<double>(to - from) * progress));
}

inline unsigned blend(unsigned from, unsigned to, double progress)
{
    return static_cast<unsigned>(lround(to > from ? static_cast<double>(from) + static_cast<double>(to - from) * progress : static_cast<double>(from) - static_cast<double>(from - to) * progress));
}

inline double blend(double from, double to, double progress)
{  
    return from + (to - from) * progress;
}

inline float blend(float from, float to, double progress)
{  
    return static_cast<float>(from + (to - from) * progress);
}

inline LayoutUnit blend(LayoutUnit from, LayoutUnit to, double progress)
{  
    return LayoutUnit(from + (to - from) * progress);
}

inline IntPoint blend(const IntPoint& from, const IntPoint& to, double progress)
{
    return IntPoint(blend(from.x(), to.x(), progress),
        blend(from.y(), to.y(), progress));
}

inline LayoutPoint blend(const LayoutPoint& from, const LayoutPoint& to, double progress)
{
    return LayoutPoint(blend(from.x(), to.x(), progress),
        blend(from.y(), to.y(), progress));
}

} // namespace WebCore
