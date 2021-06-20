/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "IDBGetResult.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(IDBGetResult);

IDBGetResult::IDBGetResult(const IDBGetResult& that, IsolatedCopyTag)
{
    isolatedCopy(that, *this);
}

IDBGetResult IDBGetResult::isolatedCopy() const
{
    return { *this, IsolatedCopy };
}

void IDBGetResult::isolatedCopy(const IDBGetResult& source, IDBGetResult& destination)
{
    destination.m_value = source.m_value.isolatedCopy();
    destination.m_keyData = source.m_keyData.isolatedCopy();
    destination.m_primaryKeyData = source.m_primaryKeyData.isolatedCopy();
    destination.m_keyPath = WebCore::isolatedCopy(source.m_keyPath);
    destination.m_isDefined = source.m_isDefined;
    destination.m_prefetchedRecords = source.m_prefetchedRecords.isolatedCopy();
}

void IDBGetResult::setValue(IDBValue&& value)
{
    m_value = WTFMove(value);
}

} // namespace WebCore
