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
#include "IsoDirectory.h"

namespace bmalloc {

template<typename Config>
class IsoDirectoryPage {
    MAKE_BMALLOCED;
public:
    // By my math, this results in an IsoDirectoryPage using 4036 bytes on 64-bit platforms. We allocate it
    // with malloc, so it doesn't really matter how much memory it uses. But it's nice that this about a
    // page, since that's quite intuitive.
    //
    // On 32-bit platforms, I think that this will be 2112 bytes. That's still quite intuitive.
    //
    // Note that this is a multiple of 32 so that it uses bitvectors efficiently.
    static constexpr unsigned numPages = 480;
    
    IsoDirectoryPage(IsoHeapImpl<Config>&, unsigned index);
    
    static IsoDirectoryPage* pageFor(IsoDirectory<Config, numPages>* payload);
    
    unsigned index() const { return m_index; }
    
    IsoDirectory<Config, numPages> payload;
    IsoDirectoryPage* next { nullptr };

private:
    unsigned m_index;
};

} // namespace bmalloc

