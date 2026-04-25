/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "CSSImageSetOptionValue.h"

#include "CSSImageValue.h"
#include "CSSPrimitiveNumericTypes+CSSValueVisitation.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"

namespace WebCore {

CSSImageSetOptionValue::CSSImageSetOptionValue(Ref<CSSValue>&& image, std::optional<CSS::Resolution<>>&& resolution, std::optional<FunctionNotation<CSSValueType, CSS::String>>&& mimeType)
    : CSSValue(ClassType::ImageSetOption)
    , m_image(WTF::move(image))
    , m_resolution(resolution.value_or(CSS::Literals::x(1)))
    , m_mimeType(WTF::move(mimeType))
{
}

Ref<CSSImageSetOptionValue> CSSImageSetOptionValue::create(Ref<CSSValue>&& image, std::optional<CSS::Resolution<>>&& resolution, std::optional<FunctionNotation<CSSValueType, CSS::String>>&& mimeType)
{
    ASSERT(is<CSSImageValue>(image) || image->isImageGeneratorValue());
    return adoptRef(*new CSSImageSetOptionValue( WTF::move(image), WTF::move(resolution), WTF::move(mimeType)));
}

bool CSSImageSetOptionValue::equals(const CSSImageSetOptionValue& other) const
{
    if (!m_image->equals(other.m_image))
        return false;
    if (m_resolution != other.m_resolution)
        return false;
    if (m_mimeType != other.m_mimeType)
        return false;
    return true;
}

String CSSImageSetOptionValue::customCSSText(const CSS::SerializationContext& context) const
{
    using namespace CSS::Literals;

    StringBuilder builder;
    builder.append(m_image->cssText(context));

    // FIXME: The resolution should probably not be serialized when m_resolution is the default value to comply with the shortest serialization principle.
    builder.append(' ');
    CSS::serializationForCSS(builder, context, m_resolution);

    if (m_mimeType) {
        builder.append(' ');
        CSS::serializationForCSS(builder, context, *m_mimeType);
    }

    return builder.toString();
}

IterationStatus CSSImageSetOptionValue::customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& func) const
{
    if (func(m_image.get()) == IterationStatus::Done)
        return IterationStatus::Done;
    if (CSS::visitCSSValueChildren(func, m_resolution) == IterationStatus::Done)
        return IterationStatus::Done;
    if (CSS::visitCSSValueChildren(func, m_mimeType) == IterationStatus::Done)
        return IterationStatus::Done;
    return IterationStatus::Continue;
}

bool CSSImageSetOptionValue::customTraverseSubresources(NOESCAPE const Function<bool(const CachedResource&)>& handler) const
{
    return m_image->traverseSubresources(handler);
}

} // namespace WebCore
