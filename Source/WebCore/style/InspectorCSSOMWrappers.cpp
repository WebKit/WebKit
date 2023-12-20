/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "InspectorCSSOMWrappers.h"

#include "CSSContainerRule.h"
#include "CSSImportRule.h"
#include "CSSLayerBlockRule.h"
#include "CSSLayerStatementRule.h"
#include "CSSMediaRule.h"
#include "CSSRule.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSSupportsRule.h"
#include "ExtensionStyleSheets.h"
#include "StyleScope.h"
#include "StyleSheetContents.h"
#include "UserAgentStyle.h"

namespace WebCore {
namespace Style {

void InspectorCSSOMWrappers::collectFromStyleSheetIfNeeded(CSSStyleSheet* styleSheet)
{
    if (!m_styleRuleToCSSOMWrapperMap.isEmpty())
        collect(styleSheet);
}

template <class ListType>
void InspectorCSSOMWrappers::collect(ListType* listType)
{
    if (!listType)
        return;
    unsigned size = listType->length();
    for (unsigned i = 0; i < size; ++i) {
        CSSRule* cssRule = listType->item(i);
        if (!cssRule)
            continue;
        
        switch (cssRule->styleRuleType()) {
        case StyleRuleType::Container:
            collect(uncheckedDowncast<CSSContainerRule>(cssRule));
            break;
        case StyleRuleType::Import:
            collect(uncheckedDowncast<CSSImportRule>(*cssRule).styleSheet());
            break;
        case StyleRuleType::LayerBlock:
            collect(uncheckedDowncast<CSSLayerBlockRule>(cssRule));
            break;
        case StyleRuleType::Media:
            collect(uncheckedDowncast<CSSMediaRule>(cssRule));
            break;
        case StyleRuleType::Supports:
            collect(uncheckedDowncast<CSSSupportsRule>(cssRule));
            break;
        case StyleRuleType::Style:
            m_styleRuleToCSSOMWrapperMap.add(&uncheckedDowncast<CSSStyleRule>(*cssRule).styleRule(), uncheckedDowncast<CSSStyleRule>(cssRule));

            // Eagerly collect rules nested in this style rule.
            collect(uncheckedDowncast<CSSStyleRule>(cssRule));
            break;
        default:
            break;
        }
    }
}

void InspectorCSSOMWrappers::collectFromStyleSheetContents(StyleSheetContents* styleSheet)
{
    if (!styleSheet)
        return;
    auto styleSheetWrapper = CSSStyleSheet::create(*styleSheet);
    m_styleSheetCSSOMWrapperSet.add(styleSheetWrapper.copyRef());
    collect(styleSheetWrapper.ptr());
}

void InspectorCSSOMWrappers::collectFromStyleSheets(const Vector<RefPtr<CSSStyleSheet>>& sheets)
{
    for (auto& sheet : sheets)
        collect(sheet.get());
}

void InspectorCSSOMWrappers::maybeCollectFromStyleSheets(const Vector<RefPtr<CSSStyleSheet>>& sheets)
{
    for (auto& sheet : sheets) {
        if (!m_styleSheetCSSOMWrapperSet.contains(sheet.get())) {
            m_styleSheetCSSOMWrapperSet.add(sheet);
            collect(sheet.get());
        }
    }
}

void InspectorCSSOMWrappers::collectDocumentWrappers(ExtensionStyleSheets& extensionStyleSheets)
{
    if (m_styleRuleToCSSOMWrapperMap.isEmpty()) {
        collectFromStyleSheetContents(UserAgentStyle::defaultStyleSheet);
        collectFromStyleSheetContents(UserAgentStyle::quirksStyleSheet);
        collectFromStyleSheetContents(UserAgentStyle::svgStyleSheet);
        collectFromStyleSheetContents(UserAgentStyle::mathMLStyleSheet);
        collectFromStyleSheetContents(UserAgentStyle::mediaControlsStyleSheet);
        collectFromStyleSheetContents(UserAgentStyle::horizontalFormControlsStyleSheet);
        collectFromStyleSheetContents(UserAgentStyle::viewTransitionsStyleSheet);
        collectFromStyleSheetContents(UserAgentStyle::htmlSwitchControlStyleSheet);
#if ENABLE(FULLSCREEN_API)
        collectFromStyleSheetContents(UserAgentStyle::fullscreenStyleSheet);
#endif
#if ENABLE(DATALIST_ELEMENT)
        collectFromStyleSheetContents(UserAgentStyle::dataListStyleSheet);
#endif
#if ENABLE(INPUT_TYPE_COLOR)
        collectFromStyleSheetContents(UserAgentStyle::colorInputStyleSheet);
#endif
        collectFromStyleSheetContents(UserAgentStyle::plugInsStyleSheet);
        collectFromStyleSheetContents(UserAgentStyle::mediaQueryStyleSheet);

        collect(extensionStyleSheets.pageUserSheet());
        collectFromStyleSheets(extensionStyleSheets.injectedUserStyleSheets());
        collectFromStyleSheets(extensionStyleSheets.documentUserStyleSheets());
        collectFromStyleSheets(extensionStyleSheets.injectedAuthorStyleSheets());
        collectFromStyleSheets(extensionStyleSheets.authorStyleSheetsForTesting());
    }
}

void InspectorCSSOMWrappers::collectScopeWrappers(Scope& styleScope)
{
    maybeCollectFromStyleSheets(styleScope.activeStyleSheets());
}

CSSStyleRule* InspectorCSSOMWrappers::getWrapperForRuleInSheets(const StyleRule* rule)
{
    return m_styleRuleToCSSOMWrapperMap.get(rule);
}

} // namespace Style
} // namespace WebCore
