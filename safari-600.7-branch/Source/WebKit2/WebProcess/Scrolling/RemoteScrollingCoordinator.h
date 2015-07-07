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

#ifndef RemoteScrollingCoordinator_h
#define RemoteScrollingCoordinator_h

#if ENABLE(ASYNC_SCROLLING)

#include "MessageReceiver.h"
#include <WebCore/AsyncScrollingCoordinator.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/ScrollingConstraints.h>
#include <WebCore/Timer.h>
#include <wtf/HashMap.h>

namespace IPC {
class ArgumentDecoder;
class ArgumentEncoder;
}

namespace WebKit {

class WebPage;
class RemoteScrollingCoordinatorTransaction;

class RemoteScrollingCoordinator : public WebCore::AsyncScrollingCoordinator, public IPC::MessageReceiver {
public:
    static PassRefPtr<RemoteScrollingCoordinator> create(WebPage* page)
    {
        return adoptRef(new RemoteScrollingCoordinator(page));
    }

    void buildTransaction(RemoteScrollingCoordinatorTransaction&);

private:
    RemoteScrollingCoordinator(WebPage*);
    virtual ~RemoteScrollingCoordinator();

    virtual bool isRemoteScrollingCoordinator() const override { return true; }
    
    // ScrollingCoordinator
    virtual bool coordinatesScrollingForFrameView(WebCore::FrameView*) const override;
    virtual void scheduleTreeStateCommit() override;

    virtual bool isRubberBandInProgress() const override;
    virtual void setScrollPinningBehavior(WebCore::ScrollPinningBehavior) override;

    // IPC::MessageReceiver
    virtual void didReceiveMessage(IPC::Connection*, IPC::MessageDecoder&) override;
    
    // Respond to UI process changes.
    void scrollPositionChangedForNode(WebCore::ScrollingNodeID, const WebCore::FloatPoint& scrollPosition, bool syncLayerPosition);

    WebPage* m_webPage;
};

SCROLLING_COORDINATOR_TYPE_CASTS(RemoteScrollingCoordinator, isRemoteScrollingCoordinator());

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING)

#endif // RemoteScrollingCoordinator_h
