/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002-2021 Apple Inc. All rights reserved.
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

#include "CSSScopeRule.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "StyleRule.h"
#include "StyleSheetContents.h"
#include "WebCoreOpaqueRoot.h"
#include "css/parser/CSSParserEnum.h"

namespace WebCore {

struct SameSizeAsCSSRule : public RefCountedAndCanMakeWeakPtr<SameSizeAsCSSRule> {
    virtual ~SameSizeAsCSSRule();
    unsigned char bitfields;
#ifndef RC_REMOVE_SEC_ACCEL_3
    Lock opaqueRootLockForGC;
#endif
    void* pointerUnion;
};

static_assert(sizeof(CSSRule) == sizeof(SameSizeAsCSSRule), "CSSRule should stay small");

unsigned short CSSRule::typeForCSSOM() const
{
    // "This enumeration is thus frozen in its current state, and no new new values will be
    // added to reflect additional at-rules; all at-rules beyond the ones listed above will return 0."
    // https://drafts.csswg.org/cssom/#the-cssrule-interface
    if (styleRuleType() >= firstUnexposedStyleRuleType)
        return 0;

    return std::to_underlying(styleRuleType());
}

ExceptionOr<void> CSSRule::setCssText(const String&)
{
    return { };
}

#ifndef RC_REMOVE_SEC_ACCEL_3
WebCoreOpaqueRoot CSSRule::opaqueRootForGCThread()
{
    // Runs on the parallel GC marker threads concurrently with each other and with the
    // conducting mutator, so this is deliberately limited to reading the parent links under
    // opaqueRootLockForGC(). Note that parentStyleSheet() is not used for the parent-rule hop:
    // it walks the same links without taking any of the locks.
    Locker locker { opaqueRootLockForGC() };
    if (SUPPRESS_UNCOUNTED_LOCAL auto* parentRule = this->parentRule())
        return parentRule->opaqueRootForGCThread();
    if (SUPPRESS_UNCOUNTED_LOCAL auto* parentStyleSheet = this->parentStyleSheet())
        return parentStyleSheet->opaqueRootForGCThread();
    return WebCoreOpaqueRoot { this };
}
#endif // RC_REMOVE_SEC_ACCEL_3

const CSSParserContext& CSSRule::parserContext() const
{
    RefPtr styleSheet = parentStyleSheet();
    return styleSheet ? styleSheet->contents().parserContext() : strictCSSParserContext();
}

bool CSSRule::hasStyleRuleAncestor() const
{
    RefPtr current = this->parentRule();
    while (current) {
        if (current->styleRuleType() == StyleRuleType::Style)
            return true;

        current = current->parentRule();
    }
    return false;
}

CSSParserEnum::NestedContext CSSRule::nestedContext() const
{
    for (RefPtr parentRule = this->parentRule(); parentRule; parentRule = parentRule->parentRule()) {
        if (is<CSSStyleRule>(*parentRule))
            return CSSParserEnum::NestedContextType::Style;
        if (is<CSSScopeRule>(*parentRule))
            return CSSParserEnum::NestedContextType::Scope;
    }

    return { };
}

RefPtr<StyleRuleWithNesting> CSSRule::prepareChildStyleRuleForNesting(StyleRule&)
{
    return nullptr;
}

} // namespace WebCore
