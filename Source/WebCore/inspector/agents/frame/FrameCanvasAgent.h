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

#pragma once

#include "InspectorCanvasAgent.h"
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>

namespace WebCore {

class LocalFrame;

// FrameCanvasAgent is the per-frame Canvas agent for Site Isolation. Each LocalFrame owns one, and it
// only reports canvases created by its own frame's documents, so that their stack traces are resolved
// against the scripts that the frame target's Debugger domain reports.
class FrameCanvasAgent final : public InspectorCanvasAgent {
    WTF_MAKE_NONCOPYABLE(FrameCanvasAgent);
    WTF_MAKE_TZONE_ALLOCATED(FrameCanvasAgent);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(FrameCanvasAgent);
public:
    explicit FrameCanvasAgent(FrameAgentContext&);
    ~FrameCanvasAgent();

    // CanvasBackendDispatcherHandler
    Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>>> requestNodes(const Inspector::Protocol::Canvas::CanvasId&) override;
    Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>>> requestCSSCanvasClientNodes(const Inspector::Protocol::Canvas::CanvasId&) override;

    // InspectorInstrumentation
    void frameNavigated(LocalFrame&);
    void didChangeGPUDeviceClientNodes(GPUDevice&) override;

private:
    bool enabled() const override;

    void internalEnable() override;
    void internalDisable() override;

    bool matchesCurrentContext(ScriptExecutionContext*) const override;

    WeakRef<LocalFrame> m_inspectedFrame;
};

} // namespace WebCore
