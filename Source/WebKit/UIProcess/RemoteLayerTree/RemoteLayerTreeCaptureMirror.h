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

#pragma once

#include "RemoteLayerTreeHost.h"
#include "TransactionID.h"
#include <WebCore/ProcessIdentifier.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>

OBJC_CLASS WKCaptureMirrorContainerLayer;

namespace IPC {
class Connection;
}

namespace WebKit {

class RemoteLayerTreeDrawingAreaProxy;
class RemoteLayerTreeTransaction;
struct MainFrameData;
struct RemoteLayerTreeMirrorSeed;

// A copy of the layer tree of a web view for tab capture, fed by the same transactions as the primary tree.
// Backing stores and CA animations can't be replayed, so they are copied from the primary tree instead.
class RemoteLayerTreeCaptureMirror {
    WTF_MAKE_TZONE_ALLOCATED(RemoteLayerTreeCaptureMirror);
    WTF_MAKE_NONCOPYABLE(RemoteLayerTreeCaptureMirror);
public:
    explicit RemoteLayerTreeCaptureMirror(RemoteLayerTreeDrawingAreaProxy&);
    ~RemoteLayerTreeCaptureMirror();

    const RemoteLayerTreeHost& host() const LIFETIME_BOUND { return m_host; }

    // Contains the mirror root. Embedders may position and transform it, but not resize it.
    CALayer *containerLayer() const;

    // Each web process first sends a seed with its current layers.
    void willRequestSeed(WebCore::ProcessIdentifier);
    void didReceiveSeed(const IPC::Connection&, WebCore::ProcessIdentifier, std::optional<RemoteLayerTreeMirrorSeed>&&, const RemoteLayerTreeHost& primaryHost);
    bool isWaitingForSeeds() const { return !m_processesAwaitingSeed.isEmpty(); }

    // Called once the primary host applied the transaction.
    void didCommitTransactionToPrimaryTree(const IPC::Connection&, const RemoteLayerTreeTransaction&, const std::optional<MainFrameData>&, const TransactionID&, const RemoteLayerTreeHost& primaryHost);
    void didSetLayerContentsInPrimaryTree(WebCore::PlatformLayerIdentifier, const RemoteLayerTreeHost& primaryHost);

    // CA drops the animations of layers committed outside of a context, as the mirror is until its container is parented.
    void copyAnimationsFromPrimaryTree(const RemoteLayerTreeHost& primaryHost);

    // The scrolling tree changes the geometry of primary layers outside of transactions.
    void updateGeometryFromPrimaryTree(const RemoteLayerTreeHost& primaryHost);

    void processDidTerminate(WebCore::ProcessIdentifier);

private:
    bool shouldApplyTransaction(const TransactionID&) const;
    void copyContentsFromPrimaryLayer(WebCore::PlatformLayerIdentifier, const RemoteLayerTreeHost& primaryHost);
    void copyAnimationsFromPrimaryLayer(WebCore::PlatformLayerIdentifier, const RemoteLayerTreeHost& primaryHost);
    void updateContainerLayer(const RemoteLayerTreeHost& primaryHost);

    RemoteLayerTreeHost m_host;
    const RetainPtr<WKCaptureMirrorContainerLayer> m_containerLayer;
    HashSet<WebCore::ProcessIdentifier> m_processesAwaitingSeed;
    HashMap<WebCore::ProcessIdentifier, TransactionID> m_lastTransactionCoveredBySeed;
};

} // namespace WebKit
