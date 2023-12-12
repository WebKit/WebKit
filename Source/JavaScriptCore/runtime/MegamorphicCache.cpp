/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "MegamorphicCache.h"

#include <wtf/TZoneMallocInlines.h>

namespace JSC {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(MegamorphicCache);
WTF_MAKE_TZONE_ALLOCATED_IMPL(MegamorphicCache);

void MegamorphicCache::age(CollectionScope collectionScope)
{
    ++m_epoch;
    if (collectionScope == CollectionScope::Full || m_epoch == invalidEpoch) {
        for (auto& entry : m_primaryEntries) {
            entry.m_uid = nullptr;
            entry.m_epoch = invalidEpoch;
        }
        for (auto& entry : m_secondaryEntries) {
            entry.m_uid = nullptr;
            entry.m_epoch = invalidEpoch;
        }
        for (auto& entry : m_storeCachePrimaryEntries) {
            entry.m_uid = nullptr;
            entry.m_epoch = invalidEpoch;
        }
        for (auto& entry : m_storeCacheSecondaryEntries) {
            entry.m_uid = nullptr;
            entry.m_epoch = invalidEpoch;
        }
        if (m_epoch == invalidEpoch)
            m_epoch = 1;
    }
}

void MegamorphicCache::clearEntries()
{
    for (auto& entry : m_primaryEntries)
        entry.m_epoch = invalidEpoch;
    for (auto& entry : m_secondaryEntries)
        entry.m_epoch = invalidEpoch;
    for (auto& entry : m_storeCachePrimaryEntries)
        entry.m_epoch = invalidEpoch;
    for (auto& entry : m_storeCacheSecondaryEntries)
        entry.m_epoch = invalidEpoch;
    m_epoch = 1;
}

} // namespace JSC
