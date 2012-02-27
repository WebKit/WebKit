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
#import "RemoteLayerClient.h"

#import <wtf/PassOwnPtr.h>

#if !defined(BUILDING_ON_SNOW_LEOPARD)
#import <QuartzCore/CARemoteLayerClient.h>
#else
#import <WebKitSystemInterface.h>
#endif

namespace WebKit {

PassOwnPtr<RemoteLayerClient> RemoteLayerClient::create(mach_port_t serverPort, CALayer *rootLayer)
{
    return adoptPtr(new RemoteLayerClient(serverPort, rootLayer));
}

RemoteLayerClient::RemoteLayerClient(mach_port_t serverPort, CALayer *rootLayer)
{
#if !defined(BUILDING_ON_SNOW_LEOPARD)
    m_platformClient = adoptNS([[CARemoteLayerClient alloc] initWithServerPort:serverPort]);
    m_platformClient.get().layer = rootLayer;
#else
    m_platformClient = WKCARemoteLayerClientMakeWithServerPort(serverPort);
    WKCARemoteLayerClientSetLayer(m_platformClient.get(), rootLayer);
#endif
}

RemoteLayerClient::~RemoteLayerClient()
{
}

uint32_t RemoteLayerClient::clientID() const
{
#if !defined(BUILDING_ON_SNOW_LEOPARD)
    return m_platformClient.get().clientId;
#else
    return WKCARemoteLayerClientGetClientId(m_platformClient.get());
#endif
}

void RemoteLayerClient::invalidate()
{
#if !defined(BUILDING_ON_SNOW_LEOPARD)
    [m_platformClient.get() invalidate];
#else
    WKCARemoteLayerClientInvalidate(m_platformClient.get());
#endif
}

} // namespace WebKit
