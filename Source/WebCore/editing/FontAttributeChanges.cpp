/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "FontAttributeChanges.h"

#include "CSSPropertyNames.h"
#include "CSSTextShadowPropertyValue.h"
#include "CSSValueKeywords.h"
#include "CSSValuePool.h"
#include "EditAction.h"
#include "EditingStyle.h"
#include "MutableStyleProperties.h"
#include "StyleProperties.h"

namespace WebCore {

FontChanges::FontChanges(String&& fontName, String&& fontFamily, std::optional<double>&& fontSize, std::optional<double>&& fontSizeDelta, std::optional<bool>&& bold, std::optional<bool>&& italic)
    : m_fontName(WTF::move(fontName))
    , m_fontFamily(WTF::move(fontFamily))
    , m_fontSize(WTF::move(fontSize))
    , m_fontSizeDelta(WTF::move(fontSizeDelta))
    , m_bold(WTF::move(bold))
    , m_italic(WTF::move(italic))
{
    ASSERT(!m_fontSize || !m_fontSizeDelta);
}

#if !PLATFORM(COCOA)

const String& FontChanges::platformFontFamilyNameForCSS() const
{
    return m_fontFamily;
}

#endif

Ref<EditingStyle> FontChanges::createEditingStyle() const
{
    auto properties = createStyleProperties();
    return EditingStyle::create(properties.ptr());
}

Ref<MutableStyleProperties> FontChanges::createStyleProperties() const
{
    auto style = MutableStyleProperties::create();

    if (!m_fontFamily.isNull()) {
        AtomString familyNameForCSS { platformFontFamilyNameForCSS() };
        if (!familyNameForCSS.isNull())
            style->setProperty(CSSPropertyFontFamily, CSSValuePool::singleton().createFontFamilyValue(familyNameForCSS));
    }

    if (m_italic)
        style->setProperty(CSSPropertyFontStyle, *m_italic ? CSSValueItalic : CSSValueNormal);

    if (m_bold)
        style->setProperty(CSSPropertyFontWeight, *m_bold ? CSSValueBold : CSSValueNormal);

    if (m_fontSize)
        style->setProperty(CSSPropertyFontSize, CSSPrimitiveValue::create(*m_fontSize, CSSUnitType::CSS_PX));

    if (m_fontSizeDelta)
        style->setProperty(CSSPropertyWebkitFontSizeDelta, CSSPrimitiveValue::create(*m_fontSizeDelta, CSSUnitType::CSS_PX));

    return style;
}

static RefPtr<CSSValue> cssValueForTextShadow(const FontShadow& shadow)
{
    if (shadow.offset.isZero() && !shadow.blurRadius)
        return nullptr;

    auto color = CSS::Color { CSS::ResolvedColor { shadow.color } };
    auto width = CSS::Length<CSS::AllUnzoomed> { CSS::LengthUnit::Px, shadow.offset.width() };
    auto height = CSS::Length<CSS::AllUnzoomed> { CSS::LengthUnit::Px, shadow.offset.height() };
    auto blur = CSS::Length<CSS::NonnegativeUnzoomed> { CSS::LengthUnit::Px, shadow.blurRadius };

    CSS::TextShadowProperty::List list {
        CSS::TextShadow {
            .color = WTF::move(color),
            .location = { WTF::move(width), WTF::move(height) },
            .blur = WTF::move(blur),
        }
    };

    return CSSTextShadowPropertyValue::create(CSS::TextShadowProperty { WTF::move(list) });
}

FontAttributeChanges::FontAttributeChanges(std::optional<VerticalAlignChange>&& verticalAlign, std::optional<Color>&& backgroundColor, std::optional<Color>&& foregroundColor, std::optional<FontShadow>&& shadow, std::optional<bool>&& strikeThrough, std::optional<bool>&& underline, FontChanges&& fontChanges)
    : m_verticalAlign(WTF::move(verticalAlign))
    , m_backgroundColor(WTF::move(backgroundColor))
    , m_foregroundColor(WTF::move(foregroundColor))
    , m_shadow(WTF::move(shadow))
    , m_strikeThrough(WTF::move(strikeThrough))
    , m_underline(WTF::move(underline))
    , m_fontChanges(WTF::move(fontChanges))
{
}

EditAction FontAttributeChanges::editAction() const
{
    if (!m_verticalAlign && !m_backgroundColor && !m_shadow && !m_strikeThrough && !m_underline) {
        if (m_foregroundColor && m_fontChanges.isEmpty())
            return EditAction::SetColor;

        if (!m_foregroundColor && !m_fontChanges.isEmpty())
            return EditAction::SetFont;
    }
    return EditAction::ChangeAttributes;
}

Ref<EditingStyle> FontAttributeChanges::createEditingStyle() const
{
    auto style = m_fontChanges.createStyleProperties();
    auto& cssValuePool = CSSValuePool::singleton();

    if (m_backgroundColor)
        style->setProperty(CSSPropertyBackgroundColor, cssValuePool.createColorValue(*m_backgroundColor));

    if (m_foregroundColor)
        style->setProperty(CSSPropertyColor, cssValuePool.createColorValue(*m_foregroundColor));

    if (m_shadow) {
        if (auto shadowValue = cssValueForTextShadow(*m_shadow))
            style->setProperty(CSSPropertyTextShadow, shadowValue.releaseNonNull());
        else
            style->setProperty(CSSPropertyTextShadow, CSSValueNone);
    }

    if (m_verticalAlign) {
        switch (*m_verticalAlign) {
        case VerticalAlignChange::Superscript:
            style->setProperty(CSSPropertyVerticalAlign, CSSValueSuper);
            break;
        case VerticalAlignChange::Subscript:
            style->setProperty(CSSPropertyVerticalAlign, CSSValueSub);
            break;
        case VerticalAlignChange::Baseline:
            style->setProperty(CSSPropertyVerticalAlign, CSSValueBaseline);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    auto editingStyle = EditingStyle::create(style.ptr());

    if (m_strikeThrough)
        editingStyle->setStrikeThroughChange(*m_strikeThrough ? TextDecorationChange::Add : TextDecorationChange::Remove);

    if (m_underline)
        editingStyle->setUnderlineChange(*m_underline ? TextDecorationChange::Add : TextDecorationChange::Remove);

    return editingStyle;
}

} // namespace WebCore
