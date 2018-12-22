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

#include "CSSDefaultStyleSheets.h"
#include "CSSImportRule.h"
#include "CSSMediaRule.h"
#include "CSSRule.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSSupportsRule.h"
#include "ExtensionStyleSheets.h"
#include "StyleScope.h"
#include "StyleSheetContents.h"

namespace WebCore {

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
        switch (cssRule->type()) {
        case CSSRule::IMPORT_RULE:
            collect(downcast<CSSImportRule>(*cssRule).styleSheet());
            break;
        case CSSRule::MEDIA_RULE:
            collect(downcast<CSSMediaRule>(cssRule));
            break;
        case CSSRule::SUPPORTS_RULE:
            collect(downcast<CSSSupportsRule>(cssRule));
            break;
        case CSSRule::STYLE_RULE:
            m_styleRuleToCSSOMWrapperMap.add(&downcast<CSSStyleRule>(*cssRule).styleRule(), downcast<CSSStyleRule>(cssRule));
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
        collectFromStyleSheetContents(CSSDefaultStyleSheets::simpleDefaultStyleSheet);
        collectFromStyleSheetContents(CSSDefaultStyleSheets::defaultStyleSheet);
        collectFromStyleSheetContents(CSSDefaultStyleSheets::quirksStyleSheet);
        collectFromStyleSheetContents(CSSDefaultStyleSheets::svgStyleSheet);
        collectFromStyleSheetContents(CSSDefaultStyleSheets::mathMLStyleSheet);
        collectFromStyleSheetContents(CSSDefaultStyleSheets::mediaControlsStyleSheet);
        collectFromStyleSheetContents(CSSDefaultStyleSheets::fullscreenStyleSheet);
#if ENABLE(DATALIST_ELEMENT)
        collectFromStyleSheetContents(CSSDefaultStyleSheets::dataListStyleSheet);
#endif
#if ENABLE(INPUT_TYPE_COLOR)
        collectFromStyleSheetContents(CSSDefaultStyleSheets::colorInputStyleSheet);
#endif
        collectFromStyleSheetContents(CSSDefaultStyleSheets::plugInsStyleSheet);
        collectFromStyleSheetContents(CSSDefaultStyleSheets::mediaQueryStyleSheet);

        collect(extensionStyleSheets.pageUserSheet());
        collectFromStyleSheets(extensionStyleSheets.injectedUserStyleSheets());
        collectFromStyleSheets(extensionStyleSheets.documentUserStyleSheets());
    }
}

void InspectorCSSOMWrappers::collectScopeWrappers(Style::Scope& styleScope)
{
    maybeCollectFromStyleSheets(styleScope.activeStyleSheets());
}

CSSStyleRule* InspectorCSSOMWrappers::getWrapperForRuleInSheets(StyleRule* rule)
{
    return m_styleRuleToCSSOMWrapperMap.get(rule);
}

} // namespace WebCore
