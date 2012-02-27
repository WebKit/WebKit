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

#ifndef RemoteLayerClient_h
#define RemoteLayerClient_h

#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS CALayer;

#if !defined(BUILDING_ON_SNOW_LEOPARD)
OBJC_CLASS CARemoteLayerClient;
typedef CARemoteLayerClient *PlatformRemoteLayerClient;
#else
typedef struct __WKCARemoteLayerClientRef* WKCARemoteLayerClientRef;
typedef WKCARemoteLayerClientRef PlatformRemoteLayerClient;
#endif

namespace WebKit {

class RemoteLayerClient {
    WTF_MAKE_NONCOPYABLE(RemoteLayerClient);
public:
    static PassOwnPtr<RemoteLayerClient> create(mach_port_t serverPort, CALayer *rootLayer);
    ~RemoteLayerClient();

    uint32_t clientID() const;
    void invalidate();

private:
    RemoteLayerClient(mach_port_t serverPort, CALayer *rootLayer);

    RetainPtr<PlatformRemoteLayerClient> m_platformClient;
};

} // namespace WebKit

#endif // RemoteLayerClient_h
