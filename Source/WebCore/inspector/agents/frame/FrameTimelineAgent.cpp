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
#include "FrameTimelineAgent.h"

#include "InspectorWebAgentBase.h"
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameTimelineAgent);

FrameTimelineAgent::FrameTimelineAgent(FrameAgentContext& context)
    : InspectorTimelineAgent(context)
{
}

FrameTimelineAgent::~FrameTimelineAgent() = default;

Inspector::Protocol::ErrorStringOr<void> FrameTimelineAgent::setAutoCaptureEnabled(bool)
{
    return makeUnexpected("Not supported"_s);
}

bool FrameTimelineAgent::shouldToggleInstrument(Inspector::Protocol::Timeline::Instrument instrument) const
{
    // The other instruments belong to domains that frame targets do not have yet, so their getters would reach
    // the page's agents.
    // FIXME: <https://webkit.org/b/329360> Also toggle ScriptProfiler once frame targets have that domain.
    return instrument == Inspector::Protocol::Timeline::Instrument::Timeline;
}

Inspector::Protocol::DOM::NodeId FrameTimelineAgent::nodeIdForNode(Node*)
{
    // FIXME: <https://webkit.org/b/329361> Push the node through FrameDOMAgent once the frontend resolves
    // node IDs in Timeline records through the record's target.
    return 0;
}

} // namespace WebCore
