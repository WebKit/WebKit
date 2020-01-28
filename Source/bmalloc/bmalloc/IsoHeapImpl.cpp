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

#include "IsoHeapImpl.h"

#include "AllIsoHeaps.h"
#include "PerProcess.h"
#include <climits>

namespace bmalloc {

IsoHeapImplBase::IsoHeapImplBase(Mutex& lock)
    : lock(lock)
{
}

IsoHeapImplBase::~IsoHeapImplBase()
{
}

void IsoHeapImplBase::addToAllIsoHeaps()
{
    AllIsoHeaps::get()->add(this);
}

void IsoHeapImplBase::scavengeNow()
{
    Vector<DeferredDecommit> deferredDecommits;
    scavenge(deferredDecommits);
    finishScavenging(deferredDecommits);
}

void IsoHeapImplBase::finishScavenging(Vector<DeferredDecommit>& deferredDecommits)
{
    std::sort(
        deferredDecommits.begin(), deferredDecommits.end(),
        [&] (const DeferredDecommit& a, const DeferredDecommit& b) -> bool {
            return a.page < b.page;
        });
    unsigned runStartIndex = UINT_MAX;
    char* run = nullptr;
    size_t size = 0;
    auto resetRun = [&] (unsigned endIndex) {
        if (!run) {
            RELEASE_BASSERT(!size);
            RELEASE_BASSERT(runStartIndex == UINT_MAX);
            return;
        }
        RELEASE_BASSERT(size);
        RELEASE_BASSERT(runStartIndex != UINT_MAX);
        vmDeallocatePhysicalPages(run, size);
        for (unsigned i = runStartIndex; i < endIndex; ++i) {
            const DeferredDecommit& value = deferredDecommits[i];
            value.directory->didDecommit(value.pageIndex);
        }
        run = nullptr;
        size = 0;
        runStartIndex = UINT_MAX;
    };
    for (unsigned i = 0; i < deferredDecommits.size(); ++i) {
        const DeferredDecommit& value = deferredDecommits[i];
        char* page = reinterpret_cast<char*>(value.page);
        RELEASE_BASSERT(page >= run + size);
        if (page != run + size) {
            resetRun(i);
            runStartIndex = i;
            run = page;
        }
        size += IsoPageBase::pageSize;
    }
    resetRun(deferredDecommits.size());
}

} // namespace bmalloc

