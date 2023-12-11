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
#include "UserAgentStyle.h"

#include "CSSCounterStyleRegistry.h"
#include "CSSCounterStyleRule.h"
#include "CSSValuePool.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ElementInlines.h"
#include "FullscreenManager.h"
#include "HTMLAnchorElement.h"
#include "HTMLAttachmentElement.h"
#include "HTMLBRElement.h"
#include "HTMLBodyElement.h"
#include "HTMLDataListElement.h"
#include "HTMLDivElement.h"
#include "HTMLEmbedElement.h"
#include "HTMLHeadElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLInputElement.h"
#include "HTMLMediaElement.h"
#include "HTMLMeterElement.h"
#include "HTMLObjectElement.h"
#include "HTMLProgressElement.h"
#include "HTMLSpanElement.h"
#include "MathMLElement.h"
#include "MediaQueryEvaluator.h"
#include "Page.h"
#include "Quirks.h"
#include "RenderTheme.h"
#include "RuleSetBuilder.h"
#include "SVGElement.h"
#include "StyleSheetContents.h"
#include "UserAgentStyleSheets.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {
namespace Style {

using namespace HTMLNames;

RuleSet* UserAgentStyle::defaultStyle;
RuleSet* UserAgentStyle::defaultQuirksStyle;
RuleSet* UserAgentStyle::defaultPrintStyle;
unsigned UserAgentStyle::defaultStyleVersion;

StyleSheetContents* UserAgentStyle::defaultStyleSheet;
StyleSheetContents* UserAgentStyle::quirksStyleSheet;
StyleSheetContents* UserAgentStyle::svgStyleSheet;
StyleSheetContents* UserAgentStyle::mathMLStyleSheet;
StyleSheetContents* UserAgentStyle::mediaControlsStyleSheet;
StyleSheetContents* UserAgentStyle::mediaQueryStyleSheet;
StyleSheetContents* UserAgentStyle::popoverStyleSheet;
StyleSheetContents* UserAgentStyle::plugInsStyleSheet;
StyleSheetContents* UserAgentStyle::horizontalFormControlsStyleSheet;
StyleSheetContents* UserAgentStyle::htmlSwitchControlStyleSheet;
StyleSheetContents* UserAgentStyle::counterStylesStyleSheet;
StyleSheetContents* UserAgentStyle::rubyStyleSheet;
StyleSheetContents* UserAgentStyle::viewTransitionsStyleSheet;
#if ENABLE(FULLSCREEN_API)
StyleSheetContents* UserAgentStyle::fullscreenStyleSheet;
#endif
#if ENABLE(SERVICE_CONTROLS)
StyleSheetContents* UserAgentStyle::imageControlsStyleSheet;
#endif
#if ENABLE(ATTACHMENT_ELEMENT)
StyleSheetContents* UserAgentStyle::attachmentStyleSheet;
#endif
#if ENABLE(DATALIST_ELEMENT)
StyleSheetContents* UserAgentStyle::dataListStyleSheet;
#endif
#if ENABLE(INPUT_TYPE_COLOR)
StyleSheetContents* UserAgentStyle::colorInputStyleSheet;
#endif

static const MQ::MediaQueryEvaluator& screenEval()
{
    static NeverDestroyed<const MQ::MediaQueryEvaluator> staticScreenEval(screenAtom());
    return staticScreenEval;
}

static const MQ::MediaQueryEvaluator& printEval()
{
    static NeverDestroyed<const MQ::MediaQueryEvaluator> staticPrintEval(printAtom());
    return staticPrintEval;
}

static StyleSheetContents* parseUASheet(const String& str)
{
    StyleSheetContents& sheet = StyleSheetContents::create(CSSParserContext(UASheetMode)).leakRef(); // leak the sheet on purpose
    sheet.parseString(str);
    return &sheet;
}
void static addToCounterStyleRegistry(StyleSheetContents& sheet)
{
    for (auto& rule : sheet.childRules()) {
        if (auto* counterStyleRule = dynamicDowncast<StyleRuleCounterStyle>(rule.get()))
            CSSCounterStyleRegistry::addUserAgentCounterStyle(counterStyleRule->descriptors());
    }
    CSSCounterStyleRegistry::resolveUserAgentReferences();
}

void UserAgentStyle::addToDefaultStyle(StyleSheetContents& sheet)
{
    RuleSetBuilder screenBuilder(*defaultStyle, screenEval());
    screenBuilder.addRulesFromSheet(sheet);

    RuleSetBuilder printBuilder(*defaultPrintStyle, printEval());
    printBuilder.addRulesFromSheet(sheet);

    // Build a stylesheet consisting of non-trivial media queries seen in default style.
    // Rulesets for these can't be global and need to be built in document context.
    for (auto& rule : sheet.childRules()) {
        auto mediaRule = dynamicDowncast<StyleRuleMedia>(rule);
        if (!mediaRule)
            continue;
        auto& mediaQuery = mediaRule->mediaQueries();
        if (screenEval().evaluate(mediaQuery))
            continue;
        if (printEval().evaluate(mediaQuery))
            continue;
        mediaQueryStyleSheet->parserAppendRule(mediaRule->copy());
    }

    ++defaultStyleVersion;
}

void UserAgentStyle::initDefaultStyleSheet()
{
    if (defaultStyle)
        return;

    defaultStyle = &RuleSet::create().leakRef();
    defaultPrintStyle = &RuleSet::create().leakRef();
    defaultQuirksStyle = &RuleSet::create().leakRef();
    mediaQueryStyleSheet = &StyleSheetContents::create(CSSParserContext(UASheetMode)).leakRef();

    // Strict-mode rules.
    String defaultRules = String(StringImpl::createWithoutCopying(htmlUserAgentStyleSheet, sizeof(htmlUserAgentStyleSheet))) + RenderTheme::singleton().extraDefaultStyleSheet();
    defaultStyleSheet = parseUASheet(defaultRules);
    addToDefaultStyle(*defaultStyleSheet);

    // Quirks-mode rules.
    String quirksRules = String(StringImpl::createWithoutCopying(quirksUserAgentStyleSheet, sizeof(quirksUserAgentStyleSheet))) + RenderTheme::singleton().extraQuirksStyleSheet();
    quirksStyleSheet = parseUASheet(quirksRules);

    RuleSetBuilder quirkBuilder(*defaultQuirksStyle, screenEval());
    quirkBuilder.addRulesFromSheet(*quirksStyleSheet);

    ++defaultStyleVersion;
}

void UserAgentStyle::ensureDefaultStyleSheetsForElement(const Element& element)
{
    if (is<HTMLElement>(element)) {
        if (is<HTMLObjectElement>(element) || is<HTMLEmbedElement>(element)) {
            if (!plugInsStyleSheet && element.document().page()) {
                String plugInsRules = RenderTheme::singleton().extraPlugInsStyleSheet() + element.document().page()->chrome().client().plugInExtraStyleSheet();
                if (plugInsRules.isEmpty())
                    plugInsRules = String(StringImpl::createWithoutCopying(plugInsUserAgentStyleSheet, sizeof(plugInsUserAgentStyleSheet)));
                plugInsStyleSheet = parseUASheet(plugInsRules);
                addToDefaultStyle(*plugInsStyleSheet);
            }
        }
#if ENABLE(VIDEO) && !ENABLE(MODERN_MEDIA_CONTROLS)
        else if (is<HTMLMediaElement>(element)) {
            if (!mediaControlsStyleSheet) {
                String mediaRules = RenderTheme::singleton().mediaControlsStyleSheet();
                if (mediaRules.isEmpty())
                    mediaRules = String(StringImpl::createWithoutCopying(mediaControlsUserAgentStyleSheet, sizeof(mediaControlsUserAgentStyleSheet))) + RenderTheme::singleton().extraMediaControlsStyleSheet();
                mediaControlsStyleSheet = parseUASheet(mediaRules);
                addToDefaultStyle(*mediaControlsStyleSheet);

            }
        }
#endif // ENABLE(VIDEO) && !ENABLE(MODERN_MEDIA_CONTROLS)
#if ENABLE(ATTACHMENT_ELEMENT)
        else if (!attachmentStyleSheet && is<HTMLAttachmentElement>(element)) {
            attachmentStyleSheet = parseUASheet(RenderTheme::singleton().attachmentStyleSheet());
            addToDefaultStyle(*attachmentStyleSheet);
        }
#endif // ENABLE(ATTACHMENT_ELEMENT)
#if ENABLE(DATALIST_ELEMENT)
        else if (!dataListStyleSheet && is<HTMLDataListElement>(element)) {
            dataListStyleSheet = parseUASheet(RenderTheme::singleton().dataListStyleSheet());
            addToDefaultStyle(*dataListStyleSheet);
        }
#endif // ENABLE(DATALIST_ELEMENT)
#if ENABLE(INPUT_TYPE_COLOR)
        else if (RefPtr input = dynamicDowncast<HTMLInputElement>(element); !colorInputStyleSheet && input && input->isColorControl()) {
            colorInputStyleSheet = parseUASheet(RenderTheme::singleton().colorInputStyleSheet());
            addToDefaultStyle(*colorInputStyleSheet);
        }
#endif // ENABLE(INPUT_TYPE_COLOR)
        else if (RefPtr input = dynamicDowncast<HTMLInputElement>(element); !htmlSwitchControlStyleSheet && input && input->isSwitch()) {
            htmlSwitchControlStyleSheet = parseUASheet(StringImpl::createWithoutCopying(htmlSwitchControlUserAgentStyleSheet, sizeof(htmlSwitchControlUserAgentStyleSheet)));
            addToDefaultStyle(*htmlSwitchControlStyleSheet);
        }
    } else if (is<SVGElement>(element)) {
        if (!svgStyleSheet) {
            // SVG rules.
            svgStyleSheet = parseUASheet(StringImpl::createWithoutCopying(svgUserAgentStyleSheet, sizeof(svgUserAgentStyleSheet)));
            addToDefaultStyle(*svgStyleSheet);
        }
    }
#if ENABLE(MATHML)
    else if (is<MathMLElement>(element)) {
        if (!mathMLStyleSheet) {
            // MathML rules.
            mathMLStyleSheet = parseUASheet(StringImpl::createWithoutCopying(mathmlUserAgentStyleSheet, sizeof(mathmlUserAgentStyleSheet)));
            addToDefaultStyle(*mathMLStyleSheet);
        }
    }
#endif // ENABLE(MATHML)

    bool popoverAttributeEnabled = element.document().settings().popoverAttributeEnabled() && !element.document().quirks().shouldDisablePopoverAttributeQuirk();
    if (!popoverStyleSheet && popoverAttributeEnabled && element.hasAttributeWithoutSynchronization(popoverAttr)) {
        popoverStyleSheet = parseUASheet(StringImpl::createWithoutCopying(popoverUserAgentStyleSheet, sizeof(popoverUserAgentStyleSheet)));
        addToDefaultStyle(*popoverStyleSheet);
    }

    if (!counterStylesStyleSheet) {
        counterStylesStyleSheet = parseUASheet(StringImpl::createWithoutCopying(counterStylesUserAgentStyleSheet, sizeof(counterStylesUserAgentStyleSheet)));
        addToCounterStyleRegistry(*counterStylesStyleSheet);
    }

    if (!rubyStyleSheet && element.document().settings().cssBasedRubyEnabled()) {
        rubyStyleSheet = parseUASheet(StringImpl::createWithoutCopying(rubyUserAgentStyleSheet, sizeof(rubyUserAgentStyleSheet)));
        addToDefaultStyle(*rubyStyleSheet);
    }

#if ENABLE(FULLSCREEN_API)
    if (!fullscreenStyleSheet && element.document().fullscreenManager().isFullscreen()) {
        fullscreenStyleSheet = parseUASheet(StringImpl::createWithoutCopying(fullscreenUserAgentStyleSheet, sizeof(fullscreenUserAgentStyleSheet)));
        addToDefaultStyle(*fullscreenStyleSheet);
    }
#endif // ENABLE(FULLSCREEN_API)

    if ((is<HTMLFormControlElement>(element) || is<HTMLMeterElement>(element) || is<HTMLProgressElement>(element)) && !element.document().settings().verticalFormControlsEnabled()) {
        if (!horizontalFormControlsStyleSheet) {
            horizontalFormControlsStyleSheet = parseUASheet(StringImpl::createWithoutCopying(horizontalFormControlsUserAgentStyleSheet, sizeof(horizontalFormControlsUserAgentStyleSheet)));
            addToDefaultStyle(*horizontalFormControlsStyleSheet);
        }
    }

    if (!viewTransitionsStyleSheet && element.document().settings().viewTransitionsEnabled()) {
        viewTransitionsStyleSheet = parseUASheet(StringImpl::createWithoutCopying(viewTransitionsUserAgentStyleSheet, sizeof(viewTransitionsUserAgentStyleSheet)));
        addToDefaultStyle(*viewTransitionsStyleSheet);
    }

    ASSERT(defaultStyle->features().idsInRules.isEmpty());
    ASSERT(mathMLStyleSheet || defaultStyle->features().siblingRules.isEmpty());
}

} // namespace Style
} // namespace WebCore
