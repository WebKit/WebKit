/*
 * Copyright (C) 2009-2021 Apple Inc. All rights reserved.
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

#include "WebGLContextGroup.h"
#include "WebGLDrawBuffers.h"
#include "WebGLRenderingContextBase.h"
#include <JavaScriptCore/SlotVisitor.h>
#include <JavaScriptCore/SlotVisitorInlines.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>

#if !USE(ANGLE)
#include "GraphicsContextGLOpenGL.h"
#endif
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
        void onDetached(const AbstractLocker&, GraphicsContextGL*) override;
        void attach(GraphicsContextGL*, GCGLenum target, GCGLenum attachment) override;
        void unattach(GraphicsContextGL*, GCGLenum target, GCGLenum attachment) override;
        void addMembersToOpaqueRoots(const AbstractLocker&, JSC::AbstractSlotVisitor&) override;

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

    void WebGLRenderbufferAttachment::onDetached(const AbstractLocker& locker, GraphicsContextGL* context)
    {
        m_renderbuffer->onDetached(locker, context);
    }

    void WebGLRenderbufferAttachment::attach(GraphicsContextGL* context, GCGLenum target, GCGLenum attachment)
    {
        PlatformGLObject object = objectOrZero(m_renderbuffer.get());
        context->framebufferRenderbuffer(target, attachment, GraphicsContextGL::RENDERBUFFER, object);
    }

    void WebGLRenderbufferAttachment::unattach(GraphicsContextGL* context, GCGLenum target, GCGLenum attachment)
    {
#if !USE(ANGLE)
        if (attachment == GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT) {
            context->framebufferRenderbuffer(target, GraphicsContextGL::DEPTH_ATTACHMENT, GraphicsContextGL::RENDERBUFFER, 0);
            context->framebufferRenderbuffer(target, GraphicsContextGL::STENCIL_ATTACHMENT, GraphicsContextGL::RENDERBUFFER, 0);
        } else
#endif
            context->framebufferRenderbuffer(target, attachment, GraphicsContextGL::RENDERBUFFER, 0);
    }

    void WebGLRenderbufferAttachment::addMembersToOpaqueRoots(const AbstractLocker&, JSC::AbstractSlotVisitor& visitor)
    {
        visitor.addOpaqueRoot(m_renderbuffer.get());
    }

    class WebGLTextureAttachment : public WebGLFramebuffer::WebGLAttachment {
    public:
        static Ref<WebGLFramebuffer::WebGLAttachment> create(WebGLTexture*, GCGLenum target, GCGLint level, GCGLint layer);

    private:
        WebGLTextureAttachment(WebGLTexture*, GCGLenum target, GCGLint level, GCGLint layer);
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
        void onDetached(const AbstractLocker&, GraphicsContextGL*) override;
        void attach(GraphicsContextGL*, GCGLenum target, GCGLenum attachment) override;
        void unattach(GraphicsContextGL*, GCGLenum target, GCGLenum attachment) override;
        void addMembersToOpaqueRoots(const AbstractLocker&, JSC::AbstractSlotVisitor&) override;

        WebGLTextureAttachment() { };

        RefPtr<WebGLTexture> m_texture;
        GCGLenum m_target;
        GCGLint m_level;
        GCGLint m_layer;
    };

    Ref<WebGLFramebuffer::WebGLAttachment> WebGLTextureAttachment::create(WebGLTexture* texture, GCGLenum target, GCGLint level, GCGLint layer)
    {
        return adoptRef(*new WebGLTextureAttachment(texture, target, level, layer));
    }

    WebGLTextureAttachment::WebGLTextureAttachment(WebGLTexture* texture, GCGLenum target, GCGLint level, GCGLint layer)
        : m_texture(texture)
        , m_target(target)
        , m_level(level)
        , m_layer(layer)
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

    void WebGLTextureAttachment::onDetached(const AbstractLocker& locker, GraphicsContextGL* context)
    {
        m_texture->onDetached(locker, context);
    }

    void WebGLTextureAttachment::attach(GraphicsContextGL* context, GCGLenum target, GCGLenum attachment)
    {
        PlatformGLObject object = objectOrZero(m_texture.get());
        if (m_target == GraphicsContextGL::TEXTURE_3D || m_target == GraphicsContextGL::TEXTURE_2D_ARRAY)
            context->framebufferTextureLayer(target, attachment, object, m_level, m_layer);
        else
            context->framebufferTexture2D(target, attachment, m_target, object, m_level);
    }

    void WebGLTextureAttachment::unattach(GraphicsContextGL* context, GCGLenum target, GCGLenum attachment)
    {
#if USE(ANGLE)
        // GL_DEPTH_STENCIL_ATTACHMENT attachment is valid in ES3.
        if (m_target == GraphicsContextGL::TEXTURE_3D || m_target == GraphicsContextGL::TEXTURE_2D_ARRAY)
            context->framebufferTextureLayer(target, attachment, 0, m_level, m_layer);
        else
            context->framebufferTexture2D(target, attachment, m_target, 0, m_level);
#else
        UNUSED_PARAM(target);
        if (attachment == GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT) {
            context->framebufferTexture2D(GraphicsContextGL::FRAMEBUFFER, GraphicsContextGL::DEPTH_ATTACHMENT, m_target, 0, m_level);
            context->framebufferTexture2D(GraphicsContextGL::FRAMEBUFFER, GraphicsContextGL::STENCIL_ATTACHMENT, m_target, 0, m_level);
        } else
            context->framebufferTexture2D(GraphicsContextGL::FRAMEBUFFER, attachment, m_target, 0, m_level);
#endif
    }

    void WebGLTextureAttachment::addMembersToOpaqueRoots(const AbstractLocker&, JSC::AbstractSlotVisitor& visitor)
    {
        visitor.addOpaqueRoot(m_texture.get());
    }

} // anonymous namespace

#if !USE(ANGLE)
static unsigned getClearBitsByAttachmentType(GCGLenum attachment)
{
    switch (attachment) {
    case GraphicsContextGL::COLOR_ATTACHMENT0:
    case GraphicsContextGL::COLOR_ATTACHMENT1_EXT:
    case GraphicsContextGL::COLOR_ATTACHMENT2_EXT:
    case GraphicsContextGL::COLOR_ATTACHMENT3_EXT:
    case GraphicsContextGL::COLOR_ATTACHMENT4_EXT:
    case GraphicsContextGL::COLOR_ATTACHMENT5_EXT:
    case GraphicsContextGL::COLOR_ATTACHMENT6_EXT:
    case GraphicsContextGL::COLOR_ATTACHMENT7_EXT:
    case GraphicsContextGL::COLOR_ATTACHMENT8_EXT:
    case GraphicsContextGL::COLOR_ATTACHMENT9_EXT:
    case GraphicsContextGL::COLOR_ATTACHMENT10_EXT:
    case GraphicsContextGL::COLOR_ATTACHMENT11_EXT:
    case GraphicsContextGL::COLOR_ATTACHMENT12_EXT:
    case GraphicsContextGL::COLOR_ATTACHMENT13_EXT:
    case GraphicsContextGL::COLOR_ATTACHMENT14_EXT:
    case GraphicsContextGL::COLOR_ATTACHMENT15_EXT:
        return GraphicsContextGL::COLOR_BUFFER_BIT;
    case GraphicsContextGL::DEPTH_ATTACHMENT:
        return GraphicsContextGL::DEPTH_BUFFER_BIT;
    case GraphicsContextGL::STENCIL_ATTACHMENT:
        return GraphicsContextGL::STENCIL_BUFFER_BIT;
    case GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT:
        return GraphicsContextGL::DEPTH_BUFFER_BIT | GraphicsContextGL::STENCIL_BUFFER_BIT;
    default:
        return 0;
    }
}

static unsigned getClearBitsByFormat(GCGLenum format)
{
    switch (format) {
    case GraphicsContextGL::RGB:
    case GraphicsContextGL::RGBA:
    case GraphicsContextGL::LUMINANCE_ALPHA:
    case GraphicsContextGL::LUMINANCE:
    case GraphicsContextGL::ALPHA:
    case GraphicsContextGL::R8:
    case GraphicsContextGL::R8_SNORM:
    case GraphicsContextGL::R16F:
    case GraphicsContextGL::R32F:
    case GraphicsContextGL::R8UI:
    case GraphicsContextGL::R8I:
    case GraphicsContextGL::R16UI:
    case GraphicsContextGL::R16I:
    case GraphicsContextGL::R32UI:
    case GraphicsContextGL::R32I:
    case GraphicsContextGL::RG8:
    case GraphicsContextGL::RG8_SNORM:
    case GraphicsContextGL::RG16F:
    case GraphicsContextGL::RG32F:
    case GraphicsContextGL::RG8UI:
    case GraphicsContextGL::RG8I:
    case GraphicsContextGL::RG16UI:
    case GraphicsContextGL::RG16I:
    case GraphicsContextGL::RG32UI:
    case GraphicsContextGL::RG32I:
    case GraphicsContextGL::RGB8:
    case GraphicsContextGL::SRGB8:
    case GraphicsContextGL::RGB565:
    case GraphicsContextGL::RGB8_SNORM:
    case GraphicsContextGL::R11F_G11F_B10F:
    case GraphicsContextGL::RGB9_E5:
    case GraphicsContextGL::RGB16F:
    case GraphicsContextGL::RGB32F:
    case GraphicsContextGL::RGB8UI:
    case GraphicsContextGL::RGB8I:
    case GraphicsContextGL::RGB16UI:
    case GraphicsContextGL::RGB16I:
    case GraphicsContextGL::RGB32UI:
    case GraphicsContextGL::RGB32I:
    case GraphicsContextGL::RGBA8:
    case GraphicsContextGL::SRGB8_ALPHA8:
    case GraphicsContextGL::RGBA8_SNORM:
    case GraphicsContextGL::RGB5_A1:
    case GraphicsContextGL::RGBA4:
    case GraphicsContextGL::RGB10_A2:
    case GraphicsContextGL::RGBA16F:
    case GraphicsContextGL::RGBA32F:
    case GraphicsContextGL::RGBA8UI:
    case GraphicsContextGL::RGBA8I:
    case GraphicsContextGL::RGB10_A2UI:
    case GraphicsContextGL::RGBA16UI:
    case GraphicsContextGL::RGBA16I:
    case GraphicsContextGL::RGBA32I:
    case GraphicsContextGL::RGBA32UI:
    case GraphicsContextGL::SRGB_EXT:
    case GraphicsContextGL::SRGB_ALPHA_EXT:
        return GraphicsContextGL::COLOR_BUFFER_BIT;
    case GraphicsContextGL::DEPTH_COMPONENT16:
    case GraphicsContextGL::DEPTH_COMPONENT24:
    case GraphicsContextGL::DEPTH_COMPONENT32F:
    case GraphicsContextGL::DEPTH_COMPONENT:
        return GraphicsContextGL::DEPTH_BUFFER_BIT;
    case GraphicsContextGL::STENCIL_INDEX8:
        return GraphicsContextGL::STENCIL_BUFFER_BIT;
    case GraphicsContextGL::DEPTH_STENCIL:
    case GraphicsContextGL::DEPTH24_STENCIL8:
    case GraphicsContextGL::DEPTH32F_STENCIL8:
        return GraphicsContextGL::DEPTH_BUFFER_BIT | GraphicsContextGL::STENCIL_BUFFER_BIT;
    default:
        return 0;
    }
}

static bool isAttachmentComplete(WebGLFramebuffer::WebGLAttachment* attachedObject, GCGLenum attachment, const char** reason)
{
    ASSERT(attachedObject && attachedObject->isValid());
    ASSERT(reason);
    GCGLenum format = attachedObject->getFormat();
    unsigned need = getClearBitsByAttachmentType(attachment);
    unsigned have = getClearBitsByFormat(format);

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
WebGLFramebuffer::WebGLAttachment::WebGLAttachment() = default;

WebGLFramebuffer::WebGLAttachment::~WebGLAttachment() = default;

Ref<WebGLFramebuffer> WebGLFramebuffer::create(WebGLRenderingContextBase& ctx)
{
    return adoptRef(*new WebGLFramebuffer(ctx));
}

#if ENABLE(WEBXR)

Ref<WebGLFramebuffer> WebGLFramebuffer::createOpaque(WebGLRenderingContextBase& ctx)
{
    auto framebuffer = adoptRef(*new WebGLFramebuffer(ctx));
    framebuffer->m_opaque = true;
    return framebuffer;
}

#endif

WebGLFramebuffer::WebGLFramebuffer(WebGLRenderingContextBase& ctx)
    : WebGLContextObject(ctx)
    , m_hasEverBeenBound(false)
{
    setObject(ctx.graphicsContextGL()->createFramebuffer());
}

WebGLFramebuffer::~WebGLFramebuffer()
{
    if (!context())
        return;

    runDestructor();
}

void WebGLFramebuffer::setAttachmentForBoundFramebuffer(GCGLenum target, GCGLenum attachment, GCGLenum texTarget, WebGLTexture* texture, GCGLint level, GCGLint layer)
{
    ASSERT(object());
    ASSERT(isBound(target));
    setAttachmentInternal(attachment, texTarget, texture, level, layer);
    if (context()->isWebGL2()) {
        GCGLuint textureID = objectOrZero(texture);
        // texTarget can be 0 if detaching using framebufferTextureLayer.
        ASSERT(texTarget || !textureID);
        switch (texTarget) {
        case 0:
        case GraphicsContextGL::TEXTURE_3D:
        case GraphicsContextGL::TEXTURE_2D_ARRAY:
            context()->graphicsContextGL()->framebufferTextureLayer(target, attachment, textureID, level, layer);
            break;
        default:
            ASSERT(!layer);
            context()->graphicsContextGL()->framebufferTexture2D(target, attachment, texTarget, textureID, level);
            break;
        }
    } else {
        ASSERT(!layer);
#if USE(ANGLE)
        context()->graphicsContextGL()->framebufferTexture2D(target, attachment, texTarget, objectOrZero(texture), level);
#else
        GCGLuint textureID = objectOrZero(texture);
        if (attachment == GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT) {
            context()->graphicsContextGL()->framebufferTexture2D(target, GraphicsContextGL::DEPTH_ATTACHMENT, texTarget, textureID, level);
            context()->graphicsContextGL()->framebufferTexture2D(target, GraphicsContextGL::STENCIL_ATTACHMENT, texTarget, textureID, level);
        } else
            context()->graphicsContextGL()->framebufferTexture2D(target, attachment, texTarget, textureID, level);
#endif
    }
}

void WebGLFramebuffer::setAttachmentForBoundFramebuffer(GCGLenum target, GCGLenum attachment, WebGLRenderbuffer* renderbuffer)
{
    ASSERT(object());
    ASSERT(isBound(target));
    setAttachmentInternal(attachment, renderbuffer);
    context()->graphicsContextGL()->framebufferRenderbuffer(target, attachment, GraphicsContextGL::RENDERBUFFER, objectOrZero(renderbuffer));
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
        attachmentObject->attach(context()->graphicsContextGL(), target, attachmentPoint);
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

void WebGLFramebuffer::removeAttachmentFromBoundFramebuffer(const AbstractLocker& locker, GCGLenum target, GCGLenum attachment)
{
    if (!context()) {
        // Context has been deleted - should not be calling this.
        return;
    }

#if ASSERT_ENABLED
    ASSERT(isBound(target));
#else
    UNUSED_PARAM(target);
#endif
    if (!object())
        return;

    RefPtr<WebGLAttachment> attachmentObject = getAttachment(attachment);
    if (attachmentObject) {
        attachmentObject->onDetached(locker, context()->graphicsContextGL());
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

void WebGLFramebuffer::removeAttachmentFromBoundFramebuffer(const AbstractLocker& locker, GCGLenum target, WebGLSharedObject* attachment)
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
                attachmentObject->unattach(context()->graphicsContextGL(), target, attachmentType);
                removeAttachmentFromBoundFramebuffer(locker, target, attachmentType);
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
#if ENABLE(WEBXR)
    // https://immersive-web.github.io/webxr/#opaque-framebuffer
    if (m_opaque) {
        if (m_opaqueActive)
            return GL_FRAMEBUFFER_COMPLETE;
        *reason = "An opaque framebuffer is considered incomplete outside of a requestAnimationFrame";
        return GL_FRAMEBUFFER_UNSUPPORTED;
    }
#endif

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
        if (attachmentFormat == GraphicsContextGL::SRGB_EXT)
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

bool WebGLFramebuffer::onAccess(GraphicsContextGL* context3d, const char** reason)
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

void WebGLFramebuffer::deleteObjectImpl(const AbstractLocker& locker, GraphicsContextGL* context3d, PlatformGLObject object)
{
    for (auto& attachment : m_attachments.values())
        attachment->onDetached(locker, context3d);

    context3d->deleteFramebuffer(object);
}

#if !USE(ANGLE)
bool WebGLFramebuffer::initializeAttachments(GraphicsContextGL* g3d, const char** reason)
{
    if (!context()) {
        // Context has been deleted - should not be calling this.
        return false;
    }
    Locker locker { objectGraphLockForContext() };

    ASSERT(object());
    GCGLbitfield mask = 0;

    for (auto& entry : m_attachments) {
        GCGLenum attachmentType = entry.key;
        RefPtr<WebGLAttachment> attachment = entry.value.get();
        if (!attachment->isInitialized())
            mask |= getClearBitsByAttachmentType(attachmentType);
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
        depthClearValue = g3d->getFloat(GraphicsContextGL::DEPTH_CLEAR_VALUE);
        depthMask = g3d->getBoolean(GraphicsContextGL::DEPTH_WRITEMASK);
        g3d->clearDepth(1.0f);
        g3d->depthMask(true);
    }
    if (initStencil) {
        stencilClearValue = g3d->getInteger(GraphicsContextGL::STENCIL_CLEAR_VALUE);
        stencilMask = g3d->getInteger(GraphicsContextGL::STENCIL_WRITEMASK);
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
        GCGLbitfield bits = getClearBitsByAttachmentType(attachmentType);
        if (bits & mask)
            attachment->setInitialized();
    }
    return true;
}
#endif

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
    if (context()->isWebGL2() || context()->m_webglDrawBuffers) {
        bool reset = force;
        // This filtering works around graphics driver bugs on macOS.
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
            if (context()->isWebGL2())
                context()->graphicsContextGL()->drawBuffers(m_filteredDrawBuffers);
            else
                context()->graphicsContextGL()->drawBuffersEXT(m_filteredDrawBuffers);
        }
    }
}

GCGLenum WebGLFramebuffer::getDrawBuffer(GCGLenum drawBuffer)
{
    int index = static_cast<int>(drawBuffer - GraphicsContextGL::DRAW_BUFFER0_EXT);
    ASSERT(index >= 0);
    if (index < static_cast<int>(m_drawBuffers.size()))
        return m_drawBuffers[index];
    if (drawBuffer == GraphicsContextGL::DRAW_BUFFER0_EXT)
        return GraphicsContextGL::COLOR_ATTACHMENT0;
    return GraphicsContextGL::NONE;
}

void WebGLFramebuffer::addMembersToOpaqueRoots(const AbstractLocker& locker, JSC::AbstractSlotVisitor& visitor)
{
    for (auto& entry : m_attachments)
        entry.value->addMembersToOpaqueRoots(locker, visitor);
}

void WebGLFramebuffer::setAttachmentInternal(GCGLenum attachment, GCGLenum texTarget, WebGLTexture* texture, GCGLint level, GCGLint layer)
{
    if (!context()) {
        // Context has been deleted - should not be calling this.
        return;
    }
    Locker locker { objectGraphLockForContext() };

    removeAttachmentInternal(locker, attachment);
    if (texture && texture->object()) {
        m_attachments.set(attachment, WebGLTextureAttachment::create(texture, texTarget, level, layer));
        drawBuffersIfNecessary(false);
        texture->onAttached();
    }
}

void WebGLFramebuffer::setAttachmentInternal(GCGLenum attachment, WebGLRenderbuffer* renderbuffer)
{
    if (!context()) {
        // Context has been deleted - should not be calling this.
        return;
    }
    Locker locker { objectGraphLockForContext() };

    removeAttachmentInternal(locker, attachment);
    if (renderbuffer && renderbuffer->object()) {
        m_attachments.set(attachment, WebGLRenderbufferAttachment::create(renderbuffer));
        drawBuffersIfNecessary(false);
        renderbuffer->onAttached();
    }
}

void WebGLFramebuffer::removeAttachmentInternal(const AbstractLocker& locker, GCGLenum attachment)
{
    WebGLAttachment* attachmentObject = getAttachment(attachment);
    if (attachmentObject) {
        attachmentObject->onDetached(locker, context()->graphicsContextGL());
        m_attachments.remove(attachment);
        drawBuffersIfNecessary(false);
    }
}

}

#endif // ENABLE(WEBGL)
