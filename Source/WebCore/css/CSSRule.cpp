/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#include "CSSCharsetRule.h"
#include "CSSFontFaceRule.h"
#include "CSSImportRule.h"
#include "CSSMediaRule.h"
#include "CSSPageRule.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSUnknownRule.h"
#include "WebKitCSSKeyframeRule.h"
#include "WebKitCSSKeyframesRule.h"
#include "WebKitCSSRegionRule.h"
#include "NotImplemented.h"
#include "StyleRule.h"

namespace WebCore {

struct SameSizeAsCSSRule : public RefCounted<SameSizeAsCSSRule> {
    unsigned bitfields;
    void* pointerUnion;
};

COMPILE_ASSERT(sizeof(CSSRule) == sizeof(SameSizeAsCSSRule), CSSRule_should_stay_small);

void CSSRule::setCssText(const String& /*cssText*/, ExceptionCode& /*ec*/)
{
    notImplemented();
}

String CSSRule::cssText() const
{
    switch (type()) {
    case UNKNOWN_RULE:
        return String();
    case STYLE_RULE:
        return static_cast<const CSSStyleRule*>(this)->cssText();
    case PAGE_RULE:
        return static_cast<const CSSPageRule*>(this)->cssText();
    case CHARSET_RULE:
        return static_cast<const CSSCharsetRule*>(this)->cssText();
    case IMPORT_RULE:
        return static_cast<const CSSImportRule*>(this)->cssText();
    case MEDIA_RULE:
        return static_cast<const CSSMediaRule*>(this)->cssText();
    case FONT_FACE_RULE:
        return static_cast<const CSSFontFaceRule*>(this)->cssText();
    case WEBKIT_KEYFRAMES_RULE:
        return static_cast<const WebKitCSSKeyframesRule*>(this)->cssText();
    case WEBKIT_KEYFRAME_RULE:
        return static_cast<const WebKitCSSKeyframeRule*>(this)->cssText();
#if ENABLE(CSS_REGIONS)
    case WEBKIT_REGION_RULE:
        return static_cast<const WebKitCSSRegionRule*>(this)->cssText();
#endif
    }
    ASSERT_NOT_REACHED();
    return String();
}

void CSSRule::destroy()
{
    switch (type()) {
    case UNKNOWN_RULE:
        delete static_cast<CSSUnknownRule*>(this);
        return;
    case STYLE_RULE:
        delete static_cast<CSSStyleRule*>(this);
        return;
    case PAGE_RULE:
        delete static_cast<CSSPageRule*>(this);
        return;
    case CHARSET_RULE:
        delete static_cast<CSSCharsetRule*>(this);
        return;
    case IMPORT_RULE:
        delete static_cast<CSSImportRule*>(this);
        return;
    case MEDIA_RULE:
        delete static_cast<CSSMediaRule*>(this);
        return;
    case FONT_FACE_RULE:
        delete static_cast<CSSFontFaceRule*>(this);
        return;
    case WEBKIT_KEYFRAMES_RULE:
        delete static_cast<WebKitCSSKeyframesRule*>(this);
        return;
    case WEBKIT_KEYFRAME_RULE:
        delete static_cast<WebKitCSSKeyframeRule*>(this);
        return;
#if ENABLE(CSS_REGIONS)
    case WEBKIT_REGION_RULE:
        delete static_cast<WebKitCSSRegionRule*>(this);
        return;
#endif
    }
    ASSERT_NOT_REACHED();
}

void CSSRule::reattach(StyleRuleBase* rule)
{
    switch (type()) {
    case UNKNOWN_RULE:
        return;
    case STYLE_RULE:
        static_cast<CSSStyleRule*>(this)->reattach(static_cast<StyleRule*>(rule));
        return;
    case PAGE_RULE:
        static_cast<CSSPageRule*>(this)->reattach(static_cast<StyleRulePage*>(rule));
        return;
    case CHARSET_RULE:
        ASSERT(!rule);
        return;
    case IMPORT_RULE:
        // FIXME: Implement when enabling caching for stylesheets with import rules.
        ASSERT_NOT_REACHED();
        return;
    case MEDIA_RULE:
        static_cast<CSSMediaRule*>(this)->reattach(static_cast<StyleRuleMedia*>(rule));
        return;
    case FONT_FACE_RULE:
        static_cast<CSSFontFaceRule*>(this)->reattach(static_cast<StyleRuleFontFace*>(rule));
        return;
    case WEBKIT_KEYFRAMES_RULE:
        static_cast<WebKitCSSKeyframesRule*>(this)->reattach(static_cast<StyleRuleKeyframes*>(rule));
        return;
    case WEBKIT_KEYFRAME_RULE:
        // No need to reattach, the underlying data is shareable on mutation.
        ASSERT_NOT_REACHED();
        return;
#if ENABLE(CSS_REGIONS)
    case WEBKIT_REGION_RULE:
        static_cast<WebKitCSSRegionRule*>(this)->reattach(static_cast<StyleRuleRegion*>(rule));
        return;
#endif
    }
    ASSERT_NOT_REACHED();
}

const CSSParserContext& CSSRule::parserContext() const
{
    CSSStyleSheet* styleSheet = parentStyleSheet();
    return styleSheet ? styleSheet->internal()->parserContext() : strictCSSParserContext();
}

} // namespace WebCore
