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
#include "AcceleratedEffectPathFunction.h"

#if ENABLE(THREADED_ANIMATIONS)

#include "AcceleratedEffectAnimationUtilities.h"
#include "AffineTransform.h"
#include "GeometryUtilities.h"
#include "Path.h"
#include "SVGPathUtilities.h"
#include <wtf/TinyLRUCache.h>
#include <wtf/ZippedRange.h>

namespace WebCore {

WindRule windRule(const AcceleratedEffectPathFunction& value)
{
    return value.fillRule.value_or(WindRule::NonZero);
}

// MARK: - Path Caching

struct AcceleratedEffectSVGPathTransformedByteStream {
    bool NODELETE isEmpty() const
    {
        return rawStream.isEmpty();
    }

    WebCore::Path path() const
    {
        auto path = buildPathFromByteStream(rawStream);
        if (zoom != 1)
            path.transform(AffineTransform().scale(zoom));
        path.translate(toFloatSize(offset));
        return path;
    }

    bool operator==(const AcceleratedEffectSVGPathTransformedByteStream&) const = default;

    SVGPathByteStream rawStream;
    float zoom;
    FloatPoint offset;
};

struct AcceleratedEffectTransformedByteStreamPathPolicy : TinyLRUCachePolicy<AcceleratedEffectSVGPathTransformedByteStream, WebCore::Path> {
    static bool NODELETE isKeyNull(const AcceleratedEffectSVGPathTransformedByteStream& stream)
    {
        return stream.isEmpty();
    }

    static WebCore::Path createValueForKey(const AcceleratedEffectSVGPathTransformedByteStream& stream)
    {
        return stream.path();
    }
};

static const WebCore::Path& cachedAcceleratedEffectTransformedByteStreamPath(const SVGPathByteStream& stream, float zoom, const FloatPoint& offset)
{
    static NeverDestroyed<TinyLRUCache<AcceleratedEffectSVGPathTransformedByteStream, WebCore::Path, 4, AcceleratedEffectTransformedByteStreamPathPolicy>> cache;
    return cache.get().get(AcceleratedEffectSVGPathTransformedByteStream { stream, zoom, offset });
}

// MARK: - Path Evaluation

Path path(const AcceleratedEffectPathFunction& value, const FloatRect& boundingBox)
{
    return cachedAcceleratedEffectTransformedByteStreamPath(value.data.byteStream, value.zoom, boundingBox.location());
}

// MARK: - Blending

bool AcceleratedEffectInterpolation<AcceleratedEffectPathFunction>::canBlend(const AcceleratedEffectPathFunction& from, const AcceleratedEffectPathFunction& to)
{
    return windRule(from) == windRule(to)
        && canBlendSVGPathByteStreams(from.data.byteStream, to.data.byteStream);
}

AcceleratedEffectPathFunction AcceleratedEffectInterpolation<AcceleratedEffectPathFunction>::blend(const AcceleratedEffectPathFunction& from, const AcceleratedEffectPathFunction& to, const BlendingContext& context)
{
    SVGPathByteStream resultingPathBytes;
    buildAnimatedSVGPathByteStream(from.data.byteStream, to.data.byteStream, resultingPathBytes, context.progress);

    return {
        .fillRule = from.fillRule,
        .data = { WTF::move(resultingPathBytes) },
        .zoom = from.zoom
    };
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
