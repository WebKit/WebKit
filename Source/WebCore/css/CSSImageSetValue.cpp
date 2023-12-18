/*
 * Copyright (C) 2012-2021 Apple Inc. All rights reserved.
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
#include "CSSImageSetValue.h"

#include "CSSImageSetOptionValue.h"
#include "CSSImageValue.h"
#include "CSSPrimitiveValue.h"
#include "StyleBuilderState.h"
#include "StyleImageSet.h"
#include <numeric>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

Ref<CSSImageSetValue> CSSImageSetValue::create(CSSValueListBuilder builder)
{
    return adoptRef(*new CSSImageSetValue(WTFMove(builder)));
}

CSSImageSetValue::CSSImageSetValue(CSSValueListBuilder builder)
    : CSSValueContainingVector(ImageSetClass, CommaSeparator, WTFMove(builder))
{
}

String CSSImageSetValue::customCSSText() const
{
    StringBuilder result;
    result.append("image-set("_s);
    for (size_t i = 0; i < this->length(); ++i) {
        if (i > 0)
            result.append(", "_s);
        ASSERT(is<CSSImageSetOptionValue>(item(i)));
        result.append(item(i)->cssText());
    }
    result.append(')');
    return result.toString();
}

RefPtr<StyleImage> CSSImageSetValue::createStyleImage(Style::BuilderState& state) const
{
    size_t length = this->length();

    Vector<ImageWithScale> images(length, [&](size_t i) {
        auto option = downcast<CSSImageSetOptionValue>(item(i));
        return ImageWithScale { state.createStyleImage(option->image()), option->resolution()->floatValue(CSSUnitType::CSS_DPPX), option->type() };
    });

    // Sort the images so that they are stored in order from lowest resolution to highest.
    // We want to maintain the authored order for serialization so we create a sorted indexing vector.
    Vector<size_t> sortedIndices(length);
    std::iota(sortedIndices.begin(), sortedIndices.end(), 0);

    std::stable_sort(sortedIndices.begin(), sortedIndices.end(), [&images](size_t lhs, size_t rhs) {
        return images[lhs].scaleFactor < images[rhs].scaleFactor;
    });

    return StyleImageSet::create(WTFMove(images), WTFMove(sortedIndices));
}

} // namespace WebCore
