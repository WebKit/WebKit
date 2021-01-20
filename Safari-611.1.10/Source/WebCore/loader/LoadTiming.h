/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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

#pragma once

#include <wtf/Forward.h>
#include <wtf/MonotonicTime.h>
#include <wtf/WallTime.h>

namespace WebCore {

class LoadTiming {
public:
    Seconds secondsSinceStartTime(MonotonicTime) const;
    WallTime monotonicTimeToPseudoWallTime(MonotonicTime) const;

    void markStartTime();
    void addRedirect(const URL& redirectingUrl, const URL& redirectedUrl);

    void markStartTimeAndFetchStart() { markStartTime(); m_fetchStart = m_startTime; }

    void markUnloadEventStart() { m_unloadEventStart = MonotonicTime::now(); }
    void markUnloadEventEnd() { m_unloadEventEnd = MonotonicTime::now(); }
    void markRedirectStart() { m_redirectStart = MonotonicTime::now(); }
    void markRedirectEnd() { m_redirectEnd = MonotonicTime::now(); }
    void markFetchStart() { m_fetchStart = MonotonicTime::now(); }
    void setResponseEnd(MonotonicTime time) { m_responseEnd = time; }
    void markLoadEventStart() { m_loadEventStart = MonotonicTime::now(); }
    void markLoadEventEnd() { m_loadEventEnd = MonotonicTime::now(); }

    void setHasSameOriginAsPreviousDocument(bool value) { m_hasSameOriginAsPreviousDocument = value; }

    MonotonicTime startTime() const { return m_startTime; }
    MonotonicTime unloadEventStart() const { return m_unloadEventStart; }
    MonotonicTime unloadEventEnd() const { return m_unloadEventEnd; }
    MonotonicTime redirectStart() const { return m_redirectStart; }
    MonotonicTime redirectEnd() const { return m_redirectEnd; }
    MonotonicTime fetchStart() const { return m_fetchStart; }
    MonotonicTime responseEnd() const { return m_responseEnd; }
    MonotonicTime loadEventStart() const { return m_loadEventStart; }
    MonotonicTime loadEventEnd() const { return m_loadEventEnd; }
    short redirectCount() const { return m_redirectCount; }
    bool hasCrossOriginRedirect() const { return m_hasCrossOriginRedirect; }
    bool hasSameOriginAsPreviousDocument() const { return m_hasSameOriginAsPreviousDocument; }

    MonotonicTime referenceMonotonicTime() const { return m_referenceMonotonicTime; }
    WallTime referenceWallTime() const { return m_referenceWallTime; }

    LoadTiming isolatedCopy() const;

private:
    WallTime m_referenceWallTime;
    MonotonicTime m_referenceMonotonicTime;
    MonotonicTime m_startTime;
    MonotonicTime m_unloadEventStart;
    MonotonicTime m_unloadEventEnd;
    MonotonicTime m_redirectStart;
    MonotonicTime m_redirectEnd;
    MonotonicTime m_fetchStart;
    MonotonicTime m_responseEnd;
    MonotonicTime m_loadEventStart;
    MonotonicTime m_loadEventEnd;
    short m_redirectCount { 0 };
    bool m_hasCrossOriginRedirect { false };
    bool m_hasSameOriginAsPreviousDocument { false };
};

} // namespace WebCore
