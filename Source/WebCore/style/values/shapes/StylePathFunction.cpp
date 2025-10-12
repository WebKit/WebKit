/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StylePathFunction.h"

#include "AffineTransform.h"
#include "CSSPathValue.h"
#include "FloatRect.h"
#include "GeometryUtilities.h"
#include "Path.h"
#include "RenderStyle.h"
#include "RenderStyleInlines.h"
#include "SVGPathUtilities.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include <wtf/TinyLRUCache.h>

namespace WebCore {
namespace Style {

// MARK: - Path Caching

struct SVGPathTransformedByteStream {
    bool isEmpty() const
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

    bool operator==(const SVGPathTransformedByteStream&) const = default;

    SVGPathByteStream rawStream;
    float zoom;
    FloatPoint offset;
};

struct TransformedByteStreamPathPolicy : TinyLRUCachePolicy<SVGPathTransformedByteStream, WebCore::Path> {
    static bool isKeyNull(const SVGPathTransformedByteStream& stream)
    {
        return stream.isEmpty();
    }

    static WebCore::Path createValueForKey(const SVGPathTransformedByteStream& stream)
    {
        return stream.path();
    }
};

static const WebCore::Path& cachedTransformedByteStreamPath(const SVGPathByteStream& stream, float zoom, const FloatPoint& offset)
{
    static NeverDestroyed<TinyLRUCache<SVGPathTransformedByteStream, WebCore::Path, 4, TransformedByteStreamPathPolicy>> cache;
    return cache.get().get(SVGPathTransformedByteStream { stream, zoom, offset });
}

// MARK: - Conversion

static SVGPathByteStream copySVGPathByteStream(const SVGPathByteStream& source, PathConversion conversion)
{
    if (conversion == PathConversion::ForceAbsolute) {
        // Only returns the resulting absolute path if the conversion succeeds.
        if (auto result = convertSVGPathByteStreamToAbsoluteCoordinates(source))
            return *result;
    }
    return source;
}

auto ToCSS<Path::Data>::operator()(const Path::Data& value, const RenderStyle&, PathConversion conversion) -> CSS::Path::Data
{
    return { copySVGPathByteStream(value.byteStream, conversion) };
}

auto ToStyle<CSS::Path::Data>::operator()(const CSS::Path::Data& value, const BuilderState&) -> Path::Data
{
    return { copySVGPathByteStream(value.byteStream, PathConversion::None) };
}

auto ToCSS<Path>::operator()(const Path& value, const RenderStyle& style, PathConversion conversion) -> CSS::Path
{
    return {
        .fillRule = toCSS(value.fillRule, style),
        .data = toCSS(value.data, style, conversion)
    };
}

auto ToStyle<CSS::Path>::operator()(const CSS::Path& value, const BuilderState& state, std::optional<float> zoom) -> Path
{
    return {
        .fillRule = toStyle(value.fillRule, state),
        .data = toStyle(value.data, state),
        .zoom = zoom.value_or(state.style().usedZoom())
    };
}

Ref<CSSValue> CSSValueCreation<PathFunction>::operator()(CSSValuePool&, const RenderStyle& style, const PathFunction& value, PathConversion conversion)
{
    return CSSPathValue::create(toCSS(value, style, conversion));
}

// MARK: - Serialization

void Serialize<Path>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const Path& value, PathConversion conversion)
{
    CSS::serializationForCSS(builder, context, toCSS(value, style, conversion));
}

// MARK: - Path

WebCore::Path PathComputation<Path>::operator()(const Path& value, const FloatRect& boundingBox)
{
    return cachedTransformedByteStreamPath(value.data.byteStream, value.zoom, boundingBox.location());
}

// MARK: - Wind Rule

WebCore::WindRule WindRuleComputation<Path>::operator()(const Path& value)
{
    return (!value.fillRule || std::holds_alternative<CSS::Keyword::Nonzero>(*value.fillRule)) ? WindRule::NonZero : WindRule::EvenOdd;
}

// MARK: - Blending

auto Blending<Path>::canBlend(const Path& a, const Path& b) -> bool
{
    return windRule(a) == windRule(b)
        && canBlendSVGPathByteStreams(a.data.byteStream, b.data.byteStream);
}

auto Blending<Path>::blend(const Path& a, const Path& b, const BlendingContext& context) -> Path
{
    SVGPathByteStream resultingPathBytes;
    buildAnimatedSVGPathByteStream(a.data.byteStream, b.data.byteStream, resultingPathBytes, context.progress);

    return {
        .fillRule = a.fillRule,
        .data = { WTFMove(resultingPathBytes) },
        .zoom = a.zoom
    };
}

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream& ts, const Path::Data& value)
{
    String pathString;
    buildStringFromByteStream(value.byteStream, pathString, UnalteredParsing);
    return ts << pathString;
}

} // namespace Style
} // namespace WebCore
