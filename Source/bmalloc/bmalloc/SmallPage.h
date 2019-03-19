/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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

#ifndef SmallPage_h
#define SmallPage_h

#include "BAssert.h"
#include "List.h"
#include "Mutex.h"
#include "VMAllocate.h"
#include <mutex>

namespace bmalloc {

class SmallLine;

class SmallPage : public ListNode<SmallPage> {
public:
    void ref(std::unique_lock<Mutex>&);
    bool deref(std::unique_lock<Mutex>&);
    unsigned refCount(std::unique_lock<Mutex>&) { return m_refCount; }
    
    size_t sizeClass() { return m_sizeClass; }
    void setSizeClass(size_t sizeClass) { m_sizeClass = sizeClass; }
    
    bool hasFreeLines(std::unique_lock<Mutex>&) const { return m_hasFreeLines; }
    void setHasFreeLines(std::unique_lock<Mutex>&, bool hasFreeLines) { m_hasFreeLines = hasFreeLines; }
    
    bool hasPhysicalPages() { return m_hasPhysicalPages; }
    void setHasPhysicalPages(bool hasPhysicalPages) { m_hasPhysicalPages = hasPhysicalPages; }
    
    bool usedSinceLastScavenge() { return m_usedSinceLastScavenge; }
    void clearUsedSinceLastScavenge() { m_usedSinceLastScavenge = false; }
    void setUsedSinceLastScavenge() { m_usedSinceLastScavenge = true; }

    SmallLine* begin();

    unsigned char slide() const { return m_slide; }
    void setSlide(unsigned char slide) { m_slide = slide; }
    
private:
    unsigned char m_hasFreeLines: 1;
    unsigned char m_hasPhysicalPages: 1;
    unsigned char m_usedSinceLastScavenge: 1;
    unsigned char m_refCount: 7;
    unsigned char m_sizeClass;
    unsigned char m_slide;

static_assert(
    sizeClassCount <= std::numeric_limits<decltype(m_sizeClass)>::max(),
    "Largest size class must fit in SmallPage metadata");
};

using LineCache = std::array<List<SmallPage>, sizeClassCount>;

inline void SmallPage::ref(std::unique_lock<Mutex>&)
{
    BASSERT(!m_slide);
    ++m_refCount;
    BASSERT(m_refCount);
}

inline bool SmallPage::deref(std::unique_lock<Mutex>&)
{
    BASSERT(!m_slide);
    BASSERT(m_refCount);
    --m_refCount;
    return !m_refCount;
}

} // namespace bmalloc

#endif // SmallPage_h
