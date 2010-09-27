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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(3D_CANVAS)

#include "WebGLFramebuffer.h"

#include "WebGLRenderingContext.h"

namespace WebCore {
    
PassRefPtr<WebGLFramebuffer> WebGLFramebuffer::create(WebGLRenderingContext* ctx)
{
    return adoptRef(new WebGLFramebuffer(ctx));
}

WebGLFramebuffer::WebGLFramebuffer(WebGLRenderingContext* ctx)
    : WebGLObject(ctx)
{
    setObject(context()->graphicsContext3D()->createFramebuffer());
}

void WebGLFramebuffer::setAttachment(unsigned long attachment, WebGLObject* attachedObject)
{
    if (!object())
        return;
    if (attachedObject && !attachedObject->object())
        attachedObject = 0;
    switch (attachment) {
    case GraphicsContext3D::COLOR_ATTACHMENT0:
        m_colorAttachment = attachedObject;
        break;
    case GraphicsContext3D::DEPTH_ATTACHMENT:
        m_depthAttachment = attachedObject;
        break;
    case GraphicsContext3D::STENCIL_ATTACHMENT:
        m_stencilAttachment = attachedObject;
        break;
    case GraphicsContext3D::DEPTH_STENCIL_ATTACHMENT:
        m_depthStencilAttachment = attachedObject;
        break;
    default:
        return;
    }
    initializeRenderbuffers();
}

void WebGLFramebuffer::removeAttachment(WebGLObject* attachment)
{
    if (!object())
        return;
    if (attachment == m_colorAttachment.get())
        m_colorAttachment = 0;
    else if (attachment == m_depthAttachment.get())
        m_depthAttachment = 0;
    else if (attachment == m_stencilAttachment.get())
        m_stencilAttachment = 0;
    else if (attachment == m_depthStencilAttachment.get())
        m_depthStencilAttachment = 0;
    else
        return;
    initializeRenderbuffers();
}

void WebGLFramebuffer::onBind()
{
    initializeRenderbuffers();
}

void WebGLFramebuffer::onAttachedObjectChange(WebGLObject* object)
{
    // Currently object == 0 is not considered, but this might change if the
    // lifespan of WebGLObject changes.
    if (object
        && (object == m_colorAttachment.get() || object == m_depthAttachment.get()
            || object == m_stencilAttachment.get() || object == m_depthStencilAttachment.get()))
        initializeRenderbuffers();
}

unsigned long WebGLFramebuffer::getColorBufferFormat()
{
    if (object() && m_colorAttachment && m_colorAttachment->object()) {
        if (m_colorAttachment->isRenderbuffer()) {
            unsigned long format = (reinterpret_cast<WebGLRenderbuffer*>(m_colorAttachment.get()))->getInternalFormat();
            switch (format) {
            case GraphicsContext3D::RGBA4:
            case GraphicsContext3D::RGB5_A1:
                return GraphicsContext3D::RGBA;
            case GraphicsContext3D::RGB565:
                return GraphicsContext3D::RGB;
            }
        } else if (m_colorAttachment->isTexture())
            return (reinterpret_cast<WebGLTexture*>(m_colorAttachment.get()))->getInternalFormat(0);
    }
    return 0;
}

void WebGLFramebuffer::deleteObjectImpl(Platform3DObject object)
{
    if (!isDeleted())
        context()->graphicsContext3D()->deleteFramebuffer(object);
    m_colorAttachment = 0;
    m_depthAttachment = 0;
    m_stencilAttachment = 0;
    m_depthStencilAttachment = 0;
}

bool WebGLFramebuffer::isUninitialized(WebGLObject* attachedObject)
{
    if (attachedObject && attachedObject->object() && attachedObject->isRenderbuffer()
        && !(reinterpret_cast<WebGLRenderbuffer*>(attachedObject))->isInitialized())
        return true;
    return false;
}

void WebGLFramebuffer::setInitialized(WebGLObject* attachedObject)
{
    if (attachedObject && attachedObject->object() && attachedObject->isRenderbuffer())
        (reinterpret_cast<WebGLRenderbuffer*>(attachedObject))->setInitialized();
}

void WebGLFramebuffer::initializeRenderbuffers()
{
    if (!object())
        return;
    bool initColor = false, initDepth = false, initStencil = false;
    unsigned long mask = 0;
    if (isUninitialized(m_colorAttachment.get())) {
        initColor = true;
        mask |= GraphicsContext3D::COLOR_BUFFER_BIT;
    }
    if (isUninitialized(m_depthAttachment.get())) {
        initDepth = true;
        mask |= GraphicsContext3D::DEPTH_BUFFER_BIT;
    }
    if (isUninitialized(m_stencilAttachment.get())) {
        initStencil = true;
        mask |= GraphicsContext3D::STENCIL_BUFFER_BIT;
    }
    if (isUninitialized(m_depthStencilAttachment.get())) {
        initDepth = true;
        initStencil = true;
        mask |= (GraphicsContext3D::DEPTH_BUFFER_BIT | GraphicsContext3D::STENCIL_BUFFER_BIT);
    }
    if (!initColor && !initDepth && !initStencil)
        return;

    // We only clear un-initialized renderbuffers when they are ready to be
    // read, i.e., when the framebuffer is complete.
    GraphicsContext3D* g3d = context()->graphicsContext3D();
    if (g3d->checkFramebufferStatus(GraphicsContext3D::FRAMEBUFFER) != GraphicsContext3D::FRAMEBUFFER_COMPLETE)
        return;

    float colorClearValue[] = {0, 0, 0, 0}, depthClearValue = 0;
    int stencilClearValue = 0;
    unsigned char colorMask[] = {1, 1, 1, 1}, depthMask = 1, stencilMask = 1;
    bool isScissorEnabled = false;
    bool isDitherEnabled = false;
    if (initColor) {
        g3d->getFloatv(GraphicsContext3D::COLOR_CLEAR_VALUE, colorClearValue);
        g3d->getBooleanv(GraphicsContext3D::COLOR_WRITEMASK, colorMask);
        g3d->clearColor(0, 0, 0, 0);
        g3d->colorMask(true, true, true, true);
    }
    if (initDepth) {
        g3d->getFloatv(GraphicsContext3D::DEPTH_CLEAR_VALUE, &depthClearValue);
        g3d->getBooleanv(GraphicsContext3D::DEPTH_WRITEMASK, &depthMask);
        g3d->clearDepth(0);
        g3d->depthMask(true);
    }
    if (initStencil) {
        g3d->getIntegerv(GraphicsContext3D::STENCIL_CLEAR_VALUE, &stencilClearValue);
        g3d->getBooleanv(GraphicsContext3D::STENCIL_WRITEMASK, &stencilMask);
        g3d->clearStencil(0);
        g3d->stencilMask(true);
    }
    isScissorEnabled = g3d->isEnabled(GraphicsContext3D::SCISSOR_TEST);
    g3d->disable(GraphicsContext3D::SCISSOR_TEST);
    isDitherEnabled = g3d->isEnabled(GraphicsContext3D::DITHER);
    g3d->disable(GraphicsContext3D::DITHER);

    g3d->clear(mask);

    if (initColor) {
        g3d->clearColor(colorClearValue[0], colorClearValue[1], colorClearValue[2], colorClearValue[3]);
        g3d->colorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    }
    if (initDepth) {
        g3d->clearDepth(depthClearValue);
        g3d->depthMask(depthMask);
    }
    if (initStencil) {
        g3d->clearStencil(stencilClearValue);
        g3d->stencilMask(stencilMask);
    }
    if (isScissorEnabled)
        g3d->enable(GraphicsContext3D::SCISSOR_TEST);
    else
        g3d->disable(GraphicsContext3D::SCISSOR_TEST);
    if (isDitherEnabled)
        g3d->enable(GraphicsContext3D::DITHER);
    else
        g3d->disable(GraphicsContext3D::DITHER);

    if (initColor)
        setInitialized(m_colorAttachment.get());
    if (initDepth && initStencil && m_depthStencilAttachment)
        setInitialized(m_depthStencilAttachment.get());
    else {
        if (initDepth)
            setInitialized(m_depthAttachment.get());
        if (initStencil)
            setInitialized(m_stencilAttachment.get());
    }
}

}

#endif // ENABLE(3D_CANVAS)
