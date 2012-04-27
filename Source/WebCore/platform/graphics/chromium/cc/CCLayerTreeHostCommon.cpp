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

#include "FloatQuad.h"
#include "IntRect.h"
#include "LayerChromium.h"
#include "RenderSurfaceChromium.h"
#include "TransformationMatrix.h"
#include "cc/CCActiveAnimation.h"
#include "cc/CCLayerAnimationController.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCLayerIterator.h"
#include "cc/CCLayerSorter.h"
#include "cc/CCMathUtil.h"
#include "cc/CCRenderSurface.h"

namespace WebCore {

IntRect CCLayerTreeHostCommon::calculateVisibleRect(const IntRect& targetSurfaceRect, const IntRect& layerBoundRect, const TransformationMatrix& transform)
{
    // Is this layer fully contained within the target surface?
    IntRect layerInSurfaceSpace = CCMathUtil::mapClippedRect(transform, layerBoundRect);
    if (targetSurfaceRect.contains(layerInSurfaceSpace))
        return layerBoundRect;

    // If the layer doesn't fill up the entire surface, then find the part of
    // the surface rect where the layer could be visible. This avoids trying to
    // project surface rect points that are behind the projection point.
    IntRect minimalSurfaceRect = targetSurfaceRect;
    minimalSurfaceRect.intersect(layerInSurfaceSpace);

    // Project the corners of the target surface rect into the layer space.
    // This bounding rectangle may be larger than it needs to be (being
    // axis-aligned), but is a reasonable filter on the space to consider.
    // Non-invertible transforms will create an empty rect here.
    const TransformationMatrix surfaceToLayer = transform.inverse();
    IntRect layerRect = enclosingIntRect(CCMathUtil::projectClippedRect(surfaceToLayer, FloatRect(minimalSurfaceRect)));
    layerRect.intersect(layerBoundRect);
    return layerRect;
}

template<typename LayerType>
static IntRect calculateVisibleLayerRect(LayerType* layer)
{
    ASSERT(layer->targetRenderSurface());

    // Animated layers can exist in the render surface tree that are not visible currently
    // and have their back face showing. In this case, their visible rect should be empty.
    if (!layer->doubleSided() && layer->screenSpaceTransform().isBackFaceVisible())
        return IntRect();

    IntRect targetSurfaceRect = layer->targetRenderSurface()->contentRect();

    if (layer->usesLayerClipping())
        targetSurfaceRect.intersect(layer->clipRect());

    if (targetSurfaceRect.isEmpty() || layer->contentBounds().isEmpty())
        return targetSurfaceRect;

    // Note carefully these are aliases
    const IntSize& bounds = layer->bounds();
    const IntSize& contentBounds = layer->contentBounds();

    const IntRect layerBoundRect = IntRect(IntPoint(), contentBounds);
    TransformationMatrix transform = layer->drawTransform();

    transform.scaleNonUniform(bounds.width() / static_cast<double>(contentBounds.width()),
                              bounds.height() / static_cast<double>(contentBounds.height()));
    transform.translate(-contentBounds.width() / 2.0, -contentBounds.height() / 2.0);

    IntRect visibleLayerRect = CCLayerTreeHostCommon::calculateVisibleRect(targetSurfaceRect, layerBoundRect, transform);
    return visibleLayerRect;
}

static bool isScaleOrTranslation(const TransformationMatrix& m)
{
    return !m.m12() && !m.m13() && !m.m14()
           && !m.m21() && !m.m23() && !m.m24()
           && !m.m31() && !m.m32() && !m.m43()
           && m.m44();
}

static inline bool layerOpacityIsOpaque(CCLayerImpl* layer)
{
    return layer->opacity() == 1;
}

static inline bool layerOpacityIsOpaque(LayerChromium* layer)
{
    // If the opacity is being animated then the opacity on the main thread is unreliable
    // (since the impl thread may be using a different opacity), so it should not be trusted.
    // In particular, it should not be treated as opaque.
    return layer->opacity() == 1 && !layer->opacityIsAnimating();
}

static inline bool transformToParentIsKnown(CCLayerImpl*)
{
    return true;
}

static inline bool transformToParentIsKnown(LayerChromium* layer)
{
    return !layer->transformIsAnimating();
}

static inline bool transformToScreenIsKnown(CCLayerImpl*)
{
    return true;
}

static inline bool transformToScreenIsKnown(LayerChromium* layer)
{
    return !layer->screenSpaceTransformIsAnimating();
}

template<typename LayerType>
static bool layerShouldBeSkipped(LayerType* layer)
{
    // Layers can be skipped if any of these conditions are met.
    //   - does not draw content.
    //   - is transparent
    //   - has empty bounds
    //   - the layer is not double-sided, but its back face is visible.
    //
    // Some additional conditions need to be computed at a later point after the recursion is finished.
    //   - the intersection of render surface content and layer clipRect is empty
    //   - the visibleLayerRect is empty
    //
    // Note, if the layer should not have been drawn due to being fully transparent,
    // we would have skipped the entire subtree and never made it into this function,
    // so it is safe to omit this check here.

    if (!layer->drawsContent() || layer->bounds().isEmpty())
        return true;

    // The layer should not be drawn if (1) it is not double-sided and (2) the back of the layer is known to be facing the screen.
    if (!layer->doubleSided() && transformToScreenIsKnown(layer) && layer->screenSpaceTransform().isBackFaceVisible())
        return true;

    return false;
}

static inline bool subtreeShouldBeSkipped(CCLayerImpl* layer)
{
    // The opacity of a layer always applies to its children (either implicitly
    // via a render surface or explicitly if the parent preserves 3D), so the
    // entire subtree can be skipped if this layer is fully transparent.
    return !layer->opacity();
}

static inline bool subtreeShouldBeSkipped(LayerChromium* layer)
{
    // If the opacity is being animated then the opacity on the main thread is unreliable
    // (since the impl thread may be using a different opacity), so it should not be trusted.
    // In particular, it should not cause the subtree to be skipped.
    return !layer->opacity() && !layer->opacityIsAnimating();
}

template<typename LayerType>
static bool subtreeShouldRenderToSeparateSurface(LayerType* layer, bool axisAlignedWithRespectToParent)
{
    // FIXME: If we decide to create a render surface here while this layer does
    //        preserve-3d, then we may be sorting incorrectly because we will not be
    //        sorting the individual layers of this subtree with other layers outside of
    //        this subtree.

    // Cache this value, because otherwise it walks the entire subtree several times.
    bool descendantDrawsContent = layer->descendantDrawsContent();

    //
    // A layer and its descendants should render onto a new RenderSurface if any of these rules hold:
    //

    // If the layer uses a mask.
    if (layer->maskLayer())
        return true;

    // If the layer has a reflection.
    if (layer->replicaLayer())
        return true;

    // If the layer uses a CSS filter.
    if (!layer->filters().isEmpty() || !layer->backgroundFilters().isEmpty())
        return true;

    // If the layer flattens its subtree (i.e. the layer doesn't preserve-3d), but it is
    // treated as a 3D object by its parent (i.e. parent does preserve-3d).
    if (layer->parent() && layer->parent()->preserves3D() && !layer->preserves3D() && descendantDrawsContent)
        return true;

    // On the main thread side, animating transforms are unknown, and may cause a RenderSurface on the impl side.
    // Since they are cheap, we create a rendersurface for all animating transforms to cover these cases, and so
    // that we can consider descendants as not animating relative to their target to aid culling.
    if (!transformToParentIsKnown(layer) && descendantDrawsContent)
        return true;

    // If the layer clips its descendants but it is not axis-aligned with respect to its parent.
    if (layer->masksToBounds() && !axisAlignedWithRespectToParent && descendantDrawsContent)
        return true;

    // If the layer has opacity != 1 and does not have a preserves-3d transform style.
    if (!layerOpacityIsOpaque(layer) && !layer->preserves3D() && descendantDrawsContent)
        return true;

    return false;
}

// Recursively walks the layer tree starting at the given node and computes all the
// necessary transformations, clipRects, render surfaces, etc.
template<typename LayerType, typename LayerList, typename RenderSurfaceType, typename LayerSorter>
static bool calculateDrawTransformsAndVisibilityInternal(LayerType* layer, LayerType* rootLayer, const TransformationMatrix& parentMatrix, const TransformationMatrix& fullHierarchyMatrix, RenderSurfaceType* nearestAncestorThatMovesPixels, LayerList& renderSurfaceLayerList, LayerList& layerList, LayerSorter* layerSorter, int maxTextureSize)
{
    // This function computes the new matrix transformations recursively for this
    // layer and all its descendants. It also computes the appropriate render surfaces.
    // Some important points to remember:
    //
    // 0. Here, transforms are notated in Matrix x Vector order, and in words we describe what
    //    the transform does from left to right.
    //
    // 1. In our terminology, the "layer origin" refers to the top-left corner of a layer, and the
    //    positive Y-axis points downwards. This interpretation is valid because the orthographic
    //    projection applied at draw time flips the Y axis appropriately.
    //
    // 2. The anchor point, when given as a FloatPoint object, is specified in "unit layer space",
    //    where the bounds of the layer map to [0, 1]. However, as a TransformationMatrix object,
    //    the transform to the anchor point is specified in "pixel layer space", where the bounds
    //    of the layer map to [bounds.width(), bounds.height()].
    //
    // 3. The value of layer->position() is actually the position of the anchor point with respect to the position
    //    of the layer's origin. That is:
    //        layer->position() = positionOfLayerOrigin + anchorPoint (in pixel units)
    //
    //    Or, equivalently,
    //        positionOfLayerOrigin.x =  layer->position.x - (layer->anchorPoint.x * bounds.width)
    //        positionOfLayerOrigin.y =  layer->position.y - (layer->anchorPoint.y * bounds.height)
    //
    // 4. Definition of various transforms used:
    //        M[parent] is the parent matrix, with respect to the nearest render surface, passed down recursively.
    //        M[root] is the full hierarchy, with respect to the root, passed down recursively.
    //        Tr[origin] is the translation matrix from the parent's origin to this layer's origin.
    //        Tr[origin2anchor] is the translation from the layer's origin to its anchor point
    //        Tr[origin2center] is the translation from the layer's origin to its center
    //        M[layer] is the layer's matrix (applied at the anchor point)
    //        M[sublayer] is the layer's sublayer transform (applied at the layer's center)
    //        Tr[anchor2center] is the translation offset from the anchor point and the center of the layer
    //
    //    Some shortcuts and substitutions are used in the code to reduce matrix multiplications:
    //        Translating by the value of layer->position(), Tr[layer->position()] = Tr[origin] * Tr[origin2anchor]
    //        Tr[anchor2center] = Tr[origin2anchor].inverse() * Tr[origin2center]
    //
    //    Some composite transforms can help in understanding the sequence of transforms:
    //        compositeLayerTransform = Tr[origin2anchor] * M[layer] * Tr[origin2anchor].inverse()
    //        compositeSublayerTransform = Tr[origin2center] * M[sublayer] * Tr[origin2center].inverse()
    //
    //    In words, the layer transform is applied about the anchor point, and the sublayer transform is
    //    applied about the center of the layer.
    //
    // 5. When a layer (or render surface) is drawn, it is drawn into a "target render surface". Therefore the draw
    //    transform does not necessarily transform from screen space to local layer space. Instead, the draw transform
    //    is the transform between the "target render surface space" and local layer space. Note that render surfaces,
    //    except for the root, also draw themselves into a different target render surface, and so their draw
    //    transform and origin transforms are also described with respect to the target.
    //
    // Using these definitions, then:
    //
    // The draw transform for the layer is:
    //        M[draw] = M[parent] * Tr[origin] * compositeLayerTransform * Tr[origin2center]
    //                = M[parent] * Tr[layer->position()] * M[layer] * Tr[anchor2center]
    //
    //        Interpreting the math left-to-right, this transforms from the layer's render surface to the center of the layer.
    //
    // The screen space transform is:
    //        M[screenspace] = M[root] * Tr[origin] * compositeLayerTransform
    //                       = M[root] * Tr[layer->position()] * M[layer] * Tr[origin2anchor].inverse()
    //
    //        Interpreting the math left-to-right, this transforms from the root layer space to the local layer's origin.
    //
    // The transform hierarchy that is passed on to children (i.e. the child's parentMatrix) is:
    //        M[parent]_for_child = M[parent] * Tr[origin] * compositeLayerTransform * compositeSublayerTransform
    //                            = M[parent] * Tr[layer->position()] * M[layer] * Tr[anchor2center] * M[sublayer] * Tr[origin2center].inverse()
    //                            = M[draw] * M[sublayer] * Tr[origin2center].inverse()
    //
    //        and a similar matrix for the full hierarchy with respect to the root.
    //
    // Finally, note that the final matrix used by the shader for the layer is P * M[draw] * S . This final product
    // is computed in drawTexturedQuad(), where:
    //        P is the projection matrix
    //        S is the scale adjustment (to scale up to the layer size)
    //
    // When a render surface has a replica layer, that layer's transform is used to draw a second copy of the surface.
    // Transforms named here are relative to the surface, unless they specify they are relative to the replica layer.
    //
    // The render surface origin transform to its target surface origin is:
    //        M[surfaceOrigin] = M[owningLayer->Draw] * Tr[origin2center].inverse()
    //
    // The render surface origin transform to its the root (screen space) origin is:
    //        M[surface2root] = M[owningLayer->screenspace]
    //
    // The replica draw transform is:
    //        M[replicaDraw] = M[surfaceOrigin] * Tr[replica->position()] * Tr[replica] * Tr[anchor2center]
    //                       = M[owningLayer->draw] * Tr[origin2center].inverse() * Tr[replica->position()] * Tr[replica] * Tr[anchor2clippedCenter]
    //
    // The replica origin transform to its target surface origin is:
    //        M[replicaOrigin] = M[surfaceOrigin] * Tr[replica->position()] * Tr[replica] * Tr[origin2anchor].inverse()
    //
    // The replica origin transform to the root (screen space) origin is:
    //        M[replica2root] = M[surface2root] * Tr[replica->position()] * Tr[replica] * Tr[origin2anchor].inverse()
    //

    if (subtreeShouldBeSkipped(layer))
        return false;

    float drawOpacity = layer->opacity();
    bool drawOpacityIsAnimating = layer->opacityIsAnimating();
    if (layer->parent() && layer->parent()->preserves3D()) {
        drawOpacity *= layer->parent()->drawOpacity();
        drawOpacityIsAnimating |= layer->parent()->drawOpacityIsAnimating();
    }

    IntSize bounds = layer->bounds();
    FloatPoint anchorPoint = layer->anchorPoint();
    FloatPoint position = layer->position() - layer->scrollDelta();

    // Offset between anchor point and the center of the quad.
    float centerOffsetX = (0.5 - anchorPoint.x()) * bounds.width();
    float centerOffsetY = (0.5 - anchorPoint.y()) * bounds.height();

    TransformationMatrix layerLocalTransform;
    // LT = Tr[origin] * S[pageScaleDelta]
    layerLocalTransform.scale(layer->pageScaleDelta());
    // LT = Tr[origin] * S[pageScaleDelta] * Tr[origin2anchor]
    layerLocalTransform.translate3d(position.x(), position.y(), layer->anchorPointZ());
    // LT = Tr[origin] * S[pageScaleDelta] * Tr[origin2anchor] * M[layer]
    layerLocalTransform.multiply(layer->transform());
    // LT = Tr[origin] * S[pageScaleDelta] * Tr[origin2anchor] * M[layer] * Tr[anchor2center]
    layerLocalTransform.translate3d(centerOffsetX, centerOffsetY, -layer->anchorPointZ());

    TransformationMatrix combinedTransform = parentMatrix;
    combinedTransform = combinedTransform.multiply(layerLocalTransform);

    bool animatingTransformToTarget = layer->transformIsAnimating();
    bool animatingTransformToScreen = animatingTransformToTarget;
    if (layer->parent()) {
        animatingTransformToTarget |= layer->parent()->drawTransformIsAnimating();
        animatingTransformToScreen |= layer->parent()->screenSpaceTransformIsAnimating();
    }

    FloatRect layerRect(-0.5 * layer->bounds().width(), -0.5 * layer->bounds().height(), layer->bounds().width(), layer->bounds().height());
    IntRect transformedLayerRect;

    // fullHierarchyMatrix is the matrix that transforms objects between screen space (except projection matrix) and the most recent RenderSurface's space.
    // nextHierarchyMatrix will only change if this layer uses a new RenderSurface, otherwise remains the same.
    TransformationMatrix nextHierarchyMatrix = fullHierarchyMatrix;

    // FIXME: This seems like the wrong place to set this
    layer->setUsesLayerClipping(false);

    if (subtreeShouldRenderToSeparateSurface(layer, isScaleOrTranslation(combinedTransform))) {
        if (!layer->renderSurface())
            layer->createRenderSurface();

        RenderSurfaceType* renderSurface = layer->renderSurface();
        renderSurface->clearLayerList();

        // The origin of the new surface is the upper left corner of the layer.
        TransformationMatrix drawTransform;
        drawTransform.translate3d(0.5 * bounds.width(), 0.5 * bounds.height(), 0);
        layer->setDrawTransform(drawTransform);

        transformedLayerRect = IntRect(0, 0, bounds.width(), bounds.height());

        // The opacity value is moved from the layer to its surface, so that the entire subtree properly inherits opacity.
        renderSurface->setDrawOpacity(drawOpacity);
        renderSurface->setDrawOpacityIsAnimating(drawOpacityIsAnimating);
        layer->setDrawOpacity(1);
        layer->setDrawOpacityIsAnimating(false);

        TransformationMatrix surfaceOriginTransform = combinedTransform;
        surfaceOriginTransform.translate3d(-0.5 * bounds.width(), -0.5 * bounds.height(), 0);
        renderSurface->setOriginTransform(surfaceOriginTransform);

        renderSurface->setTargetSurfaceTransformsAreAnimating(animatingTransformToTarget);
        renderSurface->setScreenSpaceTransformsAreAnimating(animatingTransformToScreen);
        animatingTransformToTarget = false;
        layer->setDrawTransformIsAnimating(animatingTransformToTarget);
        layer->setScreenSpaceTransformIsAnimating(animatingTransformToScreen);

        // Update the aggregate hierarchy matrix to include the transform of the newly created RenderSurface.
        nextHierarchyMatrix.multiply(surfaceOriginTransform);

        // The render surface clipRect contributes to the scissor rect that needs to
        // be applied before drawing the render surface onto its containing
        // surface and is therefore expressed in the parent's coordinate system.
        renderSurface->setClipRect(layer->parent() ? layer->parent()->clipRect() : layer->clipRect());

        // The layer's clipRect can be reset here. The renderSurface will correctly clip the subtree.
        layer->setUsesLayerClipping(false);
        layer->setClipRect(IntRect());

        if (layer->maskLayer()) {
            renderSurface->setMaskLayer(layer->maskLayer());
            layer->maskLayer()->setTargetRenderSurface(renderSurface);
        } else
            renderSurface->setMaskLayer(0);

        if (layer->replicaLayer() && layer->replicaLayer()->maskLayer())
            layer->replicaLayer()->maskLayer()->setTargetRenderSurface(renderSurface);

        renderSurface->setFilters(layer->filters());
        if (renderSurface->filters().hasFilterThatMovesPixels())
            nearestAncestorThatMovesPixels = renderSurface;
        renderSurface->setNearestAncestorThatMovesPixels(nearestAncestorThatMovesPixels);

        renderSurface->setBackgroundFilters(layer->backgroundFilters());

        renderSurfaceLayerList.append(layer);
    } else {
        layer->setDrawTransform(combinedTransform);
        layer->setDrawTransformIsAnimating(animatingTransformToTarget);
        layer->setScreenSpaceTransformIsAnimating(animatingTransformToScreen);
        transformedLayerRect = enclosingIntRect(CCMathUtil::mapClippedRect(layer->drawTransform(), layerRect));

        layer->setDrawOpacity(drawOpacity);
        layer->setDrawOpacityIsAnimating(drawOpacityIsAnimating);

        if (layer != rootLayer) {
            ASSERT(layer->parent());
            layer->clearRenderSurface();

            // Layers inherit the clip rect from their parent.
            layer->setClipRect(layer->parent()->clipRect());
            if (layer->parent()->usesLayerClipping())
                layer->setUsesLayerClipping(true);

            // Layers without their own renderSurface will render into the nearest ancestor surface.
            layer->setTargetRenderSurface(layer->parent()->targetRenderSurface());
        }
    }

    if (layer->masksToBounds()) {
        IntRect clipRect = transformedLayerRect;

        // If the layer already inherited a clipRect, we need to intersect with it before
        // overriding the layer's clipRect and usesLayerClipping.
        if (layer->usesLayerClipping())
            clipRect.intersect(layer->clipRect());

        layer->setClipRect(clipRect);
        layer->setUsesLayerClipping(true);
    }

    // Note that at this point, layer->drawTransform() is not necessarily the same as local variable drawTransform.
    // layerScreenSpaceTransform represents the transform between root layer's "screen space" and local layer space.
    TransformationMatrix layerScreenSpaceTransform = nextHierarchyMatrix;
    layerScreenSpaceTransform.multiply(layer->drawTransform());
    layerScreenSpaceTransform.translate3d(-0.5 * bounds.width(), -0.5 * bounds.height(), 0);
    layer->setScreenSpaceTransform(layerScreenSpaceTransform);

    // drawableContentRect() is always stored in the coordinate system of the
    // RenderSurface the layer draws into.
    if (layer->drawsContent()) {
        IntRect drawableContentRect = transformedLayerRect;
        if (layer->usesLayerClipping())
            drawableContentRect.intersect(layer->clipRect());
        layer->setDrawableContentRect(drawableContentRect);
    } else
        layer->setDrawableContentRect(IntRect());

    TransformationMatrix sublayerMatrix = layer->drawTransform();

    // Flatten to 2D if the layer doesn't preserve 3D.
    if (!layer->preserves3D()) {
        sublayerMatrix.setM13(0);
        sublayerMatrix.setM23(0);
        sublayerMatrix.setM31(0);
        sublayerMatrix.setM32(0);
        sublayerMatrix.setM33(1);
        sublayerMatrix.setM34(0);
        sublayerMatrix.setM43(0);
    }

    // Apply the sublayer transform at the center of the layer.
    sublayerMatrix.multiply(layer->sublayerTransform());

    // The coordinate system given to children is located at the layer's origin, not the center.
    sublayerMatrix.translate3d(-bounds.width() * 0.5, -bounds.height() * 0.5, 0);

    LayerList& descendants = (layer->renderSurface() ? layer->renderSurface()->layerList() : layerList);

    // Any layers that are appended after this point are in the layer's subtree and should be included in the sorting process.
    unsigned sortingStartIndex = descendants.size();

    if (!layerShouldBeSkipped(layer))
        descendants.append(layer);

    for (size_t i = 0; i < layer->children().size(); ++i) {
        LayerType* child = layer->children()[i].get();
        bool drawsContent = calculateDrawTransformsAndVisibilityInternal<LayerType, LayerList, RenderSurfaceType, LayerSorter>(child, rootLayer, sublayerMatrix, nextHierarchyMatrix, nearestAncestorThatMovesPixels, renderSurfaceLayerList, descendants, layerSorter, maxTextureSize);

        if (drawsContent) {
            if (child->renderSurface()) {
                RenderSurfaceType* childRenderSurface = child->renderSurface();
                IntRect drawableContentRect = layer->drawableContentRect();
                drawableContentRect.unite(enclosingIntRect(childRenderSurface->drawableContentRect()));
                layer->setDrawableContentRect(drawableContentRect);
                descendants.append(child);
            } else {
                IntRect drawableContentRect = layer->drawableContentRect();
                drawableContentRect.unite(child->drawableContentRect());
                layer->setDrawableContentRect(drawableContentRect);
            }
        }
    }

    if (layer->masksToBounds() || layer->maskLayer()) {
        IntRect drawableContentRect = layer->drawableContentRect();
        drawableContentRect.intersect(transformedLayerRect);
        layer->setDrawableContentRect(drawableContentRect);
    }

    if (layer->renderSurface() && layer != rootLayer) {
        RenderSurfaceType* renderSurface = layer->renderSurface();
        IntRect clippedContentRect = layer->drawableContentRect();
        FloatPoint surfaceCenter = FloatRect(clippedContentRect).center();

        // Restrict the RenderSurface size to the portion that's visible.
        FloatSize centerOffsetDueToClipping;

        // Don't clip if the layer is reflected as the reflection shouldn't be
        // clipped.
        if (!layer->replicaLayer()) {
            if (!renderSurface->clipRect().isEmpty() && !clippedContentRect.isEmpty()) {
                IntRect surfaceClipRect = CCLayerTreeHostCommon::calculateVisibleRect(renderSurface->clipRect(), clippedContentRect, renderSurface->originTransform());
                clippedContentRect.intersect(surfaceClipRect);
            }
            FloatPoint clippedSurfaceCenter = FloatRect(clippedContentRect).center();
            centerOffsetDueToClipping = clippedSurfaceCenter - surfaceCenter;
        }

        // The RenderSurface backing texture cannot exceed the maximum supported
        // texture size.
        clippedContentRect.setWidth(std::min(clippedContentRect.width(), maxTextureSize));
        clippedContentRect.setHeight(std::min(clippedContentRect.height(), maxTextureSize));

        if (clippedContentRect.isEmpty())
            renderSurface->clearLayerList();

        renderSurface->setContentRect(clippedContentRect);

        // Since the layer starts a new render surface we need to adjust its
        // clipRect to be expressed in the new surface's coordinate system.
        layer->setClipRect(layer->drawableContentRect());

        // Adjust the origin of the transform to be the center of the render surface.
        TransformationMatrix drawTransform = renderSurface->originTransform();
        drawTransform.translate3d(surfaceCenter.x() + centerOffsetDueToClipping.width(), surfaceCenter.y() + centerOffsetDueToClipping.height(), 0);
        renderSurface->setDrawTransform(drawTransform);

        // The layer's origin is equal to the surface's origin so the screenSpaceTransform is the same.
        renderSurface->setScreenSpaceTransform(layer->screenSpaceTransform());

        if (layer->replicaLayer()) {
            // Compute the transformation matrix used to draw the surface's replica to the target surface.
            TransformationMatrix replicaDrawTransform = renderSurface->originTransform();
            replicaDrawTransform.translate(layer->replicaLayer()->position().x(), layer->replicaLayer()->position().y());
            replicaDrawTransform.multiply(layer->replicaLayer()->transform());
            replicaDrawTransform.translate(surfaceCenter.x() - anchorPoint.x() * bounds.width(), surfaceCenter.y() - anchorPoint.y() * bounds.height());
            renderSurface->setReplicaDrawTransform(replicaDrawTransform);

            TransformationMatrix surfaceOriginToReplicaOriginTransform;
            surfaceOriginToReplicaOriginTransform.translate(layer->replicaLayer()->position().x(), layer->replicaLayer()->position().y());
            surfaceOriginToReplicaOriginTransform.multiply(layer->replicaLayer()->transform());
            surfaceOriginToReplicaOriginTransform.translate(-anchorPoint.x() * bounds.width(), -anchorPoint.y() * bounds.height());

            // Compute the replica's "originTransform" that maps from the replica's origin space to the target surface origin space.
            TransformationMatrix replicaOriginTransform = layer->renderSurface()->originTransform() * surfaceOriginToReplicaOriginTransform;
            renderSurface->setReplicaOriginTransform(replicaOriginTransform);

            // Compute the replica's "screenSpaceTransform" that maps from the replica's origin space to the screen's origin space.
            TransformationMatrix replicaScreenSpaceTransform = layer->renderSurface()->screenSpaceTransform() * surfaceOriginToReplicaOriginTransform;
            renderSurface->setReplicaScreenSpaceTransform(replicaScreenSpaceTransform);
        }

        // If a render surface has no layer list, then it and none of its children needed to get drawn.
        if (!layer->renderSurface()->layerList().size()) {
            // FIXME: Originally we asserted that this layer was already at the end of the
            //        list, and only needed to remove that layer. For now, we remove the
            //        entire subtree of surfaces to fix a crash bug. The root cause is
            //        https://bugs.webkit.org/show_bug.cgi?id=74147 and we should be able
            //        to put the original assert after fixing that.
            while (renderSurfaceLayerList.last() != layer) {
                renderSurfaceLayerList.last()->clearRenderSurface();
                renderSurfaceLayerList.removeLast();
            }
            ASSERT(renderSurfaceLayerList.last() == layer);
            renderSurfaceLayerList.removeLast();
            layer->clearRenderSurface();
            return false;
        }
    }

    // If neither this layer nor any of its children were added, early out.
    if (sortingStartIndex == descendants.size())
        return false;

    // If preserves-3d then sort all the descendants in 3D so that they can be
    // drawn from back to front. If the preserves-3d property is also set on the parent then
    // skip the sorting as the parent will sort all the descendants anyway.
    if (descendants.size() && layer->preserves3D() && (!layer->parent() || !layer->parent()->preserves3D()))
        sortLayers(&descendants.at(sortingStartIndex), descendants.end(), layerSorter);

    return true;
}

// FIXME: Instead of using the following function to set visibility rects on a second
// tree pass, revise calculateVisibleLayerRect() so that this can be done in a single
// pass inside calculateDrawTransformsAndVisibilityInternal<>().
template<typename LayerType, typename LayerList, typename RenderSurfaceType>
static void walkLayersAndCalculateVisibleLayerRects(const LayerList& renderSurfaceLayerList)
{
    // Use BackToFront since it's cheap and this isn't order-dependent.
    typedef CCLayerIterator<LayerType, LayerList, RenderSurfaceType, CCLayerIteratorActions::BackToFront> CCLayerIteratorType;

    CCLayerIteratorType end = CCLayerIteratorType::end(&renderSurfaceLayerList);
    for (CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList); it != end; ++it) {
        if (!it.representsTargetRenderSurface()) {
            IntRect visibleLayerRect = calculateVisibleLayerRect(*it);
            it->setVisibleLayerRect(visibleLayerRect);
        }
    }
}

void CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(LayerChromium* layer, LayerChromium* rootLayer, const TransformationMatrix& parentMatrix, const TransformationMatrix& fullHierarchyMatrix, Vector<RefPtr<LayerChromium> >& renderSurfaceLayerList, Vector<RefPtr<LayerChromium> >& layerList, int maxTextureSize)
{
    WebCore::calculateDrawTransformsAndVisibilityInternal<LayerChromium, Vector<RefPtr<LayerChromium> >, RenderSurfaceChromium, void>(layer, rootLayer, parentMatrix, fullHierarchyMatrix, 0, renderSurfaceLayerList, layerList, 0, maxTextureSize);
    walkLayersAndCalculateVisibleLayerRects<LayerChromium, Vector<RefPtr<LayerChromium> >, RenderSurfaceChromium>(renderSurfaceLayerList);
}

void CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(CCLayerImpl* layer, CCLayerImpl* rootLayer, const TransformationMatrix& parentMatrix, const TransformationMatrix& fullHierarchyMatrix, Vector<CCLayerImpl*>& renderSurfaceLayerList, Vector<CCLayerImpl*>& layerList, CCLayerSorter* layerSorter, int maxTextureSize)
{
    calculateDrawTransformsAndVisibilityInternal<CCLayerImpl, Vector<CCLayerImpl*>, CCRenderSurface, CCLayerSorter>(layer, rootLayer, parentMatrix, fullHierarchyMatrix, 0, renderSurfaceLayerList, layerList, layerSorter, maxTextureSize);
    walkLayersAndCalculateVisibleLayerRects<CCLayerImpl, Vector<CCLayerImpl*>, CCRenderSurface>(renderSurfaceLayerList);
}

} // namespace WebCore
