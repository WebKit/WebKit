/*
 * Copyright (C) 2009-2021 Apple Inc. All rights reserved.
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
#include "WebGLProgram.h"

#if ENABLE(WEBGL)

#include "ContextDestructionObserverInlines.h"
#include "InspectorInstrumentation.h"
#include "ScriptExecutionContext.h"
#include "WebCoreOpaqueRootInlines.h"
#include "WebGLRenderingContextBase.h"
#include "WebGLShader.h"
#include <JavaScriptCore/SlotVisitor.h>
#include <JavaScriptCore/SlotVisitorInlines.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

Lock WebGLProgram::s_instancesLock;

HashMap<WebGLProgram*, WebGLRenderingContextBase*>& WebGLProgram::instances()
{
    static NeverDestroyed<HashMap<WebGLProgram*, WebGLRenderingContextBase*>> instances;
    return instances;
}

Lock& WebGLProgram::instancesLock()
{
    return s_instancesLock;
}

Ref<WebGLProgram> WebGLProgram::createLost(WebGLRenderingContextBase& context)
{
    return adoptRef(*new WebGLProgram { context });
}

Ref<WebGLProgram> WebGLProgram::create(WebGLRenderingContextBase& context)
{
    auto object = protect(context.graphicsContextGL())->createProgram();
    if (!object)
        return createLost(context);
    return adoptRef(*new WebGLProgram { context, object });
}

WebGLProgram::WebGLProgram(WebGLRenderingContextBase& context, PlatformGLObject object)
    : WebGLObject(context, object)
    , ContextDestructionObserver(context.scriptExecutionContext())
{
    ASSERT(scriptExecutionContext());

    {
        Locker locker { instancesLock() };
        instances().add(this, &context);
    }
}

WebGLProgram::WebGLProgram(WebGLRenderingContextBase& context)
    : ContextDestructionObserver(context.scriptExecutionContext())
{
    ASSERT(scriptExecutionContext());
    {
        Locker locker { instancesLock() };
        instances().add(this, &context);
    }
}

WebGLProgram::~WebGLProgram()
{
    InspectorInstrumentation::willDestroyWebGLProgram(*this);

    {
        Locker locker { instancesLock() };
        ASSERT(instances().contains(this));
        instances().remove(this);
    }

    if (!m_context)
        return;

    runDestructor();
}

void WebGLProgram::contextDestroyed()
{
    InspectorInstrumentation::willDestroyWebGLProgram(*this);

    ContextDestructionObserver::contextDestroyed();
}

void WebGLProgram::deleteObjectImpl(const AbstractLocker& locker, GraphicsContextGL* context3d, PlatformGLObject obj)
{
    context3d->deleteProgram(obj);
    if (RefPtr vertexShader = m_vertexShader) {
        vertexShader->onDetached(locker, context3d);
        m_vertexShader = nullptr;
    }
    if (RefPtr fragmentShader = m_fragmentShader) {
        fragmentShader->onDetached(locker, context3d);
        m_fragmentShader = nullptr;
    }
}

bool WebGLProgram::linkStatus()
{
    if (!m_state.linkStatus) {
        RefPtr context = graphicsContextGL();
        if (!context)
            return false;
        m_state.linkStatus = context->getProgrami(object(), GraphicsContextGL::LINK_STATUS);
    }
    return *m_state.linkStatus;
}

std::span<const GCGLAttribActiveInfo> WebGLProgram::activeAttribs() LIFETIME_BOUND
{
    if (!m_state.activeAttribs) {
        RefPtr context = graphicsContextGL();
        if (!context)
            return { };
        m_state.activeAttribs = context->activeAttribs(object());
    }
    return *m_state.activeAttribs;
}

const HashMap<String, int>& WebGLProgram::attribLocations() LIFETIME_BOUND
{
    if (!m_state.attribLocations) {
        auto& locations = m_state.attribLocations.emplace();
        for (auto& activeAttrib : activeAttribs()) {
            String name = String::fromUTF8(activeAttrib.name.span());
            locations.add(name, activeAttrib.location);
        }
    }
    return *m_state.attribLocations;
}

std::span<const GCGLUniformActiveInfo> WebGLProgram::activeUniforms()
{
    if (!m_state.activeUniforms) {
        RefPtr context = graphicsContextGL();
        if (!context)
            return { };
        m_state.activeUniforms = context->activeUniforms(object());
    }
    return *m_state.activeUniforms;
}

const HashMap<String, int>& WebGLProgram::uniformLocations() LIFETIME_BOUND
{
    if (!m_state.uniformLocations) {
        auto& locations = m_state.uniformLocations.emplace();
        for (auto& activeUniform : activeUniforms()) {
            if (activeUniform.blockIndex != -1)
                continue;
            auto name = String::fromUTF8(activeUniform.name.data());
            if (activeUniform.locations[0] != -1)
                locations.add(name, activeUniform.locations[0]);
            if (name.endsWith("[0]"_s)) {
                auto baseName = name.left(name.length() - 3);
                if (activeUniform.locations[0] != -1)
                    locations.add(baseName, activeUniform.locations[0]);
                for (size_t i = 1; i < activeUniform.locations.size(); ++i) {
                    if (activeUniform.locations[i] != -1)
                        locations.add(makeString(baseName, '[', i, ']'), activeUniform.locations[i]);
                }
            }
        }
    }
    return *m_state.uniformLocations;
}

const HashMap<String, unsigned>& WebGLProgram::uniformIndices() LIFETIME_BOUND
{
    if (!m_state.uniformIndices) {
        auto& indices = m_state.uniformIndices.emplace();
        auto activeUniforms = this->activeUniforms();
        for (unsigned i = 0; i < activeUniforms.size(); ++i) {
            auto& activeUniform = activeUniforms[i];
            auto name = String::fromUTF8(activeUniform.name.data());
            indices.add(name, i);
            if (name.endsWith("[0]"_s)) {
                auto baseName = name.left(name.length() - 3);
                indices.add(baseName, i);
            }
        }
    }
    return *m_state.uniformIndices;
}

int WebGLProgram::requiredTransformFeedbackBufferCount()
{
    if (!m_state.requiredTransformFeedbackBufferCount) {
        if (!linkStatus())
            return false;
        m_state.requiredTransformFeedbackBufferCount = m_requiredTransformFeedbackBufferCountAfterNextLink;
    }
    return *m_state.requiredTransformFeedbackBufferCount;
}

void WebGLProgram::increaseLinkCount()
{
    ++m_linkCount;
    m_state = { };
}

RefPtr<WebGLShader> WebGLProgram::fragmentShader() const
{
    return m_fragmentShader;
}

RefPtr<WebGLShader> WebGLProgram::vertexShader() const
{
    return m_vertexShader;
}

bool WebGLProgram::attachShader(const AbstractLocker&, WebGLShader& shader)
{
    if (!shader.object())
        return false;
    switch (shader.getType()) {
    case GraphicsContextGL::VERTEX_SHADER:
        if (m_vertexShader)
            return false;
        m_vertexShader = &shader;
        return true;
    case GraphicsContextGL::FRAGMENT_SHADER:
        if (m_fragmentShader)
            return false;
        m_fragmentShader = &shader;
        return true;
    default:
        return false;
    }
}

bool WebGLProgram::detachShader(const AbstractLocker&, WebGLShader& shader)
{
    if (!shader.object())
        return false;
    switch (shader.getType()) {
    case GraphicsContextGL::VERTEX_SHADER:
        if (m_vertexShader != &shader)
            return false;
        m_vertexShader = nullptr;
        return true;
    case GraphicsContextGL::FRAGMENT_SHADER:
        if (m_fragmentShader != &shader)
            return false;
        m_fragmentShader = nullptr;
        return true;
    default:
        return false;
    }
}

void WebGLProgram::addMembersToOpaqueRoots(const AbstractLocker&, JSC::AbstractSlotVisitor& visitor)
{
    addWebCoreOpaqueRoot(visitor, m_vertexShader.get());
    addWebCoreOpaqueRoot(visitor, m_fragmentShader.get());
}

}

#endif // ENABLE(WEBGL)
