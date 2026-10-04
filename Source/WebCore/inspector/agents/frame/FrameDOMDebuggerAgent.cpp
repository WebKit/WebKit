/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FrameDOMDebuggerAgent.h"

#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameDOMDebuggerAgent);

FrameDOMDebuggerAgent::FrameDOMDebuggerAgent(FrameAgentContext& context, InspectorDebuggerAgent* debuggerAgent)
    : InspectorDOMDebuggerAgent(context, debuggerAgent)
{
}

FrameDOMDebuggerAgent::~FrameDOMDebuggerAgent() = default;

// FIXME: <https://webkit.org/b/298981> Support DOM breakpoints on frame targets.
Inspector::Protocol::ErrorStringOr<void> FrameDOMDebuggerAgent::setDOMBreakpoint(Inspector::Protocol::DOM::NodeId, Inspector::Protocol::DOMDebugger::DOMBreakpointType, RefPtr<JSON::Object>&& /* options */)
{
    return makeUnexpected("Not supported for frame targets"_s);
}

Inspector::Protocol::ErrorStringOr<void> FrameDOMDebuggerAgent::removeDOMBreakpoint(Inspector::Protocol::DOM::NodeId, Inspector::Protocol::DOMDebugger::DOMBreakpointType)
{
    return makeUnexpected("Not supported for frame targets"_s);
}

} // namespace WebCore
