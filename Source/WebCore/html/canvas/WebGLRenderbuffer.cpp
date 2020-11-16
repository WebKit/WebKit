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

#if ENABLE(WEBGL)

#include "WebGLRenderbuffer.h"

#include "WebGLContextGroup.h"
#include "WebGLRenderingContextBase.h"
#include <wtf/Lock.h>
#include <wtf/Locker.h>

namespace WebCore {

Ref<WebGLRenderbuffer> WebGLRenderbuffer::create(WebGLRenderingContextBase& ctx)
{
    return adoptRef(*new WebGLRenderbuffer(ctx));
}

WebGLRenderbuffer::~WebGLRenderbuffer()
{
    if (!hasGroupOrContext())
        return;

    runDestructor();
}

WebGLRenderbuffer::WebGLRenderbuffer(WebGLRenderingContextBase& ctx)
    : WebGLSharedObject(ctx)
    , m_internalFormat(GraphicsContextGL::RGBA4)
    , m_initialized(false)
    , m_width(0)
    , m_height(0)
    , m_isValid(true)
    , m_hasEverBeenBound(false)
{
    setObject(ctx.graphicsContextGL()->createRenderbuffer());
}

void WebGLRenderbuffer::deleteObjectImpl(const AbstractLocker&, GraphicsContextGL* context3d, PlatformGLObject object)
{
    context3d->deleteRenderbuffer(object);
}

}

#endif // ENABLE(WEBGL)
