/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "IntersectionObserverEntry.h"

#include "Element.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(IntersectionObserverEntry);

IntersectionObserverEntry::IntersectionObserverEntry(Init&& init)
    : m_time(init.time)
    , m_rootBounds(init.rootBounds ? DOMRectReadOnly::fromRect(*init.rootBounds).ptr() : nullptr)
    , m_boundingClientRect(DOMRectReadOnly::fromRect(init.boundingClientRect))
    , m_intersectionRect(DOMRectReadOnly::fromRect(init.intersectionRect))
    , m_intersectionRatio(init.intersectionRatio)
    , m_target(WTF::move(init.target))
    , m_isIntersecting(init.isIntersecting)
{
}

TextStream& operator<<(TextStream& ts, const IntersectionObserverEntry& entry)
{
    TextStream::GroupScope scope(ts);
    ts << "IntersectionObserverEntry "_s << &entry << " target " << entry.target();
    ts.dumpProperty("time"_s, entry.time());

    if (entry.rootBounds())
        ts.dumpProperty("rootBounds"_s, entry.rootBounds()->toFloatRect());

    ts.dumpProperty("boundingClientRect"_s, entry.boundingClientRect().toFloatRect());
    ts.dumpProperty("intersectionRect"_s, entry.intersectionRect().toFloatRect());
    ts.dumpProperty("isIntersecting"_s, entry.isIntersecting());
    ts.dumpProperty("intersectionRatio"_s, entry.intersectionRatio());

    return ts;
}

} // namespace WebCore
