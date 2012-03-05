/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "LayerHostingContext.h"

#import <wtf/PassOwnPtr.h>
#import <WebKitSystemInterface.h>

namespace WebKit {

PassOwnPtr<LayerHostingContext> LayerHostingContext::createForPort(mach_port_t serverPort)
{
    return adoptPtr(new LayerHostingContext(serverPort));
}

LayerHostingContext::LayerHostingContext(mach_port_t serverPort)
{
    m_layerHostingMode = LayerHostingModeDefault;
    m_context = WKCAContextMakeRemoteWithServerPort(serverPort);
}

#if HAVE(LAYER_HOSTING_IN_WINDOW_SERVER)
PassOwnPtr<LayerHostingContext> LayerHostingContext::createForWindowServer()
{
    return adoptPtr(new LayerHostingContext);
}

LayerHostingContext::LayerHostingContext()
{
    m_layerHostingMode = LayerHostingModeInWindowServer;
    m_context = WKCAContextMakeRemoteForWindowServer();
}
#endif

LayerHostingContext::~LayerHostingContext()
{
}

void LayerHostingContext::setRootLayer(CALayer *rootLayer)
{
    WKCAContextSetLayer(m_context.get(), rootLayer);
}

CALayer *LayerHostingContext::rootLayer() const
{
    return WKCAContextGetLayer(m_context.get());
}

uint32_t LayerHostingContext::contextID() const
{
    return WKCAContextGetContextId(m_context.get());
}

void LayerHostingContext::invalidate()
{
    WKCAContextInvalidate(m_context.get());
}

} // namespace WebKit
