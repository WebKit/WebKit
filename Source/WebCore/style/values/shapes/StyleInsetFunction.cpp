/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
#include "StyleInsetFunction.h"

#include "FloatRect.h"
#include "GeometryUtilities.h"
#include "Path.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include <wtf/TinyLRUCache.h>

namespace WebCore {
namespace Style {

// MARK: - Path Caching

struct RoundedInsetPathPolicy : public TinyLRUCachePolicy<FloatRoundedRect, WebCore::Path> {
public:
    static bool isKeyNull(const FloatRoundedRect& rect)
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
    static NeverDestroyed<TinyLRUCache<FloatRoundedRect, WebCore::Path, 4, RoundedInsetPathPolicy>> cache;
    return cache.get().get(rect);
}

// MARK: - Path

WebCore::Path PathComputation<Inset>::operator()(const Inset& value, const FloatRect& boundingBox)
{
    auto boundingSize = boundingBox.size();

    auto left = evaluate(value.insets.left(), boundingSize.width());
    auto top = evaluate(value.insets.top(), boundingSize.height());
    auto rect = FloatRect {
        left + boundingBox.x(),
        top + boundingBox.y(),
        std::max<float>(boundingSize.width() - left - evaluate(value.insets.right(), boundingSize.width()), 0),
        std::max<float>(boundingSize.height() - top - evaluate(value.insets.bottom(), boundingSize.height()), 0)
    };

    auto radii = evaluate(value.radii, boundingSize);
    radii.scale(calcBorderRadiiConstraintScaleFor(rect, radii));

    return cachedRoundedInsetPath(FloatRoundedRect { rect, radii });
}

} // namespace Style
} // namespace WebCore
