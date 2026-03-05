/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002-2022 Apple Inc. All rights reserved.
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
#include "StyleRule.h"

#include "CSSContainerRule.h"
#include "CSSCounterStyleRule.h"
#include "CSSFontFaceRule.h"
#include "CSSFontFeatureValuesRule.h"
#include "CSSFontPaletteValuesRule.h"
#include "CSSFunctionDeclarations.h"
#include "CSSFunctionRule.h"
#include "CSSGroupingRule.h"
#include "CSSImportRule.h"
#include "CSSKeyframeRule.h"
#include "CSSKeyframesRule.h"
#include "CSSLayerBlockRule.h"
#include "CSSLayerStatementRule.h"
#include "CSSMediaRule.h"
#include "CSSNamespaceRule.h"
#include "CSSNestedDeclarations.h"
#include "CSSPageRule.h"
#include "CSSPositionTryRule.h"
#include "CSSPropertyRule.h"
#include "CSSScopeRule.h"
#include "CSSSerializationContext.h"
#include "CSSStartingStyleRule.h"
#include "CSSStyleRule.h"
#include "CSSSupportsRule.h"
#include "CSSViewTransitionRule.h"
#include "MediaList.h"
#include "MutableStyleProperties.h"
#include "StyleProperties.h"
#include "StylePropertiesInlines.h"
#include "StyleRuleFunction.h"
#include "StyleRuleImport.h"
#include "StyleSheetContents.h"

namespace WebCore {

struct SameSizeAsStyleRuleBase : public WTF::RefCountedBase {
    unsigned bitfields : 5;
};

#if !PLATFORM(WIN)
static_assert(sizeof(StyleRuleBase) == sizeof(SameSizeAsStyleRuleBase), "StyleRuleBase should stay small");
#endif

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleRuleBase);
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleRule);
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleRuleWithNesting);
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleRuleNestedDeclarations);

Ref<CSSRule> StyleRuleBase::createCSSOMWrapper(CSSStyleSheet& parentSheet) const
{
    return createCSSOMWrapper(&parentSheet, nullptr);
}

Ref<CSSRule> StyleRuleBase::createCSSOMWrapper(CSSGroupingRule& parentRule) const
{
    return createCSSOMWrapper(nullptr, &parentRule);
}

Ref<CSSRule> StyleRuleBase::createCSSOMWrapper(CSSStyleRule& parentRule) const
{
    return createCSSOMWrapper(nullptr, &parentRule);
}

Ref<CSSRule> StyleRuleBase::createCSSOMWrapper() const
{
    return createCSSOMWrapper(nullptr, nullptr);
}

template<typename Visitor> constexpr decltype(auto) StyleRuleBase::visitDerived(Visitor&& visitor)
{
    switch (type()) {
    case StyleRuleType::Style:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRule>(*this));
    case StyleRuleType::StyleWithNesting:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleWithNesting>(*this));
    case StyleRuleType::NestedDeclarations:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleNestedDeclarations>(*this));
    case StyleRuleType::Page:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRulePage>(*this));
    case StyleRuleType::FontFace:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleFontFace>(*this));
    case StyleRuleType::FontFeatureValues:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleFontFeatureValues>(*this));
    case StyleRuleType::FontFeatureValuesBlock:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleFontFeatureValuesBlock>(*this));
    case StyleRuleType::FontPaletteValues:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleFontPaletteValues>(*this));
    case StyleRuleType::Media:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleMedia>(*this));
    case StyleRuleType::Supports:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleSupports>(*this));
    case StyleRuleType::Import:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleImport>(*this));
    case StyleRuleType::Keyframes:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleKeyframes>(*this));
    case StyleRuleType::Namespace:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleNamespace>(*this));
    case StyleRuleType::Keyframe:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleKeyframe>(*this));
    case StyleRuleType::Charset:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleCharset>(*this));
    case StyleRuleType::CounterStyle:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleCounterStyle>(*this));
    case StyleRuleType::LayerBlock:
    case StyleRuleType::LayerStatement:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleLayer>(*this));
    case StyleRuleType::Container:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleContainer>(*this));
    case StyleRuleType::Property:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleProperty>(*this));
    case StyleRuleType::Scope:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleScope>(*this));
    case StyleRuleType::StartingStyle:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleStartingStyle>(*this));
    case StyleRuleType::ViewTransition:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleViewTransition>(*this));
    case StyleRuleType::PositionTry:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRulePositionTry>(*this));
    case StyleRuleType::Function:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleFunction>(*this));
    case StyleRuleType::FunctionDeclarations:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<StyleRuleFunctionDeclarations>(*this));
    case StyleRuleType::Margin:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

template<typename Visitor> constexpr decltype(auto) StyleRuleBase::visitDerived(Visitor&& visitor) const
{
    return const_cast<StyleRuleBase&>(*this).visitDerived([&](auto& value) {
        return std::invoke(std::forward<Visitor>(visitor), std::as_const(value));
    });
}

void StyleRuleBase::operator delete(StyleRuleBase* rule, std::destroying_delete_t)
{
    rule->visitDerived([]<typename RuleType> (RuleType& rule) {
        std::destroy_at(&rule);
        RuleType::freeAfterDestruction(&rule);
    });
}

Ref<StyleRuleBase> StyleRuleBase::copy() const
{
    return visitDerived([]<typename RuleType> (RuleType& rule) -> Ref<StyleRuleBase> {
        // Check at compile time for a mistake where this function would call itself, leading to infinite recursion.
        // We can do this with the types of pointers to member functions because they includes the type of the class.
        static_assert(!std::is_same_v<decltype(&RuleType::copy), decltype(&StyleRuleBase::copy)>);
        return rule.copy();
    });
}

Ref<CSSRule> StyleRuleBase::createCSSOMWrapper(CSSStyleSheet* parentSheet, CSSRule* parentRule) const
{
    // FIXME: const_cast is required here because a wrapper for a style rule can be used to *modify* the style rule's selector; use of const in the style system is thus inaccurate.
    auto wrapper = const_cast<StyleRuleBase&>(*this).visitDerived(WTF::makeVisitor(
        [&](StyleRule& rule) -> Ref<CSSRule> {
            return CSSStyleRule::create(rule, parentSheet);
        },
        [&](StyleRuleWithNesting& rule) -> Ref<CSSRule> {
            return CSSStyleRule::create(rule, parentSheet);
        },
        [&](StyleRuleNestedDeclarations& rule) -> Ref<CSSRule> {
            return CSSNestedDeclarations::create(rule, parentSheet);
        },
        [&](StyleRulePage& rule) -> Ref<CSSRule> {
            return CSSPageRule::create(rule, parentSheet);
        },
        [&](StyleRuleFontFace& rule) -> Ref<CSSRule> {
            return CSSFontFaceRule::create(rule, parentSheet);
        },
        [&](StyleRuleFontFeatureValues& rule) -> Ref<CSSRule> {
            return CSSFontFeatureValuesRule::create(rule, parentSheet);
        },
        [&](StyleRuleFontFeatureValuesBlock& rule) -> Ref<CSSRule> {
            return CSSFontFeatureValuesBlockRule::create(rule, parentSheet);
        },
        [&](StyleRuleFontPaletteValues& rule) -> Ref<CSSRule> {
            return CSSFontPaletteValuesRule::create(rule, parentSheet);
        },
        [&](StyleRuleMedia& rule) -> Ref<CSSRule> {
            return CSSMediaRule::create(rule, parentSheet);
        },
        [&](StyleRuleSupports& rule) -> Ref<CSSRule> {
            return CSSSupportsRule::create(rule, parentSheet);
        },
        [&](StyleRuleImport& rule) -> Ref<CSSRule> {
            return CSSImportRule::create(rule, parentSheet);
        },
        [&](StyleRuleKeyframes& rule) -> Ref<CSSRule> {
            return CSSKeyframesRule::create(rule, parentSheet);
        },
        [&](StyleRuleNamespace& rule) -> Ref<CSSRule> {
            return CSSNamespaceRule::create(rule, parentSheet);
        },
        [&](StyleRuleCounterStyle& rule) -> Ref<CSSRule> {
            return CSSCounterStyleRule::create(rule, parentSheet);
        },
        [&](StyleRuleLayer& rule) -> Ref<CSSRule> {
            if (rule.isStatement())
                return CSSLayerStatementRule::create(rule, parentSheet);
            return CSSLayerBlockRule::create(rule, parentSheet);
        },
        [&](StyleRuleContainer& rule) -> Ref<CSSRule> {
            return CSSContainerRule::create(rule, parentSheet);
        },
        [&](StyleRuleProperty& rule) -> Ref<CSSRule> {
            return CSSPropertyRule::create(rule, parentSheet);
        },
        [&](StyleRuleScope& rule) -> Ref<CSSRule> {
            return CSSScopeRule::create(rule, parentSheet);
        },
        [&](StyleRuleStartingStyle& rule) -> Ref<CSSRule> {
            return CSSStartingStyleRule::create(rule, parentSheet);
        },
        [&](StyleRuleViewTransition& rule) -> Ref<CSSRule> {
            return CSSViewTransitionRule::create(rule, parentSheet);
        },
        [&](StyleRulePositionTry& rule) -> Ref<CSSRule> {
            return CSSPositionTryRule::create(rule, parentSheet);
        },
        [&](StyleRuleFunction& rule) -> Ref<CSSRule> {
            return CSSFunctionRule::create(rule, parentSheet);
        },
        [&](StyleRuleFunctionDeclarations& rule) -> Ref<CSSRule> {
            return CSSFunctionDeclarations::create(rule, parentSheet);
        },
        [](StyleRuleCharset&) -> Ref<CSSRule> {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [](StyleRuleKeyframe&) -> Ref<CSSRule> {
            RELEASE_ASSERT_NOT_REACHED();
        }
    ));
    if (parentRule)
        wrapper->setParentRule(parentRule);
    return wrapper;
}

unsigned StyleRule::averageSizeInBytes()
{
    return sizeof(StyleRule) + sizeof(CSSSelector) + StyleProperties::averageSizeInBytes() + sizeof(Vector<Ref<StyleRuleBase>>);
}

StyleRule::StyleRule(Ref<StyleProperties>&& properties, bool hasDocumentSecurityOrigin, CSSSelectorList&& selectors)
    : StyleRuleBase(StyleRuleType::Style, hasDocumentSecurityOrigin)
    , m_properties(WTF::move(properties))
    , m_selectorList(WTF::move(selectors))
{
}

StyleRule::StyleRule(const StyleRule& o)
    : StyleRuleBase(o)
    , m_isSplitRule(o.m_isSplitRule)
    , m_isLastRuleInSplitRule(o.m_isLastRuleInSplitRule)
    , m_properties(o.properties().mutableCopy())
    , m_selectorList(o.m_selectorList)
{
}

StyleRule::~StyleRule() = default;

Ref<StyleRule> StyleRule::create(Ref<StyleProperties>&& properties, bool hasDocumentSecurityOrigin, CSSSelectorList&& selectors)
{
    return adoptRef(*new StyleRule(WTF::move(properties), hasDocumentSecurityOrigin, WTF::move(selectors)));
}

Ref<StyleRule> StyleRule::copy() const
{
    return adoptRef(*new StyleRule(*this));
}

void StyleRule::setProperties(Ref<StyleProperties>&& properties)
{
    m_properties = WTF::move(properties);
}

MutableStyleProperties& StyleRule::mutableProperties()
{
    if (!is<MutableStyleProperties>(m_properties))
        m_properties = m_properties->mutableCopy();
    return uncheckedDowncast<MutableStyleProperties>(m_properties.get());
}

void StyleRule::wrapperAdoptSelectorList(CSSSelectorList&& selectors)
{
    adoptSelectorList(WTF::move(selectors));
}

void StyleRuleWithNesting::wrapperAdoptOriginalSelectorList(CSSSelectorList&& selectors)
{
    m_originalSelectorList = WTF::move(selectors);
    invalidateResolvedSelectorListRecursively();
}

Ref<StyleRule> StyleRule::createForSplitting(const Vector<const CSSSelector*>& selectors, Ref<StyleProperties>&& properties, bool hasDocumentSecurityOrigin)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!selectors.isEmpty());
    Ref styleRule = StyleRule::create(WTF::move(properties), hasDocumentSecurityOrigin, CSSSelectorList { selectors });
    styleRule->markAsSplitRule();
    return styleRule;
}

Vector<Ref<StyleRule>> StyleRule::splitIntoMultipleRulesWithMaximumSelectorComponentCount(unsigned maxCount) const
{
    ASSERT(selectorList().componentCount() > maxCount);

    Vector<Ref<StyleRule>> rules;
    Vector<const CSSSelector*> componentsSinceLastSplit;

    for (auto& selector : selectorList()) {
        Vector<const CSSSelector*, 8> componentsInThisSelector;
        for (const CSSSelector* component = &selector; component; component = component->precedingInComplexSelector())
            componentsInThisSelector.append(component);

        if (componentsInThisSelector.size() + componentsSinceLastSplit.size() > maxCount && !componentsSinceLastSplit.isEmpty()) {
            rules.append(createForSplitting(componentsSinceLastSplit, const_cast<StyleProperties&>(properties()), hasDocumentSecurityOrigin()));
            componentsSinceLastSplit.clear();
        }

        componentsSinceLastSplit.appendVector(componentsInThisSelector);
    }

    if (!componentsSinceLastSplit.isEmpty())
        rules.append(createForSplitting(componentsSinceLastSplit, const_cast<StyleProperties&>(properties()), hasDocumentSecurityOrigin()));

    if (!rules.isEmpty())
        rules.last()->markAsLastRuleInSplitRule();

    return rules;
}

String StyleRule::debugDescription() const
{
    return makeString(" StyleRule ["_s, " selector: "_s, selectorList().selectorsText(), " properties: "_s, m_properties->asText(CSS::defaultSerializationContext()), ']');
}

StyleRuleWithNesting::~StyleRuleWithNesting() = default;

Ref<StyleRuleWithNesting> StyleRuleWithNesting::copy() const
{
    return adoptRef(*new StyleRuleWithNesting(*this));
}

String StyleRuleWithNesting::debugDescription() const
{
    StringBuilder builder;
    builder.append(" StyleRuleWithNesting ["_s, "originalSelector: "_s, originalSelectorList().selectorsText(), StyleRule::debugDescription());
    for (const auto& rule : m_nestedRules)
        builder.append(rule->debugDescription());
    builder.append(']');
    return builder.toString();
}

StyleRuleWithNesting::StyleRuleWithNesting(const StyleRuleWithNesting& other)
    : StyleRule(other)
    , m_nestedRules(other.m_nestedRules.map([](auto& rule) { return rule->copy(); }))
    , m_originalSelectorList(other.m_originalSelectorList)
{
}

StyleRuleWithNesting::StyleRuleWithNesting(StyleRule&& styleRule)
    : StyleRule(WTF::move(styleRule))
    , m_nestedRules({ })
    , m_originalSelectorList(selectorList())
{
    setType(StyleRuleType::StyleWithNesting);
}

Ref<StyleRuleWithNesting> StyleRuleWithNesting::create(Ref<StyleProperties>&& properties, bool hasDocumentSecurityOrigin, CSSSelectorList&& selectors, Vector<Ref<StyleRuleBase>>&& nestedRules)
{
    return adoptRef(*new StyleRuleWithNesting(WTF::move(properties), hasDocumentSecurityOrigin, WTF::move(selectors), WTF::move(nestedRules)));
}

Ref<StyleRuleWithNesting> StyleRuleWithNesting::create(StyleRule&& styleRule)
{
    return adoptRef(*new StyleRuleWithNesting(WTF::move(styleRule)));
}

StyleRuleWithNesting::StyleRuleWithNesting(Ref<StyleProperties>&& properties, bool hasDocumentSecurityOrigin, CSSSelectorList&& selectors, Vector<Ref<StyleRuleBase>>&& nestedRules)
    // Actual selectors will be resolved later, at RuleSetBuilder time.
    : StyleRule(WTF::move(properties), hasDocumentSecurityOrigin, { })
    , m_nestedRules(WTF::move(nestedRules))
    , m_originalSelectorList(WTF::move(selectors))
{

    setType(StyleRuleType::StyleWithNesting);
}

StyleRulePage::StyleRulePage(Ref<StyleProperties>&& properties, CSSSelectorList&& selectors)
    : StyleRuleBase(StyleRuleType::Page)
    , m_properties(WTF::move(properties))
    , m_selectorList(WTF::move(selectors))
{
}

StyleRulePage::StyleRulePage(const StyleRulePage& o)
    : StyleRuleBase(o)
    , m_properties(o.m_properties->mutableCopy())
    , m_selectorList(o.m_selectorList)
{
}

StyleRuleNestedDeclarations::StyleRuleNestedDeclarations(Ref<StyleProperties>&& properties)
    : StyleRule(WTF::move(properties), false, { })
{
    setType(StyleRuleType::NestedDeclarations);
}

String StyleRuleNestedDeclarations::debugDescription() const
{
    return makeString("StyleRuleNestedDeclarations ["_s, properties().asText(CSS::defaultSerializationContext()), ']');
}

StyleRulePage::~StyleRulePage() = default;

Ref<StyleRulePage> StyleRulePage::create(Ref<StyleProperties>&& properties, CSSSelectorList&& selectors)
{
    return adoptRef(*new StyleRulePage(WTF::move(properties), WTF::move(selectors)));
}

MutableStyleProperties& StyleRulePage::mutableProperties()
{
    if (!is<MutableStyleProperties>(m_properties))
        m_properties = m_properties->mutableCopy();
    return uncheckedDowncast<MutableStyleProperties>(m_properties.get());
}

StyleRuleFontFace::StyleRuleFontFace(Ref<StyleProperties>&& properties)
    : StyleRuleBase(StyleRuleType::FontFace)
    , m_properties(WTF::move(properties))
{
}

StyleRuleFontFace::StyleRuleFontFace(const StyleRuleFontFace& o)
    : StyleRuleBase(o)
    , m_properties(o.m_properties->mutableCopy())
{
}

StyleRuleFontFace::~StyleRuleFontFace() = default;

MutableStyleProperties& StyleRuleFontFace::mutableProperties()
{
    if (!is<MutableStyleProperties>(m_properties))
        m_properties = m_properties->mutableCopy();
    return uncheckedDowncast<MutableStyleProperties>(m_properties.get());
}

StyleRuleFontFeatureValues::StyleRuleFontFeatureValues(const Vector<AtomString>& fontFamilies, Ref<FontFeatureValues>&& value)
    : StyleRuleBase(StyleRuleType::FontFeatureValues)
    , m_fontFamilies(fontFamilies)
    , m_value(WTF::move(value))
{
}

StyleRuleFontFeatureValuesBlock::StyleRuleFontFeatureValuesBlock(FontFeatureValuesType type, const Vector<FontFeatureValuesTag>& tags)
    : StyleRuleBase(StyleRuleType::FontFeatureValuesBlock)
    , m_type(type)
    , m_tags(tags)
{
}

Ref<StyleRuleFontFeatureValues> StyleRuleFontFeatureValues::create(const Vector<AtomString>& fontFamilies, Ref<FontFeatureValues>&& values)
{
    return adoptRef(*new StyleRuleFontFeatureValues(fontFamilies, WTF::move(values)));
}

Ref<StyleRuleFontPaletteValues> StyleRuleFontPaletteValues::create(const AtomString& name, Vector<AtomString>&& fontFamilies, std::optional<FontPaletteIndex> basePalette, Vector<FontPaletteValues::OverriddenColor>&& overrideColors)
{
    return adoptRef(*new StyleRuleFontPaletteValues(name, WTF::move(fontFamilies), basePalette, WTF::move(overrideColors)));
}

StyleRuleFontPaletteValues::StyleRuleFontPaletteValues(const AtomString& name, Vector<AtomString>&& fontFamilies, std::optional<FontPaletteIndex> basePalette, Vector<FontPaletteValues::OverriddenColor>&& overrideColors)
    : StyleRuleBase(StyleRuleType::FontPaletteValues)
    , m_name(name)
    , m_fontFamilies(WTF::move(fontFamilies))
    , m_fontPaletteValues(basePalette, WTF::move(overrideColors))
{
}

StyleRuleGroup::StyleRuleGroup(StyleRuleType type, Vector<Ref<StyleRuleBase>>&& rules)
    : StyleRuleBase(type)
    , m_childRules(WTF::move(rules))
{
}

StyleRuleGroup::StyleRuleGroup(const StyleRuleGroup& other)
    : StyleRuleBase(other)
    , m_childRules(other.childRules().map([](auto& rule) -> Ref<StyleRuleBase> { return rule->copy(); }))
{
}

const Vector<Ref<StyleRuleBase>>& StyleRuleGroup::childRules() const
{
    return m_childRules;
}

void StyleRuleGroup::wrapperInsertRule(unsigned index, Ref<StyleRuleBase>&& rule)
{
    m_childRules.insert(index, WTF::move(rule));
}

void StyleRuleGroup::wrapperRemoveRule(unsigned index)
{
    m_childRules.removeAt(index);
}

String StyleRuleGroup::debugDescription() const
{
    StringBuilder builder;
    builder.append("StyleRuleGroup ["_s);
    for (const auto& rule : m_childRules)
        builder.append(rule->debugDescription());
    builder.append(']');
    return builder.toString();
}

StyleRuleMedia::StyleRuleMedia(MQ::MediaQueryList&& mediaQueries, Vector<Ref<StyleRuleBase>>&& rules)
    : StyleRuleGroup(StyleRuleType::Media, WTF::move(rules))
    , m_mediaQueries(WTF::move(mediaQueries))
{
}

StyleRuleMedia::StyleRuleMedia(const StyleRuleMedia& other)
    : StyleRuleGroup(other)
    , m_mediaQueries(other.m_mediaQueries)
{
}

Ref<StyleRuleMedia> StyleRuleMedia::create(MQ::MediaQueryList&& mediaQueries, Vector<Ref<StyleRuleBase>>&& rules)
{
    return adoptRef(*new StyleRuleMedia(WTF::move(mediaQueries), WTF::move(rules)));
}

Ref<StyleRuleMedia> StyleRuleMedia::copy() const
{
    return adoptRef(*new StyleRuleMedia(*this));
}

String StyleRuleMedia::debugDescription() const
{
    StringBuilder builder;
    builder.append("StyleRuleMedia ["_s, StyleRuleGroup::debugDescription(), ']');
    return builder.toString();
}

StyleRuleSupports::StyleRuleSupports(const String& conditionText, bool conditionIsSupported, Vector<Ref<StyleRuleBase>>&& rules)
    : StyleRuleGroup(StyleRuleType::Supports, WTF::move(rules))
    , m_conditionText(conditionText)
    , m_conditionIsSupported(conditionIsSupported)
{
}

Ref<StyleRuleSupports> StyleRuleSupports::create(const String& conditionText, bool conditionIsSupported, Vector<Ref<StyleRuleBase>>&& rules)
{
    return adoptRef(*new StyleRuleSupports(conditionText, conditionIsSupported, WTF::move(rules)));
}

StyleRuleLayer::StyleRuleLayer(Vector<CascadeLayerName>&& nameList)
    : StyleRuleGroup(StyleRuleType::LayerStatement, Vector<Ref<StyleRuleBase>> { })
    , m_nameVariant(WTF::move(nameList))
{
}

StyleRuleLayer::StyleRuleLayer(CascadeLayerName&& name, Vector<Ref<StyleRuleBase>>&& rules)
    : StyleRuleGroup(StyleRuleType::LayerBlock, WTF::move(rules))
    , m_nameVariant(WTF::move(name))
{
}

Ref<StyleRuleLayer> StyleRuleLayer::createStatement(Vector<CascadeLayerName>&& nameList)
{
    return adoptRef(*new StyleRuleLayer(WTF::move(nameList)));
}

Ref<StyleRuleLayer> StyleRuleLayer::createBlock(CascadeLayerName&& name, Vector<Ref<StyleRuleBase>>&& rules)
{
    return adoptRef(*new StyleRuleLayer(WTF::move(name), WTF::move(rules)));
}

StyleRuleContainer::StyleRuleContainer(CQ::ContainerQuery&& query, Vector<Ref<StyleRuleBase>>&& rules)
    : StyleRuleGroup(StyleRuleType::Container, WTF::move(rules))
    , m_containerQuery(WTF::move(query))
{
}

Ref<StyleRuleContainer> StyleRuleContainer::create(CQ::ContainerQuery&& query, Vector<Ref<StyleRuleBase>>&& rules)
{
    return adoptRef(*new StyleRuleContainer(WTF::move(query), WTF::move(rules)));
}

StyleRuleProperty::StyleRuleProperty(Descriptor&& descriptor)
    : StyleRuleBase(StyleRuleType::Property)
    , m_descriptor(WTF::move(descriptor))
{
}

Ref<StyleRuleProperty> StyleRuleProperty::create(Descriptor&& descriptor)
{
    return adoptRef(*new StyleRuleProperty(WTF::move(descriptor)));
}

Ref<StyleRuleScope> StyleRuleScope::create(CSSSelectorList&& scopeStart, CSSSelectorList&& scopeEnd, Vector<Ref<StyleRuleBase>>&& rules)
{
    return adoptRef(*new StyleRuleScope(WTF::move(scopeStart), WTF::move(scopeEnd), WTF::move(rules)));
}

StyleRuleScope::~StyleRuleScope() = default;

Ref<StyleRuleScope> StyleRuleScope::copy() const
{
    return adoptRef(*new StyleRuleScope(*this));
}

StyleRuleScope::StyleRuleScope(CSSSelectorList&& scopeStart, CSSSelectorList&& scopeEnd, Vector<Ref<StyleRuleBase>>&& rules)
    : StyleRuleGroup(StyleRuleType::Scope, WTF::move(rules))
    , m_originalScopeStart(WTF::move(scopeStart))
    , m_originalScopeEnd(WTF::move(scopeEnd))
{
}

StyleRuleScope::StyleRuleScope(const StyleRuleScope&) = default;

WeakPtr<const StyleSheetContents> StyleRuleScope::styleSheetContents() const
{
    return m_styleSheetOwner;
}

void StyleRuleScope::setStyleSheetContents(const StyleSheetContents& sheet)
{
    m_styleSheetOwner = sheet;
}

Ref<StyleRuleStartingStyle> StyleRuleStartingStyle::create(Vector<Ref<StyleRuleBase>>&& rules)
{
    return adoptRef(*new StyleRuleStartingStyle(WTF::move(rules)));
}

StyleRuleStartingStyle::StyleRuleStartingStyle(Vector<Ref<StyleRuleBase>>&& rules)
    : StyleRuleGroup(StyleRuleType::StartingStyle, WTF::move(rules))
{
}

StyleRuleCharset::StyleRuleCharset()
    : StyleRuleBase(StyleRuleType::Charset)
{
}

StyleRuleNamespace::StyleRuleNamespace(const AtomString& prefix, const AtomString& uri)
    : StyleRuleBase(StyleRuleType::Namespace)
    , m_prefix(prefix)
    , m_uri(uri)
{
}

Ref<StyleRuleNamespace> StyleRuleNamespace::create(const AtomString& prefix, const AtomString& uri)
{
    return adoptRef(*new StyleRuleNamespace(prefix, uri));
}

void StyleRuleBase::invalidateResolvedSelectorListRecursively()
{
    visitDerived(WTF::makeVisitor(
        [](StyleRuleWithNesting& rule) {
            rule.adoptSelectorList({ });
            for (auto& child : rule.nestedRules())
                child->invalidateResolvedSelectorListRecursively();
        },
        [&](StyleRuleNestedDeclarations& rule) {
            rule.adoptSelectorList({ });
        },
        [&](StyleRuleScope& rule) {
            rule.setScopeStart({ });
            rule.setScopeEnd({ });
            for (auto& child : rule.childRules())
                child->invalidateResolvedSelectorListRecursively();
        },
        [&](const auto& rule) {
            using ItemType = std::decay_t<decltype(rule)>;
            if constexpr (std::is_base_of_v<StyleRuleGroup, ItemType>) {
                for (auto& child : rule.childRules())
                    child->invalidateResolvedSelectorListRecursively();
            }
        }
        ));
}

String StyleRuleBase::debugDescription() const
{
    return visitDerived([]<typename RuleType> (const RuleType& rule) -> String {
        // FIXME: implement debugDescription() for all classes which inherit StyleRuleBase.
        if constexpr (std::is_same_v<decltype(&RuleType::debugDescription), decltype(&StyleRuleBase::debugDescription)>)
            return "StyleRuleBase"_s;
        return rule.debugDescription();
    });
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const StyleRuleBase& rule)
{
    ts << rule.debugDescription();
    return ts;
}

} // namespace WebCore
