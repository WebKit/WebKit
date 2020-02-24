/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "JSImmutableButterfly.h"
#include "JSPropertyNameEnumerator.h"
#include "JSString.h"
#include "StructureRareData.h"

namespace JSC {

inline void StructureRareData::setPreviousID(VM& vm, Structure* structure)
{
    m_previous.set(vm, this, structure);
}

inline void StructureRareData::clearPreviousID()
{
    m_previous.clear();
}

inline JSString* StructureRareData::objectToStringValue() const
{
    auto* value = m_objectToStringValue.unvalidatedGet();
    if (value == objectToStringCacheGiveUpMarker())
        return nullptr;
    if (value)
        validateCell(value);
    return value;
}

inline JSPropertyNameEnumerator* StructureRareData::cachedPropertyNameEnumerator() const
{
    return m_cachedPropertyNameEnumerator.get();
}

inline void StructureRareData::setCachedPropertyNameEnumerator(VM& vm, JSPropertyNameEnumerator* enumerator)
{
    m_cachedPropertyNameEnumerator.set(vm, this, enumerator);
}

inline JSImmutableButterfly* StructureRareData::cachedOwnKeys() const
{
    ASSERT(!isCompilationThread());
    auto* butterfly = m_cachedOwnKeys.unvalidatedGet();
    if (butterfly == cachedOwnKeysSentinel())
        return nullptr;
    return butterfly;
}

inline JSImmutableButterfly* StructureRareData::cachedOwnKeysIgnoringSentinel() const
{
    ASSERT(!isCompilationThread());
    return m_cachedOwnKeys.unvalidatedGet();
}

inline JSImmutableButterfly* StructureRareData::cachedOwnKeysConcurrently() const
{
    auto* butterfly = m_cachedOwnKeys.unvalidatedGet();
    if (butterfly == cachedOwnKeysSentinel())
        return nullptr;
    return butterfly;
}

inline void StructureRareData::setCachedOwnKeys(VM& vm, JSImmutableButterfly* butterfly)
{
    if (butterfly == cachedOwnKeysSentinel()) {
        m_cachedOwnKeys.setWithoutWriteBarrier(butterfly);
        return;
    }

    WTF::storeStoreFence();
    m_cachedOwnKeys.set(vm, this, butterfly);
}

} // namespace JSC
