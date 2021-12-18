/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PerformanceNavigationTiming.h"

#include "CachedResource.h"
#include "ResourceTiming.h"

namespace WebCore {

static PerformanceNavigationTiming::NavigationType toPerformanceNavigationTimingNavigationType(NavigationType navigationType)
{
    switch (navigationType) {
    case NavigationType::BackForward:
        return PerformanceNavigationTiming::NavigationType::Back_forward;
    case NavigationType::Reload:
        return PerformanceNavigationTiming::NavigationType::Reload;
    case NavigationType::LinkClicked:
    case NavigationType::FormSubmitted:
    case NavigationType::FormResubmitted:
    case NavigationType::Other:
        return PerformanceNavigationTiming::NavigationType::Navigate;
    }
    ASSERT_NOT_REACHED();
    return PerformanceNavigationTiming::NavigationType::Navigate;
}

PerformanceNavigationTiming::PerformanceNavigationTiming(MonotonicTime timeOrigin, CachedResource& resource, const DocumentLoadTiming& documentLoadTiming, const NetworkLoadMetrics& metrics, const DocumentEventTiming& documentEventTiming, const SecurityOrigin& origin, WebCore::NavigationType navigationType)
    : PerformanceResourceTiming(timeOrigin, ResourceTiming::fromLoad(resource, resource.response().url(), "navigation"_s, documentLoadTiming, metrics, origin))
    , m_documentEventTiming(documentEventTiming)
    , m_documentLoadTiming(documentLoadTiming)
    , m_navigationType(toPerformanceNavigationTimingNavigationType(navigationType)) { }

PerformanceNavigationTiming::~PerformanceNavigationTiming() = default;

double PerformanceNavigationTiming::millisecondsSinceOrigin(MonotonicTime time) const
{
    if (!time)
        return 0;
    return Performance::reduceTimeResolution(time - m_timeOrigin).milliseconds();
}

bool PerformanceNavigationTiming::sameOriginCheckFails() const
{
    // https://www.w3.org/TR/navigation-timing-2/#dfn-same-origin-check
    return m_resourceTiming.networkLoadMetrics().hasCrossOriginRedirect
        || !m_documentLoadTiming.hasSameOriginAsPreviousDocument();
}

double PerformanceNavigationTiming::unloadEventStart() const
{
    if (sameOriginCheckFails())
        return 0.0;
    return millisecondsSinceOrigin(m_documentLoadTiming.unloadEventStart());
}

double PerformanceNavigationTiming::unloadEventEnd() const
{
    if (sameOriginCheckFails())
        return 0.0;
    return millisecondsSinceOrigin(m_documentLoadTiming.unloadEventEnd());
}

double PerformanceNavigationTiming::domInteractive() const
{
    return millisecondsSinceOrigin(m_documentEventTiming.domInteractive);
}

double PerformanceNavigationTiming::domContentLoadedEventStart() const
{
    return millisecondsSinceOrigin(m_documentEventTiming.domContentLoadedEventStart);
}

double PerformanceNavigationTiming::domContentLoadedEventEnd() const
{
    return millisecondsSinceOrigin(m_documentEventTiming.domContentLoadedEventEnd);
}

double PerformanceNavigationTiming::domComplete() const
{
    return millisecondsSinceOrigin(m_documentEventTiming.domComplete);
}

double PerformanceNavigationTiming::loadEventStart() const
{
    return millisecondsSinceOrigin(m_documentLoadTiming.loadEventStart());
}

double PerformanceNavigationTiming::loadEventEnd() const
{
    return millisecondsSinceOrigin(m_documentLoadTiming.loadEventEnd());
}

PerformanceNavigationTiming::NavigationType PerformanceNavigationTiming::type() const
{
    return m_navigationType;
}

unsigned short PerformanceNavigationTiming::redirectCount() const
{
    if (m_resourceTiming.networkLoadMetrics().hasCrossOriginRedirect)
        return 0;

    return m_resourceTiming.networkLoadMetrics().redirectCount;
}

double PerformanceNavigationTiming::startTime() const
{
    // https://www.w3.org/TR/navigation-timing-2/#dom-PerformanceNavigationTiming-startTime
    return 0.0;
}

double PerformanceNavigationTiming::duration() const
{
    // https://www.w3.org/TR/navigation-timing-2/#dom-PerformanceNavigationTiming-duration
    return loadEventEnd() - startTime();
}

} // namespace WebCore
