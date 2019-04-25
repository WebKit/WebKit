/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "IsoHeap.h"
#include "IsoPage.h"
#include "IsoSharedConfig.h"

namespace bmalloc {

class IsoHeapImplBase;

class IsoSharedPage : public IsoPageBase {
public:
    BEXPORT static IsoSharedPage* tryCreate();

    template<typename Config, typename Type>
    void free(const std::lock_guard<Mutex>&, api::IsoHeap<Type>&, void*);
    VariadicBumpAllocator startAllocating();
    void stopAllocating();

private:
    IsoSharedPage()
        : IsoPageBase(true)
    {
    }
};

template<typename Config>
uint8_t* indexSlotFor(void* ptr)
{
    BASSERT(IsoPageBase::pageFor(ptr)->isShared());
    return static_cast<uint8_t*>(ptr) + Config::objectSize;
}

} // namespace bmalloc

