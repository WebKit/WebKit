/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebLayer_h
#define WebLayer_h

#include "WebAnimation.h"
#include "WebColor.h"
#include "WebCommon.h"
#include "WebPoint.h"
#include "WebPrivatePtr.h"
#include "WebRect.h"
#include "WebString.h"
#include "WebVector.h"

class SkMatrix44;

namespace WebKit {
class WebAnimationDelegate;
class WebFilterOperations;
class WebTransformationMatrix;
struct WebFloatPoint;
struct WebFloatRect;
struct WebSize;

class WebLayerImpl;

class WebLayer {
public:
#define WEBLAYER_IS_PURE_VIRTUAL
    WEBKIT_EXPORT static WebLayer* create();

    virtual ~WebLayer() { }

    // Sets a region of the layer as invalid, i.e. needs to update its content.
    virtual void invalidateRect(const WebFloatRect&) = 0;

    // Sets the entire layer as invalid, i.e. needs to update its content.
    virtual void invalidate() = 0;

    // These functions do not take ownership of the WebLayer* parameter.
    virtual void addChild(WebLayer*) = 0;
    virtual void insertChild(WebLayer*, size_t index) = 0;
    virtual void replaceChild(WebLayer* reference, WebLayer* newLayer) = 0;
    virtual void setChildren(const WebVector<WebLayer*>&) = 0;
    virtual void removeFromParent() = 0;
    virtual void removeAllChildren() = 0;

    virtual void setAnchorPoint(const WebFloatPoint&) = 0;
    virtual WebFloatPoint anchorPoint() const = 0;

    virtual void setAnchorPointZ(float) = 0;
    virtual float anchorPointZ() const = 0;

    virtual void setBounds(const WebSize&) = 0;
    virtual WebSize bounds() const = 0;

    virtual void setMasksToBounds(bool) = 0;
    virtual bool masksToBounds() const = 0;

    virtual void setMaskLayer(WebLayer*) = 0;
    virtual void setReplicaLayer(WebLayer*) = 0;

    virtual void setOpacity(float) = 0;
    virtual float opacity() const = 0;

    virtual void setOpaque(bool) = 0;
    virtual bool opaque() const = 0;

    virtual void setPosition(const WebFloatPoint&) = 0;
    virtual WebFloatPoint position() const = 0;

    virtual void setSublayerTransform(const SkMatrix44&) = 0;
    virtual void setSublayerTransform(const WebTransformationMatrix&) = 0;
    virtual SkMatrix44 sublayerTransform() const = 0;

    virtual void setTransform(const SkMatrix44&) = 0;
    virtual void setTransform(const WebTransformationMatrix&) = 0;
    virtual SkMatrix44 transform() const = 0;

    // Sets whether the layer draws its content when compositing.
    virtual void setDrawsContent(bool) = 0;
    virtual bool drawsContent() const = 0;

    virtual void setPreserves3D(bool) = 0;

    // Mark that this layer should use its parent's transform and double-sided
    // properties in determining this layer's backface visibility instead of
    // using its own properties. If this property is set, this layer must
    // have a parent, and the parent may not have this property set.
    // Note: This API is to work around issues with visibility the handling of
    // WebKit layers that have a contents layer (canvas, plugin, WebGL, video,
    // etc).
    virtual void setUseParentBackfaceVisibility(bool) = 0;

    virtual void setBackgroundColor(WebColor) = 0;

    // Clear the filters in use by passing in a newly instantiated
    // WebFilterOperations object.
    virtual void setFilters(const WebFilterOperations&) = 0;

    // Apply filters to pixels that show through the background of this layer.
    // Note: These filters are only possible on layers that are drawn directly
    // to a root render surface with an opaque background. This means if an
    // ancestor of the background-filtered layer sets certain properties
    // (opacity, transforms), it may conflict and hide the background filters.
    virtual void setBackgroundFilters(const WebFilterOperations&) = 0;

    virtual void setDebugBorderColor(const WebColor&) = 0;
    virtual void setDebugBorderWidth(float) = 0;
    virtual void setDebugName(WebString) = 0;

    // An animation delegate is notified when animations are started and
    // stopped. The WebLayer does not take ownership of the delegate, and it is
    // the responsibility of the client to reset the layer's delegate before
    // deleting the delegate.
    virtual void setAnimationDelegate(WebAnimationDelegate*) = 0;

    // Returns false if the animation cannot be added.
    virtual bool addAnimation(WebAnimation*) = 0;

    // Removes all animations with the given id.
    virtual void removeAnimation(int animationId) = 0;

    // Removes all animations with the given id targeting the given property.
    virtual void removeAnimation(int animationId, WebAnimation::TargetProperty) = 0;

    // Pauses all animations with the given id.
    virtual void pauseAnimation(int animationId, double timeOffset) = 0;

    // The following functions suspend and resume all animations. The given time
    // is assumed to use the same time base as monotonicallyIncreasingTime().
    virtual void suspendAnimations(double monotonicTime) = 0;
    virtual void resumeAnimations(double monotonicTime) = 0;

    // Returns true if this layer has any active animations - useful for tests.
    virtual bool hasActiveAnimation() = 0;

    // Transfers all animations running on the current layer.
    virtual void transferAnimationsTo(WebLayer*) = 0;

    // Scrolling
    virtual void setScrollPosition(WebPoint) = 0;
    virtual void setScrollable(bool) = 0;
    virtual void setHaveWheelEventHandlers(bool) = 0;
    virtual void setShouldScrollOnMainThread(bool) = 0;
    virtual void setNonFastScrollableRegion(const WebVector<WebRect>&) = 0;
    virtual void setIsContainerForFixedPositionLayers(bool) = 0;
    virtual void setFixedToContainerLayer(bool) = 0;

    // Forces this layer to use a render surface. There is no benefit in doing
    // so, but this is to facilitate benchmarks and tests.
    virtual void setForceRenderSurface(bool) = 0;
};

} // namespace WebKit

#endif // WebLayer_h
