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

#include <wtf/CurrentTime.h>

namespace WebCore {

class Frame;
class URL;

class LoadTiming {
public:

    double monotonicTimeToZeroBasedDocumentTime(double) const;
    double monotonicTimeToPseudoWallTime(double) const;

    void markStartTime();
    void addRedirect(const URL& redirectingUrl, const URL& redirectedUrl);

    void markStartTimeAndFetchStart() { markStartTime(); m_fetchStart = m_startTime; }

    void markUnloadEventStart() { m_unloadEventStart = monotonicallyIncreasingTime(); }
    void markUnloadEventEnd() { m_unloadEventEnd = monotonicallyIncreasingTime(); }
    void markRedirectStart() { m_redirectStart = monotonicallyIncreasingTime(); }
    void markRedirectEnd() { m_redirectEnd = monotonicallyIncreasingTime(); }
    void markFetchStart() { m_fetchStart = monotonicallyIncreasingTime(); }
    void setResponseEnd(double monotonicTime) { m_responseEnd = monotonicTime; }
    void markLoadEventStart() { m_loadEventStart = monotonicallyIncreasingTime(); }
    void markLoadEventEnd() { m_loadEventEnd = monotonicallyIncreasingTime(); }

    void setHasSameOriginAsPreviousDocument(bool value) { m_hasSameOriginAsPreviousDocument = value; }

    double startTime() const { return m_startTime; }
    double unloadEventStart() const { return m_unloadEventStart; }
    double unloadEventEnd() const { return m_unloadEventEnd; }
    double redirectStart() const { return m_redirectStart; }
    double redirectEnd() const { return m_redirectEnd; }
    short redirectCount() const { return m_redirectCount; }
    double fetchStart() const { return m_fetchStart; }
    double responseEnd() const { return m_responseEnd; }
    double loadEventStart() const { return m_loadEventStart; }
    double loadEventEnd() const { return m_loadEventEnd; }
    bool hasCrossOriginRedirect() const { return m_hasCrossOriginRedirect; }
    bool hasSameOriginAsPreviousDocument() const { return m_hasSameOriginAsPreviousDocument; }

    double referenceMonotonicTime() const { return m_referenceMonotonicTime; }
    double referenceWallTime() const { return m_referenceWallTime; }

private:
    double m_referenceMonotonicTime { 0.0 };
    double m_referenceWallTime { 0.0 };
    double m_startTime { 0.0 };
    double m_unloadEventStart { 0.0 };
    double m_unloadEventEnd { 0.0 };
    double m_redirectStart { 0.0 };
    double m_redirectEnd { 0.0 };
    short m_redirectCount { 0 };
    double m_fetchStart { 0.0 };
    double m_responseEnd { 0.0 };
    double m_loadEventStart { 0.0 };
    double m_loadEventEnd { 0.0 };
    bool m_hasCrossOriginRedirect { false };
    bool m_hasSameOriginAsPreviousDocument { false };
};

} // namespace WebCore
