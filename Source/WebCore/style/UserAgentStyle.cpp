/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2025 Apple Inc. All rights reserved.
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
#include "CSSKeyframesRule.h"
#include "CSSValuePool.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "DocumentFullscreen.h"
#include "ElementInlines.h"
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
#include "Settings.h"
#include "StyleResolver.h"
#include "StyleSheetContents.h"
#include "UserAgentStyleSheets.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/MakeString.h>

namespace WebCore {
namespace Style {

using namespace HTMLNames;

namespace {

struct UserAgentStyleState {
    RefPtr<RuleSet> defaultStyle;
    RefPtr<RuleSet> defaultQuirksStyle;
    RefPtr<RuleSet> defaultPrintStyle;
    unsigned defaultStyleVersion { 0 };

    RefPtr<StyleSheetContents> defaultStyleSheet;
    RefPtr<StyleSheetContents> quirksStyleSheet;
    RefPtr<StyleSheetContents> svgStyleSheet;
    RefPtr<StyleSheetContents> mathMLStyleSheet;
    RefPtr<StyleSheetContents> mathMLCoreExtrasStyleSheet;
    RefPtr<StyleSheetContents> mathMLFontSizeMathStyleSheet;
    RefPtr<StyleSheetContents> mathMLLegacyFontSizeMathStyleSheet;
    RefPtr<StyleSheetContents> mediaQueryStyleSheet;
    RefPtr<StyleSheetContents> popoverStyleSheet;
    RefPtr<StyleSheetContents> horizontalFormControlsStyleSheet;
    RefPtr<StyleSheetContents> htmlSwitchControlStyleSheet;
    RefPtr<StyleSheetContents> counterStylesStyleSheet;
    RefPtr<StyleSheetContents> viewTransitionsStyleSheet;
#if ENABLE(FULLSCREEN_API)
    RefPtr<StyleSheetContents> fullscreenStyleSheet;
#endif
#if ENABLE(SERVICE_CONTROLS)
    RefPtr<StyleSheetContents> imageControlsStyleSheet;
#endif
#if ENABLE(ATTACHMENT_ELEMENT)
    RefPtr<StyleSheetContents> attachmentStyleSheet;
#endif
};

UserAgentStyleState& userAgentStyleState()
{
    static MainThreadNeverDestroyed<UserAgentStyleState> state;
    return state;
}

RefPtr<RuleSet>& defaultStyleStorage()
{
    return userAgentStyleState().defaultStyle;
}

RefPtr<RuleSet>& defaultQuirksStyleStorage()
{
    return userAgentStyleState().defaultQuirksStyle;
}

RefPtr<RuleSet>& defaultPrintStyleStorage()
{
    return userAgentStyleState().defaultPrintStyle;
}

RefPtr<StyleSheetContents>& defaultStyleSheetStorage()
{
    return userAgentStyleState().defaultStyleSheet;
}

RefPtr<StyleSheetContents>& quirksStyleSheetStorage()
{
    return userAgentStyleState().quirksStyleSheet;
}

RefPtr<StyleSheetContents>& svgStyleSheetStorage()
{
    return userAgentStyleState().svgStyleSheet;
}

RefPtr<StyleSheetContents>& mathMLStyleSheetStorage()
{
    return userAgentStyleState().mathMLStyleSheet;
}

RefPtr<StyleSheetContents>& mathMLCoreExtrasStyleSheetStorage()
{
    return userAgentStyleState().mathMLCoreExtrasStyleSheet;
}

RefPtr<StyleSheetContents>& mathMLFontSizeMathStyleSheetStorage()
{
    return userAgentStyleState().mathMLFontSizeMathStyleSheet;
}

RefPtr<StyleSheetContents>& mathMLLegacyFontSizeMathStyleSheetStorage()
{
    return userAgentStyleState().mathMLLegacyFontSizeMathStyleSheet;
}

RefPtr<StyleSheetContents>& mediaQueryStyleSheetStorage()
{
    return userAgentStyleState().mediaQueryStyleSheet;
}

RefPtr<StyleSheetContents>& popoverStyleSheetStorage()
{
    return userAgentStyleState().popoverStyleSheet;
}

RefPtr<StyleSheetContents>& horizontalFormControlsStyleSheetStorage()
{
    return userAgentStyleState().horizontalFormControlsStyleSheet;
}

RefPtr<StyleSheetContents>& htmlSwitchControlStyleSheetStorage()
{
    return userAgentStyleState().htmlSwitchControlStyleSheet;
}

RefPtr<StyleSheetContents>& counterStylesStyleSheetStorage()
{
    return userAgentStyleState().counterStylesStyleSheet;
}

RefPtr<StyleSheetContents>& viewTransitionsStyleSheetStorage()
{
    return userAgentStyleState().viewTransitionsStyleSheet;
}

#if ENABLE(FULLSCREEN_API)
RefPtr<StyleSheetContents>& fullscreenStyleSheetStorage()
{
    return userAgentStyleState().fullscreenStyleSheet;
}
#endif

#if ENABLE(SERVICE_CONTROLS)
RefPtr<StyleSheetContents>& imageControlsStyleSheetStorage()
{
    return userAgentStyleState().imageControlsStyleSheet;
}
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
RefPtr<StyleSheetContents>& attachmentStyleSheetStorage()
{
    return userAgentStyleState().attachmentStyleSheet;
}
#endif

} // namespace

RuleSet& UserAgentStyle::defaultStyle()
{
    ASSERT(defaultStyleStorage());
    return *defaultStyleStorage();
}

RefPtr<RuleSet> UserAgentStyle::defaultStyleIfExists()
{
    return defaultStyleStorage();
}

RuleSet& UserAgentStyle::defaultQuirksStyle()
{
    ASSERT(defaultQuirksStyleStorage());
    return *defaultQuirksStyleStorage();
}

RefPtr<RuleSet> UserAgentStyle::defaultQuirksStyleIfExists()
{
    return defaultQuirksStyleStorage();
}

RuleSet& UserAgentStyle::defaultPrintStyle()
{
    ASSERT(defaultPrintStyleStorage());
    return *defaultPrintStyleStorage();
}

RefPtr<RuleSet> UserAgentStyle::defaultPrintStyleIfExists()
{
    return defaultPrintStyleStorage();
}

unsigned UserAgentStyle::defaultStyleVersion()
{
    return userAgentStyleState().defaultStyleVersion;
}

RefPtr<StyleSheetContents> UserAgentStyle::defaultStyleSheet()
{
    return defaultStyleSheetStorage();
}

RefPtr<StyleSheetContents> UserAgentStyle::quirksStyleSheet()
{
    return quirksStyleSheetStorage();
}

RefPtr<StyleSheetContents> UserAgentStyle::svgStyleSheet()
{
    return svgStyleSheetStorage();
}

RefPtr<StyleSheetContents> UserAgentStyle::mathMLStyleSheet()
{
    return mathMLStyleSheetStorage();
}

RefPtr<StyleSheetContents> UserAgentStyle::mathMLCoreExtrasStyleSheet()
{
    return mathMLCoreExtrasStyleSheetStorage();
}

RefPtr<StyleSheetContents> UserAgentStyle::mathMLFontSizeMathStyleSheet()
{
    return mathMLFontSizeMathStyleSheetStorage();
}

RefPtr<StyleSheetContents> UserAgentStyle::mathMLLegacyFontSizeMathStyleSheet()
{
    return mathMLLegacyFontSizeMathStyleSheetStorage();
}

RefPtr<StyleSheetContents> UserAgentStyle::mediaQueryStyleSheet()
{
    return mediaQueryStyleSheetStorage();
}

RefPtr<StyleSheetContents> UserAgentStyle::popoverStyleSheet()
{
    return popoverStyleSheetStorage();
}

RefPtr<StyleSheetContents> UserAgentStyle::horizontalFormControlsStyleSheet()
{
    return horizontalFormControlsStyleSheetStorage();
}

RefPtr<StyleSheetContents> UserAgentStyle::htmlSwitchControlStyleSheet()
{
    return htmlSwitchControlStyleSheetStorage();
}

RefPtr<StyleSheetContents> UserAgentStyle::counterStylesStyleSheet()
{
    return counterStylesStyleSheetStorage();
}

RefPtr<StyleSheetContents> UserAgentStyle::viewTransitionsStyleSheet()
{
    return viewTransitionsStyleSheetStorage();
}

#if ENABLE(FULLSCREEN_API)
RefPtr<StyleSheetContents> UserAgentStyle::fullscreenStyleSheet()
{
    return fullscreenStyleSheetStorage();
}
#endif

#if ENABLE(SERVICE_CONTROLS)
RefPtr<StyleSheetContents> UserAgentStyle::imageControlsStyleSheet()
{
    return imageControlsStyleSheetStorage();
}
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
RefPtr<StyleSheetContents> UserAgentStyle::attachmentStyleSheet()
{
    return attachmentStyleSheetStorage();
}
#endif

static const MQ::MediaQueryEvaluator& screenEval()
{
    static MainThreadNeverDestroyed<const MQ::MediaQueryEvaluator> screenEvaluator(screenAtom());
    return screenEvaluator;
}

static const MQ::MediaQueryEvaluator& printEval()
{
    static MainThreadNeverDestroyed<const MQ::MediaQueryEvaluator> printEvaluator(printAtom());
    return printEvaluator;
}

static Ref<StyleSheetContents> parseUASheet(const String& str)
{
    Ref sheet = StyleSheetContents::create(CSSParserContext(UASheetMode));
    sheet->parseString(str);
    return sheet;
}

void static addToCounterStyleRegistry(StyleSheetContents& sheet)
{
    for (auto& rule : sheet.childRules()) {
        if (RefPtr counterStyleRule = dynamicDowncast<StyleRuleCounterStyle>(rule.get()))
            CSSCounterStyleRegistry::addUserAgentCounterStyle(counterStyleRule->descriptors());
    }
    CSSCounterStyleRegistry::resolveUserAgentReferences();
}

void static addUserAgentKeyframes(StyleSheetContents& sheet)
{
    // This does not handle nested rules.
    for (auto& rule : sheet.childRules()) {
        if (RefPtr styleRuleKeyframes = dynamicDowncast<StyleRuleKeyframes>(rule.get()))
            Style::Resolver::addUserAgentKeyframeStyle(*styleRuleKeyframes);
    }
}

void UserAgentStyle::addToDefaultStyle(StyleSheetContents& sheet)
{
    RuleSetBuilder screenBuilder(defaultStyle(), screenEval());
    screenBuilder.addRulesFromSheet(sheet);

    RuleSetBuilder printBuilder(defaultPrintStyle(), printEval());
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
        mediaQueryStyleSheet()->parserAppendRule(mediaRule->copy());
    }

    ++userAgentStyleState().defaultStyleVersion;
}

void UserAgentStyle::initDefaultStyleSheet()
{
    if (defaultStyleIfExists())
        return;

    defaultStyleStorage() = RuleSet::create();
    defaultPrintStyleStorage() = RuleSet::create();
    defaultQuirksStyleStorage() = RuleSet::create();
    mediaQueryStyleSheetStorage() = StyleSheetContents::create(CSSParserContext(UASheetMode));

    String defaultRules;
    auto extraDefaultStyleSheetStr = RenderTheme::singleton().extraDefaultStyleSheet();
    if (extraDefaultStyleSheetStr.isEmpty())
        defaultRules = StringImpl::createWithoutCopying(htmlUserAgentStyleSheet);
    else
        defaultRules = makeString(std::span { htmlUserAgentStyleSheet }, extraDefaultStyleSheetStr);
    defaultStyleSheetStorage() = parseUASheet(defaultRules);
    addToDefaultStyle(*defaultStyleSheet());

    counterStylesStyleSheetStorage() = parseUASheet(StringImpl::createWithoutCopying(counterStylesUserAgentStyleSheet));
    addToCounterStyleRegistry(*counterStylesStyleSheet());

    quirksStyleSheetStorage() = parseUASheet(StringImpl::createWithoutCopying(quirksUserAgentStyleSheet));

    RuleSetBuilder quirkBuilder(defaultQuirksStyle(), screenEval());
    quirkBuilder.addRulesFromSheet(*quirksStyleSheet());

    ++userAgentStyleState().defaultStyleVersion;
}

void UserAgentStyle::ensureDefaultStyleSheetsForElement(const Element& element)
{
    if (is<HTMLElement>(element)) {
        if (RefPtr input = dynamicDowncast<HTMLInputElement>(element)) {
            if (!htmlSwitchControlStyleSheet() && input->isSwitch()) {
                htmlSwitchControlStyleSheetStorage() = parseUASheet(StringImpl::createWithoutCopying(htmlSwitchControlUserAgentStyleSheet));
                addToDefaultStyle(*htmlSwitchControlStyleSheet());
            }
        }
#if ENABLE(ATTACHMENT_ELEMENT)
        else if (!attachmentStyleSheet() && is<HTMLAttachmentElement>(element)) {
            attachmentStyleSheetStorage() = parseUASheet(RenderTheme::singleton().attachmentStyleSheet());
            addToDefaultStyle(*attachmentStyleSheet());
        }
#endif // ENABLE(ATTACHMENT_ELEMENT)

        if (!popoverStyleSheet() && element.document().settings().popoverAttributeEnabled() && element.hasAttributeWithoutSynchronization(popoverAttr)) {
            popoverStyleSheetStorage() = parseUASheet(StringImpl::createWithoutCopying(popoverUserAgentStyleSheet));
            addToDefaultStyle(*popoverStyleSheet());
        }

        if (isAnyOf<HTMLFormControlElement, HTMLMeterElement, HTMLProgressElement>(element) && !element.document().settings().verticalFormControlsEnabled()) {
            if (!horizontalFormControlsStyleSheet()) {
                horizontalFormControlsStyleSheetStorage() = parseUASheet(StringImpl::createWithoutCopying(horizontalFormControlsUserAgentStyleSheet));
                addToDefaultStyle(*horizontalFormControlsStyleSheet());
            }
        }

    } else if (is<SVGElement>(element)) {
        if (!svgStyleSheet()) {
            svgStyleSheetStorage() = parseUASheet(StringImpl::createWithoutCopying(svgUserAgentStyleSheet));
            addToDefaultStyle(*svgStyleSheet());
        }
    }
#if ENABLE(MATHML)
    else if (is<MathMLElement>(element)) {
        if (!mathMLStyleSheet()) {
            mathMLStyleSheetStorage() = parseUASheet(StringImpl::createWithoutCopying(mathmlUserAgentStyleSheet));
            addToDefaultStyle(*mathMLStyleSheet());
        }
        if (!mathMLCoreExtrasStyleSheet() && element.document().settings().coreMathMLEnabled()) {
            mathMLCoreExtrasStyleSheetStorage() = parseUASheet(StringImpl::createWithoutCopying(mathmlCoreExtrasUserAgentStyleSheet));
            addToDefaultStyle(*mathMLCoreExtrasStyleSheet());
        }
        if (element.document().settings().cssMathDepthEnabled()) {
            if (!mathMLFontSizeMathStyleSheet()) {
                mathMLFontSizeMathStyleSheetStorage() = parseUASheet(StringImpl::createWithoutCopying(mathmlFontSizeMathUserAgentStyleSheet));
                addToDefaultStyle(*mathMLFontSizeMathStyleSheet());
            }
        } else {
            if (!mathMLLegacyFontSizeMathStyleSheet()) {
                mathMLLegacyFontSizeMathStyleSheetStorage() = parseUASheet(StringImpl::createWithoutCopying(mathmlLegacyFontSizeMathUserAgentStyleSheet));
                addToDefaultStyle(*mathMLLegacyFontSizeMathStyleSheet());
            }
        }
    }
#endif // ENABLE(MATHML)

#if ENABLE(FULLSCREEN_API)
    if (RefPtr documentFullscreen = element.document().fullscreenIfExists(); !fullscreenStyleSheet() && documentFullscreen) {
        fullscreenStyleSheetStorage() = parseUASheet(StringImpl::createWithoutCopying(fullscreenUserAgentStyleSheet));
        addToDefaultStyle(*fullscreenStyleSheet());
    }
#endif // ENABLE(FULLSCREEN_API)

    if (!viewTransitionsStyleSheet() && element.document().settings().viewTransitionsEnabled()) {
        viewTransitionsStyleSheetStorage() = parseUASheet(StringImpl::createWithoutCopying(viewTransitionsUserAgentStyleSheet));
        addToDefaultStyle(*viewTransitionsStyleSheet());
        addUserAgentKeyframes(*viewTransitionsStyleSheet());
    }

    ASSERT(defaultStyle().features().idsInRules.isEmpty());
}

} // namespace Style
} // namespace WebCore
