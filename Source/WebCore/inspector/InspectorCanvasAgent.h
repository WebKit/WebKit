/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CallTracerTypes.h"
#include "HTMLCanvasElement.h"
#include "InspectorCanvas.h"
#include "InspectorWebAgentBase.h"
#include "Timer.h"
#include <inspector/InspectorBackendDispatchers.h>
#include <inspector/InspectorFrontendDispatchers.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if ENABLE(WEBGL)
#include "InspectorShaderProgram.h"
#endif

namespace Inspector {
class InjectedScriptManager;
}

namespace WebCore {

class CanvasRenderingContext;
#if ENABLE(WEBGL)
class WebGLProgram;
class WebGLRenderingContextBase;
#endif

typedef String ErrorString;

class InspectorCanvasAgent final : public InspectorAgentBase, public CanvasObserver, public Inspector::CanvasBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorCanvasAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit InspectorCanvasAgent(WebAgentContext&);
    virtual ~InspectorCanvasAgent() { }

    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*) override;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) override;
    void discardAgent() override;

    // CanvasBackendDispatcherHandler
    void enable(ErrorString&) override;
    void disable(ErrorString&) override;
    void requestNode(ErrorString&, const String& canvasId, int* nodeId) override;
    void requestContent(ErrorString&, const String& canvasId, String* content) override;
    void requestCSSCanvasClientNodes(ErrorString&, const String& canvasId, RefPtr<Inspector::Protocol::Array<int>>&) override;
    void resolveCanvasContext(ErrorString&, const String& canvasId, const String* const objectGroup, RefPtr<Inspector::Protocol::Runtime::RemoteObject>&) override;
    void requestRecording(ErrorString&, const String& canvasId, const bool* const singleFrame, const int* const memoryLimit) override;
    void cancelRecording(ErrorString&, const String& canvasId) override;
    void requestShaderSource(ErrorString&, const String& programId, const String& shaderType, String*) override;
    void updateShader(ErrorString&, const String& programId, const String& shaderType, const String& source) override;

    // InspectorInstrumentation
    void frameNavigated(Frame&);
    void didCreateCSSCanvas(HTMLCanvasElement&, const String&);
    void didChangeCSSCanvasClientNodes(HTMLCanvasElement&);
    void didCreateCanvasRenderingContext(HTMLCanvasElement&);
    void didChangeCanvasMemory(HTMLCanvasElement&);
    void recordCanvasAction(CanvasRenderingContext&, const String&, Vector<RecordCanvasActionVariant>&& = { });
    void didFinishRecordingCanvasFrame(HTMLCanvasElement&, bool forceDispatch = false);
#if ENABLE(WEBGL)
    void didCreateProgram(WebGLRenderingContextBase&, WebGLProgram&);
    void willDeleteProgram(WebGLProgram&);
#endif

    // CanvasObserver
    void canvasChanged(HTMLCanvasElement&, const FloatRect&) override { }
    void canvasResized(HTMLCanvasElement&) override { }
    void canvasDestroyed(HTMLCanvasElement&) override;

private:
    void canvasDestroyedTimerFired();
    void canvasRecordingTimerFired();
    void clearCanvasData();
    String unbindCanvas(InspectorCanvas&);
    InspectorCanvas* assertInspectorCanvas(ErrorString&, const String& identifier);
    InspectorCanvas* findInspectorCanvas(HTMLCanvasElement&);
#if ENABLE(WEBGL)
    String unbindProgram(InspectorShaderProgram&);
    InspectorShaderProgram* assertInspectorProgram(ErrorString&, const String& identifier);
    InspectorShaderProgram* findInspectorProgram(WebGLProgram&);

    HashMap<String, RefPtr<InspectorShaderProgram>> m_identifierToInspectorProgram;
#endif

    std::unique_ptr<Inspector::CanvasFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::CanvasBackendDispatcher> m_backendDispatcher;
    Inspector::InjectedScriptManager& m_injectedScriptManager;
    HashMap<String, RefPtr<InspectorCanvas>> m_identifierToInspectorCanvas;
    HashMap<HTMLCanvasElement*, String> m_canvasToCSSCanvasName;
    Vector<String> m_removedCanvasIdentifiers;
    Timer m_canvasDestroyedTimer;
    Timer m_canvasRecordingTimer;

    bool m_enabled { false };
};

} // namespace WebCore
