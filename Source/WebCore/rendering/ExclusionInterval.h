/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ExclusionInterval_h
#define ExclusionInterval_h

#include <wtf/Vector.h>

namespace WebCore {

struct ExclusionInterval {
public:
    float x1;
    float x2;

    ExclusionInterval(float x1 = 0, float x2 = 0)
        : x1(x1)
        , x2(x2)
    {
    }

    bool intersect(const ExclusionInterval&, ExclusionInterval&) const;
};

void sortExclusionIntervals(Vector<ExclusionInterval>&);
void mergeExclusionIntervals(const Vector<ExclusionInterval>&, const Vector<ExclusionInterval>&, Vector<ExclusionInterval>&);
void intersectExclusionIntervals(const Vector<ExclusionInterval>&, const Vector<ExclusionInterval>&, Vector<ExclusionInterval>&);
void subtractExclusionIntervals(const Vector<ExclusionInterval>&, const Vector<ExclusionInterval>&, Vector<ExclusionInterval>&);

} // namespace WebCore

#endif // ExclusionInterval_h
