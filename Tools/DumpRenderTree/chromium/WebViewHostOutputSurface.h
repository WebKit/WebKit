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

#ifndef WebViewHostOutputSurface_h
#define WebViewHostOutputSurface_h

#include <public/WebCompositorOutputSurface.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebKit {

class WebCompositorOutputSurfaceClient;
class WebCompositorSoftwareOutputDevice;
class WebGraphicsContext3D;

class WebViewHostOutputSurface : public WebKit::WebCompositorOutputSurface {
public:
    static PassOwnPtr<WebViewHostOutputSurface> create3d(PassOwnPtr<WebKit::WebGraphicsContext3D>);
    static PassOwnPtr<WebViewHostOutputSurface> createSoftware(PassOwnPtr<WebKit::WebCompositorSoftwareOutputDevice>);
    virtual ~WebViewHostOutputSurface();

    virtual bool bindToClient(WebCompositorOutputSurfaceClient*) OVERRIDE;

    virtual const WebKit::WebCompositorOutputSurface::Capabilities& capabilities() const OVERRIDE;
    virtual WebGraphicsContext3D* context3D() const OVERRIDE;
    virtual WebCompositorSoftwareOutputDevice* softwareDevice() const OVERRIDE;
    virtual void sendFrameToParentCompositor(const WebCompositorFrame&) OVERRIDE;

private:
    explicit WebViewHostOutputSurface(PassOwnPtr<WebKit::WebGraphicsContext3D>);
    explicit WebViewHostOutputSurface(PassOwnPtr<WebKit::WebCompositorSoftwareOutputDevice>);

    WebKit::WebCompositorOutputSurface::Capabilities m_capabilities;
    OwnPtr<WebKit::WebGraphicsContext3D> m_context;
    OwnPtr<WebKit::WebCompositorSoftwareOutputDevice> m_softwareDevice;
};

}

#endif // WebViewHostOutputSurface_h
