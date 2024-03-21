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

namespace WebCore {

CSSImageSetOptionValue::CSSImageSetOptionValue(Ref<CSSValue>&& image, Ref<CSSPrimitiveValue>&& resolution)
    : CSSValue(ImageSetOptionClass)
    , m_image(WTFMove(image))
    , m_resolution(WTFMove(resolution))
{
}

CSSImageSetOptionValue::CSSImageSetOptionValue(Ref<CSSValue>&& image, Ref<CSSPrimitiveValue>&& resolution, String&& type)
    : CSSValue(ImageSetOptionClass)
    , m_image(WTFMove(image))
    , m_resolution(WTFMove(resolution))
    , m_mimeType(WTFMove(type))
{
}

Ref<CSSImageSetOptionValue> CSSImageSetOptionValue::create(Ref<CSSValue>&& image)
{
    ASSERT(is<CSSImageValue>(image) || image->isImageGeneratorValue());
    return adoptRef(*new CSSImageSetOptionValue(WTFMove(image), CSSPrimitiveValue::create(1.0, CSSUnitType::CSS_X)));
}

Ref<CSSImageSetOptionValue> CSSImageSetOptionValue::create(Ref<CSSValue>&& image, Ref<CSSPrimitiveValue>&& resolution)
{
    ASSERT(is<CSSImageValue>(image) || image->isImageGeneratorValue());
    return adoptRef(*new CSSImageSetOptionValue(WTFMove(image), WTFMove(resolution)));
}

Ref<CSSImageSetOptionValue> CSSImageSetOptionValue::create(Ref<CSSValue>&& image, Ref<CSSPrimitiveValue>&& resolution, String type)
{
    ASSERT(is<CSSImageValue>(image) || image->isImageGeneratorValue());
    return adoptRef(*new CSSImageSetOptionValue(WTFMove(image), WTFMove(resolution), WTFMove(type)));
}

bool CSSImageSetOptionValue::equals(const CSSImageSetOptionValue& other) const
{
    if (!m_image->equals(other.m_image))
        return false;

    if (!m_resolution->equals(other.m_resolution))
        return false;

    if (m_mimeType != other.m_mimeType)
        return false;

    return true;
}

String CSSImageSetOptionValue::customCSSText() const
{
    StringBuilder result;
    result.append(m_image->cssText());
    result.append(' ', m_resolution->cssText());
    if (!m_mimeType.isNull())
        result.append(" type(\""_s, m_mimeType, "\")"_s);

    return result.toString();
}

void CSSImageSetOptionValue::setResolution(Ref<CSSPrimitiveValue>&& resolution)
{
    m_resolution = WTFMove(resolution);
}

void CSSImageSetOptionValue::setType(String type)
{
    m_mimeType = WTFMove(type);
}

bool CSSImageSetOptionValue::customTraverseSubresources(const Function<bool(const CachedResource&)>& handler) const
{
    return m_resolution->traverseSubresources(handler) || m_image->traverseSubresources(handler);
}

void CSSImageSetOptionValue::customSetReplacementURLForSubresources(const HashMap<String, String>& replacementURLStrings)
{
    m_image->setReplacementURLForSubresources(replacementURLStrings);
    m_resolution->setReplacementURLForSubresources(replacementURLStrings);
}

void CSSImageSetOptionValue::customClearReplacementURLForSubresources()
{
    m_image->clearReplacementURLForSubresources();
    m_resolution->clearReplacementURLForSubresources();
}

} // namespace WebCore
