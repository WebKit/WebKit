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
#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "InspectorDOMAgent.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "MainFrame.h"
#include <inspector/IdentifiersFactory.h>
#include <inspector/InspectorProtocolObjects.h>

using namespace Inspector;

namespace WebCore {

InspectorCanvasAgent::InspectorCanvasAgent(WebAgentContext& context, InspectorPageAgent* pageAgent)
    : InspectorAgentBase(ASCIILiteral("Canvas"), context)
    , m_frontendDispatcher(std::make_unique<Inspector::CanvasFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::CanvasBackendDispatcher::create(context.backendDispatcher, this))
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

    m_enabled = false;
}

void InspectorCanvasAgent::frameNavigated(DocumentLoader* loader)
{
    if (loader->frame()->isMainFrame()) {
        clearCanvasData();
        return;
    }

    Vector<HTMLCanvasElement*> canvasesForFrame;
    for (const auto& canvasElement : m_canvasEntries.keys()) {
        if (canvasElement->document().frame() == loader->frame())
            canvasesForFrame.append(canvasElement);
    }

    if (!m_enabled) {
        m_canvasEntries.clear();
        return;
    }

    for (auto* canvasElement : canvasesForFrame) {
        auto canvasEntry = m_canvasEntries.take(canvasElement);
        m_frontendDispatcher->canvasRemoved(canvasEntry.identifier);
    }
}

void InspectorCanvasAgent::didCreateCSSCanvas(HTMLCanvasElement& canvasElement, const String& name)
{
    ASSERT(!m_canvasToCSSCanvasId.contains(&canvasElement));
    ASSERT(!m_canvasEntries.contains(&canvasElement));

    m_canvasToCSSCanvasId.set(&canvasElement, name);
}

void InspectorCanvasAgent::didCreateCanvasRenderingContext(HTMLCanvasElement& canvasElement)
{
    if (m_canvasEntries.contains(&canvasElement)) {
        ASSERT_NOT_REACHED();
        return;
    }

    CanvasEntry newCanvasEntry("canvas:" + IdentifiersFactory::createIdentifier(), &canvasElement);
    if (m_canvasToCSSCanvasId.contains(&canvasElement)) {
        newCanvasEntry.cssCanvasName = m_canvasToCSSCanvasId.get(&canvasElement);
        m_canvasToCSSCanvasId.remove(&canvasElement);
    }

    m_canvasEntries.set(&canvasElement, newCanvasEntry);
    canvasElement.addObserver(*this);

    if (!m_enabled)
        return;

    m_frontendDispatcher->canvasAdded(buildObjectForCanvas(newCanvasEntry, canvasElement));
}

void InspectorCanvasAgent::canvasDestroyed(HTMLCanvasElement& canvasElement)
{
    auto it = m_canvasEntries.find(&canvasElement);
    if (it == m_canvasEntries.end())
        return;

    canvasElement.removeObserver(*this);

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
    if (context->is2d())
        contextType = Inspector::Protocol::Canvas::ContextType::Canvas2D;
    else {
        ASSERT(context->isWebGL());
        contextType = Inspector::Protocol::Canvas::ContextType::WebGL;
    }

    auto canvas = Inspector::Protocol::Canvas::Canvas::create()
        .setCanvasId(canvasEntry.identifier)
        .setFrameId(m_pageAgent->frameId(frame))
        .setContextType(contextType)
        .release();

    if (!canvasEntry.cssCanvasName.isEmpty())
        canvas->setCssCanvasName(canvasEntry.cssCanvasName);

    return canvas;
}

} // namespace WebCore
