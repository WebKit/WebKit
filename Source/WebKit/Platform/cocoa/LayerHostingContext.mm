/*
 * Copyright (C) 2012-2025 Apple Inc. All rights reserved.
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

#import "LayerTreeContext.h"
#import "Logging.h"
#import <WebCore/WebCoreCALayerExtras.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/FixedVector.h>
#import <wtf/MachSendRight.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cocoa/SpanCocoa.h>

#if USE(EXTENSIONKIT)
#import "ExtensionKitSPI.h"
#import <BrowserEngineKit/BELayerHierarchy.h>
#import <BrowserEngineKit/BELayerHierarchyHandle.h>
#import <BrowserEngineKit/BELayerHierarchyHostingTransactionCoordinator.h>
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LayerHostingContext);

std::unique_ptr<LayerHostingContext> LayerHostingContext::create(const LayerHostingContextOptions& options)
{
    auto layerHostingContext = makeUnique<LayerHostingContext>();

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    // Use a very large display ID to ensure that the context is never put on-screen
    // without being explicitly parented. See <rdar://problem/16089267> for details.
    auto contextOptions = @{
        kCAContextSecure: @(options.canShowWhileLocked),
#if HAVE(CORE_ANIMATION_RENDER_SERVER)
        kCAContextIgnoresHitTest : @YES,
        kCAContextDisplayId : @10000
#endif
    };
#if USE(EXTENSIONKIT)
    if (options.useHostable) {
        layerHostingContext->m_hostable = [BELayerHierarchy layerHierarchyWithOptions:contextOptions error:nil];
        return layerHostingContext;
    }
#endif
    layerHostingContext->m_context = [CAContext remoteContextWithOptions:contextOptions];
#elif !PLATFORM(MACCATALYST)
    [CAContext setAllowsCGSConnections:NO];
    layerHostingContext->m_context = [CAContext remoteContextWithOptions:@{
        kCAContextCIFilterBehavior :  @"ignore",
    }];
#else
    layerHostingContext->m_context = [CAContext contextWithCGSConnection:CGSMainConnectionID() options:@{
        kCAContextCIFilterBehavior : @"ignore",
    }];
#endif
    layerHostingContext->m_cachedContextID = layerHostingContext->contextID();
    return layerHostingContext;
}

std::unique_ptr<LayerHostingContext> LayerHostingContext::createTransportLayerForRemoteHosting(LayerHostingContextID contextID)
{
    auto layerHostingContext = makeUnique<LayerHostingContext>();
    layerHostingContext->m_cachedContextID = contextID;
    return layerHostingContext;
}

RetainPtr<CALayer> LayerHostingContext::createPlatformLayerForHostingContext(LayerHostingContextID contextID)
{
    return [CALayer _web_renderLayerWithContextID:contextID shouldPreserveFlip:NO];
}

LayerHostingContext::LayerHostingContext()
{
}

LayerHostingContext::~LayerHostingContext()
{
#if USE(EXTENSIONKIT)
    [m_hostable invalidate];
#endif
}

void LayerHostingContext::setRootLayer(CALayer *rootLayer)
{
#if USE(EXTENSIONKIT)
    if (m_hostable) {
        [m_hostable setLayer:rootLayer];
        return;
    }
#endif
    [m_context setLayer:rootLayer];
}

CALayer *LayerHostingContext::rootLayer() const
{
#if USE(EXTENSIONKIT)
    if (m_hostable)
        return [m_hostable layer];
#endif
    return [m_context layer];
}

RetainPtr<CALayer> LayerHostingContext::protectedRootLayer() const
{
    return rootLayer();
}

LayerHostingContextID LayerHostingContext::contextID() const
{
#if USE(EXTENSIONKIT)
#if ENABLE(MACH_PORT_LAYER_HOSTING)
    // When layer hosting with Mach ports is enabled, we do not have access to the actual CA context ID.
    // In this case we generate an ID. This is ok, since it is only used as an identifier in the WebContent process.
    static LayerHostingContextID contextID = 0;
    return ++contextID;
#else
    if (auto xpcDictionary = xpcRepresentation())
        return xpc_dictionary_get_uint64(xpcDictionary.get(), contextIDKey);
#endif
#endif
    return [m_context contextId];
}

void LayerHostingContext::invalidate()
{
    [m_context invalidate];
}

void LayerHostingContext::setColorSpace(CGColorSpaceRef colorSpace)
{
    [m_context setColorSpace:colorSpace];
}

CGColorSpaceRef LayerHostingContext::colorSpace() const
{
    return [m_context colorSpace];
}

void LayerHostingContext::setFencePort(mach_port_t fencePort)
{
#if USE(EXTENSIONKIT)
    ASSERT(!m_hostable);
#endif
    [m_context setFencePort:fencePort];
}

MachSendRight LayerHostingContext::createFencePort()
{
    return MachSendRight::adopt([m_context createFencePort]);
}

LayerHostingContextID LayerHostingContext::cachedContextID()
{
    return m_cachedContextID;
}

#if USE(EXTENSIONKIT)
#if ENABLE(MACH_PORT_LAYER_HOSTING)
WTF::MachSendRightAnnotated LayerHostingContext::sendRightAnnotated() const
{
    __block MachSendRight sendRight;
    __block RetainPtr<NSData> dataRepresentation;
    [[m_hostable handle] encodeWithBlock:^(mach_port_t copiedPort, NSData * _Nonnull data) {
        sendRight = MachSendRight::adopt(copiedPort);
        dataRepresentation = data;
    }];
    return { WTFMove(sendRight), FixedVector<uint8_t> { span(dataRepresentation.get()) } };
}

RetainPtr<BELayerHierarchyHostingTransactionCoordinator> LayerHostingContext::createHostingUpdateCoordinator(WTF::MachSendRightAnnotated&& sendRightAnnotated)
{
    // We are leaking the send right here, since [BELayerHierarchyHostingTransactionCoordinator coordinatorWithPort] takes ownership of the send right, even in the error case.
    NSError *error = nil;
    auto coordinator = [BELayerHierarchyHostingTransactionCoordinator coordinatorWithPort:sendRightAnnotated.sendRight.leakSendRight() data:toNSData(sendRightAnnotated.data.span()).get() error:&error];
    if (error)
        RELEASE_LOG_ERROR(Process, "Could not create update coordinator, error = %@", error);
    return coordinator;
}

WTF::MachSendRightAnnotated LayerHostingContext::fence(BELayerHierarchyHostingTransactionCoordinator *coordinator)
{
    __block MachSendRight sendRight;
    __block RetainPtr<NSData> dataRepresentation;
    [coordinator encodeWithBlock:^(mach_port_t copiedPort, NSData * _Nonnull data) {
        sendRight = MachSendRight::adopt(copiedPort);
        dataRepresentation = data;
    }];
    return { WTFMove(sendRight), FixedVector<uint8_t> { span(dataRepresentation.get()) } };
}

RetainPtr<BELayerHierarchyHandle> LayerHostingContext::createHostingHandle(WTF::MachSendRightAnnotated&& sendRightAnnotated)
{
    // We are leaking the send right here, since [BELayerHierarchyHandle handleWithPort] takes ownership of the send right, even in the error case.
    NSError *error = nil;
    auto handle = [BELayerHierarchyHandle handleWithPort:sendRightAnnotated.sendRight.leakSendRight() data:toNSData(sendRightAnnotated.data.span()).get() error:&error];
    if (error)
        RELEASE_LOG_ERROR(Process, "Could not create layer hierarchy handle, error = %@", error);
    return handle;
}
#else
OSObjectPtr<xpc_object_t> LayerHostingContext::xpcRepresentation() const
{
    if (!m_hostable)
        return nullptr;
    return [[m_hostable handle] createXPCRepresentation];
}

RetainPtr<BELayerHierarchyHostingTransactionCoordinator> LayerHostingContext::createHostingUpdateCoordinator(mach_port_t sendRight)
{
    auto xpcRepresentation = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_dictionary_set_mach_send(xpcRepresentation.get(), machPortKey, sendRight);
    NSError* error = nil;
    auto coordinator = [BELayerHierarchyHostingTransactionCoordinator coordinatorWithXPCRepresentation:xpcRepresentation.get() error:&error];
    if (error)
        RELEASE_LOG_ERROR(Process, "Could not create update coordinator, error = %@", error);
    return coordinator;
}

RetainPtr<BELayerHierarchyHandle> LayerHostingContext::createHostingHandle(uint64_t pid, uint64_t contextID)
{
    auto xpcRepresentation = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_dictionary_set_uint64(xpcRepresentation.get(), processIDKey, pid);
    xpc_dictionary_set_uint64(xpcRepresentation.get(), contextIDKey, contextID);
    NSError* error = nil;
    auto handle = [BELayerHierarchyHandle handleWithXPCRepresentation:xpcRepresentation.get() error:&error];
    if (error)
        RELEASE_LOG_ERROR(Process, "Could not create layer hierarchy handle, error = %@", error);
    return handle;
}
#endif // ENABLE(MACH_PORT_LAYER_HOSTING)
#endif // USE(EXTENSIONKIT)

WebCore::HostingContext LayerHostingContext::hostingContext() const
{
    WebCore::HostingContext context;
    context.contextID = contextID();
#if ENABLE(MACH_PORT_LAYER_HOSTING)
    context.sendRightAnnotated = sendRightAnnotated();
#endif
    return context;
}

} // namespace WebKit
