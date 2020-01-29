/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#include "BAssert.h"
#include "BMalloced.h"
#include "IsoTLSLayout.h"
#include <climits>

namespace bmalloc {

class IsoTLS;

template<typename Entry>
class IsoTLSEntryHolder {
    MAKE_BMALLOCED;
    IsoTLSEntryHolder(const IsoTLSEntryHolder&) = delete;
    IsoTLSEntryHolder& operator=(const IsoTLSEntryHolder&) = delete;
public:
    template<typename... Args>
    IsoTLSEntryHolder(Args&&... args)
        : m_entry(std::forward<Args>(args)...)
    {
        IsoTLSLayout::get()->add(&m_entry);
        RELEASE_BASSERT(m_entry.offset() != UINT_MAX);
    }

    inline const Entry& operator*() const { m_entry; }
    inline Entry& operator*() { m_entry; }
    inline const Entry* operator->() const { return &m_entry; }
    inline Entry* operator->() { return &m_entry; }

private:
    Entry m_entry;
};

class BEXPORT IsoTLSEntry {
    MAKE_BMALLOCED;
    IsoTLSEntry(const IsoTLSEntry&) = delete;
    IsoTLSEntry& operator=(const IsoTLSEntry&) = delete;
public:
    virtual ~IsoTLSEntry();
    
    size_t offset() const { return m_offset; }
    size_t alignment() const { return sizeof(void*); }
    size_t size() const { return m_size; }
    size_t extent() const { return m_offset + m_size; }
    
    virtual void construct(void* entry) = 0;
    virtual void move(void* src, void* dst) = 0;
    virtual void destruct(void* entry) = 0;
    virtual void scavenge(void* entry) = 0;
    
    template<typename Func>
    void walkUpToInclusive(IsoTLSEntry*, const Func&);

protected:
    IsoTLSEntry(size_t size);
    
private:
    friend class IsoTLS;
    friend class IsoTLSLayout;

    IsoTLSEntry* m_next { nullptr };
    
    unsigned m_offset { UINT_MAX }; // Computed in constructor.
    unsigned m_size;
};

template<typename EntryType>
class DefaultIsoTLSEntry : public IsoTLSEntry {
public:
    ~DefaultIsoTLSEntry() = default;
    
protected:
    DefaultIsoTLSEntry();

    // This clones src onto dst and then destructs src. Therefore, entry destructors cannot do
    // scavenging.
    void move(void* src, void* dst) override;
    
    // Likewise, this is separate from scavenging. When the TLS is shutting down, we will be asked to
    // scavenge and then we will be asked to destruct.
    void destruct(void* entry) override;
};

} // namespace bmalloc

