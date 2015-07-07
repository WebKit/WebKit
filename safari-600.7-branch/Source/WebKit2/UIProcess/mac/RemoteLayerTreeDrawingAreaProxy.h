/*
 * Copyright (C) 2012-2014 Apple Inc. All rights reserved.
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

#ifndef RemoteLayerTreeDrawingAreaProxy_h
#define RemoteLayerTreeDrawingAreaProxy_h

#include "DrawingAreaProxy.h"
#include "RemoteLayerTreeHost.h"
#include <WebCore/FloatPoint.h>
#include <WebCore/IntPoint.h>
#include <WebCore/IntSize.h>

OBJC_CLASS OneShotDisplayLinkHandler;

namespace WebKit {

class RemoteLayerTreeTransaction;
class RemoteScrollingCoordinatorTransaction;

class RemoteLayerTreeDrawingAreaProxy : public DrawingAreaProxy {
public:
    explicit RemoteLayerTreeDrawingAreaProxy(WebPageProxy*);
    virtual ~RemoteLayerTreeDrawingAreaProxy();

    const RemoteLayerTreeHost& remoteLayerTreeHost() const { return m_remoteLayerTreeHost; }

    void acceleratedAnimationDidStart(uint64_t layerID, const String& key, double startTime);

    uint64_t nextLayerTreeTransactionID() const { return m_pendingLayerTreeTransactionID + 1; }
    uint64_t lastCommittedLayerTreeTransactionID() const { return m_transactionIDForPendingCACommit; }

    void didRefreshDisplay(double timestamp);

private:
    virtual void sizeDidChange() override;
    virtual void deviceScaleFactorDidChange() override;
    virtual void didUpdateGeometry() override;
    
    // For now, all callbacks are called before committing changes, because that's what the only client requires.
    // Once we have other callbacks, it may make sense to have a before-commit/after-commit option.
    virtual void dispatchAfterEnsuringDrawing(std::function<void (CallbackBase::Error)>) override;

    WebCore::FloatRect scaledExposedRect() const;

#if PLATFORM(MAC)
    virtual void setExposedRect(const WebCore::FloatRect&) override;
#endif

    float indicatorScale(WebCore::IntSize contentsSize) const;
    virtual void updateDebugIndicator() override;
    void updateDebugIndicator(WebCore::IntSize contentsSize, bool rootLayerChanged, float scale);
    void updateDebugIndicatorPosition();
    void initializeDebugIndicator();

    virtual void waitForDidUpdateViewState() override;
    virtual void hideContentUntilNextUpdate() override;
    
    WebCore::FloatPoint indicatorLocation() const;

    // IPC::MessageReceiver
    virtual void didReceiveMessage(IPC::Connection*, IPC::MessageDecoder&) override;

    // Message handlers
    void willCommitLayerTree(uint64_t transactionID);
    void commitLayerTree(const RemoteLayerTreeTransaction&, const RemoteScrollingCoordinatorTransaction&);
    
    void sendUpdateGeometry();

    RemoteLayerTreeHost m_remoteLayerTreeHost;
    bool m_isWaitingForDidUpdateGeometry;

    WebCore::IntSize m_lastSentSize;
    WebCore::IntSize m_lastSentLayerPosition;

    std::unique_ptr<RemoteLayerTreeHost> m_debugIndicatorLayerTreeHost;
    RetainPtr<CALayer> m_tileMapHostLayer;
    RetainPtr<CALayer> m_exposedRectIndicatorLayer;

    uint64_t m_pendingLayerTreeTransactionID;
    uint64_t m_lastVisibleTransactionID;
    uint64_t m_transactionIDForPendingCACommit;

    CallbackMap m_callbacks;

    RetainPtr<OneShotDisplayLinkHandler> m_displayLinkHandler;
};

DRAWING_AREA_PROXY_TYPE_CASTS(RemoteLayerTreeDrawingAreaProxy, type() == DrawingAreaTypeRemoteLayerTree);

} // namespace WebKit

#endif // RemoteLayerTreeDrawingAreaProxy_h
