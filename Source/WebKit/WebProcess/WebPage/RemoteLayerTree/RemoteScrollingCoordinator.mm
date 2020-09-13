/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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
#import "RemoteScrollingCoordinator.h"

#if ENABLE(ASYNC_SCROLLING)

#import "ArgumentCoders.h"
#import "GraphicsLayerCARemote.h"
#import "RemoteLayerTreeDrawingArea.h"
#import "RemoteScrollingCoordinatorMessages.h"
#import "RemoteScrollingCoordinatorTransaction.h"
#import "RemoteScrollingUIState.h"
#import "WebCoreArgumentCoders.h"
#import "WebPage.h"
#import "WebProcess.h"
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsLayer.h>
#import <WebCore/RenderLayerCompositor.h>
#import <WebCore/RenderView.h>
#import <WebCore/ScrollingTreeFixedNode.h>
#import <WebCore/ScrollingTreeStickyNode.h>


namespace WebKit {
using namespace WebCore;

RemoteScrollingCoordinator::RemoteScrollingCoordinator(WebPage* page)
    : AsyncScrollingCoordinator(page->corePage())
    , m_webPage(page)
{
    WebProcess::singleton().addMessageReceiver(Messages::RemoteScrollingCoordinator::messageReceiverName(), m_webPage->identifier(), *this);
}

RemoteScrollingCoordinator::~RemoteScrollingCoordinator()
{
    WebProcess::singleton().removeMessageReceiver(Messages::RemoteScrollingCoordinator::messageReceiverName(), m_webPage->identifier());
}

void RemoteScrollingCoordinator::scheduleTreeStateCommit()
{
    m_webPage->drawingArea()->scheduleRenderingUpdate();
}

bool RemoteScrollingCoordinator::coordinatesScrollingForFrameView(const FrameView& frameView) const
{
    RenderView* renderView = frameView.renderView();
    return renderView && renderView->usesCompositing();
}

bool RemoteScrollingCoordinator::isRubberBandInProgress(ScrollingNodeID nodeID) const
{
    return m_nodesWithActiveRubberBanding.contains(nodeID);
}

bool RemoteScrollingCoordinator::isUserScrollInProgress(ScrollingNodeID nodeID) const
{
    return m_nodesWithActiveUserScrolls.contains(nodeID);
}

bool RemoteScrollingCoordinator::isScrollSnapInProgress(ScrollingNodeID nodeID) const
{
    return m_nodesWithActiveScrollSnap.contains(nodeID);
}

void RemoteScrollingCoordinator::setScrollPinningBehavior(ScrollPinningBehavior)
{
    // FIXME: send to the UI process.
}

void RemoteScrollingCoordinator::buildTransaction(RemoteScrollingCoordinatorTransaction& transaction)
{
    willCommitTree();
    transaction.setStateTreeToEncode(scrollingStateTree()->commit(LayerRepresentation::PlatformLayerIDRepresentation));
}

// Notification from the UI process that we scrolled.
void RemoteScrollingCoordinator::scrollPositionChangedForNode(ScrollingNodeID nodeID, const FloatPoint& scrollPosition, bool syncLayerPosition)
{
    scheduleUpdateScrollPositionAfterAsyncScroll(nodeID, scrollPosition, WTF::nullopt, syncLayerPosition ? ScrollingLayerPositionAction::Sync : ScrollingLayerPositionAction::Set);
}

void RemoteScrollingCoordinator::currentSnapPointIndicesChangedForNode(ScrollingNodeID nodeID, unsigned horizontal, unsigned vertical)
{
    setActiveScrollSnapIndices(nodeID, horizontal, vertical);
}

void RemoteScrollingCoordinator::scrollingStateInUIProcessChanged(const RemoteScrollingUIState& uiState)
{
    // FIXME: Also track m_nodesWithActiveRubberBanding.
    if (uiState.changes().contains(RemoteScrollingUIState::Changes::ScrollSnapNodes))
        m_nodesWithActiveScrollSnap = uiState.nodesWithActiveScrollSnap();

    if (uiState.changes().contains(RemoteScrollingUIState::Changes::UserScrollNodes))
        m_nodesWithActiveUserScrolls = uiState.nodesWithActiveUserScrolls();
}

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING)
