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

RefPtr<RuleSet>& UserAgentStyle::defaultStyle()
{
    static NeverDestroyed<RefPtr<RuleSet>> defaultStyle;
    return defaultStyle.get();
}

RefPtr<RuleSet>& UserAgentStyle::defaultQuirksStyle()
{
    static NeverDestroyed<RefPtr<RuleSet>> defaultQuirksStyle;
    return defaultQuirksStyle.get();
}

RefPtr<RuleSet>& UserAgentStyle::defaultPrintStyle()
{
    static NeverDestroyed<RefPtr<RuleSet>> defaultPrintStyle;
    return defaultPrintStyle.get();
}

unsigned UserAgentStyle::defaultStyleVersion;

RefPtr<StyleSheetContents>& UserAgentStyle::defaultStyleSheet()
{
    static NeverDestroyed<RefPtr<StyleSheetContents>> defaultStyleSheet;
    return defaultStyleSheet.get();
}

RefPtr<StyleSheetContents>& UserAgentStyle::quirksStyleSheet()
{
    static NeverDestroyed<RefPtr<StyleSheetContents>> quirksStyleSheet;
    return quirksStyleSheet.get();
}

RefPtr<StyleSheetContents>& UserAgentStyle::svgStyleSheet()
{
    static NeverDestroyed<RefPtr<StyleSheetContents>> svgStyleSheet;
    return svgStyleSheet.get();
}

RefPtr<StyleSheetContents>& UserAgentStyle::mathMLStyleSheet()
{
    static NeverDestroyed<RefPtr<StyleSheetContents>> mathMLStyleSheet;
    return mathMLStyleSheet.get();
}

RefPtr<StyleSheetContents>& UserAgentStyle::mathMLCoreExtrasStyleSheet()
{
    static NeverDestroyed<RefPtr<StyleSheetContents>> mathMLCoreExtrasStyleSheet;
    return mathMLCoreExtrasStyleSheet.get();
}

RefPtr<StyleSheetContents>& UserAgentStyle::mathMLFontSizeMathStyleSheet()
{
    static NeverDestroyed<RefPtr<StyleSheetContents>> mathMLFontSizeMathStyleSheet;
    return mathMLFontSizeMathStyleSheet.get();
}

RefPtr<StyleSheetContents>& UserAgentStyle::mathMLLegacyFontSizeMathStyleSheet()
{
    static NeverDestroyed<RefPtr<StyleSheetContents>> mathMLLegacyFontSizeMathStyleSheet;
    return mathMLLegacyFontSizeMathStyleSheet.get();
}

RefPtr<StyleSheetContents>& UserAgentStyle::mediaQueryStyleSheet()
{
    static NeverDestroyed<RefPtr<StyleSheetContents>> mediaQueryStyleSheet;
    return mediaQueryStyleSheet.get();
}

RefPtr<StyleSheetContents>& UserAgentStyle::popoverStyleSheet()
{
    static NeverDestroyed<RefPtr<StyleSheetContents>> popoverStyleSheet;
    return popoverStyleSheet.get();
}

RefPtr<StyleSheetContents>& UserAgentStyle::horizontalFormControlsStyleSheet()
{
    static NeverDestroyed<RefPtr<StyleSheetContents>> horizontalFormControlsStyleSheet;
    return horizontalFormControlsStyleSheet.get();
}

RefPtr<StyleSheetContents>& UserAgentStyle::htmlSwitchControlStyleSheet()
{
    static NeverDestroyed<RefPtr<StyleSheetContents>> htmlSwitchControlStyleSheet;
    return htmlSwitchControlStyleSheet.get();
}

RefPtr<StyleSheetContents>& UserAgentStyle::counterStylesStyleSheet()
{
    static NeverDestroyed<RefPtr<StyleSheetContents>> counterStylesStyleSheet;
    return counterStylesStyleSheet.get();
}

RefPtr<StyleSheetContents>& UserAgentStyle::viewTransitionsStyleSheet()
{
    static NeverDestroyed<RefPtr<StyleSheetContents>> viewTransitionsStyleSheet;
    return viewTransitionsStyleSheet.get();
}

#if ENABLE(FULLSCREEN_API)
RefPtr<StyleSheetContents>& UserAgentStyle::fullscreenStyleSheet()
{
    static NeverDestroyed<RefPtr<StyleSheetContents>> fullscreenStyleSheet;
    return fullscreenStyleSheet.get();
}
#endif

#if ENABLE(SERVICE_CONTROLS)
RefPtr<StyleSheetContents>& UserAgentStyle::imageControlsStyleSheet()
{
    static NeverDestroyed<RefPtr<StyleSheetContents>> imageControlsStyleSheet;
    return imageControlsStyleSheet.get();
}
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
RefPtr<StyleSheetContents>& UserAgentStyle::attachmentStyleSheet()
{
    static NeverDestroyed<RefPtr<StyleSheetContents>> attachmentStyleSheet;
    return attachmentStyleSheet.get();
}
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
    Ref sheet = StyleSheetContents::create(CSSParserContext(UASheetMode));
    sheet->parseString(str);
    return &sheet.leakRef();
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
    RuleSetBuilder screenBuilder(*defaultStyle(), screenEval());
    screenBuilder.addRulesFromSheet(sheet);

    RuleSetBuilder printBuilder(*defaultPrintStyle(), printEval());
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

    ++defaultStyleVersion;
}

void UserAgentStyle::initDefaultStyleSheet()
{
    if (defaultStyle())
        return;

    defaultStyle() = &RuleSet::create().leakRef();
    defaultPrintStyle() = &RuleSet::create().leakRef();
    defaultQuirksStyle() = &RuleSet::create().leakRef();
    mediaQueryStyleSheet() = &StyleSheetContents::create(CSSParserContext(UASheetMode)).leakRef();

    String defaultRules;
    auto extraDefaultStyleSheet = RenderTheme::singleton().extraDefaultStyleSheet();
    if (extraDefaultStyleSheet.isEmpty())
        defaultRules = StringImpl::createWithoutCopying(htmlUserAgentStyleSheet);
    else
        defaultRules = makeString(std::span { htmlUserAgentStyleSheet }, extraDefaultStyleSheet);
    defaultStyleSheet() = parseUASheet(defaultRules);
    addToDefaultStyle(*defaultStyleSheet());

    counterStylesStyleSheet() = parseUASheet(StringImpl::createWithoutCopying(counterStylesUserAgentStyleSheet));
    addToCounterStyleRegistry(*counterStylesStyleSheet());

    quirksStyleSheet() = parseUASheet(StringImpl::createWithoutCopying(quirksUserAgentStyleSheet));

    RuleSetBuilder quirkBuilder(*defaultQuirksStyle(), screenEval());
    quirkBuilder.addRulesFromSheet(*quirksStyleSheet());

    ++defaultStyleVersion;
}

void UserAgentStyle::ensureDefaultStyleSheetsForElement(const Element& element)
{
    if (is<HTMLElement>(element)) {
        if (RefPtr input = dynamicDowncast<HTMLInputElement>(element)) {
            if (!htmlSwitchControlStyleSheet() && input->isSwitch()) {
                htmlSwitchControlStyleSheet() = parseUASheet(StringImpl::createWithoutCopying(htmlSwitchControlUserAgentStyleSheet));
                addToDefaultStyle(*htmlSwitchControlStyleSheet());
            }
        }
#if ENABLE(ATTACHMENT_ELEMENT)
        else if (!attachmentStyleSheet() && is<HTMLAttachmentElement>(element)) {
            attachmentStyleSheet() = parseUASheet(RenderTheme::singleton().attachmentStyleSheet());
            addToDefaultStyle(*attachmentStyleSheet());
        }
#endif // ENABLE(ATTACHMENT_ELEMENT)

        if (!popoverStyleSheet() && element.document().settings().popoverAttributeEnabled() && element.hasAttributeWithoutSynchronization(popoverAttr)) {
            popoverStyleSheet() = parseUASheet(StringImpl::createWithoutCopying(popoverUserAgentStyleSheet));
            addToDefaultStyle(*popoverStyleSheet());
        }

        if ((is<HTMLFormControlElement>(element) || is<HTMLMeterElement>(element) || is<HTMLProgressElement>(element)) && !element.document().settings().verticalFormControlsEnabled()) {
            if (!horizontalFormControlsStyleSheet()) {
                horizontalFormControlsStyleSheet() = parseUASheet(StringImpl::createWithoutCopying(horizontalFormControlsUserAgentStyleSheet));
                addToDefaultStyle(*horizontalFormControlsStyleSheet());
            }
        }

    } else if (is<SVGElement>(element)) {
        if (!svgStyleSheet()) {
            svgStyleSheet() = parseUASheet(StringImpl::createWithoutCopying(svgUserAgentStyleSheet));
            addToDefaultStyle(*svgStyleSheet());
        }
    }
#if ENABLE(MATHML)
    else if (is<MathMLElement>(element)) {
        if (!mathMLStyleSheet()) {
            mathMLStyleSheet() = parseUASheet(StringImpl::createWithoutCopying(mathmlUserAgentStyleSheet));
            addToDefaultStyle(*mathMLStyleSheet());
        }
        if (!mathMLCoreExtrasStyleSheet() && element.document().settings().coreMathMLEnabled()) {
            mathMLCoreExtrasStyleSheet() = parseUASheet(StringImpl::createWithoutCopying(mathmlCoreExtrasUserAgentStyleSheet));
            addToDefaultStyle(*mathMLCoreExtrasStyleSheet());
        }
        if (element.document().settings().cssMathDepthEnabled()) {
            if (!mathMLFontSizeMathStyleSheet()) {
                mathMLFontSizeMathStyleSheet() = parseUASheet(StringImpl::createWithoutCopying(mathmlFontSizeMathUserAgentStyleSheet));
                addToDefaultStyle(*mathMLFontSizeMathStyleSheet());
            }
        } else {
            if (!mathMLLegacyFontSizeMathStyleSheet()) {
                mathMLLegacyFontSizeMathStyleSheet() = parseUASheet(StringImpl::createWithoutCopying(mathmlLegacyFontSizeMathUserAgentStyleSheet));
                addToDefaultStyle(*mathMLLegacyFontSizeMathStyleSheet());
            }
        }
    }
#endif // ENABLE(MATHML)

#if ENABLE(FULLSCREEN_API)
    if (RefPtr documentFullscreen = element.document().fullscreenIfExists(); !fullscreenStyleSheet() && documentFullscreen) {
        fullscreenStyleSheet() = parseUASheet(StringImpl::createWithoutCopying(fullscreenUserAgentStyleSheet));
        addToDefaultStyle(*fullscreenStyleSheet());
    }
#endif // ENABLE(FULLSCREEN_API)

    if (!viewTransitionsStyleSheet() && element.document().settings().viewTransitionsEnabled()) {
        viewTransitionsStyleSheet() = parseUASheet(StringImpl::createWithoutCopying(viewTransitionsUserAgentStyleSheet));
        addToDefaultStyle(*viewTransitionsStyleSheet());
        addUserAgentKeyframes(*viewTransitionsStyleSheet());
    }

    ASSERT(defaultStyle()->features().idsInRules.isEmpty());
}

} // namespace Style
} // namespace WebCore
