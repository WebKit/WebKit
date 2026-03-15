/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RemoteLayerHostingManagerProxy.h"

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)

#import "DrawingAreaProxy.h"
#import "GPUProcessProxy.h"
#import "LayerHostingContext.h"
#import "RemoteLayerHostingManagerMessages.h"
#import "RemoteLayerHostingManagerProxyMessages.h"
#import "WKVideoLayerHost.h"
#import "WKVideoLayerHostView.h"
#import "WebPageProxy.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/FloatRect.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/MachSendRightAnnotated.h>
#import <wtf/RunLoop.h>
#import <wtf/TZoneMallocInlines.h>

#if USE(EXTENSIONKIT)
#import <BrowserEngineKit/BELayerHierarchyHandle.h>
#import <BrowserEngineKit/BELayerHierarchyHostingTransactionCoordinator.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIWindow.h>
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerHostingManagerProxy);

Ref<RemoteLayerHostingManagerProxy> RemoteLayerHostingManagerProxy::create(GPUProcessProxy& gpuProcess)
{
    return adoptRef(*new RemoteLayerHostingManagerProxy(gpuProcess));
}

RemoteLayerHostingManagerProxy::RemoteLayerHostingManagerProxy(GPUProcessProxy& gpuProcess)
    : m_gpuProcess(gpuProcess)
{
    gpuProcess.addMessageReceiver(Messages::RemoteLayerHostingManagerProxy::messageReceiverName(), *this);
}

RemoteLayerHostingManagerProxy::~RemoteLayerHostingManagerProxy()
{
    invalidate();
}

void RemoteLayerHostingManagerProxy::invalidate()
{
    if (RefPtr process = m_gpuProcess.get())
        process->removeMessageReceiver(Messages::RemoteLayerHostingManagerProxy::messageReceiverName());

    m_hostedLayers.clear();
}

RetainPtr<WKVideoLayerHostView> RemoteLayerHostingManagerProxy::ensureLayerHostViewForIdentifier(PlaybackSessionContextIdentifier identifier)
{
    return ensureRemoteLayerHostInfo(identifier).hostView;
}

void RemoteLayerHostingManagerProxy::applyHostingContextToLayerHost(RemoteLayerHostInfo& info)
{
    if (!info.hostView)
        return;

#if USE(EXTENSIONKIT)
    RetainPtr<BELayerHierarchyHandle> layerHandle;
    if (info.hasGPUProcessVideoLayer) {
#if ENABLE(MACH_PORT_LAYER_HOSTING)
        layerHandle = LayerHostingContext::createHostingHandle(WTF::MachSendRightAnnotated { info.hostingContext.sendRightAnnotated });
#else
        if (RefPtr process = m_gpuProcess.get())
            layerHandle = LayerHostingContext::createHostingHandle(process->processID(), info.hostingContext.contextID);
#endif
        if (!layerHandle)
            RELEASE_LOG_ERROR(Media, "RemoteLayerHostingManagerProxy: could not create a layer hierarchy handle for the GPU process video layer");
    }
    [[info.hostView layerHierarchyHostingView] setHandle:layerHandle.get()];
#else
    [[info.hostView layerHost] setContextId:info.hostingContext.contextID];
#endif
}

auto RemoteLayerHostingManagerProxy::ensureRemoteLayerHostInfo(PlaybackSessionContextIdentifier identifier) -> RemoteLayerHostInfo&
{
    auto& info = m_hostedLayers.ensure(identifier, [] {
        return RemoteLayerHostInfo { };
    }).iterator->value;

    if (info.hostView)
        return info;

    RetainPtr view = adoptNS([[WKVideoLayerHostView alloc] initWithFrame:CGRectZero]);
#if PLATFORM(MAC)
    [view setWantsLayer:YES];
#endif
#if PLATFORM(IOS_FAMILY)
    [view setUserInteractionEnabled:NO];
#endif

    RetainPtr layerHost = [view layerHost];

    info.hostView = WTF::move(view);

    [layerHost setIdentifier:identifier];
    [layerHost setParent:this];

    applyHostingContextToLayerHost(info);

    return info;
}

void RemoteLayerHostingManagerProxy::setPageForIdentifier(PlaybackSessionContextIdentifier identifier, WebPageProxy& page)
{
    auto& info = ensureRemoteLayerHostInfo(identifier);
    info.page = page;
    info.hasUIProcessConsumer = true;
}

void RemoteLayerHostingManagerProxy::releaseLayerHostForIdentifier(PlaybackSessionContextIdentifier identifier)
{
    auto it = m_hostedLayers.find(identifier);
    if (it == m_hostedLayers.end())
        return;

    it->value.hasUIProcessConsumer = false;
    removeRemoteLayerHostIfUnused(identifier);
}

void RemoteLayerHostingManagerProxy::removeRemoteLayerHostIfUnused(PlaybackSessionContextIdentifier identifier)
{
    auto it = m_hostedLayers.find(identifier);
    if (it == m_hostedLayers.end())
        return;

    if (it->value.hasGPUProcessVideoLayer || it->value.hasUIProcessConsumer)
        return;

    RetainPtr hostView = it->value.hostView;
    m_hostedLayers.remove(it);
    [hostView removeFromSuperview];
}

void RemoteLayerHostingManagerProxy::setRemoteLayerHostSize(PlaybackSessionContextIdentifier identifier, const WebCore::FloatSize& size)
{
    auto it = m_hostedLayers.find(identifier);
    if (it == m_hostedLayers.end())
        return;

    auto& info = it->value;
    if (info.size == size)
        return;

    info.size = size;

    RefPtr process = m_gpuProcess.get();
    if (!process)
        return;

    runWithFenceForIdentifier(identifier, [&](auto&& fence) {
        process->send(Messages::RemoteLayerHostingManager::SetRemoteLayerSizeFenced(identifier, size, WTF::move(fence)), 0);
    });
}

void RemoteLayerHostingManagerProxy::setRemoteLayerVideoGravity(PlaybackSessionContextIdentifier identifier, const String& videoGravity)
{
    auto it = m_hostedLayers.find(identifier);
    if (it == m_hostedLayers.end())
        return;

    auto& info = it->value;
    if (info.videoGravity == videoGravity)
        return;

    info.videoGravity = videoGravity;

    RefPtr process = m_gpuProcess.get();
    if (!process)
        return;

    runWithFenceForIdentifier(identifier, [&](auto&& fence) {
        process->send(Messages::RemoteLayerHostingManager::SetRemoteLayerVideoGravityFenced(identifier, videoGravity, WTF::move(fence)), 0);
    });
}

void RemoteLayerHostingManagerProxy::countGPUProcessRemoteLayersForTesting(CompletionHandler<void(uint64_t)>&& completionHandler)
{
    RefPtr process = m_gpuProcess.get();
    if (!process) {
        completionHandler(0);
        return;
    }

    process->sendWithAsyncReply(Messages::RemoteLayerHostingManager::CountRemoteLayersForTesting(), WTF::move(completionHandler), 0);
}

String RemoteLayerHostingManagerProxy::gpuProcessRemoteLayerTreeAsTextForTesting()
{
    RefPtr process = m_gpuProcess.get();
    if (!process)
        return { };

    auto sendResult = process->sendSync(Messages::RemoteLayerHostingManager::RemoteLayerTreeAsTextForTesting(), 0);
    if (!sendResult.succeeded())
        return "<no reply from GPU process>"_s;

    auto [description] = sendResult.takeReply();
    return description;
}

void RemoteLayerHostingManagerProxy::didCreateRemoteLayer(PlaybackSessionContextIdentifier identifier, WebCore::HostingContext&& hostingContext)
{
    auto& info = ensureRemoteLayerHostInfo(identifier);
    info.hostingContext = WTF::move(hostingContext);
    info.hasGPUProcessVideoLayer = true;
    applyHostingContextToLayerHost(info);
}

void RemoteLayerHostingManagerProxy::didRemoveRemoteLayer(PlaybackSessionContextIdentifier identifier)
{
    auto it = m_hostedLayers.find(identifier);
    if (it == m_hostedLayers.end())
        return;

    auto& info = it->value;
    info.hasGPUProcessVideoLayer = false;
    info.hostingContext = { };

    applyHostingContextToLayerHost(info);

    removeRemoteLayerHostIfUnused(identifier);
}

void RemoteLayerHostingManagerProxy::runWithFenceForIdentifier(PlaybackSessionContextIdentifier identifier, Function<void(WTF::MachSendRightAnnotated&&)>&& task)
{
    WTF::MachSendRightAnnotated fence;

    auto it = m_hostedLayers.find(identifier);
    if (it == m_hostedLayers.end()) {
        task(WTF::move(fence));
        return;
    }

#if PLATFORM(IOS_FAMILY)
#if USE(EXTENSIONKIT)
    RetainPtr hostingView = [it->value.hostView layerHierarchyHostingView];
    if (hostingView) {
        RetainPtr hostingUpdateCoordinator = [BELayerHierarchyHostingTransactionCoordinator coordinatorWithError:nil];
        [hostingUpdateCoordinator addLayerHierarchyHostingView:hostingView.get()];
#if ENABLE(MACH_PORT_LAYER_HOSTING)
        fence = LayerHostingContext::fence(hostingUpdateCoordinator.get());
#else
        OSObjectPtr<xpc_object_t> xpcRepresentationHostingCoordinator = [hostingUpdateCoordinator createXPCRepresentation];
        fence.sendRight = MachSendRight::adopt(xpc_dictionary_copy_mach_send(xpcRepresentationHostingCoordinator.get(), machPortKey));
#endif
        task(WTF::move(fence));
        [hostingUpdateCoordinator commit];
        return;
    }
#else
    fence.sendRight = MachSendRight::adopt([UIWindow _synchronizeDrawingAcrossProcesses]);
#endif // USE(EXTENSIONKIT)
#else
    if (RefPtr page = it->value.page) {
        if (RefPtr drawingArea = page->drawingArea())
            fence.sendRight = drawingArea->createFence();
    }
#endif
    task(WTF::move(fence));
}

#if USE(EXTENSIONKIT)
void RemoteLayerHostingManagerProxy::addVisibilityPropogationView(PlaybackSessionContextIdentifier identifier)
{
    auto it = m_hostedLayers.find(identifier);
    if (it == m_hostedLayers.end())
        return;
    RetainPtr hostView = it->value.hostView;
    RefPtr page = it->value.page;

    if (!hostView || !page)
        return;

    RefPtr pageClient = page->pageClient();
    if (!pageClient)
        return;

    if (RetainPtr oldVisibilityPropagationView = [hostView visibilityPropagationView])
        pageClient->removeVisibilityPropagationView(oldVisibilityPropagationView.get());

    if (RetainPtr newVisibilityPropagationView = pageClient->createVisibilityPropagationView())
        [hostView setVisibilityPropagationView:newVisibilityPropagationView];
}

void RemoteLayerHostingManagerProxy::removeVisibilityPropogationView(PlaybackSessionContextIdentifier identifier)
{
    auto it = m_hostedLayers.find(identifier);
    if (it == m_hostedLayers.end())
        return;
    RetainPtr hostView = it->value.hostView;
    RefPtr page = it->value.page;

    if (!hostView || !page)
        return;

    RefPtr pageClient = page->pageClient();
    if (!pageClient)
        return;

    if (RetainPtr oldVisibilityPropagationView = [hostView visibilityPropagationView])
        pageClient->removeVisibilityPropagationView(oldVisibilityPropagationView.get());
    [hostView setVisibilityPropagationView:nil];
}
#endif


} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
