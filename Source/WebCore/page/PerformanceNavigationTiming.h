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

#pragma once

#include "DocumentEventTiming.h"
#include "DocumentLoadTiming.h"
#include "PerformanceResourceTiming.h"
#include <wtf/MonotonicTime.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CachedResource;
class NetworkLoadMetrics;
class SecurityOrigin;
enum class NavigationType : uint8_t;

class PerformanceNavigationTiming final : public PerformanceResourceTiming {
public:
    enum class NavigationType : uint8_t {
        Navigate,
        Reload,
        Back_forward,
        Prerender,
    };

    template<typename... Args> static Ref<PerformanceNavigationTiming> create(Args&&... args) { return adoptRef(*new PerformanceNavigationTiming(std::forward<Args>(args)...)); }
    ~PerformanceNavigationTiming();

    Type performanceEntryType() const final { return Type::Navigation; }
    ASCIILiteral entryType() const final { return "navigation"_s; }

    double unloadEventStart() const;
    double unloadEventEnd() const;
    double domInteractive() const;
    double domContentLoadedEventStart() const;
    double domContentLoadedEventEnd() const;
    double domComplete() const;
    double loadEventStart() const;
    double loadEventEnd() const;
    NavigationType type() const;
    unsigned short redirectCount() const;

    double startTime() const final;
    double duration() const final;

    DocumentEventTiming& documentEventTiming() { return m_documentEventTiming; }
    DocumentLoadTiming& documentLoadTiming() { return m_documentLoadTiming; }

private:
    PerformanceNavigationTiming(MonotonicTime timeOrigin, CachedResource&, const DocumentLoadTiming&, const NetworkLoadMetrics&, const DocumentEventTiming&, const SecurityOrigin&, WebCore::NavigationType);

    double millisecondsSinceOrigin(MonotonicTime) const;
    bool sameOriginCheckFails() const;

    DocumentEventTiming m_documentEventTiming;
    DocumentLoadTiming m_documentLoadTiming;
    NavigationType m_navigationType;
};

} // namespace WebCore
