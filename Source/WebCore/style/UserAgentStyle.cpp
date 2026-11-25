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
#include "HTMLSelectElement.h"
#include "HTMLSpanElement.h"
#include "MathMLElement.h"
#include "MediaQueryEvaluator.h"
#include "Page.h"
#include "PlatformRenderTheme.h"
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

namespace WebCore::Style {

using namespace HTMLNames;

unsigned UserAgentStyle::defaultStyleVersion;

UserAgentStyle::Sheets::Sheets() = default;

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

static Ref<StyleSheetContents> parseUASheet(const WTF::String& str)
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
    auto& sheets = UserAgentStyle::sheets();
    RuleSetBuilder screenBuilder(*sheets.defaultStyle, screenEval());
    screenBuilder.addRulesFromSheet(sheet);

    RuleSetBuilder printBuilder(*sheets.defaultPrintStyle, printEval());
    printBuilder.addRulesFromSheet(sheet);

    // Build a stylesheet consisting of non-trivial media queries seen in default style.
    // Rulesets for these can't be global and need to be built in document context.
    for (auto& rule : sheet.childRules()) {
        RefPtr mediaRule = dynamicDowncast<StyleRuleMedia>(rule);
        if (!mediaRule)
            continue;
        auto& mediaQuery = mediaRule->mediaQueries();
        if (screenEval().evaluate(mediaQuery))
            continue;
        if (printEval().evaluate(mediaQuery))
            continue;
        sheets.mediaQueryStyleSheet->parserAppendRule(mediaRule->copy());
    }

    ++defaultStyleVersion;
}

void UserAgentStyle::initDefaultStyleSheet()
{
    auto& sheets = UserAgentStyle::sheets();
    if (sheets.defaultStyle)
        return;

    lazyInitialize(sheets.defaultStyle, RuleSet::create());
    lazyInitialize(sheets.defaultPrintStyle, RuleSet::create());
    lazyInitialize(sheets.defaultQuirksStyle, RuleSet::create());
    lazyInitialize(sheets.mediaQueryStyleSheet, StyleSheetContents::create(CSSParserContext(UASheetMode)));

    WTF::String defaultRules;
    auto extraDefaultStyleSheet = RenderTheme::singleton().extraDefaultStyleSheet();
    if (extraDefaultStyleSheet.isEmpty())
        defaultRules = StringImpl::createWithoutCopying(htmlUserAgentStyleSheet);
    else
        defaultRules = makeString(std::span { htmlUserAgentStyleSheet }, extraDefaultStyleSheet);
    lazyInitialize(sheets.defaultStyleSheet, parseUASheet(defaultRules));
    addToDefaultStyle(*sheets.defaultStyleSheet);

    lazyInitialize(sheets.counterStylesStyleSheet, parseUASheet(StringImpl::createWithoutCopying(counterStylesUserAgentStyleSheet)));
    addToCounterStyleRegistry(*sheets.counterStylesStyleSheet);

    lazyInitialize(sheets.quirksStyleSheet, parseUASheet(StringImpl::createWithoutCopying(quirksUserAgentStyleSheet)));

    RuleSetBuilder quirkBuilder(*sheets.defaultQuirksStyle, screenEval());
    quirkBuilder.addRulesFromSheet(*sheets.quirksStyleSheet);

    ++defaultStyleVersion;
}

void UserAgentStyle::ensureDefaultStyleSheetsForElement(const Element& element)
{
    auto& sheets = UserAgentStyle::sheets();
    if (is<HTMLElement>(element)) {
        if (RefPtr input = dynamicDowncast<HTMLInputElement>(element)) {
            if (!sheets.htmlSwitchControlStyleSheet && input->isSwitch()) {
                lazyInitialize(sheets.htmlSwitchControlStyleSheet, parseUASheet(StringImpl::createWithoutCopying(htmlSwitchControlUserAgentStyleSheet)));
                addToDefaultStyle(*sheets.htmlSwitchControlStyleSheet);
            }
        }
#if ENABLE(ATTACHMENT_ELEMENT)
        else if (!sheets.attachmentStyleSheet && is<HTMLAttachmentElement>(element)) {
            lazyInitialize(sheets.attachmentStyleSheet, parseUASheet(RenderTheme::singleton().attachmentStyleSheet()));
            addToDefaultStyle(*sheets.attachmentStyleSheet);
        }
#endif // ENABLE(ATTACHMENT_ELEMENT)

        if (!sheets.popoverStyleSheet && element.document().settings().popoverAttributeEnabled() && element.hasAttributeWithoutSynchronization(popoverAttr)) {
            lazyInitialize(sheets.popoverStyleSheet, parseUASheet(StringImpl::createWithoutCopying(popoverUserAgentStyleSheet)));
            addToDefaultStyle(*sheets.popoverStyleSheet);
        }

        if (!sheets.selectMultipleAndListBoxStyleSheet && is<HTMLSelectElement>(element) && element.document().settings().htmlEnhancedSelectMultipleAndListBoxEnabled()) {
            lazyInitialize(sheets.selectMultipleAndListBoxStyleSheet, parseUASheet(StringImpl::createWithoutCopying(selectMultipleAndListBoxUserAgentStyleSheet)));
            addToDefaultStyle(*sheets.selectMultipleAndListBoxStyleSheet);
        }

        if (isAnyOf<HTMLFormControlElement, HTMLMeterElement, HTMLProgressElement>(element) && !element.document().settings().verticalFormControlsEnabled()) {
            if (!sheets.horizontalFormControlsStyleSheet) {
                lazyInitialize(sheets.horizontalFormControlsStyleSheet, parseUASheet(StringImpl::createWithoutCopying(horizontalFormControlsUserAgentStyleSheet)));
                addToDefaultStyle(*sheets.horizontalFormControlsStyleSheet);
            }
        }

    } else if (is<SVGElement>(element)) {
        if (!sheets.svgStyleSheet) {
            lazyInitialize(sheets.svgStyleSheet, parseUASheet(StringImpl::createWithoutCopying(svgUserAgentStyleSheet)));
            addToDefaultStyle(*sheets.svgStyleSheet);
        }
    }
#if ENABLE(MATHML)
    else if (is<MathMLElement>(element)) {
        if (!sheets.mathMLStyleSheet) {
            lazyInitialize(sheets.mathMLStyleSheet, parseUASheet(StringImpl::createWithoutCopying(mathmlUserAgentStyleSheet)));
            addToDefaultStyle(*sheets.mathMLStyleSheet);
        }
        if (!sheets.mathMLCoreExtrasStyleSheet && element.document().settings().coreMathMLEnabled()) {
            lazyInitialize(sheets.mathMLCoreExtrasStyleSheet, parseUASheet(StringImpl::createWithoutCopying(mathmlCoreExtrasUserAgentStyleSheet)));
            addToDefaultStyle(*sheets.mathMLCoreExtrasStyleSheet);
        }
        if (!sheets.mathMLCoreMathvariantStyleSheet && element.document().settings().coreMathMLDeprecateLegacyMathvariant()) {
            lazyInitialize(sheets.mathMLCoreMathvariantStyleSheet, parseUASheet(StringImpl::createWithoutCopying(mathmlCoreMathvariantUserAgentStyleSheet)));
            addToDefaultStyle(*sheets.mathMLCoreMathvariantStyleSheet);
        }
        if (element.document().settings().cssMathDepthEnabled()) {
            if (!sheets.mathMLFontSizeMathStyleSheet) {
                lazyInitialize(sheets.mathMLFontSizeMathStyleSheet, parseUASheet(StringImpl::createWithoutCopying(mathmlFontSizeMathUserAgentStyleSheet)));
                addToDefaultStyle(*sheets.mathMLFontSizeMathStyleSheet);
            }
        } else {
            if (!sheets.mathMLLegacyFontSizeMathStyleSheet) {
                lazyInitialize(sheets.mathMLLegacyFontSizeMathStyleSheet, parseUASheet(StringImpl::createWithoutCopying(mathmlLegacyFontSizeMathUserAgentStyleSheet)));
                addToDefaultStyle(*sheets.mathMLLegacyFontSizeMathStyleSheet);
            }
        }
    }
#endif // ENABLE(MATHML)

#if ENABLE(FULLSCREEN_API)
    if (RefPtr documentFullscreen = element.document().fullscreenIfExists(); !sheets.fullscreenStyleSheet && documentFullscreen) {
        lazyInitialize(sheets.fullscreenStyleSheet, parseUASheet(StringImpl::createWithoutCopying(fullscreenUserAgentStyleSheet)));
        addToDefaultStyle(*sheets.fullscreenStyleSheet);
    }
#endif // ENABLE(FULLSCREEN_API)

    if (!sheets.viewTransitionsStyleSheet) {
        lazyInitialize(sheets.viewTransitionsStyleSheet, parseUASheet(StringImpl::createWithoutCopying(viewTransitionsUserAgentStyleSheet)));
        addToDefaultStyle(*sheets.viewTransitionsStyleSheet);
        addUserAgentKeyframes(*sheets.viewTransitionsStyleSheet);
    }

    ASSERT(sheets.defaultStyle->features().idsInRules.isEmpty());
}

} // namespace WebCore::Style
