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
#include "CSSImageSetOption.h"

#include "CSSImageValue.h"
#include "CSSPrimitiveValue.h"

namespace WebCore {

CSSImageSetOption::CSSImageSetOption(Ref<CSSValue>&& image)
: m_image {
    [image = WTFMove(image)] {
        ASSERT(is<CSSImageValue>(image) || image->isImageGeneratorValue());
        return image;
    }()
}
{
}

const CSSValue& CSSImageSetOption::image() const
{
    return m_image.get();
}

CSSPrimitiveValue* CSSImageSetOption::resolution() const
{
    return m_resolution.get();
}

void CSSImageSetOption::setResolution(Ref<CSSPrimitiveValue>&& resolution)
{
    m_resolution = resolution.ptr();
}

std::optional<CSSImageSetType> CSSImageSetOption::type() const
{
    return m_type;
}

void CSSImageSetOption::setType(String&& type)
{
    m_type = CSSImageSetType(WTFMove(type));
}

String CSSImageSetOption::cssText() const
{
    StringBuilder builder;
    builder.append(m_image->cssText());
    if (m_resolution) {
        builder.append(' ');
        builder.append(m_resolution->cssText());
    }
    if (m_type.has_value()) {
        builder.append(' ');
        builder.append(m_type->cssText());
    }

    return builder.toString();
}

} // namespace WebCore
