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
    // Compute the new matrix transformation that will be applied to this layer and
    // all its children. It's important to remember that the layer's position
    // is the position of the layer's anchor point. Also, the coordinate system used
    // assumes that the origin is at the lower left even though the coordinates the browser
    // gives us for the layers are for the upper left corner. The Y flip happens via
    // the orthographic projection applied at render time.
    // The transformation chain for the layer is (using the Matrix x Vector order):
    // M = M[p] * Tr[l] * M[l] * Tr[c]
    // Where M[p] is the parent matrix passed down to the function
    //       Tr[l] is the translation matrix locating the layer's anchor point
    //       Tr[c] is the translation offset between the anchor point and the center of the layer
    //       M[l] is the layer's matrix (applied at the anchor point)
    // This transform creates a coordinate system whose origin is the center of the layer.
    // Note that the final matrix used by the shader for the layer is P * M * S . This final product
    // is computed in drawTexturedQuad().
    // Where: P is the projection matrix
    //        M is the layer's matrix computed above
    //        S is the scale adjustment (to scale up to the layer size)
    IntSize bounds = layer->bounds();
    FloatPoint anchorPoint = layer->anchorPoint();
    FloatPoint position = layer->position() - layer->scrollDelta();

    // Offset between anchor point and the center of the quad.
    float centerOffsetX = (0.5 - anchorPoint.x()) * bounds.width();
    float centerOffsetY = (0.5 - anchorPoint.y()) * bounds.height();

    TransformationMatrix layerLocalTransform;
    // LT = Tr[l]
    layerLocalTransform.translate3d(position.x(), position.y(), layer->anchorPointZ());
    // LT = Tr[l] * M[l]
    layerLocalTransform.multiply(layer->transform());
    // LT = Tr[l] * M[l] * Tr[c]
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
        // DT = M[p] * LT
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
    // layerScreenSpaceTransform represents the transform between layer space (in pixels) and screen space.
    // So we do not include projection P or scaling transform S (see comments above that describe P*M*S).
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

    // The origin of the children is the top left corner of the layer, not the
    // center. The matrix passed down to the children is therefore:
    // M[s] = M * Tr[-center]
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
