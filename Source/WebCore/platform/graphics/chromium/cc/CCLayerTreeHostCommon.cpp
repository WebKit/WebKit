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
#include "cc/CCLayerImpl.h"
#include "cc/CCLayerSorter.h"
#include "cc/CCRenderSurface.h"

namespace WebCore {

IntRect CCLayerTreeHostCommon::calculateVisibleRect(const IntRect& targetSurfaceRect, const IntRect& layerBoundRect, const TransformationMatrix& transform)
{
    // Is this layer fully contained within the target surface?
    IntRect layerInSurfaceSpace = transform.mapRect(layerBoundRect);
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
    IntRect layerRect = surfaceToLayer.projectQuad(FloatQuad(FloatRect(minimalSurfaceRect))).enclosingBoundingBox();
    layerRect.intersect(layerBoundRect);
    return layerRect;
}

IntRect CCLayerTreeHostCommon::calculateVisibleLayerRect(const IntRect& targetSurfaceRect, const IntSize& bounds, const IntSize& contentBounds, const TransformationMatrix& tilingTransform)
{
    if (targetSurfaceRect.isEmpty() || contentBounds.isEmpty())
        return targetSurfaceRect;

    const IntRect layerBoundRect = IntRect(IntPoint(), contentBounds);
    TransformationMatrix transform = tilingTransform;

    transform.scaleNonUniform(bounds.width() / static_cast<double>(contentBounds.width()),
                              bounds.height() / static_cast<double>(contentBounds.height()));
    transform.translate(-contentBounds.width() / 2.0, -contentBounds.height() / 2.0);

    return calculateVisibleRect(targetSurfaceRect, layerBoundRect, transform);
}

static bool isScaleOrTranslation(const TransformationMatrix& m)
{
    return !m.m12() && !m.m13() && !m.m14()
           && !m.m21() && !m.m23() && !m.m24()
           && !m.m31() && !m.m32() && !m.m43()
           && m.m44();

}

// Recursively walks the layer tree starting at the given node and computes all the
// necessary transformations, scissor rectangles, render surfaces, etc.
template<typename LayerType, typename RenderSurfaceType, typename LayerSorter>
static void calculateDrawTransformsAndVisibilityInternal(LayerType* layer, LayerType* rootLayer, const TransformationMatrix& parentMatrix, const TransformationMatrix& fullHierarchyMatrix, Vector<RefPtr<LayerType> >& renderSurfaceLayerList, Vector<RefPtr<LayerType> >& layerList, LayerSorter* layerSorter, int maxTextureSize)
{
    typedef Vector<RefPtr<LayerType> > LayerList;
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

    IntSize bounds = layer->bounds();
    FloatPoint anchorPoint = layer->anchorPoint();
    FloatPoint position = layer->position() - layer->scrollDelta();

    // Offset between anchor point and the center of the quad.
    float centerOffsetX = (0.5 - anchorPoint.x()) * bounds.width();
    float centerOffsetY = (0.5 - anchorPoint.y()) * bounds.height();

    TransformationMatrix layerLocalTransform;
    // LT = Tr[origin] * Tr[origin2anchor]
    layerLocalTransform.translate3d(position.x(), position.y(), layer->anchorPointZ());
    // LT = Tr[origin] * Tr[origin2anchor] * M[layer]
    layerLocalTransform.multiply(layer->transform());
    // LT = Tr[origin] * Tr[origin2anchor] * M[layer] * Tr[anchor2center]
    layerLocalTransform.translate3d(centerOffsetX, centerOffsetY, -layer->anchorPointZ());

    TransformationMatrix combinedTransform = parentMatrix;
    combinedTransform = combinedTransform.multiply(layerLocalTransform);

    FloatRect layerRect(-0.5 * layer->bounds().width(), -0.5 * layer->bounds().height(), layer->bounds().width(), layer->bounds().height());
    IntRect transformedLayerRect;

    // fullHierarchyMatrix is the matrix that transforms objects between screen space (except projection matrix) and the most recent RenderSurface's space.
    // nextHierarchyMatrix will only change if this layer uses a new RenderSurface, otherwise remains the same.
    TransformationMatrix nextHierarchyMatrix = fullHierarchyMatrix;

    // FIXME: This seems like the wrong place to set this
    layer->setUsesLayerScissor(false);

    // The layer and its descendants render on a new RenderSurface if any of
    // these conditions hold:
    // 1. The layer clips its descendants and its transform is not a simple translation.
    // 2. If the layer has opacity != 1 and does not have a preserves-3d transform style.
    // 3. The layer uses a mask
    // 4. The layer has a replica (used for reflections)
    // 5. The layer doesn't preserve-3d but is the child of a layer which does.
    // If a layer preserves-3d then we don't create a RenderSurface for it to avoid flattening
    // out its children. The opacity value of the children layers is multiplied by the opacity
    // of their parent.
    bool useSurfaceForClipping = layer->masksToBounds() && !isScaleOrTranslation(combinedTransform);
    bool useSurfaceForOpacity = layer->opacity() != 1 && !layer->preserves3D();
    bool useSurfaceForMasking = layer->maskLayer();
    bool useSurfaceForReflection = layer->replicaLayer();
    bool useSurfaceForFlatDescendants = layer->parent() && layer->parent()->preserves3D() && !layer->preserves3D() && layer->descendantDrawsContent();
    if (useSurfaceForMasking || useSurfaceForReflection || useSurfaceForFlatDescendants || ((useSurfaceForClipping || useSurfaceForOpacity) && layer->descendantDrawsContent())) {
        if (!layer->renderSurface())
            layer->createRenderSurface();
        RenderSurfaceType* renderSurface = layer->renderSurface();

        // The origin of the new surface is the upper left corner of the layer.
        TransformationMatrix drawTransform;
        drawTransform.translate3d(0.5 * bounds.width(), 0.5 * bounds.height(), 0);
        layer->setDrawTransform(drawTransform);

        transformedLayerRect = IntRect(0, 0, bounds.width(), bounds.height());

        // Layer's opacity will be applied when drawing the render surface.
        float drawOpacity = layer->opacity();
        if (layer->parent() && layer->parent()->preserves3D())
            drawOpacity *= layer->parent()->drawOpacity();
        renderSurface->setDrawOpacity(drawOpacity);
        layer->setDrawOpacity(1);

        TransformationMatrix layerOriginTransform = combinedTransform;
        layerOriginTransform.translate3d(-0.5 * bounds.width(), -0.5 * bounds.height(), 0);
        renderSurface->setOriginTransform(layerOriginTransform);

        // Update the aggregate hierarchy matrix to include the transform of the newly created RenderSurface.
        nextHierarchyMatrix.multiply(layerOriginTransform);

        // The render surface scissor rect is the scissor rect that needs to
        // be applied before drawing the render surface onto its containing
        // surface and is therefore expressed in the parent's coordinate system.
        renderSurface->setScissorRect(layer->parent() ? layer->parent()->scissorRect() : layer->scissorRect());

        renderSurface->clearLayerList();

        if (layer->maskLayer()) {
            renderSurface->setMaskLayer(layer->maskLayer());
            layer->maskLayer()->setTargetRenderSurface(renderSurface);
        } else
            renderSurface->setMaskLayer(0);

        if (layer->replicaLayer() && layer->replicaLayer()->maskLayer())
            layer->replicaLayer()->maskLayer()->setTargetRenderSurface(renderSurface);

        renderSurfaceLayerList.append(layer);
    } else {
        layer->setDrawTransform(combinedTransform);
        transformedLayerRect = enclosingIntRect(layer->drawTransform().mapRect(layerRect));

        layer->setDrawOpacity(layer->opacity());

        if (layer->parent()) {
            if (layer->parent()->preserves3D())
               layer->setDrawOpacity(layer->drawOpacity() * layer->parent()->drawOpacity());

            // Layers inherit the scissor rect from their parent.
            layer->setScissorRect(layer->parent()->scissorRect());
            if (layer->parent()->usesLayerScissor())
                layer->setUsesLayerScissor(true);

            layer->setTargetRenderSurface(layer->parent()->targetRenderSurface());
        }

        if (layer != rootLayer)
            layer->clearRenderSurface();

        if (layer->masksToBounds()) {
            IntRect scissor = transformedLayerRect;
            scissor.intersect(layer->scissorRect());
            layer->setScissorRect(scissor);
            layer->setUsesLayerScissor(true);
        }
    }

    // Note that at this point, layer->drawTransform() is not necessarily the same as local variable drawTransform.
    // layerScreenSpaceTransform represents the transform between root layer's "screen space" and local layer space.
    TransformationMatrix layerScreenSpaceTransform = nextHierarchyMatrix;
    layerScreenSpaceTransform.multiply(layer->drawTransform());
    layerScreenSpaceTransform.translate3d(-0.5 * bounds.width(), -0.5 * bounds.height(), 0);
    layer->setScreenSpaceTransform(layerScreenSpaceTransform);

    if (layer->renderSurface())
        layer->setTargetRenderSurface(layer->renderSurface());
    else {
        ASSERT(layer->parent());
        layer->setTargetRenderSurface(layer->parent()->targetRenderSurface());
    }

    // drawableContentRect() is always stored in the coordinate system of the
    // RenderSurface the layer draws into.
    if (layer->drawsContent()) {
        IntRect drawableContentRect = transformedLayerRect;
        if (layer->usesLayerScissor())
            drawableContentRect.intersect(layer->scissorRect());
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
    descendants.append(layer);

    unsigned thisLayerIndex = descendants.size() - 1;

    for (size_t i = 0; i < layer->children().size(); ++i) {
        LayerType* child = layer->children()[i].get();
        calculateDrawTransformsAndVisibilityInternal<LayerType, RenderSurfaceType, LayerSorter>(child, rootLayer, sublayerMatrix, nextHierarchyMatrix, renderSurfaceLayerList, descendants, layerSorter, maxTextureSize);

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

    if (layer->masksToBounds() || useSurfaceForMasking) {
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
            if (!renderSurface->scissorRect().isEmpty() && !clippedContentRect.isEmpty()) {
                IntRect surfaceScissorRect = CCLayerTreeHostCommon::calculateVisibleRect(renderSurface->scissorRect(), clippedContentRect, renderSurface->originTransform());
                clippedContentRect.intersect(surfaceScissorRect);
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
        // scissor rect to be expressed in the new surface's coordinate system.
        layer->setScissorRect(layer->drawableContentRect());

        // Adjust the origin of the transform to be the center of the render surface.
        TransformationMatrix drawTransform = renderSurface->originTransform();
        drawTransform.translate3d(surfaceCenter.x() + centerOffsetDueToClipping.width(), surfaceCenter.y() + centerOffsetDueToClipping.height(), 0);
        renderSurface->setDrawTransform(drawTransform);

        // Compute the transformation matrix used to draw the replica of the render
        // surface.
        if (layer->replicaLayer()) {
            TransformationMatrix replicaDrawTransform = renderSurface->originTransform();
            replicaDrawTransform.translate3d(layer->replicaLayer()->position().x(), layer->replicaLayer()->position().y(), 0);
            replicaDrawTransform.multiply(layer->replicaLayer()->transform());
            replicaDrawTransform.translate3d(surfaceCenter.x() - anchorPoint.x() * bounds.width(), surfaceCenter.y() - anchorPoint.y() * bounds.height(), 0);
            renderSurface->setReplicaDrawTransform(replicaDrawTransform);
        }
    }

    // If preserves-3d then sort all the descendants in 3D so that they can be
    // drawn from back to front. If the preserves-3d property is also set on the parent then
    // skip the sorting as the parent will sort all the descendants anyway.
    if (layer->preserves3D() && (!layer->parent() || !layer->parent()->preserves3D()))
        sortLayers(&descendants.at(thisLayerIndex), descendants.end(), layerSorter);
}

void CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(LayerChromium* layer, LayerChromium* rootLayer, const TransformationMatrix& parentMatrix, const TransformationMatrix& fullHierarchyMatrix, Vector<RefPtr<LayerChromium> >& renderSurfaceLayerList, Vector<RefPtr<LayerChromium> >& layerList, int maxTextureSize)
{
    return WebCore::calculateDrawTransformsAndVisibilityInternal<LayerChromium, RenderSurfaceChromium, void*>(layer, rootLayer, parentMatrix, fullHierarchyMatrix, renderSurfaceLayerList, layerList, 0, maxTextureSize);
}

void CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(CCLayerImpl* layer, CCLayerImpl* rootLayer, const TransformationMatrix& parentMatrix, const TransformationMatrix& fullHierarchyMatrix, Vector<RefPtr<CCLayerImpl> >& renderSurfaceLayerList, Vector<RefPtr<CCLayerImpl> >& layerList, CCLayerSorter* layerSorter, int maxTextureSize)
{
    return calculateDrawTransformsAndVisibilityInternal<CCLayerImpl, CCRenderSurface, CCLayerSorter>(layer, rootLayer, parentMatrix, fullHierarchyMatrix, renderSurfaceLayerList, layerList, layerSorter, maxTextureSize);
}

} // namespace WebCore
