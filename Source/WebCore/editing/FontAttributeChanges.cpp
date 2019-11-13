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
#include "StyleProperties.h"

namespace WebCore {

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
    String familyNameForCSS;
    if (!!m_fontFamily)
        familyNameForCSS = platformFontFamilyNameForCSS();

    auto style = MutableStyleProperties::create();
    auto& cssValuePool = CSSValuePool::singleton();

    if (!!familyNameForCSS)
        style->setProperty(CSSPropertyFontFamily, cssValuePool.createFontFamilyValue(familyNameForCSS));

    if (m_italic)
        style->setProperty(CSSPropertyFontStyle, *m_italic ? CSSValueItalic : CSSValueNormal);

    if (m_bold)
        style->setProperty(CSSPropertyFontWeight, *m_bold ? CSSValueBold : CSSValueNormal);

    if (m_fontSize)
        style->setProperty(CSSPropertyFontSize, cssValuePool.createValue(*m_fontSize, CSSUnitType::CSS_PX));

    if (m_fontSizeDelta)
        style->setProperty(CSSPropertyWebkitFontSizeDelta, cssValuePool.createValue(*m_fontSizeDelta, CSSUnitType::CSS_PX));

    return style;
}

static RefPtr<CSSValueList> cssValueListForShadow(const FontShadow& shadow)
{
    if (shadow.offset.isZero() && !shadow.blurRadius)
        return nullptr;

    auto list = CSSValueList::createCommaSeparated();
    auto& cssValuePool = CSSValuePool::singleton();
    auto width = cssValuePool.createValue(shadow.offset.width(), CSSUnitType::CSS_PX);
    auto height = cssValuePool.createValue(shadow.offset.height(), CSSUnitType::CSS_PX);
    auto blurRadius = cssValuePool.createValue(shadow.blurRadius, CSSUnitType::CSS_PX);
    auto color = cssValuePool.createValue(shadow.color);
    list->prepend(CSSShadowValue::create(WTFMove(width), WTFMove(height), WTFMove(blurRadius), { }, { }, WTFMove(color)));
    return list.ptr();
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
        style->setProperty(CSSPropertyBackgroundColor, cssValuePool.createValue(*m_backgroundColor));

    if (m_foregroundColor)
        style->setProperty(CSSPropertyColor, cssValuePool.createValue(*m_foregroundColor));

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
