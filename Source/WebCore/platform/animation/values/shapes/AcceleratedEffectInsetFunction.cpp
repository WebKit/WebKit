/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AcceleratedEffectInsetFunction.h"

#if ENABLE(THREADED_ANIMATIONS)

#include "AcceleratedEffectAnimationUtilities.h"
#include "Path.h"
#include <wtf/TinyLRUCache.h>

namespace WebCore {

// MARK: - Path Caching

struct AcceleratedEffectInsetPathPolicy : public TinyLRUCachePolicy<FloatRoundedRect, WebCore::Path> {
public:
    static bool NODELETE isKeyNull(const FloatRoundedRect& rect)
    {
        return rect.isEmpty();
    }

    static WebCore::Path createValueForKey(const FloatRoundedRect& rect)
    {
        WebCore::Path path;
        path.addRoundedRect(rect, PathRoundedRect::Strategy::PreferBezier);
        return path;
    }
};

static const WebCore::Path& cachedRoundedInsetPath(const FloatRoundedRect& rect)
{
    static NeverDestroyed<TinyLRUCache<FloatRoundedRect, WebCore::Path, 4, AcceleratedEffectInsetPathPolicy>> cache;
    return cache.get().get(rect);
}

// MARK: - Path Evaluation

Path path(const AcceleratedEffectInsetFunction& value, const FloatRect& boundingBox)
{
    auto boundingSize = boundingBox.size();

    auto left = value.insets.left();
    auto top = value.insets.top();
    auto rect = FloatRect {
        left + boundingBox.x(),
        top + boundingBox.y(),
        std::max<float>(boundingSize.width() - left - value.insets.right(), 0),
        std::max<float>(boundingSize.height() - top - value.insets.bottom(), 0)
    };

    auto radii = value.radii;
    radii.scale(calcBorderRadiiConstraintScaleFor(rect, radii));

    return cachedRoundedInsetPath(FloatRoundedRect { rect, radii });
}

// MARK: - Blending

static RectEdges<float> blendRectEdges(const RectEdges<float>& from, const RectEdges<float>& to, const BlendingContext& context)
{
    return RectEdges<float> {
        WebCore::blendForAcceleratedEffect(from.top(), to.top(), context),
        WebCore::blendForAcceleratedEffect(from.right(), to.right(), context),
        WebCore::blendForAcceleratedEffect(from.bottom(), to.bottom(), context),
        WebCore::blendForAcceleratedEffect(from.left(), to.left(), context),
    };
}

static CornerRadii blendCornerRadii(const CornerRadii& from, const CornerRadii& to, const BlendingContext& context)
{
    return CornerRadii {
        WebCore::blendForAcceleratedEffect(from.topLeft(), to.topLeft(), context),
        WebCore::blendForAcceleratedEffect(from.topRight(), to.topRight(), context),
        WebCore::blendForAcceleratedEffect(from.bottomLeft(), to.bottomLeft(), context),
        WebCore::blendForAcceleratedEffect(from.bottomRight(), to.bottomRight(), context),
    };
}

bool AcceleratedEffectInterpolation<AcceleratedEffectInsetFunction>::canBlend(const AcceleratedEffectInsetFunction&, const AcceleratedEffectInsetFunction&)
{
    return true;
}

AcceleratedEffectInsetFunction AcceleratedEffectInterpolation<AcceleratedEffectInsetFunction>::blend(const AcceleratedEffectInsetFunction& from, const AcceleratedEffectInsetFunction& to, const BlendingContext& context)
{
    return {
        .insets = blendRectEdges(from.insets, to.insets, context),
        .radii = blendCornerRadii(from.radii, to.radii, context),
    };
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
