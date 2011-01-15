/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "InjectedBundleRangeHandle.h"

#include <WebCore/Range.h>
#include <wtf/HashMap.h>

using namespace WebCore;

namespace WebKit {

typedef HashMap<Range*, InjectedBundleRangeHandle*> DOMHandleCache;

static DOMHandleCache& domHandleCache()
{
    DEFINE_STATIC_LOCAL(DOMHandleCache, cache, ());
    return cache;
}

PassRefPtr<InjectedBundleRangeHandle> InjectedBundleRangeHandle::getOrCreate(Range* range)
{
    if (!range)
        return 0;

    std::pair<DOMHandleCache::iterator, bool> result = domHandleCache().add(range, 0);
    if (!result.second)
        return PassRefPtr<InjectedBundleRangeHandle>(result.first->second);

    RefPtr<InjectedBundleRangeHandle> rangeHandle = InjectedBundleRangeHandle::create(range);
    result.first->second = rangeHandle.get();
    return rangeHandle.release();
}

PassRefPtr<InjectedBundleRangeHandle> InjectedBundleRangeHandle::create(Range* range)
{
    return adoptRef(new InjectedBundleRangeHandle(range));
}

InjectedBundleRangeHandle::InjectedBundleRangeHandle(Range* range)
    : m_range(range)
{
}

InjectedBundleRangeHandle::~InjectedBundleRangeHandle()
{
    domHandleCache().remove(m_range.get());
}

Range* InjectedBundleRangeHandle::coreRange() const
{
    return m_range.get();
}

} // namespace WebKit
