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

#include "cc/CCSingleThreadProxy.h"

#include "CCThreadedTest.h"
#include "CompositorFakeWebGraphicsContext3D.h"
#include "FakeWebGraphicsContext3D.h"
#include "GraphicsContext3DPrivate.h"
#include "platform/WebThread.h"

using namespace WebCore;
using namespace WebKit;
using namespace WebKitTests;

class FakeWebGraphicsContext3DMakeCurrentFails : public FakeWebGraphicsContext3D {
public:
    virtual bool makeContextCurrent() { return false; }
};

class CCSingleThreadProxyTestInitializeLayerRendererFailsAfterAddAnimation : public CCThreadedTest {
public:
    CCSingleThreadProxyTestInitializeLayerRendererFailsAfterAddAnimation()
    {
    }

    virtual void beginTest()
    {
        // This will cause the animation timer to be set which will fire in
        // CCSingleThreadProxy::animationTimerDelay() seconds.
        postAddAnimationToMainThread();
    }

    virtual void animateLayers(CCLayerTreeHostImpl* layerTreeHostImpl, double monotonicTime)
    {
        ASSERT_NOT_REACHED();
    }

    virtual void didRecreateContext(bool succeeded)
    {
        EXPECT_FALSE(succeeded);

        // Make sure we wait CCSingleThreadProxy::animationTimerDelay() seconds
        // (use ceil just to be sure). If the timer was not disabled, we will
        // attempt to call CCSingleThreadProxy::compositeImmediately and the
        // test will fail.
        endTestAfterDelay(ceil(CCSingleThreadProxy::animationTimerDelay() * 1000));
    }

    virtual PassRefPtr<GraphicsContext3D> createContext() OVERRIDE
    {
        return GraphicsContext3DPrivate::createGraphicsContextFromWebContext(adoptPtr(new FakeWebGraphicsContext3DMakeCurrentFails), GraphicsContext3D::RenderDirectlyToHostWindow);
    }

    virtual void afterTest()
    {
    }
};

TEST_F(CCSingleThreadProxyTestInitializeLayerRendererFailsAfterAddAnimation, runSingleThread)
{
    runTest(false);
}

class CCSingleThreadProxyTestDidAddAnimationBeforeInitializingLayerRenderer : public CCThreadedTest {
public:
    CCSingleThreadProxyTestDidAddAnimationBeforeInitializingLayerRenderer()
    {
    }

    virtual void beginTest()
    {
        // This will cause the animation timer to be set which will fire in
        // CCSingleThreadProxy::animationTimerDelay() seconds.
        postDidAddAnimationToMainThread();
    }

    virtual void animateLayers(CCLayerTreeHostImpl*, double)
    {
        ASSERT_NOT_REACHED();
    }

    virtual void didRecreateContext(bool)
    {
        ASSERT_NOT_REACHED();
    }

    virtual void didAddAnimation()
    {
        // Make sure we wait CCSingleThreadProxy::animationTimerDelay() seconds
        // (use ceil just to be sure). If the timer was not disabled, we will
        // attempt to call CCSingleThreadProxy::compositeImmediately and the
        // test will fail.
        endTestAfterDelay(ceil(CCSingleThreadProxy::animationTimerDelay() * 1000));
    }

    virtual PassRefPtr<GraphicsContext3D> createContext() OVERRIDE
    {
        return GraphicsContext3DPrivate::createGraphicsContextFromWebContext(adoptPtr(new FakeWebGraphicsContext3DMakeCurrentFails), GraphicsContext3D::RenderDirectlyToHostWindow);
    }

    virtual void afterTest()
    {
    }
};

TEST_F(CCSingleThreadProxyTestDidAddAnimationBeforeInitializingLayerRenderer, runSingleThread)
{
    runTest(false);
}
