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

#pragma once

#if ENABLE(WEBGL)

#include "WebGLObject.h"
#include <variant>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace JSC {
class AbstractSlotVisitor;
}

namespace WTF {
class AbstractLocker;
}

namespace WebCore {

class WebGLRenderbuffer;
class WebGLTexture;

class WebGLFramebuffer final : public WebGLObject {
public:
    virtual ~WebGLFramebuffer();

    static RefPtr<WebGLFramebuffer> create(WebGLRenderingContextBase&);
#if ENABLE(WEBXR)
    static RefPtr<WebGLFramebuffer> createOpaque(WebGLRenderingContextBase&);
#endif

    struct TextureAttachment {
        RefPtr<WebGLTexture> texture;
        GCGLenum texTarget;
        GCGLint level;
        friend bool operator==(const TextureAttachment&, const TextureAttachment&) = default;
    };
    struct TextureLayerAttachment {
        RefPtr<WebGLTexture> texture;
        GCGLint level;
        GCGLint layer;
        friend bool operator==(const TextureLayerAttachment&, const TextureLayerAttachment&) = default;
    };
    using AttachmentEntry = std::variant<RefPtr<WebGLRenderbuffer>, TextureAttachment, TextureLayerAttachment>;

    void setAttachmentForBoundFramebuffer(GCGLenum target, GCGLenum attachment, AttachmentEntry);

    // Below are nonnull. RefPtr instead of Ref due to call site object identity
    // purposes, call site uses i.e pointer operator==.
    using AttachmentObject = std::variant<RefPtr<WebGLRenderbuffer>, RefPtr<WebGLTexture>>;

    // If an object is attached to the currently bound framebuffer, remove it.
    void removeAttachmentFromBoundFramebuffer(const AbstractLocker&, GCGLenum target, AttachmentObject);
    std::optional<AttachmentObject> getAttachmentObject(GCGLenum) const;

    void didBind() { m_hasEverBeenBound = true; }

    // Wrapper for drawBuffersEXT/drawBuffersARB to work around a driver bug.
    void drawBuffers(const Vector<GCGLenum>& bufs);

    GCGLenum getDrawBuffer(GCGLenum);

    void addMembersToOpaqueRoots(const AbstractLocker&, JSC::AbstractSlotVisitor&);

#if ENABLE(WEBXR)
    bool isOpaque() const { return m_isOpaque; }
#endif

    bool isUsable() const { return object() && !isDeleted(); }
    bool isInitialized() const { return m_hasEverBeenBound; }

private:
    enum class Type : bool {
        Plain,
#if ENABLE(WEBXR)
        Opaque
#endif
    };
    WebGLFramebuffer(WebGLRenderingContextBase&, PlatformGLObject, Type);

    void deleteObjectImpl(const AbstractLocker&, GraphicsContextGL*, PlatformGLObject) override;

    // If a given attachment point for the currently bound framebuffer is not null, remove the attached object.
    void removeAttachmentFromBoundFramebuffer(const AbstractLocker&, GCGLenum target, GCGLenum attachment);

    // Check if the framebuffer is currently bound to the given target.
    bool isBound(GCGLenum target) const;

    // Check if a new drawBuffers call should be issued. This is called when we add or remove an attachment.
    void drawBuffersIfNecessary(bool force);

    void setAttachmentInternal(GCGLenum attachment, AttachmentEntry);
    // If a given attachment point for the currently bound framebuffer is not
    // null, remove the attached object.
    void removeAttachmentInternal(const AbstractLocker&, GCGLenum attachment);

    HashMap<GCGLenum, AttachmentEntry> m_attachments;
    bool m_hasEverBeenBound { false };
    Vector<GCGLenum> m_drawBuffers;
    Vector<GCGLenum> m_filteredDrawBuffers;
#if ENABLE(WEBXR)
    const bool m_isOpaque;
#endif
};

} // namespace WebCore

#endif
