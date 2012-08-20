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

#include "config.h"

#include "CCLayerTreeHost.h"

#include "CCGraphicsContext.h"
#include "CCLayerTreeHostImpl.h"
#include "CCOcclusionTrackerTestCommon.h"
#include "CCSettings.h"
#include "CCTextureUpdateQueue.h"
#include "CCThreadedTest.h"
#include "CCTimingFunction.h"
#include "ContentLayerChromium.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <public/Platform.h>
#include <public/WebThread.h>
#include <wtf/MainThread.h>
#include <wtf/OwnArrayPtr.h>

using namespace WebCore;
using namespace WebKit;
using namespace WebKitTests;
using namespace WTF;

#define EXPECT_EQ_RECT(a, b) \
    EXPECT_EQ(a.x(), b.x()); \
    EXPECT_EQ(a.y(), b.y()); \
    EXPECT_EQ(a.width(), b.width()); \
    EXPECT_EQ(a.height(), b.height());

namespace {

class CCLayerTreeHostTest : public CCThreadedTest { };

// Shortlived layerTreeHosts shouldn't die.
class CCLayerTreeHostTestShortlived1 : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestShortlived1() { }

    virtual void beginTest()
    {
        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.clear();

        endTest();
    }

    virtual void afterTest()
    {
    }
};

// Shortlived layerTreeHosts shouldn't die with a commit in flight.
class CCLayerTreeHostTestShortlived2 : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestShortlived2() { }

    virtual void beginTest()
    {
        postSetNeedsCommitToMainThread();

        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.clear();

        endTest();
    }

    virtual void afterTest()
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestShortlived2)

// Shortlived layerTreeHosts shouldn't die with a redraw in flight.
class CCLayerTreeHostTestShortlived3 : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestShortlived3() { }

    virtual void beginTest()
    {
        postSetNeedsRedrawToMainThread();

        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.clear();

        endTest();
    }

    virtual void afterTest()
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestShortlived3)

// Test interleaving of redraws and commits
class CCLayerTreeHostTestCommitingWithContinuousRedraw : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestCommitingWithContinuousRedraw()
        : m_numCompleteCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest()
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*)
    {
        m_numCompleteCommits++;
        if (m_numCompleteCommits == 2)
            endTest();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl*)
    {
        if (m_numDraws == 1)
          postSetNeedsCommitToMainThread();
        m_numDraws++;
        postSetNeedsRedrawToMainThread();
    }

    virtual void afterTest()
    {
    }

private:
    int m_numCompleteCommits;
    int m_numDraws;
};

TEST_F(CCLayerTreeHostTestCommitingWithContinuousRedraw, runMultiThread)
{
    runTest(true);
}

// Two setNeedsCommits in a row should lead to at least 1 commit and at least 1
// draw with frame 0.
class CCLayerTreeHostTestSetNeedsCommit1 : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestSetNeedsCommit1()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest()
    {
        postSetNeedsCommitToMainThread();
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        m_numDraws++;
        if (!impl->sourceFrameNumber())
            endTest();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*)
    {
        m_numCommits++;
    }

    virtual void afterTest()
    {
        EXPECT_GE(1, m_numCommits);
        EXPECT_GE(1, m_numDraws);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

TEST_F(CCLayerTreeHostTestSetNeedsCommit1, DISABLED_runMultiThread)
{
    runTest(true);
}

// A setNeedsCommit should lead to 1 commit. Issuing a second commit after that
// first committed frame draws should lead to another commit.
class CCLayerTreeHostTestSetNeedsCommit2 : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestSetNeedsCommit2()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest()
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        if (!impl->sourceFrameNumber())
            postSetNeedsCommitToMainThread();
        else if (impl->sourceFrameNumber() == 1)
            endTest();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*)
    {
        m_numCommits++;
    }

    virtual void afterTest()
    {
        EXPECT_EQ(2, m_numCommits);
        EXPECT_GE(2, m_numDraws);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

#if OS(WINDOWS)
// http://webkit.org/b/74623
TEST_F(CCLayerTreeHostTestSetNeedsCommit2, FLAKY_runMultiThread)
#else
TEST_F(CCLayerTreeHostTestSetNeedsCommit2, runMultiThread)
#endif
{
    runTest(true);
}

// 1 setNeedsRedraw after the first commit has completed should lead to 1
// additional draw.
class CCLayerTreeHostTestSetNeedsRedraw : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestSetNeedsRedraw()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest()
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        EXPECT_EQ(0, impl->sourceFrameNumber());
        if (!m_numDraws)
            postSetNeedsRedrawToMainThread(); // Redraw again to verify that the second redraw doesn't commit.
        else
            endTest();
        m_numDraws++;
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*)
    {
        EXPECT_EQ(0, m_numDraws);
        m_numCommits++;
    }

    virtual void afterTest()
    {
        EXPECT_GE(2, m_numDraws);
        EXPECT_EQ(1, m_numCommits);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

TEST_F(CCLayerTreeHostTestSetNeedsRedraw, runMultiThread)
{
    runTest(true);
}

// If the layerTreeHost says it can't draw, then we should not try to draw.
class CCLayerTreeHostTestCanDrawBlocksDrawing : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestCanDrawBlocksDrawing()
        : m_numCommits(0)
    {
    }

    virtual void beginTest()
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        // Only the initial draw should bring us here.
        EXPECT_TRUE(impl->canDraw());
        EXPECT_EQ(0, impl->sourceFrameNumber());
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl)
    {
        if (m_numCommits >= 1) {
            // After the first commit, we should not be able to draw.
            EXPECT_FALSE(impl->canDraw());
        }
    }

    virtual void didCommit()
    {
        m_numCommits++;
        if (m_numCommits == 1) {
            // Make the viewport empty so the host says it can't draw.
            m_layerTreeHost->setViewportSize(IntSize(0, 0), IntSize(0, 0));

            OwnArrayPtr<char> pixels(adoptArrayPtr(new char[4]));
            m_layerTreeHost->compositeAndReadback(static_cast<void*>(pixels.get()), IntRect(0, 0, 1, 1));
        } else if (m_numCommits == 2) {
            m_layerTreeHost->setNeedsRedraw();
            m_layerTreeHost->setNeedsCommit();
        } else
            endTest();
    }

    virtual void afterTest()
    {
    }

private:
    int m_numCommits;
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestCanDrawBlocksDrawing)

// beginLayerWrite should prevent draws from executing until a commit occurs
class CCLayerTreeHostTestWriteLayersRedraw : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestWriteLayersRedraw()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest()
    {
        postAcquireLayerTextures();
        postSetNeedsRedrawToMainThread(); // should be inhibited without blocking
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        m_numDraws++;
        EXPECT_EQ(m_numDraws, m_numCommits);
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*)
    {
        m_numCommits++;
        endTest();
    }

    virtual void afterTest()
    {
        EXPECT_EQ(1, m_numCommits);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

TEST_F(CCLayerTreeHostTestWriteLayersRedraw, runMultiThread)
{
    runTest(true);
}

// Verify that when resuming visibility, requesting layer write permission
// will not deadlock the main thread even though there are not yet any
// scheduled redraws. This behavior is critical for reliably surviving tab
// switching. There are no failure conditions to this test, it just passes
// by not timing out.
class CCLayerTreeHostTestWriteLayersAfterVisible : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestWriteLayersAfterVisible()
        : m_numCommits(0)
    {
    }

    virtual void beginTest()
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*)
    {
        m_numCommits++;
        if (m_numCommits == 2)
            endTest();
        else {
            postSetVisibleToMainThread(false);
            postSetVisibleToMainThread(true);
            postAcquireLayerTextures();
            postSetNeedsCommitToMainThread();
        }
    }

    virtual void afterTest()
    {
    }

private:
    int m_numCommits;
};

TEST_F(CCLayerTreeHostTestWriteLayersAfterVisible, runMultiThread)
{
    runTest(true);
}

// A compositeAndReadback while invisible should force a normal commit without assertion.
class CCLayerTreeHostTestCompositeAndReadbackWhileInvisible : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestCompositeAndReadbackWhileInvisible()
        : m_numCommits(0)
    {
    }

    virtual void beginTest()
    {
    }

    virtual void didCommitAndDrawFrame()
    {
        m_numCommits++;
        if (m_numCommits == 1) {
            m_layerTreeHost->setVisible(false);
            m_layerTreeHost->setNeedsCommit();
            m_layerTreeHost->setNeedsCommit();
            OwnArrayPtr<char> pixels(adoptArrayPtr(new char[4]));
            m_layerTreeHost->compositeAndReadback(static_cast<void*>(pixels.get()), IntRect(0, 0, 1, 1));
        } else
            endTest();

    }

    virtual void afterTest()
    {
    }

private:
    int m_numCommits;
};

TEST_F(CCLayerTreeHostTestCompositeAndReadbackWhileInvisible, runMultiThread)
{
    runTest(true);
}

class CCLayerTreeHostTestAbortFrameWhenInvisible : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestAbortFrameWhenInvisible()
    {
    }

    virtual void beginTest()
    {
        // Request a commit (from the main thread), which will trigger the commit flow from the impl side.
        m_layerTreeHost->setNeedsCommit();
        // Then mark ourselves as not visible before processing any more messages on the main thread.
        m_layerTreeHost->setVisible(false);
        // If we make it without kicking a frame, we pass!
        endTestAfterDelay(1);
    }

    virtual void layout() OVERRIDE
    {
        ASSERT_FALSE(true);
        endTest();
    }

    virtual void afterTest()
    {
    }

private:
};

TEST_F(CCLayerTreeHostTestAbortFrameWhenInvisible, runMultiThread)
{
    runTest(true);
}


// Trigger a frame with setNeedsCommit. Then, inside the resulting animate
// callback, requet another frame using setNeedsAnimate. End the test when
// animate gets called yet-again, indicating that the proxy is correctly
// handling the case where setNeedsAnimate() is called inside the begin frame
// flow.
class CCLayerTreeHostTestSetNeedsAnimateInsideAnimationCallback : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestSetNeedsAnimateInsideAnimationCallback()
        : m_numAnimates(0)
    {
    }

    virtual void beginTest()
    {
        postSetNeedsAnimateToMainThread();
    }

    virtual void updateAnimations(double)
    {
        if (!m_numAnimates) {
            m_layerTreeHost->setNeedsAnimate();
            m_numAnimates++;
            return;
        }
        endTest();
    }

    virtual void afterTest()
    {
    }

private:
    int m_numAnimates;
};

TEST_F(CCLayerTreeHostTestSetNeedsAnimateInsideAnimationCallback, runMultiThread)
{
    runTest(true);
}

// Add a layer animation and confirm that CCLayerTreeHostImpl::animateLayers does get
// called and continues to get called.
class CCLayerTreeHostTestAddAnimation : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestAddAnimation()
        : m_numAnimates(0)
        , m_receivedAnimationStartedNotification(false)
        , m_startTime(0)
        , m_firstMonotonicTime(0)
    {
    }

    virtual void beginTest()
    {
        postAddInstantAnimationToMainThread();
    }

    virtual void animateLayers(CCLayerTreeHostImpl* layerTreeHostImpl, double monotonicTime)
    {
        if (!m_numAnimates) {
            // The animation had zero duration so layerTreeHostImpl should no
            // longer need to animate its layers.
            EXPECT_FALSE(layerTreeHostImpl->needsAnimateLayers());
            m_numAnimates++;
            m_firstMonotonicTime = monotonicTime;
            return;
        }
        EXPECT_LT(0, m_startTime);
        EXPECT_LT(0, m_firstMonotonicTime);
        EXPECT_NE(m_startTime, m_firstMonotonicTime);
        EXPECT_TRUE(m_receivedAnimationStartedNotification);
        endTest();
    }

    virtual void notifyAnimationStarted(double wallClockTime)
    {
        m_receivedAnimationStartedNotification = true;
        m_startTime = wallClockTime;
    }

    virtual void afterTest()
    {
    }

private:
    int m_numAnimates;
    bool m_receivedAnimationStartedNotification;
    double m_startTime;
    double m_firstMonotonicTime;
};

TEST_F(CCLayerTreeHostTestAddAnimation, runMultiThread)
{
    runTest(true);
}

// Add a layer animation to a layer, but continually fail to draw. Confirm that after
// a while, we do eventually force a draw.
class CCLayerTreeHostTestCheckerboardDoesNotStarveDraws : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestCheckerboardDoesNotStarveDraws()
        : m_startedAnimating(false)
    {
    }

    virtual void beginTest()
    {
        postAddAnimationToMainThread();
    }

    virtual void afterTest()
    {
    }

    virtual void animateLayers(CCLayerTreeHostImpl* layerTreeHostImpl, double monotonicTime)
    {
        m_startedAnimating = true;
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl*)
    {
        if (m_startedAnimating)
            endTest();
    }

    virtual bool prepareToDrawOnCCThread(CCLayerTreeHostImpl*)
    {
        return false;
    }

private:
    bool m_startedAnimating;
};

// Starvation can only be an issue with the MT compositor.
TEST_F(CCLayerTreeHostTestCheckerboardDoesNotStarveDraws, runMultiThread)
{
    runTest(true);
}

// Ensures that animations continue to be ticked when we are backgrounded.
class CCLayerTreeHostTestTickAnimationWhileBackgrounded : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestTickAnimationWhileBackgrounded()
        : m_numAnimates(0)
    {
    }

    virtual void beginTest()
    {
        postAddAnimationToMainThread();
    }

    // Use willAnimateLayers to set visible false before the animation runs and
    // causes a commit, so we block the second visible animate in single-thread
    // mode.
    virtual void willAnimateLayers(CCLayerTreeHostImpl* layerTreeHostImpl, double monotonicTime)
    {
        if (m_numAnimates < 2) {
            if (!m_numAnimates) {
                // We have a long animation running. It should continue to tick even if we are not visible.
                postSetVisibleToMainThread(false);
            }
            m_numAnimates++;
            return;
        }
        endTest();
    }

    virtual void afterTest()
    {
    }

private:
    int m_numAnimates;
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestTickAnimationWhileBackgrounded)

// Ensures that animations continue to be ticked when we are backgrounded.
class CCLayerTreeHostTestAddAnimationWithTimingFunction : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestAddAnimationWithTimingFunction()
    {
    }

    virtual void beginTest()
    {
        postAddAnimationToMainThread();
    }

    virtual void animateLayers(CCLayerTreeHostImpl* layerTreeHostImpl, double monotonicTime)
    {
        const CCActiveAnimation* animation = m_layerTreeHost->rootLayer()->layerAnimationController()->getActiveAnimation(0, CCActiveAnimation::Opacity);
        if (!animation)
            return;
        const CCFloatAnimationCurve* curve = animation->curve()->toFloatAnimationCurve();
        float startOpacity = curve->getValue(0);
        float endOpacity = curve->getValue(curve->duration());
        float linearlyInterpolatedOpacity = 0.25 * endOpacity + 0.75 * startOpacity;
        double time = curve->duration() * 0.25;
        // If the linear timing function associated with this animation was not picked up,
        // then the linearly interpolated opacity would be different because of the
        // default ease timing function.
        EXPECT_FLOAT_EQ(linearlyInterpolatedOpacity, curve->getValue(time));
        endTest();
    }

    virtual void afterTest()
    {
    }

private:
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestAddAnimationWithTimingFunction)

// Ensures that when opacity is being animated, this value does not cause the subtree to be skipped.
class CCLayerTreeHostTestDoNotSkipLayersWithAnimatedOpacity : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestDoNotSkipLayersWithAnimatedOpacity()
    {
    }

    virtual void beginTest()
    {
        m_layerTreeHost->rootLayer()->setDrawOpacity(1);
        m_layerTreeHost->setViewportSize(IntSize(10, 10), IntSize(10, 10));
        m_layerTreeHost->rootLayer()->setOpacity(0);
        postAddAnimationToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*)
    {
        // If the subtree was skipped when preparing to draw, the layer's draw opacity
        // will not have been updated. It should be set to 0 due to the animation.
        // Without the animation, the layer will be skipped since it has zero opacity.
        EXPECT_EQ(0, m_layerTreeHost->rootLayer()->drawOpacity());
        endTest();
    }

    virtual void afterTest()
    {
    }
};

#if OS(WINDOWS)
// http://webkit.org/b/74623
TEST_F(CCLayerTreeHostTestDoNotSkipLayersWithAnimatedOpacity, FLAKY_runMultiThread)
#else
TEST_F(CCLayerTreeHostTestDoNotSkipLayersWithAnimatedOpacity, runMultiThread)
#endif
{
    runTest(true);
}

// Ensures that main thread animations have their start times synchronized with impl thread animations.
class CCLayerTreeHostTestSynchronizeAnimationStartTimes : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestSynchronizeAnimationStartTimes()
        : m_layerTreeHostImpl(0)
    {
    }

    virtual void beginTest()
    {
        postAddAnimationToMainThread();
    }

    // This is guaranteed to be called before CCLayerTreeHostImpl::animateLayers.
    virtual void willAnimateLayers(CCLayerTreeHostImpl* layerTreeHostImpl, double monotonicTime)
    {
        m_layerTreeHostImpl = layerTreeHostImpl;
    }

    virtual void notifyAnimationStarted(double time)
    {
        EXPECT_TRUE(m_layerTreeHostImpl);

        CCLayerAnimationController* controllerImpl = m_layerTreeHostImpl->rootLayer()->layerAnimationController();
        CCLayerAnimationController* controller = m_layerTreeHost->rootLayer()->layerAnimationController();
        CCActiveAnimation* animationImpl = controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity);
        CCActiveAnimation* animation = controller->getActiveAnimation(0, CCActiveAnimation::Opacity);

        EXPECT_EQ(animationImpl->startTime(), animation->startTime());

        endTest();
    }

    virtual void afterTest()
    {
    }

private:
    CCLayerTreeHostImpl* m_layerTreeHostImpl;
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestSynchronizeAnimationStartTimes)

// Ensures that main thread animations have their start times synchronized with impl thread animations.
class CCLayerTreeHostTestAnimationFinishedEvents : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestAnimationFinishedEvents()
    {
    }

    virtual void beginTest()
    {
        postAddInstantAnimationToMainThread();
    }

    virtual void notifyAnimationFinished(double time)
    {
        endTest();
    }

    virtual void afterTest()
    {
    }

private:
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestAnimationFinishedEvents)

class CCLayerTreeHostTestScrollSimple : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestScrollSimple()
        : m_initialScroll(IntPoint(10, 20))
        , m_secondScroll(IntPoint(40, 5))
        , m_scrollAmount(2, -1)
        , m_scrolls(0)
    {
    }

    virtual void beginTest()
    {
        m_layerTreeHost->rootLayer()->setScrollable(true);
        m_layerTreeHost->rootLayer()->setScrollPosition(m_initialScroll);
        postSetNeedsCommitToMainThread();
    }

    virtual void layout()
    {
        LayerChromium* root = m_layerTreeHost->rootLayer();
        if (!m_layerTreeHost->commitNumber())
            EXPECT_EQ(root->scrollPosition(), m_initialScroll);
        else {
            EXPECT_EQ(root->scrollPosition(), m_initialScroll + m_scrollAmount);

            // Pretend like Javascript updated the scroll position itself.
            root->setScrollPosition(m_secondScroll);
        }
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        CCLayerImpl* root = impl->rootLayer();
        EXPECT_EQ(root->scrollDelta(), IntSize());

        root->setScrollable(true);
        root->setMaxScrollPosition(IntSize(100, 100));
        root->scrollBy(m_scrollAmount);

        if (!impl->sourceFrameNumber()) {
            EXPECT_EQ(root->scrollPosition(), m_initialScroll);
            EXPECT_EQ(root->scrollDelta(), m_scrollAmount);
            postSetNeedsCommitToMainThread();
        } else if (impl->sourceFrameNumber() == 1) {
            EXPECT_EQ(root->scrollPosition(), m_secondScroll);
            EXPECT_EQ(root->scrollDelta(), m_scrollAmount);
            endTest();
        }
    }

    virtual void applyScrollAndScale(const IntSize& scrollDelta, float scale)
    {
        IntPoint position = m_layerTreeHost->rootLayer()->scrollPosition();
        m_layerTreeHost->rootLayer()->setScrollPosition(position + scrollDelta);
        m_scrolls++;
    }

    virtual void afterTest()
    {
        EXPECT_EQ(1, m_scrolls);
    }
private:
    IntPoint m_initialScroll;
    IntPoint m_secondScroll;
    IntSize m_scrollAmount;
    int m_scrolls;
};

TEST_F(CCLayerTreeHostTestScrollSimple, DISABLED_runMultiThread)
{
    runTest(true);
}

class CCLayerTreeHostTestScrollMultipleRedraw : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestScrollMultipleRedraw()
        : m_initialScroll(IntPoint(40, 10))
        , m_scrollAmount(-3, 17)
        , m_scrolls(0)
    {
    }

    virtual void beginTest()
    {
        m_layerTreeHost->rootLayer()->setScrollable(true);
        m_layerTreeHost->rootLayer()->setScrollPosition(m_initialScroll);
        postSetNeedsCommitToMainThread();
    }

    virtual void beginCommitOnCCThread(CCLayerTreeHostImpl* impl)
    {
        LayerChromium* root = m_layerTreeHost->rootLayer();
        if (!impl->sourceFrameNumber())
            EXPECT_EQ(root->scrollPosition(), m_initialScroll);
        else if (impl->sourceFrameNumber() == 1)
            EXPECT_EQ(root->scrollPosition(), m_initialScroll + m_scrollAmount + m_scrollAmount);
        else if (impl->sourceFrameNumber() == 2)
            EXPECT_EQ(root->scrollPosition(), m_initialScroll + m_scrollAmount + m_scrollAmount);
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        CCLayerImpl* root = impl->rootLayer();
        root->setScrollable(true);
        root->setMaxScrollPosition(IntSize(100, 100));

        if (!impl->sourceFrameNumber() && impl->sourceAnimationFrameNumber() == 1) {
            // First draw after first commit.
            EXPECT_EQ(root->scrollDelta(), IntSize());
            root->scrollBy(m_scrollAmount);
            EXPECT_EQ(root->scrollDelta(), m_scrollAmount);

            EXPECT_EQ(root->scrollPosition(), m_initialScroll);
            postSetNeedsRedrawToMainThread();
        } else if (!impl->sourceFrameNumber() && impl->sourceAnimationFrameNumber() == 2) {
            // Second draw after first commit.
            EXPECT_EQ(root->scrollDelta(), m_scrollAmount);
            root->scrollBy(m_scrollAmount);
            EXPECT_EQ(root->scrollDelta(), m_scrollAmount + m_scrollAmount);

            EXPECT_EQ(root->scrollPosition(), m_initialScroll);
            postSetNeedsCommitToMainThread();
        } else if (impl->sourceFrameNumber() == 1) {
            // Third or later draw after second commit.
            EXPECT_GE(impl->sourceAnimationFrameNumber(), 3);
            EXPECT_EQ(root->scrollDelta(), IntSize());
            EXPECT_EQ(root->scrollPosition(), m_initialScroll + m_scrollAmount + m_scrollAmount);
            endTest();
        }
    }

    virtual void applyScrollAndScale(const IntSize& scrollDelta, float scale)
    {
        IntPoint position = m_layerTreeHost->rootLayer()->scrollPosition();
        m_layerTreeHost->rootLayer()->setScrollPosition(position + scrollDelta);
        m_scrolls++;
    }

    virtual void afterTest()
    {
        EXPECT_EQ(1, m_scrolls);
    }
private:
    IntPoint m_initialScroll;
    IntSize m_scrollAmount;
    int m_scrolls;
};

TEST_F(CCLayerTreeHostTestScrollMultipleRedraw, DISABLED_runMultiThread)
{
    runTest(true);
}

// This test verifies that properties on the layer tree host are commited to the impl side.
class CCLayerTreeHostTestCommit : public CCLayerTreeHostTest {
public:

    CCLayerTreeHostTestCommit() { }

    virtual void beginTest()
    {
        m_layerTreeHost->setViewportSize(IntSize(20, 20), IntSize(20, 20));
        m_layerTreeHost->setBackgroundColor(SK_ColorGRAY);
        m_layerTreeHost->setPageScaleFactorAndLimits(5, 5, 5);

        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl)
    {
        EXPECT_EQ(IntSize(20, 20), impl->layoutViewportSize());
        EXPECT_EQ(SK_ColorGRAY, impl->backgroundColor());
        EXPECT_EQ(5, impl->pageScale());

        endTest();
    }

    virtual void afterTest() { }
};

TEST_F(CCLayerTreeHostTestCommit, runTest)
{
    runTest(true);
}

// Verifies that startPageScaleAnimation events propagate correctly from CCLayerTreeHost to
// CCLayerTreeHostImpl in the MT compositor.
class CCLayerTreeHostTestStartPageScaleAnimation : public CCLayerTreeHostTest {
public:

    CCLayerTreeHostTestStartPageScaleAnimation()
        : m_animationRequested(false)
    {
    }

    virtual void beginTest()
    {
        m_layerTreeHost->rootLayer()->setScrollable(true);
        m_layerTreeHost->rootLayer()->setScrollPosition(IntPoint());
        postSetNeedsRedrawToMainThread();
    }

    static void requestStartPageScaleAnimation(void* self)
    {
        CCLayerTreeHostTestStartPageScaleAnimation* test = static_cast<CCLayerTreeHostTestStartPageScaleAnimation*>(self);
        if (test->layerTreeHost())
            test->layerTreeHost()->startPageScaleAnimation(IntSize(), false, 1.25, 0);
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        impl->rootLayer()->setScrollable(true);
        impl->rootLayer()->setScrollPosition(IntPoint());
        impl->setPageScaleFactorAndLimits(impl->pageScale(), 0.5, 2);

        // We request animation only once.
        if (!m_animationRequested) {
            callOnMainThread(CCLayerTreeHostTestStartPageScaleAnimation::requestStartPageScaleAnimation, this);
            m_animationRequested = true;
        }
    }

    virtual void applyScrollAndScale(const IntSize& scrollDelta, float scale)
    {
        IntPoint position = m_layerTreeHost->rootLayer()->scrollPosition();
        m_layerTreeHost->rootLayer()->setScrollPosition(position + scrollDelta);
        m_layerTreeHost->setPageScaleFactorAndLimits(scale, 0.5, 2);
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl)
    {
        impl->processScrollDeltas();
        // We get one commit before the first draw, and the animation doesn't happen until the second draw.
        if (impl->sourceFrameNumber() == 1) {
            EXPECT_EQ(1.25, impl->pageScale());
            endTest();
        } else
            postSetNeedsRedrawToMainThread();
    }

    virtual void afterTest()
    {
    }

private:
    bool m_animationRequested;
};

TEST_F(CCLayerTreeHostTestStartPageScaleAnimation, runTest)
{
    runTest(true);
}

class CCLayerTreeHostTestSetVisible : public CCLayerTreeHostTest {
public:

    CCLayerTreeHostTestSetVisible()
        : m_numDraws(0)
    {
    }

    virtual void beginTest()
    {
        postSetVisibleToMainThread(false);
        postSetNeedsRedrawToMainThread(); // This is suppressed while we're invisible.
        postSetVisibleToMainThread(true); // Triggers the redraw.
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        EXPECT_TRUE(impl->visible());
        ++m_numDraws;
        endTest();
    }

    virtual void afterTest()
    {
        EXPECT_EQ(1, m_numDraws);
    }

private:
    int m_numDraws;
};

TEST_F(CCLayerTreeHostTestSetVisible, runMultiThread)
{
    runTest(true);
}

class TestOpacityChangeLayerDelegate : public ContentLayerDelegate {
public:
    TestOpacityChangeLayerDelegate(CCLayerTreeHostTest* test)
        : m_test(test)
    {
    }

    virtual void paintContents(SkCanvas*, const IntRect&, FloatRect&) OVERRIDE
    {
        // Set layer opacity to 0.
        m_test->layerTreeHost()->rootLayer()->setOpacity(0);
    }

    virtual bool preserves3D() { return false; }

private:
    CCLayerTreeHostTest* m_test;
};

class ContentLayerChromiumWithUpdateTracking : public ContentLayerChromium {
public:
    static PassRefPtr<ContentLayerChromiumWithUpdateTracking> create(ContentLayerDelegate *delegate) { return adoptRef(new ContentLayerChromiumWithUpdateTracking(delegate)); }

    int paintContentsCount() { return m_paintContentsCount; }
    void resetPaintContentsCount() { m_paintContentsCount = 0; }

    virtual void update(CCTextureUpdateQueue& queue, const CCOcclusionTracker* occlusion, CCRenderingStats& stats) OVERRIDE
    {
        ContentLayerChromium::update(queue, occlusion, stats);
        m_paintContentsCount++;
    }

private:
    explicit ContentLayerChromiumWithUpdateTracking(ContentLayerDelegate* delegate)
        : ContentLayerChromium(delegate)
        , m_paintContentsCount(0)
    {
        setAnchorPoint(FloatPoint(0, 0));
        setBounds(IntSize(10, 10));
        setIsDrawable(true);
    }

    int m_paintContentsCount;
};

// Layer opacity change during paint should not prevent compositor resources from being updated during commit.
class CCLayerTreeHostTestOpacityChange : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestOpacityChange()
        : m_testOpacityChangeDelegate(this)
        , m_updateCheckLayer(ContentLayerChromiumWithUpdateTracking::create(&m_testOpacityChangeDelegate))
    {
    }

    virtual void beginTest()
    {
        m_layerTreeHost->setRootLayer(m_updateCheckLayer);
        m_layerTreeHost->setViewportSize(IntSize(10, 10), IntSize(10, 10));

        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*)
    {
        endTest();
    }

    virtual void afterTest()
    {
        // update() should have been called once.
        EXPECT_EQ(1, m_updateCheckLayer->paintContentsCount());

        // clear m_updateCheckLayer so CCLayerTreeHost dies.
        m_updateCheckLayer.clear();
    }

private:
    TestOpacityChangeLayerDelegate m_testOpacityChangeDelegate;
    RefPtr<ContentLayerChromiumWithUpdateTracking> m_updateCheckLayer;
};

TEST_F(CCLayerTreeHostTestOpacityChange, runMultiThread)
{
    runTest(true);
}

class MockContentLayerDelegate : public ContentLayerDelegate {
public:
    bool drawsContent() const { return true; }
    MOCK_CONST_METHOD0(preserves3D, bool());
    void paintContents(SkCanvas*, const IntRect&, FloatRect&) OVERRIDE { }
    void notifySyncRequired() { }
};

class CCLayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers : public CCLayerTreeHostTest {
public:

    CCLayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers()
        : m_rootLayer(ContentLayerChromium::create(&m_delegate))
        , m_childLayer(ContentLayerChromium::create(&m_delegate))
    {
    }

    virtual void beginTest()
    {
        m_layerTreeHost->setViewportSize(IntSize(40, 40), IntSize(60, 60));
        m_layerTreeHost->setDeviceScaleFactor(1.5);
        EXPECT_EQ(IntSize(40, 40), m_layerTreeHost->layoutViewportSize());
        EXPECT_EQ(IntSize(60, 60), m_layerTreeHost->deviceViewportSize());

        m_rootLayer->addChild(m_childLayer);

        m_rootLayer->setIsDrawable(true);
        m_rootLayer->setBounds(IntSize(30, 30));
        m_rootLayer->setAnchorPoint(FloatPoint(0, 0));

        m_childLayer->setIsDrawable(true);
        m_childLayer->setPosition(IntPoint(2, 2));
        m_childLayer->setBounds(IntSize(10, 10));
        m_childLayer->setAnchorPoint(FloatPoint(0, 0));

        m_layerTreeHost->setRootLayer(m_rootLayer);
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl)
    {
        // Get access to protected methods.
        MockLayerTreeHostImpl* mockImpl = static_cast<MockLayerTreeHostImpl*>(impl);

        // Should only do one commit.
        EXPECT_EQ(0, impl->sourceFrameNumber());
        // Device scale factor should come over to impl.
        EXPECT_NEAR(impl->deviceScaleFactor(), 1.5, 0.00001);

        // Both layers are on impl.
        ASSERT_EQ(1u, impl->rootLayer()->children().size());

        // Device viewport is scaled.
        EXPECT_EQ(IntSize(40, 40), impl->layoutViewportSize());
        EXPECT_EQ(IntSize(60, 60), impl->deviceViewportSize());

        CCLayerImpl* root = impl->rootLayer();
        CCLayerImpl* child = impl->rootLayer()->children()[0].get();

        // Positions remain in layout pixels.
        EXPECT_EQ(IntPoint(0, 0), root->position());
        EXPECT_EQ(IntPoint(2, 2), child->position());

        // Compute all the layer transforms for the frame.
        MockLayerTreeHostImpl::CCLayerList renderSurfaceLayerList;
        mockImpl->calculateRenderSurfaceLayerList(renderSurfaceLayerList);

        // Both layers should be drawing into the root render surface.
        ASSERT_EQ(1u, renderSurfaceLayerList.size());
        ASSERT_EQ(root->renderSurface(), renderSurfaceLayerList[0]->renderSurface());
        ASSERT_EQ(2u, root->renderSurface()->layerList().size());

        // The root render surface is the size of the viewport.
        EXPECT_EQ_RECT(IntRect(0, 0, 60, 60), root->renderSurface()->contentRect());

        WebTransformationMatrix scaleTransform;
        scaleTransform.scale(impl->deviceScaleFactor());

        // The root layer is scaled by 2x.
        WebTransformationMatrix rootScreenSpaceTransform = scaleTransform;
        WebTransformationMatrix rootDrawTransform = scaleTransform;

        EXPECT_EQ(rootDrawTransform, root->drawTransform());
        EXPECT_EQ(rootScreenSpaceTransform, root->screenSpaceTransform());

        // The child is at position 2,2, so translate by 2,2 before applying the scale by 2x.
        WebTransformationMatrix childScreenSpaceTransform = scaleTransform;
        childScreenSpaceTransform.translate(2, 2);
        WebTransformationMatrix childDrawTransform = scaleTransform;
        childDrawTransform.translate(2, 2);

        EXPECT_EQ(childDrawTransform, child->drawTransform());
        EXPECT_EQ(childScreenSpaceTransform, child->screenSpaceTransform());

        endTest();
    }

    virtual void afterTest()
    {
        m_rootLayer.clear();
        m_childLayer.clear();
    }

private:
    MockContentLayerDelegate m_delegate;
    RefPtr<ContentLayerChromium> m_rootLayer;
    RefPtr<ContentLayerChromium> m_childLayer;
};

TEST_F(CCLayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers, runMultiThread)
{
    runTest(true);
}

// Verify atomicity of commits and reuse of textures.
class CCLayerTreeHostTestAtomicCommit : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestAtomicCommit()
        : m_layer(ContentLayerChromiumWithUpdateTracking::create(&m_delegate))
    {
        // Make sure partial texture updates are turned off.
        m_settings.maxPartialTextureUpdates = 0;
    }

    virtual void beginTest()
    {
        m_layerTreeHost->setRootLayer(m_layer);
        m_layerTreeHost->setViewportSize(IntSize(10, 10), IntSize(10, 10));

        postSetNeedsCommitToMainThread();
        postSetNeedsRedrawToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl)
    {
        CompositorFakeWebGraphicsContext3DWithTextureTracking* context = static_cast<CompositorFakeWebGraphicsContext3DWithTextureTracking*>(impl->context()->context3D());

        switch (impl->sourceFrameNumber()) {
        case 0:
            // Number of textures should be one.
            ASSERT_EQ(1, context->numTextures());
            // Number of textures used for commit should be one.
            EXPECT_EQ(1, context->numUsedTextures());
            // Verify that used texture is correct.
            EXPECT_TRUE(context->usedTexture(context->texture(0)));

            context->resetUsedTextures();
            break;
        case 1:
            // Number of textures should be two as the first texture
            // is used by impl thread and cannot by used for update.
            ASSERT_EQ(2, context->numTextures());
            // Number of textures used for commit should still be one.
            EXPECT_EQ(1, context->numUsedTextures());
            // First texture should not have been used.
            EXPECT_FALSE(context->usedTexture(context->texture(0)));
            // New texture should have been used.
            EXPECT_TRUE(context->usedTexture(context->texture(1)));

            context->resetUsedTextures();
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        CompositorFakeWebGraphicsContext3DWithTextureTracking* context = static_cast<CompositorFakeWebGraphicsContext3DWithTextureTracking*>(impl->context()->context3D());

        // Number of textures used for draw should always be one.
        EXPECT_EQ(1, context->numUsedTextures());

        if (impl->sourceFrameNumber() < 1) {
            context->resetUsedTextures();
            postSetNeedsAnimateAndCommitToMainThread();
            postSetNeedsRedrawToMainThread();
        } else
            endTest();
    }

    virtual void layout()
    {
        m_layer->setNeedsDisplay();
    }

    virtual void afterTest()
    {
    }

private:
    MockContentLayerDelegate m_delegate;
    RefPtr<ContentLayerChromiumWithUpdateTracking> m_layer;
};

TEST_F(CCLayerTreeHostTestAtomicCommit, runMultiThread)
{
    runTest(true);
}

static void setLayerPropertiesForTesting(LayerChromium* layer, LayerChromium* parent, const WebTransformationMatrix& transform, const FloatPoint& anchor, const FloatPoint& position, const IntSize& bounds, bool opaque)
{
    layer->removeAllChildren();
    if (parent)
        parent->addChild(layer);
    layer->setTransform(transform);
    layer->setAnchorPoint(anchor);
    layer->setPosition(position);
    layer->setBounds(bounds);
    layer->setOpaque(opaque);
}

class CCLayerTreeHostTestAtomicCommitWithPartialUpdate : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestAtomicCommitWithPartialUpdate()
        : m_parent(ContentLayerChromiumWithUpdateTracking::create(&m_delegate))
        , m_child(ContentLayerChromiumWithUpdateTracking::create(&m_delegate))
        , m_numCommits(0)
    {
        // Allow one partial texture update.
        m_settings.maxPartialTextureUpdates = 1;
    }

    virtual void beginTest()
    {
        m_layerTreeHost->setRootLayer(m_parent);
        m_layerTreeHost->setViewportSize(IntSize(10, 20), IntSize(10, 20));

        WebTransformationMatrix identityMatrix;
        setLayerPropertiesForTesting(m_parent.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(10, 20), true);
        setLayerPropertiesForTesting(m_child.get(), m_parent.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(0, 10), IntSize(10, 10), false);

        postSetNeedsCommitToMainThread();
        postSetNeedsRedrawToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl)
    {
        CompositorFakeWebGraphicsContext3DWithTextureTracking* context = static_cast<CompositorFakeWebGraphicsContext3DWithTextureTracking*>(impl->context()->context3D());

        switch (impl->sourceFrameNumber()) {
        case 0:
            // Number of textures should be two.
            ASSERT_EQ(2, context->numTextures());
            // Number of textures used for commit should be two.
            EXPECT_EQ(2, context->numUsedTextures());
            // Verify that used textures are correct.
            EXPECT_TRUE(context->usedTexture(context->texture(0)));
            EXPECT_TRUE(context->usedTexture(context->texture(1)));

            context->resetUsedTextures();
            break;
        case 1:
            // Number of textures used for commit should still be two.
            EXPECT_EQ(2, context->numUsedTextures());
            // First two textures should not have been used.
            EXPECT_FALSE(context->usedTexture(context->texture(0)));
            EXPECT_FALSE(context->usedTexture(context->texture(1)));
            // New textures should have been used.
            EXPECT_TRUE(context->usedTexture(context->texture(2)));
            EXPECT_TRUE(context->usedTexture(context->texture(3)));

            context->resetUsedTextures();
            break;
        case 2:
            // Number of textures used for commit should still be two.
            EXPECT_EQ(2, context->numUsedTextures());

            context->resetUsedTextures();
            break;
        case 3:
            // No textures should be used for commit.
            EXPECT_EQ(0, context->numUsedTextures());

            context->resetUsedTextures();
            break;
        case 4:
            // Number of textures used for commit should be one.
            EXPECT_EQ(1, context->numUsedTextures());

            context->resetUsedTextures();
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        CompositorFakeWebGraphicsContext3DWithTextureTracking* context = static_cast<CompositorFakeWebGraphicsContext3DWithTextureTracking*>(impl->context()->context3D());

        // Number of textures used for drawing should two except for frame 4
        // where the viewport only contains one layer.
        if (impl->sourceFrameNumber() == 3)
            EXPECT_EQ(1, context->numUsedTextures());
        else
            EXPECT_EQ(2, context->numUsedTextures());

        if (impl->sourceFrameNumber() < 4) {
            context->resetUsedTextures();
            postSetNeedsAnimateAndCommitToMainThread();
            postSetNeedsRedrawToMainThread();
        } else
            endTest();
    }

    virtual void layout()
    {
        switch (m_numCommits++) {
        case 0:
        case 1:
            m_parent->setNeedsDisplay();
            m_child->setNeedsDisplay();
            break;
        case 2:
            // Damage part of layers.
            m_parent->setNeedsDisplayRect(FloatRect(0, 0, 5, 5));
            m_child->setNeedsDisplayRect(FloatRect(0, 0, 5, 5));
            break;
        case 3:
            m_child->setNeedsDisplay();
            m_layerTreeHost->setViewportSize(IntSize(10, 10), IntSize(10, 10));
            break;
        case 4:
            m_layerTreeHost->setViewportSize(IntSize(10, 20), IntSize(10, 20));
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    virtual void afterTest()
    {
    }

private:
    MockContentLayerDelegate m_delegate;
    RefPtr<ContentLayerChromiumWithUpdateTracking> m_parent;
    RefPtr<ContentLayerChromiumWithUpdateTracking> m_child;
    int m_numCommits;
};

TEST_F(CCLayerTreeHostTestAtomicCommitWithPartialUpdate, runMultiThread)
{
    runTest(true);
}

class TestLayerChromium : public LayerChromium {
public:
    static PassRefPtr<TestLayerChromium> create() { return adoptRef(new TestLayerChromium()); }

    virtual void update(CCTextureUpdateQueue&, const CCOcclusionTracker* occlusion, CCRenderingStats&) OVERRIDE
    {
        // Gain access to internals of the CCOcclusionTracker.
        const TestCCOcclusionTracker* testOcclusion = static_cast<const TestCCOcclusionTracker*>(occlusion);
        m_occludedScreenSpace = testOcclusion ? testOcclusion->occlusionInScreenSpace() : Region();
    }

    virtual bool drawsContent() const OVERRIDE { return true; }

    const Region& occludedScreenSpace() const { return m_occludedScreenSpace; }
    void clearOccludedScreenSpace() { m_occludedScreenSpace = Region(); }

private:
    TestLayerChromium() : LayerChromium() { }

    Region m_occludedScreenSpace;
};

static void setTestLayerPropertiesForTesting(TestLayerChromium* layer, LayerChromium* parent, const WebTransformationMatrix& transform, const FloatPoint& anchor, const FloatPoint& position, const IntSize& bounds, bool opaque)
{
    setLayerPropertiesForTesting(layer, parent, transform, anchor, position, bounds, opaque);
    layer->clearOccludedScreenSpace();
}

class CCLayerTreeHostTestLayerOcclusion : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestLayerOcclusion() { }

    virtual void beginTest()
    {
        RefPtr<TestLayerChromium> rootLayer = TestLayerChromium::create();
        RefPtr<TestLayerChromium> child = TestLayerChromium::create();
        RefPtr<TestLayerChromium> child2 = TestLayerChromium::create();
        RefPtr<TestLayerChromium> grandChild = TestLayerChromium::create();
        RefPtr<TestLayerChromium> mask = TestLayerChromium::create();

        WebTransformationMatrix identityMatrix;
        WebTransformationMatrix childTransform;
        childTransform.translate(250, 250);
        childTransform.rotate(90);
        childTransform.translate(-250, -250);

        child->setMasksToBounds(true);

        // See CCLayerTreeHostCommonTest.layerAddsSelfToOccludedRegionWithRotatedSurface for a nice visual of these layers and how they end up
        // positioned on the screen.

        // The child layer is rotated and the grandChild is opaque, but clipped to the child and rootLayer
        setTestLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setTestLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), false);
        setTestLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds(), rootLayer->bounds());
        ASSERT_TRUE(m_layerTreeHost->initializeLayerRendererIfNeeded());
        CCTextureUpdateQueue queue;
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 170, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 170, 160), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer is opaque, then it adds to the occlusion seen by the rootLayer.
        setLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds(), rootLayer->bounds());
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 170, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 30, 170, 170), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // Add a second child to the root layer and the regions should merge
        setTestLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setTestLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(70, 20), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds(), rootLayer->bounds());
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 170, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 30, 170, 170), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 20, 170, 180), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(2u, rootLayer->occludedScreenSpace().rects().size());

        // Move the second child to be sure.
        setTestLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setTestLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds(), rootLayer->bounds());
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 170, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 30, 170, 170), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 30, 190, 170), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(2u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer has a mask on it, then it shouldn't contribute to occlusion on stuff below it
        setLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);
        setLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        child->setMaskLayer(mask.get());

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds(), rootLayer->bounds());
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 170, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 190, 130), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer with a mask is below child2, then child2 should contribute to occlusion on everything, and child shouldn't contribute to the rootLayer
        setLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);
        setLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);

        child->setMaskLayer(mask.get());

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds(), rootLayer->bounds());
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 190, 130), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 40, 190, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(2u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 190, 130), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer has a non-opaque drawOpacity, then it shouldn't contribute to occlusion on stuff below it
        setTestLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setTestLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        child->setMaskLayer(0);
        child->setOpacity(0.5);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds(), rootLayer->bounds());
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 170, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 190, 130), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer with non-opaque drawOpacity is below child2, then child2 should contribute to occlusion on everything, and child shouldn't contribute to the rootLayer
        setTestLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setTestLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);

        child->setMaskLayer(0);
        child->setOpacity(0.5);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds(), rootLayer->bounds());
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 190, 130), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 40, 190, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(2u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 190, 130), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.clear();

        endTest();
    }

    virtual void afterTest()
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestLayerOcclusion)

class CCLayerTreeHostTestLayerOcclusionWithFilters : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestLayerOcclusionWithFilters() { }

    virtual void beginTest()
    {
        RefPtr<TestLayerChromium> rootLayer = TestLayerChromium::create();
        RefPtr<TestLayerChromium> child = TestLayerChromium::create();
        RefPtr<TestLayerChromium> child2 = TestLayerChromium::create();
        RefPtr<TestLayerChromium> grandChild = TestLayerChromium::create();
        RefPtr<TestLayerChromium> mask = TestLayerChromium::create();

        WebTransformationMatrix identityMatrix;
        WebTransformationMatrix childTransform;
        childTransform.translate(250, 250);
        childTransform.rotate(90);
        childTransform.translate(-250, -250);

        child->setMasksToBounds(true);

        // If the child layer has a filter that changes alpha values, and is below child2, then child2 should contribute to occlusion on everything,
        // and child shouldn't contribute to the rootLayer
        setTestLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setTestLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);

        {
            WebFilterOperations filters;
            filters.append(WebFilterOperation::createOpacityFilter(0.5));
            child->setFilters(filters);
        }

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds(), rootLayer->bounds());
        ASSERT_TRUE(m_layerTreeHost->initializeLayerRendererIfNeeded());
        CCTextureUpdateQueue queue;
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 190, 130), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 40, 190, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(2u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 190, 130), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer has a filter that moves pixels/changes alpha, and is below child2, then child should not inherit occlusion from outside its subtree,
        // and should not contribute to the rootLayer
        setTestLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setTestLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);

        {
            WebFilterOperations filters;
            filters.append(WebFilterOperation::createBlurFilter(10));
            child->setFilters(filters);
        }

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds(), rootLayer->bounds());
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 170, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 190, 130), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.clear();

        CCLayerTreeHost::setNeedsFilterContext(false);
        endTest();
    }

    virtual void afterTest()
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestLayerOcclusionWithFilters)

class CCLayerTreeHostTestManySurfaces : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestManySurfaces() { }

    virtual void beginTest()
    {
        // We create enough RenderSurfaces that it will trigger Vector reallocation while computing occlusion.
        Region occluded;
        const WebTransformationMatrix identityMatrix;
        Vector<RefPtr<TestLayerChromium> > layers;
        Vector<RefPtr<TestLayerChromium> > children;
        int numSurfaces = 20;
        RefPtr<TestLayerChromium> replica = TestLayerChromium::create();

        for (int i = 0; i < numSurfaces; ++i) {
            layers.append(TestLayerChromium::create());
            if (!i) {
                setTestLayerPropertiesForTesting(layers.last().get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
                layers.last()->createRenderSurface();
            } else {
                setTestLayerPropertiesForTesting(layers.last().get(), layers[layers.size()-2].get(), identityMatrix, FloatPoint(0, 0), FloatPoint(1, 1), IntSize(200-i, 200-i), true);
                layers.last()->setMasksToBounds(true);
                layers.last()->setReplicaLayer(replica.get()); // Make it have a RenderSurface
            }
        }

        for (int i = 1; i < numSurfaces; ++i) {
            children.append(TestLayerChromium::create());
            setTestLayerPropertiesForTesting(children.last().get(), layers[i].get(), identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(500, 500), false);
        }

        m_layerTreeHost->setRootLayer(layers[0].get());
        m_layerTreeHost->setViewportSize(layers[0]->bounds(), layers[0]->bounds());
        ASSERT_TRUE(m_layerTreeHost->initializeLayerRendererIfNeeded());
        CCTextureUpdateQueue queue;
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        for (int i = 0; i < numSurfaces-1; ++i) {
            IntRect expectedOcclusion(i+1, i+1, 200-i-1, 200-i-1);

            EXPECT_EQ_RECT(expectedOcclusion, layers[i]->occludedScreenSpace().bounds());
            EXPECT_EQ(1u, layers[i]->occludedScreenSpace().rects().size());
        }

        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.clear();

        endTest();
    }

    virtual void afterTest()
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestManySurfaces)

// A loseContext(1) should lead to a didRecreateOutputSurface(true)
class CCLayerTreeHostTestSetSingleLostContext : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestSetSingleLostContext()
    {
    }

    virtual void beginTest()
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void didCommitAndDrawFrame()
    {
        m_layerTreeHost->loseContext(1);
    }

    virtual void didRecreateOutputSurface(bool succeeded)
    {
        EXPECT_TRUE(succeeded);
        endTest();
    }

    virtual void afterTest()
    {
    }
};

TEST_F(CCLayerTreeHostTestSetSingleLostContext, runMultiThread)
{
    runTest(true);
}

// A loseContext(10) should lead to a didRecreateOutputSurface(false), and
// a finishAllRendering() should not hang.
class CCLayerTreeHostTestSetRepeatedLostContext : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestSetRepeatedLostContext()
    {
    }

    virtual void beginTest()
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void didCommitAndDrawFrame()
    {
        m_layerTreeHost->loseContext(10);
    }

    virtual void didRecreateOutputSurface(bool succeeded)
    {
        EXPECT_FALSE(succeeded);
        m_layerTreeHost->finishAllRendering();
        endTest();
    }

    virtual void afterTest()
    {
    }
};

TEST_F(CCLayerTreeHostTestSetRepeatedLostContext, runMultiThread)
{
    runTest(true);
}

class CCLayerTreeHostTestFractionalScroll : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestFractionalScroll()
        : m_scrollAmount(1.75, 0)
    {
    }

    virtual void beginTest()
    {
        m_layerTreeHost->rootLayer()->setScrollable(true);
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        CCLayerImpl* root = impl->rootLayer();
        root->setMaxScrollPosition(IntSize(100, 100));

        // Check that a fractional scroll delta is correctly accumulated over multiple commits.
        if (!impl->sourceFrameNumber()) {
            EXPECT_EQ(root->scrollPosition(), IntPoint(0, 0));
            EXPECT_EQ(root->scrollDelta(), FloatSize(0, 0));
            postSetNeedsCommitToMainThread();
        } else if (impl->sourceFrameNumber() == 1) {
            EXPECT_EQ(root->scrollPosition(), flooredIntPoint(m_scrollAmount));
            EXPECT_EQ(root->scrollDelta(), FloatSize(fmod(m_scrollAmount.width(), 1), 0));
            postSetNeedsCommitToMainThread();
        } else if (impl->sourceFrameNumber() == 2) {
            EXPECT_EQ(root->scrollPosition(), flooredIntPoint(m_scrollAmount + m_scrollAmount));
            EXPECT_EQ(root->scrollDelta(), FloatSize(fmod(2 * m_scrollAmount.width(), 1), 0));
            endTest();
        }
        root->scrollBy(m_scrollAmount);
    }

    virtual void applyScrollAndScale(const IntSize& scrollDelta, float scale)
    {
        IntPoint position = m_layerTreeHost->rootLayer()->scrollPosition();
        m_layerTreeHost->rootLayer()->setScrollPosition(position + scrollDelta);
    }

    virtual void afterTest()
    {
    }
private:
    FloatSize m_scrollAmount;
};

TEST_F(CCLayerTreeHostTestFractionalScroll, runMultiThread)
{
    runTest(true);
}

class CCLayerTreeHostTestFinishAllRendering : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestFinishAllRendering()
        : m_once(false)
        , m_mutex()
        , m_drawCount(0)
    {
    }

    virtual void beginTest()
    {
        m_layerTreeHost->setNeedsRedraw();
    }

    virtual void didCommitAndDrawFrame()
    {
        if (m_once)
            return;
        m_once = true;
        m_layerTreeHost->setNeedsRedraw();
        m_layerTreeHost->acquireLayerTextures();
        {
            Locker<Mutex> lock(m_mutex);
            m_drawCount = 0;
        }
        m_layerTreeHost->finishAllRendering();
        {
            Locker<Mutex> lock(m_mutex);
            EXPECT_EQ(0, m_drawCount);
        }
        endTest();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        Locker<Mutex> lock(m_mutex);
        ++m_drawCount;
    }

    virtual void afterTest()
    {
    }
private:

    bool m_once;
    Mutex m_mutex;
    int m_drawCount;
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestFinishAllRendering)

// Layers added to tree with existing active animations should have the animation
// correctly recognized.
class CCLayerTreeHostTestLayerAddedWithAnimation : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestLayerAddedWithAnimation()
        : m_addedAnimation(false)
    {
    }

    virtual void beginTest()
    {
        EXPECT_FALSE(m_addedAnimation);

        RefPtr<LayerChromium> layer = LayerChromium::create();
        layer->setLayerAnimationDelegate(this);

        // Any valid CCAnimationCurve will do here.
        OwnPtr<CCAnimationCurve> curve(CCEaseTimingFunction::create());
        OwnPtr<CCActiveAnimation> animation(CCActiveAnimation::create(curve.release(), 1, 1, CCActiveAnimation::Opacity));
        layer->layerAnimationController()->addAnimation(animation.release());

        // We add the animation *before* attaching the layer to the tree.
        m_layerTreeHost->rootLayer()->addChild(layer);
        EXPECT_TRUE(m_addedAnimation);

        endTest();
    }

    virtual void didAddAnimation()
    {
        m_addedAnimation = true;
    }

    virtual void afterTest() { }

private:
    bool m_addedAnimation;
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestLayerAddedWithAnimation)

class CCLayerTreeHostTestScrollChildLayer : public CCLayerTreeHostTest, public LayerChromiumScrollDelegate {
public:
    CCLayerTreeHostTestScrollChildLayer()
        : m_scrollAmount(2, 1)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setViewportSize(IntSize(10, 10), IntSize(10, 10));
        m_layerTreeHost->rootLayer()->setBounds(IntSize(10, 10));
        
        m_rootScrollLayer = ContentLayerChromium::create(&m_mockDelegate);
        m_rootScrollLayer->setBounds(IntSize(10, 10));

        m_rootScrollLayer->setPosition(FloatPoint(0, 0));
        m_rootScrollLayer->setAnchorPoint(FloatPoint(0, 0));

        m_rootScrollLayer->setIsDrawable(true);
        m_rootScrollLayer->setScrollable(true);
        m_rootScrollLayer->setMaxScrollPosition(IntSize(100, 100));
        m_layerTreeHost->rootLayer()->addChild(m_rootScrollLayer);
        m_childLayer = ContentLayerChromium::create(&m_mockDelegate);
        m_childLayer->setLayerScrollDelegate(this);
        m_childLayer->setBounds(IntSize(50, 50));
        m_childLayer->setIsDrawable(true);
        m_childLayer->setScrollable(true);
        m_childLayer->setMaxScrollPosition(IntSize(100, 100));

        m_childLayer->setPosition(FloatPoint(0, 0));
        m_childLayer->setAnchorPoint(FloatPoint(0, 0));

        m_rootScrollLayer->addChild(m_childLayer);
        postSetNeedsCommitToMainThread();
    }

    virtual void didScroll(const IntSize& scrollDelta) OVERRIDE
    {
        m_reportedScrollAmount = scrollDelta;
    }

    virtual void applyScrollAndScale(const IntSize& scrollDelta, float) OVERRIDE
    {
        IntPoint position = m_rootScrollLayer->scrollPosition();
        m_rootScrollLayer->setScrollPosition(position + scrollDelta);
    }

    virtual void beginCommitOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        EXPECT_EQ(m_rootScrollLayer->scrollPosition(), IntPoint());
        if (!m_layerTreeHost->commitNumber())
            EXPECT_EQ(m_childLayer->scrollPosition(), IntPoint());
        else
            EXPECT_EQ(m_childLayer->scrollPosition(), IntPoint() + m_scrollAmount);
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        if (impl->sourceAnimationFrameNumber() == 1) {
            EXPECT_EQ(impl->scrollBegin(IntPoint(5, 5), CCInputHandlerClient::Wheel), CCInputHandlerClient::ScrollStarted);
            impl->scrollBy(IntPoint(), m_scrollAmount);
            impl->scrollEnd();
        } else if (impl->sourceAnimationFrameNumber() == 2)
            endTest();
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(m_scrollAmount, m_reportedScrollAmount);
    }

private:
    const IntSize m_scrollAmount;
    IntSize m_reportedScrollAmount;
    MockContentLayerDelegate m_mockDelegate;
    RefPtr<LayerChromium> m_childLayer;
    RefPtr<LayerChromium> m_rootScrollLayer;
};

TEST_F(CCLayerTreeHostTestScrollChildLayer, runMultiThread)
{
    runTest(true);
}

class CCLayerTreeHostTestCompositeAndReadbackCleanup : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestCompositeAndReadbackCleanup() { }

    virtual void beginTest()
    {
        LayerChromium* rootLayer = m_layerTreeHost->rootLayer();

        OwnArrayPtr<char> pixels(adoptArrayPtr(new char[4]));
        m_layerTreeHost->compositeAndReadback(static_cast<void*>(pixels.get()), IntRect(0, 0, 1, 1));
        EXPECT_FALSE(rootLayer->renderSurface());

        endTest();
    }

    virtual void afterTest()
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestCompositeAndReadbackCleanup)

class CCLayerTreeHostTestSurfaceNotAllocatedForLayersOutsideMemoryLimit : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestSurfaceNotAllocatedForLayersOutsideMemoryLimit()
        : m_rootLayer(ContentLayerChromiumWithUpdateTracking::create(&m_mockDelegate))
        , m_surfaceLayer1(ContentLayerChromiumWithUpdateTracking::create(&m_mockDelegate))
        , m_replicaLayer1(ContentLayerChromiumWithUpdateTracking::create(&m_mockDelegate))
        , m_surfaceLayer2(ContentLayerChromiumWithUpdateTracking::create(&m_mockDelegate))
        , m_replicaLayer2(ContentLayerChromiumWithUpdateTracking::create(&m_mockDelegate))
    {
    }

    virtual void beginTest()
    {
        m_layerTreeHost->setViewportSize(IntSize(100, 100), IntSize(100, 100));

        m_rootLayer->setBounds(IntSize(100, 100));
        m_surfaceLayer1->setBounds(IntSize(100, 100));
        m_surfaceLayer1->setForceRenderSurface(true);
        m_surfaceLayer1->setOpacity(0.5);
        m_surfaceLayer2->setBounds(IntSize(100, 100));
        m_surfaceLayer2->setForceRenderSurface(true);
        m_surfaceLayer2->setOpacity(0.5);

        m_surfaceLayer1->setReplicaLayer(m_replicaLayer1.get());
        m_surfaceLayer2->setReplicaLayer(m_replicaLayer2.get());

        m_rootLayer->addChild(m_surfaceLayer1);
        m_surfaceLayer1->addChild(m_surfaceLayer2);
        m_layerTreeHost->setRootLayer(m_rootLayer);
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* hostImpl)
    {
        CCRenderer* renderer = hostImpl->layerRenderer();
        unsigned surface1RenderPassId = hostImpl->rootLayer()->children()[0]->id();
        unsigned surface2RenderPassId = hostImpl->rootLayer()->children()[0]->children()[0]->id();

        switch (hostImpl->sourceFrameNumber()) {
        case 0:
            EXPECT_TRUE(renderer->haveCachedResourcesForRenderPassId(surface1RenderPassId));
            EXPECT_TRUE(renderer->haveCachedResourcesForRenderPassId(surface2RenderPassId));

            // Reduce the memory limit to only fit the root layer and one render surface. This
            // prevents any contents drawing into surfaces from being allocated.
            hostImpl->setMemoryAllocationLimitBytes(100 * 100 * 4 * 2);
            break;
        case 1:
            EXPECT_FALSE(renderer->haveCachedResourcesForRenderPassId(surface1RenderPassId));
            EXPECT_FALSE(renderer->haveCachedResourcesForRenderPassId(surface2RenderPassId));

            endTest();
            break;
        }
    }

    virtual void afterTest()
    {
        EXPECT_EQ(2, m_rootLayer->paintContentsCount());
        EXPECT_EQ(2, m_surfaceLayer1->paintContentsCount());
        EXPECT_EQ(2, m_surfaceLayer2->paintContentsCount());

        // Clear layer references so CCLayerTreeHost dies.
        m_rootLayer.clear();
        m_surfaceLayer1.clear();
        m_replicaLayer1.clear();
        m_surfaceLayer2.clear();
        m_replicaLayer2.clear();
    }

private:
    MockContentLayerDelegate m_mockDelegate;
    RefPtr<ContentLayerChromiumWithUpdateTracking> m_rootLayer;
    RefPtr<ContentLayerChromiumWithUpdateTracking> m_surfaceLayer1;
    RefPtr<ContentLayerChromiumWithUpdateTracking> m_replicaLayer1;
    RefPtr<ContentLayerChromiumWithUpdateTracking> m_surfaceLayer2;
    RefPtr<ContentLayerChromiumWithUpdateTracking> m_replicaLayer2;
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestSurfaceNotAllocatedForLayersOutsideMemoryLimit)


class EvictionTrackingTexture : public LayerTextureUpdater::Texture {
public:
    static PassOwnPtr<EvictionTrackingTexture> create(PassOwnPtr<CCPrioritizedTexture> texture) { return adoptPtr(new EvictionTrackingTexture(texture)); }
    virtual ~EvictionTrackingTexture() { }

    virtual void updateRect(CCResourceProvider* resourceProvider, const IntRect&, const IntSize&) OVERRIDE
    {
        ASSERT_TRUE(!texture()->haveBackingTexture() || resourceProvider->numResources() > 0);
        texture()->acquireBackingTexture(resourceProvider);
        m_updated = true;
    }
    void resetUpdated() { m_updated = false; }
    bool updated() const { return m_updated; }

private:
    explicit EvictionTrackingTexture(PassOwnPtr<CCPrioritizedTexture> texture)
        : LayerTextureUpdater::Texture(texture)
        , m_updated(false)
    { }
    bool m_updated;
};

class EvictionTestLayer : public LayerChromium {
public:
    static PassRefPtr<EvictionTestLayer> create() { return adoptRef(new EvictionTestLayer()); }

    virtual void update(CCTextureUpdateQueue&, const CCOcclusionTracker*, CCRenderingStats&) OVERRIDE;
    virtual bool drawsContent() const OVERRIDE { return true; }

    virtual PassOwnPtr<CCLayerImpl> createCCLayerImpl() OVERRIDE;
    virtual void pushPropertiesTo(CCLayerImpl*) OVERRIDE;
    virtual void setTexturePriorities(const CCPriorityCalculator&) OVERRIDE;

    void resetUpdated()
    {
        if (m_texture.get())
            m_texture->resetUpdated();
    }
    bool updated() const { return m_texture.get() ? m_texture->updated() : false; }

private:
    EvictionTestLayer() : LayerChromium() { }

    void createTextureIfNeeded()
    {
        if (m_texture.get())
            return;
        m_texture = EvictionTrackingTexture::create(CCPrioritizedTexture::create(layerTreeHost()->contentsTextureManager()));
        m_texture->texture()->setDimensions(WebCore::IntSize(10, 10), WebCore::GraphicsContext3D::RGBA);
    }

    OwnPtr<EvictionTrackingTexture> m_texture;
};

class EvictionTestLayerImpl : public CCLayerImpl {
public:
    static PassOwnPtr<EvictionTestLayerImpl> create(int id)
    {
        return adoptPtr(new EvictionTestLayerImpl(id));
    }
    virtual ~EvictionTestLayerImpl() { }
    virtual void appendQuads(CCQuadSink&, const CCSharedQuadState*, bool& hadMissingTiles)
    {
        ASSERT_TRUE(m_hasTexture);
        ASSERT_NE(0u, layerTreeHostImpl()->resourceProvider()->numResources());
    }
    void setHasTexture(bool hasTexture) { m_hasTexture = hasTexture; }

private:
    explicit EvictionTestLayerImpl(int id)
        : CCLayerImpl(id)
        , m_hasTexture(false) { }

    bool m_hasTexture;
};

void EvictionTestLayer::setTexturePriorities(const CCPriorityCalculator&)
{
    createTextureIfNeeded();
    if (!m_texture.get())
        return;
    m_texture->texture()->setRequestPriority(CCPriorityCalculator::uiPriority(true));
}

void EvictionTestLayer::update(CCTextureUpdateQueue& queue, const CCOcclusionTracker*, CCRenderingStats&)
{
    createTextureIfNeeded();
    if (!m_texture.get())
        return;
    IntRect fullRect(0, 0, 10, 10);
    TextureUploader::Parameters parameters = { m_texture.get(), fullRect, IntSize() };
    queue.appendFullUpload(parameters);
}

PassOwnPtr<CCLayerImpl> EvictionTestLayer::createCCLayerImpl()
{
    return EvictionTestLayerImpl::create(m_layerId);
}

void EvictionTestLayer::pushPropertiesTo(CCLayerImpl* layerImpl)
{
    LayerChromium::pushPropertiesTo(layerImpl);

    EvictionTestLayerImpl* testLayerImpl = static_cast<EvictionTestLayerImpl*>(layerImpl);
    testLayerImpl->setHasTexture(m_texture->texture()->haveBackingTexture());
}

class CCLayerTreeHostTestEvictTextures : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestEvictTextures()
        : m_layer(EvictionTestLayer::create())
        , m_implForEvictTextures(0)
        , m_numCommits(0)
    {
    }

    virtual void beginTest()
    {
        m_layerTreeHost->setRootLayer(m_layer);
        m_layerTreeHost->setViewportSize(IntSize(10, 20), IntSize(10, 20));

        WebTransformationMatrix identityMatrix;
        setLayerPropertiesForTesting(m_layer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(10, 20), true);
    }

    class EvictTexturesTask : public WebKit::WebThread::Task {
    public:
        EvictTexturesTask(CCLayerTreeHostTestEvictTextures* test) : m_test(test) { }
        virtual ~EvictTexturesTask() { }
        virtual void run()
        {
            ASSERT(m_test->m_implForEvictTextures);
            m_test->m_implForEvictTextures->releaseContentsTextures();
        }

    private:
        CCLayerTreeHostTestEvictTextures* m_test;
    };

    void postEvictTextures()
    {
        ASSERT(webThread());
        webThread()->postTask(new EvictTexturesTask(this));
    }

    // Commit 1: Just commit and draw normally, then post an eviction at the end
    // that will trigger a commit.
    // Commit 2: Triggered by the eviction, let it go through and then set
    // needsCommit.
    // Commit 3: Triggered by the setNeedsCommit. In layout(), post an eviction
    // task, which will be handled before the commit. Don't set needsCommit, it
    // should have been posted. A frame should not be drawn (note,
    // didCommitAndDrawFrame may be called anyway).
    // Commit 4: Triggered by the eviction, let it go through and then set
    // needsCommit.
    // Commit 5: Triggered by the setNeedsCommit, post an eviction task in
    // layout(), a frame should not be drawn but a commit will be posted.
    // Commit 6: Triggered by the eviction, post an eviction task in
    // layout(), which will be a noop, letting the commit (which recreates the
    // textures) go through and draw a frame, then end the test.
    //
    // Commits 1+2 test the eviction recovery path where eviction happens outside
    // of the beginFrame/commit pair.
    // Commits 3+4 test the eviction recovery path where eviction happens inside
    // the beginFrame/commit pair.
    // Commits 5+6 test the path where an eviction happens during the eviction
    // recovery path.
    virtual void didCommitAndDrawFrame()
    {
        switch (m_numCommits) {
        case 1:
            EXPECT_TRUE(m_layer->updated());
            postEvictTextures();
            break;
        case 2:
            EXPECT_TRUE(m_layer->updated());
            m_layerTreeHost->setNeedsCommit();
            break;
        case 3:
            break;
        case 4:
            EXPECT_TRUE(m_layer->updated());
            m_layerTreeHost->setNeedsCommit();
            break;
        case 5:
            break;
        case 6:
            EXPECT_TRUE(m_layer->updated());
            endTest();
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl)
    {
        m_implForEvictTextures = impl;
    }

    virtual void layout()
    {
        ++m_numCommits;
        switch (m_numCommits) {
        case 1:
        case 2:
            break;
        case 3:
            postEvictTextures();
            break;
        case 4:
            // We couldn't check in didCommitAndDrawFrame on commit 3, so check here.
            EXPECT_FALSE(m_layer->updated());
            break;
        case 5:
            postEvictTextures();
            break;
        case 6:
            // We couldn't check in didCommitAndDrawFrame on commit 5, so check here.
            EXPECT_FALSE(m_layer->updated());
            postEvictTextures();
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        m_layer->resetUpdated();
    }

    virtual void afterTest()
    {
    }

private:
    MockContentLayerDelegate m_delegate;
    RefPtr<EvictionTestLayer> m_layer;
    CCLayerTreeHostImpl* m_implForEvictTextures;
    int m_numCommits;
};

TEST_F(CCLayerTreeHostTestEvictTextures, runMultiThread)
{
    runTest(true);
}

} // namespace
