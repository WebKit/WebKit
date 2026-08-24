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

#import "config.h"
#import "RemoteLayerHostingManager.h"

#if ENABLE(GPU_PROCESS)

#import "GPUProcess.h"
#import "LayerHostingContext.h"
#import "RemoteLayerHostingManagerMessages.h"
#import "RemoteLayerHostingManagerProxyMessages.h"
#import <WebCore/FloatRect.h>
#import <objc/runtime.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <ranges>
#import <wtf/MachSendRightAnnotated.h>
#import <wtf/RunLoop.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/StringBuilder.h>

#if USE(EXTENSIONKIT)
#import <BrowserEngineKit/BELayerHierarchyHostingTransactionCoordinator.h>
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerHostingManager);

Ref<RemoteLayerHostingManager> RemoteLayerHostingManager::create(GPUProcess& gpuProcess)
{
    return adoptRef(*new RemoteLayerHostingManager(gpuProcess));
}

RemoteLayerHostingManager::RemoteLayerHostingManager(GPUProcess& gpuProcess)
    : m_gpuProcess(&gpuProcess)
{
    gpuProcess.addMessageReceiver(Messages::RemoteLayerHostingManager::messageReceiverName(), *this);
}

RemoteLayerHostingManager::~RemoteLayerHostingManager()
{
    invalidate();
}

void RemoteLayerHostingManager::invalidate()
{
    if (RefPtr process = m_gpuProcess.get())
        process->removeMessageReceiver(Messages::RemoteLayerHostingManager::messageReceiverName());
    m_remoteLayers.clear();
}

void RemoteLayerHostingManager::createRemoteLayerForPlayer(PlaybackSessionContextIdentifier identifier, RetainPtr<CALayer>&& platformLayer, bool canShowWhileLocked)
{
    auto it = m_remoteLayers.find(identifier);
    if (it != m_remoteLayers.end()) {
        // Already exists — just update the content layer.
        setContentLayerForRemoteLayer(identifier, WTF::move(platformLayer));
        return;
    }

    RemoteLayerInfo info;

    LayerHostingContextOptions contextOptions;
#if USE(EXTENSIONKIT)
    // Without this the context has no BELayerHierarchy, so it cannot be hosted by, or
    // fenced against, a BELayerHierarchyHostingView in the UI process.
    contextOptions.useHostable = true;
#endif
#if PLATFORM(IOS_FAMILY)
    contextOptions.canShowWhileLocked = canShowWhileLocked;
#else
    UNUSED_PARAM(canShowWhileLocked);
#endif
    info.hostingContext = LayerHostingContext::create(contextOptions);

    info.hostingContext->setRootLayer(platformLayer.get());

    info.size = WebCore::FloatSize([platformLayer bounds].size);
    info.contentLayer = WTF::move(platformLayer);

    auto hostingContext = info.hostingContext->hostingContext();

    m_remoteLayers.add(identifier, WTF::move(info));

    if (RefPtr process = m_gpuProcess.get()) {
        if (auto* connection = process->parentProcessConnection())
            connection->send(Messages::RemoteLayerHostingManagerProxy::DidCreateRemoteLayer(identifier, WTF::move(hostingContext)), 0);
    }
}

void RemoteLayerHostingManager::setContentLayerForRemoteLayer(PlaybackSessionContextIdentifier identifier, RetainPtr<CALayer>&& contentLayer)
{
    auto it = m_remoteLayers.find(identifier);
    if (it == m_remoteLayers.end())
        return;

    auto& info = it->value;

    if (info.contentLayer)
        [info.contentLayer removeFromSuperlayer];

    info.contentLayer = WTF::move(contentLayer);
    info.hostingContext->setRootLayer(info.contentLayer.get());
    [info.contentLayer setFrame:CGRectMake(0, 0, info.size.width(), info.size.height())];
}

void RemoteLayerHostingManager::setRemoteLayerSizeFenced(PlaybackSessionContextIdentifier identifier, const WebCore::FloatSize& size, WTF::MachSendRightAnnotated&& fence)
{
    auto it = m_remoteLayers.find(identifier);
    if (it == m_remoteLayers.end())
        return;

    auto& info = it->value;
    if (info.size == size)
        return;

    if (!info.hostingContext)
        return;

#if USE(EXTENSIONKIT)
    RetainPtr<BELayerHierarchyHostingTransactionCoordinator> hostingUpdateCoordinator;
#if ENABLE(MACH_PORT_LAYER_HOSTING)
    auto fenceCopy = fence;
    hostingUpdateCoordinator = LayerHostingContext::createHostingUpdateCoordinator(WTF::move(fenceCopy));
#else
    hostingUpdateCoordinator = LayerHostingContext::createHostingUpdateCoordinator(fence.sendRight.sendRight());
#endif // ENABLE(MACH_PORT_LAYER_HOSTING)
    [hostingUpdateCoordinator addLayerHierarchy:info.hostingContext->hostable().get()];
#else
    info.hostingContext->setFencePort(fence.sendRight.sendRight());
#endif // USE(EXTENSIONKIT)

    info.size = size;

    if (info.contentLayer) {
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        [info.contentLayer setFrame:CGRectMake(0, 0, size.width(), size.height())];
        [CATransaction commit];
    }

#if USE(EXTENSIONKIT)
    [hostingUpdateCoordinator commit];
#endif
}

void RemoteLayerHostingManager::setRemoteLayerVideoGravityFenced(PlaybackSessionContextIdentifier identifier, const String& videoGravity, WTF::MachSendRightAnnotated&& fence)
{
    auto it = m_remoteLayers.find(identifier);
    if (it == m_remoteLayers.end())
        return;

    auto& info = it->value;
    if (info.videoGravity == videoGravity)
        return;

    if (!info.hostingContext)
        return;

#if USE(EXTENSIONKIT)
    RetainPtr<BELayerHierarchyHostingTransactionCoordinator> hostingUpdateCoordinator;
#if ENABLE(MACH_PORT_LAYER_HOSTING)
    auto fenceCopy = fence;
    hostingUpdateCoordinator = LayerHostingContext::createHostingUpdateCoordinator(WTF::move(fenceCopy));
#else
    hostingUpdateCoordinator = LayerHostingContext::createHostingUpdateCoordinator(fence.sendRight.sendRight());
#endif // ENABLE(MACH_PORT_LAYER_HOSTING)
    [hostingUpdateCoordinator addLayerHierarchy:info.hostingContext->hostable().get()];
#else
    info.hostingContext->setFencePort(fence.sendRight.sendRight());
#endif // USE(EXTENSIONKIT)

    info.videoGravity = videoGravity;

    if (info.contentLayer) {
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        if ([info.contentLayer respondsToSelector:@selector(setVideoGravity:)])
            [info.contentLayer performSelector:@selector(setVideoGravity:) withObject:videoGravity.createNSString().get()];
        [CATransaction commit];
    }

#if USE(EXTENSIONKIT)
    [hostingUpdateCoordinator commit];
#endif
}

void RemoteLayerHostingManager::takeAndInvalidateRemoteLayer(PlaybackSessionContextIdentifier identifier)
{
    auto info = m_remoteLayers.take(identifier);

    if (info.hostingContext)
        info.hostingContext->invalidate();

    if (info.contentLayer)
        [info.contentLayer removeFromSuperlayer];
}

// Initiated here, because the player or renderer that owned the video layer has gone away or
// stopped rendering: the UIProcess must be told to drop its layer host.
void RemoteLayerHostingManager::removeRemoteLayerForPlayer(PlaybackSessionContextIdentifier identifier)
{
    if (!m_remoteLayers.contains(identifier))
        return;

    takeAndInvalidateRemoteLayer(identifier);

    if (RefPtr process = m_gpuProcess.get()) {
        if (auto* connection = process->parentProcessConnection())
            connection->send(Messages::RemoteLayerHostingManagerProxy::DidRemoveRemoteLayer(identifier), 0);
    }
}

void RemoteLayerHostingManager::countRemoteLayersForTesting(CompletionHandler<void(uint64_t)>&& completionHandler)
{
    completionHandler(m_remoteLayers.size());
}

static void appendLayerTreeAsText(StringBuilder& builder, CALayer *layer, unsigned depth)
{
    for (unsigned i = 0; i < depth; ++i)
        builder.append("  "_s);
    builder.append(String::fromUTF8(class_getName([layer class])));
    if (NSString *name = [layer name]; name.length)
        builder.append(makeString(" name=\""_s, String { name }, '"'));
    auto bounds = [layer bounds];
    auto position = [layer position];
    builder.append(makeString(" bounds=("_s, bounds.size.width, ", "_s, bounds.size.height, ')'));
    builder.append(makeString(" position=("_s, position.x, ", "_s, position.y, ')'));
    builder.append('\n');

    for (CALayer *sublayer in [layer sublayers])
        appendLayerTreeAsText(builder, sublayer, depth + 1);
}

void RemoteLayerHostingManager::remoteLayerTreeAsTextForTesting(CompletionHandler<void(String&&)>&& completionHandler)
{
    StringBuilder builder;

    // Sorted so that the output is stable across runs; HashMap iteration order is not.
    auto identifiers = copyToVector(m_remoteLayers.keys());
    std::ranges::sort(identifiers, [](auto& a, auto& b) {
        if (a.processIdentifier() != b.processIdentifier())
            return a.processIdentifier().toUInt64() < b.processIdentifier().toUInt64();
        return a.object().toUInt64() < b.object().toUInt64();
    });

    for (auto& identifier : identifiers) {
        auto it = m_remoteLayers.find(identifier);
        if (it == m_remoteLayers.end())
            continue;

        auto& info = it->value;
        builder.append(makeString("remote layer "_s, identifier.toString(),
            " contextID="_s, info.hostingContext ? info.hostingContext->cachedContextID() : 0,
            " size=("_s, info.size.width(), ", "_s, info.size.height(), ")\n"_s));
        if (info.contentLayer)
            appendLayerTreeAsText(builder, info.contentLayer.get(), 1);
        else
            builder.append("  <no content layer>\n"_s);
    }

    String description = builder.toString();
    completionHandler(WTF::move(description));
}

void RemoteLayerHostingManager::removeAllRemoteLayersForProcess(WebCore::ProcessIdentifier processIdentifier)
{
    // Identifiers are process-qualified, so a disconnecting or crashing WebContent process
    // can be swept without depending on each player or renderer being torn down
    // individually. RemoteAudioVideoRendererProxyManager in particular is destroyed without
    // calling shutdown() on its renderers.
    Vector<PlaybackSessionContextIdentifier> identifiersToRemove;
    for (auto& identifier : m_remoteLayers.keys()) {
        if (identifier.processIdentifier() == processIdentifier)
            identifiersToRemove.append(identifier);
    }

    for (auto& identifier : identifiersToRemove)
        removeRemoteLayerForPlayer(identifier);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
