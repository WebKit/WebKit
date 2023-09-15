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

#include "GraphicsContextGL.h"
#include "WebGLObject.h"

namespace WebCore {

class WebGLRenderbuffer final : public WebGLObject {
public:
    virtual ~WebGLRenderbuffer();

    static RefPtr<WebGLRenderbuffer> create(WebGLRenderingContextBase&);

    void setInternalFormat(GCGLenum internalformat)
    {
        m_internalFormat = internalformat;
    }
    GCGLenum getInternalFormat() const { return m_internalFormat; }

    void setSize(GCGLsizei width, GCGLsizei height)
    {
        m_width = width;
        m_height = height;
    }
    GCGLsizei getWidth() const { return m_width; }
    GCGLsizei getHeight() const { return m_height; }

    void setIsValid(bool isValid) { m_isValid = isValid; }
    bool isValid() const { return m_isValid; }

    void didBind() { m_hasEverBeenBound = true; }
    bool hasEverBeenBound() const { return m_hasEverBeenBound; }

    bool isUsable() const { return object() && !isDeleted(); }
    bool isInitialized() const { return m_hasEverBeenBound; }

private:
    WebGLRenderbuffer(WebGLRenderingContextBase&, PlatformGLObject);

    void deleteObjectImpl(const AbstractLocker&, GraphicsContextGL*, PlatformGLObject) override;

    GCGLenum m_internalFormat { GraphicsContextGL::RGBA4 };
    GCGLsizei m_width { 0 };
    GCGLsizei m_height { 0 };
    bool m_isValid { true }; // This is only false if internalFormat is DEPTH_STENCIL and packed_depth_stencil is not supported.
    bool m_hasEverBeenBound { false };
};

} // namespace WebCore

#endif
