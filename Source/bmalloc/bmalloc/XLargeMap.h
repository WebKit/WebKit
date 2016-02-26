/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#ifndef XLargeMap_h
#define XLargeMap_h

#include "SortedVector.h"
#include "Vector.h"
#include "XLargeRange.h"
#include <algorithm>

namespace bmalloc {

class XLargeMap {
public:
    void addFree(const XLargeRange&);
    XLargeRange takeFree(size_t alignment, size_t);

    void addAllocated(const XLargeRange& prev, const std::pair<XLargeRange, XLargeRange>&, const XLargeRange& next);
    XLargeRange getAllocated(void*);
    XLargeRange takeAllocated(void*);

    XLargeRange takePhysical();
    void addVirtual(const XLargeRange&);
    
    void shrinkToFit();

private:
    struct Allocation {
        bool operator<(const Allocation& other) const { return object < other.object; }
        bool operator<(void* ptr) const { return object.begin() < ptr; }

        XLargeRange object;
        XLargeRange unused;
    };

    Vector<XLargeRange> m_free;
    SortedVector<Allocation> m_allocated;
};

} // namespace bmalloc

#endif // XLargeMap_h
