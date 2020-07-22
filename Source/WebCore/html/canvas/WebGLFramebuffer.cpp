/*
 * Copyright (C) 2009-2017 Apple Inc. All rights reserved.
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
#include "WebGLFramebuffer.h"

#if ENABLE(WEBGL)

#include "ExtensionsGL.h"
#include "WebGLContextGroup.h"
#include "WebGLDrawBuffers.h"
#include "WebGLRenderingContextBase.h"

namespace WebCore {

namespace {

    class WebGLRenderbufferAttachment : public WebGLFramebuffer::WebGLAttachment {
    public:
        static Ref<WebGLFramebuffer::WebGLAttachment> create(WebGLRenderbuffer*);

    private:
        WebGLRenderbufferAttachment(WebGLRenderbuffer*);
#if !USE(ANGLE)
        GCGLsizei getWidth() const override;
        GCGLsizei getHeight() const override;
        GCGLenum getFormat() const override;
#endif
        WebGLSharedObject* getObject() const override;
        bool isSharedObject(WebGLSharedObject*) const override;
        bool isValid() const override;
        bool isInitialized() const override;
        void setInitialized() override;
        void onDetached(GraphicsContextGLOpenGL*) override;
        void attach(GraphicsContextGLOpenGL*, GCGLenum attachment) override;
        void unattach(GraphicsContextGLOpenGL*, GCGLenum attachment) override;

        WebGLRenderbufferAttachment() { };

        RefPtr<WebGLRenderbuffer> m_renderbuffer;
    };

    Ref<WebGLFramebuffer::WebGLAttachment> WebGLRenderbufferAttachment::create(WebGLRenderbuffer* renderbuffer)
    {
        return adoptRef(*new WebGLRenderbufferAttachment(renderbuffer));
    }

    WebGLRenderbufferAttachment::WebGLRenderbufferAttachment(WebGLRenderbuffer* renderbuffer)
        : m_renderbuffer(renderbuffer)
    {
    }

#if !USE(ANGLE)
    GCGLsizei WebGLRenderbufferAttachment::getWidth() const
    {
        return m_renderbuffer->getWidth();
    }

    GCGLsizei WebGLRenderbufferAttachment::getHeight() const
    {
        return m_renderbuffer->getHeight();
    }

    GCGLenum WebGLRenderbufferAttachment::getFormat() const
    {
        return m_renderbuffer->getInternalFormat();
    }
#endif

    WebGLSharedObject* WebGLRenderbufferAttachment::getObject() const
    {
        return m_renderbuffer->object() ? m_renderbuffer.get() : 0;
    }

    bool WebGLRenderbufferAttachment::isSharedObject(WebGLSharedObject* object) const
    {
        return object == m_renderbuffer;
    }

    bool WebGLRenderbufferAttachment::isValid() const
    {
        return m_renderbuffer->object();
    }

    bool WebGLRenderbufferAttachment::isInitialized() const
    {
        return m_renderbuffer->object() && m_renderbuffer->isInitialized();
    }

    void WebGLRenderbufferAttachment::setInitialized()
    {
        if (m_renderbuffer->object())
            m_renderbuffer->setInitialized();
    }

    void WebGLRenderbufferAttachment::onDetached(GraphicsContextGLOpenGL* context)
    {
        m_renderbuffer->onDetached(context);
    }

    void WebGLRenderbufferAttachment::attach(GraphicsContextGLOpenGL* context, GCGLenum attachment)
    {
        PlatformGLObject object = objectOrZero(m_renderbuffer.get());
        context->framebufferRenderbuffer(GraphicsContextGL::FRAMEBUFFER, attachment, GraphicsContextGL::RENDERBUFFER, object);
    }

    void WebGLRenderbufferAttachment::unattach(GraphicsContextGLOpenGL* context, GCGLenum attachment)
    {
#if !USE(ANGLE)
        if (attachment == GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT) {
            context->framebufferRenderbuffer(GraphicsContextGL::FRAMEBUFFER, GraphicsContextGL::DEPTH_ATTACHMENT, GraphicsContextGL::RENDERBUFFER, 0);
            context->framebufferRenderbuffer(GraphicsContextGL::FRAMEBUFFER, GraphicsContextGL::STENCIL_ATTACHMENT, GraphicsContextGL::RENDERBUFFER, 0);
        } else
#endif
            context->framebufferRenderbuffer(GraphicsContextGL::FRAMEBUFFER, attachment, GraphicsContextGL::RENDERBUFFER, 0);
    }

    class WebGLTextureAttachment : public WebGLFramebuffer::WebGLAttachment {
    public:
        static Ref<WebGLFramebuffer::WebGLAttachment> create(WebGLTexture*, GCGLenum target, GCGLint level);

    private:
        WebGLTextureAttachment(WebGLTexture*, GCGLenum target, GCGLint level);
#if !USE(ANGLE)
        GCGLsizei getWidth() const override;
        GCGLsizei getHeight() const override;
        GCGLenum getFormat() const override;
#endif
        WebGLSharedObject* getObject() const override;
        bool isSharedObject(WebGLSharedObject*) const override;
        bool isValid() const override;
        bool isInitialized() const override;
        void setInitialized() override;
        void onDetached(GraphicsContextGLOpenGL*) override;
        void attach(GraphicsContextGLOpenGL*, GCGLenum attachment) override;
        void unattach(GraphicsContextGLOpenGL*, GCGLenum attachment) override;

        WebGLTextureAttachment() { };

        RefPtr<WebGLTexture> m_texture;
        GCGLenum m_target;
        GCGLint m_level;
    };

    Ref<WebGLFramebuffer::WebGLAttachment> WebGLTextureAttachment::create(WebGLTexture* texture, GCGLenum target, GCGLint level)
    {
        return adoptRef(*new WebGLTextureAttachment(texture, target, level));
    }

    WebGLTextureAttachment::WebGLTextureAttachment(WebGLTexture* texture, GCGLenum target, GCGLint level)
        : m_texture(texture)
        , m_target(target)
        , m_level(level)
    {
    }

#if !USE(ANGLE)
    GCGLsizei WebGLTextureAttachment::getWidth() const
    {
        return m_texture->getWidth(m_target, m_level);
    }

    GCGLsizei WebGLTextureAttachment::getHeight() const
    {
        return m_texture->getHeight(m_target, m_level);
    }

    GCGLenum WebGLTextureAttachment::getFormat() const
    {
        return m_texture->getInternalFormat(m_target, m_level);
    }
#endif

    WebGLSharedObject* WebGLTextureAttachment::getObject() const
    {
        return m_texture->object() ? m_texture.get() : 0;
    }

    bool WebGLTextureAttachment::isSharedObject(WebGLSharedObject* object) const
    {
        return object == m_texture;
    }

    bool WebGLTextureAttachment::isValid() const
    {
        return m_texture->object();
    }

    bool WebGLTextureAttachment::isInitialized() const
    {
        // Textures are assumed to be initialized.
        return true;
    }

    void WebGLTextureAttachment::setInitialized()
    {
        // Textures are assumed to be initialized.
    }

    void WebGLTextureAttachment::onDetached(GraphicsContextGLOpenGL* context)
    {
        m_texture->onDetached(context);
    }

    void WebGLTextureAttachment::attach(GraphicsContextGLOpenGL* context, GCGLenum attachment)
    {
        PlatformGLObject object = objectOrZero(m_texture.get());
        context->framebufferTexture2D(GraphicsContextGL::FRAMEBUFFER, attachment, m_target, object, m_level);
    }

    void WebGLTextureAttachment::unattach(GraphicsContextGLOpenGL* context, GCGLenum attachment)
    {
#if !USE(ANGLE)
        if (attachment == GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT) {
            context->framebufferTexture2D(GraphicsContextGL::FRAMEBUFFER, GraphicsContextGL::DEPTH_ATTACHMENT, m_target, 0, m_level);
            context->framebufferTexture2D(GraphicsContextGL::FRAMEBUFFER, GraphicsContextGL::STENCIL_ATTACHMENT, m_target, 0, m_level);
        } else
#endif
            context->framebufferTexture2D(GraphicsContextGL::FRAMEBUFFER, attachment, m_target, 0, m_level);
    }

#if !USE(ANGLE)
    bool isAttachmentComplete(WebGLFramebuffer::WebGLAttachment* attachedObject, GCGLenum attachment, const char** reason)
    {
        ASSERT(attachedObject && attachedObject->isValid());
        ASSERT(reason);
        GCGLenum format = attachedObject->getFormat();
        unsigned need = GraphicsContextGLOpenGL::getClearBitsByAttachmentType(attachment);
        unsigned have = GraphicsContextGLOpenGL::getClearBitsByFormat(format);

        if ((need & have) != need) {
            *reason = "attachment type is not correct for attachment";
            return false;
        }
        if (!attachedObject->getWidth() || !attachedObject->getHeight()) {
            *reason = "attachment has a 0 dimension";
            return false;
        }
        if ((attachment == GraphicsContextGL::DEPTH_ATTACHMENT || attachment == GraphicsContextGL::STENCIL_ATTACHMENT)
            && format == GraphicsContextGL::DEPTH_STENCIL) {
          *reason = "attachment DEPTH_STENCIL not allowed on DEPTH or STENCIL attachment";
          return false;
        }
        return true;
    }
#endif

} // anonymous namespace

WebGLFramebuffer::WebGLAttachment::WebGLAttachment() = default;

WebGLFramebuffer::WebGLAttachment::~WebGLAttachment() = default;

Ref<WebGLFramebuffer> WebGLFramebuffer::create(WebGLRenderingContextBase& ctx)
{
    return adoptRef(*new WebGLFramebuffer(ctx));
}

WebGLFramebuffer::WebGLFramebuffer(WebGLRenderingContextBase& ctx)
    : WebGLContextObject(ctx)
    , m_hasEverBeenBound(false)
{
    setObject(ctx.graphicsContextGL()->createFramebuffer());
}

WebGLFramebuffer::~WebGLFramebuffer()
{
    deleteObject(0);
}

void WebGLFramebuffer::setAttachmentForBoundFramebuffer(GCGLenum target, GCGLenum attachment, GCGLenum texTarget, WebGLTexture* texture, GCGLint level)
{
    ASSERT(isBound(target));
    removeAttachmentFromBoundFramebuffer(target, attachment);
    if (!object())
        return;
    if (texture && texture->object()) {
        m_attachments.add(attachment, WebGLTextureAttachment::create(texture, texTarget, level));
        drawBuffersIfNecessary(false);
        texture->onAttached();
    }
}

void WebGLFramebuffer::setAttachmentForBoundFramebuffer(GCGLenum target, GCGLenum attachment, WebGLRenderbuffer* renderbuffer)
{
    ASSERT(isBound(target));
    removeAttachmentFromBoundFramebuffer(target, attachment);
    if (!object())
        return;
    if (renderbuffer && renderbuffer->object()) {
        m_attachments.add(attachment, WebGLRenderbufferAttachment::create(renderbuffer));
        drawBuffersIfNecessary(false);
        renderbuffer->onAttached();
    }
}

void WebGLFramebuffer::attach(GCGLenum target, GCGLenum attachment, GCGLenum attachmentPoint)
{
#if ASSERT_ENABLED
    ASSERT(isBound(target));
#else
    UNUSED_PARAM(target);
#endif
    RefPtr<WebGLAttachment> attachmentObject = getAttachment(attachment);
    if (attachmentObject)
        attachmentObject->attach(context()->graphicsContextGL(), attachmentPoint);
}

WebGLSharedObject* WebGLFramebuffer::getAttachmentObject(GCGLenum attachment) const
{
    if (!object())
        return 0;
    RefPtr<WebGLAttachment> attachmentObject = getAttachment(attachment);
    return attachmentObject ? attachmentObject->getObject() : 0;
}

WebGLFramebuffer::WebGLAttachment* WebGLFramebuffer::getAttachment(GCGLenum attachment) const
{
    const AttachmentMap::const_iterator it = m_attachments.find(attachment);
    return (it != m_attachments.end()) ? it->value.get() : 0;
}

void WebGLFramebuffer::removeAttachmentFromBoundFramebuffer(GCGLenum target, GCGLenum attachment)
{
#if ASSERT_ENABLED
    ASSERT(isBound(target));
#else
    UNUSED_PARAM(target);
#endif
    if (!object())
        return;

    RefPtr<WebGLAttachment> attachmentObject = getAttachment(attachment);
    if (attachmentObject) {
        attachmentObject->onDetached(context()->graphicsContextGL());
        m_attachments.remove(attachment);
        drawBuffersIfNecessary(false);
#if !USE(ANGLE)
        switch (attachment) {
        case GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT:
            attach(target, GraphicsContextGL::DEPTH_ATTACHMENT, GraphicsContextGL::DEPTH_ATTACHMENT);
            attach(target, GraphicsContextGL::STENCIL_ATTACHMENT, GraphicsContextGL::STENCIL_ATTACHMENT);
            break;
        case GraphicsContextGL::DEPTH_ATTACHMENT:
            attach(target, GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT, GraphicsContextGL::DEPTH_ATTACHMENT);
            break;
        case GraphicsContextGL::STENCIL_ATTACHMENT:
            attach(target, GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT, GraphicsContextGL::STENCIL_ATTACHMENT);
            break;
        }
#endif
    }
}

void WebGLFramebuffer::removeAttachmentFromBoundFramebuffer(GCGLenum target, WebGLSharedObject* attachment)
{
    ASSERT(isBound(target));
    if (!object())
        return;
    if (!attachment)
        return;

    bool checkMore = true;
    do {
        checkMore = false;
        for (auto& entry : m_attachments) {
            RefPtr<WebGLAttachment> attachmentObject = entry.value.get();
            if (attachmentObject->isSharedObject(attachment)) {
                GCGLenum attachmentType = entry.key;
                attachmentObject->unattach(context()->graphicsContextGL(), attachmentType);
                removeAttachmentFromBoundFramebuffer(target, attachmentType);
                checkMore = true;
                break;
            }
        }
    } while (checkMore);
}

#if !USE(ANGLE)
GCGLsizei WebGLFramebuffer::getColorBufferWidth() const
{
    if (!object())
        return 0;
    RefPtr<WebGLAttachment> attachment = getAttachment(GraphicsContextGL::COLOR_ATTACHMENT0);
    if (!attachment)
        return 0;

    return attachment->getWidth();
}

GCGLsizei WebGLFramebuffer::getColorBufferHeight() const
{
    if (!object())
        return 0;
    RefPtr<WebGLAttachment> attachment = getAttachment(GraphicsContextGL::COLOR_ATTACHMENT0);
    if (!attachment)
        return 0;

    return attachment->getHeight();
}

GCGLenum WebGLFramebuffer::getColorBufferFormat() const
{
    if (!object())
        return 0;
    RefPtr<WebGLAttachment> attachment = getAttachment(GraphicsContextGL::COLOR_ATTACHMENT0);
    if (!attachment)
        return 0;
    return attachment->getFormat();
}

GCGLenum WebGLFramebuffer::checkStatus(const char** reason) const
{
    unsigned int count = 0;
    GCGLsizei width = 0, height = 0;
    bool haveDepth = false;
    bool haveStencil = false;
    bool haveDepthStencil = false;
    for (auto& entry : m_attachments) {
        RefPtr<WebGLAttachment> attachment = entry.value.get();
        if (!isAttachmentComplete(attachment.get(), entry.key, reason))
            return GraphicsContextGL::FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        if (!attachment->isValid()) {
            *reason = "attachment is not valid";
            return GraphicsContextGL::FRAMEBUFFER_UNSUPPORTED;
        }
        GCGLenum attachmentFormat = attachment->getFormat();

        // Attaching an SRGB_EXT format attachment to a framebuffer is invalid.
        if (attachmentFormat == ExtensionsGL::SRGB_EXT)
            attachmentFormat = 0;

        if (!attachmentFormat) {
            *reason = "attachment is an unsupported format";
            return GraphicsContextGL::FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }
        switch (entry.key) {
        case GraphicsContextGL::DEPTH_ATTACHMENT:
            haveDepth = true;
            break;
        case GraphicsContextGL::STENCIL_ATTACHMENT:
            haveStencil = true;
            break;
        case GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT:
            haveDepthStencil = true;
            break;
        }
        if (!count) {
            width = attachment->getWidth();
            height = attachment->getHeight();
        } else {
            if (width != attachment->getWidth() || height != attachment->getHeight()) {
                *reason = "attachments do not have the same dimensions";
                return GraphicsContextGL::FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
            }
        }
        ++count;
    }
    if (!count) {
        *reason = "no attachments";
        return GraphicsContextGL::FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
    }
    if (!width || !height) {
        *reason = "framebuffer has a 0 dimension";
        return GraphicsContextGL::FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
    }
    // WebGL specific: no conflicting DEPTH/STENCIL/DEPTH_STENCIL attachments.
    if ((haveDepthStencil && (haveDepth || haveStencil)) || (haveDepth && haveStencil)) {
        *reason = "conflicting DEPTH/STENCIL/DEPTH_STENCIL attachments";
        return GraphicsContextGL::FRAMEBUFFER_UNSUPPORTED;
    }
    return GraphicsContextGL::FRAMEBUFFER_COMPLETE;
}

bool WebGLFramebuffer::onAccess(GraphicsContextGLOpenGL* context3d, const char** reason)
{
    if (checkStatus(reason) != GraphicsContextGL::FRAMEBUFFER_COMPLETE)
        return false;
    return initializeAttachments(context3d, reason);
}
#endif

bool WebGLFramebuffer::hasStencilBuffer() const
{
    RefPtr<WebGLAttachment> attachment = getAttachment(GraphicsContextGL::STENCIL_ATTACHMENT);
    if (!attachment)
        attachment = getAttachment(GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT);
    return attachment && attachment->isValid();
}

void WebGLFramebuffer::deleteObjectImpl(GraphicsContextGLOpenGL* context3d, PlatformGLObject object)
{
    for (auto& attachment : m_attachments.values())
        attachment->onDetached(context3d);

    context3d->deleteFramebuffer(object);
}

bool WebGLFramebuffer::initializeAttachments(GraphicsContextGLOpenGL* g3d, const char** reason)
{
    ASSERT(object());
    GCGLbitfield mask = 0;

    for (auto& entry : m_attachments) {
        GCGLenum attachmentType = entry.key;
        RefPtr<WebGLAttachment> attachment = entry.value.get();
        if (!attachment->isInitialized())
            mask |= GraphicsContextGLOpenGL::getClearBitsByAttachmentType(attachmentType);
    }
    if (!mask)
        return true;

    // We only clear un-initialized renderbuffers when they are ready to be
    // read, i.e., when the framebuffer is complete.
    if (g3d->checkFramebufferStatus(GraphicsContextGL::FRAMEBUFFER) != GraphicsContextGL::FRAMEBUFFER_COMPLETE) {
        *reason = "framebuffer not complete";
        return false;
    }

    bool initColor = mask & GraphicsContextGL::COLOR_BUFFER_BIT;
    bool initDepth = mask & GraphicsContextGL::DEPTH_BUFFER_BIT;
    bool initStencil = mask & GraphicsContextGL::STENCIL_BUFFER_BIT;

    GCGLfloat colorClearValue[] = {0, 0, 0, 0}, depthClearValue = 0;
    GCGLint stencilClearValue = 0;
    GCGLboolean colorMask[] = {0, 0, 0, 0}, depthMask = 0;
    GCGLuint stencilMask = 0xffffffff;
    GCGLboolean isScissorEnabled = 0;
    GCGLboolean isDitherEnabled = 0;
    if (initColor) {
        g3d->getFloatv(GraphicsContextGL::COLOR_CLEAR_VALUE, colorClearValue);
        g3d->getBooleanv(GraphicsContextGL::COLOR_WRITEMASK, colorMask);
        g3d->clearColor(0, 0, 0, 0);
        g3d->colorMask(true, true, true, true);
    }
    if (initDepth) {
        g3d->getFloatv(GraphicsContextGL::DEPTH_CLEAR_VALUE, &depthClearValue);
        g3d->getBooleanv(GraphicsContextGL::DEPTH_WRITEMASK, &depthMask);
        g3d->clearDepth(1.0f);
        g3d->depthMask(true);
    }
    if (initStencil) {
        g3d->getIntegerv(GraphicsContextGL::STENCIL_CLEAR_VALUE, &stencilClearValue);
        g3d->getIntegerv(GraphicsContextGL::STENCIL_WRITEMASK, reinterpret_cast<GCGLint*>(&stencilMask));
        g3d->clearStencil(0);
        g3d->stencilMask(0xffffffff);
    }
    isScissorEnabled = g3d->isEnabled(GraphicsContextGL::SCISSOR_TEST);
    g3d->disable(GraphicsContextGL::SCISSOR_TEST);
    isDitherEnabled = g3d->isEnabled(GraphicsContextGL::DITHER);
    g3d->disable(GraphicsContextGL::DITHER);

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
        g3d->enable(GraphicsContextGL::SCISSOR_TEST);
    else
        g3d->disable(GraphicsContextGL::SCISSOR_TEST);
    if (isDitherEnabled)
        g3d->enable(GraphicsContextGL::DITHER);
    else
        g3d->disable(GraphicsContextGL::DITHER);

    for (AttachmentMap::iterator it = m_attachments.begin(); it != m_attachments.end(); ++it) {
        GCGLenum attachmentType = it->key;
        auto attachment = it->value;
        GCGLbitfield bits = GraphicsContextGLOpenGL::getClearBitsByAttachmentType(attachmentType);
        if (bits & mask)
            attachment->setInitialized();
    }
    return true;
}

bool WebGLFramebuffer::isBound(GCGLenum target) const
{
    return (context()->getFramebufferBinding(target) == this);
}

void WebGLFramebuffer::drawBuffers(const Vector<GCGLenum>& bufs)
{
    m_drawBuffers = bufs;
    m_filteredDrawBuffers.resize(m_drawBuffers.size());
    for (auto& buffer : m_filteredDrawBuffers)
        buffer = GraphicsContextGL::NONE;
    drawBuffersIfNecessary(true);
}

void WebGLFramebuffer::drawBuffersIfNecessary(bool force)
{
#if ENABLE(WEBGL2)
    // FIXME: The logic here seems wrong. If we don't have WebGL 2 enabled at all, then
    // we skip the m_webglDrawBuffers check. But if we do have WebGL 2 enabled, then we
    // perform this check, for WebGL 1 contexts only.
    if (!context()->m_webglDrawBuffers && !context()->isWebGL2())
        return;
#endif
    bool reset = force;
    // This filtering works around graphics driver bugs on Mac OS X.
    for (size_t i = 0; i < m_drawBuffers.size(); ++i) {
        if (m_drawBuffers[i] != GraphicsContextGL::NONE && getAttachment(m_drawBuffers[i])) {
            if (m_filteredDrawBuffers[i] != m_drawBuffers[i]) {
                m_filteredDrawBuffers[i] = m_drawBuffers[i];
                reset = true;
            }
        } else {
            if (m_filteredDrawBuffers[i] != GraphicsContextGL::NONE) {
                m_filteredDrawBuffers[i] = GraphicsContextGL::NONE;
                reset = true;
            }
        }
    }
    if (reset) {
        context()->graphicsContextGL()->getExtensions().drawBuffersEXT(
            m_filteredDrawBuffers.size(), m_filteredDrawBuffers.data());
    }
}

GCGLenum WebGLFramebuffer::getDrawBuffer(GCGLenum drawBuffer)
{
    int index = static_cast<int>(drawBuffer - ExtensionsGL::DRAW_BUFFER0_EXT);
    ASSERT(index >= 0);
    if (index < static_cast<int>(m_drawBuffers.size()))
        return m_drawBuffers[index];
    if (drawBuffer == ExtensionsGL::DRAW_BUFFER0_EXT)
        return GraphicsContextGL::COLOR_ATTACHMENT0;
    return GraphicsContextGL::NONE;
}

}

#endif // ENABLE(WEBGL)
