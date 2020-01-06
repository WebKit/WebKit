/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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

namespace WebCore {

class GraphicsContextGLOpenGL;
class WebGLContextGroup;
class WebGLRenderingContextBase;

// WebGLSharedObject the base class for objects that can be shared by multiple WebGLRenderingContexts.
class WebGLSharedObject : public WebGLObject {
public:
    virtual ~WebGLSharedObject();

    WebGLContextGroup* contextGroup() const { return m_contextGroup; }

    virtual bool isRenderbuffer() const { return false; }
    virtual bool isTexture() const { return false; }

    bool validate(const WebGLContextGroup* contextGroup, const WebGLRenderingContextBase&) const override
    {
        return contextGroup == m_contextGroup;
    }

    void detachContextGroup();

protected:
    WebGLSharedObject(WebGLRenderingContextBase&);

    bool hasGroupOrContext() const override
    {
        return m_contextGroup;
    }

    GraphicsContextGLOpenGL* getAGraphicsContextGL() const override;

private:
    WebGLContextGroup* m_contextGroup;
};

} // namespace WebCore

#endif
