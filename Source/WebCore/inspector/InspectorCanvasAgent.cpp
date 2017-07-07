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

#include "CanvasRenderingContext.h"
#include "CanvasRenderingContext2D.h"
#include "Document.h"
#include "Element.h"
#include "Frame.h"
#include "InspectorDOMAgent.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "JSCanvasRenderingContext2D.h"
#include "JSMainThreadExecState.h"
#include "MainFrame.h"
#include "ScriptState.h"
#include <inspector/IdentifiersFactory.h>
#include <inspector/InjectedScript.h>
#include <inspector/InjectedScriptManager.h>
#include <inspector/InspectorProtocolObjects.h>
#include <runtime/JSCInlines.h>

#if ENABLE(WEBGL)
#include "JSWebGLRenderingContext.h"
#include "WebGLContextAttributes.h"
#include "WebGLRenderingContext.h"
#include "WebGLRenderingContextBase.h"
#endif

#if ENABLE(WEBGL2)
#include "JSWebGL2RenderingContext.h"
#include "WebGL2RenderingContext.h"
#endif

#if ENABLE(WEBGPU)
#include "JSWebGPURenderingContext.h"
#include "WebGPURenderingContext.h"
#endif

using namespace Inspector;

namespace WebCore {

InspectorCanvasAgent::InspectorCanvasAgent(WebAgentContext& context, InspectorPageAgent* pageAgent)
    : InspectorAgentBase(ASCIILiteral("Canvas"), context)
    , m_frontendDispatcher(std::make_unique<Inspector::CanvasFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::CanvasBackendDispatcher::create(context.backendDispatcher, this))
    , m_injectedScriptManager(context.injectedScriptManager)
    , m_pageAgent(pageAgent)
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

    for (const auto& pair : m_canvasEntries) {
        auto* canvasElement = pair.key;
        auto& canvasEntry = pair.value;
        m_frontendDispatcher->canvasAdded(buildObjectForCanvas(canvasEntry, *canvasElement));
    }
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
    const CanvasEntry* canvasEntry = getCanvasEntry(canvasId);
    if (!canvasEntry) {
        errorString = ASCIILiteral("Invalid canvas identifier");
        return;
    }

    int documentNodeId = m_instrumentingAgents.inspectorDOMAgent()->boundNodeId(&canvasEntry->element->document());
    if (!documentNodeId) {
        errorString = ASCIILiteral("Document has not been requested");
        return;
    }

    *nodeId = m_instrumentingAgents.inspectorDOMAgent()->pushNodeToFrontend(errorString, documentNodeId, canvasEntry->element);
}

void InspectorCanvasAgent::requestContent(ErrorString& errorString, const String& canvasId, String* content)
{
    const CanvasEntry* canvasEntry = getCanvasEntry(canvasId);
    if (!canvasEntry) {
        errorString = ASCIILiteral("Invalid canvas identifier");
        return;
    }

    CanvasRenderingContext* context = canvasEntry->element->renderingContext();
    if (is<CanvasRenderingContext2D>(context)) {
        ExceptionOr<String> result = canvasEntry->element->toDataURL(ASCIILiteral("image/png"));
        if (result.hasException()) {
            errorString = result.releaseException().releaseMessage();
            return;
        }
        *content = result.releaseReturnValue();
    }
#if ENABLE(WEBGL)
    else if (is<WebGLRenderingContextBase>(context)) {
        WebGLRenderingContextBase* gl = downcast<WebGLRenderingContextBase>(context);

        gl->setPreventBufferClearForInspector(true);
        ExceptionOr<String> result = canvasEntry->element->toDataURL(ASCIILiteral("image/png"));
        gl->setPreventBufferClearForInspector(false);

        if (result.hasException()) {
            errorString = result.releaseException().releaseMessage();
            return;
        }
        *content = result.releaseReturnValue();
    }
#endif
    // FIXME: <https://webkit.org/b/173621> Web Inspector: Support getting the content of WebGPU contexts
    else
        errorString = ASCIILiteral("Unsupported canvas context type");
}

void InspectorCanvasAgent::requestCSSCanvasClientNodes(ErrorString& errorString, const String& canvasId, RefPtr<Inspector::Protocol::Array<int>>& result)
{
    const CanvasEntry* canvasEntry = getCanvasEntry(canvasId);
    if (!canvasEntry) {
        errorString = ASCIILiteral("Invalid canvas identifier");
        return;
    }

    result = Inspector::Protocol::Array<int>::create();
    for (Element* element : canvasEntry->element->cssCanvasClients()) {
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
    const CanvasEntry* canvasEntry = getCanvasEntry(canvasId);
    if (!canvasEntry) {
        errorString = ASCIILiteral("Invalid canvas identifier");
        return;
    }

    Frame* frame = canvasEntry->element->document().frame();
    if (!frame) {
        errorString = ASCIILiteral("Canvas belongs to a document without a frame");
        return;
    }

    auto& state = *mainWorldExecState(frame);
    auto injectedScript = m_injectedScriptManager.injectedScriptFor(&state);
    ASSERT(!injectedScript.hasNoValue());

    CanvasRenderingContext* context = canvasEntry->element->renderingContext();
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

    Vector<HTMLCanvasElement*> canvasesForFrame;
    for (const auto& canvasElement : m_canvasEntries.keys()) {
        if (canvasElement->document().frame() == &frame)
            canvasesForFrame.append(canvasElement);
    }

    for (auto* canvasElement : canvasesForFrame) {
        auto canvasEntry = m_canvasEntries.take(canvasElement);
        if (m_enabled)
            m_frontendDispatcher->canvasRemoved(canvasEntry.identifier);
    }
}

void InspectorCanvasAgent::didCreateCSSCanvas(HTMLCanvasElement& canvasElement, const String& name)
{
    ASSERT(!m_canvasToCSSCanvasId.contains(&canvasElement));
    ASSERT(!m_canvasEntries.contains(&canvasElement));

    m_canvasToCSSCanvasId.set(&canvasElement, name);
}

void InspectorCanvasAgent::didChangeCSSCanvasClientNodes(HTMLCanvasElement& canvasElement)
{
    const CanvasEntry* canvasEntry = getCanvasEntry(canvasElement);
    if (!canvasEntry)
        return;

    m_frontendDispatcher->cssCanvasClientNodesChanged(canvasEntry->identifier);
}

void InspectorCanvasAgent::didCreateCanvasRenderingContext(HTMLCanvasElement& canvasElement)
{
    if (m_canvasEntries.contains(&canvasElement)) {
        ASSERT_NOT_REACHED();
        return;
    }

    CanvasEntry newCanvasEntry("canvas:" + IdentifiersFactory::createIdentifier(), &canvasElement);
    newCanvasEntry.cssCanvasName = m_canvasToCSSCanvasId.take(&canvasElement);

    m_canvasEntries.set(&canvasElement, newCanvasEntry);
    canvasElement.addObserver(*this);

    if (!m_enabled)
        return;

    m_frontendDispatcher->canvasAdded(buildObjectForCanvas(newCanvasEntry, canvasElement));
}

void InspectorCanvasAgent::didChangeCanvasMemory(HTMLCanvasElement& canvasElement)
{
    CanvasEntry* canvasEntry = getCanvasEntry(canvasElement);
    if (!canvasEntry)
        return;

    m_frontendDispatcher->canvasMemoryChanged(canvasEntry->identifier, canvasElement.memoryCost());
}

void InspectorCanvasAgent::canvasDestroyed(HTMLCanvasElement& canvasElement)
{
    auto it = m_canvasEntries.find(&canvasElement);
    if (it == m_canvasEntries.end())
        return;

    String canvasIdentifier = it->value.identifier;
    m_canvasEntries.remove(it);

    if (!m_enabled)
        return;

    // WebCore::CanvasObserver::canvasDestroyed is called in response to the GC destroying the HTMLCanvasElement.
    // Due to the single-process model used in WebKit1, the event must be dispatched from a timer to prevent
    // the frontend from making JS allocations while the GC is still active.
    m_removedCanvasIdentifiers.append(canvasIdentifier);

    if (!m_timer.isActive())
        m_timer.startOneShot(0_s);
}

void InspectorCanvasAgent::canvasDestroyedTimerFired()
{
    if (!m_removedCanvasIdentifiers.size())
        return;

    for (const auto& identifier : m_removedCanvasIdentifiers)
        m_frontendDispatcher->canvasRemoved(identifier);

    m_removedCanvasIdentifiers.clear();
}

void InspectorCanvasAgent::clearCanvasData()
{
    for (auto* canvasElement : m_canvasEntries.keys())
        canvasElement->removeObserver(*this);

    m_canvasEntries.clear();
    m_canvasToCSSCanvasId.clear();
    m_removedCanvasIdentifiers.clear();

    if (m_timer.isActive())
        m_timer.stop();
}

InspectorCanvasAgent::CanvasEntry* InspectorCanvasAgent::getCanvasEntry(HTMLCanvasElement& canvasElement)
{
    auto findResult = m_canvasEntries.find(&canvasElement);
    if (findResult != m_canvasEntries.end())
        return &findResult->value;

    return nullptr;
}

InspectorCanvasAgent::CanvasEntry* InspectorCanvasAgent::getCanvasEntry(const String& canvasIdentifier)
{
    for (auto& canvasEntry : m_canvasEntries.values()) {
        if (canvasEntry.identifier == canvasIdentifier)
            return &canvasEntry;
    }

    return nullptr;
}

Ref<Inspector::Protocol::Canvas::Canvas> InspectorCanvasAgent::buildObjectForCanvas(const CanvasEntry& canvasEntry, HTMLCanvasElement& canvasElement)
{
    Frame* frame = canvasElement.document().frame();
    CanvasRenderingContext* context = canvasElement.renderingContext();

    Inspector::Protocol::Canvas::ContextType contextType;
    if (is<CanvasRenderingContext2D>(context))
        contextType = Inspector::Protocol::Canvas::ContextType::Canvas2D;
#if ENABLE(WEBGL)
    else if (is<WebGLRenderingContext>(context))
        contextType = Inspector::Protocol::Canvas::ContextType::WebGL;
#endif
#if ENABLE(WEBGL2)
    else if (is<WebGL2RenderingContext>(context))
        contextType = Inspector::Protocol::Canvas::ContextType::WebGL2;
#endif
#if ENABLE(WEBGPU)
    else if (is<WebGPURenderingContext>(context))
        contextType = Inspector::Protocol::Canvas::ContextType::WebGPU;
#endif
    else {
        ASSERT_NOT_REACHED();
        contextType = Inspector::Protocol::Canvas::ContextType::Canvas2D;
    }

    auto canvas = Inspector::Protocol::Canvas::Canvas::create()
        .setCanvasId(canvasEntry.identifier)
        .setFrameId(m_pageAgent->frameId(frame))
        .setContextType(contextType)
        .release();

    if (!canvasEntry.cssCanvasName.isEmpty())
        canvas->setCssCanvasName(canvasEntry.cssCanvasName);
    else {
        InspectorDOMAgent* domAgent = m_instrumentingAgents.inspectorDOMAgent();
        int nodeId = domAgent->boundNodeId(&canvasElement);
        if (!nodeId) {
            if (int documentNodeId = domAgent->boundNodeId(&canvasElement.document())) {
                ErrorString ignored;
                nodeId = domAgent->pushNodeToFrontend(ignored, documentNodeId, &canvasElement);
            }
        }

        if (nodeId)
            canvas->setNodeId(nodeId);
    }

#if ENABLE(WEBGL)
    if (is<WebGLRenderingContextBase>(context)) {
        if (std::optional<WebGLContextAttributes> attributes = downcast<WebGLRenderingContextBase>(context)->getContextAttributes()) {
            canvas->setContextAttributes(Inspector::Protocol::Canvas::ContextAttributes::create()
                .setAlpha(attributes->alpha)
                .setDepth(attributes->depth)
                .setStencil(attributes->stencil)
                .setAntialias(attributes->antialias)
                .setPremultipliedAlpha(attributes->premultipliedAlpha)
                .setPreserveDrawingBuffer(attributes->preserveDrawingBuffer)
                .setFailIfMajorPerformanceCaveat(attributes->failIfMajorPerformanceCaveat)
                .release());
        }
    }
#endif

    if (size_t memoryCost = canvasElement.memoryCost())
        canvas->setMemoryCost(memoryCost);

    return canvas;
}

} // namespace WebCore
