/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/HostingContext.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>

#if ENABLE(MACH_PORT_LAYER_HOSTING)
#include <wtf/MachSendRightAnnotated.h>
#endif

OBJC_CLASS CALayer;
OBJC_CLASS CAContext;

#if USE(EXTENSIONKIT)
OBJC_CLASS BELayerHierarchy;
OBJC_CLASS BELayerHierarchyHandle;
OBJC_CLASS BELayerHierarchyHostingTransactionCoordinator;
#endif

namespace WTF {
class MachSendRight;
}

namespace WebKit {

#if USE(EXTENSIONKIT)
constexpr auto contextIDKey = "cid";
constexpr auto processIDKey = "pid";
constexpr auto machPortKey = "p";
#endif

using LayerHostingContextID = uint32_t;

struct LayerHostingContextOptions {
#if PLATFORM(IOS_FAMILY)
    bool canShowWhileLocked { false };
#endif
#if USE(EXTENSIONKIT)
    bool useHostable { false };
#endif
};

class LayerHostingContext {
    WTF_MAKE_TZONE_ALLOCATED(LayerHostingContext);
    WTF_MAKE_NONCOPYABLE(LayerHostingContext);
public:
    static std::unique_ptr<LayerHostingContext> create(const LayerHostingContextOptions& = { });
    
    static std::unique_ptr<LayerHostingContext> createTransportLayerForRemoteHosting(LayerHostingContextID);

    static RetainPtr<CALayer> createPlatformLayerForHostingContext(LayerHostingContextID);

    LayerHostingContext();
    ~LayerHostingContext();

    void setRootLayer(CALayer *);
    CALayer *rootLayer() const;
    RetainPtr<CALayer> protectedRootLayer() const;

    LayerHostingContextID contextID() const;
    void invalidate();

    void setColorSpace(CGColorSpaceRef);
    CGColorSpaceRef colorSpace() const;

    void setFencePort(mach_port_t);

    // createFencePort does not install the fence port on the LayerHostingContext's
    // CAContext; call setFencePort() with the newly created port if synchronization
    // with this context is desired.
    WTF::MachSendRight createFencePort();

    LayerHostingContextID cachedContextID();

#if USE(EXTENSIONKIT)
    RetainPtr<BELayerHierarchy> hostable() const { return m_hostable; }

#if ENABLE(MACH_PORT_LAYER_HOSTING)
    WTF::MachSendRightAnnotated sendRightAnnotated() const;
    static RetainPtr<BELayerHierarchyHandle> createHostingHandle(WTF::MachSendRightAnnotated&&);
    static RetainPtr<BELayerHierarchyHostingTransactionCoordinator> createHostingUpdateCoordinator(WTF::MachSendRightAnnotated&&);
    static WTF::MachSendRightAnnotated fence(BELayerHierarchyHostingTransactionCoordinator *);
#else
    OSObjectPtr<xpc_object_t> xpcRepresentation() const;
    static RetainPtr<BELayerHierarchyHandle> createHostingHandle(uint64_t pid, uint64_t contextID);
    static RetainPtr<BELayerHierarchyHostingTransactionCoordinator> createHostingUpdateCoordinator(mach_port_t sendRight);
#endif // ENABLE(MACH_PORT_LAYER_HOSTING)
#endif // USE(EXTENSIONKIT)

    WebCore::HostingContext hostingContext() const;

private:
    // Denotes the contextID obtained from GPU process, should be returned
    // for all calls to context ID in web process when UI side compositing
    // is enabled. This is done to avoid making calls to CARenderServer from webprocess
    LayerHostingContextID m_cachedContextID;
    RetainPtr<CAContext> m_context;
#if USE(EXTENSIONKIT)
    RetainPtr<BELayerHierarchy> m_hostable;
#endif
};

} // namespace WebKit

