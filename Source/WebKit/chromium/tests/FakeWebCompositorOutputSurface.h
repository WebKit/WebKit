/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FakeWebCompositorOutputSurface_h
#define FakeWebCompositorOutputSurface_h

#include <public/WebCompositorOutputSurface.h>
#include <public/WebGraphicsContext3D.h>

namespace WebKit {

class FakeWebCompositorOutputSurface : public WebCompositorOutputSurface {
public:
    static inline PassOwnPtr<FakeWebCompositorOutputSurface> create(PassOwnPtr<WebGraphicsContext3D> context3D)
    {
        return adoptPtr(new FakeWebCompositorOutputSurface(context3D));
    }


    virtual bool bindToClient(WebCompositorOutputSurfaceClient* client) OVERRIDE
    {
        ASSERT(client);
        if (!m_context3D->makeContextCurrent())
            return false;
        m_client = client;
        return true;
    }

    virtual const Capabilities& capabilities() const OVERRIDE
    {
        return m_capabilities;
    }

    virtual WebGraphicsContext3D* context3D() const OVERRIDE
    {
        return m_context3D.get();
    }

    virtual void sendFrameToParentCompositor(const WebCompositorFrame&) OVERRIDE
    {
    }

private:
    explicit FakeWebCompositorOutputSurface(PassOwnPtr<WebGraphicsContext3D> context3D)
    {
        m_context3D = context3D;
    }

    OwnPtr<WebGraphicsContext3D> m_context3D;
    Capabilities m_capabilities;
    WebCompositorOutputSurfaceClient* m_client;
};

} // namespace WebKit

#endif // FakeWebCompositorOutputSurface_h
