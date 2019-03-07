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

#include "WebGLContextGroup.h"
#include "WebGLRenderingContextBase.h"
#include "WebGLShader.h"
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

HashMap<WebGLProgram*, WebGLRenderingContextBase*>& WebGLProgram::instances(const LockHolder&)
{
    static NeverDestroyed<HashMap<WebGLProgram*, WebGLRenderingContextBase*>> instances;
    return instances;
}

Lock& WebGLProgram::instancesMutex()
{
    static LazyNeverDestroyed<Lock> mutex;
    static std::once_flag initializeMutex;
    std::call_once(initializeMutex, [] {
        mutex.construct();
    });
    return mutex.get();
}

Ref<WebGLProgram> WebGLProgram::create(WebGLRenderingContextBase& ctx)
{
    return adoptRef(*new WebGLProgram(ctx));
}

WebGLProgram::WebGLProgram(WebGLRenderingContextBase& ctx)
    : WebGLSharedObject(ctx)
{
    {
        LockHolder lock(instancesMutex());
        instances(lock).add(this, &ctx);
    }

    setObject(ctx.graphicsContext3D()->createProgram());
}

WebGLProgram::~WebGLProgram()
{
    deleteObject(0);

    {
        LockHolder lock(instancesMutex());
        ASSERT(instances(lock).contains(this));
        instances(lock).remove(this);
    }
}

void WebGLProgram::deleteObjectImpl(GraphicsContext3D* context3d, Platform3DObject obj)
{
    context3d->deleteProgram(obj);
    if (m_vertexShader) {
        m_vertexShader->onDetached(context3d);
        m_vertexShader = nullptr;
    }
    if (m_fragmentShader) {
        m_fragmentShader->onDetached(context3d);
        m_fragmentShader = nullptr;
    }
}

unsigned WebGLProgram::numActiveAttribLocations()
{
    cacheInfoIfNeeded();
    return m_activeAttribLocations.size();
}

GC3Dint WebGLProgram::getActiveAttribLocation(GC3Duint index)
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

WebGLShader* WebGLProgram::getAttachedShader(GC3Denum type)
{
    switch (type) {
    case GraphicsContext3D::VERTEX_SHADER:
        return m_vertexShader.get();
    case GraphicsContext3D::FRAGMENT_SHADER:
        return m_fragmentShader.get();
    default:
        return 0;
    }
}

bool WebGLProgram::attachShader(WebGLShader* shader)
{
    if (!shader || !shader->object())
        return false;
    switch (shader->getType()) {
    case GraphicsContext3D::VERTEX_SHADER:
        if (m_vertexShader)
            return false;
        m_vertexShader = shader;
        return true;
    case GraphicsContext3D::FRAGMENT_SHADER:
        if (m_fragmentShader)
            return false;
        m_fragmentShader = shader;
        return true;
    default:
        return false;
    }
}

bool WebGLProgram::detachShader(WebGLShader* shader)
{
    if (!shader || !shader->object())
        return false;
    switch (shader->getType()) {
    case GraphicsContext3D::VERTEX_SHADER:
        if (m_vertexShader != shader)
            return false;
        m_vertexShader = nullptr;
        return true;
    case GraphicsContext3D::FRAGMENT_SHADER:
        if (m_fragmentShader != shader)
            return false;
        m_fragmentShader = nullptr;
        return true;
    default:
        return false;
    }
}

void WebGLProgram::cacheActiveAttribLocations(GraphicsContext3D* context3d)
{
    m_activeAttribLocations.clear();

    GC3Dint numAttribs = 0;
    context3d->getProgramiv(object(), GraphicsContext3D::ACTIVE_ATTRIBUTES, &numAttribs);
    m_activeAttribLocations.resize(static_cast<size_t>(numAttribs));
    for (int i = 0; i < numAttribs; ++i) {
        ActiveInfo info;
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

    GraphicsContext3D* context = getAGraphicsContext3D();
    if (!context)
        return;
    GC3Dint linkStatus = 0;
    context->getProgramiv(object(), GraphicsContext3D::LINK_STATUS, &linkStatus);
    m_linkStatus = linkStatus;
    if (m_linkStatus)
        cacheActiveAttribLocations(context);
    m_infoValid = true;
}

}

#endif // ENABLE(WEBGL)
