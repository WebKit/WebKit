/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "OESVertexArrayObject.h"

#if ENABLE(WEBGL)

#include "ExtensionsGL.h"
#include "WebGLRenderingContext.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(OESVertexArrayObject);

OESVertexArrayObject::OESVertexArrayObject(WebGLRenderingContextBase& context)
    : WebGLExtension(context)
{
}

WebGLExtension::ExtensionName OESVertexArrayObject::getName() const
{
    return OESVertexArrayObjectName;
}

RefPtr<WebGLVertexArrayObjectOES> OESVertexArrayObject::createVertexArrayOES()
{
    if (!m_context || m_context->isContextLost())
        return nullptr;

    auto object = WebGLVertexArrayObjectOES::create(*m_context, WebGLVertexArrayObjectOES::Type::User);
    m_context->addContextObject(object.get());
    return object;
}

void OESVertexArrayObject::deleteVertexArrayOES(WebGLVertexArrayObjectOES* arrayObject)
{
    if (!m_context)
        return;

    Locker locker { m_context->objectGraphLock() };

    if (!arrayObject || m_context->isContextLost())
        return;

    if (!arrayObject->isDefaultObject() && arrayObject == static_cast<WebGLRenderingContext*>(m_context)->m_boundVertexArrayObject)
        static_cast<WebGLRenderingContext*>(m_context)->setBoundVertexArrayObject(locker, nullptr);

    arrayObject->deleteObject(locker, m_context->graphicsContextGL());
}

GCGLboolean OESVertexArrayObject::isVertexArrayOES(WebGLVertexArrayObjectOES* arrayObject)
{
    if (!m_context)
        return false;

    return arrayObject && !m_context->isContextLost() && arrayObject->hasEverBeenBound()
        && m_context->graphicsContextGL()->isVertexArray(arrayObject->object());
}

void OESVertexArrayObject::bindVertexArrayOES(WebGLVertexArrayObjectOES* arrayObject)
{
    if (!m_context)
        return;

    Locker locker { m_context->objectGraphLock() };

    if (m_context->isContextLost())
        return;

    if (arrayObject && (arrayObject->isDeleted() || !arrayObject->validate(nullptr, *context()))) {
        m_context->graphicsContextGL()->synthesizeGLError(GraphicsContextGL::INVALID_OPERATION);
        return;
    }

    auto* contextGL = m_context->graphicsContextGL();
    auto& context = downcast<WebGLRenderingContext>(*m_context);
    if (arrayObject && !arrayObject->isDefaultObject() && arrayObject->object()) {
        contextGL->bindVertexArray(arrayObject->object());
        arrayObject->setHasEverBeenBound();
        context.setBoundVertexArrayObject(locker, arrayObject);
    } else {
        contextGL->bindVertexArray(0);
        context.setBoundVertexArrayObject(locker, nullptr);
    }
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
