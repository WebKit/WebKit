/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCScheduler_h
#define CCScheduler_h

#include "cc/CCFrameRateController.h"
#include "cc/CCSchedulerStateMachine.h"

#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class CCThread;

struct CCScheduledActionDrawAndSwapResult {
    CCScheduledActionDrawAndSwapResult()
            : didDraw(false)
            , didSwap(false)
    {
    }
    CCScheduledActionDrawAndSwapResult(bool didDraw, bool didSwap)
            : didDraw(didDraw)
            , didSwap(didSwap)
    {
    }
    bool didDraw;
    bool didSwap;
};

class CCSchedulerClient {
public:
    virtual bool canDraw() = 0;
    virtual bool hasMoreResourceUpdates() const = 0;

    virtual void scheduledActionBeginFrame() = 0;
    virtual CCScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapIfPossible() = 0;
    virtual CCScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapForced() = 0;
    virtual void scheduledActionUpdateMoreResources() = 0;
    virtual void scheduledActionCommit() = 0;
    virtual void scheduledActionBeginContextRecreation() = 0;
    virtual void scheduledActionAcquireLayerTexturesForMainThread() = 0;

protected:
    virtual ~CCSchedulerClient() { }
};

class CCScheduler : CCFrameRateControllerClient {
    WTF_MAKE_NONCOPYABLE(CCScheduler);
public:
    static PassOwnPtr<CCScheduler> create(CCSchedulerClient* client, PassOwnPtr<CCFrameRateController> frameRateController)
    {
        return adoptPtr(new CCScheduler(client, frameRateController));
    }

    virtual ~CCScheduler();

    void setCanBeginFrame(bool);

    void setVisible(bool);

    void setNeedsCommit();

    // Like setNeedsCommit(), but ensures a commit will definitely happen even if we are not visible.
    void setNeedsForcedCommit();

    void setNeedsRedraw();

    void setMainThreadNeedsLayerTextures();

    // Like setNeedsRedraw(), but ensures the draw will definitely happen even if we are not visible.
    void setNeedsForcedRedraw();

    void beginFrameComplete();

    void setMaxFramesPending(int);
    void didSwapBuffersComplete();

    void didLoseContext();
    void didRecreateContext();

    bool commitPending() const { return m_stateMachine.commitPending(); }
    bool redrawPending() const { return m_stateMachine.redrawPending(); }

    // CCFrameRateControllerClient implementation
    virtual void vsyncTick() OVERRIDE;

private:
    CCScheduler(CCSchedulerClient*, PassOwnPtr<CCFrameRateController>);

    CCSchedulerStateMachine::Action nextAction();
    void processScheduledActions();

    CCSchedulerClient* m_client;
    OwnPtr<CCFrameRateController> m_frameRateController;
    CCSchedulerStateMachine m_stateMachine;
    bool m_updateMoreResourcesPending;
};

}

#endif // CCScheduler_h
