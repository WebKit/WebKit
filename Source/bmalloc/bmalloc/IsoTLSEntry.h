/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "BMalloced.h"

namespace bmalloc {

class IsoTLS;
class IsoTLSLayout;

class BEXPORT IsoTLSEntry {
    MAKE_BMALLOCED;
public:
    IsoTLSEntry(size_t alignment, size_t size);
    virtual ~IsoTLSEntry();
    
    size_t offset() const { return m_offset; }
    size_t alignment() const { return m_alignment; }
    size_t size() const { return m_size; }
    size_t extent() const { return m_offset + m_size; }
    
    virtual void construct(void* entry) = 0;
    virtual void move(void* src, void* dst) = 0;
    virtual void destruct(void* entry) = 0;
    virtual void scavenge(void* entry) = 0;
    
    template<typename Func>
    void walkUpToInclusive(IsoTLSEntry*, const Func&);
    
private:
    friend class IsoTLS;
    friend class IsoTLSLayout;

    IsoTLSEntry* m_next { nullptr };
    
    size_t m_offset; // Computed in constructor.
    size_t m_alignment;
    size_t m_size;
};

template<typename EntryType>
class DefaultIsoTLSEntry : public IsoTLSEntry {
public:
    DefaultIsoTLSEntry();
    ~DefaultIsoTLSEntry();
    
protected:
    // This clones src onto dst and then destructs src. Therefore, entry destructors cannot do
    // scavenging.
    void move(void* src, void* dst) override;
    
    // Likewise, this is separate from scavenging. When the TLS is shutting down, we will be asked to
    // scavenge and then we will be asked to destruct.
    void destruct(void* entry) override;

    void scavenge(void* entry) override;
};

} // namespace bmalloc

