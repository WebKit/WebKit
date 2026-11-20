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
#import "RemoteLayerTreeMirror.h"

#import "LayerProperties.h"
#import "RemoteLayerTreeCommitBundle.h"
#import "RemoteLayerTreeTransaction.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/WebActionDisablingCALayerDelegate.h>
#import <wtf/Borrow.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerTreeMirror);

RemoteLayerTreeMirror::RemoteLayerTreeMirror(RemoteLayerTreeDrawingAreaProxy& drawingArea)
    : m_host(drawingArea, RemoteLayerTreeHost::Role::Mirror)
    , m_mirrorRootLayer(adoptNS([[CALayer alloc] init]))
{
    [m_mirrorRootLayer setName:@"Layer tree mirror container"];
    [m_mirrorRootLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];
    [m_mirrorRootLayer setAnchorPoint:CGPointZero];
    [m_mirrorRootLayer setMasksToBounds:YES];
}

RemoteLayerTreeMirror::~RemoteLayerTreeMirror() = default;

CALayer *RemoteLayerTreeMirror::layer() const
{
    return m_mirrorRootLayer;
}

void RemoteLayerTreeMirror::willRequestFullState(WebCore::ProcessIdentifier processIdentifier)
{
    m_processesAwaitingFullState.add(processIdentifier);
}

void RemoteLayerTreeMirror::didReceiveFullState(const IPC::Connection& connection, WebCore::ProcessIdentifier processIdentifier, const RemoteLayerTreeFullState& fullState, bool hasMainFrame, const RemoteLayerTreeHost& primaryHost)
{
    ASSERT(m_processesAwaitingFullState.contains(processIdentifier));
    if (!m_processesAwaitingFullState.remove(processIdentifier))
        return;

    for (auto& transaction : borrow(fullState)) {
        std::optional<MainFrameData> mainFrameData;
        if (hasMainFrame && !transaction->remoteContextHostedIdentifier())
            mainFrameData = MainFrameData { };
        m_host.updateLayerTree(connection, transaction, mainFrameData);
    }

    for (auto& transaction : borrow(fullState)) {
        for (auto layerID : transaction->changedLayerProperties().keys())
            copyContentsFromPrimaryLayer(layerID, primaryHost);
    }

    if (hasMainFrame) {
        ASSERT(m_host.rootLayer());
        if (RetainPtr rootLayer = m_host.rootLayer())
            [m_mirrorRootLayer setSublayers:@[ rootLayer.get() ]];
    }

    updateLayerFrom(primaryHost);
}

void RemoteLayerTreeMirror::didCommitTransactionToPrimaryTree(const IPC::Connection& connection, const RemoteLayerTreeTransaction& transaction, const std::optional<MainFrameData>& mainFrameData, const TransactionID& transactionID, const RemoteLayerTreeHost& primaryHost)
{
    bool isWaitingForFullState = m_processesAwaitingFullState.contains(transactionID.processIdentifier());
    if (isWaitingForFullState)
        return;

    m_host.updateLayerTree(connection, transaction, mainFrameData);
    ASSERT(!m_host.rootLayer() || [m_host.rootLayer() superlayer] == m_mirrorRootLayer);

    for (auto layerID : transaction.changedLayerProperties().keys())
        copyContentsFromPrimaryLayer(layerID, primaryHost);

    updateLayerFrom(primaryHost);
}

void RemoteLayerTreeMirror::didSetLayerContentsInPrimaryTree(WebCore::PlatformLayerIdentifier layerID, const RemoteLayerTreeHost& primaryHost)
{
    copyContentsFromPrimaryLayer(layerID, primaryHost);
}

void RemoteLayerTreeMirror::copyContentsFromPrimaryLayer(WebCore::PlatformLayerIdentifier layerID, const RemoteLayerTreeHost& primaryHost)
{
    RefPtr primaryNode = primaryHost.nodeForID(layerID);
    RefPtr mirrorNode = m_host.nodeForID(layerID);
    if (primaryNode && mirrorNode)
        mirrorNode->copyContentsFrom(*primaryNode);
}

void RemoteLayerTreeMirror::updateLayerFrom(const RemoteLayerTreeHost& primaryHost)
{
    // Use the coordinate space of the primary root layer, so that the mirror root keeps the same geometry.
    RetainPtr primaryRootSuperlayer = [primaryHost.rootLayer() superlayer];
    if (!primaryRootSuperlayer)
        return;

    auto bounds = [primaryRootSuperlayer bounds];
    if (!CGRectEqualToRect(bounds, [m_mirrorRootLayer bounds]))
        [m_mirrorRootLayer setBounds:bounds];

    // The primary root superlayer may inherit its flipping from an ancestor, like the web view layer on macOS. We flip it relative to its superlayer.
    bool isGeometryFlipped = [primaryRootSuperlayer contentsAreFlipped] != [[m_mirrorRootLayer superlayer] contentsAreFlipped];
    if (isGeometryFlipped != [m_mirrorRootLayer isGeometryFlipped])
        [m_mirrorRootLayer setGeometryFlipped:isGeometryFlipped];

    auto sublayerTransform = [primaryRootSuperlayer sublayerTransform];
    if (!CATransform3DEqualToTransform(sublayerTransform, [m_mirrorRootLayer sublayerTransform]))
        [m_mirrorRootLayer setSublayerTransform:sublayerTransform];
}

void RemoteLayerTreeMirror::processDidTerminate(WebCore::ProcessIdentifier processIdentifier)
{
    m_processesAwaitingFullState.remove(processIdentifier);
    m_host.remotePageProcessDidTerminate(processIdentifier);
}

void RemoteLayerTreeMirror::processDidLeavePage(WebCore::ProcessIdentifier processIdentifier)
{
    m_processesAwaitingFullState.remove(processIdentifier);
}

} // namespace WebKit
