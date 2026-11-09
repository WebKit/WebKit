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
#import "RemoteLayerTreeCaptureMirror.h"

#import "LayerProperties.h"
#import "Logging.h"
#import "RemoteLayerTreeCommitBundle.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteLayerTreeTransaction.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/WebActionDisablingCALayerDelegate.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/TZoneMallocInlines.h>

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
#import <WebCore/DynamicContentScalingTypes.h>
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerTreeCaptureMirror);

RemoteLayerTreeCaptureMirror::RemoteLayerTreeCaptureMirror(RemoteLayerTreeDrawingAreaProxy& drawingArea)
    : m_host(drawingArea, RemoteLayerTreeHost::Role::CaptureMirror)
    , m_containerLayer(adoptNS([[CALayer alloc] init]))
{
    [m_containerLayer setName:@"Capture mirror container"];
    [m_containerLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];
    [m_containerLayer setAnchorPoint:CGPointZero];
}

RemoteLayerTreeCaptureMirror::~RemoteLayerTreeCaptureMirror() = default;

CALayer *RemoteLayerTreeCaptureMirror::containerLayer() const
{
    return m_containerLayer.get();
}

void RemoteLayerTreeCaptureMirror::willRequestSeed(WebCore::ProcessIdentifier processIdentifier)
{
    m_processesAwaitingSeed.add(processIdentifier);
}

void RemoteLayerTreeCaptureMirror::didReceiveSeed(const IPC::Connection& connection, WebCore::ProcessIdentifier processIdentifier, std::optional<RemoteLayerTreeMirrorSeed>&& seed, const RemoteLayerTreeHost& primaryHost)
{
    if (!m_processesAwaitingSeed.remove(processIdentifier) || !seed)
        return;

    if (seed->lastBuiltTransactionID.processIdentifier() != processIdentifier) {
        RELEASE_LOG_ERROR(RemoteLayerTree, "%p RemoteLayerTreeCaptureMirror::didReceiveSeed - ignoring seed with a transaction from another process", this);
        return;
    }

    m_lastTransactionCoveredBySeed.set(processIdentifier, seed->lastBuiltTransactionID);

    for (auto& transaction : seed->transactions) {
        // Like regular commits, only the main frame root comes with main frame data.
        std::optional<MainFrameData> mainFrameData;
        if (seed->hasMainFrame && !transaction.remoteContextHostedIdentifier())
            mainFrameData = MainFrameData { };
        m_host.updateLayerTree(connection, transaction, mainFrameData);
    }

    for (auto& transaction : seed->transactions) {
        for (auto layerID : transaction.changedLayerProperties().keys())
            copyContentsFromPrimaryLayer(layerID, primaryHost);
    }

    updateContainerLayer(primaryHost);
}

bool RemoteLayerTreeCaptureMirror::shouldApplyTransaction(const TransactionID& transactionID) const
{
    auto processIdentifier = transactionID.processIdentifier();
    if (m_processesAwaitingSeed.contains(processIdentifier))
        return false;

    // Processes started after the mirror was created send all their layers in their commits.
    auto lastCoveredTransactionID = m_lastTransactionCoveredBySeed.getOptional(processIdentifier);
    return !lastCoveredTransactionID || lastCoveredTransactionID->object() < transactionID.object();
}

void RemoteLayerTreeCaptureMirror::didCommitTransactionToPrimaryTree(const IPC::Connection& connection, const RemoteLayerTreeTransaction& transaction, const std::optional<MainFrameData>& mainFrameData, const TransactionID& transactionID, const RemoteLayerTreeHost& primaryHost)
{
    bool shouldApply = shouldApplyTransaction(transactionID);
    if (shouldApply)
        m_host.updateLayerTree(connection, transaction, mainFrameData);

    constexpr OptionSet<LayerChange> contentsChanges {
        LayerChange::BackingStoreChanged,
        LayerChange::BackingStoreAttachmentChanged,
        LayerChange::ClonedContentsChanged,
        LayerChange::DisplayOnlyImageChanged,
    };

    for (auto& [layerID, properties] : transaction.changedLayerProperties()) {
        if (properties->changedProperties.containsAny(contentsChanges))
            copyContentsFromPrimaryLayer(layerID, primaryHost);
    }

    if (!shouldApply) {
        for (auto layerID : transaction.layerIDsWithNewlyUnreachableBackingStore())
            copyContentsFromPrimaryLayer(layerID, primaryHost);
    }

    updateContainerLayer(primaryHost);
}

void RemoteLayerTreeCaptureMirror::didSetLayerContentsInPrimaryTree(WebCore::PlatformLayerIdentifier layerID, const RemoteLayerTreeHost& primaryHost)
{
    copyContentsFromPrimaryLayer(layerID, primaryHost);
}

void RemoteLayerTreeCaptureMirror::copyContentsFromPrimaryLayer(WebCore::PlatformLayerIdentifier layerID, const RemoteLayerTreeHost& primaryHost)
{
    RefPtr primaryNode = primaryHost.nodeForID(layerID);
    RefPtr mirrorNode = m_host.nodeForID(layerID);
    if (!primaryNode || !mirrorNode)
        return;

    RetainPtr primaryLayer = primaryNode->layer();
    RetainPtr mirrorLayer = mirrorNode->layer();

    // Contents are IOSurfaces or images, which layers can share.
    RetainPtr<id> contents = [primaryLayer contents];
    [mirrorLayer setContents:contents.get()];
    [mirrorLayer setContentsOpaque:contents && primaryNode->backingStoreIsOpaque()];

#if HAVE(SUPPORT_HDR_DISPLAY_APIS)
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [mirrorLayer setWantsExtendedDynamicRangeContent:[primaryLayer wantsExtendedDynamicRangeContent]];
    [mirrorLayer setContentsHeadroom:[primaryLayer contentsHeadroom]];
    ALLOW_DEPRECATED_DECLARATIONS_END
#endif

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    [mirrorLayer setDrawsAsynchronously:[primaryLayer drawsAsynchronously]];
    // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
    for (SUPPRESS_UNRETAINED_LOCAL NSString *key in @[ WKDynamicContentScalingContentsKey, WKDynamicContentScalingPortsKey, WKDynamicContentScalingEnabledKey, WKDynamicContentScalingBifurcationEnabledKey, WKDynamicContentScalingBifurcationScaleKey ])
        [mirrorLayer setValue:[primaryLayer valueForKeyPath:key] forKeyPath:key];
#endif
}

void RemoteLayerTreeCaptureMirror::updateContainerLayer(const RemoteLayerTreeHost& primaryHost)
{
    if (RetainPtr rootLayer = m_host.rootLayer(); rootLayer && [rootLayer superlayer] != m_containerLayer.get())
        [m_containerLayer setSublayers:@[ rootLayer.get() ]];

    // Use the coordinate space of the primary root layer, so that the mirror root keeps the same geometry.
    RetainPtr primaryRootSuperlayer = [primaryHost.rootLayer() superlayer];
    if (!primaryRootSuperlayer)
        return;

    if (auto bounds = [primaryRootSuperlayer bounds]; !CGRectEqualToRect(bounds, [m_containerLayer bounds]))
        [m_containerLayer setBounds:bounds];
    if (auto isGeometryFlipped = [primaryRootSuperlayer isGeometryFlipped]; isGeometryFlipped != [m_containerLayer isGeometryFlipped])
        [m_containerLayer setGeometryFlipped:isGeometryFlipped];
    if (auto sublayerTransform = [primaryRootSuperlayer sublayerTransform]; !CATransform3DEqualToTransform(sublayerTransform, [m_containerLayer sublayerTransform]))
        [m_containerLayer setSublayerTransform:sublayerTransform];
}

void RemoteLayerTreeCaptureMirror::processDidTerminate(WebCore::ProcessIdentifier processIdentifier)
{
    m_processesAwaitingSeed.remove(processIdentifier);
    m_lastTransactionCoveredBySeed.remove(processIdentifier);
    m_host.remotePageProcessDidTerminate(processIdentifier);
}

} // namespace WebKit
