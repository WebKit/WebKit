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

#include "CSSMarkup.h"
#include "StyleBuilderState.h"
#include "StyleImageSet.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

Ref<CSSImageSetValue> CSSImageSetValue::create(Span<Option> options)
{
    return adoptRef(*new CSSImageSetValue(options));
}

CSSImageSetValue::CSSImageSetValue(Span<Option> options)
    : CSSValue(Type::ImageSet)
    , m_options(options.begin(), options.end())
{
}

String CSSImageSetValue::customCSSText() const
{
    StringBuilder result;
    result.append("image-set("_s);
    for (size_t i = 0; i < m_options.size(); ++i) {
        auto& option = m_options[i];
        if (i > 0)
            result.append(", "_s);
        result.append(option.image->cssText());
        if (option.resolution)
            result.append(' ', option.resolution->cssText());
        else
            result.append(" 1x"_s);
        if (!option.type.isNull()) {
            result.append(" type("_s);
            serializeString(option.type, result);
            result.append(')');
        }
    }
    result.append(')');
    return result.toString();
}

bool CSSImageSetValue::equals(const CSSImageSetValue& other) const
{
    auto size = m_options.size();
    if (size != other.m_options.size())
        return false;
    for (size_t i = 0; i < size; ++i) {
        auto& a = m_options[i];
        auto& b = other.m_options[i];
        if (!(compareCSSValue(a.image, b.image) && compareCSSValuePtr(a.resolution, b.resolution) && a.type == b.type))
            return false;
    }
    return true;
}

RefPtr<StyleImage> CSSImageSetValue::createStyleImage(Style::BuilderState& state) const
{
    Vector<ImageWithScale> images;
    images.reserveInitialCapacity(m_options.size());
    for (auto& option : m_options) {
        float scaleFactor = 1;
        if (option.resolution)
            scaleFactor = option.resolution->floatValue(CSSUnitType::CSS_DPPX);
        images.uncheckedAppend({ state.createStyleImage(option.image), scaleFactor, option.type });
    }
    return StyleImageSet::create(WTFMove(images));
}

bool CSSImageSetValue::customTraverseSubresources(const Function<bool(const CachedResource&)>& handler) const
{
    for (auto& option : m_options) {
        if (option.image->traverseSubresources(handler))
            return true;
    }
    return false;
}

} // namespace WebCore
