/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ArrayAllocationProfile.h"

#include "JSCellInlines.h"
#include <algorithm>

namespace JSC {

void ArrayAllocationProfile::updateProfile()
{
    // This is awkwardly racy but totally sound even when executed concurrently. The
    // worst cases go something like this:
    //
    // - Two threads race to execute this code; one of them succeeds in updating the
    //   m_currentIndexingType and the other either updates it again, or sees a null
    //   m_lastArray; if it updates it again then at worst it will cause the profile
    //   to "forget" some array. That's still sound, since we don't promise that
    //   this profile is a reflection of any kind of truth.
    //
    // - A concurrent thread reads m_lastArray, but that array is now dead. While
    //   it's possible for that array to no longer be reachable, it cannot actually
    //   be freed, since we require the GC to wait until all concurrent JITing
    //   finishes.
    //
    // But one exception is vector length. We access vector length to get the vector
    // length hint. However vector length can be accessible only from the main
    // thread because large butterfly can be realloced in the main thread.
    // So for now, we update the allocation profile only from the main thread.
    
    ASSERT(!isCompilationThread());
    JSArray* lastArray = m_lastArray;
    if (!lastArray)
        return;
    if (LIKELY(Options::useArrayAllocationProfiling())) {
        // The basic model here is that we will upgrade ourselves to whatever the CoW version of lastArray is except ArrayStorage since we don't have CoW ArrayStorage.
        IndexingType indexingType = leastUpperBoundOfIndexingTypes(m_currentIndexingType & IndexingTypeMask, lastArray->indexingType());
        if (isCopyOnWrite(m_currentIndexingType)) {
            if (indexingType > ArrayWithContiguous)
                indexingType = ArrayWithContiguous;
            indexingType |= CopyOnWrite;
        }
        m_currentIndexingType = indexingType;
        m_largestSeenVectorLength = std::min(std::max(m_largestSeenVectorLength, lastArray->getVectorLength()), BASE_CONTIGUOUS_VECTOR_LEN_MAX);
    }
    m_lastArray = nullptr;
}

} // namespace JSC

