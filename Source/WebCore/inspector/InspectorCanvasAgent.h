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

#ifndef InspectorCanvasAgent_h
#define InspectorCanvasAgent_h

#include "HTMLCanvasElement.h"
#include "InspectorWebAgentBase.h"
#include "Timer.h"
#include <inspector/InspectorBackendDispatchers.h>
#include <inspector/InspectorFrontendDispatchers.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class DocumentLoader;
class InspectorPageAgent;
class WebGLProgram;
class WebGLShader;
class WebGLRenderingContextBase;

typedef String ErrorString;

class InspectorCanvasAgent final
    : public InspectorAgentBase
    , public CanvasObserver
    , public Inspector::CanvasBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorCanvasAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorCanvasAgent(InstrumentingAgents*, InspectorPageAgent*);
    virtual ~InspectorCanvasAgent() { }

    virtual void didCreateFrontendAndBackend(Inspector::FrontendChannel*, Inspector::BackendDispatcher*) override;
    virtual void willDestroyFrontendAndBackend(Inspector::DisconnectReason) override;

    // InspectorInstrumentation API
    void frameNavigated(DocumentLoader*);
    void didCreateCSSCanvas(HTMLCanvasElement&, const String&);
    void didCreateCanvasRenderingContext(HTMLCanvasElement&);
    void didAttachShader(WebGLRenderingContextBase&, WebGLProgram&, WebGLShader&);
    void didDetachShader(WebGLRenderingContextBase&, WebGLProgram&, WebGLShader&);
    void didCreateProgram(WebGLRenderingContextBase&, WebGLProgram&);
    void didDeleteProgram(WebGLRenderingContextBase&, WebGLProgram&);

    // Canvas API for InspectorFrontend
    virtual void getCanvases(ErrorString&, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::Canvas::Canvas>>&) override;

    // CanvasObserver implementation
    virtual void canvasChanged(HTMLCanvasElement&, const FloatRect&) override { }
    virtual void canvasResized(HTMLCanvasElement&) override { }
    virtual void canvasDestroyed(HTMLCanvasElement&) override;

private:
    class CanvasObjectId {
    public:
        CanvasObjectId() { }

        explicit CanvasObjectId(const RefPtr<Inspector::InspectorObject> value)
        {
            if (!value->getInteger(ASCIILiteral("canvasId"), m_canvasId))
                return;
            if (!value->getInteger(ASCIILiteral("objectId"), m_objectId))
                m_canvasId = 0;
        }

        CanvasObjectId(unsigned canvasId, unsigned programId)
            : m_canvasId(canvasId)
            , m_objectId(programId)
        {
        }

        bool isEmpty() const { return !(m_canvasId && m_objectId); }
        unsigned canvasId() const { return m_canvasId; }
        unsigned objectId() const { return m_objectId; }

        // ID type is either Inspector::Protocol::Canvas::ProgramId or Inspector::Protocol::Canvas::TextureId.
        template<typename ID>
        RefPtr<ID> asProtocolValue() const
        {
            if (isEmpty())
                return nullptr;

            return ID::create()
                .setCanvasId(m_canvasId)
                .setObjectId(m_objectId).release();
        }

    private:
        unsigned m_canvasId = {0};
        unsigned m_objectId = {0};
    };

    struct CanvasEntry;

    struct ProgramEntry {
        unsigned programId = {0};
        CanvasEntry* canvasEntry = {nullptr};
        WebGLProgram* program = {nullptr};
        WebGLShader* vertexShader = {nullptr};
        WebGLShader* fragmentShader = {nullptr};

        ProgramEntry() { }

        ProgramEntry(unsigned id, CanvasEntry* canvasEntry, WebGLProgram* program)
            : programId(id)
            , canvasEntry(canvasEntry)
            , program(program)
        {
        }
    };

    struct CanvasEntry {
        unsigned canvasId = {0};
        unsigned nextProgramId = {1};
        bool cssCanvas = {false};
        String name;
        HTMLCanvasElement* element = {nullptr};
        HashMap<WebGLProgram*, ProgramEntry> programEntries;

        CanvasEntry() { }

        CanvasEntry(unsigned id, HTMLCanvasElement* canvasElement)
            : canvasId(id)
            , element(canvasElement)
        {
        }
    };

    void canvasDestroyedTimerFired();
    void reset();
    CanvasEntry* getCanvasEntry(HTMLCanvasElement*);
    CanvasEntry* getCanvasEntry(unsigned);
    ProgramEntry* getProgramEntry(WebGLRenderingContextBase&, WebGLProgram&);
    void removeShaderFromShaderMap(const ProgramEntry&, WebGLShader*);
    static Inspector::Protocol::Canvas::ContextType contextTypeJson(const CanvasRenderingContext*);
    Ref<Inspector::Protocol::Canvas::Canvas> buildObjectForCanvas(const CanvasEntry&, HTMLCanvasElement&);

    InspectorPageAgent* m_pageAgent;
    std::unique_ptr<Inspector::CanvasFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::CanvasBackendDispatcher> m_backendDispatcher;

    HashMap<HTMLCanvasElement*, String> m_canvasToCSSCanvasId;
    HashMap<HTMLCanvasElement*, CanvasEntry> m_canvasEntries;
    HashMap<const WebGLShader*, HashSet<const ProgramEntry*>> m_shaderToProgramEntries;
    Timer m_timer;
    unsigned m_nextCanvasId = {0};
    unsigned m_lastRemovedCanvasId = {0};
};

} // namespace WebCore

#endif // !defined(InspectorCanvasAgent_h)
