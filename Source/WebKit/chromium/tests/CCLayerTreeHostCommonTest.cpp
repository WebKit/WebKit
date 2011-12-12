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

#include "CCLayerTreeTestCommon.h"
#include "LayerChromium.h"
#include "TransformationMatrix.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebCore;

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
    explicit LayerChromiumWithForcedDrawsContent(CCLayerDelegate* delegate)
        : LayerChromium(delegate)
    {
    }

    virtual bool drawsContent() const { return true; }
};

TEST(CCLayerTreeHostCommonTest, verifyTransformsForNoOpLayer)
{
    // Sanity check: For layers positioned at zero, with zero size,
    // and with identity transforms, then the drawTransform,
    // screenSpaceTransform, and the hierarchy passed on to children
    // layers should also be identity transforms.

    RefPtr<LayerChromium> parent = LayerChromium::create(0);
    RefPtr<LayerChromium> child = LayerChromium::create(0);
    RefPtr<LayerChromium> grandChild = LayerChromium::create(0);
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
    RefPtr<LayerChromium> layer = LayerChromium::create(0);
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
    RefPtr<LayerChromium> parent = LayerChromium::create(0);
    RefPtr<LayerChromium> child = LayerChromium::create(0);
    RefPtr<LayerChromium> grandChild = LayerChromium::create(0);
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
    RefPtr<LayerChromium> parent = LayerChromium::create(0);
    RefPtr<LayerChromium> child = LayerChromium::create(0);
    RefPtr<LayerChromiumWithForcedDrawsContent> grandChild = adoptRef(new LayerChromiumWithForcedDrawsContent(0));
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

}

TEST(CCLayerTreeHostCommonTest, verifyTransformsForRenderSurfaceHierarchy)
{
    // This test creates a more complex tree and verifies it all at once. This covers the following cases:
    //   - layers that are described w.r.t. a render surface: should have draw transforms described w.r.t. that surface
    //   - A render surface described w.r.t. an ancestor render surface: should have a draw transform described w.r.t. that ancestor surface
    //   - Sanity check on recursion: verify transforms of layers described w.r.t. a render surface that is described w.r.t. an ancestor render surface.
    //   - verifying that each layer has a reference to the correct renderSurface and targetRenderSurface values.

    RefPtr<LayerChromium> parent = LayerChromium::create(0);
    RefPtr<LayerChromium> renderSurface1 = LayerChromium::create(0);
    RefPtr<LayerChromium> renderSurface2 = LayerChromium::create(0);
    RefPtr<LayerChromium> childOfRoot = LayerChromium::create(0);
    RefPtr<LayerChromium> childOfRS1 = LayerChromium::create(0);
    RefPtr<LayerChromium> childOfRS2 = LayerChromium::create(0);
    RefPtr<LayerChromium> grandChildOfRoot = LayerChromium::create(0);
    RefPtr<LayerChromiumWithForcedDrawsContent> grandChildOfRS1 = adoptRef(new LayerChromiumWithForcedDrawsContent(0));
    RefPtr<LayerChromiumWithForcedDrawsContent> grandChildOfRS2 = adoptRef(new LayerChromiumWithForcedDrawsContent(0));
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
    renderSurface1->setOpacity(0.5f);
    renderSurface2->setOpacity(0.33f);

    // All layers in the tree are initialized with an anchor at 2.5 and a size of (10,10).
    // matrix "A" is the composite layer transform used in all layers, centered about the anchor point
    // matrix "B" is the sublayer transform used in all layers, centered about the center position of the layer.
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

    TransformationMatrix A = translationToAnchor * layerTransform * translationToAnchor.inverse();
    TransformationMatrix B = translationToCenter * sublayerTransform * translationToCenter.inverse();

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
    // Origin transform of render surface 2 is described with respect to render surface 2.
    EXPECT_TRANSFORMATION_MATRIX_EQ(B * A, renderSurface2->renderSurface()->originTransform());

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
    RefPtr<LayerChromium> parent = LayerChromium::create(0);
    RefPtr<LayerChromium> renderSurface1 = LayerChromium::create(0);
    RefPtr<LayerChromiumWithForcedDrawsContent> child = adoptRef(new LayerChromiumWithForcedDrawsContent(0));
    renderSurface1->setOpacity(0.9);

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
    RefPtr<LayerChromium> parent = LayerChromium::create(0);
    RefPtr<LayerChromium> renderSurface1 = LayerChromium::create(0);
    RefPtr<LayerChromiumWithForcedDrawsContent> child = adoptRef(new LayerChromiumWithForcedDrawsContent(0));
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
    RefPtr<LayerChromium> parent = LayerChromium::create(0);
    RefPtr<LayerChromium> child = LayerChromium::create(0);
    RefPtr<LayerChromium> grandChild = LayerChromium::create(0);
    RefPtr<LayerChromium> greatGrandChild = LayerChromium::create(0);
    RefPtr<LayerChromiumWithForcedDrawsContent> leafNode1 = adoptRef(new LayerChromiumWithForcedDrawsContent(0));
    RefPtr<LayerChromiumWithForcedDrawsContent> leafNode2 = adoptRef(new LayerChromiumWithForcedDrawsContent(0));
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
    child->setOpacity(0.4);
    grandChild->setOpacity(0.5);
    greatGrandChild->setOpacity(0.4);

    // Contaminate the grandChild and greatGrandChild's clipRect to reproduce the crash
    // bug found in http://code.google.com/p/chromium/issues/detail?id=106734. In this
    // bug, the clipRect was not re-computed for layers that create RenderSurfaces, and
    // therefore leafNode2 thinks it should draw itself. As a result, an extra
    // renderSurface remains on the renderSurfaceLayerList, which violates the assumption
    // that an empty renderSurface will always be the last item on the list, which
    // ultimately caused the crash.
    //
    // FIXME: it is also useful to test with this commented out. Eventually we should
    // create several test cases that test clipRect/drawableContentRect computation.
    child->setClipRect(IntRect(IntPoint::zero(), IntSize(20, 20)));
    greatGrandChild->setClipRect(IntRect(IntPoint::zero(), IntSize(1234, 1234)));

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    // FIXME: when we fix this "root-layer special case" behavior in CCLayerTreeHost, we will have to fix it here, too.
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    ASSERT_EQ(2U, renderSurfaceLayerList.size());
    EXPECT_EQ(parent->id(), renderSurfaceLayerList[0]->id());
    EXPECT_EQ(child->id(), renderSurfaceLayerList[1]->id());
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
