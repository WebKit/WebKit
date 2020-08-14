/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#include "InspectorInstrumentation.h"
#include "ScriptExecutionContext.h"
#include "WebGLContextGroup.h"
#include "WebGLRenderingContextBase.h"
#include "WebGLShader.h"
#include <JavaScriptCore/SlotVisitor.h>
#include <JavaScriptCore/SlotVisitorInlines.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

HashMap<WebGLProgram*, WebGLRenderingContextBase*>& WebGLProgram::instances(const LockHolder&)
{
    static NeverDestroyed<HashMap<WebGLProgram*, WebGLRenderingContextBase*>> instances;
    return instances;
}

Lock& WebGLProgram::instancesMutex()
{
    static Lock mutex;
    return mutex;
}

Ref<WebGLProgram> WebGLProgram::create(WebGLRenderingContextBase& ctx)
{
    return adoptRef(*new WebGLProgram(ctx));
}

WebGLProgram::WebGLProgram(WebGLRenderingContextBase& ctx)
    : WebGLSharedObject(ctx)
    , ContextDestructionObserver(ctx.scriptExecutionContext())
{
    ASSERT(scriptExecutionContext());

    {
        LockHolder lock(instancesMutex());
        instances(lock).add(this, &ctx);
    }

    setObject(ctx.graphicsContextGL()->createProgram());
}

WebGLProgram::~WebGLProgram()
{
    InspectorInstrumentation::willDestroyWebGLProgram(*this);

    {
        LockHolder lock(instancesMutex());
        ASSERT(instances(lock).contains(this));
        instances(lock).remove(this);
    }

    if (!hasGroupOrContext())
        return;

    runDestructor();
}

void WebGLProgram::contextDestroyed()
{
    InspectorInstrumentation::willDestroyWebGLProgram(*this);

    ContextDestructionObserver::contextDestroyed();
}

void WebGLProgram::deleteObjectImpl(const AbstractLocker& locker, GraphicsContextGLOpenGL* context3d, PlatformGLObject obj)
{
    context3d->deleteProgram(obj);
    if (m_vertexShader) {
        m_vertexShader->onDetached(locker, context3d);
        m_vertexShader = nullptr;
    }
    if (m_fragmentShader) {
        m_fragmentShader->onDetached(locker, context3d);
        m_fragmentShader = nullptr;
    }
}

unsigned WebGLProgram::numActiveAttribLocations()
{
    cacheInfoIfNeeded();
    return m_activeAttribLocations.size();
}

GCGLint WebGLProgram::getActiveAttribLocation(GCGLuint index)
{
    cacheInfoIfNeeded();
    if (index >= numActiveAttribLocations())
        return -1;
    return m_activeAttribLocations[index];
}

bool WebGLProgram::isUsingVertexAttrib0()
{
    cacheInfoIfNeeded();
    for (unsigned ii = 0; ii < numActiveAttribLocations(); ++ii) {
        if (!getActiveAttribLocation(ii))
            return true;
    }
    return false;
}

bool WebGLProgram::getLinkStatus()
{
    cacheInfoIfNeeded();
    return m_linkStatus;
}

void WebGLProgram::setLinkStatus(bool status)
{
    cacheInfoIfNeeded();
    m_linkStatus = status;
}

void WebGLProgram::increaseLinkCount()
{
    ++m_linkCount;
    m_infoValid = false;
}

WebGLShader* WebGLProgram::getAttachedShader(GCGLenum type)
{
    switch (type) {
    case GraphicsContextGL::VERTEX_SHADER:
        return m_vertexShader.get();
    case GraphicsContextGL::FRAGMENT_SHADER:
        return m_fragmentShader.get();
    default:
        return 0;
    }
}

bool WebGLProgram::attachShader(const AbstractLocker&, WebGLShader* shader)
{
    if (!shader || !shader->object())
        return false;
    switch (shader->getType()) {
    case GraphicsContextGL::VERTEX_SHADER:
        if (m_vertexShader)
            return false;
        m_vertexShader = shader;
        return true;
    case GraphicsContextGL::FRAGMENT_SHADER:
        if (m_fragmentShader)
            return false;
        m_fragmentShader = shader;
        return true;
    default:
        return false;
    }
}

bool WebGLProgram::detachShader(const AbstractLocker&, WebGLShader* shader)
{
    if (!shader || !shader->object())
        return false;
    switch (shader->getType()) {
    case GraphicsContextGL::VERTEX_SHADER:
        if (m_vertexShader != shader)
            return false;
        m_vertexShader = nullptr;
        return true;
    case GraphicsContextGL::FRAGMENT_SHADER:
        if (m_fragmentShader != shader)
            return false;
        m_fragmentShader = nullptr;
        return true;
    default:
        return false;
    }
}

void WebGLProgram::addMembersToOpaqueRoots(const AbstractLocker&, JSC::SlotVisitor& visitor)
{
    visitor.addOpaqueRoot(m_vertexShader.get());
    visitor.addOpaqueRoot(m_fragmentShader.get());
}

void WebGLProgram::cacheActiveAttribLocations(GraphicsContextGLOpenGL* context3d)
{
    m_activeAttribLocations.clear();

    GCGLint numAttribs = 0;
    context3d->getProgramiv(object(), GraphicsContextGL::ACTIVE_ATTRIBUTES, &numAttribs);
    m_activeAttribLocations.resize(static_cast<size_t>(numAttribs));
    for (int i = 0; i < numAttribs; ++i) {
        GraphicsContextGL::ActiveInfo info;
        context3d->getActiveAttribImpl(object(), i, info);
        m_activeAttribLocations[i] = context3d->getAttribLocation(object(), info.name);
    }
}

void WebGLProgram::cacheInfoIfNeeded()
{
    if (m_infoValid)
        return;

    if (!object())
        return;

    GraphicsContextGLOpenGL* context = getAGraphicsContextGL();
    if (!context)
        return;
    GCGLint linkStatus = 0;
    context->getProgramiv(object(), GraphicsContextGL::LINK_STATUS, &linkStatus);
    m_linkStatus = linkStatus;
    if (m_linkStatus) {
        cacheActiveAttribLocations(context);
        m_requiredTransformFeedbackBufferCount = m_requiredTransformFeedbackBufferCountAfterNextLink;
    }
    m_infoValid = true;
}

}

#endif // ENABLE(WEBGL)
