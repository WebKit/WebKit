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

#include <wtf/Lock.h>
#include <wtf/Locker.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(OESVertexArrayObject);

OESVertexArrayObject::OESVertexArrayObject(WebGLRenderingContext& context)
    : WebGLExtension(context, WebGLExtensionName::OESVertexArrayObject)
{
    context.protectedGraphicsContextGL()->ensureExtensionEnabled("GL_OES_vertex_array_object"_s);
}

OESVertexArrayObject::~OESVertexArrayObject() = default;

bool OESVertexArrayObject::supported(GraphicsContextGL& context)
{
    return context.supportsExtension("GL_OES_vertex_array_object"_s);
}

RefPtr<WebGLVertexArrayObjectOES> OESVertexArrayObject::createVertexArrayOES()
{
    if (isContextLost())
        return nullptr;
    auto& context = this->context();
    return WebGLVertexArrayObjectOES::createUser(context);
}

void OESVertexArrayObject::deleteVertexArrayOES(WebGLVertexArrayObjectOES* arrayObject)
{
    if (isContextLost())
        return;
    auto& context = this->context();

    Locker locker { context.objectGraphLock() };

    if (!arrayObject)
        return;

    if (!arrayObject->validate(context)) {
        context.synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "delete"_s, "object does not belong to this context"_s);
        return;
    }

    if (arrayObject->isDeleted())
        return;

    if (!arrayObject->isDefaultObject() && arrayObject == context.m_boundVertexArrayObject)
        context.setBoundVertexArrayObject(locker, nullptr);

    arrayObject->deleteObject(locker, context.protectedGraphicsContextGL().get());
}

GCGLboolean OESVertexArrayObject::isVertexArrayOES(WebGLVertexArrayObjectOES* arrayObject)
{
    if (isContextLost())
        return false;
    auto& context = this->context();
    if (!context.validateIsWebGLObject(arrayObject))
        return false;
    return context.protectedGraphicsContextGL()->isVertexArray(arrayObject->object());
}

void OESVertexArrayObject::bindVertexArrayOES(WebGLVertexArrayObjectOES* arrayObject)
{
    if (isContextLost())
        return;
    auto& context = this->context();
    Locker locker { context.objectGraphLock() };

    // Checks for already deleted objects and objects from other contexts. 
    if (!context.validateNullableWebGLObject("bindVertexArrayOES"_s, arrayObject))
        return;

    RefPtr contextGL = context.graphicsContextGL();
    if (arrayObject && !arrayObject->isDefaultObject() && arrayObject->object()) {
        contextGL->bindVertexArray(arrayObject->object());
        context.setBoundVertexArrayObject(locker, arrayObject);
    } else {
        contextGL->bindVertexArray(0);
        context.setBoundVertexArrayObject(locker, nullptr);
    }
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
