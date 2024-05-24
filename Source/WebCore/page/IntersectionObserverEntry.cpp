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
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(IntersectionObserverEntry);

IntersectionObserverEntry::IntersectionObserverEntry(Init&& init)
    : m_time(WTFMove(init.time))
    , m_boundingClientRect(DOMRectReadOnly::fromRect(WTFMove(init.boundingClientRect)))
    , m_intersectionRect(DOMRectReadOnly::fromRect(WTFMove(init.intersectionRect)))
    , m_intersectionRatio(WTFMove(init.intersectionRatio))
    , m_target(WTFMove(init.target))
    , m_isIntersecting(WTFMove(init.isIntersecting))
{
    if (init.rootBounds)
        m_rootBounds = DOMRectReadOnly::fromRect(WTFMove(*init.rootBounds));
}

TextStream& operator<<(TextStream& ts, const IntersectionObserverEntry& entry)
{
    TextStream::GroupScope scope(ts);
    ts << "IntersectionObserverEntry " << &entry;
    ts.dumpProperty("time", entry.time());
    
    if (entry.rootBounds())
        ts.dumpProperty("rootBounds", entry.rootBounds()->toFloatRect());

    ts.dumpProperty("boundingClientRect", entry.boundingClientRect().toFloatRect());
    ts.dumpProperty("intersectionRect", entry.intersectionRect().toFloatRect());
    ts.dumpProperty("isIntersecting", entry.isIntersecting());
    ts.dumpProperty("intersectionRatio", entry.intersectionRatio());

    return ts;
}

} // namespace WebCore
