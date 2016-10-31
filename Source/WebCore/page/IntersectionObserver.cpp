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

#if ENABLE(INTERSECTION_OBSERVER)
#include "IntersectionObserver.h"

#include "Element.h"
#include "IntersectionObserverCallback.h"
#include "IntersectionObserverEntry.h"
#include <wtf/Vector.h>

namespace WebCore {

IntersectionObserver::IntersectionObserver(Ref<IntersectionObserverCallback>&& callback, Init&& init)
    : m_root(init.root)
    , m_rootMargin(WTFMove(init.rootMargin))
    , m_callback(WTFMove(callback))
{
    if (WTF::holds_alternative<double>(init.threshold))
        m_thresholds.append(WTF::get<double>(init.threshold));
    else
        m_thresholds = WTF::get<Vector<double>>(WTFMove(init.threshold));
}

void IntersectionObserver::observe(Element&)
{
}

void IntersectionObserver::unobserve(Element&)
{
}

void IntersectionObserver::disconnect()
{
}

Vector<RefPtr<IntersectionObserverEntry>> IntersectionObserver::takeRecords()
{
    return { };
}


} // namespace WebCore

#endif // ENABLE(INTERSECTION_OBSERVER)
