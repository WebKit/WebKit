/*
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
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

#ifndef TimeRanges_h
#define TimeRanges_h

#include "PlatformTimeRanges.h"
#include <algorithm>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

typedef int ExceptionCode;

class TimeRanges : public RefCounted<TimeRanges> {
public:
    static PassRefPtr<TimeRanges> create();
    static PassRefPtr<TimeRanges> create(double start, double end);
    static PassRefPtr<TimeRanges> create(const PlatformTimeRanges&);

    double start(unsigned index, ExceptionCode&) const;
    double end(unsigned index, ExceptionCode&) const;

    PassRefPtr<TimeRanges> copy() const;
    void invert();
    void intersectWith(const TimeRanges&);
    void unionWith(const TimeRanges&);
    
    unsigned length() const;

    void add(double start, double end);
    bool contain(double time) const;
    
    size_t find(double time) const;
    double nearest(double time) const;
    double totalDuration() const;

    const PlatformTimeRanges& ranges() const { return m_ranges; }

private:
    explicit TimeRanges();
    TimeRanges(double start, double end);
    TimeRanges(const PlatformTimeRanges&);


    PlatformTimeRanges m_ranges;
};

} // namespace WebCore

#endif
