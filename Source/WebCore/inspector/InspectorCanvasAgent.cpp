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

#include "config.h"
#include "InspectorCanvasAgent.h"

#include "CanvasRenderingContext2D.h"
#include "Document.h"
#include "Element.h"
#include "Frame.h"
#include "InspectorDOMAgent.h"
#include "InstrumentingAgents.h"
#include "JSCanvasRenderingContext2D.h"
#include "JSMainThreadExecState.h"
#include "MainFrame.h"
#include "ScriptState.h"
#include "StringAdaptors.h"
#include <inspector/IdentifiersFactory.h>
#include <inspector/InjectedScript.h>
#include <inspector/InjectedScriptManager.h>
#include <inspector/InspectorProtocolObjects.h>
#include <runtime/JSCInlines.h>

#if ENABLE(WEBGL)
#include "JSWebGLRenderingContext.h"
#endif

#if ENABLE(WEBGL2)
#include "JSWebGL2RenderingContext.h"
#endif

#if ENABLE(WEBGPU)
#include "JSWebGPURenderingContext.h"
#endif

using namespace Inspector;

namespace WebCore {

InspectorCanvasAgent::InspectorCanvasAgent(WebAgentContext& context)
    : InspectorAgentBase(ASCIILiteral("Canvas"), context)
    , m_frontendDispatcher(std::make_unique<Inspector::CanvasFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::CanvasBackendDispatcher::create(context.backendDispatcher, this))
    , m_injectedScriptManager(context.injectedScriptManager)
    , m_timer(*this, &InspectorCanvasAgent::canvasDestroyedTimerFired)
{
}

void InspectorCanvasAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
}

void InspectorCanvasAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    ErrorString ignored;
    disable(ignored);
}

void InspectorCanvasAgent::discardAgent()
{
    clearCanvasData();
}

void InspectorCanvasAgent::enable(ErrorString&)
{
    if (m_enabled)
        return;

    m_enabled = true;

    for (auto& inspectorCanvas : m_identifierToInspectorCanvas.values())
        m_frontendDispatcher->canvasAdded(inspectorCanvas->buildObjectForCanvas(m_instrumentingAgents));
}

void InspectorCanvasAgent::disable(ErrorString&)
{
    if (!m_enabled)
        return;

    if (m_timer.isActive())
        m_timer.stop();

    m_removedCanvasIdentifiers.clear();

    m_enabled = false;
}

void InspectorCanvasAgent::requestNode(ErrorString& errorString, const String& canvasId, int* nodeId)
{
    auto* inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return;

    int documentNodeId = m_instrumentingAgents.inspectorDOMAgent()->boundNodeId(&inspectorCanvas->canvas().document());
    if (!documentNodeId) {
        errorString = ASCIILiteral("Document has not been requested");
        return;
    }

    *nodeId = m_instrumentingAgents.inspectorDOMAgent()->pushNodeToFrontend(errorString, documentNodeId, &inspectorCanvas->canvas());
}

void InspectorCanvasAgent::requestContent(ErrorString& errorString, const String& canvasId, String* content)
{
    auto* inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return;

    CanvasRenderingContext* context = inspectorCanvas->canvas().renderingContext();
    if (is<CanvasRenderingContext2D>(context)) {
        auto result = inspectorCanvas->canvas().toDataURL(ASCIILiteral("image/png"));
        if (result.hasException()) {
            errorString = result.releaseException().releaseMessage();
            return;
        }
        *content = result.releaseReturnValue().string;
    }
#if ENABLE(WEBGL)
    else if (is<WebGLRenderingContextBase>(context)) {
        WebGLRenderingContextBase* gl = downcast<WebGLRenderingContextBase>(context);

        gl->setPreventBufferClearForInspector(true);
        auto result = inspectorCanvas->canvas().toDataURL(ASCIILiteral("image/png"));
        gl->setPreventBufferClearForInspector(false);

        if (result.hasException()) {
            errorString = result.releaseException().releaseMessage();
            return;
        }
        *content = result.releaseReturnValue().string;
    }
#endif
    // FIXME: <https://webkit.org/b/173621> Web Inspector: Support getting the content of WebGPU contexts
    else
        errorString = ASCIILiteral("Unsupported canvas context type");
}

void InspectorCanvasAgent::requestCSSCanvasClientNodes(ErrorString& errorString, const String& canvasId, RefPtr<Inspector::Protocol::Array<int>>& result)
{
    auto* inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return;

    result = Inspector::Protocol::Array<int>::create();
    for (Element* element : inspectorCanvas->canvas().cssCanvasClients()) {
        if (int documentNodeId = m_instrumentingAgents.inspectorDOMAgent()->boundNodeId(&element->document()))
            result->addItem(m_instrumentingAgents.inspectorDOMAgent()->pushNodeToFrontend(errorString, documentNodeId, element));
    }
}

static JSC::JSValue contextAsScriptValue(JSC::ExecState& state, CanvasRenderingContext* context)
{
    JSC::JSLockHolder lock(&state);

    if (is<CanvasRenderingContext2D>(context))
        return toJS(&state, deprecatedGlobalObjectForPrototype(&state), downcast<CanvasRenderingContext2D>(context));
#if ENABLE(WEBGL)
    if (is<WebGLRenderingContext>(context))
        return toJS(&state, deprecatedGlobalObjectForPrototype(&state), downcast<WebGLRenderingContext>(context));
#endif
#if ENABLE(WEBGL2)
    if (is<WebGL2RenderingContext>(context))
        return toJS(&state, deprecatedGlobalObjectForPrototype(&state), downcast<WebGL2RenderingContext>(context));
#endif
#if ENABLE(WEBGPU)
    if (is<WebGPURenderingContext>(context))
        return toJS(&state, deprecatedGlobalObjectForPrototype(&state), downcast<WebGPURenderingContext>(context));
#endif

    return { };
}

void InspectorCanvasAgent::resolveCanvasContext(ErrorString& errorString, const String& canvasId, const String* const objectGroup, RefPtr<Inspector::Protocol::Runtime::RemoteObject>& result)
{
    auto* inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return;

    Frame* frame = inspectorCanvas->canvas().document().frame();
    if (!frame) {
        errorString = ASCIILiteral("Canvas belongs to a document without a frame");
        return;
    }

    auto& state = *mainWorldExecState(frame);
    auto injectedScript = m_injectedScriptManager.injectedScriptFor(&state);
    ASSERT(!injectedScript.hasNoValue());

    CanvasRenderingContext* context = inspectorCanvas->canvas().renderingContext();
    JSC::JSValue value = contextAsScriptValue(state, context);
    if (!value) {
        ASSERT_NOT_REACHED();
        errorString = ASCIILiteral("Unknown context type");
        return;
    }

    String objectGroupName = objectGroup ? *objectGroup : String();
    result = injectedScript.wrapObject(value, objectGroupName);
}

void InspectorCanvasAgent::frameNavigated(Frame& frame)
{
    if (frame.isMainFrame()) {
        clearCanvasData();
        return;
    }

    Vector<InspectorCanvas*> inspectorCanvases;
    for (RefPtr<InspectorCanvas>& inspectorCanvas : m_identifierToInspectorCanvas.values()) {
        if (inspectorCanvas->canvas().document().frame() == &frame)
            inspectorCanvases.append(inspectorCanvas.get());
    }

    for (auto* inspectorCanvas : inspectorCanvases) {
        String identifier = unbindCanvas(*inspectorCanvas);
        if (m_enabled)
            m_frontendDispatcher->canvasRemoved(identifier);
    }
}

void InspectorCanvasAgent::didCreateCSSCanvas(HTMLCanvasElement& canvasElement, const String& name)
{
    if (findInspectorCanvas(canvasElement)) {
        ASSERT_NOT_REACHED();
        return;
    }

    ASSERT(!m_canvasToCSSCanvasName.contains(&canvasElement));
    m_canvasToCSSCanvasName.set(&canvasElement, name);
}

void InspectorCanvasAgent::didChangeCSSCanvasClientNodes(HTMLCanvasElement& canvasElement)
{
    auto* inspectorCanvas = findInspectorCanvas(canvasElement);
    if (!inspectorCanvas)
        return;

    m_frontendDispatcher->cssCanvasClientNodesChanged(inspectorCanvas->identifier());
}

void InspectorCanvasAgent::didCreateCanvasRenderingContext(HTMLCanvasElement& canvasElement)
{
    if (findInspectorCanvas(canvasElement)) {
        ASSERT_NOT_REACHED();
        return;
    }

    canvasElement.addObserver(*this);

    String cssCanvasName = m_canvasToCSSCanvasName.take(&canvasElement);
    auto inspectorCanvas = InspectorCanvas::create(canvasElement, cssCanvasName);

    if (m_enabled)
        m_frontendDispatcher->canvasAdded(inspectorCanvas->buildObjectForCanvas(m_instrumentingAgents));

    m_identifierToInspectorCanvas.set(inspectorCanvas->identifier(), WTFMove(inspectorCanvas));
}

void InspectorCanvasAgent::didChangeCanvasMemory(HTMLCanvasElement& canvasElement)
{
    auto* inspectorCanvas = findInspectorCanvas(canvasElement);
    if (!inspectorCanvas)
        return;

    m_frontendDispatcher->canvasMemoryChanged(inspectorCanvas->identifier(), canvasElement.memoryCost());
}

void InspectorCanvasAgent::canvasDestroyed(HTMLCanvasElement& canvasElement)
{
    auto* inspectorCanvas = findInspectorCanvas(canvasElement);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    String identifier = unbindCanvas(*inspectorCanvas);
    if (!m_enabled)
        return;

    // WebCore::CanvasObserver::canvasDestroyed is called in response to the GC destroying the HTMLCanvasElement.
    // Due to the single-process model used in WebKit1, the event must be dispatched from a timer to prevent
    // the frontend from making JS allocations while the GC is still active.
    m_removedCanvasIdentifiers.append(identifier);

    if (!m_timer.isActive())
        m_timer.startOneShot(0_s);
}

void InspectorCanvasAgent::canvasDestroyedTimerFired()
{
    if (!m_removedCanvasIdentifiers.size())
        return;

    if (m_enabled) {
        for (auto& identifier : m_removedCanvasIdentifiers)
            m_frontendDispatcher->canvasRemoved(identifier);
    }

    m_removedCanvasIdentifiers.clear();
}

void InspectorCanvasAgent::clearCanvasData()
{
    for (auto& inspectorCanvas : m_identifierToInspectorCanvas.values())
        inspectorCanvas->canvas().removeObserver(*this);

    m_identifierToInspectorCanvas.clear();
    m_canvasToCSSCanvasName.clear();
    m_removedCanvasIdentifiers.clear();

    if (m_timer.isActive())
        m_timer.stop();
}

String InspectorCanvasAgent::unbindCanvas(InspectorCanvas& inspectorCanvas)
{
    ASSERT(!m_canvasToCSSCanvasName.contains(&inspectorCanvas.canvas()));

    String identifier = inspectorCanvas.identifier();
    m_identifierToInspectorCanvas.remove(identifier);

    return identifier;
}

InspectorCanvas* InspectorCanvasAgent::assertInspectorCanvas(ErrorString& errorString, const String& identifier)
{
    RefPtr<InspectorCanvas> inspectorCanvas = m_identifierToInspectorCanvas.get(identifier);
    if (!inspectorCanvas) {
        errorString = ASCIILiteral("No canvas for given identifier.");
        return nullptr;
    }

    return inspectorCanvas.get();
}

InspectorCanvas* InspectorCanvasAgent::findInspectorCanvas(HTMLCanvasElement& canvasElement)
{
    for (auto& inspectorCanvas : m_identifierToInspectorCanvas.values()) {
        if (&inspectorCanvas->canvas() == &canvasElement)
            return inspectorCanvas.get();
    }

    return nullptr;
}

} // namespace WebCore
