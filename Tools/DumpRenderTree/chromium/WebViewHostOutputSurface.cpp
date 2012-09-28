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

#include "config.h"

#include "WebViewHostOutputSurface.h"

#include <public/WebCompositorSoftwareOutputDevice.h>
#include <public/WebGraphicsContext3D.h>
#include <wtf/Assertions.h>

namespace WebKit {

PassOwnPtr<WebViewHostOutputSurface> WebViewHostOutputSurface::create3d(PassOwnPtr<WebKit::WebGraphicsContext3D> context3d)
{
    return adoptPtr(new WebViewHostOutputSurface(context3d));
}

PassOwnPtr<WebViewHostOutputSurface> WebViewHostOutputSurface::createSoftware(PassOwnPtr<WebKit::WebCompositorSoftwareOutputDevice> softwareDevice)
{
    return adoptPtr(new WebViewHostOutputSurface(softwareDevice));
}

WebViewHostOutputSurface::WebViewHostOutputSurface(PassOwnPtr<WebKit::WebGraphicsContext3D> context)
    : m_context(context)
{
}

WebViewHostOutputSurface::WebViewHostOutputSurface(PassOwnPtr<WebKit::WebCompositorSoftwareOutputDevice> softwareDevice)
    : m_softwareDevice(softwareDevice)
{
}

WebViewHostOutputSurface::~WebViewHostOutputSurface()
{
}

bool WebViewHostOutputSurface::bindToClient(WebCompositorOutputSurfaceClient*)
{
    if (!m_context)
        return true;

    return m_context->makeContextCurrent();
}

const WebKit::WebCompositorOutputSurface::Capabilities& WebViewHostOutputSurface::capabilities() const
{
    return m_capabilities;
}

WebGraphicsContext3D* WebViewHostOutputSurface::context3D() const
{
    return m_context.get();
}

WebCompositorSoftwareOutputDevice* WebViewHostOutputSurface::softwareDevice() const
{
    return m_softwareDevice.get();
}

void WebViewHostOutputSurface::sendFrameToParentCompositor(const WebCompositorFrame&)
{
    ASSERT_NOT_REACHED();
}

}
