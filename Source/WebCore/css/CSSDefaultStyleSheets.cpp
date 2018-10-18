/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
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
#include "CSSDefaultStyleSheets.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "HTMLAnchorElement.h"
#include "HTMLBRElement.h"
#include "HTMLBodyElement.h"
#include "HTMLDataListElement.h"
#include "HTMLDivElement.h"
#include "HTMLEmbedElement.h"
#include "HTMLHeadElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLInputElement.h"
#include "HTMLMediaElement.h"
#include "HTMLObjectElement.h"
#include "HTMLSpanElement.h"
#include "MathMLElement.h"
#include "MediaQueryEvaluator.h"
#include "Page.h"
#include "RenderTheme.h"
#include "RuleSet.h"
#include "SVGElement.h"
#include "StyleSheetContents.h"
#include "UserAgentStyleSheets.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

using namespace HTMLNames;

RuleSet* CSSDefaultStyleSheets::defaultStyle;
RuleSet* CSSDefaultStyleSheets::defaultQuirksStyle;
RuleSet* CSSDefaultStyleSheets::defaultPrintStyle;
unsigned CSSDefaultStyleSheets::defaultStyleVersion;

StyleSheetContents* CSSDefaultStyleSheets::simpleDefaultStyleSheet;
StyleSheetContents* CSSDefaultStyleSheets::defaultStyleSheet;
StyleSheetContents* CSSDefaultStyleSheets::quirksStyleSheet;
StyleSheetContents* CSSDefaultStyleSheets::svgStyleSheet;
StyleSheetContents* CSSDefaultStyleSheets::mathMLStyleSheet;
StyleSheetContents* CSSDefaultStyleSheets::mediaControlsStyleSheet;
StyleSheetContents* CSSDefaultStyleSheets::fullscreenStyleSheet;
StyleSheetContents* CSSDefaultStyleSheets::plugInsStyleSheet;
StyleSheetContents* CSSDefaultStyleSheets::imageControlsStyleSheet;
StyleSheetContents* CSSDefaultStyleSheets::mediaQueryStyleSheet;
#if ENABLE(DATALIST_ELEMENT)
StyleSheetContents* CSSDefaultStyleSheets::dataListStyleSheet;
#endif
#if ENABLE(INPUT_TYPE_COLOR)
StyleSheetContents* CSSDefaultStyleSheets::colorInputStyleSheet;
#endif

// FIXME: It would be nice to use some mechanism that guarantees this is in sync with the real UA stylesheet.
static const char* simpleUserAgentStyleSheet = "html,body,div{display:block}head{display:none}body{margin:8px}div:focus,span:focus,a:focus{outline:auto 5px -webkit-focus-ring-color}a:any-link{color:-webkit-link;text-decoration:underline}a:any-link:active{color:-webkit-activelink}";

static inline bool elementCanUseSimpleDefaultStyle(const Element& element)
{
    return is<HTMLHtmlElement>(element) || is<HTMLHeadElement>(element)
        || is<HTMLBodyElement>(element) || is<HTMLDivElement>(element)
        || is<HTMLSpanElement>(element) || is<HTMLBRElement>(element)
        || is<HTMLAnchorElement>(element);
}

static const MediaQueryEvaluator& screenEval()
{
    static NeverDestroyed<const MediaQueryEvaluator> staticScreenEval(String(MAKE_STATIC_STRING_IMPL("screen")));
    return staticScreenEval;
}

static const MediaQueryEvaluator& printEval()
{
    static NeverDestroyed<const MediaQueryEvaluator> staticPrintEval(String(MAKE_STATIC_STRING_IMPL("print")));
    return staticPrintEval;
}

static StyleSheetContents* parseUASheet(const String& str)
{
    StyleSheetContents& sheet = StyleSheetContents::create(CSSParserContext(UASheetMode)).leakRef(); // leak the sheet on purpose
    sheet.parseString(str);
    return &sheet;
}

static StyleSheetContents* parseUASheet(const char* characters, unsigned size)
{
    return parseUASheet(String(characters, size));
}

void CSSDefaultStyleSheets::initDefaultStyle(const Element* root)
{
    if (!defaultStyle) {
        if (!root || elementCanUseSimpleDefaultStyle(*root))
            loadSimpleDefaultStyle();
        else
            loadFullDefaultStyle();
    }
}

void CSSDefaultStyleSheets::addToDefaultStyle(StyleSheetContents& sheet)
{
    defaultStyle->addRulesFromSheet(sheet, screenEval());
    defaultPrintStyle->addRulesFromSheet(sheet, printEval());

    // Build a stylesheet consisting of non-trivial media queries seen in default style.
    // Rulesets for these can't be global and need to be built in document context.
    for (auto& rule : sheet.childRules()) {
        if (!is<StyleRuleMedia>(*rule))
            continue;
        auto& mediaRule = downcast<StyleRuleMedia>(*rule);
        auto* mediaQuery = mediaRule.mediaQueries();
        if (!mediaQuery)
            continue;
        if (screenEval().evaluate(*mediaQuery, nullptr))
            continue;
        if (printEval().evaluate(*mediaQuery, nullptr))
            continue;
        mediaQueryStyleSheet->parserAppendRule(mediaRule.copy());
    }

    ++defaultStyleVersion;
}

void CSSDefaultStyleSheets::loadFullDefaultStyle()
{
    if (defaultStyle && !simpleDefaultStyleSheet)
        return;
    
    if (simpleDefaultStyleSheet) {
        ASSERT(defaultStyle);
        ASSERT(defaultPrintStyle == defaultStyle);
        delete defaultStyle;
        simpleDefaultStyleSheet->deref();
        simpleDefaultStyleSheet = nullptr;
    } else {
        ASSERT(!defaultStyle);
        defaultQuirksStyle = std::make_unique<RuleSet>().release();
    }

    defaultStyle = std::make_unique<RuleSet>().release();
    defaultPrintStyle = std::make_unique<RuleSet>().release();
    mediaQueryStyleSheet = &StyleSheetContents::create(CSSParserContext(UASheetMode)).leakRef();

    // Strict-mode rules.
    String defaultRules = String(htmlUserAgentStyleSheet, sizeof(htmlUserAgentStyleSheet)) + RenderTheme::singleton().extraDefaultStyleSheet();
    defaultStyleSheet = parseUASheet(defaultRules);
    addToDefaultStyle(*defaultStyleSheet);

    // Quirks-mode rules.
    String quirksRules = String(quirksUserAgentStyleSheet, sizeof(quirksUserAgentStyleSheet)) + RenderTheme::singleton().extraQuirksStyleSheet();
    quirksStyleSheet = parseUASheet(quirksRules);
    defaultQuirksStyle->addRulesFromSheet(*quirksStyleSheet, screenEval());
}

void CSSDefaultStyleSheets::loadSimpleDefaultStyle()
{
    ASSERT(!defaultStyle);
    ASSERT(!simpleDefaultStyleSheet);

    defaultStyle = std::make_unique<RuleSet>().release();
    // There are no media-specific rules in the simple default style.
    defaultPrintStyle = defaultStyle;
    defaultQuirksStyle = std::make_unique<RuleSet>().release();

    simpleDefaultStyleSheet = parseUASheet(simpleUserAgentStyleSheet, strlen(simpleUserAgentStyleSheet));
    defaultStyle->addRulesFromSheet(*simpleDefaultStyleSheet, screenEval());
    ++defaultStyleVersion;
    // No need to initialize quirks sheet yet as there are no quirk rules for elements allowed in simple default style.
}

void CSSDefaultStyleSheets::ensureDefaultStyleSheetsForElement(const Element& element)
{
    if (simpleDefaultStyleSheet && !elementCanUseSimpleDefaultStyle(element)) {
        loadFullDefaultStyle();
        ++defaultStyleVersion;
    }

    if (is<HTMLElement>(element)) {
        if (is<HTMLObjectElement>(element) || is<HTMLEmbedElement>(element)) {
            if (!plugInsStyleSheet && element.document().page()) {
                String plugInsRules = RenderTheme::singleton().extraPlugInsStyleSheet() + element.document().page()->chrome().client().plugInExtraStyleSheet();
                if (plugInsRules.isEmpty())
                    plugInsRules = String(plugInsUserAgentStyleSheet, sizeof(plugInsUserAgentStyleSheet));
                plugInsStyleSheet = parseUASheet(plugInsRules);
                addToDefaultStyle(*plugInsStyleSheet);
            }
        }
#if ENABLE(VIDEO)
        else if (is<HTMLMediaElement>(element)) {
            if (!mediaControlsStyleSheet) {
                String mediaRules = RenderTheme::singleton().mediaControlsStyleSheet();
                if (mediaRules.isEmpty())
                    mediaRules = String(mediaControlsUserAgentStyleSheet, sizeof(mediaControlsUserAgentStyleSheet)) + RenderTheme::singleton().extraMediaControlsStyleSheet();
                mediaControlsStyleSheet = parseUASheet(mediaRules);
                addToDefaultStyle(*mediaControlsStyleSheet);

            }
        }
#endif // ENABLE(VIDEO)
#if ENABLE(SERVICE_CONTROLS)
        else if (is<HTMLDivElement>(element) && element.isImageControlsRootElement()) {
            if (!imageControlsStyleSheet) {
                String imageControlsRules = RenderTheme::singleton().imageControlsStyleSheet();
                imageControlsStyleSheet = parseUASheet(imageControlsRules);
                addToDefaultStyle(*imageControlsStyleSheet);
            }
        }
#endif // ENABLE(SERVICE_CONTROLS)
#if ENABLE(DATALIST_ELEMENT)
        else if (!dataListStyleSheet && is<HTMLDataListElement>(element)) {
            dataListStyleSheet = parseUASheet(RenderTheme::singleton().dataListStyleSheet());
            addToDefaultStyle(*dataListStyleSheet);
        }
#endif // ENABLE(DATALIST_ELEMENT)
#if ENABLE(INPUT_TYPE_COLOR)
        else if (!colorInputStyleSheet && is<HTMLInputElement>(element) && downcast<HTMLInputElement>(element).isColorControl()) {
            colorInputStyleSheet = parseUASheet(RenderTheme::singleton().colorInputStyleSheet());
            addToDefaultStyle(*colorInputStyleSheet);
        }
#endif // ENABLE(INPUT_TYPE_COLOR)
    } else if (is<SVGElement>(element)) {
        if (!svgStyleSheet) {
            // SVG rules.
            svgStyleSheet = parseUASheet(svgUserAgentStyleSheet, sizeof(svgUserAgentStyleSheet));
            addToDefaultStyle(*svgStyleSheet);
        }
    }
#if ENABLE(MATHML)
    else if (is<MathMLElement>(element)) {
        if (!mathMLStyleSheet) {
            // MathML rules.
            mathMLStyleSheet = parseUASheet(mathmlUserAgentStyleSheet, sizeof(mathmlUserAgentStyleSheet));
            addToDefaultStyle(*mathMLStyleSheet);
        }
    }
#endif // ENABLE(MATHML)

#if ENABLE(FULLSCREEN_API)
    if (!fullscreenStyleSheet && element.document().webkitIsFullScreen()) {
        String fullscreenRules = String(fullscreenUserAgentStyleSheet, sizeof(fullscreenUserAgentStyleSheet)) + RenderTheme::singleton().extraFullScreenStyleSheet();
        fullscreenStyleSheet = parseUASheet(fullscreenRules);
        addToDefaultStyle(*fullscreenStyleSheet);
    }
#endif // ENABLE(FULLSCREEN_API)

    ASSERT(defaultStyle->features().idsInRules.isEmpty());
    ASSERT(mathMLStyleSheet || defaultStyle->features().siblingRules.isEmpty());
}

} // namespace WebCore
