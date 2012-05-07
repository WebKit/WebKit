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

#include "cc/CCLayerTreeHostCommon.h"

#include "CCAnimationTestCommon.h"
#include "CCLayerTreeTestCommon.h"
#include "LayerChromium.h"
#include "TransformationMatrix.h"
#include "TranslateTransformOperation.h"
#include "cc/CCLayerAnimationController.h"
#include "cc/CCMathUtil.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebCore;
using namespace WebKitTests;

namespace {

void setLayerPropertiesForTesting(LayerChromium* layer, const TransformationMatrix& transform, const TransformationMatrix& sublayerTransform, const FloatPoint& anchor, const FloatPoint& position, const IntSize& bounds, bool preserves3D)
{
    layer->setTransform(transform);
    layer->setSublayerTransform(sublayerTransform);
    layer->setAnchorPoint(anchor);
    layer->setPosition(position);
    layer->setBounds(bounds);
    layer->setPreserves3D(preserves3D);
}

void executeCalculateDrawTransformsAndVisibility(LayerChromium* rootLayer)
{
    TransformationMatrix identityMatrix;
    Vector<RefPtr<LayerChromium> > dummyRenderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;
    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(rootLayer, rootLayer, identityMatrix, identityMatrix, dummyRenderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);
}

TransformationMatrix remove3DComponentOfMatrix(const TransformationMatrix& mat)
{
    TransformationMatrix ret = mat;
    ret.setM13(0);
    ret.setM23(0);
    ret.setM31(0);
    ret.setM32(0);
    ret.setM33(1);
    ret.setM34(0);
    ret.setM43(0);
    return ret;
}

class LayerChromiumWithForcedDrawsContent : public LayerChromium {
public:
    LayerChromiumWithForcedDrawsContent()
        : LayerChromium()
    {
    }

    virtual bool drawsContent() const OVERRIDE { return true; }
};

TEST(CCLayerTreeHostCommonTest, verifyTransformsForNoOpLayer)
{
    // Sanity check: For layers positioned at zero, with zero size,
    // and with identity transforms, then the drawTransform,
    // screenSpaceTransform, and the hierarchy passed on to children
    // layers should also be identity transforms.

    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromium> child = LayerChromium::create();
    RefPtr<LayerChromium> grandChild = LayerChromium::create();
    parent->createRenderSurface();
    parent->addChild(child);
    child->addChild(grandChild);

    TransformationMatrix identityMatrix;
    setLayerPropertiesForTesting(parent.get(), identityMatrix, identityMatrix, FloatPoint(0.0f, 0.0f), FloatPoint(0.0f, 0.0f), IntSize(0, 0), false);
    setLayerPropertiesForTesting(child.get(), identityMatrix, identityMatrix, FloatPoint(0.0f, 0.0f), FloatPoint(0.0f, 0.0f), IntSize(0, 0), false);
    setLayerPropertiesForTesting(grandChild.get(), identityMatrix, identityMatrix, FloatPoint(0.0f, 0.0f), FloatPoint(0.0f, 0.0f), IntSize(0, 0), false);

    executeCalculateDrawTransformsAndVisibility(parent.get());

    EXPECT_TRANSFORMATION_MATRIX_EQ(identityMatrix, parent->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(identityMatrix, parent->screenSpaceTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(identityMatrix, child->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(identityMatrix, child->screenSpaceTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(identityMatrix, grandChild->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(identityMatrix, grandChild->screenSpaceTransform());
}

TEST(CCLayerTreeHostCommonTest, verifyTransformsForSingleLayer)
{
    // NOTE CAREFULLY:
    // LayerChromium::position is actually the sum of anchorPoint (in pixel space) and actual position. Because of this, the
    // value of LayerChromium::position() changes if the anchor changes, even though the layer is not actually located in a
    // different position. When we initialize layers for testing here, we need to initialize that unintutive position value.

    TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> layer = LayerChromium::create();
    layer->createRenderSurface();

    // Case 1: setting the sublayer transform should not affect this layer's draw transform or screen-space transform.
    TransformationMatrix arbitraryTranslation;
    arbitraryTranslation.translate(10.0, 20.0);
    setLayerPropertiesForTesting(layer.get(), identityMatrix, arbitraryTranslation, FloatPoint(0.0f, 0.0f), FloatPoint(0.0f, 0.0f), IntSize(0, 0), false);
    executeCalculateDrawTransformsAndVisibility(layer.get());
    EXPECT_TRANSFORMATION_MATRIX_EQ(identityMatrix, layer->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(identityMatrix, layer->screenSpaceTransform());

    // Case 2: setting the bounds of the layer should result in a draw transform that translates to half the width and height.
    //         The screen-space transform should remain as the identity, because it does not deal with transforming to/from the center of the layer.
    TransformationMatrix translationToCenter;
    translationToCenter.translate(5.0, 6.0);
    setLayerPropertiesForTesting(layer.get(), identityMatrix, identityMatrix, FloatPoint(0.0f, 0.0f), FloatPoint(0.0f, 0.0f), IntSize(10, 12), false);
    executeCalculateDrawTransformsAndVisibility(layer.get());
    EXPECT_TRANSFORMATION_MATRIX_EQ(translationToCenter, layer->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(identityMatrix, layer->screenSpaceTransform());

    // Case 3: The anchor point by itself (without a layer transform) should have no effect on the transforms.
    setLayerPropertiesForTesting(layer.get(), identityMatrix, identityMatrix, FloatPoint(0.25f, 0.25f), FloatPoint(2.5f, 3.0f), IntSize(10, 12), false);
    executeCalculateDrawTransformsAndVisibility(layer.get());
    EXPECT_TRANSFORMATION_MATRIX_EQ(translationToCenter, layer->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(identityMatrix, layer->screenSpaceTransform());

    // Case 4: A change in "actual" position affects both the draw transform and screen space transform.
    TransformationMatrix positionTransform;
    positionTransform.translate(0.0, 1.2);
    setLayerPropertiesForTesting(layer.get(), identityMatrix, identityMatrix, FloatPoint(0.25f, 0.25f), FloatPoint(2.5f, 4.2f), IntSize(10, 12), false);
    executeCalculateDrawTransformsAndVisibility(layer.get());
    EXPECT_TRANSFORMATION_MATRIX_EQ(positionTransform * translationToCenter, layer->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(positionTransform, layer->screenSpaceTransform());

    // Case 5: In the correct sequence of transforms, the layer transform should pre-multiply the translationToCenter. This is easily tested by
    //         using a scale transform, because scale and translation are not commutative.
    TransformationMatrix layerTransform;
    layerTransform.scale3d(2.0, 2.0, 1.0);
    setLayerPropertiesForTesting(layer.get(), layerTransform, identityMatrix, FloatPoint(0.0f, 0.0f), FloatPoint(0.0f, 0.0f), IntSize(10, 12), false);
    executeCalculateDrawTransformsAndVisibility(layer.get());
    EXPECT_TRANSFORMATION_MATRIX_EQ(layerTransform * translationToCenter, layer->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(layerTransform, layer->screenSpaceTransform());

    // Case 6: The layer transform should occur with respect to the anchor point.
    TransformationMatrix translationToAnchor;
    translationToAnchor.translate(5.0, 0.0);
    TransformationMatrix expectedResult = translationToAnchor * layerTransform * translationToAnchor.inverse();
    setLayerPropertiesForTesting(layer.get(), layerTransform, identityMatrix, FloatPoint(0.5f, 0.0f), FloatPoint(5.0f, 0.0f), IntSize(10, 12), false);
    executeCalculateDrawTransformsAndVisibility(layer.get());
    EXPECT_TRANSFORMATION_MATRIX_EQ(expectedResult * translationToCenter, layer->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(expectedResult, layer->screenSpaceTransform());

    // Case 7: Verify that position pre-multiplies the layer transform.
    //         The current implementation of calculateDrawTransformsAndVisibility does this implicitly, but it is
    //         still worth testing to detect accidental regressions.
    expectedResult = positionTransform * translationToAnchor * layerTransform * translationToAnchor.inverse();
    setLayerPropertiesForTesting(layer.get(), layerTransform, identityMatrix, FloatPoint(0.5f, 0.0f), FloatPoint(5.0f, 1.2f), IntSize(10, 12), false);
    executeCalculateDrawTransformsAndVisibility(layer.get());
    EXPECT_TRANSFORMATION_MATRIX_EQ(expectedResult * translationToCenter, layer->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(expectedResult, layer->screenSpaceTransform());
}

TEST(CCLayerTreeHostCommonTest, verifyTransformsForSimpleHierarchy)
{
    TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromium> child = LayerChromium::create();
    RefPtr<LayerChromium> grandChild = LayerChromium::create();
    parent->createRenderSurface();
    parent->addChild(child);
    child->addChild(grandChild);

    // Case 1: parent's anchorPoint should not affect child or grandChild.
    TransformationMatrix childTranslationToCenter, grandChildTranslationToCenter;
    childTranslationToCenter.translate(8.0, 9.0);
    grandChildTranslationToCenter.translate(38.0, 39.0);
    setLayerPropertiesForTesting(parent.get(), identityMatrix, identityMatrix, FloatPoint(0.25f, 0.25f), FloatPoint(2.5f, 3.0f), IntSize(10, 12), false);
    setLayerPropertiesForTesting(child.get(), identityMatrix, identityMatrix, FloatPoint(0.0f, 0.0f), FloatPoint(0.0f, 0.0f), IntSize(16, 18), false);
    setLayerPropertiesForTesting(grandChild.get(), identityMatrix, identityMatrix, FloatPoint(0.0f, 0.0f), FloatPoint(0.0f, 0.0f), IntSize(76, 78), false);
    executeCalculateDrawTransformsAndVisibility(parent.get());
    EXPECT_TRANSFORMATION_MATRIX_EQ(childTranslationToCenter, child->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(identityMatrix, child->screenSpaceTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(grandChildTranslationToCenter, grandChild->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(identityMatrix, grandChild->screenSpaceTransform());

    // Case 2: parent's position affects child and grandChild.
    TransformationMatrix parentPositionTransform;
    parentPositionTransform.translate(0.0, 1.2);
    setLayerPropertiesForTesting(parent.get(), identityMatrix, identityMatrix, FloatPoint(0.25f, 0.25f), FloatPoint(2.5f, 4.2f), IntSize(10, 12), false);
    setLayerPropertiesForTesting(child.get(), identityMatrix, identityMatrix, FloatPoint(0.0f, 0.0f), FloatPoint(0.0f, 0.0f), IntSize(16, 18), false);
    setLayerPropertiesForTesting(grandChild.get(), identityMatrix, identityMatrix, FloatPoint(0.0f, 0.0f), FloatPoint(0.0f, 0.0f), IntSize(76, 78), false);
    executeCalculateDrawTransformsAndVisibility(parent.get());
    EXPECT_TRANSFORMATION_MATRIX_EQ(parentPositionTransform * childTranslationToCenter, child->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(parentPositionTransform, child->screenSpaceTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(parentPositionTransform * grandChildTranslationToCenter, grandChild->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(parentPositionTransform, grandChild->screenSpaceTransform());

    // Case 3: parent's local transform affects child and grandchild
    TransformationMatrix parentLayerTransform;
    parentLayerTransform.scale3d(2.0, 2.0, 1.0);
    TransformationMatrix parentTranslationToAnchor;
    parentTranslationToAnchor.translate(2.5, 3.0);
    TransformationMatrix parentCompositeTransform = parentTranslationToAnchor * parentLayerTransform * parentTranslationToAnchor.inverse();
    setLayerPropertiesForTesting(parent.get(), parentLayerTransform, identityMatrix, FloatPoint(0.25f, 0.25f), FloatPoint(2.5f, 3.0f), IntSize(10, 12), false);
    setLayerPropertiesForTesting(child.get(), identityMatrix, identityMatrix, FloatPoint(0.0f, 0.0f), FloatPoint(0.0f, 0.0f), IntSize(16, 18), false);
    setLayerPropertiesForTesting(grandChild.get(), identityMatrix, identityMatrix, FloatPoint(0.0f, 0.0f), FloatPoint(0.0f, 0.0f), IntSize(76, 78), false);
    executeCalculateDrawTransformsAndVisibility(parent.get());
    EXPECT_TRANSFORMATION_MATRIX_EQ(parentCompositeTransform * childTranslationToCenter, child->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(parentCompositeTransform, child->screenSpaceTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(parentCompositeTransform * grandChildTranslationToCenter, grandChild->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(parentCompositeTransform, grandChild->screenSpaceTransform());

    // Case 4: parent's sublayerMatrix affects child and grandchild
    //         scaling is used here again so that the correct sequence of transforms is properly tested.
    //         Note that preserves3D is false, but the sublayer matrix should retain its 3D properties when given to child.
    //         But then, the child also does not preserve3D. When it gives its hierarchy to the grandChild, it should be flattened to 2D.
    TransformationMatrix parentSublayerMatrix;
    parentSublayerMatrix.scale3d(10.0, 10.0, 3.3);
    TransformationMatrix parentTranslationToCenter;
    parentTranslationToCenter.translate(5.0, 6.0);
    // Sublayer matrix is applied to the center of the parent layer.
    parentCompositeTransform = parentTranslationToAnchor * parentLayerTransform * parentTranslationToAnchor.inverse()
            * parentTranslationToCenter * parentSublayerMatrix * parentTranslationToCenter.inverse();
    TransformationMatrix flattenedCompositeTransform = remove3DComponentOfMatrix(parentCompositeTransform);
    setLayerPropertiesForTesting(parent.get(), parentLayerTransform, parentSublayerMatrix, FloatPoint(0.25f, 0.25f), FloatPoint(2.5f, 3.0f), IntSize(10, 12), false);
    setLayerPropertiesForTesting(child.get(), identityMatrix, identityMatrix, FloatPoint(0.0f, 0.0f), FloatPoint(0.0f, 0.0f), IntSize(16, 18), false);
    setLayerPropertiesForTesting(grandChild.get(), identityMatrix, identityMatrix, FloatPoint(0.0f, 0.0f), FloatPoint(0.0f, 0.0f), IntSize(76, 78), false);
    executeCalculateDrawTransformsAndVisibility(parent.get());
    EXPECT_TRANSFORMATION_MATRIX_EQ(parentCompositeTransform * childTranslationToCenter, child->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(parentCompositeTransform, child->screenSpaceTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(flattenedCompositeTransform * grandChildTranslationToCenter, grandChild->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(flattenedCompositeTransform, grandChild->screenSpaceTransform());

    // Case 5: same as Case 4, except that child does preserve 3D, so the grandChild should receive the non-flattened composite transform.
    //
    setLayerPropertiesForTesting(parent.get(), parentLayerTransform, parentSublayerMatrix, FloatPoint(0.25f, 0.25f), FloatPoint(2.5f, 3.0f), IntSize(10, 12), false);
    setLayerPropertiesForTesting(child.get(), identityMatrix, identityMatrix, FloatPoint(0.0f, 0.0f), FloatPoint(0.0f, 0.0f), IntSize(16, 18), true);
    setLayerPropertiesForTesting(grandChild.get(), identityMatrix, identityMatrix, FloatPoint(0.0f, 0.0f), FloatPoint(0.0f, 0.0f), IntSize(76, 78), false);
    executeCalculateDrawTransformsAndVisibility(parent.get());
    EXPECT_TRANSFORMATION_MATRIX_EQ(parentCompositeTransform * childTranslationToCenter, child->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(parentCompositeTransform, child->screenSpaceTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(parentCompositeTransform * grandChildTranslationToCenter, grandChild->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(parentCompositeTransform, grandChild->screenSpaceTransform());
}

TEST(CCLayerTreeHostCommonTest, verifyTransformsForSingleRenderSurface)
{
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromium> child = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> grandChild = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(child);
    child->addChild(grandChild);

    // Child is set up so that a new render surface should be created.
    child->setOpacity(0.5f);

    TransformationMatrix identityMatrix;
    TransformationMatrix parentLayerTransform;
    parentLayerTransform.scale3d(2.0, 2.0, 1.0);
    TransformationMatrix parentTranslationToAnchor;
    parentTranslationToAnchor.translate(2.5, 3.0);
    TransformationMatrix parentSublayerMatrix;
    parentSublayerMatrix.scale3d(10.0, 10.0, 3.3);
    TransformationMatrix parentTranslationToCenter;
    parentTranslationToCenter.translate(5.0, 6.0);
    TransformationMatrix parentCompositeTransform = parentTranslationToAnchor * parentLayerTransform * parentTranslationToAnchor.inverse()
            * parentTranslationToCenter * parentSublayerMatrix * parentTranslationToCenter.inverse();
    TransformationMatrix childTranslationToCenter;
    childTranslationToCenter.translate(8.0, 9.0);

    // Child's render surface should not exist yet.
    ASSERT_FALSE(child->renderSurface());

    setLayerPropertiesForTesting(parent.get(), parentLayerTransform, parentSublayerMatrix, FloatPoint(0.25f, 0.25f), FloatPoint(2.5f, 3.0f), IntSize(10, 12), false);
    setLayerPropertiesForTesting(child.get(), identityMatrix, identityMatrix, FloatPoint(0.0f, 0.0f), FloatPoint(0.0f, 0.0f), IntSize(16, 18), false);
    setLayerPropertiesForTesting(grandChild.get(), identityMatrix, identityMatrix, FloatPoint(0.0f, 0.0f), FloatPoint(-0.5f, -0.5f), IntSize(1, 1), false);
    executeCalculateDrawTransformsAndVisibility(parent.get());

    // Render surface should have been created now.
    ASSERT_TRUE(child->renderSurface());
    ASSERT_EQ(child->renderSurface(), child->targetRenderSurface());

    // The child layer's draw transform should refer to its new render surface; they only differ by a translation to center.
    // The screen-space transform, however, should still refer to the root.
    EXPECT_TRANSFORMATION_MATRIX_EQ(childTranslationToCenter, child->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(parentCompositeTransform, child->screenSpaceTransform());

    // Without clipping, the origin transform and draw transform (in this particular case) should be the same.
    EXPECT_TRANSFORMATION_MATRIX_EQ(parentCompositeTransform, child->targetRenderSurface()->originTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(parentCompositeTransform, child->targetRenderSurface()->drawTransform());

    // The screen space is the same as the target since the child surface draws into the root.
    EXPECT_TRANSFORMATION_MATRIX_EQ(parentCompositeTransform, child->targetRenderSurface()->screenSpaceTransform());
}

TEST(CCLayerTreeHostCommonTest, verifyTransformsForReplica)
{
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromium> child = LayerChromium::create();
    RefPtr<LayerChromium> childReplica = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> grandChild = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(child);
    child->addChild(grandChild);
    child->setReplicaLayer(childReplica.get());

    // Child is set up so that a new render surface should be created.
    child->setOpacity(0.5f);

    TransformationMatrix identityMatrix;
    TransformationMatrix parentLayerTransform;
    parentLayerTransform.scale3d(2.0, 2.0, 1.0);
    TransformationMatrix parentTranslationToAnchor;
    parentTranslationToAnchor.translate(2.5, 3.0);
    TransformationMatrix parentSublayerMatrix;
    parentSublayerMatrix.scale3d(10.0, 10.0, 3.3);
    TransformationMatrix parentTranslationToCenter;
    parentTranslationToCenter.translate(5.0, 6.0);
    TransformationMatrix parentCompositeTransform = parentTranslationToAnchor * parentLayerTransform * parentTranslationToAnchor.inverse()
            * parentTranslationToCenter * parentSublayerMatrix * parentTranslationToCenter.inverse();
    TransformationMatrix childTranslationToCenter;
    childTranslationToCenter.translate(8.0, 9.0);
    TransformationMatrix replicaLayerTransform;
    replicaLayerTransform.scale3d(3.0, 3.0, 1.0);
    TransformationMatrix replicaCompositeTransform = parentCompositeTransform * replicaLayerTransform;

    // Child's render surface should not exist yet.
    ASSERT_FALSE(child->renderSurface());

    setLayerPropertiesForTesting(parent.get(), parentLayerTransform, parentSublayerMatrix, FloatPoint(0.25f, 0.25f), FloatPoint(2.5f, 3.0f), IntSize(10, 12), false);
    setLayerPropertiesForTesting(child.get(), identityMatrix, identityMatrix, FloatPoint(0.0f, 0.0f), FloatPoint(0.0f, 0.0f), IntSize(16, 18), false);
    setLayerPropertiesForTesting(grandChild.get(), identityMatrix, identityMatrix, FloatPoint(0.0f, 0.0f), FloatPoint(-0.5f, -0.5f), IntSize(1, 1), false);
    setLayerPropertiesForTesting(childReplica.get(), replicaLayerTransform, identityMatrix, FloatPoint(0.0f, 0.0f), FloatPoint(0.0f, 0.0f), IntSize(0, 0), false);
    executeCalculateDrawTransformsAndVisibility(parent.get());

    // Render surface should have been created now.
    ASSERT_TRUE(child->renderSurface());
    ASSERT_EQ(child->renderSurface(), child->targetRenderSurface());

    EXPECT_TRANSFORMATION_MATRIX_EQ(replicaCompositeTransform, child->targetRenderSurface()->replicaOriginTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(replicaCompositeTransform, child->targetRenderSurface()->replicaScreenSpaceTransform());
}

TEST(CCLayerTreeHostCommonTest, verifyTransformsForRenderSurfaceHierarchy)
{
    // This test creates a more complex tree and verifies it all at once. This covers the following cases:
    //   - layers that are described w.r.t. a render surface: should have draw transforms described w.r.t. that surface
    //   - A render surface described w.r.t. an ancestor render surface: should have a draw transform described w.r.t. that ancestor surface
    //   - Replicas of a render surface are described w.r.t. the replica's transform around its anchor, along with the surface itself.
    //   - Sanity check on recursion: verify transforms of layers described w.r.t. a render surface that is described w.r.t. an ancestor render surface.
    //   - verifying that each layer has a reference to the correct renderSurface and targetRenderSurface values.

    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromium> renderSurface1 = LayerChromium::create();
    RefPtr<LayerChromium> renderSurface2 = LayerChromium::create();
    RefPtr<LayerChromium> childOfRoot = LayerChromium::create();
    RefPtr<LayerChromium> childOfRS1 = LayerChromium::create();
    RefPtr<LayerChromium> childOfRS2 = LayerChromium::create();
    RefPtr<LayerChromium> replicaOfRS1 = LayerChromium::create();
    RefPtr<LayerChromium> replicaOfRS2 = LayerChromium::create();
    RefPtr<LayerChromium> grandChildOfRoot = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> grandChildOfRS1 = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> grandChildOfRS2 = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(renderSurface1);
    parent->addChild(childOfRoot);
    renderSurface1->addChild(childOfRS1);
    renderSurface1->addChild(renderSurface2);
    renderSurface2->addChild(childOfRS2);
    childOfRoot->addChild(grandChildOfRoot);
    childOfRS1->addChild(grandChildOfRS1);
    childOfRS2->addChild(grandChildOfRS2);
    renderSurface1->setReplicaLayer(replicaOfRS1.get());
    renderSurface2->setReplicaLayer(replicaOfRS2.get());

    // In combination with descendantDrawsContent, opacity != 1 forces the layer to have a new renderSurface.
    renderSurface1->setOpacity(0.5f);
    renderSurface2->setOpacity(0.33f);

    // All layers in the tree are initialized with an anchor at 2.5 and a size of (10,10).
    // matrix "A" is the composite layer transform used in all layers, centered about the anchor point
    // matrix "B" is the sublayer transform used in all layers, centered about the center position of the layer.
    // matrix "R" is the composite replica transform used in all replica layers.
    //
    // x component tests that layerTransform and sublayerTransform are done in the right order (translation and scale are noncommutative).
    // y component has a translation by 1.0 for every ancestor, which indicates the "depth" of the layer in the hierarchy.
    TransformationMatrix translationToAnchor;
    translationToAnchor.translate(2.5, 0.0);
    TransformationMatrix translationToCenter;
    translationToCenter.translate(5.0, 5.0);
    TransformationMatrix layerTransform;
    layerTransform.translate(1.0, 1.0);
    TransformationMatrix sublayerTransform;
    sublayerTransform.scale3d(10.0, 1.0, 1.0);
    TransformationMatrix replicaLayerTransform;
    replicaLayerTransform.scale3d(-2.0, 5.0, 1.0);

    TransformationMatrix A = translationToAnchor * layerTransform * translationToAnchor.inverse();
    TransformationMatrix B = translationToCenter * sublayerTransform * translationToCenter.inverse();
    TransformationMatrix R = A * translationToAnchor * replicaLayerTransform * translationToAnchor.inverse();

    setLayerPropertiesForTesting(parent.get(), layerTransform, sublayerTransform, FloatPoint(0.25f, 0.0f), FloatPoint(2.5f, 0.0f), IntSize(10, 10), false);
    setLayerPropertiesForTesting(renderSurface1.get(), layerTransform, sublayerTransform, FloatPoint(0.25f, 0.0f), FloatPoint(2.5f, 0.0f), IntSize(10, 10), false);
    setLayerPropertiesForTesting(renderSurface2.get(), layerTransform, sublayerTransform, FloatPoint(0.25f, 0.0f), FloatPoint(2.5f, 0.0f), IntSize(10, 10), false);
    setLayerPropertiesForTesting(childOfRoot.get(), layerTransform, sublayerTransform, FloatPoint(0.25f, 0.0f), FloatPoint(2.5f, 0.0f), IntSize(10, 10), false);
    setLayerPropertiesForTesting(childOfRS1.get(), layerTransform, sublayerTransform, FloatPoint(0.25f, 0.0f), FloatPoint(2.5f, 0.0f), IntSize(10, 10), false);
    setLayerPropertiesForTesting(childOfRS2.get(), layerTransform, sublayerTransform, FloatPoint(0.25f, 0.0f), FloatPoint(2.5f, 0.0f), IntSize(10, 10), false);
    setLayerPropertiesForTesting(grandChildOfRoot.get(), layerTransform, sublayerTransform, FloatPoint(0.25f, 0.0f), FloatPoint(2.5f, 0.0f), IntSize(10, 10), false);
    setLayerPropertiesForTesting(grandChildOfRS1.get(), layerTransform, sublayerTransform, FloatPoint(0.25f, 0.0f), FloatPoint(2.5f, 0.0f), IntSize(10, 10), false);
    setLayerPropertiesForTesting(grandChildOfRS2.get(), layerTransform, sublayerTransform, FloatPoint(0.25f, 0.0f), FloatPoint(2.5f, 0.0f), IntSize(10, 10), false);
    setLayerPropertiesForTesting(replicaOfRS1.get(), replicaLayerTransform, sublayerTransform, FloatPoint(), FloatPoint(2.5f, 0.0f), IntSize(), false);
    setLayerPropertiesForTesting(replicaOfRS2.get(), replicaLayerTransform, sublayerTransform, FloatPoint(), FloatPoint(2.5f, 0.0f), IntSize(), false);

    executeCalculateDrawTransformsAndVisibility(parent.get());

    // Only layers that are associated with render surfaces should have an actual renderSurface() value.
    //
    ASSERT_TRUE(parent->renderSurface());
    ASSERT_FALSE(childOfRoot->renderSurface());
    ASSERT_FALSE(grandChildOfRoot->renderSurface());

    ASSERT_TRUE(renderSurface1->renderSurface());
    ASSERT_FALSE(childOfRS1->renderSurface());
    ASSERT_FALSE(grandChildOfRS1->renderSurface());

    ASSERT_TRUE(renderSurface2->renderSurface());
    ASSERT_FALSE(childOfRS2->renderSurface());
    ASSERT_FALSE(grandChildOfRS2->renderSurface());

    // Verify all targetRenderSurface accessors
    //
    EXPECT_EQ(parent->renderSurface(), parent->targetRenderSurface());
    EXPECT_EQ(parent->renderSurface(), childOfRoot->targetRenderSurface());
    EXPECT_EQ(parent->renderSurface(), grandChildOfRoot->targetRenderSurface());

    EXPECT_EQ(renderSurface1->renderSurface(), renderSurface1->targetRenderSurface());
    EXPECT_EQ(renderSurface1->renderSurface(), childOfRS1->targetRenderSurface());
    EXPECT_EQ(renderSurface1->renderSurface(), grandChildOfRS1->targetRenderSurface());

    EXPECT_EQ(renderSurface2->renderSurface(), renderSurface2->targetRenderSurface());
    EXPECT_EQ(renderSurface2->renderSurface(), childOfRS2->targetRenderSurface());
    EXPECT_EQ(renderSurface2->renderSurface(), grandChildOfRS2->targetRenderSurface());

    // Verify layer draw transforms
    //  note that draw transforms are described with respect to the nearest ancestor render surface
    //  but screen space transforms are described with respect to the root.
    //
    EXPECT_TRANSFORMATION_MATRIX_EQ(A * translationToCenter, parent->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A * translationToCenter, childOfRoot->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A * B * A * translationToCenter, grandChildOfRoot->drawTransform());

    EXPECT_TRANSFORMATION_MATRIX_EQ(translationToCenter, renderSurface1->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(B * A * translationToCenter, childOfRS1->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(B * A * B * A * translationToCenter, grandChildOfRS1->drawTransform());

    EXPECT_TRANSFORMATION_MATRIX_EQ(translationToCenter, renderSurface2->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(B * A * translationToCenter, childOfRS2->drawTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(B * A * B * A * translationToCenter, grandChildOfRS2->drawTransform());

    // Verify layer screen-space transforms
    //
    EXPECT_TRANSFORMATION_MATRIX_EQ(A, parent->screenSpaceTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A, childOfRoot->screenSpaceTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A * B * A, grandChildOfRoot->screenSpaceTransform());

    EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A, renderSurface1->screenSpaceTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A * B * A, childOfRS1->screenSpaceTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A * B * A * B * A, grandChildOfRS1->screenSpaceTransform());

    EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A * B * A, renderSurface2->screenSpaceTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A * B * A * B * A, childOfRS2->screenSpaceTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A * B * A * B * A * B * A, grandChildOfRS2->screenSpaceTransform());

    // Verify render surface transforms.
    //
    // Origin transform of render surface 1 is described with respect to root.
    EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A, renderSurface1->renderSurface()->originTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * R, renderSurface1->renderSurface()->replicaOriginTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A, renderSurface1->renderSurface()->screenSpaceTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * R, renderSurface1->renderSurface()->replicaScreenSpaceTransform());
    // Origin transform of render surface 2 is described with respect to render surface 2.
    EXPECT_TRANSFORMATION_MATRIX_EQ(B * A, renderSurface2->renderSurface()->originTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(B * R, renderSurface2->renderSurface()->replicaOriginTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A * B * A, renderSurface2->renderSurface()->screenSpaceTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A * B * R, renderSurface2->renderSurface()->replicaScreenSpaceTransform());

    // Sanity check. If these fail there is probably a bug in the test itself.
    // It is expected that we correctly set up transforms so that the y-component of the screen-space transform
    // encodes the "depth" of the layer in the tree.
    EXPECT_FLOAT_EQ(1.0, parent->screenSpaceTransform().m42());
    EXPECT_FLOAT_EQ(2.0, childOfRoot->screenSpaceTransform().m42());
    EXPECT_FLOAT_EQ(3.0, grandChildOfRoot->screenSpaceTransform().m42());

    EXPECT_FLOAT_EQ(2.0, renderSurface1->screenSpaceTransform().m42());
    EXPECT_FLOAT_EQ(3.0, childOfRS1->screenSpaceTransform().m42());
    EXPECT_FLOAT_EQ(4.0, grandChildOfRS1->screenSpaceTransform().m42());

    EXPECT_FLOAT_EQ(3.0, renderSurface2->screenSpaceTransform().m42());
    EXPECT_FLOAT_EQ(4.0, childOfRS2->screenSpaceTransform().m42());
    EXPECT_FLOAT_EQ(5.0, grandChildOfRS2->screenSpaceTransform().m42());
}

TEST(CCLayerTreeHostCommonTest, verifyRenderSurfaceListForClipLayer)
{
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromium> renderSurface1 = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> child = adoptRef(new LayerChromiumWithForcedDrawsContent());
    renderSurface1->setOpacity(0.9f);

    const TransformationMatrix identityMatrix;
    setLayerPropertiesForTesting(renderSurface1.get(), identityMatrix, identityMatrix, FloatPoint::zero(), FloatPoint::zero(), IntSize(10, 10), false);
    setLayerPropertiesForTesting(child.get(), identityMatrix, identityMatrix, FloatPoint::zero(), FloatPoint(30, 30), IntSize(10, 10), false);

    parent->createRenderSurface();
    parent->setClipRect(IntRect(0, 0, 10, 10));
    parent->addChild(renderSurface1);
    renderSurface1->createRenderSurface();
    renderSurface1->addChild(child);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;
    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    // The child layer's content is entirely outside the parent's clip rect, so the intermediate
    // render surface should have been removed. Render surfaces without children or visible
    // content are unexpected at draw time (e.g. we might try to create a content texture of size 0).
    ASSERT_FALSE(renderSurface1->renderSurface());
    EXPECT_EQ(renderSurfaceLayerList.size(), 0U);
}

TEST(CCLayerTreeHostCommonTest, verifyRenderSurfaceListForTransparentChild)
{
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromium> renderSurface1 = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> child = adoptRef(new LayerChromiumWithForcedDrawsContent());
    renderSurface1->setOpacity(0);

    const TransformationMatrix identityMatrix;
    setLayerPropertiesForTesting(renderSurface1.get(), identityMatrix, identityMatrix, FloatPoint::zero(), FloatPoint::zero(), IntSize(10, 10), false);
    setLayerPropertiesForTesting(child.get(), identityMatrix, identityMatrix, FloatPoint::zero(), FloatPoint::zero(), IntSize(10, 10), false);

    parent->createRenderSurface();
    parent->addChild(renderSurface1);
    renderSurface1->createRenderSurface();
    renderSurface1->addChild(child);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;
    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    // Since the layer is transparent, renderSurface1->renderSurface() should not have gotten added anywhere.
    // Also, the drawable content rect should not have been extended by the children.
    EXPECT_EQ(parent->renderSurface()->layerList().size(), 0U);
    EXPECT_EQ(renderSurfaceLayerList.size(), 0U);
    EXPECT_EQ(parent->drawableContentRect(), IntRect());
}

TEST(CCLayerTreeHostCommonTest, verifyClipRectCullsRenderSurfaces)
{
    // The entire subtree of layers that are outside the clipRect should be culled away,
    // and should not affect the renderSurfaceLayerList.
    //
    // The test tree is set up as follows:
    //  - all layers except the leafNodes are forced to be a new renderSurface that have something to draw.
    //  - parent is a large container layer.
    //  - child has masksToBounds=true to cause clipping.
    //  - grandChild is positioned outside of the child's bounds
    //  - greatGrandChild is also kept outside child's bounds.
    //
    // In this configuration, grandChild and greatGrandChild are completely outside the
    // clipRect, and they should never get scheduled on the list of renderSurfaces.
    //

    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromium> child = LayerChromium::create();
    RefPtr<LayerChromium> grandChild = LayerChromium::create();
    RefPtr<LayerChromium> greatGrandChild = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> leafNode1 = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> leafNode2 = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(child);
    child->addChild(grandChild);
    grandChild->addChild(greatGrandChild);

    // leafNode1 ensures that parent and child are kept on the renderSurfaceLayerList,
    // even though grandChild and greatGrandChild should be clipped.
    child->addChild(leafNode1);
    greatGrandChild->addChild(leafNode2);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(500, 500), false);
    setLayerPropertiesForTesting(child.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(20, 20), false);
    setLayerPropertiesForTesting(grandChild.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(45, 45), IntSize(10, 10), false);
    setLayerPropertiesForTesting(greatGrandChild.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(10, 10), false);
    setLayerPropertiesForTesting(leafNode1.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(500, 500), false);
    setLayerPropertiesForTesting(leafNode2.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(20, 20), false);

    child->setMasksToBounds(true);
    child->setOpacity(0.4f);
    grandChild->setOpacity(0.5f);
    greatGrandChild->setOpacity(0.4f);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    // FIXME: when we fix this "root-layer special case" behavior in CCLayerTreeHost, we will have to fix it here, too.
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent.get());

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    ASSERT_EQ(2U, renderSurfaceLayerList.size());
    EXPECT_EQ(parent->id(), renderSurfaceLayerList[0]->id());
    EXPECT_EQ(child->id(), renderSurfaceLayerList[1]->id());
}

TEST(CCLayerTreeHostCommonTest, verifyClipRectCullsRenderSurfacesCrashRepro)
{
    // This is a similar situation as verifyClipRectCullsRenderSurfaces, except that
    // it reproduces a crash bug http://code.google.com/p/chromium/issues/detail?id=106734.

    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromium> child = LayerChromium::create();
    RefPtr<LayerChromium> grandChild = LayerChromium::create();
    RefPtr<LayerChromium> greatGrandChild = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> leafNode1 = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> leafNode2 = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(child);
    child->addChild(grandChild);
    grandChild->addChild(greatGrandChild);

    // leafNode1 ensures that parent and child are kept on the renderSurfaceLayerList,
    // even though grandChild and greatGrandChild should be clipped.
    child->addChild(leafNode1);
    greatGrandChild->addChild(leafNode2);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(500, 500), false);
    setLayerPropertiesForTesting(child.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(20, 20), false);
    setLayerPropertiesForTesting(grandChild.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(45, 45), IntSize(10, 10), false);
    setLayerPropertiesForTesting(greatGrandChild.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(10, 10), false);
    setLayerPropertiesForTesting(leafNode1.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(500, 500), false);
    setLayerPropertiesForTesting(leafNode2.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(20, 20), false);

    child->setMasksToBounds(true);
    child->setOpacity(0.4f);
    grandChild->setOpacity(0.5f);
    greatGrandChild->setOpacity(0.4f);

    // Contaminate the grandChild and greatGrandChild's clipRect to reproduce the crash
    // bug found in http://code.google.com/p/chromium/issues/detail?id=106734. In this
    // bug, the clipRect was not re-computed for layers that create RenderSurfaces, and
    // therefore leafNode2 thinks it should draw itself. As a result, an extra
    // renderSurface remains on the renderSurfaceLayerList, which violates the assumption
    // that an empty renderSurface will always be the last item on the list, which
    // ultimately caused the crash.
    child->setClipRect(IntRect(IntPoint::zero(), IntSize(20, 20)));
    greatGrandChild->setClipRect(IntRect(IntPoint::zero(), IntSize(1234, 1234)));

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    // FIXME: when we fix this "root-layer special case" behavior in CCLayerTreeHost, we will have to fix it here, too.
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent.get());

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    ASSERT_EQ(2U, renderSurfaceLayerList.size());
    EXPECT_EQ(parent->id(), renderSurfaceLayerList[0]->id());
    EXPECT_EQ(child->id(), renderSurfaceLayerList[1]->id());
}

TEST(CCLayerTreeHostCommonTest, verifyClipRectIsPropagatedCorrectlyToLayers)
{
    // Verify that layers get the appropriate clipRects when their parent masksToBounds is true.
    //
    //   grandChild1 - completely inside the region; clipRect should be the mask region (larger than this layer's bounds).
    //   grandChild2 - partially clipped but NOT masksToBounds; the clipRect should be the parent's clipRect regardless of the layer's bounds.
    //   grandChild3 - partially clipped and masksToBounds; the clipRect will be the intersection of layerBounds and the mask region.
    //   grandChild4 - outside parent's clipRect, and masksToBounds; the clipRect should be empty.
    //

    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromium> child = LayerChromium::create();
    RefPtr<LayerChromium> grandChild1 = LayerChromium::create();
    RefPtr<LayerChromium> grandChild2 = LayerChromium::create();
    RefPtr<LayerChromium> grandChild3 = LayerChromium::create();
    RefPtr<LayerChromium> grandChild4 = LayerChromium::create();

    parent->createRenderSurface();
    parent->addChild(child);
    child->addChild(grandChild1);
    child->addChild(grandChild2);
    child->addChild(grandChild3);
    child->addChild(grandChild4);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(500, 500), false);
    setLayerPropertiesForTesting(child.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(20, 20), false);
    setLayerPropertiesForTesting(grandChild1.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(5, 5), IntSize(10, 10), false);
    setLayerPropertiesForTesting(grandChild2.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(15, 15), IntSize(10, 10), false);
    setLayerPropertiesForTesting(grandChild3.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(15, 15), IntSize(10, 10), false);
    setLayerPropertiesForTesting(grandChild4.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(45, 45), IntSize(10, 10), false);

    child->setMasksToBounds(true);
    grandChild3->setMasksToBounds(true);
    grandChild4->setMasksToBounds(true);

    // Force everyone to be a render surface.
    child->setOpacity(0.4f);
    grandChild1->setOpacity(0.5f);
    grandChild2->setOpacity(0.5f);
    grandChild3->setOpacity(0.5f);
    grandChild4->setOpacity(0.5f);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    // FIXME: when we fix this "root-layer special case" behavior in CCLayerTreeHost, we will have to fix it here, too.
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent.get());

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    EXPECT_INT_RECT_EQ(IntRect(IntPoint::zero(), IntSize(20, 20)), grandChild1->clipRect());
    EXPECT_INT_RECT_EQ(IntRect(IntPoint::zero(), IntSize(20, 20)), grandChild2->clipRect());
    EXPECT_INT_RECT_EQ(IntRect(IntPoint(15, 15), IntSize(5, 5)), grandChild3->clipRect());
    EXPECT_TRUE(grandChild4->clipRect().isEmpty());
}

TEST(CCLayerTreeHostCommonTest, verifyClipRectIsPropagatedCorrectlyToSurfaces)
{
    // Verify that renderSurfaces (and their layers) get the appropriate clipRects when their parent masksToBounds is true.
    //
    // Layers that own renderSurfaces (at least for now) do not inherit any clipRect;
    // instead the surface will enforce the clip for the entire subtree. They may still
    // have a clipRect of their own layer bounds, however, if masksToBounds was true.
    //

    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromium> child = LayerChromium::create();
    RefPtr<LayerChromium> grandChild1 = LayerChromium::create();
    RefPtr<LayerChromium> grandChild2 = LayerChromium::create();
    RefPtr<LayerChromium> grandChild3 = LayerChromium::create();
    RefPtr<LayerChromium> grandChild4 = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> leafNode1 = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> leafNode2 = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> leafNode3 = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> leafNode4 = adoptRef(new LayerChromiumWithForcedDrawsContent());

    parent->createRenderSurface();
    parent->addChild(child);
    child->addChild(grandChild1);
    child->addChild(grandChild2);
    child->addChild(grandChild3);
    child->addChild(grandChild4);

    // the leaf nodes ensure that these grandChildren become renderSurfaces for this test.
    grandChild1->addChild(leafNode1);
    grandChild2->addChild(leafNode2);
    grandChild3->addChild(leafNode3);
    grandChild4->addChild(leafNode4);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(500, 500), false);
    setLayerPropertiesForTesting(child.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(20, 20), false);
    setLayerPropertiesForTesting(grandChild1.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(5, 5), IntSize(10, 10), false);
    setLayerPropertiesForTesting(grandChild2.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(15, 15), IntSize(10, 10), false);
    setLayerPropertiesForTesting(grandChild3.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(15, 15), IntSize(10, 10), false);
    setLayerPropertiesForTesting(grandChild4.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(45, 45), IntSize(10, 10), false);
    setLayerPropertiesForTesting(leafNode1.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(10, 10), false);
    setLayerPropertiesForTesting(leafNode2.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(10, 10), false);
    setLayerPropertiesForTesting(leafNode3.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(10, 10), false);
    setLayerPropertiesForTesting(leafNode4.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(10, 10), false);

    child->setMasksToBounds(true);
    grandChild3->setMasksToBounds(true);
    grandChild4->setMasksToBounds(true);

    // Force everyone to be a render surface.
    child->setOpacity(0.4f);
    grandChild1->setOpacity(0.5f);
    grandChild2->setOpacity(0.5f);
    grandChild3->setOpacity(0.5f);
    grandChild4->setOpacity(0.5f);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    // FIXME: when we fix this "root-layer special case" behavior in CCLayerTreeHost, we will have to fix it here, too.
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent.get());

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    ASSERT_TRUE(grandChild1->renderSurface());
    ASSERT_TRUE(grandChild2->renderSurface());
    ASSERT_TRUE(grandChild3->renderSurface());
    EXPECT_FALSE(grandChild4->renderSurface()); // Because grandChild4 is entirely clipped, it is expected to not have a renderSurface.

    // Surfaces are clipped by their parent, but un-affected by the owning layer's masksToBounds.
    EXPECT_INT_RECT_EQ(IntRect(IntPoint(0, 0), IntSize(20, 20)), grandChild1->renderSurface()->clipRect());
    EXPECT_INT_RECT_EQ(IntRect(IntPoint(0, 0), IntSize(20, 20)), grandChild2->renderSurface()->clipRect());
    EXPECT_INT_RECT_EQ(IntRect(IntPoint(0, 0), IntSize(20, 20)), grandChild3->renderSurface()->clipRect());

    // Layers do not inherit the clipRect from their owned surfaces, but if masksToBounds is true, they do create their own clipRect.
    EXPECT_FALSE(grandChild1->usesLayerClipping());
    EXPECT_FALSE(grandChild2->usesLayerClipping());
    EXPECT_TRUE(grandChild3->usesLayerClipping());
    EXPECT_TRUE(grandChild4->usesLayerClipping());
}

TEST(CCLayerTreeHostCommonTest, verifyAnimationsForRenderSurfaceHierarchy)
{
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromium> renderSurface1 = LayerChromium::create();
    RefPtr<LayerChromium> renderSurface2 = LayerChromium::create();
    RefPtr<LayerChromium> childOfRoot = LayerChromium::create();
    RefPtr<LayerChromium> childOfRS1 = LayerChromium::create();
    RefPtr<LayerChromium> childOfRS2 = LayerChromium::create();
    RefPtr<LayerChromium> grandChildOfRoot = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> grandChildOfRS1 = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> grandChildOfRS2 = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(renderSurface1);
    parent->addChild(childOfRoot);
    renderSurface1->addChild(childOfRS1);
    renderSurface1->addChild(renderSurface2);
    renderSurface2->addChild(childOfRS2);
    childOfRoot->addChild(grandChildOfRoot);
    childOfRS1->addChild(grandChildOfRS1);
    childOfRS2->addChild(grandChildOfRS2);

    // In combination with descendantDrawsContent, opacity != 1 forces the layer to have a new renderSurface.
    addOpacityTransitionToController(*renderSurface1->layerAnimationController(), 10, 1, 0, false);

    // Also put an animated opacity on a layer without descendants.
    addOpacityTransitionToController(*grandChildOfRoot->layerAnimationController(), 10, 1, 0, false);

    TransformationMatrix layerTransform;
    layerTransform.translate(1.0, 1.0);
    TransformationMatrix sublayerTransform;
    sublayerTransform.scale3d(10.0, 1.0, 1.0);

    // In combination with descendantDrawsContent, an animated transform forces the layer to have a new renderSurface.
    addAnimatedTransformToController(*renderSurface2->layerAnimationController(), 10, 30, 0);

    // Also put transform animations on grandChildOfRoot, and grandChildOfRS2
    addAnimatedTransformToController(*grandChildOfRoot->layerAnimationController(), 10, 30, 0);
    addAnimatedTransformToController(*grandChildOfRS2->layerAnimationController(), 10, 30, 0);

    setLayerPropertiesForTesting(parent.get(), layerTransform, sublayerTransform, FloatPoint(0.25f, 0.0f), FloatPoint(2.5f, 0.0f), IntSize(10, 10), false);
    setLayerPropertiesForTesting(renderSurface1.get(), layerTransform, sublayerTransform, FloatPoint(0.25f, 0.0f), FloatPoint(2.5f, 0.0f), IntSize(10, 10), false);
    setLayerPropertiesForTesting(renderSurface2.get(), layerTransform, sublayerTransform, FloatPoint(0.25f, 0.0f), FloatPoint(2.5f, 0.0f), IntSize(10, 10), false);
    setLayerPropertiesForTesting(childOfRoot.get(), layerTransform, sublayerTransform, FloatPoint(0.25f, 0.0f), FloatPoint(2.5f, 0.0f), IntSize(10, 10), false);
    setLayerPropertiesForTesting(childOfRS1.get(), layerTransform, sublayerTransform, FloatPoint(0.25f, 0.0f), FloatPoint(2.5f, 0.0f), IntSize(10, 10), false);
    setLayerPropertiesForTesting(childOfRS2.get(), layerTransform, sublayerTransform, FloatPoint(0.25f, 0.0f), FloatPoint(2.5f, 0.0f), IntSize(10, 10), false);
    setLayerPropertiesForTesting(grandChildOfRoot.get(), layerTransform, sublayerTransform, FloatPoint(0.25f, 0.0f), FloatPoint(2.5f, 0.0f), IntSize(10, 10), false);
    setLayerPropertiesForTesting(grandChildOfRS1.get(), layerTransform, sublayerTransform, FloatPoint(0.25f, 0.0f), FloatPoint(2.5f, 0.0f), IntSize(10, 10), false);
    setLayerPropertiesForTesting(grandChildOfRS2.get(), layerTransform, sublayerTransform, FloatPoint(0.25f, 0.0f), FloatPoint(2.5f, 0.0f), IntSize(10, 10), false);

    executeCalculateDrawTransformsAndVisibility(parent.get());

    // Only layers that are associated with render surfaces should have an actual renderSurface() value.
    //
    ASSERT_TRUE(parent->renderSurface());
    ASSERT_FALSE(childOfRoot->renderSurface());
    ASSERT_FALSE(grandChildOfRoot->renderSurface());

    ASSERT_TRUE(renderSurface1->renderSurface());
    ASSERT_FALSE(childOfRS1->renderSurface());
    ASSERT_FALSE(grandChildOfRS1->renderSurface());

    ASSERT_TRUE(renderSurface2->renderSurface());
    ASSERT_FALSE(childOfRS2->renderSurface());
    ASSERT_FALSE(grandChildOfRS2->renderSurface());

    // Verify all targetRenderSurface accessors
    //
    EXPECT_EQ(parent->renderSurface(), parent->targetRenderSurface());
    EXPECT_EQ(parent->renderSurface(), childOfRoot->targetRenderSurface());
    EXPECT_EQ(parent->renderSurface(), grandChildOfRoot->targetRenderSurface());

    EXPECT_EQ(renderSurface1->renderSurface(), renderSurface1->targetRenderSurface());
    EXPECT_EQ(renderSurface1->renderSurface(), childOfRS1->targetRenderSurface());
    EXPECT_EQ(renderSurface1->renderSurface(), grandChildOfRS1->targetRenderSurface());

    EXPECT_EQ(renderSurface2->renderSurface(), renderSurface2->targetRenderSurface());
    EXPECT_EQ(renderSurface2->renderSurface(), childOfRS2->targetRenderSurface());
    EXPECT_EQ(renderSurface2->renderSurface(), grandChildOfRS2->targetRenderSurface());

    // Verify drawOpacityIsAnimating values
    //
    EXPECT_FALSE(parent->drawOpacityIsAnimating());
    EXPECT_FALSE(childOfRoot->drawOpacityIsAnimating());
    EXPECT_TRUE(grandChildOfRoot->drawOpacityIsAnimating());
    EXPECT_FALSE(renderSurface1->drawOpacityIsAnimating());
    EXPECT_TRUE(renderSurface1->renderSurface()->drawOpacityIsAnimating());
    EXPECT_FALSE(childOfRS1->drawOpacityIsAnimating());
    EXPECT_FALSE(grandChildOfRS1->drawOpacityIsAnimating());
    EXPECT_FALSE(renderSurface2->drawOpacityIsAnimating());
    EXPECT_FALSE(renderSurface2->renderSurface()->drawOpacityIsAnimating());
    EXPECT_FALSE(childOfRS2->drawOpacityIsAnimating());
    EXPECT_FALSE(grandChildOfRS2->drawOpacityIsAnimating());

    // Verify drawTransformsAnimatingInTarget values
    //
    EXPECT_FALSE(parent->drawTransformIsAnimating());
    EXPECT_FALSE(childOfRoot->drawTransformIsAnimating());
    EXPECT_TRUE(grandChildOfRoot->drawTransformIsAnimating());
    EXPECT_FALSE(renderSurface1->drawTransformIsAnimating());
    EXPECT_FALSE(renderSurface1->renderSurface()->targetSurfaceTransformsAreAnimating());
    EXPECT_FALSE(childOfRS1->drawTransformIsAnimating());
    EXPECT_FALSE(grandChildOfRS1->drawTransformIsAnimating());
    EXPECT_FALSE(renderSurface2->drawTransformIsAnimating());
    EXPECT_TRUE(renderSurface2->renderSurface()->targetSurfaceTransformsAreAnimating());
    EXPECT_FALSE(childOfRS2->drawTransformIsAnimating());
    EXPECT_TRUE(grandChildOfRS2->drawTransformIsAnimating());

    // Verify drawTransformsAnimatingInScreen values
    //
    EXPECT_FALSE(parent->screenSpaceTransformIsAnimating());
    EXPECT_FALSE(childOfRoot->screenSpaceTransformIsAnimating());
    EXPECT_TRUE(grandChildOfRoot->screenSpaceTransformIsAnimating());
    EXPECT_FALSE(renderSurface1->screenSpaceTransformIsAnimating());
    EXPECT_FALSE(renderSurface1->renderSurface()->screenSpaceTransformsAreAnimating());
    EXPECT_FALSE(childOfRS1->screenSpaceTransformIsAnimating());
    EXPECT_FALSE(grandChildOfRS1->screenSpaceTransformIsAnimating());
    EXPECT_TRUE(renderSurface2->screenSpaceTransformIsAnimating());
    EXPECT_TRUE(renderSurface2->renderSurface()->screenSpaceTransformsAreAnimating());
    EXPECT_TRUE(childOfRS2->screenSpaceTransformIsAnimating());
    EXPECT_TRUE(grandChildOfRS2->screenSpaceTransformIsAnimating());


    // Sanity check. If these fail there is probably a bug in the test itself.
    // It is expected that we correctly set up transforms so that the y-component of the screen-space transform
    // encodes the "depth" of the layer in the tree.
    EXPECT_FLOAT_EQ(1.0, parent->screenSpaceTransform().m42());
    EXPECT_FLOAT_EQ(2.0, childOfRoot->screenSpaceTransform().m42());
    EXPECT_FLOAT_EQ(3.0, grandChildOfRoot->screenSpaceTransform().m42());

    EXPECT_FLOAT_EQ(2.0, renderSurface1->screenSpaceTransform().m42());
    EXPECT_FLOAT_EQ(3.0, childOfRS1->screenSpaceTransform().m42());
    EXPECT_FLOAT_EQ(4.0, grandChildOfRS1->screenSpaceTransform().m42());

    EXPECT_FLOAT_EQ(3.0, renderSurface2->screenSpaceTransform().m42());
    EXPECT_FLOAT_EQ(4.0, childOfRS2->screenSpaceTransform().m42());
    EXPECT_FLOAT_EQ(5.0, grandChildOfRS2->screenSpaceTransform().m42());
}

TEST(CCLayerTreeHostCommonTest, verifyVisibleRectForIdentityTransform)
{
    // Test the calculateVisibleRect() function works correctly for identity transforms.

    IntRect targetSurfaceRect = IntRect(IntPoint(0, 0), IntSize(100, 100));
    TransformationMatrix layerToSurfaceTransform;

    // Case 1: Layer is contained within the surface.
    IntRect layerContentRect = IntRect(IntPoint(10, 10), IntSize(30, 30));
    IntRect expected = IntRect(IntPoint(10, 10), IntSize(30, 30));
    IntRect actual = CCLayerTreeHostCommon::calculateVisibleRect(targetSurfaceRect, layerContentRect, layerToSurfaceTransform);
    EXPECT_INT_RECT_EQ(expected, actual);

    // Case 2: Layer is outside the surface rect.
    layerContentRect = IntRect(IntPoint(120, 120), IntSize(30, 30));
    actual = CCLayerTreeHostCommon::calculateVisibleRect(targetSurfaceRect, layerContentRect, layerToSurfaceTransform);
    EXPECT_TRUE(actual.isEmpty());

    // Case 3: Layer is partially overlapping the surface rect.
    layerContentRect = IntRect(IntPoint(80, 80), IntSize(30, 30));
    expected = IntRect(IntPoint(80, 80), IntSize(20, 20));
    actual = CCLayerTreeHostCommon::calculateVisibleRect(targetSurfaceRect, layerContentRect, layerToSurfaceTransform);
    EXPECT_INT_RECT_EQ(expected, actual);
}

TEST(CCLayerTreeHostCommonTest, verifyVisibleRectForTranslations)
{
    // Test the calculateVisibleRect() function works correctly for scaling transforms.

    IntRect targetSurfaceRect = IntRect(IntPoint(0, 0), IntSize(100, 100));
    IntRect layerContentRect = IntRect(IntPoint(0, 0), IntSize(30, 30));
    TransformationMatrix layerToSurfaceTransform;

    // Case 1: Layer is contained within the surface.
    layerToSurfaceTransform.makeIdentity();
    layerToSurfaceTransform.translate(10, 10);
    IntRect expected = IntRect(IntPoint(0, 0), IntSize(30, 30));
    IntRect actual = CCLayerTreeHostCommon::calculateVisibleRect(targetSurfaceRect, layerContentRect, layerToSurfaceTransform);
    EXPECT_INT_RECT_EQ(expected, actual);

    // Case 2: Layer is outside the surface rect.
    layerToSurfaceTransform.makeIdentity();
    layerToSurfaceTransform.translate(120, 120);
    actual = CCLayerTreeHostCommon::calculateVisibleRect(targetSurfaceRect, layerContentRect, layerToSurfaceTransform);
    EXPECT_TRUE(actual.isEmpty());

    // Case 3: Layer is partially overlapping the surface rect.
    layerToSurfaceTransform.makeIdentity();
    layerToSurfaceTransform.translate(80, 80);
    expected = IntRect(IntPoint(0, 0), IntSize(20, 20));
    actual = CCLayerTreeHostCommon::calculateVisibleRect(targetSurfaceRect, layerContentRect, layerToSurfaceTransform);
    EXPECT_INT_RECT_EQ(expected, actual);
}

TEST(CCLayerTreeHostCommonTest, verifyVisibleRectFor2DRotations)
{
    // Test the calculateVisibleRect() function works correctly for rotations about z-axis (i.e. 2D rotations).
    // Remember that calculateVisibleRect() should return the visible rect in the layer's space.

    IntRect targetSurfaceRect = IntRect(IntPoint(0, 0), IntSize(100, 100));
    IntRect layerContentRect = IntRect(IntPoint(0, 0), IntSize(30, 30));
    TransformationMatrix layerToSurfaceTransform;

    // Case 1: Layer is contained within the surface.
    layerToSurfaceTransform.makeIdentity();
    layerToSurfaceTransform.translate(50, 50);
    layerToSurfaceTransform.rotate(45);
    IntRect expected = IntRect(IntPoint(0, 0), IntSize(30, 30));
    IntRect actual = CCLayerTreeHostCommon::calculateVisibleRect(targetSurfaceRect, layerContentRect, layerToSurfaceTransform);
    EXPECT_INT_RECT_EQ(expected, actual);

    // Case 2: Layer is outside the surface rect.
    layerToSurfaceTransform.makeIdentity();
    layerToSurfaceTransform.translate(-50, 0);
    layerToSurfaceTransform.rotate(45);
    actual = CCLayerTreeHostCommon::calculateVisibleRect(targetSurfaceRect, layerContentRect, layerToSurfaceTransform);
    EXPECT_TRUE(actual.isEmpty());

    // Case 3: The layer is rotated about its top-left corner. In surface space, the layer
    //         is oriented diagonally, with the left half outside of the renderSurface. In
    //         this case, the visible rect should still be the entire layer (remember the
    //         visible rect is computed in layer space); both the top-left and
    //         bottom-right corners of the layer are still visible.
    layerToSurfaceTransform.makeIdentity();
    layerToSurfaceTransform.rotate(45);
    expected = IntRect(IntPoint(0, 0), IntSize(30, 30));
    actual = CCLayerTreeHostCommon::calculateVisibleRect(targetSurfaceRect, layerContentRect, layerToSurfaceTransform);
    EXPECT_INT_RECT_EQ(expected, actual);

    // Case 4: The layer is rotated about its top-left corner, and translated upwards. In
    //         surface space, the layer is oriented diagonally, with only the top corner
    //         of the surface overlapping the layer. In layer space, the render surface
    //         overlaps the right side of the layer. The visible rect should be the
    //         layer's right half.
    layerToSurfaceTransform.makeIdentity();
    layerToSurfaceTransform.translate(0, -sqrt(2.0) * 15);
    layerToSurfaceTransform.rotate(45);
    expected = IntRect(IntPoint(15, 0), IntSize(15, 30)); // right half of layer bounds.
    actual = CCLayerTreeHostCommon::calculateVisibleRect(targetSurfaceRect, layerContentRect, layerToSurfaceTransform);
    EXPECT_INT_RECT_EQ(expected, actual);
}

TEST(CCLayerTreeHostCommonTest, verifyVisibleRectFor3dOrthographicTransform)
{
    // Test that the calculateVisibleRect() function works correctly for 3d transforms.

    IntRect targetSurfaceRect = IntRect(IntPoint(0, 0), IntSize(100, 100));
    IntRect layerContentRect = IntRect(IntPoint(0, 0), IntSize(100, 100));
    TransformationMatrix layerToSurfaceTransform;

    // Case 1: Orthographic projection of a layer rotated about y-axis by 45 degrees, should be fully contained in the renderSurface.
    layerToSurfaceTransform.makeIdentity();
    layerToSurfaceTransform.rotate3d(0, 45, 0);
    IntRect expected = IntRect(IntPoint(0, 0), IntSize(100, 100));
    IntRect actual = CCLayerTreeHostCommon::calculateVisibleRect(targetSurfaceRect, layerContentRect, layerToSurfaceTransform);
    EXPECT_INT_RECT_EQ(expected, actual);

    // Case 2: Orthographic projection of a layer rotated about y-axis by 45 degrees, but
    //         shifted to the side so only the right-half the layer would be visible on
    //         the surface.
    double halfWidthOfRotatedLayer = (100.0 / sqrt(2.0)) * 0.5; // 100.0 is the un-rotated layer width; divided by sqrt(2.0) is the rotated width.
    layerToSurfaceTransform.makeIdentity();
    layerToSurfaceTransform.translate(-halfWidthOfRotatedLayer, 0);
    layerToSurfaceTransform.rotate3d(0, 45, 0); // rotates about the left edge of the layer
    expected = IntRect(IntPoint(50, 0), IntSize(50, 100)); // right half of the layer.
    actual = CCLayerTreeHostCommon::calculateVisibleRect(targetSurfaceRect, layerContentRect, layerToSurfaceTransform);
    EXPECT_INT_RECT_EQ(expected, actual);
}

TEST(CCLayerTreeHostCommonTest, verifyVisibleRectFor3dPerspectiveTransform)
{
    // Test the calculateVisibleRect() function works correctly when the layer has a
    // perspective projection onto the target surface.

    IntRect targetSurfaceRect = IntRect(IntPoint(0, 0), IntSize(100, 100));
    IntRect layerContentRect = IntRect(IntPoint(-50, -50), IntSize(200, 200));
    TransformationMatrix layerToSurfaceTransform;

    // Case 1: Even though the layer is twice as large as the surface, due to perspective
    //         foreshortening, the layer will fit fully in the surface when its translated
    //         more than the perspective amount.
    layerToSurfaceTransform.makeIdentity();

    // The following sequence of transforms applies the perspective about the center of the surface.
    layerToSurfaceTransform.translate(50, 50);
    layerToSurfaceTransform.applyPerspective(9);
    layerToSurfaceTransform.translate(-50, -50);

    // This translate places the layer in front of the surface's projection plane.
    layerToSurfaceTransform.translate3d(0, 0, -27);

    IntRect expected = IntRect(IntPoint(-50, -50), IntSize(200, 200));
    IntRect actual = CCLayerTreeHostCommon::calculateVisibleRect(targetSurfaceRect, layerContentRect, layerToSurfaceTransform);
    EXPECT_INT_RECT_EQ(expected, actual);

    // Case 2: same projection as before, except that the layer is also translated to the
    //         side, so that only the right half of the layer should be visible.
    //
    // Explanation of expected result:
    // The perspective ratio is (z distance between layer and camera origin) / (z distance between projection plane and camera origin) == ((-27 - 9) / 9)
    // Then, by similar triangles, if we want to move a layer by translating -50 units in projected surface units (so that only half of it is
    // visible), then we would need to translate by (-36 / 9) * -50 == -200 in the layer's units.
    //
    layerToSurfaceTransform.translate3d(-200, 0, 0);
    expected = IntRect(IntPoint(50, -50), IntSize(100, 200)); // The right half of the layer's bounding rect.
    actual = CCLayerTreeHostCommon::calculateVisibleRect(targetSurfaceRect, layerContentRect, layerToSurfaceTransform);
    EXPECT_INT_RECT_EQ(expected, actual);
}

TEST(CCLayerTreeHostCommonTest, verifyVisibleRectFor3dOrthographicIsNotClippedBehindSurface)
{
    // There is currently no explicit concept of an orthographic projection plane in our
    // code (nor in the CSS spec to my knowledge). Therefore, layers that are technically
    // behind the surface in an orthographic world should not be clipped when they are
    // flattened to the surface.

    IntRect targetSurfaceRect = IntRect(IntPoint(0, 0), IntSize(100, 100));
    IntRect layerContentRect = IntRect(IntPoint(0, 0), IntSize(100, 100));
    TransformationMatrix layerToSurfaceTransform;

    // This sequence of transforms effectively rotates the layer about the y-axis at the
    // center of the layer.
    layerToSurfaceTransform.makeIdentity();
    layerToSurfaceTransform.translate(50, 0);
    layerToSurfaceTransform.rotate3d(0, 45, 0);
    layerToSurfaceTransform.translate(-50, 0);

    IntRect expected = IntRect(IntPoint(0, 0), IntSize(100, 100));
    IntRect actual = CCLayerTreeHostCommon::calculateVisibleRect(targetSurfaceRect, layerContentRect, layerToSurfaceTransform);
    EXPECT_INT_RECT_EQ(expected, actual);
}

TEST(CCLayerTreeHostCommonTest, verifyVisibleRectFor3dPerspectiveWhenClippedByW)
{
    // Test the calculateVisibleRect() function works correctly when projecting a surface
    // onto a layer, but the layer is partially behind the camera (not just behind the
    // projection plane). In this case, the cartesian coordinates may seem to be valid,
    // but actually they are not. The visibleRect needs to be properly clipped by the
    // w = 0 plane in homogeneous coordinates before converting to cartesian coordinates.

    IntRect targetSurfaceRect = IntRect(IntPoint(-50, -50), IntSize(100, 100));
    IntRect layerContentRect = IntRect(IntPoint(-10, -1), IntSize(20, 2));
    TransformationMatrix layerToSurfaceTransform;

    // The layer is positioned so that the right half of the layer should be in front of
    // the camera, while the other half is behind the surface's projection plane. The
    // following sequence of transforms applies the perspective and rotation about the
    // center of the layer.
    layerToSurfaceTransform.makeIdentity();
    layerToSurfaceTransform.applyPerspective(1);
    layerToSurfaceTransform.translate3d(-1, 0, 1);
    layerToSurfaceTransform.rotate3d(0, 45, 0);

    // Sanity check that this transform does indeed cause w < 0 when applying the
    // transform, otherwise this code is not testing the intended scenario.
    bool clipped = false;
    CCMathUtil::mapQuad(layerToSurfaceTransform, FloatQuad(FloatRect(layerContentRect)), clipped);
    ASSERT_TRUE(clipped);

    int expectedXPosition = 0;
    int expectedWidth = 10;
    IntRect actual = CCLayerTreeHostCommon::calculateVisibleRect(targetSurfaceRect, layerContentRect, layerToSurfaceTransform);
    EXPECT_EQ(expectedXPosition, actual.x());
    EXPECT_EQ(expectedWidth, actual.width());
}

TEST(CCLayerTreeHostCommonTest, verifyVisibleRectForPerspectiveUnprojection)
{
    // To determine visibleRect in layer space, there needs to be an un-projection from
    // surface space to layer space. When the original transform was a perspective
    // projection that was clipped, it returns a rect that encloses the clipped bounds.
    // Un-projecting this new rect may require clipping again.

    // This sequence of transforms causes one corner of the layer to protrude across the w = 0 plane, and should be clipped.
    IntRect targetSurfaceRect = IntRect(IntPoint(-50, -50), IntSize(100, 100));
    IntRect layerContentRect = IntRect(IntPoint(-10, -10), IntSize(20, 20));
    TransformationMatrix layerToSurfaceTransform;
    layerToSurfaceTransform.makeIdentity();
    layerToSurfaceTransform.applyPerspective(1);
    layerToSurfaceTransform.translate3d(0, 0, -5);
    layerToSurfaceTransform.rotate3d(0, 45, 0);
    layerToSurfaceTransform.rotate3d(80, 0, 0);

    // Sanity check that un-projection does indeed cause w < 0, otherwise this code is not
    // testing the intended scenario.
    bool clipped = false;
    FloatRect clippedRect = CCMathUtil::mapClippedRect(layerToSurfaceTransform, layerContentRect);
    CCMathUtil::projectQuad(layerToSurfaceTransform.inverse(), FloatQuad(clippedRect), clipped);
    ASSERT_TRUE(clipped);

    // Only the corner of the layer is not visible on the surface because of being
    // clipped. But, the net result of rounding visible region to an axis-aligned rect is
    // that the entire layer should still be considered visible.
    IntRect expected = IntRect(IntPoint(-10, -10), IntSize(20, 20));
    IntRect actual = CCLayerTreeHostCommon::calculateVisibleRect(targetSurfaceRect, layerContentRect, layerToSurfaceTransform);
    EXPECT_INT_RECT_EQ(expected, actual);
}

TEST(CCLayerTreeHostCommonTest, verifyBackFaceCulling)
{
    // Verify that layers are appropriately culled when their back face is showing and they are not double sided.
    //
    // Layers that are animating do not get culled on the main thread, as their transforms should be
    // treated as "unknown" so we can not be sure that their back face is really showing.
    //

    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> child = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> animatingSurface = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> childOfAnimatingSurface = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> animatingChild = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> child2 = adoptRef(new LayerChromiumWithForcedDrawsContent());

    parent->createRenderSurface();
    parent->addChild(child);
    parent->addChild(animatingSurface);
    animatingSurface->addChild(childOfAnimatingSurface);
    parent->addChild(animatingChild);
    parent->addChild(child2);

    // Nothing is double-sided
    child->setDoubleSided(false);
    child2->setDoubleSided(false);
    animatingSurface->setDoubleSided(false);
    childOfAnimatingSurface->setDoubleSided(false);
    animatingChild->setDoubleSided(false);

    TransformationMatrix backfaceMatrix;
    backfaceMatrix.translate(50, 50);
    backfaceMatrix.rotate3d(0, 1, 0, 180);
    backfaceMatrix.translate(-50, -50);

    // Having a descendent, and animating transforms, will make the animatingSurface own a render surface.
    addAnimatedTransformToController(*animatingSurface->layerAnimationController(), 10, 30, 0);
    // This is just an animating layer, not a surface.
    addAnimatedTransformToController(*animatingChild->layerAnimationController(), 10, 30, 0);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), true);
    setLayerPropertiesForTesting(child.get(), backfaceMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), false);
    setLayerPropertiesForTesting(animatingSurface.get(), backfaceMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), false);
    setLayerPropertiesForTesting(childOfAnimatingSurface.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), false);
    setLayerPropertiesForTesting(animatingChild.get(), backfaceMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), false);
    setLayerPropertiesForTesting(child2.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), false);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent.get());

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    EXPECT_FALSE(child->renderSurface());
    EXPECT_TRUE(animatingSurface->renderSurface());
    EXPECT_FALSE(childOfAnimatingSurface->renderSurface());
    EXPECT_FALSE(animatingChild->renderSurface());
    EXPECT_FALSE(child2->renderSurface());

    // Verify that the animatingChild and childOfAnimatingSurface were not culled, but that child was.
    ASSERT_EQ(2u, renderSurfaceLayerList.size());
    EXPECT_EQ(parent->id(), renderSurfaceLayerList[0]->id());
    EXPECT_EQ(animatingSurface->id(), renderSurfaceLayerList[1]->id());

    // The non-animating child be culled from the layer list for the parent render surface.
    ASSERT_EQ(3u, renderSurfaceLayerList[0]->renderSurface()->layerList().size());
    EXPECT_EQ(animatingSurface->id(), renderSurfaceLayerList[0]->renderSurface()->layerList()[0]->id());
    EXPECT_EQ(animatingChild->id(), renderSurfaceLayerList[0]->renderSurface()->layerList()[1]->id());
    EXPECT_EQ(child2->id(), renderSurfaceLayerList[0]->renderSurface()->layerList()[2]->id());

    ASSERT_EQ(2u, renderSurfaceLayerList[1]->renderSurface()->layerList().size());
    EXPECT_EQ(animatingSurface->id(), renderSurfaceLayerList[1]->renderSurface()->layerList()[0]->id());
    EXPECT_EQ(childOfAnimatingSurface->id(), renderSurfaceLayerList[1]->renderSurface()->layerList()[1]->id());

    EXPECT_FALSE(child2->visibleLayerRect().isEmpty());

    // But if the back face is visible, then the visibleLayerRect should be empty.
    EXPECT_TRUE(animatingChild->visibleLayerRect().isEmpty());
    EXPECT_TRUE(animatingSurface->visibleLayerRect().isEmpty());
    // And any layers in the subtree should not be considered visible either.
    EXPECT_TRUE(childOfAnimatingSurface->visibleLayerRect().isEmpty());
}

// FIXME:
// continue working on https://bugs.webkit.org/show_bug.cgi?id=68942
//  - add a test to verify clipping that changes the "center point"
//  - add a case that checks if a render surface's drawTransform is computed correctly. For the general case, and for special cases when clipping.
//  - add a case that checks if a render surface's replicaTransform is computed correctly.
//  - test all the conditions under which render surfaces are created
//  - if possible, test all conditions under which render surfaces are not created
//  - verify that the layer lists of render surfaces are correct, verify that "targetRenderSurface" values for each layer are correct.
//  - test the computation of clip rects and content rects
//  - test the special cases for mask layers and replica layers
//  - test the other functions in CCLayerTreeHostCommon
//

} // namespace
