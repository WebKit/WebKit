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
#include "WebPrivatePtr.h"
#include "WebString.h"
#include "WebVector.h"

class SkMatrix44;
namespace WebCore { class LayerChromium; }

namespace WebKit {
class WebAnimationDelegate;
class WebFilterOperations;
class WebTransformationMatrix;
struct WebFloatPoint;
struct WebFloatRect;
struct WebSize;

class WebLayer {
public:
    WEBKIT_EXPORT static WebLayer create();

    WebLayer() { }
    WebLayer(const WebLayer& layer) { assign(layer); }
    virtual ~WebLayer() { reset(); }
    WebLayer& operator=(const WebLayer& layer)
    {
        assign(layer);
        return *this;
    }
    bool isNull() const { return m_private.isNull(); }
    WEBKIT_EXPORT void reset();
    WEBKIT_EXPORT void assign(const WebLayer&);
    WEBKIT_EXPORT bool equals(const WebLayer&) const;

    // Sets a region of the layer as invalid, i.e. needs to update its content.
    WEBKIT_EXPORT void invalidateRect(const WebFloatRect&);

    // Sets the entire layer as invalid, i.e. needs to update its content.
    WEBKIT_EXPORT void invalidate();

    WEBKIT_EXPORT WebLayer rootLayer() const;
    WEBKIT_EXPORT WebLayer parent() const;
    WEBKIT_EXPORT size_t numberOfChildren() const;
    WEBKIT_EXPORT WebLayer childAt(size_t) const;

    WEBKIT_EXPORT void addChild(const WebLayer&);
    WEBKIT_EXPORT void insertChild(const WebLayer&, size_t index);
    WEBKIT_EXPORT void replaceChild(const WebLayer& reference, const WebLayer& newLayer);
    WEBKIT_EXPORT void setChildren(const WebVector<WebLayer>&);
    WEBKIT_EXPORT void removeFromParent();
    WEBKIT_EXPORT void removeAllChildren();

    WEBKIT_EXPORT void setAnchorPoint(const WebFloatPoint&);
    WEBKIT_EXPORT WebFloatPoint anchorPoint() const;

    WEBKIT_EXPORT void setAnchorPointZ(float);
    WEBKIT_EXPORT float anchorPointZ() const;

    WEBKIT_EXPORT void setBounds(const WebSize&);
    WEBKIT_EXPORT WebSize bounds() const;

    WEBKIT_EXPORT void setMasksToBounds(bool);
    WEBKIT_EXPORT bool masksToBounds() const;

    WEBKIT_EXPORT void setMaskLayer(const WebLayer&);
    WEBKIT_EXPORT WebLayer maskLayer() const;

    WEBKIT_EXPORT void setReplicaLayer(const WebLayer&);

    WEBKIT_EXPORT void setOpacity(float);
    WEBKIT_EXPORT float opacity() const;

    WEBKIT_EXPORT void setOpaque(bool);
    WEBKIT_EXPORT bool opaque() const;

    WEBKIT_EXPORT void setPosition(const WebFloatPoint&);
    WEBKIT_EXPORT WebFloatPoint position() const;

    WEBKIT_EXPORT void setSublayerTransform(const SkMatrix44&);
    WEBKIT_EXPORT void setSublayerTransform(const WebTransformationMatrix&);
    WEBKIT_EXPORT SkMatrix44 sublayerTransform() const;

    WEBKIT_EXPORT void setTransform(const SkMatrix44&);
    WEBKIT_EXPORT void setTransform(const WebTransformationMatrix&);
    WEBKIT_EXPORT SkMatrix44 transform() const;

    // Sets whether the layer draws its content when compositing.
    WEBKIT_EXPORT void setDrawsContent(bool);
    WEBKIT_EXPORT bool drawsContent() const;

    WEBKIT_EXPORT void setPreserves3D(bool);

    // Mark that this layer should use its parent's transform and double-sided
    // properties in determining this layer's backface visibility instead of
    // using its own properties. If this property is set, this layer must
    // have a parent, and the parent may not have this property set.
    // Note: This API is to work around issues with visibility the handling of
    // WebKit layers that have a contents layer (canvas, plugin, WebGL, video,
    // etc).
    WEBKIT_EXPORT void setUseParentBackfaceVisibility(bool);

    WEBKIT_EXPORT void setBackgroundColor(WebColor);

    // Clear the filters in use by passing in a newly instantiated
    // WebFilterOperations object.
    WEBKIT_EXPORT void setFilters(const WebFilterOperations&);

    // Apply filters to pixels that show through the background of this layer.
    // Note: These filters are only possible on layers that are drawn directly
    // to a root render surface with an opaque background. This means if an
    // ancestor of the background-filtered layer sets certain properties
    // (opacity, transforms), it may conflict and hide the background filters.
    WEBKIT_EXPORT void setBackgroundFilters(const WebFilterOperations&);

    WEBKIT_EXPORT void setDebugBorderColor(const WebColor&);
    WEBKIT_EXPORT void setDebugBorderWidth(float);
    WEBKIT_EXPORT void setDebugName(WebString);

    // An animation delegate is notified when animations are started and
    // stopped. The WebLayer does not take ownership of the delegate, and it is
    // the responsibility of the client to reset the layer's delegate before
    // deleting the delegate.
    WEBKIT_EXPORT void setAnimationDelegate(WebAnimationDelegate*);

    // Returns false if the animation cannot be added.
    WEBKIT_EXPORT bool addAnimation(WebAnimation*);

    // Removes all animations with the given id.
    WEBKIT_EXPORT void removeAnimation(int animationId);

    // Removes all animations with the given id targeting the given property.
    WEBKIT_EXPORT void removeAnimation(int animationId, WebAnimation::TargetProperty);

    // Pauses all animations with the given id.
    WEBKIT_EXPORT void pauseAnimation(int animationId, double timeOffset);

    // The following functions suspend and resume all animations. The given time
    // is assumed to use the same time base as monotonicallyIncreasingTime().
    WEBKIT_EXPORT void suspendAnimations(double monotonicTime);
    WEBKIT_EXPORT void resumeAnimations(double monotonicTime);

    // Returns true if this layer has any active animations - useful for tests.
    WEBKIT_EXPORT bool hasActiveAnimation();

    // Transfers all animations running on the current layer.
    WEBKIT_EXPORT void transferAnimationsTo(WebLayer*);

    // DEPRECATED.
    // This requests that this layer's compositor-managed textures always be reserved
    // when determining texture limits.
    WEBKIT_EXPORT void setAlwaysReserveTextures(bool);

    // Forces this layer to use a render surface. There is no benefit in doing
    // so, but this is to facilitate benchmarks and tests.
    WEBKIT_EXPORT void setForceRenderSurface(bool);

    // Drops this layer's render surface, if it has one. Used to break cycles in some
    // cases - if you aren't sure, you don't need to call this.
    WEBKIT_EXPORT void clearRenderSurface();

    template<typename T> T to()
    {
        T res;
        res.WebLayer::assign(*this);
        return res;
    }

    template<typename T> const T toConst() const
    {
        T res;
        res.WebLayer::assign(*this);
        return res;
    }

#if WEBKIT_IMPLEMENTATION
    WebLayer(const WTF::PassRefPtr<WebCore::LayerChromium>&);
    WebLayer& operator=(const WTF::PassRefPtr<WebCore::LayerChromium>&);
    operator WTF::PassRefPtr<WebCore::LayerChromium>() const;
    template<typename T> T* unwrap() const
    {
        return static_cast<T*>(m_private.get());
    }

    template<typename T> const T* constUnwrap() const
    {
        return static_cast<const T*>(m_private.get());
    }
#endif

protected:
    WebPrivatePtr<WebCore::LayerChromium> m_private;
};

inline bool operator==(const WebLayer& a, const WebLayer& b)
{
    return a.equals(b);
}

inline bool operator!=(const WebLayer& a, const WebLayer& b)
{
    return !(a == b);
}

} // namespace WebKit

#endif // WebLayer_h
