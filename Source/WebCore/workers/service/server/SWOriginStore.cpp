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

#include "config.h"
#include "SWOriginStore.h"

#if ENABLE(SERVICE_WORKER)

#include "SecurityOrigin.h"

namespace WebCore {

void SWOriginStore::add(const SecurityOriginData& origin)
{
    ++m_originCounts.ensure(origin, [&] {
        addToStore(origin);
        return 0;
    }).iterator->value;
}

void SWOriginStore::remove(const SecurityOriginData& origin)
{
    auto iterator = m_originCounts.find(origin);
    ASSERT(iterator != m_originCounts.end());
    if (iterator == m_originCounts.end())
        return;

    if (--iterator->value)
        return;

    m_originCounts.remove(iterator);
    removeFromStore(origin);
}

void SWOriginStore::clear(const SecurityOriginData& origin)
{
    m_originCounts.remove(origin);
    removeFromStore(origin);
}

void SWOriginStore::clearAll()
{
    m_originCounts.clear();
    clearStore();
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
