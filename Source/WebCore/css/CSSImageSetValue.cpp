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

#include "CSSImageValue.h"
#include "CSSPrimitiveValue.h"
#include "StyleBuilderState.h"
#include "StyleImageSet.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

Ref<CSSImageSetValue> CSSImageSetValue::create()
{
    return adoptRef(*new CSSImageSetValue);
}

CSSImageSetValue::CSSImageSetValue()
    : CSSValueList(ImageSetClass, CommaSeparator)
{
}

CSSImageSetValue::~CSSImageSetValue() = default;

String CSSImageSetValue::customCSSText() const
{
    StringBuilder result;
    result.append("image-set(");
    size_t length = this->length();
    for (size_t i = 0; i + 1 < length; i += 2) {
        if (i > 0)
            result.append(", ");
        result.append(item(i)->cssText(), ' ', item(i + 1)->cssText());
    }
    result.append(')');
    return result.toString();
}

RefPtr<StyleImage> CSSImageSetValue::createStyleImage(Style::BuilderState& state) const
{
    size_t length = this->length();

    Vector<ImageWithScale> images;
    images.reserveInitialCapacity(length / 2);

    for (size_t i = 0; i + 1 < length; i += 2) {
        auto* imageValue = item(i);
        auto* scaleFactorValue = item(i + 1);

        ASSERT(is<CSSImageValue>(imageValue) || imageValue->isImageGeneratorValue());
        ASSERT(is<CSSPrimitiveValue>(scaleFactorValue));

        float scaleFactor = downcast<CSSPrimitiveValue>(scaleFactorValue)->floatValue(CSSUnitType::CSS_DPPX);
        images.uncheckedAppend({ state.createStyleImage(*imageValue), scaleFactor });
    }

    // Sort the images so that they are stored in order from lowest resolution to highest.
    std::stable_sort(images.begin(), images.end(), [](auto& a, auto& b) -> bool {
        return a.scaleFactor < b.scaleFactor;
    });

    return StyleImageSet::create(WTFMove(images));
}

} // namespace WebCore
