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
#include "CSSPrimitiveValue.h"
#include "MIMETypeRegistry.h"

namespace WebCore {

CSSImageSetOptionValue::Type::Type(String mimeType)
    : m_isSupported(MIMETypeRegistry::isSupportedImageMIMEType(mimeType))
    , m_mimeType(mimeType)
{
}

String CSSImageSetOptionValue::Type::cssText() const
{
    return makeString("type(\""_s, m_mimeType, "\")"_s);
}

CSSImageSetOptionValue::CSSImageSetOptionValue(Ref<CSSValue>&& image)
    : CSSValue(ImageSetOptionClass)
    , m_image(WTFMove(image))
{
}

CSSImageSetOptionValue::CSSImageSetOptionValue(Ref<CSSValue>&& image, Ref<CSSPrimitiveValue>&& resolution)
    : CSSValue(ImageSetOptionClass)
    , m_image(WTFMove(image))
    , m_resolution(resolution.ptr())
{
}

CSSImageSetOptionValue::CSSImageSetOptionValue(Ref<CSSValue>&& image, Ref<CSSPrimitiveValue>&& resolution, String&& type)
    : CSSValue(ImageSetOptionClass)
    , m_image(WTFMove(image))
    , m_resolution(resolution.ptr())
    , m_type(CSSImageSetOptionValue::Type(type))
{
}

Ref<CSSImageSetOptionValue> CSSImageSetOptionValue::create(Ref<CSSValue>&& image)
{
    ASSERT(is<CSSImageValue>(image) || image->isImageGeneratorValue());
    return adoptRef(*new CSSImageSetOptionValue(WTFMove(image)));
}

Ref<CSSImageSetOptionValue> CSSImageSetOptionValue::create(Ref<CSSValue>&& image, Ref<CSSPrimitiveValue>&& resolution)
{
    ASSERT(is<CSSImageValue>(image) || image->isImageGeneratorValue());
    return adoptRef(*new CSSImageSetOptionValue(WTFMove(image), WTFMove(resolution)));
}

Ref<CSSImageSetOptionValue> CSSImageSetOptionValue::create(Ref<CSSValue>&& image, Ref<CSSPrimitiveValue>&& resolution, String type)
{
    ASSERT(is<CSSImageValue>(image) || image->isImageGeneratorValue());
    if (type.isNull())
        return CSSImageSetOptionValue::create(WTFMove(image), WTFMove(resolution));
    return adoptRef(*new CSSImageSetOptionValue(WTFMove(image), WTFMove(resolution), WTFMove(type)));
}

bool CSSImageSetOptionValue::equals(const CSSImageSetOptionValue& other) const
{
    if (!m_image->equals(other.m_image))
        return false;

    if ((m_resolution && !other.m_resolution) || (!m_resolution && other.m_resolution))
        return false;

    if ((m_resolution && other.m_resolution) && !m_resolution->equals(*other.m_resolution))
        return false;

    if ((m_type && !other.m_type) || (!m_type && other.m_type))
        return false;

    if (m_type && other.m_type) {
        if ((m_type->isSupported() != other.m_type->isSupported()) || (m_type->mimeType() != other.m_type->mimeType()))
            return false;
    }

    return true;
}

String CSSImageSetOptionValue::customCSSText() const
{
    StringBuilder result;
    result.append(m_image->cssText());
    if (m_resolution)
        result.append(' ', m_resolution->cssText());
    else
        result.append(" 1x"_s);
    if (m_type)
        result.append(' ', m_type->cssText());

    return result.toString();
}

Ref<CSSValue> CSSImageSetOptionValue::image() const
{
    return m_image;
}

RefPtr<CSSPrimitiveValue> CSSImageSetOptionValue::resolution() const
{
    return m_resolution;
}

void CSSImageSetOptionValue::setResolution(Ref<CSSPrimitiveValue>&& resolution)
{
    m_resolution = resolution.ptr();
}

std::optional<CSSImageSetOptionValue::Type> CSSImageSetOptionValue::type() const
{
    return m_type;
}

void CSSImageSetOptionValue::setType(Type&& type)
{
    m_type = WTFMove(type);
}

void CSSImageSetOptionValue::setType(String&& type)
{
    m_type = { WTFMove(type) };
}

} // namespace WebCore
