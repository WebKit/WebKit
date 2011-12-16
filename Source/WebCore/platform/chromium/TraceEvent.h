/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef TraceEvent_h
#define TraceEvent_h

#include "PlatformSupport.h"
#include <wtf/OwnArrayPtr.h>

// Implementation detail: trace event macros create temporary variables
// to keep instrumentation overhead low. These macros give each temporary
// variable a unique name based on the line number to prevent name collissions.
#define TRACE_EVENT_MAKE_UNIQUE_IDENTIFIER3(a, b) a##b
#define TRACE_EVENT_MAKE_UNIQUE_IDENTIFIER2(a, b) TRACE_EVENT_MAKE_UNIQUE_IDENTIFIER3(a, b)
#define TRACE_EVENT_MAKE_UNIQUE_IDENTIFIER(name_prefix) TRACE_EVENT_MAKE_UNIQUE_IDENTIFIER2(name_prefix, __LINE__)

// Issues PlatformSupport::traceEventBegin and traceEventEnd calls for the enclosing scope
#define TRACE_EVENT(name, id, extra) WebCore::internal::ScopeTracer TRACE_EVENT_MAKE_UNIQUE_IDENTIFIER(__traceEventScope)(name, id, extra);

namespace WebCore {

namespace internal {

// Used by TRACE_EVENT macro. Do not use directly.
class ScopeTracer {
public:
    ScopeTracer(const char* name, void*, const char* extra);
    ~ScopeTracer();

private:
    const char* m_name;
    void* m_id;
    OwnArrayPtr<char> m_extra;
};

inline ScopeTracer::ScopeTracer(const char* name, void* id, const char* extra)
    : m_name(name)
    , m_id(id)
{
    PlatformSupport::traceEventBegin(name, id, extra); \
    if (extra && PlatformSupport::isTraceEventEnabled())
        m_extra = adoptArrayPtr(strdup(extra));
}

inline ScopeTracer::~ScopeTracer()
{
    PlatformSupport::traceEventEnd(m_name, m_id, m_extra.get());
}

} // namespace internal

} // namespace WebCore

#endif
