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
#include <wtf/HashSet.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>

OBJC_CLASS CALayer;

namespace IPC {
class Connection;
}

namespace WebKit {

class RemoteLayerTreeDrawingAreaProxy;
class RemoteLayerTreeTransaction;
struct MainFrameData;

class RemoteLayerTreeMirror {
    WTF_MAKE_TZONE_ALLOCATED(RemoteLayerTreeMirror);
    WTF_MAKE_NONCOPYABLE(RemoteLayerTreeMirror);
public:
    explicit RemoteLayerTreeMirror(RemoteLayerTreeDrawingAreaProxy&);
    ~RemoteLayerTreeMirror();

    const RemoteLayerTreeHost& host() const LIFETIME_BOUND { return m_host; }

    CALayer *layer() const;

    void willRequestFullState(WebCore::ProcessIdentifier);
    void didReceiveFullState(const IPC::Connection&, WebCore::ProcessIdentifier, const RemoteLayerTreeFullState&, bool hasMainFrame, const RemoteLayerTreeHost& primaryHost);
    bool isWaitingForFullStates() const { return !m_processesAwaitingFullState.isEmpty(); }

    void didCommitTransactionToPrimaryTree(const IPC::Connection&, const RemoteLayerTreeTransaction&, const std::optional<MainFrameData>&, const TransactionID&, const RemoteLayerTreeHost& primaryHost);
    void didSetLayerContentsInPrimaryTree(WebCore::PlatformLayerIdentifier, const RemoteLayerTreeHost& primaryHost);

    void processDidTerminate(WebCore::ProcessIdentifier);
    void processDidLeavePage(WebCore::ProcessIdentifier);

private:
    void copyContentsFromPrimaryLayer(WebCore::PlatformLayerIdentifier, const RemoteLayerTreeHost& primaryHost);
    void updateLayerFrom(const RemoteLayerTreeHost& primaryHost);

    RemoteLayerTreeHost m_host;
    const RetainPtr<CALayer> m_mirrorRootLayer;
    HashSet<WebCore::ProcessIdentifier> m_processesAwaitingFullState;
};

} // namespace WebKit
