/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "MessageDecoder.h"
#import "MessageEncoder.h"
#import "RemoteLayerTreeDrawingArea.h"
#import "RemoteScrollingCoordinatorMessages.h"
#import "RemoteScrollingCoordinatorTransaction.h"
#import "WebCoreArgumentCoders.h"
#import "WebPage.h"
#import "WebProcess.h"
#import <WebCore/GraphicsLayer.h>
#import <WebCore/FrameView.h>
#import <WebCore/Frame.h>
#import <WebCore/RenderLayerCompositor.h>
#import <WebCore/RenderView.h>
#import <WebCore/ScrollingTreeFixedNode.h>
#import <WebCore/ScrollingTreeStickyNode.h>

using namespace WebCore;

namespace WebKit {

RemoteScrollingCoordinator::RemoteScrollingCoordinator(WebPage* page)
    : AsyncScrollingCoordinator(page->corePage())
    , m_webPage(page)
{
    WebProcess::shared().addMessageReceiver(Messages::RemoteScrollingCoordinator::messageReceiverName(), m_webPage->pageID(), *this);
}

RemoteScrollingCoordinator::~RemoteScrollingCoordinator()
{
    WebProcess::shared().removeMessageReceiver(Messages::RemoteScrollingCoordinator::messageReceiverName(), m_webPage->pageID());
}

void RemoteScrollingCoordinator::scheduleTreeStateCommit()
{
    m_webPage->drawingArea()->scheduleCompositingLayerFlush();
}

PassOwnPtr<ScrollingTreeNode> RemoteScrollingCoordinator::createScrollingTreeNode(ScrollingNodeType, ScrollingNodeID)
{
    // We never created scrolling nodes in the WebProcess with remote scrolling.
    ASSERT_NOT_REACHED();
    return nullptr;
}

bool RemoteScrollingCoordinator::isRubberBandInProgress() const
{
    // FIXME: need to maintain state in the web process?
    return false;
}

void RemoteScrollingCoordinator::setScrollPinningBehavior(ScrollPinningBehavior)
{
    // FIXME: send to the UI process.
}

void RemoteScrollingCoordinator::buildTransaction(RemoteScrollingCoordinatorTransaction& transaction)
{
    transaction.setStateTreeToEncode(scrollingStateTree()->commit(LayerRepresentation::PlatformLayerIDRepresentation));
}

// Notification from the UI process that we scrolled.
void RemoteScrollingCoordinator::scrollPositionChangedForNode(ScrollingNodeID nodeID, const FloatPoint& scrollPosition)
{
    scheduleUpdateScrollPositionAfterAsyncScroll(nodeID, scrollPosition, false /* FIXME */, SyncScrollingLayerPosition);
}

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING)
