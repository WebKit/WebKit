/*
 * Copyright (C) 2015, 2022 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSNamedImageValue.h"

#include "StyleNamedImage.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSNamedImageValue::CSSNamedImageValue(CSS::CustomIdent&& name)
    : CSSValue { ClassType::NamedImage }
    , m_name { WTF::move(name) }
{
}

CSSNamedImageValue::~CSSNamedImageValue() = default;

String CSSNamedImageValue::customCSSText(const CSS::SerializationContext& context) const
{
    StringBuilder builder;
    builder.append("-webkit-named-image("_s);
    CSS::serializationForCSS(builder, context, m_name);
    builder.append(')');
    return builder.toString();
}

bool CSSNamedImageValue::equals(const CSSNamedImageValue& other) const
{
    return m_name == other.m_name;
}

RefPtr<Style::Image> CSSNamedImageValue::createStyleImage(const Style::BuilderState& state) const
{
    if (m_cachedStyleImage)
        return m_cachedStyleImage;

    m_cachedStyleImage = Style::NamedImage::create(Style::toStyle(m_name, state));
    return m_cachedStyleImage;
}

} // namespace WebCore
