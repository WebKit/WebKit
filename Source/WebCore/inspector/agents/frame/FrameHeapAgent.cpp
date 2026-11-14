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
#include "FrameHeapAgent.h"

#include "InspectorWebAgentBase.h"
#include "JSDOMWindowBase.h"
#include "JSWindowProxy.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include <JavaScriptCore/InspectorEnvironment.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameHeapAgent);

FrameHeapAgent::FrameHeapAgent(FrameAgentContext& context)
    : WebHeapAgent(context)
{
}

FrameHeapAgent::~FrameHeapAgent() = default;

bool FrameHeapAgent::includesGlobalObject(JSC::JSGlobalObject& globalObject) const
{
    // A window that wraps a RemoteDOMWindow stands for a frame in another process.
    auto* window = dynamicDowncast<JSDOMWindowBase>(globalObject);
    if (!window || !is<LocalDOMWindow>(window->wrapped()))
        return false;

    if (protect(environment())->coversGlobalObject(globalObject))
        return true;

    // A window whose proxy has no local frame left (for example a removed iframe whose window is still referenced, or a
    // frame that navigated to another process) belongs to no frame target, so every frame of its process reports it,
    // rather than hiding a leak.
    // FIXME: <https://webkit.org/b/331575> Report such a window only in the frames whose objects retain it.
    return !is<LocalFrame>(protect(window->proxy().wrapped())->frame());
}

} // namespace WebCore
