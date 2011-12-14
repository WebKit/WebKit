/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#import "config.h"
#import "CoreAnimationRenderer.h"

#import <WebKitSystemInterface.h>
#import <QuartzCore/QuartzCore.h>
#import <OpenGL/CGLMacro.h>

// The CGLMacro.h header adds an implicit CGLContextObj parameter to all OpenGL calls,
// which is good because it allows us to make OpenGL calls without saving and restoring the
// current context. The context argument is named "cgl_ctx" by default, so we use the macro
// below to declare this variable.
#define DECLARE_GL_CONTEXT_VARIABLE(name) \
    CGLContextObj cgl_ctx = (name)

namespace WebKit {

PassRefPtr<CoreAnimationRenderer> CoreAnimationRenderer::create(Client* client, CGLContextObj cglContextObj, CALayer *layer)
{
    return adoptRef(new CoreAnimationRenderer(client, cglContextObj, layer));
}

CoreAnimationRenderer::CoreAnimationRenderer(Client* client, CGLContextObj cglContextObj, CALayer *layer)
    : m_client(client)
    , m_cglContext(cglContextObj)
    , m_renderer([CARenderer rendererWithCGLContext:m_cglContext options:nil])
{
    [m_renderer.get() setLayer:layer];

    WKCARendererAddChangeNotificationObserver(m_renderer.get(), rendererDidChange, this);
}

CoreAnimationRenderer::~CoreAnimationRenderer()
{
    ASSERT(!m_client);
}

void CoreAnimationRenderer::setBounds(CGRect bounds)
{
    [m_renderer.get() setBounds:bounds];

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [[m_renderer.get() layer] setFrame:bounds];
    [CATransaction commit];
}

void CoreAnimationRenderer::render(CFTimeInterval frameTime, CVTimeStamp* timeStamp, CFTimeInterval& nextFrameTime)
{
    [m_renderer.get() beginFrameAtTime:frameTime timeStamp:timeStamp];
    [m_renderer.get() render];
    nextFrameTime = [m_renderer.get() nextFrameTime];
    [m_renderer.get() endFrame];
}

void CoreAnimationRenderer::invalidate()
{
    ASSERT(m_client);

    WKCARendererRemoveChangeNotificationObserver(m_renderer.get(), rendererDidChange, this);
    m_client = 0;
}

void CoreAnimationRenderer::rendererDidChange(void* context)
{
    static_cast<CoreAnimationRenderer*>(context)->rendererDidChange();
}

void CoreAnimationRenderer::rendererDidChange()
{
    ASSERT(m_client);

    m_client->rendererDidChange(this);
}

} // namespace WebKit
