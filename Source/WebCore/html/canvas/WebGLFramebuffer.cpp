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

#include "WebCoreOpaqueRootInlines.h"
#include "WebGLDrawBuffers.h"
#include "WebGLRenderingContextBase.h"
#include <JavaScriptCore/SlotVisitor.h>
#include <JavaScriptCore/SlotVisitorInlines.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>

namespace WebCore {

static void entryAddMembersToOpaqueRoots(const WebGLFramebuffer::AttachmentEntry& entry, const AbstractLocker&, JSC::AbstractSlotVisitor& visitor)
{
    // This runs on GC thread, so use const RefPtr<>& to make ensure no RefPtrs are created.
    switchOn(entry,
        [&](const RefPtr<WebGLRenderbuffer>& renderbuffer) {
            addWebCoreOpaqueRoot(visitor, renderbuffer.get());
        },
        [&](const WebGLFramebuffer::TextureAttachment& textureAttachment) {
            addWebCoreOpaqueRoot(visitor, textureAttachment.texture.get());
        },
        [&](const WebGLFramebuffer::TextureLayerAttachment& layerAttachment) {
            addWebCoreOpaqueRoot(visitor, layerAttachment.texture.get());
        }
    );
}

static void entryDetachAndClear(WebGLFramebuffer::AttachmentEntry& entry, const AbstractLocker& locker, GraphicsContextGL* gl)
{
    switchOn(entry,
        [&](RefPtr<WebGLRenderbuffer>& renderbuffer) {
            renderbuffer->onDetached(locker, gl);
            renderbuffer = nullptr;
        },
        [&](WebGLFramebuffer::TextureAttachment& textureAttachment) {
            RefPtr { textureAttachment.texture }->onDetached(locker, gl);
            textureAttachment.texture = nullptr;
        },
        [&](WebGLFramebuffer::TextureLayerAttachment& layerAttachment) {
            RefPtr { layerAttachment.texture }->onDetached(locker, gl);
            layerAttachment.texture = nullptr;
        }
    );
}

static void entryAttach(WebGLFramebuffer::AttachmentEntry& entry)
{
    switchOn(entry,
        [&](RefPtr<WebGLRenderbuffer>& renderbuffer) {
            renderbuffer->onAttached();
        },
        [&](WebGLFramebuffer::TextureAttachment& textureAttachment) {
            textureAttachment.texture->onAttached();
        },
        [&](WebGLFramebuffer::TextureLayerAttachment& layerAttachment) {
            layerAttachment.texture->onAttached();
        });
}

static void entryContextSetAttachment(const WebGLFramebuffer::AttachmentEntry& entry, GraphicsContextGL* gl, GCGLenum target, GCGLenum attachment)
{
    switchOn(entry,
        [&](RefPtr<WebGLRenderbuffer> renderbuffer) {
            gl->framebufferRenderbuffer(target, attachment, GraphicsContextGL::RENDERBUFFER, objectOrZero(renderbuffer));
        },
        [&](WebGLFramebuffer::TextureAttachment textureAttachment) {
            gl->framebufferTexture2D(target, attachment, textureAttachment.texTarget, objectOrZero(textureAttachment.texture), textureAttachment.level);
        },
        [&](WebGLFramebuffer::TextureLayerAttachment layerAttachment) {
            gl->framebufferTextureLayer(target, attachment, objectOrZero(layerAttachment.texture), layerAttachment.level, layerAttachment.layer);
        });
}

static WebGLFramebuffer::AttachmentObject entryObject(const WebGLFramebuffer::AttachmentEntry& entry)
{
    return switchOn(entry,
        [&](RefPtr<WebGLRenderbuffer> renderbuffer) -> WebGLFramebuffer::AttachmentObject {
            return renderbuffer;
        },
        [&](const WebGLFramebuffer::TextureAttachment& textureAttachment) -> WebGLFramebuffer::AttachmentObject {
            return textureAttachment.texture;
        },
        [&](const WebGLFramebuffer::TextureLayerAttachment& layerAttachment) -> WebGLFramebuffer::AttachmentObject {
            return layerAttachment.texture;
        });
}

static bool entryHasObject(const WebGLFramebuffer::AttachmentEntry& entry)
{
    return switchOn(entryObject(entry),
        [&] (auto&& object) -> bool {
            return object;
        });
}

Ref<WebGLFramebuffer> WebGLFramebuffer::createLost()
{
    return adoptRef(*new WebGLFramebuffer { });
}

Ref<WebGLFramebuffer> WebGLFramebuffer::create(WebGLRenderingContextBase& context)
{
    auto object = protect(context.graphicsContextGL())->createFramebuffer();
    if (!object)
        return createLost();
    return adoptRef(*new WebGLFramebuffer { context, object, Type::Plain });
}

#if ENABLE(WEBXR)
RefPtr<WebGLFramebuffer> WebGLFramebuffer::createOpaque(WebGLRenderingContextBase& context)
{
    auto object = protect(context.graphicsContextGL())->createFramebuffer();
    if (!object)
        return nullptr;
    return adoptRef(*new WebGLFramebuffer { context, object, Type::Opaque });
}
#endif

WebGLFramebuffer::WebGLFramebuffer(WebGLRenderingContextBase& context, PlatformGLObject object, Type type)
    : WebGLObject(context, object)
#if ENABLE(WEBXR)
    , m_isOpaque(type == Type::Opaque)
#endif
{
    UNUSED_PARAM(type);
}

WebGLFramebuffer::WebGLFramebuffer()
#if ENABLE(WEBXR)
    : m_isOpaque(false)
#endif
{
}

WebGLFramebuffer::~WebGLFramebuffer()
{
    if (!context())
        return;

    runDestructor();
}

void WebGLFramebuffer::setAttachmentForBoundFramebuffer(GCGLenum target, GCGLenum attachment, AttachmentEntry entry)
{
    ASSERT(object());
    ASSERT(isBound(target));
    RefPtr gl = graphicsContextGL();
    if (attachment == GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT && context()->isWebGL2()) {
        setAttachmentInternal(GraphicsContextGL::STENCIL_ATTACHMENT, entry);
        entryContextSetAttachment(entry, gl.get(), target, GraphicsContextGL::STENCIL_ATTACHMENT);
        attachment = GraphicsContextGL::DEPTH_ATTACHMENT;
    }
    setAttachmentInternal(attachment, entry);
    entryContextSetAttachment(entry, gl.get(), target, attachment);
}

std::optional<WebGLFramebuffer::AttachmentObject> WebGLFramebuffer::getAttachmentObject(GCGLenum attachment) const
{
    if (!object())
        return std::nullopt;
    auto it = m_attachments.find(attachment);
    if (it == m_attachments.end())
        return std::nullopt;
    return entryObject(it->value);
}

void WebGLFramebuffer::removeAttachmentFromBoundFramebuffer(const AbstractLocker& locker, GCGLenum target, AttachmentObject removedObject)
{
    ASSERT(isBound(target));
    if (!object())
        return;
    bool checkMore = true;
    RefPtr gl = graphicsContextGL();
    do {
        checkMore = false;
        for (auto it = m_attachments.begin(); it != m_attachments.end(); ++it) {
            if (entryObject(it->value) != removedObject)
                continue;
            GCGLenum attachment = it->key;
            auto entry = WTF::move(it->value);
            m_attachments.remove(it);
            checkMore = true;
            entryDetachAndClear(entry, locker, gl.get());
            entryContextSetAttachment(entry, gl.get(), target, attachment);
            break;
        }
    } while (checkMore);
}

void WebGLFramebuffer::deleteObjectImpl(const AbstractLocker& locker, GraphicsContextGL* context3d, PlatformGLObject object)
{
    for (auto& entry : m_attachments.values())
        entryDetachAndClear(entry, locker, context3d);

    context3d->deleteFramebuffer(object);
}

bool WebGLFramebuffer::isBound(GCGLenum target) const
{
    return context()->getFramebufferBinding(target) == this;
}

void WebGLFramebuffer::drawBuffers(const Vector<GCGLenum>& bufs)
{
    m_drawBuffers = bufs;
    // ANGLE handles filtering of draw buffers for unattached color attachments internally,
    // so we pass the user-specified buffers directly.
    RefPtr context = this->context();
    if (context->isWebGL2())
        protect(context->graphicsContextGL())->drawBuffers(m_drawBuffers);
    else if (context->m_webglDrawBuffers)
        protect(context->graphicsContextGL())->drawBuffersEXT(m_drawBuffers);
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
        entryAddMembersToOpaqueRoots(entry.value, locker, visitor);
}

void WebGLFramebuffer::setAttachmentInternal(GCGLenum attachment, AttachmentEntry entry)
{
    if (!context()) {
        // Context has been deleted - should not be calling this.
        return;
    }
    Locker locker { objectGraphLockForContext() };
    auto it = m_attachments.find(attachment);
    if (it != m_attachments.end()) {
        if (entry == it->value)
            return;
        RefPtr gl = graphicsContextGL();
        entryDetachAndClear(it->value, locker, gl.get());
        m_attachments.remove(it);
    }
    if (!entryHasObject(entry))
        return;
    auto result = m_attachments.add(attachment, WTF::move(entry));
    entryAttach(result.iterator->value);
}

}

#endif // ENABLE(WEBGL)
