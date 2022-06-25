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

#pragma once

#include "IndexingType.h"
#include "JSArray.h"

namespace JSC {

class ArrayAllocationProfile {
public:
    ArrayAllocationProfile() = default;

    ArrayAllocationProfile(IndexingType recommendedIndexingMode)
    {
        initializeIndexingMode(recommendedIndexingMode);
    }

    IndexingType selectIndexingTypeConcurrently()
    {
        return m_currentIndexingType;
    }

    IndexingType selectIndexingType()
    {
        ASSERT(!isCompilationThread());
        JSArray* lastArray = m_lastArray;
        if (lastArray && UNLIKELY(lastArray->indexingType() != m_currentIndexingType))
            updateProfile();
        return m_currentIndexingType;
    }

    // vector length hint becomes [0, BASE_CONTIGUOUS_VECTOR_LEN_MAX].
    unsigned vectorLengthHintConcurrently()
    {
        return m_largestSeenVectorLength;
    }

    unsigned vectorLengthHint()
    {
        ASSERT(!isCompilationThread());
        JSArray* lastArray = m_lastArray;
        if (lastArray && (m_largestSeenVectorLength != BASE_CONTIGUOUS_VECTOR_LEN_MAX) && UNLIKELY(lastArray->getVectorLength() > m_largestSeenVectorLength))
            updateProfile();
        return m_largestSeenVectorLength;
    }
    
    JSArray* updateLastAllocation(JSArray* lastArray)
    {
        m_lastArray = lastArray;
        return lastArray;
    }
    
    JS_EXPORT_PRIVATE void updateProfile();

    static IndexingType selectIndexingTypeFor(ArrayAllocationProfile* profile)
    {
        if (!profile)
            return ArrayWithUndecided;
        return profile->selectIndexingType();
    }
    
    static JSArray* updateLastAllocationFor(ArrayAllocationProfile* profile, JSArray* lastArray)
    {
        if (profile)
            profile->updateLastAllocation(lastArray);
        return lastArray;
    }

    void initializeIndexingMode(IndexingType recommendedIndexingMode) { m_currentIndexingType = recommendedIndexingMode; }

private:
    
    IndexingType m_currentIndexingType { ArrayWithUndecided };
    unsigned m_largestSeenVectorLength { 0 };
    JSArray* m_lastArray { nullptr };
};

} // namespace JSC
