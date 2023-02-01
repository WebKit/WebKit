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
#include "CSSShadowValue.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "EditAction.h"
#include "EditingStyle.h"
#include "MutableStyleProperties.h"
#include "StyleProperties.h"

namespace WebCore {

FontChanges::FontChanges(String&& fontName, String&& fontFamily, std::optional<double>&& fontSize, std::optional<double>&& fontSizeDelta, std::optional<bool>&& bold, std::optional<bool>&& italic)
    : m_fontName(WTFMove(fontName))
    , m_fontFamily(WTFMove(fontFamily))
    , m_fontSize(WTFMove(fontSize))
    , m_fontSizeDelta(WTFMove(fontSizeDelta))
    , m_bold(WTFMove(bold))
    , m_italic(WTFMove(italic))
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

static RefPtr<CSSValueList> cssValueListForShadow(const FontShadow& shadow)
{
    if (shadow.offset.isZero() && !shadow.blurRadius)
        return nullptr;

    auto list = CSSValueList::createCommaSeparated();
    auto width = CSSPrimitiveValue::create(shadow.offset.width(), CSSUnitType::CSS_PX);
    auto height = CSSPrimitiveValue::create(shadow.offset.height(), CSSUnitType::CSS_PX);
    auto blurRadius = CSSPrimitiveValue::create(shadow.blurRadius, CSSUnitType::CSS_PX);
    auto color = CSSValuePool::singleton().createColorValue(shadow.color);
    list->prepend(CSSShadowValue::create(WTFMove(width), WTFMove(height), WTFMove(blurRadius), { }, { }, WTFMove(color)));
    return list.ptr();
}

FontAttributeChanges::FontAttributeChanges(std::optional<VerticalAlignChange>&& verticalAlign, std::optional<Color>&& backgroundColor, std::optional<Color>&& foregroundColor, std::optional<FontShadow>&& shadow, std::optional<bool>&& strikeThrough, std::optional<bool>&& underline, FontChanges&& fontChanges)
    : m_verticalAlign(WTFMove(verticalAlign))
    , m_backgroundColor(WTFMove(backgroundColor))
    , m_foregroundColor(WTFMove(foregroundColor))
    , m_shadow(WTFMove(shadow))
    , m_strikeThrough(WTFMove(strikeThrough))
    , m_underline(WTFMove(underline))
    , m_fontChanges(WTFMove(fontChanges))
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
        if (auto shadowValue = cssValueListForShadow(*m_shadow))
            style->setProperty(CSSPropertyTextShadow, WTFMove(shadowValue));
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
