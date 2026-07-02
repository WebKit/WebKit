/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FrameCSSAgent.h"

#include "CSSComputedStyleDeclaration.h"
#include "CSSImportRule.h"
#include "CSSParserContext.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParserState.h"
#include "CSSPropertyParsing.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSValueKeywords.h"
#include "CommonAtomStrings.h"
#include "ContentSecurityPolicy.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "DocumentPage.h"
#include "ElementAncestorIteratorInlines.h"
#include "ExtensionStyleSheets.h"
#include "Font.h"
#include "FontCascadeInlines.h"
#include "FontPlatformData.h"
#include "FrameDOMAgent.h"
#include "HTMLHeadElement.h"
#include "HTMLStyleElement.h"
#include "InspectorDOMAgent.h"
#include "InspectorHistory.h"
#include "InspectorIdentifierRegistry.h"
#include "InstrumentingAgents.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "Page.h"
#include "PageInspectorController.h"
#include "PseudoElement.h"
#include "PseudoElementIdentifier.h"
#include "RenderStyleConstants.h"
#include "SelectorChecker.h"
#include "StyleDisplay.h"
#include "StyleComputedStyleBase+GettersInlines.h"
#include "StyleDocumentScope.h"
#include "StyleProperties.h"
#include "StylePropertyShorthand.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include "StyleScope.h"
#include "StyleSheetContents.h"
#include "StyledElement.h"
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameCSSAgent);

static std::optional<Inspector::Protocol::CSS::PseudoId> protocolValueForPseudoElementType(PseudoElementType pseudoElementType)
{
    switch (pseudoElementType) {
    case PseudoElementType::FirstLine:
        return Inspector::Protocol::CSS::PseudoId::FirstLine;
    case PseudoElementType::FirstLetter:
        return Inspector::Protocol::CSS::PseudoId::FirstLetter;
    case PseudoElementType::GrammarError:
        return Inspector::Protocol::CSS::PseudoId::GrammarError;
    case PseudoElementType::Marker:
        return Inspector::Protocol::CSS::PseudoId::Marker;
    case PseudoElementType::Backdrop:
        return Inspector::Protocol::CSS::PseudoId::Backdrop;
    case PseudoElementType::Before:
        return Inspector::Protocol::CSS::PseudoId::Before;
    case PseudoElementType::After:
        return Inspector::Protocol::CSS::PseudoId::After;
    case PseudoElementType::Selection:
        return Inspector::Protocol::CSS::PseudoId::Selection;
    case PseudoElementType::Highlight:
        return Inspector::Protocol::CSS::PseudoId::Highlight;
    case PseudoElementType::SpellingError:
        return Inspector::Protocol::CSS::PseudoId::SpellingError;
    case PseudoElementType::TargetText:
        return Inspector::Protocol::CSS::PseudoId::TargetText;
    case PseudoElementType::Checkmark:
        return Inspector::Protocol::CSS::PseudoId::Checkmark;
    case PseudoElementType::PickerIcon:
        return Inspector::Protocol::CSS::PseudoId::PickerIcon;
    case PseudoElementType::ViewTransition:
        return Inspector::Protocol::CSS::PseudoId::ViewTransition;
    case PseudoElementType::ViewTransitionGroup:
        return Inspector::Protocol::CSS::PseudoId::ViewTransitionGroup;
    case PseudoElementType::ViewTransitionImagePair:
        return Inspector::Protocol::CSS::PseudoId::ViewTransitionImagePair;
    case PseudoElementType::ViewTransitionOld:
        return Inspector::Protocol::CSS::PseudoId::ViewTransitionOld;
    case PseudoElementType::ViewTransitionNew:
        return Inspector::Protocol::CSS::PseudoId::ViewTransitionNew;
    case PseudoElementType::WebKitResizer:
        return Inspector::Protocol::CSS::PseudoId::WebKitResizer;
    case PseudoElementType::WebKitScrollbar:
        return Inspector::Protocol::CSS::PseudoId::WebKitScrollbar;
    case PseudoElementType::WebKitScrollbarThumb:
        return Inspector::Protocol::CSS::PseudoId::WebKitScrollbarThumb;
    case PseudoElementType::WebKitScrollbarButton:
        return Inspector::Protocol::CSS::PseudoId::WebKitScrollbarButton;
    case PseudoElementType::WebKitScrollbarTrack:
        return Inspector::Protocol::CSS::PseudoId::WebKitScrollbarTrack;
    case PseudoElementType::WebKitScrollbarTrackPiece:
        return Inspector::Protocol::CSS::PseudoId::WebKitScrollbarTrackPiece;
    case PseudoElementType::WebKitScrollbarCorner:
        return Inspector::Protocol::CSS::PseudoId::WebKitScrollbarCorner;
    case PseudoElementType::InternalWritingSuggestions:
        return { };
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

FrameCSSAgent::FrameCSSAgent(FrameAgentContext& context)
    : InspectorAgentBase("CSS"_s, context)
    , m_frontendDispatcher(makeUniqueRef<Inspector::CSSFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::CSSBackendDispatcher::create(Ref { context.backendDispatcher }, this))
    , m_inspectedFrame(context.inspectedFrame)
{
}

FrameCSSAgent::~FrameCSSAgent() = default;

void FrameCSSAgent::didCreateFrontendAndBackend()
{
}

void FrameCSSAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    disable();
}

Inspector::CommandResult<void> FrameCSSAgent::enable()
{
    Ref agents = m_instrumentingAgents.get();
    if (agents->enabledFrameCSSAgent() == this)
        return { };

    agents->setEnabledFrameCSSAgent(this);

    if (RefPtr document = m_inspectedFrame->document())
        activeStyleSheetsUpdated(*document);

    return { };
}

Inspector::CommandResult<void> FrameCSSAgent::disable()
{
    Ref { m_instrumentingAgents.get() }->setEnabledFrameCSSAgent(nullptr);

    reset();

    return { };
}

Inspector::CommandResult<Ref<JSON::ArrayOf<Inspector::Protocol::CSS::CSSComputedStyleProperty>>> FrameCSSAgent::getComputedStyleForNode(Inspector::Protocol::DOM::NodeId nodeId)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr element = elementForId(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    if (!element->isConnected())
        return makeUnexpected("Element for given nodeId was not connected to DOM tree."_s);

    auto computedStyleInfo = CSSComputedStyleDeclaration::create(*element, CSSComputedStyleDeclaration::AllowVisited::Yes);
    auto inspectorStyle = InspectorStyle::create(InspectorCSSId(), WTF::move(computedStyleInfo), nullptr);
    return inspectorStyle->buildArrayForComputedStyle();
}

Inspector::CommandResult<Ref<Inspector::Protocol::CSS::Font>> FrameCSSAgent::getFontDataForNode(Inspector::Protocol::DOM::NodeId nodeId)
{
    Inspector::Protocol::ErrorString errorString;
    RefPtr element = elementForId(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    CheckedPtr computedStyle = element->computedStyle();
    if (!computedStyle)
        return makeUnexpected("No computed style for node."_s);

    Ref font = protect(computedStyle->fontCascade())->primaryFont();
    auto& fontPlatformData = font->platformData();

    auto resultVariationAxes = JSON::ArrayOf<Inspector::Protocol::CSS::FontVariationAxis>::create();
    for (auto& variationAxis : fontPlatformData.variationAxes(ShouldLocalizeAxisNames::Yes)) {
        auto axis = Inspector::Protocol::CSS::FontVariationAxis::create()
            .setTag(variationAxis.tag())
            .setMinimumValue(variationAxis.minimumValue())
            .setMaximumValue(variationAxis.maximumValue())
            .setDefaultValue(variationAxis.defaultValue())
            .release();
        if (variationAxis.name().length() && variationAxis.name() != variationAxis.tag())
            axis->setName(variationAxis.name());
        resultVariationAxes->addItem(WTF::move(axis));
    }

    auto protocolFont = Inspector::Protocol::CSS::Font::create()
        .setDisplayName(fontPlatformData.familyName())
        .setVariationAxes(WTF::move(resultVariationAxes))
        .release();
    protocolFont->setSynthesizedBold(fontPlatformData.syntheticBold());
    protocolFont->setSynthesizedOblique(fontPlatformData.syntheticOblique());

    return protocolFont;
}

Inspector::CommandResultOf<RefPtr<Inspector::Protocol::CSS::CSSStyle>, RefPtr<Inspector::Protocol::CSS::CSSStyle>> FrameCSSAgent::getInlineStylesForNode(Inspector::Protocol::DOM::NodeId nodeId)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr element = elementForId(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    RefPtr styledElement = dynamicDowncast<StyledElement>(*element);
    if (!styledElement)
        return { { nullptr, nullptr } };

    Ref styleSheet = asInspectorStyleSheet(*styledElement);
    return { { styleSheet->buildObjectForStyle(protect(styledElement->cssomStyle()).ptr()), buildObjectForAttributesStyle(*styledElement) } };
}

Inspector::CommandResultOf<RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::RuleMatch>>, RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::PseudoIdMatches>>, RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::InheritedStyleEntry>>> FrameCSSAgent::getMatchedStylesForNode(Inspector::Protocol::DOM::NodeId nodeId, std::optional<bool>&& includePseudo, std::optional<bool>&& includeInherited)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr element = elementForId(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    if (!element->isConnected())
        return makeUnexpected("Element for given nodeId was not connected to DOM tree."_s);

    RefPtr originalElement = element;
    auto elementPseudoId = element->pseudoElementIdentifier();
    if (elementPseudoId) {
        element = downcast<PseudoElement>(*element).hostElement();
        if (!element)
            return makeUnexpected("Missing parent of pseudo-element node for given nodeId"_s);
    }

    Ref styleResolver = element->styleResolver();
    auto matchedRules = styleResolver->pseudoStyleRulesForElement(element, elementPseudoId, Style::Resolver::AllCSSRules);
    auto matchedCSSRules = buildArrayForMatchedRuleList(matchedRules, styleResolver, *element, elementPseudoId);
    RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::PseudoIdMatches>> pseudoElements;
    RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::InheritedStyleEntry>> inherited;

    if (!originalElement->isPseudoElement()) {
        if (!includePseudo || *includePseudo) {
            pseudoElements = JSON::ArrayOf<Inspector::Protocol::CSS::PseudoIdMatches>::create();
            for (auto pseudoElementType : allPseudoElementTypes) {
                if (pseudoElementType == PseudoElementType::Marker && !element->computedStyle()->isListItemType())
                    continue;
                if (pseudoElementType == PseudoElementType::Backdrop && !element->isInTopLayer())
                    continue;
                if (pseudoElementType == PseudoElementType::ViewTransition && (!element->document().activeViewTransition() || element != element->document().documentElement()))
                    continue;

                auto pseudoElementIdentifier = Style::PseudoElementIdentifier { pseudoElementType };
                if (isNamedViewTransitionPseudoElement(pseudoElementIdentifier))
                    continue;

                if (auto protocolPseudoId = protocolValueForPseudoElementType(pseudoElementType)) {
                    auto pseudoRules = styleResolver->pseudoStyleRulesForElement(element, pseudoElementType, Style::Resolver::AllCSSRules);
                    if (!pseudoRules.isEmpty()) {
                        auto matches = Inspector::Protocol::CSS::PseudoIdMatches::create()
                            .setPseudoId(protocolPseudoId.value())
                            .setMatches(buildArrayForMatchedRuleList(pseudoRules, styleResolver, *element, pseudoElementIdentifier))
                            .release();
                        pseudoElements->addItem(WTF::move(matches));
                    }
                }
            }
        }

        if (!includeInherited || *includeInherited) {
            inherited = JSON::ArrayOf<Inspector::Protocol::CSS::InheritedStyleEntry>::create();
            for (Ref ancestor : ancestorsOfType<Element>(*element)) {
                Ref parentStyleResolver = ancestor->styleResolver();
                auto parentMatchedRules = parentStyleResolver->styleRulesForElement(ancestor.ptr(), Style::Resolver::AllCSSRules);
                auto entry = Inspector::Protocol::CSS::InheritedStyleEntry::create()
                    .setMatchedCSSRules(buildArrayForMatchedRuleList(parentMatchedRules, styleResolver, ancestor, { }))
                    .release();
                if (RefPtr styledElement = dynamicDowncast<StyledElement>(ancestor); styledElement && protect(styledElement->cssomStyle())->length()) {
                    Ref inlineStyleSheet = asInspectorStyleSheet(*styledElement);
                    entry->setInlineStyle(inlineStyleSheet->buildObjectForStyle(protect(inlineStyleSheet->styleForId(InspectorCSSId(inlineStyleSheet->id(), 0)))));
                }
                inherited->addItem(WTF::move(entry));
            }
        }
    }

    return { { WTF::move(matchedCSSRules), WTF::move(pseudoElements), WTF::move(inherited) } };
}

Inspector::CommandResult<Ref<JSON::ArrayOf<Inspector::Protocol::CSS::CSSStyleSheetHeader>>> FrameCSSAgent::getAllStyleSheets()
{
    auto headers = JSON::ArrayOf<Inspector::Protocol::CSS::CSSStyleSheetHeader>::create();

    RefPtr document = m_inspectedFrame->document();
    if (!document)
        return headers;

    Vector<CSSStyleSheet*> cssStyleSheets;
    collectAllDocumentStyleSheets(*document, cssStyleSheets);

    for (RefPtr cssStyleSheet : cssStyleSheets) {
        Ref inspectorStyleSheet = bindStyleSheet(cssStyleSheet.get());
        if (auto header = inspectorStyleSheet->buildObjectForStyleSheetInfo())
            headers->addItem(header.releaseNonNull());
    }

    return headers;
}

Inspector::CommandResult<Ref<Inspector::Protocol::CSS::CSSStyleSheetBody>> FrameCSSAgent::getStyleSheet(const Inspector::Protocol::CSS::StyleSheetId& styleSheetId)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return makeUnexpected(errorString);

    auto styleSheet = inspectorStyleSheet->buildObjectForStyleSheet();
    if (!styleSheet)
        return makeUnexpected("Internal error: missing style sheet"_s);

    return styleSheet.releaseNonNull();
}

Inspector::CommandResult<String> FrameCSSAgent::getStyleSheetText(const Inspector::Protocol::CSS::StyleSheetId& styleSheetId)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return makeUnexpected(errorString);

    auto text = inspectorStyleSheet->text();
    if (text.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(text.releaseException()));

    return text.releaseReturnValue();
}

Inspector::CommandResult<void> FrameCSSAgent::setStyleSheetText(const Inspector::Protocol::CSS::StyleSheetId& styleSheetId, const String& text)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return makeUnexpected(errorString);

    // FIXME <https://webkit.org/b/314244>: Integrate with InspectorHistory for undo/redo support.
    auto result = inspectorStyleSheet->setText(text);
    if (result.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(result.releaseException()));

    inspectorStyleSheet->reparseStyleSheet(text);
    return { };
}

Inspector::CommandResult<Ref<Inspector::Protocol::CSS::CSSStyle>> FrameCSSAgent::setStyleText(Ref<JSON::Object>&& styleId, const String& text)
{
    Inspector::Protocol::ErrorString errorString;

    InspectorCSSId compoundId { styleId };
    ASSERT(!compoundId.isEmpty());

    RefPtr inspectorStyleSheet = assertStyleSheetForId(errorString, compoundId.styleSheetId());
    if (!inspectorStyleSheet)
        return makeUnexpected(errorString);

    Ref agents = m_instrumentingAgents.get();
    CheckedPtr domAgent = agents->persistentFrameDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    auto performResult = domAgent->history()->perform(makeUnique<SetStyleTextAction>(inspectorStyleSheet.get(), compoundId, text));
    if (performResult.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(performResult.releaseException()));

    return inspectorStyleSheet->buildObjectForStyle(protect(inspectorStyleSheet->styleForId(compoundId)));
}

Inspector::CommandResult<Ref<Inspector::Protocol::CSS::CSSRule>> FrameCSSAgent::setRuleSelector(Ref<JSON::Object>&& ruleId, const String& selector)
{
    Inspector::Protocol::ErrorString errorString;

    InspectorCSSId compoundId { ruleId };
    ASSERT(!compoundId.isEmpty());

    RefPtr inspectorStyleSheet = assertStyleSheetForId(errorString, compoundId.styleSheetId());
    if (!inspectorStyleSheet)
        return makeUnexpected(errorString);

    Ref agents = m_instrumentingAgents.get();
    CheckedPtr domAgent = agents->persistentFrameDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    auto performResult = domAgent->history()->perform(makeUnique<SetRuleHeaderTextAction>(inspectorStyleSheet.get(), compoundId, selector));
    if (performResult.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(performResult.releaseException()));

    auto rule = inspectorStyleSheet->buildObjectForRule(protect(dynamicDowncast<CSSStyleRule>(inspectorStyleSheet->ruleForId(compoundId))));
    if (!rule)
        return makeUnexpected("Internal error: missing style sheet"_s);

    return rule.releaseNonNull();
}

Inspector::CommandResult<Ref<Inspector::Protocol::CSS::Grouping>> FrameCSSAgent::setGroupingHeaderText(Ref<JSON::Object>&& ruleId, const String& headerText)
{
    Inspector::Protocol::ErrorString errorString;

    InspectorCSSId compoundId { WTF::move(ruleId) };
    ASSERT(!compoundId.isEmpty());

    RefPtr inspectorStyleSheet = assertStyleSheetForId(errorString, compoundId.styleSheetId());
    if (!inspectorStyleSheet)
        return makeUnexpected(errorString);

    Ref agents = m_instrumentingAgents.get();
    CheckedPtr domAgent = agents->persistentFrameDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    auto performResult = domAgent->history()->perform(makeUnique<SetRuleHeaderTextAction>(inspectorStyleSheet.get(), compoundId, headerText));
    if (performResult.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(performResult.releaseException()));

    if (auto rule = inspectorStyleSheet->buildObjectForGrouping(protect(inspectorStyleSheet->ruleForId(compoundId))))
        return rule.releaseNonNull();

    ASSERT_NOT_REACHED();
    return makeUnexpected("Internal error: missing grouping payload"_s);
}

Inspector::CommandResult<Inspector::Protocol::CSS::StyleSheetId> FrameCSSAgent::createStyleSheet(const Inspector::Protocol::Network::FrameId&)
{
    RefPtr document = m_inspectedFrame->document();
    if (!document)
        return makeUnexpected("No document for frame"_s);

    RefPtr inspectorStyleSheet = createInspectorStyleSheetForDocument(*document);
    if (!inspectorStyleSheet)
        return makeUnexpected("Could not create stylesheet for document"_s);

    return inspectorStyleSheet->id();
}

Inspector::CommandResult<Ref<Inspector::Protocol::CSS::CSSRule>> FrameCSSAgent::addRule(const Inspector::Protocol::CSS::StyleSheetId& styleSheetId, const String& selector)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return makeUnexpected(errorString);

    Ref agents = m_instrumentingAgents.get();
    CheckedPtr domAgent = agents->persistentFrameDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    auto action = makeUnique<AddRuleAction>(inspectorStyleSheet.get(), selector);
    auto& rawAction = *action;
    auto performResult = domAgent->history()->perform(WTF::move(action));
    if (performResult.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(performResult.releaseException()));

    // FIXME <https://webkit.org/b/317684>: Reconsider whether accessing rawAction.newRuleId here is safe.
    RefPtr styleRule = dynamicDowncast<CSSStyleRule>(inspectorStyleSheet->ruleForId(rawAction.newRuleId()));
    auto rule = inspectorStyleSheet->buildObjectForRule(styleRule);
    if (!rule)
        return makeUnexpected("Internal error: missing style sheet"_s);

    return rule.releaseNonNull();
}

Inspector::CommandResult<Ref<JSON::ArrayOf<Inspector::Protocol::CSS::CSSPropertyInfo>>> FrameCSSAgent::getSupportedCSSProperties()
{
    RefPtr page = m_inspectedFrame->page();
    if (!page)
        return makeUnexpected("No page for frame"_s);

    auto cssProperties = JSON::ArrayOf<Inspector::Protocol::CSS::CSSPropertyInfo>::create();

    auto& settings = page->settings();
    auto parserContext = CSSParserContext { settings };

    for (auto propertyID : allCSSProperties()) {
        if (!isExposed(propertyID, &settings))
            continue;

        auto property = Inspector::Protocol::CSS::CSSPropertyInfo::create()
            .setName(nameString(propertyID))
            .release();

        auto aliases = CSSProperty::aliasesForProperty(propertyID);
        if (!aliases.isEmpty()) {
            auto aliasesArray = JSON::ArrayOf<String>::create();
            for (auto& alias : aliases)
                aliasesArray->addItem(alias);
            property->setAliases(WTF::move(aliasesArray));
        }

        auto shorthand = shorthandForProperty(propertyID);
        if (shorthand.length()) {
            auto longhands = JSON::ArrayOf<String>::create();
            for (auto longhand : shorthand) {
                if (isExposed(longhand, &settings))
                    longhands->addItem(nameString(longhand));
            }
            if (longhands->length())
                property->setLonghands(WTF::move(longhands));
        }

        if (CSSPropertyParsing::isKeywordFastPathEligibleStyleProperty(propertyID)) {
            auto propertyParserState = CSS::PropertyParserState {
                .context = parserContext,
            };
            auto values = JSON::ArrayOf<String>::create();
            for (auto valueID : allCSSValueKeywords()) {
                if (CSSPropertyParsing::isKeywordValidForStyleProperty(propertyID, valueID, propertyParserState))
                    values->addItem(nameString(valueID));
            }
            if (values->length())
                property->setValues(WTF::move(values));
        } else {
            auto validKeywords = CSSProperty::validKeywordsForProperty(propertyID);
            if (!validKeywords.empty()) {
                auto values = JSON::ArrayOf<String>::create();
                for (auto valueID : validKeywords) {
                    if (CSSProperty::isKeywordValidForPropertyValues(propertyID, valueID, parserContext))
                        values->addItem(nameString(valueID));
                }
                if (values->length())
                    property->setValues(WTF::move(values));
            }
        }

        if (CSSProperty::isInheritedProperty(propertyID))
            property->setInherited(true);

        cssProperties->addItem(WTF::move(property));
    }

    return cssProperties;
}

Inspector::CommandResult<Ref<JSON::ArrayOf<String>>> FrameCSSAgent::getSupportedSystemFontFamilyNames()
{
    // FIXME: <https://webkit.org/b/316844>: Consider how to deal with global CSS commands.
    return makeUnexpected("Not supported on frame targets"_s);
}

Inspector::CommandResult<void> FrameCSSAgent::forcePseudoState(Inspector::Protocol::DOM::NodeId nodeId, Ref<JSON::Array>&& forcedPseudoClasses)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr element = elementForId(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    PseudoClassHashSet forcedPseudoClassesToSet;
    for (const auto& pseudoClassValue : forcedPseudoClasses.get()) {
        auto pseudoClassString = pseudoClassValue->asString();
        if (!pseudoClassString)
            return makeUnexpected("Unexpected non-string value in given forcedPseudoClasses"_s);

        auto pseudoClass = Inspector::Protocol::Helpers::parseEnumValueFromString<Inspector::Protocol::CSS::ForceablePseudoClass>(pseudoClassString);
        if (!pseudoClass)
            return makeUnexpected(makeString("Unknown forcedPseudoClass: "_s, pseudoClassString));

        switch (*pseudoClass) {
        case Inspector::Protocol::CSS::ForceablePseudoClass::Active:
            forcedPseudoClassesToSet.add(CSSSelector::PseudoClass::Active);
            break;
        case Inspector::Protocol::CSS::ForceablePseudoClass::Hover:
            forcedPseudoClassesToSet.add(CSSSelector::PseudoClass::Hover);
            break;
        case Inspector::Protocol::CSS::ForceablePseudoClass::Focus:
            forcedPseudoClassesToSet.add(CSSSelector::PseudoClass::Focus);
            break;
        case Inspector::Protocol::CSS::ForceablePseudoClass::FocusVisible:
            forcedPseudoClassesToSet.add(CSSSelector::PseudoClass::FocusVisible);
            break;
        case Inspector::Protocol::CSS::ForceablePseudoClass::FocusWithin:
            forcedPseudoClassesToSet.add(CSSSelector::PseudoClass::FocusWithin);
            break;
        case Inspector::Protocol::CSS::ForceablePseudoClass::Target:
            forcedPseudoClassesToSet.add(CSSSelector::PseudoClass::Target);
            break;
        case Inspector::Protocol::CSS::ForceablePseudoClass::Visited:
            forcedPseudoClassesToSet.add(CSSSelector::PseudoClass::Visited);
            break;
        }
    }

    if (!forcedPseudoClassesToSet.isEmpty()) {
        m_nodeIdToForcedPseudoState.set(nodeId, WTF::move(forcedPseudoClassesToSet));
        m_documentsWithForcedPseudoStates.add(&element->document());
    } else {
        if (!m_nodeIdToForcedPseudoState.remove(nodeId))
            return { };
        if (m_nodeIdToForcedPseudoState.isEmpty())
            m_documentsWithForcedPseudoStates.clear();
    }

    element->document().styleScope().didChangeStyleSheetEnvironment();

    return { };
}

bool FrameCSSAgent::forcePseudoState(const Element& element, CSSSelector::PseudoClass pseudoClass)
{
    if (m_nodeIdToForcedPseudoState.isEmpty())
        return false;

    Ref agents = m_instrumentingAgents.get();
    CheckedPtr domAgent = agents->persistentFrameDOMAgent();
    if (!domAgent)
        return false;

    auto nodeId = domAgent->boundNodeId(&element);
    if (!nodeId)
        return false;

    return m_nodeIdToForcedPseudoState.get(nodeId).contains(pseudoClass);
}

Inspector::CommandResult<void> FrameCSSAgent::setLayoutContextTypeChangedMode(Inspector::Protocol::CSS::LayoutContextTypeChangedMode)
{
    // FIXME: <https://webkit.org/b/316844>: Consider how to deal with global CSS commands.
    return makeUnexpected("Not supported on frame targets"_s);
}

void FrameCSSAgent::styleSheetChanged(InspectorStyleSheet* inspectorStyleSheet)
{
    if (!inspectorStyleSheet)
        return;

    m_frontendDispatcher->styleSheetChanged(inspectorStyleSheet->id());
}

void FrameCSSAgent::documentDetached(Document& document)
{
    Vector<CSSStyleSheet*> emptyList;
    setActiveStyleSheetsForDocument(document, emptyList);
    m_documentToKnownCSSStyleSheets.remove(&document);
}

void FrameCSSAgent::mediaQueryResultChanged()
{
    m_frontendDispatcher->mediaQueryResultChanged();
}

void FrameCSSAgent::activeStyleSheetsUpdated(Document& document)
{
    Vector<CSSStyleSheet*> cssStyleSheets;
    collectAllDocumentStyleSheets(document, cssStyleSheets);
    setActiveStyleSheetsForDocument(document, cssStyleSheets);
}

void FrameCSSAgent::reset()
{
    m_idToInspectorStyleSheet.clear();
    m_cssStyleSheetToInspectorStyleSheet.clear();
    m_documentToInspectorStyleSheet.clear();
    m_documentToKnownCSSStyleSheets.clear();
    m_nodeToInspectorStyleSheet.clear();

    for (auto& document : m_documentsWithForcedPseudoStates)
        document->styleScope().didChangeStyleSheetEnvironment();
    m_nodeIdToForcedPseudoState.clear();
    m_documentsWithForcedPseudoStates.clear();
}

RefPtr<Element> FrameCSSAgent::elementForId(Inspector::Protocol::ErrorString& errorString, Inspector::Protocol::DOM::NodeId nodeId)
{
    Ref agents = m_instrumentingAgents.get();
    CheckedPtr domAgent = agents->persistentFrameDOMAgent();
    if (!domAgent) {
        errorString = "DOM domain must be enabled"_s;
        return nullptr;
    }
    RefPtr node = domAgent->nodeForId(nodeId);
    if (!node) {
        errorString = "Missing node for given nodeId"_s;
        return nullptr;
    }
    RefPtr element = dynamicDowncast<Element>(*node);
    if (!element)
        errorString = "Node for given nodeId is not an element"_s;
    return element;
}

InspectorStyleSheetForInlineStyle& FrameCSSAgent::asInspectorStyleSheet(StyledElement& element)
{
    auto it = m_nodeToInspectorStyleSheet.find(&element);
    if (it != m_nodeToInspectorStyleSheet.end())
        return it->value;

    auto id = String::number(m_lastStyleSheetId++);
    Ref identifierRegistry = protect(protect(m_inspectedFrame->page())->inspectorController())->identifierRegistry();
    auto inspectorStyleSheet = InspectorStyleSheetForInlineStyle::create(identifierRegistry, id, element, Inspector::Protocol::CSS::StyleSheetOrigin::Author, this);
    m_idToInspectorStyleSheet.set(id, inspectorStyleSheet.copyRef());
    return m_nodeToInspectorStyleSheet.set(&element, WTF::move(inspectorStyleSheet)).iterator->value;
}

RefPtr<Inspector::Protocol::CSS::CSSStyle> FrameCSSAgent::buildObjectForAttributesStyle(StyledElement& element)
{
    RefPtr presentationalHintStyle = element.presentationalHintStyle();
    if (!presentationalHintStyle)
        return nullptr;
    auto mutableStyle = presentationalHintStyle->mutableCopy();
    auto inspectorStyle = InspectorStyle::create(InspectorCSSId(), mutableStyle->ensureCSSStyleProperties(), nullptr);
    return inspectorStyle->buildObjectForStyle();
}

RefPtr<Inspector::Protocol::CSS::CSSRule> FrameCSSAgent::buildObjectForRule(const StyleRule* styleRule, Style::Resolver& styleResolver, Element& element)
{
    if (!styleRule)
        return nullptr;

    Ref document = element.document();
    styleResolver.inspectorCSSOMWrappers().collectDocumentWrappers(protect(document->extensionStyleSheets()));
    styleResolver.inspectorCSSOMWrappers().collectScopeWrappers(document->styleScope());
    if (RefPtr shadowRoot = element.shadowRoot())
        styleResolver.inspectorCSSOMWrappers().collectScopeWrappers(protect(shadowRoot->styleScope()));

    return buildObjectForRule(protect(styleResolver.inspectorCSSOMWrappers().getWrapperForRuleInSheets(styleRule)));
}

RefPtr<Inspector::Protocol::CSS::CSSRule> FrameCSSAgent::buildObjectForRule(CSSStyleRule* rule)
{
    if (!rule)
        return nullptr;
    ASSERT(rule->parentStyleSheet());
    return protect(bindStyleSheet(protect(rule->parentStyleSheet())))->buildObjectForRule(protect(rule));
}

Ref<JSON::ArrayOf<Inspector::Protocol::CSS::RuleMatch>> FrameCSSAgent::buildArrayForMatchedRuleList(const Vector<Ref<const StyleRule>>& matchedRules, Style::Resolver& styleResolver, Element& element, std::optional<Style::PseudoElementIdentifier> pseudoElementIdentifier)
{
    auto result = JSON::ArrayOf<Inspector::Protocol::CSS::RuleMatch>::create();

    SelectorChecker::CheckingContext context(SelectorChecker::Mode::CollectingRules);
    if (pseudoElementIdentifier)
        context.setRequestedPseudoElement(*pseudoElementIdentifier);
    SelectorChecker selectorChecker(element.document());

    for (auto& matchedRule : matchedRules) {
        RefPtr ruleObject = buildObjectForRule(matchedRule.ptr(), styleResolver, element);
        if (!ruleObject)
            continue;

        auto matchingSelectors = JSON::ArrayOf<int>::create();
        const CSSSelectorList& selectorList = matchedRule->selectorList();
        int index = 0;
        for (auto& selector : selectorList) {
            bool matched = selectorChecker.match(selector, element, context);
            if (matched)
                matchingSelectors->addItem(index);
            ++index;
        }

        auto match = Inspector::Protocol::CSS::RuleMatch::create()
            .setRule(ruleObject.releaseNonNull())
            .setMatchingSelectors(WTF::move(matchingSelectors))
            .release();
        result->addItem(WTF::move(match));
    }

    return result;
}

InspectorStyleSheet& FrameCSSAgent::bindStyleSheet(CSSStyleSheet* styleSheet)
{
    auto it = m_cssStyleSheetToInspectorStyleSheet.find(styleSheet);
    if (it != m_cssStyleSheetToInspectorStyleSheet.end())
        return it->value;

    auto id = String::number(m_lastStyleSheetId++);
    RefPtr document = styleSheet->ownerDocument();
    Ref inspectorStyleSheet = InspectorStyleSheet::create(protect(m_inspectedFrame->page()->inspectorController().identifierRegistry()), id, styleSheet, detectOrigin(styleSheet, document.get()), InspectorDOMAgent::documentURLString(document.get()), this);
    m_idToInspectorStyleSheet.set(id, inspectorStyleSheet);
    if (m_creatingViaInspectorStyleSheet && document)
        m_documentToInspectorStyleSheet.add(document.releaseNonNull(), Vector<Ref<InspectorStyleSheet>>()).iterator->value.append(inspectorStyleSheet);
    return m_cssStyleSheetToInspectorStyleSheet.set(styleSheet, WTF::move(inspectorStyleSheet)).iterator->value;
}

InspectorStyleSheet* FrameCSSAgent::assertStyleSheetForId(Inspector::Protocol::ErrorString& errorString, const String& styleSheetId)
{
    auto it = m_idToInspectorStyleSheet.find(styleSheetId);
    if (it == m_idToInspectorStyleSheet.end()) {
        errorString = "Missing style sheet for given styleSheetId"_s;
        return nullptr;
    }
    return it->value.ptr();
}

void FrameCSSAgent::collectAllDocumentStyleSheets(Document& document, Vector<CSSStyleSheet*>& result)
{
    auto cssStyleSheets = document.styleScope().activeStyleSheetsForInspector();
    for (auto& cssStyleSheet : cssStyleSheets)
        collectStyleSheets(cssStyleSheet.ptr(), result);
}

void FrameCSSAgent::collectStyleSheets(CSSStyleSheet* styleSheet, Vector<CSSStyleSheet*>& result)
{
    result.append(styleSheet);

    for (unsigned i = 0, size = styleSheet->length(); i < size; ++i) {
        if (RefPtr rule = dynamicDowncast<CSSImportRule>(styleSheet->item(i))) {
            if (RefPtr importedStyleSheet = rule->styleSheet())
                collectStyleSheets(importedStyleSheet.get(), result);
        }
    }
}

void FrameCSSAgent::setActiveStyleSheetsForDocument(Document& document, Vector<CSSStyleSheet*>& activeStyleSheets)
{
    HashSet<CSSStyleSheet*>& previouslyKnownActiveStyleSheets = m_documentToKnownCSSStyleSheets.add(&document, HashSet<CSSStyleSheet*>()).iterator->value;

    HashSet<CSSStyleSheet*> removedStyleSheets(previouslyKnownActiveStyleSheets);
    Vector<CSSStyleSheet*> addedStyleSheets;
    for (auto& activeStyleSheet : activeStyleSheets) {
        if (removedStyleSheets.contains(activeStyleSheet))
            removedStyleSheets.remove(activeStyleSheet);
        else
            addedStyleSheets.append(activeStyleSheet);
    }

    for (RefPtr cssStyleSheet : removedStyleSheets) {
        previouslyKnownActiveStyleSheets.remove(cssStyleSheet.get());
        RefPtr<InspectorStyleSheet> inspectorStyleSheet = m_cssStyleSheetToInspectorStyleSheet.get(cssStyleSheet.get());
        if (inspectorStyleSheet && m_idToInspectorStyleSheet.contains(inspectorStyleSheet->id())) {
            auto id = inspectorStyleSheet->id();
            m_idToInspectorStyleSheet.remove(id);
            m_cssStyleSheetToInspectorStyleSheet.remove(cssStyleSheet.get());
            m_frontendDispatcher->styleSheetRemoved(id);
        }
    }

    for (RefPtr cssStyleSheet : addedStyleSheets) {
        previouslyKnownActiveStyleSheets.add(cssStyleSheet.get());
        if (!m_cssStyleSheetToInspectorStyleSheet.contains(cssStyleSheet.get())) {
            Ref inspectorStyleSheet = bindStyleSheet(cssStyleSheet.get());
            if (auto header = inspectorStyleSheet->buildObjectForStyleSheetInfo())
                m_frontendDispatcher->styleSheetAdded(header.releaseNonNull());
        }
    }
}

InspectorStyleSheet* FrameCSSAgent::createInspectorStyleSheetForDocument(Document& document)
{
    if (!document.isHTMLDocument() && !document.isSVGDocument())
        return nullptr;

    auto styleElement = HTMLStyleElement::create(document);
    styleElement->setAttributeWithoutSynchronization(HTMLNames::typeAttr, cssContentTypeAtom());

    ContainerNode* targetNode;
    if (auto* head = document.head())
        targetNode = head;
    else if (auto* body = document.bodyOrFrameset())
        targetNode = body;
    else
        return nullptr;

    m_creatingViaInspectorStyleSheet = true;
    document.contentSecurityPolicy()->setOverrideAllowInlineStyle(true);
    auto appendResult = targetNode->appendChild(styleElement);
    document.styleScope().flushPendingUpdate();
    document.contentSecurityPolicy()->setOverrideAllowInlineStyle(false);
    m_creatingViaInspectorStyleSheet = false;
    if (appendResult.hasException())
        return nullptr;

    auto iterator = m_documentToInspectorStyleSheet.find(document);
    ASSERT(iterator != m_documentToInspectorStyleSheet.end());
    if (iterator == m_documentToInspectorStyleSheet.end())
        return nullptr;

    auto& inspectorStyleSheetsForDocument = iterator->value;
    ASSERT(!inspectorStyleSheetsForDocument.isEmpty());
    if (inspectorStyleSheetsForDocument.isEmpty())
        return nullptr;

    return inspectorStyleSheetsForDocument.last().ptr();
}

Inspector::Protocol::CSS::StyleSheetOrigin FrameCSSAgent::detectOrigin(CSSStyleSheet* pageStyleSheet, Document* ownerDocument)
{
    if (m_creatingViaInspectorStyleSheet)
        return Inspector::Protocol::CSS::StyleSheetOrigin::Inspector;

    if (pageStyleSheet && !pageStyleSheet->ownerNode() && pageStyleSheet->href().isEmpty())
        return Inspector::Protocol::CSS::StyleSheetOrigin::UserAgent;

    if (pageStyleSheet && pageStyleSheet->contents().isUserStyleSheet())
        return Inspector::Protocol::CSS::StyleSheetOrigin::User;

    if (!ownerDocument)
        return Inspector::Protocol::CSS::StyleSheetOrigin::Author;

    auto iterator = m_documentToInspectorStyleSheet.find(*ownerDocument);
    if (iterator != m_documentToInspectorStyleSheet.end()) {
        for (auto& inspectorStyleSheet : iterator->value) {
            if (inspectorStyleSheet->pageStyleSheet() == pageStyleSheet)
                return Inspector::Protocol::CSS::StyleSheetOrigin::Inspector;
        }
    }

    return Inspector::Protocol::CSS::StyleSheetOrigin::Author;
}

} // namespace WebCore
