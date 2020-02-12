/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2007, 2012 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "CSSRule.h"

#include "CSSStyleSheet.h"
#include "StyleRule.h"
#include "StyleSheetContents.h"

namespace WebCore {

struct SameSizeAsCSSRule : public RefCounted<SameSizeAsCSSRule> {
    virtual ~SameSizeAsCSSRule();
    unsigned char bitfields;
    void* pointerUnion;
};

COMPILE_ASSERT(sizeof(CSSRule) == sizeof(SameSizeAsCSSRule), CSSRule_should_stay_small);

COMPILE_ASSERT(StyleRuleType::Unknown == static_cast<StyleRuleType>(CSSRule::Type::UNKNOWN_RULE), enums_should_match);
COMPILE_ASSERT(StyleRuleType::Style == static_cast<StyleRuleType>(CSSRule::Type::STYLE_RULE), enums_should_match);
COMPILE_ASSERT(StyleRuleType::Charset == static_cast<StyleRuleType>(CSSRule::Type::CHARSET_RULE), enums_should_match);
COMPILE_ASSERT(StyleRuleType::Import == static_cast<StyleRuleType>(CSSRule::Type::IMPORT_RULE), enums_should_match);
COMPILE_ASSERT(StyleRuleType::Media == static_cast<StyleRuleType>(CSSRule::Type::MEDIA_RULE), enums_should_match);
COMPILE_ASSERT(StyleRuleType::FontFace == static_cast<StyleRuleType>(CSSRule::Type::FONT_FACE_RULE), enums_should_match);
COMPILE_ASSERT(StyleRuleType::Page == static_cast<StyleRuleType>(CSSRule::Type::PAGE_RULE), enums_should_match);
COMPILE_ASSERT(StyleRuleType::Keyframes == static_cast<StyleRuleType>(CSSRule::Type::KEYFRAMES_RULE), enums_should_match);
COMPILE_ASSERT(StyleRuleType::Keyframe == static_cast<StyleRuleType>(CSSRule::Type::KEYFRAME_RULE), enums_should_match);
COMPILE_ASSERT(StyleRuleType::Namespace == static_cast<StyleRuleType>(CSSRule::Type::NAMESPACE_RULE), enums_should_match);
COMPILE_ASSERT(StyleRuleType::Supports == static_cast<StyleRuleType>(CSSRule::Type::SUPPORTS_RULE), enums_should_match);

ExceptionOr<void> CSSRule::setCssText(const String&)
{
    return { };
}

const CSSParserContext& CSSRule::parserContext() const
{
    CSSStyleSheet* styleSheet = parentStyleSheet();
    return styleSheet ? styleSheet->contents().parserContext() : strictCSSParserContext();
}

} // namespace WebCore
