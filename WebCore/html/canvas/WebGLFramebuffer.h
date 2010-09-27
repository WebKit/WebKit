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

#ifndef WebGLFramebuffer_h
#define WebGLFramebuffer_h

#include "WebGLObject.h"

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class WebGLFramebuffer : public WebGLObject {
public:
    virtual ~WebGLFramebuffer() { deleteObject(); }

    static PassRefPtr<WebGLFramebuffer> create(WebGLRenderingContext*);

    bool isDepthAttached() const { return (m_depthAttachment && m_depthAttachment->object()); }
    bool isStencilAttached() const { return (m_stencilAttachment && m_stencilAttachment->object()); }
    bool isDepthStencilAttached() const { return (m_depthStencilAttachment && m_depthStencilAttachment->object()); }

    void setAttachment(unsigned long, WebGLObject*);
    // If an object is attached to the framebuffer, remove it.
    void removeAttachment(WebGLObject*);

    // This function is called right after a framebuffer is bound.
    // Because renderbuffers and textures attached to the framebuffer might
    // have changed and the framebuffer might have become complete when it
    // isn't bound, so we need to clear un-initialized renderbuffers.
    void onBind();

    // When a texture or a renderbuffer changes, we need to check the
    // current bound framebuffer; if the newly changed object is attached
    // to the framebuffer and the framebuffer becomes complete, we need to
    // clear un-initialized renderbuffers.
    void onAttachedObjectChange(WebGLObject*);

    unsigned long getColorBufferFormat();

protected:
    WebGLFramebuffer(WebGLRenderingContext*);

    virtual void deleteObjectImpl(Platform3DObject);

private:
    virtual bool isFramebuffer() const { return true; }

    bool isUninitialized(WebGLObject*);
    void setInitialized(WebGLObject*);
    void initializeRenderbuffers();

    RefPtr<WebGLObject> m_colorAttachment;
    RefPtr<WebGLObject> m_depthAttachment;
    RefPtr<WebGLObject> m_stencilAttachment;
    RefPtr<WebGLObject> m_depthStencilAttachment;
};

} // namespace WebCore

#endif // WebGLFramebuffer_h
