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
#include "FrameDOMAgent.h"

namespace WebCore {

using namespace Inspector;

#if ENABLE(INSPECTOR_ALTERNATE_DISPATCHERS)
Inspector::CommandResult<Ref<JSON::ArrayOf<Inspector::Protocol::DOM::DataBinding>>> FrameDOMAgent::getDataBindingsForNode(int)
{
    return makeUnexpected("Not supported for frame targets"_s);
}

Inspector::CommandResult<String> FrameDOMAgent::getAssociatedDataForNode(int)
{
    return makeUnexpected("Not supported for frame targets"_s);
}
#endif

Inspector::CommandResult<void> FrameDOMAgent::setBreakpointForEventListener(int, RefPtr<JSON::Object>&&)
{
    return makeUnexpected("Not supported for frame targets"_s);
}

Inspector::CommandResult<void> FrameDOMAgent::removeBreakpointForEventListener(int)
{
    return makeUnexpected("Not supported for frame targets"_s);
}

Inspector::CommandResult<Ref<Inspector::Protocol::DOM::AccessibilityProperties>> FrameDOMAgent::getAccessibilityPropertiesForNode(int)
{
    return makeUnexpected("Not yet implemented for frame targets"_s);
}

Inspector::CommandResult<int> FrameDOMAgent::requestNode(const String&)
{
    return makeUnexpected("Not yet implemented for frame targets"_s);
}

#if PLATFORM(IOS_FAMILY)
Inspector::CommandResult<void> FrameDOMAgent::setInspectModeEnabled(bool, RefPtr<JSON::Object>&&, RefPtr<JSON::Object>&&, RefPtr<JSON::Object>&&)
{
    return makeUnexpected("Not supported for frame targets"_s);
}
#else
Inspector::CommandResult<void> FrameDOMAgent::setInspectModeEnabled(bool, RefPtr<JSON::Object>&&, RefPtr<JSON::Object>&&, RefPtr<JSON::Object>&&, std::optional<bool>&&)
{
    return makeUnexpected("Not supported for frame targets"_s);
}
#endif

Inspector::CommandResult<void> FrameDOMAgent::highlightRect(int, int, int, int, RefPtr<JSON::Object>&&, RefPtr<JSON::Object>&&, std::optional<bool>&&)
{
    return makeUnexpected("Not supported for frame targets"_s);
}

Inspector::CommandResult<void> FrameDOMAgent::highlightQuad(Ref<JSON::Array>&&, RefPtr<JSON::Object>&&, RefPtr<JSON::Object>&&, std::optional<bool>&&)
{
    return makeUnexpected("Not supported for frame targets"_s);
}

#if PLATFORM(IOS_FAMILY)
Inspector::CommandResult<void> FrameDOMAgent::highlightSelector(const String&, const String&, Ref<JSON::Object>&&, RefPtr<JSON::Object>&&, RefPtr<JSON::Object>&&)
{
    return makeUnexpected("Not supported for frame targets"_s);
}

Inspector::CommandResult<void> FrameDOMAgent::highlightNode(std::optional<int>&&, const String&, Ref<JSON::Object>&&, RefPtr<JSON::Object>&&, RefPtr<JSON::Object>&&)
{
    return makeUnexpected("Not supported for frame targets"_s);
}

Inspector::CommandResult<void> FrameDOMAgent::highlightNodeList(Ref<JSON::Array>&&, Ref<JSON::Object>&&, RefPtr<JSON::Object>&&, RefPtr<JSON::Object>&&)
{
    return makeUnexpected("Not supported for frame targets"_s);
}
#else
Inspector::CommandResult<void> FrameDOMAgent::highlightSelector(const String&, const String&, Ref<JSON::Object>&&, RefPtr<JSON::Object>&&, RefPtr<JSON::Object>&&, std::optional<bool>&&)
{
    return makeUnexpected("Not supported for frame targets"_s);
}

Inspector::CommandResult<void> FrameDOMAgent::highlightNode(std::optional<int>&&, const String&, Ref<JSON::Object>&&, RefPtr<JSON::Object>&&, RefPtr<JSON::Object>&&, std::optional<bool>&&)
{
    return makeUnexpected("Not supported for frame targets"_s);
}

Inspector::CommandResult<void> FrameDOMAgent::highlightNodeList(Ref<JSON::Array>&&, Ref<JSON::Object>&&, RefPtr<JSON::Object>&&, RefPtr<JSON::Object>&&, std::optional<bool>&&)
{
    return makeUnexpected("Not supported for frame targets"_s);
}
#endif

Inspector::CommandResult<void> FrameDOMAgent::hideHighlight()
{
    return makeUnexpected("Not supported for frame targets"_s);
}

Inspector::CommandResult<void> FrameDOMAgent::highlightFrame(const String&, RefPtr<JSON::Object>&&, RefPtr<JSON::Object>&&)
{
    return makeUnexpected("Not supported for frame targets"_s);
}

Inspector::CommandResult<void> FrameDOMAgent::showGridOverlay(int, Ref<JSON::Object>&&)
{
    return makeUnexpected("Not supported for frame targets"_s);
}

Inspector::CommandResult<void> FrameDOMAgent::hideGridOverlay(std::optional<int>&&)
{
    return makeUnexpected("Not supported for frame targets"_s);
}

Inspector::CommandResult<void> FrameDOMAgent::showFlexOverlay(int, Ref<JSON::Object>&&)
{
    return makeUnexpected("Not supported for frame targets"_s);
}

Inspector::CommandResult<void> FrameDOMAgent::hideFlexOverlay(std::optional<int>&&)
{
    return makeUnexpected("Not supported for frame targets"_s);
}

Inspector::CommandResult<Ref<Inspector::Protocol::Runtime::RemoteObject>> FrameDOMAgent::resolveNode(int, const String&)
{
    return makeUnexpected("Not yet implemented for frame targets"_s);
}

Inspector::CommandResult<void> FrameDOMAgent::focus(int)
{
    return makeUnexpected("Not supported for frame targets"_s);
}

Inspector::CommandResult<void> FrameDOMAgent::setInspectedNode(int)
{
    return makeUnexpected("Not supported for frame targets"_s);
}

Inspector::CommandResult<Ref<Inspector::Protocol::DOM::MediaStats>> FrameDOMAgent::getMediaStats(int)
{
    return makeUnexpected("Not supported for frame targets"_s);
}

} // namespace WebCore
