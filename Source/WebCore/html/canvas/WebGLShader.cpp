/*
 * Copyright (C) 2009, 2013 Apple Inc. All rights reserved.
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

#include "WebGLShader.h"

#include "WebGLContextGroup.h"
#include "WebGLRenderingContextBase.h"
#include <wtf/Lock.h>
#include <wtf/Locker.h>

namespace WebCore {

Ref<WebGLShader> WebGLShader::create(WebGLRenderingContextBase& ctx, GCGLenum type)
{
    return adoptRef(*new WebGLShader(ctx, type));
}

WebGLShader::WebGLShader(WebGLRenderingContextBase& ctx, GCGLenum type)
    : WebGLSharedObject(ctx)
    , m_type(type)
    , m_source(emptyString())
    , m_isValid(false)
{
    setObject(ctx.graphicsContextGL()->createShader(type));
}

WebGLShader::~WebGLShader()
{
    if (!hasGroupOrContext())
        return;

    runDestructor();
}

void WebGLShader::deleteObjectImpl(const AbstractLocker&, GraphicsContextGL* context3d, PlatformGLObject object)
{
    context3d->deleteShader(object);
}

}

#endif // ENABLE(WEBGL)
