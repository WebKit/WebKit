/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "GraphicsLayerChromium.h"

#include "CCAnimationTestCommon.h"
#include "CompositorFakeWebGraphicsContext3D.h"
#include "GraphicsContext3D.h"
#include "GraphicsContext3DPrivate.h"
#include "GraphicsLayer.h"
#include "LayerChromium.h"
#include "Matrix3DTransformOperation.h"
#include "RotateTransformOperation.h"
#include "TranslateTransformOperation.h"
#include "WebCompositor.h"

#include "cc/CCLayerTreeHost.h"
#include "cc/CCLayerTreeHostImpl.h"
#include "cc/CCSingleThreadProxy.h"

#include <gtest/gtest.h>
#include <public/WebGraphicsContext3D.h>
#include <wtf/PassOwnPtr.h>

using namespace WebCore;
using namespace WebKit;
using namespace WebKitTests;

namespace {

class MockGraphicsLayerClient : public GraphicsLayerClient {
  public:
    virtual void notifyAnimationStarted(const GraphicsLayer*, double time) OVERRIDE { }
    virtual void notifySyncRequired(const GraphicsLayer*) OVERRIDE { }
    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect& inClip) OVERRIDE { }
    virtual bool showDebugBorders(const GraphicsLayer*) const OVERRIDE { return false; }
    virtual bool showRepaintCounter(const GraphicsLayer*) const OVERRIDE { return false; }
    virtual float deviceScaleFactor() const OVERRIDE { return 2; }
};

class MockLayerTreeHostClient : public CCLayerTreeHostClient {
public:
    virtual void willBeginFrame() OVERRIDE { }
    virtual void didBeginFrame() OVERRIDE { }
    virtual void updateAnimations(double frameBeginTime) OVERRIDE { }
    virtual void layout() OVERRIDE { }
    virtual void applyScrollAndScale(const IntSize& scrollDelta, float pageScale) OVERRIDE { }
    virtual PassOwnPtr<WebGraphicsContext3D> createContext3D() OVERRIDE
    {
        return CompositorFakeWebGraphicsContext3D::create(WebGraphicsContext3D::Attributes());
    }
    virtual void didRecreateContext(bool success) OVERRIDE { }
    virtual void willCommit() OVERRIDE { }
    virtual void didCommit() OVERRIDE { }
    virtual void didCommitAndDrawFrame() OVERRIDE { }
    virtual void didCompleteSwapBuffers() OVERRIDE { }
    virtual void scheduleComposite() OVERRIDE { }
};

class MockLayerTreeHost : public CCLayerTreeHost {
public:
    static PassOwnPtr<MockLayerTreeHost> create()
    {
        CCLayerTreeSettings settings;
        OwnPtr<MockLayerTreeHost> layerTreeHost(adoptPtr(new MockLayerTreeHost(new MockLayerTreeHostClient(), settings)));
        bool success = layerTreeHost->initialize();
        EXPECT_TRUE(success);
        layerTreeHost->setRootLayer(LayerChromium::create());
        layerTreeHost->setViewportSize(IntSize(1, 1));
        return layerTreeHost.release();
    }

    virtual PassOwnPtr<CCLayerTreeHostImpl> createLayerTreeHostImpl(CCLayerTreeHostImplClient* client)
    {
        return CCLayerTreeHostImpl::create(settings(), client);
    }

private:
    MockLayerTreeHost(CCLayerTreeHostClient* client, const CCLayerTreeSettings& settings)
        : CCLayerTreeHost(client, settings)
    {
    }
};

class GraphicsLayerChromiumTest : public testing::Test {
public:
    GraphicsLayerChromiumTest()
    {
        // For these tests, we will enable threaded animations.
        WebCompositor::setAcceleratedAnimationEnabled(true);
        WebCompositor::initialize(0);
        m_graphicsLayer = static_pointer_cast<GraphicsLayerChromium>(GraphicsLayer::create(&m_client));
        m_platformLayer = static_cast<LayerChromium*>(m_graphicsLayer->platformLayer());
        m_layerTreeHost = MockLayerTreeHost::create();
        m_platformLayer->setLayerTreeHost(m_layerTreeHost.get());
    }

    virtual ~GraphicsLayerChromiumTest()
    {
        m_graphicsLayer.clear();
        m_layerTreeHost.clear();
        WebCompositor::shutdown();
    }

protected:
    static void expectTranslateX(double translateX, const WebTransformationMatrix& matrix)
    {
        EXPECT_FLOAT_EQ(translateX, matrix.m41());
    }

    LayerChromium* m_platformLayer;
    OwnPtr<GraphicsLayerChromium> m_graphicsLayer;
    OwnPtr<CCLayerTreeHost> m_layerTreeHost;

private:
    MockGraphicsLayerClient m_client;
    DebugScopedSetMainThread m_main;
};

TEST_F(GraphicsLayerChromiumTest, updateLayerPreserves3DWithAnimations)
{
    ASSERT_FALSE(m_platformLayer->hasActiveAnimation());

    OwnPtr<CCActiveAnimation> floatAnimation(CCActiveAnimation::create(adoptPtr(new FakeFloatAnimationCurve), 0, 1, CCActiveAnimation::Opacity));
    m_platformLayer->layerAnimationController()->addAnimation(floatAnimation.release());

    ASSERT_TRUE(m_platformLayer->hasActiveAnimation());

    m_graphicsLayer->setPreserves3D(true);

    m_platformLayer = static_cast<LayerChromium*>(m_graphicsLayer->platformLayer());
    ASSERT_TRUE(m_platformLayer);

    ASSERT_TRUE(m_platformLayer->hasActiveAnimation());
    m_platformLayer->removeAnimation(0);
    ASSERT_FALSE(m_platformLayer->hasActiveAnimation());

    m_graphicsLayer->setPreserves3D(false);

    m_platformLayer = static_cast<LayerChromium*>(m_graphicsLayer->platformLayer());
    ASSERT_TRUE(m_platformLayer);

    ASSERT_FALSE(m_platformLayer->hasActiveAnimation());
}

TEST_F(GraphicsLayerChromiumTest, createOpacityAnimation)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyOpacity);
    values.insert(new FloatAnimationValue(0, 0));
    values.insert(new FloatAnimationValue(duration, 1));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);

    IntSize boxSize;
    bool addedAnimation = m_graphicsLayer->addAnimation(values, boxSize, animation.get(), "", 0);

    EXPECT_TRUE(addedAnimation);

    EXPECT_TRUE(m_platformLayer->layerAnimationController()->hasActiveAnimation());

    CCActiveAnimation* activeAnimation = m_platformLayer->layerAnimationController()->getActiveAnimation(CCActiveAnimation::Opacity);
    EXPECT_TRUE(activeAnimation);

    EXPECT_EQ(1, activeAnimation->iterations());
    EXPECT_EQ(CCActiveAnimation::Opacity, activeAnimation->targetProperty());

    EXPECT_EQ(CCAnimationCurve::Float, activeAnimation->curve()->type());

    const CCFloatAnimationCurve* curve = activeAnimation->curve()->toFloatAnimationCurve();
    EXPECT_TRUE(curve);

    EXPECT_EQ(0, curve->getValue(0));
    EXPECT_EQ(1, curve->getValue(duration));
}

TEST_F(GraphicsLayerChromiumTest, createTransformAnimation)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(2, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(0, &operations1));

    TransformOperations operations2;
    operations2.operations().append(TranslateTransformOperation::create(Length(4, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(duration, &operations2));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);

    IntSize boxSize;
    m_graphicsLayer->addAnimation(values, boxSize, animation.get(), "", 0);

    EXPECT_TRUE(m_platformLayer->layerAnimationController()->hasActiveAnimation());

    CCActiveAnimation* activeAnimation = m_platformLayer->layerAnimationController()->getActiveAnimation(CCActiveAnimation::Transform);
    EXPECT_TRUE(activeAnimation);

    EXPECT_EQ(1, activeAnimation->iterations());
    EXPECT_EQ(CCActiveAnimation::Transform, activeAnimation->targetProperty());

    EXPECT_EQ(CCAnimationCurve::Transform, activeAnimation->curve()->type());

    const CCTransformAnimationCurve* curve = activeAnimation->curve()->toTransformAnimationCurve();
    EXPECT_TRUE(curve);

    expectTranslateX(2, curve->getValue(0));
    expectTranslateX(4, curve->getValue(duration));
}

TEST_F(GraphicsLayerChromiumTest, createTransformAnimationWithBigRotation)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(RotateTransformOperation::create(0, TransformOperation::ROTATE));
    values.insert(new TransformAnimationValue(0, &operations1));

    TransformOperations operations2;
    operations2.operations().append(RotateTransformOperation::create(270, TransformOperation::ROTATE));
    values.insert(new TransformAnimationValue(duration, &operations2));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);

    IntSize boxSize;
    m_graphicsLayer->addAnimation(values, boxSize, animation.get(), "", 0);

    EXPECT_FALSE(m_platformLayer->layerAnimationController()->hasActiveAnimation());
}

TEST_F(GraphicsLayerChromiumTest, createTransformAnimationWithRotationInvolvingNegativeAngles)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(RotateTransformOperation::create(-330, TransformOperation::ROTATE));
    values.insert(new TransformAnimationValue(0, &operations1));

    TransformOperations operations2;
    operations2.operations().append(RotateTransformOperation::create(-320, TransformOperation::ROTATE));
    values.insert(new TransformAnimationValue(duration, &operations2));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);

    IntSize boxSize;
    m_graphicsLayer->addAnimation(values, boxSize, animation.get(), "", 0);

    EXPECT_TRUE(m_platformLayer->layerAnimationController()->hasActiveAnimation());
}

TEST_F(GraphicsLayerChromiumTest, createTransformAnimationWithSmallRotationInvolvingLargeAngles)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(RotateTransformOperation::create(270, TransformOperation::ROTATE));
    values.insert(new TransformAnimationValue(0, &operations1));

    TransformOperations operations2;
    operations2.operations().append(RotateTransformOperation::create(360, TransformOperation::ROTATE));
    values.insert(new TransformAnimationValue(duration, &operations2));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);

    IntSize boxSize;
    m_graphicsLayer->addAnimation(values, boxSize, animation.get(), "", 0);

    EXPECT_TRUE(m_platformLayer->layerAnimationController()->hasActiveAnimation());
}

TEST_F(GraphicsLayerChromiumTest, createTransformAnimationWithSingularMatrix)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformationMatrix matrix1;
    TransformOperations operations1;
    operations1.operations().append(Matrix3DTransformOperation::create(matrix1));
    values.insert(new TransformAnimationValue(0, &operations1));

    TransformationMatrix matrix2;
    matrix2.setM11(0);
    TransformOperations operations2;
    operations2.operations().append(Matrix3DTransformOperation::create(matrix2));
    values.insert(new TransformAnimationValue(duration, &operations2));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);

    IntSize boxSize;
    m_graphicsLayer->addAnimation(values, boxSize, animation.get(), "", 0);

    EXPECT_FALSE(m_platformLayer->layerAnimationController()->hasActiveAnimation());
}

TEST_F(GraphicsLayerChromiumTest, createReversedAnimation)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(2, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(0, &operations1));

    TransformOperations operations2;
    operations2.operations().append(TranslateTransformOperation::create(Length(4, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(duration, &operations2));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);
    animation->setDirection(Animation::AnimationDirectionReverse);

    IntSize boxSize;
    m_graphicsLayer->addAnimation(values, boxSize, animation.get(), "", 0);

    EXPECT_TRUE(m_platformLayer->layerAnimationController()->hasActiveAnimation());

    CCActiveAnimation* activeAnimation = m_platformLayer->layerAnimationController()->getActiveAnimation(CCActiveAnimation::Transform);
    EXPECT_TRUE(activeAnimation);

    EXPECT_EQ(1, activeAnimation->iterations());
    EXPECT_EQ(CCActiveAnimation::Transform, activeAnimation->targetProperty());

    EXPECT_EQ(CCAnimationCurve::Transform, activeAnimation->curve()->type());

    const CCTransformAnimationCurve* curve = activeAnimation->curve()->toTransformAnimationCurve();
    EXPECT_TRUE(curve);

    expectTranslateX(4, curve->getValue(0));
    expectTranslateX(2, curve->getValue(duration));
}

TEST_F(GraphicsLayerChromiumTest, createAlternatingAnimation)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(2, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(0, &operations1));

    TransformOperations operations2;
    operations2.operations().append(TranslateTransformOperation::create(Length(4, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(duration, &operations2));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);
    animation->setDirection(Animation::AnimationDirectionAlternate);
    animation->setIterationCount(2);

    IntSize boxSize;
    m_graphicsLayer->addAnimation(values, boxSize, animation.get(), "", 0);

    EXPECT_TRUE(m_platformLayer->layerAnimationController()->hasActiveAnimation());

    CCActiveAnimation* activeAnimation = m_platformLayer->layerAnimationController()->getActiveAnimation(CCActiveAnimation::Transform);
    EXPECT_TRUE(activeAnimation);
    EXPECT_TRUE(activeAnimation->alternatesDirection());

    EXPECT_EQ(2, activeAnimation->iterations());
    EXPECT_EQ(CCActiveAnimation::Transform, activeAnimation->targetProperty());

    EXPECT_EQ(CCAnimationCurve::Transform, activeAnimation->curve()->type());

    const CCTransformAnimationCurve* curve = activeAnimation->curve()->toTransformAnimationCurve();
    EXPECT_TRUE(curve);

    expectTranslateX(2, curve->getValue(0));
    expectTranslateX(4, curve->getValue(duration));
}

TEST_F(GraphicsLayerChromiumTest, createReversedAlternatingAnimation)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(2, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(0, &operations1));

    TransformOperations operations2;
    operations2.operations().append(TranslateTransformOperation::create(Length(4, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(duration, &operations2));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);
    animation->setDirection(Animation::AnimationDirectionAlternateReverse);
    animation->setIterationCount(2);

    IntSize boxSize;
    m_graphicsLayer->addAnimation(values, boxSize, animation.get(), "", 0);

    EXPECT_TRUE(m_platformLayer->layerAnimationController()->hasActiveAnimation());

    CCActiveAnimation* activeAnimation = m_platformLayer->layerAnimationController()->getActiveAnimation(CCActiveAnimation::Transform);
    EXPECT_TRUE(activeAnimation);
    EXPECT_TRUE(activeAnimation->alternatesDirection());

    EXPECT_EQ(2, activeAnimation->iterations());
    EXPECT_EQ(CCActiveAnimation::Transform, activeAnimation->targetProperty());

    EXPECT_EQ(CCAnimationCurve::Transform, activeAnimation->curve()->type());

    const CCTransformAnimationCurve* curve = activeAnimation->curve()->toTransformAnimationCurve();
    EXPECT_TRUE(curve);

    expectTranslateX(4, curve->getValue(0));
    expectTranslateX(2, curve->getValue(duration));
}

TEST_F(GraphicsLayerChromiumTest, shouldStartWithCorrectContentsScale)
{
    EXPECT_EQ(2, m_platformLayer->contentsScale());
}

} // namespace
