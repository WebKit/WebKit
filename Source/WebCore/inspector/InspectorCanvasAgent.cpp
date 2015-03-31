/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "DocumentLoader.h"
#include "Event.h"
#include "EventListener.h"
#include "EventTarget.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "GraphicsContext3D.h"
#include "GraphicsTypes3D.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "JSMainThreadExecState.h"
#include "MainFrame.h"
#include "WebGLContextAttributes.h"
#include "WebGLProgram.h"
#include "WebGLRenderingContextBase.h"
#include "WebGLShader.h"
#include <inspector/InspectorValues.h>
#include <inspector/ScriptCallStack.h>
#include <inspector/ScriptCallStackFactory.h>
#include <wtf/Vector.h>

using namespace Inspector;

namespace WebCore {

InspectorCanvasAgent::InspectorCanvasAgent(InstrumentingAgents* instrumentingAgents, InspectorPageAgent* pageAgent)
    : InspectorAgentBase(ASCIILiteral("Canvas"), instrumentingAgents)
    , m_pageAgent(pageAgent)
    , m_timer(*this, &InspectorCanvasAgent::canvasDestroyedTimerFired)
{
    reset();
}

void InspectorCanvasAgent::didCreateFrontendAndBackend(Inspector::FrontendChannel* frontendChannel, Inspector::BackendDispatcher* backendDispatcher)
{
    m_frontendDispatcher = std::make_unique<Inspector::CanvasFrontendDispatcher>(frontendChannel);
    m_backendDispatcher = Inspector::CanvasBackendDispatcher::create(backendDispatcher, this);
}

void InspectorCanvasAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    m_frontendDispatcher = nullptr;
    m_backendDispatcher.clear();
}

void InspectorCanvasAgent::frameNavigated(DocumentLoader* loader)
{
    if (loader->frame()->isMainFrame()) {
        reset();
        return;
    }

    Vector<HTMLCanvasElement*> canvasesForFrame;
    for (const auto& canvasElement : m_canvasEntries.keys()) {
        if (canvasElement->document().frame() == loader->frame())
            canvasesForFrame.append(canvasElement);
    }

    for (auto* canvasElement : canvasesForFrame) {
        const auto& canvasEntry = m_canvasEntries.get(canvasElement);
        for (const auto& programEntry : canvasEntry.programEntries.values()) {
            removeShaderFromShaderMap(programEntry, programEntry.vertexShader);
            removeShaderFromShaderMap(programEntry, programEntry.fragmentShader);
        }

        if (m_frontendDispatcher)
            m_frontendDispatcher->canvasRemoved(canvasEntry.canvasId);
    }
}

void InspectorCanvasAgent::didCreateCSSCanvas(HTMLCanvasElement& canvasElement, const String& name)
{
    ASSERT(!m_canvasToCSSCanvasId.contains(&canvasElement));
    ASSERT(!m_canvasEntries.contains(&canvasElement));
    if (m_canvasEntries.contains(&canvasElement))
        return;

    m_canvasToCSSCanvasId.set(&canvasElement, name);
}

void InspectorCanvasAgent::didCreateCanvasRenderingContext(HTMLCanvasElement& canvasElement)
{
    ASSERT(!m_canvasEntries.contains(&canvasElement));
    if (m_canvasEntries.contains(&canvasElement))
        return;

    CanvasEntry newCanvasEntry(m_nextCanvasId++, &canvasElement);
    if (m_canvasToCSSCanvasId.contains(&canvasElement)) {
        ASSERT(!canvasElement.parentElement());
        newCanvasEntry.name = m_canvasToCSSCanvasId.get(&canvasElement);
        newCanvasEntry.cssCanvas = true;
        m_canvasToCSSCanvasId.remove(&canvasElement);
    } else
        newCanvasEntry.name = canvasElement.getAttribute("id");

    m_canvasEntries.set(&canvasElement, newCanvasEntry);
    canvasElement.addObserver(*this);
    if (m_frontendDispatcher)
        m_frontendDispatcher->canvasAdded(buildObjectForCanvas(newCanvasEntry, canvasElement));
}

void InspectorCanvasAgent::didAttachShader(WebGLRenderingContextBase& context, WebGLProgram& program, WebGLShader& shader)
{
    ProgramEntry* programEntry = getProgramEntry(context, program);
    ASSERT(programEntry);
    if (!programEntry)
        return;

    ASSERT(!(programEntry->vertexShader == &shader || programEntry->fragmentShader == &shader));
    if (shader.getType() == GraphicsContext3D::VERTEX_SHADER)
        programEntry->vertexShader = &shader;
    else
        programEntry->fragmentShader = &shader;

    auto addResult = m_shaderToProgramEntries.add(&shader, HashSet<const ProgramEntry*>({programEntry}));
    if (!addResult.isNewEntry)
        addResult.iterator->value.add(programEntry);
    ASSERT(m_shaderToProgramEntries.contains(&shader));
}

void InspectorCanvasAgent::didDetachShader(WebGLRenderingContextBase& context, WebGLProgram& program, WebGLShader& shader)
{
    ProgramEntry* programEntry = getProgramEntry(context, program);
    ASSERT(programEntry);
    if (!programEntry)
        return;

    ASSERT(programEntry->vertexShader == &shader || programEntry->fragmentShader == &shader);
    if (shader.getType() == GraphicsContext3D::VERTEX_SHADER) {
        removeShaderFromShaderMap(*programEntry, programEntry->vertexShader);
        programEntry->vertexShader = nullptr;
    } else {
        removeShaderFromShaderMap(*programEntry, programEntry->fragmentShader);
        programEntry->fragmentShader = nullptr;
    }
}

void InspectorCanvasAgent::didCreateProgram(WebGLRenderingContextBase& context, WebGLProgram& program)
{
    CanvasEntry* canvasEntry = getCanvasEntry(context.canvas());
    ASSERT(canvasEntry);
    if (!canvasEntry)
        return;

    auto findResult = canvasEntry->programEntries.find(&program);
    ASSERT(findResult == canvasEntry->programEntries.end());
    if (findResult != canvasEntry->programEntries.end())
        return;

    unsigned programId = canvasEntry->nextProgramId++;
    canvasEntry->programEntries.set(&program, ProgramEntry(programId, canvasEntry, &program));
    if (m_frontendDispatcher) {
        auto compoundId = CanvasObjectId(canvasEntry->canvasId, programId);
        m_frontendDispatcher->programCreated(compoundId.asProtocolValue<Inspector::Protocol::Canvas::ProgramId>());
    }
}

void InspectorCanvasAgent::didDeleteProgram(WebGLRenderingContextBase& context, WebGLProgram& program)
{
    ProgramEntry* programEntry = getProgramEntry(context, program);
    if (!programEntry)
        return;

    if (programEntry->vertexShader)
        removeShaderFromShaderMap(*programEntry, programEntry->vertexShader);
    if (programEntry->fragmentShader)
        removeShaderFromShaderMap(*programEntry, programEntry->fragmentShader);
    programEntry->canvasEntry->programEntries.remove(&program);
    if (m_frontendDispatcher) {
        auto compoundId = CanvasObjectId(programEntry->canvasEntry->canvasId, programEntry->programId);
        m_frontendDispatcher->programDeleted(compoundId.asProtocolValue<Inspector::Protocol::Canvas::ProgramId>());
    }
}

void InspectorCanvasAgent::getCanvases(ErrorString&, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::Canvas::Canvas>>& result)
{
    result = Inspector::Protocol::Array<Inspector::Protocol::Canvas::Canvas>::create();
    for (const auto& pair : m_canvasEntries) {
        HTMLCanvasElement& canvasElement = *pair.key;
        result->addItem(buildObjectForCanvas(pair.value, canvasElement));
    }
}

void InspectorCanvasAgent::canvasDestroyed(HTMLCanvasElement& canvasElement)
{
    ASSERT(!m_canvasToCSSCanvasId.contains(&canvasElement));

    const CanvasEntry* canvasEntry = getCanvasEntry(&canvasElement);
    ASSERT(canvasEntry);
    if (!canvasEntry)
        return;

    m_canvasEntries.remove(&canvasElement);

    // WebCore::CanvasObserver::canvasDestroyed is called in response to the GC destroying the HTMLCanvasElement.
    // Due to the single-process model used in WebKit1, the event must be dispatched from a timer to prevent
    // the frontend from making JS allocations while the GC is still active.
    m_lastRemovedCanvasId = canvasEntry->canvasId;
    m_timer.startOneShot(0);
}

void InspectorCanvasAgent::canvasDestroyedTimerFired()
{
    if (m_frontendDispatcher)
        m_frontendDispatcher->canvasRemoved(m_lastRemovedCanvasId);

    m_lastRemovedCanvasId = 0;
}

void InspectorCanvasAgent::reset()
{
    m_nextCanvasId = 1;
    for (auto* canvasElement : m_canvasEntries.keys())
        canvasElement->removeObserver(*this);

    m_canvasToCSSCanvasId.clear();
    m_canvasEntries.clear();
    m_shaderToProgramEntries.clear();
}

InspectorCanvasAgent::CanvasEntry* InspectorCanvasAgent::getCanvasEntry(HTMLCanvasElement* canvasElement)
{
    ASSERT(canvasElement);
    if (!canvasElement)
        return nullptr;

    auto findResult = m_canvasEntries.find(canvasElement);
    if (findResult != m_canvasEntries.end())
        return &findResult->value;
    return nullptr;
}

InspectorCanvasAgent::CanvasEntry* InspectorCanvasAgent::getCanvasEntry(unsigned canvasId)
{
    for (auto& canvasEntry : m_canvasEntries.values()) {
        if (canvasEntry.canvasId == canvasId)
            return &canvasEntry;
    }
    return nullptr;
}

InspectorCanvasAgent::ProgramEntry* InspectorCanvasAgent::getProgramEntry(WebGLRenderingContextBase& context, WebGLProgram& program)
{
    CanvasEntry* canvasEntry = getCanvasEntry(context.canvas());
    if (!canvasEntry)
        return nullptr;

    auto findResult = canvasEntry->programEntries.find(&program);
    if (findResult == canvasEntry->programEntries.end())
        return nullptr;

    return &findResult->value;
}

void InspectorCanvasAgent::removeShaderFromShaderMap(const ProgramEntry& programEntry, WebGLShader* shader)
{
    ASSERT(shader);
    if (!shader)
        return;

    auto findResult = m_shaderToProgramEntries.find(shader);
    ASSERT(findResult != m_shaderToProgramEntries.end());
    if (findResult == m_shaderToProgramEntries.end())
        return;

    auto& programEntries = findResult->value;
    programEntries.remove(&programEntry);
    if (programEntries.isEmpty())
        m_shaderToProgramEntries.remove(shader);
}

Inspector::Protocol::Canvas::ContextType InspectorCanvasAgent::contextTypeJson(const CanvasRenderingContext* context)
{
    if (context->is2d())
        return Inspector::Protocol::Canvas::ContextType::Canvas2D;

    ASSERT(context->is3d());
    return Inspector::Protocol::Canvas::ContextType::WebGL;
}

Ref<Inspector::Protocol::Canvas::Canvas> InspectorCanvasAgent::buildObjectForCanvas(const CanvasEntry& canvasEntry, HTMLCanvasElement& canvasElement)
{
    Frame* frame = canvasElement.document().frame();
    CanvasRenderingContext* context = canvasElement.renderingContext();

    auto programIds = Inspector::Protocol::Array<Inspector::Protocol::Canvas::ProgramId>::create();
    for (auto& pair : canvasEntry.programEntries) {
        auto compoundId = CanvasObjectId(canvasEntry.canvasId, pair.value.programId);
        programIds->addItem(compoundId.asProtocolValue<Inspector::Protocol::Canvas::ProgramId>());
    }

    auto canvas = Inspector::Protocol::Canvas::Canvas::create()
        .setCanvasId(canvasEntry.canvasId)
        .setFrameId(m_pageAgent->frameId(frame))
        .setName(canvasEntry.name)
        .setCssCanvas(canvasEntry.cssCanvas)
        .setContextType(contextTypeJson(context))
        .setProgramIds(WTF::move(programIds))
        .release();

    if (context->is3d()) {
        auto attributes = static_cast<WebGLRenderingContextBase*>(context)->getContextAttributes();
        canvas->setContextAttributes(Inspector::Protocol::Canvas::ContextAttributes::create()
            .setAlpha(attributes->alpha())
            .setDepth(attributes->depth())
            .setStencil(attributes->stencil())
            .setAntialias(attributes->antialias())
            .setPremultipliedAlpha(attributes->premultipliedAlpha())
            .setPreserveDrawingBuffer(attributes->preserveDrawingBuffer())
            .release());
    }

    return canvas;
}

} // namespace WebCore
