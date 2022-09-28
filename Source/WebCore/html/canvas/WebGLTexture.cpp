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

#include "WebGLTexture.h"

#include "WebGLContextGroup.h"
#include "WebGLFramebuffer.h"
#include "WebGLRenderingContextBase.h"

namespace WebCore {

Ref<WebGLTexture> WebGLTexture::create(WebGLRenderingContextBase& ctx)
{
    return adoptRef(*new WebGLTexture(ctx));
}

WebGLTexture::WebGLTexture(WebGLRenderingContextBase& ctx)
    : WebGLSharedObject(ctx)
    , m_target(0)
{
    setObject(ctx.graphicsContextGL()->createTexture());
}

WebGLTexture::~WebGLTexture()
{
    if (!hasGroupOrContext())
        return;

    runDestructor();
}

void WebGLTexture::setTarget(GCGLenum target)
{
    if (!object())
        return;
    // Target is finalized the first time bindTexture() is called.
    if (m_target)
        return;
    m_target = target;
}

void WebGLTexture::deleteObjectImpl(const AbstractLocker&, GraphicsContextGL* context3d, PlatformGLObject object)
{
    context3d->deleteTexture(object);
}

GCGLint WebGLTexture::computeLevelCount(GCGLsizei width, GCGLsizei height)
{
    // return 1 + log2Floor(std::max(width, height));
    GCGLsizei n = std::max(width, height);
    if (n <= 0)
        return 0;
    GCGLint log = 0;
    GCGLsizei value = n;
    for (int ii = 4; ii >= 0; --ii) {
        int shift = (1 << ii);
        GCGLsizei x = (value >> shift);
        if (x) {
            value = x;
            log += shift;
        }
    }
    ASSERT(value == 1);
    return log + 1;
}

}

#endif // ENABLE(WEBGL)
