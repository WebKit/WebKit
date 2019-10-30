/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "StyleBuilderState.h"

#include "StyleResolver.h"

namespace WebCore {
namespace Style {

BuilderState::BuilderState(PropertyCascade& cascade, StyleResolver& styleResolver)
    : m_cascade(cascade)
    , m_styleMap(*this)
    , m_cssToLengthConversionData(styleResolver.state().cssToLengthConversionData())
    , m_styleResolver(styleResolver)
    , m_style(*styleResolver.style())
    , m_parentStyle(*styleResolver.parentStyle())
    , m_rootElementStyle(styleResolver.rootElementStyle() ? *styleResolver.rootElementStyle() : RenderStyle::defaultStyle())
    , m_document(m_styleResolver.document())
    , m_element(m_styleResolver.element())
{
    ASSERT(styleResolver.style());
    ASSERT(styleResolver.parentStyle());
}

bool BuilderState::useSVGZoomRules() const
{
    return m_styleResolver.useSVGZoomRules();
}

bool BuilderState::useSVGZoomRulesForLength() const
{
    return m_styleResolver.useSVGZoomRulesForLength();
}

RefPtr<StyleImage> BuilderState::createStyleImage(CSSValue& value)
{
    return m_styleResolver.styleImage(value);
}

bool BuilderState::createFilterOperations(const CSSValue& value, FilterOperations& outOperations)
{
    return m_styleResolver.createFilterOperations(value, outOperations);
}

Color BuilderState::colorFromPrimitiveValue(const CSSPrimitiveValue& value, bool forVisitedLink) const
{
    return m_styleResolver.colorFromPrimitiveValue(value, forVisitedLink);
}

void BuilderState::setFontSize(FontCascadeDescription& description, float size)
{
    m_styleResolver.setFontSize(description, size);
}

}
}
