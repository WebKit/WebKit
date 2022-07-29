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

#if ENABLE(WEBGL)
#include "OESVertexArrayObject.h"

#include "WebGLRenderingContext.h"

#include <wtf/IsoMallocInlines.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(OESVertexArrayObject);

OESVertexArrayObject::OESVertexArrayObject(WebGLRenderingContextBase& context)
    : WebGLExtension(context)
{
    context.graphicsContextGL()->ensureExtensionEnabled("GL_OES_vertex_array_object"_s);
}

OESVertexArrayObject::~OESVertexArrayObject() = default;

WebGLExtension::ExtensionName OESVertexArrayObject::getName() const
{
    return OESVertexArrayObjectName;
}

bool OESVertexArrayObject::supported(GraphicsContextGL& context)
{
    return context.supportsExtension("GL_OES_vertex_array_object"_s);
}

RefPtr<WebGLVertexArrayObjectOES> OESVertexArrayObject::createVertexArrayOES()
{
    auto context = WebGLExtensionScopedContext(this);
    if (context.isLost())
        return nullptr;

    auto object = WebGLVertexArrayObjectOES::create(*context, WebGLVertexArrayObjectOES::Type::User);
    context->addContextObject(object.get());
    return object;
}

void OESVertexArrayObject::deleteVertexArrayOES(WebGLVertexArrayObjectOES* arrayObject)
{
    auto context = WebGLExtensionScopedContext(this);
    if (context.isLost())
        return;

    Locker locker { context->objectGraphLock() };

    if (!arrayObject)
        return;

    if (!arrayObject->validate(context->contextGroup(), *context)) {
        context->synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "delete", "object does not belong to this context");
        return;
    }

    if (arrayObject->isDeleted())
        return;

    if (!arrayObject->isDefaultObject() && arrayObject == context.downcast<WebGLRenderingContext>()->m_boundVertexArrayObject)
        context.downcast<WebGLRenderingContext>()->setBoundVertexArrayObject(locker, nullptr);

    arrayObject->deleteObject(locker, context->graphicsContextGL());
}

GCGLboolean OESVertexArrayObject::isVertexArrayOES(WebGLVertexArrayObjectOES* arrayObject)
{
    auto context = WebGLExtensionScopedContext(this);
    if (context.isLost())
        return false;

    if (!arrayObject || !arrayObject->validate(context->contextGroup(), *context))
        return false;

    if (!arrayObject->hasEverBeenBound())
        return false;
    if (arrayObject->isDeleted())
        return false;

    return context->graphicsContextGL()->isVertexArray(arrayObject->object());
}

void OESVertexArrayObject::bindVertexArrayOES(WebGLVertexArrayObjectOES* arrayObject)
{
    auto context = WebGLExtensionScopedContext(this);
    if (context.isLost())
        return;

    Locker locker { context->objectGraphLock() };

    // Checks for already deleted objects and objects from other contexts. 
    if (!context->validateNullableWebGLObject("bindVertexArrayOES", arrayObject))
        return;

    auto* contextGL = context->graphicsContextGL();
    if (arrayObject && !arrayObject->isDefaultObject() && arrayObject->object()) {
        contextGL->bindVertexArray(arrayObject->object());
        arrayObject->setHasEverBeenBound();
        context.downcast<WebGLRenderingContext>()->setBoundVertexArrayObject(locker, arrayObject);
    } else {
        contextGL->bindVertexArray(0);
        context.downcast<WebGLRenderingContext>()->setBoundVertexArrayObject(locker, nullptr);
    }
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
