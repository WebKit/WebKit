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

#pragma once

#if ENABLE(WEBGL)

#include "WebGLContextObject.h"
#include "WebGLSharedObject.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace JSC {
class SlotVisitor;
}

namespace WTF {
class AbstractLocker;
}

namespace WebCore {

class WebGLRenderbuffer;
class WebGLTexture;

class WebGLFramebuffer final : public WebGLContextObject {
public:
    class WebGLAttachment : public RefCounted<WebGLAttachment> {
    public:
        virtual ~WebGLAttachment();

#if !USE(ANGLE)
        virtual GCGLsizei getWidth() const = 0;
        virtual GCGLsizei getHeight() const = 0;
        virtual GCGLenum getFormat() const = 0;
#endif
        virtual WebGLSharedObject* getObject() const = 0;
        virtual bool isSharedObject(WebGLSharedObject*) const = 0;
        virtual bool isValid() const = 0;
        virtual bool isInitialized() const = 0;
        virtual void setInitialized() = 0;
        virtual void onDetached(const WTF::AbstractLocker&, GraphicsContextGLOpenGL*) = 0;
        virtual void attach(GraphicsContextGLOpenGL*, GCGLenum target, GCGLenum attachment) = 0;
        virtual void unattach(GraphicsContextGLOpenGL*, GCGLenum target, GCGLenum attachment) = 0;
        virtual void addMembersToOpaqueRoots(const WTF::AbstractLocker&, JSC::SlotVisitor&) = 0;

    protected:
        WebGLAttachment();
    };

    virtual ~WebGLFramebuffer();

    static Ref<WebGLFramebuffer> create(WebGLRenderingContextBase&);

    void setAttachmentForBoundFramebuffer(GCGLenum target, GCGLenum attachment, GCGLenum texTarget, WebGLTexture*, GCGLint level, GCGLint layer);
    void setAttachmentForBoundFramebuffer(GCGLenum target, GCGLenum attachment, WebGLRenderbuffer*);
    // If an object is attached to the currently bound framebuffer, remove it.
    void removeAttachmentFromBoundFramebuffer(const WTF::AbstractLocker&, GCGLenum target, WebGLSharedObject*);
    // If a given attachment point for the currently bound framebuffer is not null, remove the attached object.
    void removeAttachmentFromBoundFramebuffer(const WTF::AbstractLocker&, GCGLenum target, GCGLenum attachment);
    WebGLSharedObject* getAttachmentObject(GCGLenum) const;

#if !USE(ANGLE)
    GCGLenum getColorBufferFormat() const;
    GCGLsizei getColorBufferWidth() const;
    GCGLsizei getColorBufferHeight() const;

    // This should always be called before drawArray, drawElements, clear,
    // readPixels, copyTexImage2D, copyTexSubImage2D if this framebuffer is
    // currently bound.
    // Return false if the framebuffer is incomplete; otherwise initialize
    // the buffers if they haven't been initialized and
    // needToInitializeAttachments is true.
    bool onAccess(GraphicsContextGLOpenGL*, const char** reason);

    // Software version of glCheckFramebufferStatus(), except that when
    // FRAMEBUFFER_COMPLETE is returned, it is still possible for
    // glCheckFramebufferStatus() to return FRAMEBUFFER_UNSUPPORTED,
    // depending on hardware implementation.
    GCGLenum checkStatus(const char** reason) const;
#endif

    bool hasEverBeenBound() const { return object() && m_hasEverBeenBound; }

    void setHasEverBeenBound() { m_hasEverBeenBound = true; }

    bool hasStencilBuffer() const;

    // Wrapper for drawBuffersEXT/drawBuffersARB to work around a driver bug.
    void drawBuffers(const Vector<GCGLenum>& bufs);

    GCGLenum getDrawBuffer(GCGLenum);

    void addMembersToOpaqueRoots(const WTF::AbstractLocker&, JSC::SlotVisitor&);

private:
    WebGLFramebuffer(WebGLRenderingContextBase&);

    void deleteObjectImpl(const WTF::AbstractLocker&, GraphicsContextGLOpenGL*, PlatformGLObject) override;

    WebGLAttachment* getAttachment(GCGLenum) const;

#if !USE(ANGLE)
    // Return false if framebuffer is incomplete.
    bool initializeAttachments(GraphicsContextGLOpenGL*, const char** reason);
#endif

    // Check if the framebuffer is currently bound to the given target.
    bool isBound(GCGLenum target) const;

    // attach 'attachment' at 'attachmentPoint'.
    void attach(GCGLenum target, GCGLenum attachment, GCGLenum attachmentPoint);

    // Check if a new drawBuffers call should be issued. This is called when we add or remove an attachment.
    void drawBuffersIfNecessary(bool force);

    void setAttachmentInternal(GCGLenum attachment, GCGLenum texTarget, WebGLTexture*, GCGLint level, GCGLint layer);
    void setAttachmentInternal(GCGLenum attachment, WebGLRenderbuffer*);
    // If a given attachment point for the currently bound framebuffer is not
    // null, remove the attached object.
    void removeAttachmentInternal(const WTF::AbstractLocker&, GCGLenum attachment);

    typedef WTF::HashMap<GCGLenum, RefPtr<WebGLAttachment>> AttachmentMap;

    AttachmentMap m_attachments;

    bool m_hasEverBeenBound;

    Vector<GCGLenum> m_drawBuffers;
    Vector<GCGLenum> m_filteredDrawBuffers;
};

} // namespace WebCore

#endif
