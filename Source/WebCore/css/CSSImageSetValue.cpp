/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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
#include "MIMETypeRegistry.h"
#include "StyleBuilderState.h"
#include "StyleImageSet.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

Ref<CSSImageSetValue> CSSImageSetValue::create()
{
    return adoptRef(*new CSSImageSetValue);
}

Ref<CSSImageSetValue> CSSImageSetValue::create(Vector<CSSImageSetOption>&& options)
{
    return adoptRef(*new CSSImageSetValue(WTFMove(options)));
}

CSSImageSetValue::CSSImageSetValue()
    : CSSValue(ImageSetClass)
{
}

CSSImageSetValue::CSSImageSetValue(Vector<CSSImageSetOption>&& options)
    : CSSValue(ImageSetClass)
    , m_options(WTFMove(options))
{
}

CSSImageSetValue::~CSSImageSetValue() = default;

bool CSSImageSetValue::equals(const CSSImageSetValue& other) const
{
    if (m_options.size() != other.m_options.size())
        return false;

    for (size_t i = 0; i < m_options.size(); ++i) {
        const auto& our = m_options[i];
        const auto& their = other.m_options[i];

        if (!our.image().equals(their.image()))
            return false;

        if ((our.resolution() && !their.resolution()) || (!our.resolution() && their.resolution()))
            return false;

        if ((our.resolution() && their.resolution()) && !our.resolution()->equals(*their.resolution()))
            return false;

        if ((our.type().has_value() && !their.type().has_value()) || (!our.type().has_value() && their.type().has_value()))
            return false;

        if ((our.type() && their.type()) && (*our.type() != *their.type()))
            return false;
    }

    return true;
}

String CSSImageSetValue::customCSSText() const
{
    StringBuilder result;
    result.append("image-set(");
    for (size_t i = 0; i < m_options.size(); ++i) {
        if (i > 0)
            result.append(", ");
        result.append(m_options[i].cssText());
    }
    result.append(')');
    return result.toString();
}

RefPtr<StyleImage> CSSImageSetValue::createStyleImage(Style::BuilderState& state) const
{

    Vector<ImageWithScale> images;
    for (const auto& option : m_options) {
        ASSERT(is<CSSImageValue>(option.image()) || option.image().isImageGeneratorValue());
        float scaleFactor = option.resolution()->floatValue(CSSUnitType::CSS_DPPX);
        images.append({ state.createStyleImage(option.image()), scaleFactor });
    }

    // Sort the images so that they are stored in order from lowest resolution to highest.
    std::stable_sort(images.begin(), images.end(), [](auto& a, auto& b) -> bool {
        return a.scaleFactor < b.scaleFactor;
    });

    return StyleImageSet::create(WTFMove(images));
}

} // namespace WebCore
