/*
 * Copyright (C) 2011 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LoadTiming.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "SecurityOrigin.h"
#include <wtf/RefPtr.h>

namespace WebCore {

Seconds LoadTiming::secondsSinceStartTime(MonotonicTime timeStamp) const
{
    if (!timeStamp)
        return Seconds(0);

    return timeStamp - m_referenceMonotonicTime;
}

WallTime LoadTiming::monotonicTimeToPseudoWallTime(MonotonicTime timeStamp) const
{
    if (!timeStamp)
        return WallTime::fromRawSeconds(0);

    return m_referenceWallTime + (timeStamp - m_referenceMonotonicTime);
}

void LoadTiming::markStartTime()
{
    ASSERT(!m_startTime && !m_referenceMonotonicTime && !m_referenceWallTime);

    m_referenceMonotonicTime = m_startTime = MonotonicTime::now();
    m_referenceWallTime = WallTime::now();
}

void LoadTiming::addRedirect(const URL& redirectingUrl, const URL& redirectedUrl)
{
    m_redirectCount++;
    if (!m_redirectStart)
        m_redirectStart = m_fetchStart;
    m_redirectEnd = m_fetchStart = MonotonicTime::now();
    // Check if the redirected url is allowed to access the redirecting url's timing information.
    Ref<SecurityOrigin> redirectedSecurityOrigin(SecurityOrigin::create(redirectedUrl));
    m_hasCrossOriginRedirect = !redirectedSecurityOrigin.get().canRequest(redirectingUrl);
}

LoadTiming LoadTiming::isolatedCopy() const
{
    LoadTiming result;

    result.m_referenceWallTime = m_referenceWallTime;
    result.m_referenceMonotonicTime = m_referenceMonotonicTime;
    result.m_startTime = m_startTime;
    result.m_unloadEventStart = m_unloadEventStart;
    result.m_unloadEventEnd = m_unloadEventEnd;
    result.m_redirectStart = m_redirectStart;
    result.m_redirectEnd = m_redirectEnd;
    result.m_fetchStart = m_fetchStart;
    result.m_responseEnd = m_responseEnd;
    result.m_loadEventStart = m_loadEventStart;
    result.m_loadEventEnd = m_loadEventEnd;
    result.m_redirectCount = m_redirectCount;
    result.m_hasCrossOriginRedirect = m_hasCrossOriginRedirect;
    result.m_hasSameOriginAsPreviousDocument = m_hasSameOriginAsPreviousDocument;

    return result;
}

} // namespace WebCore
