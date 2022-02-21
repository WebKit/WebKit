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
#include "StyleRule.h"

#include "CSSCounterStyleRule.h"
#include "CSSDeferredParser.h"
#include "CSSFontFaceRule.h"
#include "CSSFontPaletteValuesRule.h"
#include "CSSImportRule.h"
#include "CSSKeyframeRule.h"
#include "CSSKeyframesRule.h"
#include "CSSLayerBlockRule.h"
#include "CSSLayerStatementRule.h"
#include "CSSMediaRule.h"
#include "CSSNamespaceRule.h"
#include "CSSPageRule.h"
#include "CSSStyleRule.h"
#include "CSSSupportsRule.h"
#include "MediaList.h"
#include "StyleProperties.h"
#include "StyleRuleImport.h"

namespace WebCore {

struct SameSizeAsStyleRuleBase : public WTF::RefCountedBase {
    unsigned bitfields : 5;
};

COMPILE_ASSERT(sizeof(StyleRuleBase) == sizeof(SameSizeAsStyleRuleBase), StyleRuleBase_should_stay_small);

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleRuleBase);
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleRule);

Ref<CSSRule> StyleRuleBase::createCSSOMWrapper(CSSStyleSheet* parentSheet) const
{
    return createCSSOMWrapper(parentSheet, nullptr);
}

Ref<CSSRule> StyleRuleBase::createCSSOMWrapper(CSSRule* parentRule) const
{ 
    return createCSSOMWrapper(nullptr, parentRule);
}

void StyleRuleBase::destroy()
{
    switch (type()) {
    case StyleRuleType::Style:
        delete downcast<StyleRule>(this);
        return;
    case StyleRuleType::Page:
        delete downcast<StyleRulePage>(this);
        return;
    case StyleRuleType::FontFace:
        delete downcast<StyleRuleFontFace>(this);
        return;
    case StyleRuleType::FontPaletteValues:
        delete downcast<StyleRuleFontPaletteValues>(this);
        return;
    case StyleRuleType::Media:
        delete downcast<StyleRuleMedia>(this);
        return;
    case StyleRuleType::Supports:
        delete downcast<StyleRuleSupports>(this);
        return;
    case StyleRuleType::Import:
        delete downcast<StyleRuleImport>(this);
        return;
    case StyleRuleType::Keyframes:
        delete downcast<StyleRuleKeyframes>(this);
        return;
    case StyleRuleType::Namespace:
        delete downcast<StyleRuleNamespace>(this);
        return;
    case StyleRuleType::Keyframe:
        delete downcast<StyleRuleKeyframe>(this);
        return;
    case StyleRuleType::Charset:
        delete downcast<StyleRuleCharset>(this);
        return;
    case StyleRuleType::CounterStyle:
        delete downcast<StyleRuleCounterStyle>(this);
        return;
    case StyleRuleType::LayerBlock:
    case StyleRuleType::LayerStatement:
        delete downcast<StyleRuleLayer>(this);
        return;
    case StyleRuleType::Container:
        delete downcast<StyleRuleContainer>(this);
        return;
    case StyleRuleType::Margin:
    case StyleRuleType::Unknown:
        ASSERT_NOT_REACHED();
        return;
    }
    ASSERT_NOT_REACHED();
}

Ref<StyleRuleBase> StyleRuleBase::copy() const
{
    switch (type()) {
    case StyleRuleType::Style:
        return downcast<StyleRule>(*this).copy();
    case StyleRuleType::Page:
        return downcast<StyleRulePage>(*this).copy();
    case StyleRuleType::FontFace:
        return downcast<StyleRuleFontFace>(*this).copy();
    case StyleRuleType::FontPaletteValues:
        return downcast<StyleRuleFontPaletteValues>(*this).copy();
    case StyleRuleType::Media:
        return downcast<StyleRuleMedia>(*this).copy();
    case StyleRuleType::Supports:
        return downcast<StyleRuleSupports>(*this).copy();
    case StyleRuleType::Keyframes:
        return downcast<StyleRuleKeyframes>(*this).copy();
    case StyleRuleType::CounterStyle:
        return downcast<StyleRuleCounterStyle>(*this).copy();
    case StyleRuleType::LayerBlock:
    case StyleRuleType::LayerStatement:
        return downcast<StyleRuleLayer>(*this).copy();
    case StyleRuleType::Container:
        return downcast<StyleRuleContainer>(*this).copy();
    case StyleRuleType::Import:
    case StyleRuleType::Namespace:
        // FIXME: Copy import and namespace rules.
        break;
    case StyleRuleType::Unknown:
    case StyleRuleType::Charset:
    case StyleRuleType::Keyframe:
    case StyleRuleType::Margin:
        break;
    }
    CRASH();
}

Ref<CSSRule> StyleRuleBase::createCSSOMWrapper(CSSStyleSheet* parentSheet, CSSRule* parentRule) const
{
    RefPtr<CSSRule> rule;
    StyleRuleBase& self = const_cast<StyleRuleBase&>(*this);
    switch (type()) {
    case StyleRuleType::Style:
        rule = CSSStyleRule::create(downcast<StyleRule>(self), parentSheet);
        break;
    case StyleRuleType::Page:
        rule = CSSPageRule::create(downcast<StyleRulePage>(self), parentSheet);
        break;
    case StyleRuleType::FontFace:
        rule = CSSFontFaceRule::create(downcast<StyleRuleFontFace>(self), parentSheet);
        break;
    case StyleRuleType::FontPaletteValues:
        rule = CSSFontPaletteValuesRule::create(downcast<StyleRuleFontPaletteValues>(self), parentSheet);
        break;
    case StyleRuleType::Media:
        rule = CSSMediaRule::create(downcast<StyleRuleMedia>(self), parentSheet);
        break;
    case StyleRuleType::Supports:
        rule = CSSSupportsRule::create(downcast<StyleRuleSupports>(self), parentSheet);
        break;
    case StyleRuleType::Import:
        rule = CSSImportRule::create(downcast<StyleRuleImport>(self), parentSheet);
        break;
    case StyleRuleType::Keyframes:
        rule = CSSKeyframesRule::create(downcast<StyleRuleKeyframes>(self), parentSheet);
        break;
    case StyleRuleType::Namespace:
        rule = CSSNamespaceRule::create(downcast<StyleRuleNamespace>(self), parentSheet);
        break;
    case StyleRuleType::CounterStyle:
        rule = CSSCounterStyleRule::create(downcast<StyleRuleCounterStyle>(self), parentSheet);
        break;
    case StyleRuleType::LayerBlock:
        rule = CSSLayerBlockRule::create(downcast<StyleRuleLayer>(self), parentSheet);
        break;
    case StyleRuleType::LayerStatement:
        rule = CSSLayerStatementRule::create(downcast<StyleRuleLayer>(self), parentSheet);
        break;
    case StyleRuleType::Container:
        // FIXME: Implement CSSOM.
        break;
    case StyleRuleType::Unknown:
    case StyleRuleType::Charset:
    case StyleRuleType::Keyframe:
    case StyleRuleType::Margin:
        ASSERT_NOT_REACHED();
        break;
    }
    ASSERT(rule);

    if (parentRule)
        rule->setParentRule(parentRule);
    return rule.releaseNonNull();
}

unsigned StyleRule::averageSizeInBytes()
{
    return sizeof(StyleRule) + sizeof(CSSSelector) + StyleProperties::averageSizeInBytes();
}

StyleRule::StyleRule(Ref<StylePropertiesBase>&& properties, bool hasDocumentSecurityOrigin, CSSSelectorList&& selectors)
    : StyleRuleBase(StyleRuleType::Style, hasDocumentSecurityOrigin)
    , m_properties(WTFMove(properties))
    , m_selectorList(WTFMove(selectors))
{
}

StyleRule::StyleRule(const StyleRule& o)
    : StyleRuleBase(o)
    , m_properties(o.properties().mutableCopy())
    , m_selectorList(o.m_selectorList)
    , m_isSplitRule(o.m_isSplitRule)
    , m_isLastRuleInSplitRule(o.m_isLastRuleInSplitRule)
{
}

StyleRule::~StyleRule() = default;

Ref<StyleRule> StyleRule::create(Ref<StylePropertiesBase>&& properties, bool hasDocumentSecurityOrigin, CSSSelectorList&& selectors)
{
    return adoptRef(*new StyleRule(WTFMove(properties), hasDocumentSecurityOrigin, WTFMove(selectors)));
}

Ref<StyleRule> StyleRule::copy() const
{
    return adoptRef(*new StyleRule(*this));
}

const StyleProperties& StyleRule::properties() const
{
    if (m_properties->type() == DeferredPropertiesType)
        m_properties = downcast<DeferredStyleProperties>(m_properties.get()).parseDeferredProperties();
    return downcast<StyleProperties>(m_properties.get());
}

MutableStyleProperties& StyleRule::mutableProperties()
{
    if (!is<MutableStyleProperties>(m_properties))
        m_properties = properties().mutableCopy();
    return downcast<MutableStyleProperties>(m_properties.get());
}

Ref<StyleRule> StyleRule::createForSplitting(const Vector<const CSSSelector*>& selectors, Ref<StyleProperties>&& properties, bool hasDocumentSecurityOrigin)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!selectors.isEmpty());
    auto selectorListArray = makeUniqueArray<CSSSelector>(selectors.size());
    for (unsigned i = 0; i < selectors.size(); ++i)
        new (NotNull, &selectorListArray[i]) CSSSelector(*selectors.at(i));
    selectorListArray[selectors.size() - 1].setLastInSelectorList();
    auto styleRule = StyleRule::create(WTFMove(properties), hasDocumentSecurityOrigin, CSSSelectorList(WTFMove(selectorListArray)));
    styleRule->markAsSplitRule();
    return styleRule;
}

Vector<RefPtr<StyleRule>> StyleRule::splitIntoMultipleRulesWithMaximumSelectorComponentCount(unsigned maxCount) const
{
    ASSERT(selectorList().componentCount() > maxCount);

    Vector<RefPtr<StyleRule>> rules;
    Vector<const CSSSelector*> componentsSinceLastSplit;

    for (const CSSSelector* selector = selectorList().first(); selector; selector = CSSSelectorList::next(selector)) {
        Vector<const CSSSelector*, 8> componentsInThisSelector;
        for (const CSSSelector* component = selector; component; component = component->tagHistory())
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

StyleRulePage::StyleRulePage(Ref<StyleProperties>&& properties, CSSSelectorList&& selectors)
    : StyleRuleBase(StyleRuleType::Page)
    , m_properties(WTFMove(properties))
    , m_selectorList(WTFMove(selectors))
{
}

StyleRulePage::StyleRulePage(const StyleRulePage& o)
    : StyleRuleBase(o)
    , m_properties(o.m_properties->mutableCopy())
    , m_selectorList(o.m_selectorList)
{
}

StyleRulePage::~StyleRulePage() = default;

Ref<StyleRulePage> StyleRulePage::create(Ref<StyleProperties>&& properties, CSSSelectorList&& selectors)
{
    return adoptRef(*new StyleRulePage(WTFMove(properties), WTFMove(selectors)));
}

MutableStyleProperties& StyleRulePage::mutableProperties()
{
    if (!is<MutableStyleProperties>(m_properties))
        m_properties = m_properties->mutableCopy();
    return downcast<MutableStyleProperties>(m_properties.get());
}

StyleRuleFontFace::StyleRuleFontFace(Ref<StyleProperties>&& properties)
    : StyleRuleBase(StyleRuleType::FontFace)
    , m_properties(WTFMove(properties))
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
    return downcast<MutableStyleProperties>(m_properties.get());
}

StyleRuleFontPaletteValues::StyleRuleFontPaletteValues(const AtomString& name, const AtomString& fontFamily, std::optional<FontPaletteIndex> basePalette, Vector<FontPaletteValues::OverriddenColor>&& overrideColors)
    : StyleRuleBase(StyleRuleType::FontPaletteValues)
    , m_name(name)
    , m_fontFamily(fontFamily)
    , m_fontPaletteValues(basePalette, WTFMove(overrideColors))
{
}

StyleRuleFontPaletteValues::StyleRuleFontPaletteValues(const StyleRuleFontPaletteValues& o)
    : StyleRuleBase(o)
    , m_name(o.name())
    , m_fontFamily(o.fontFamily())
    , m_fontPaletteValues(o.m_fontPaletteValues)
{
}

StyleRuleFontPaletteValues::~StyleRuleFontPaletteValues() = default;

DeferredStyleGroupRuleList::DeferredStyleGroupRuleList(const CSSParserTokenRange& range, CSSDeferredParser& parser)
    : m_tokens(range.begin(), range.end() - range.begin())
    , m_parser(parser)
{
}

DeferredStyleGroupRuleList::~DeferredStyleGroupRuleList() = default;

void DeferredStyleGroupRuleList::parseDeferredRules(Vector<RefPtr<StyleRuleBase>>& childRules)
{
    m_parser->parseRuleList(m_tokens, childRules);
}

void DeferredStyleGroupRuleList::parseDeferredKeyframes(StyleRuleKeyframes& keyframesRule)
{
    m_parser->parseKeyframeList(m_tokens, keyframesRule);
}
    
StyleRuleGroup::StyleRuleGroup(StyleRuleType type, Vector<RefPtr<StyleRuleBase>>&& rules)
    : StyleRuleBase(type)
    , m_childRules(WTFMove(rules))
{
}

StyleRuleGroup::StyleRuleGroup(StyleRuleType type, std::unique_ptr<DeferredStyleGroupRuleList>&& rules)
    : StyleRuleBase(type)
    , m_deferredRules(WTFMove(rules))
{
}

StyleRuleGroup::StyleRuleGroup(const StyleRuleGroup& o)
    : StyleRuleBase(o)
    , m_childRules(o.childRules().map([](auto& rule) -> RefPtr<StyleRuleBase> { return rule->copy(); }))
{
}

const Vector<RefPtr<StyleRuleBase>>& StyleRuleGroup::childRules() const
{
    parseDeferredRulesIfNeeded();
    return m_childRules;
}

void StyleRuleGroup::wrapperInsertRule(unsigned index, Ref<StyleRuleBase>&& rule)
{
    parseDeferredRulesIfNeeded();
    m_childRules.insert(index, WTFMove(rule));
}
    
void StyleRuleGroup::wrapperRemoveRule(unsigned index)
{
    parseDeferredRulesIfNeeded();
    m_childRules.remove(index);
}

void StyleRuleGroup::parseDeferredRulesIfNeeded() const
{
    if (!m_deferredRules)
        return;
    
    m_deferredRules->parseDeferredRules(m_childRules);
    m_deferredRules = nullptr;
}
    
StyleRuleMedia::StyleRuleMedia(Ref<MediaQuerySet>&& media, Vector<RefPtr<StyleRuleBase>>&& rules)
    : StyleRuleGroup(StyleRuleType::Media, WTFMove(rules))
    , m_mediaQueries(WTFMove(media))
{
}

StyleRuleMedia::StyleRuleMedia(Ref<MediaQuerySet>&& media, std::unique_ptr<DeferredStyleGroupRuleList>&& rules)
    : StyleRuleGroup(StyleRuleType::Media, WTFMove(rules))
    , m_mediaQueries(WTFMove(media))
{
}

StyleRuleMedia::StyleRuleMedia(const StyleRuleMedia& other)
    : StyleRuleGroup(other)
    , m_mediaQueries(other.m_mediaQueries->copy())
{
}

Ref<StyleRuleMedia> StyleRuleMedia::create(Ref<MediaQuerySet>&& media, Vector<RefPtr<StyleRuleBase>>&& rules)
{
    return adoptRef(*new StyleRuleMedia(WTFMove(media), WTFMove(rules)));
}

Ref<StyleRuleMedia> StyleRuleMedia::create(Ref<MediaQuerySet>&& media, std::unique_ptr<DeferredStyleGroupRuleList>&& deferredChildRules)
{
    return adoptRef(*new StyleRuleMedia(WTFMove(media), WTFMove(deferredChildRules)));
}

Ref<StyleRuleMedia> StyleRuleMedia::copy() const
{
    return adoptRef(*new StyleRuleMedia(*this));
}

StyleRuleSupports::StyleRuleSupports(const String& conditionText, bool conditionIsSupported, Vector<RefPtr<StyleRuleBase>>&& rules)
    : StyleRuleGroup(StyleRuleType::Supports, WTFMove(rules))
    , m_conditionText(conditionText)
    , m_conditionIsSupported(conditionIsSupported)
{
}

StyleRuleSupports::StyleRuleSupports(const String& conditionText, bool conditionIsSupported, std::unique_ptr<DeferredStyleGroupRuleList>&& rules)
    : StyleRuleGroup(StyleRuleType::Supports, WTFMove(rules))
    , m_conditionText(conditionText)
    , m_conditionIsSupported(conditionIsSupported)
{
}

StyleRuleSupports::StyleRuleSupports(const StyleRuleSupports& o)
    : StyleRuleGroup(o)
    , m_conditionText(o.m_conditionText)
    , m_conditionIsSupported(o.m_conditionIsSupported)
{
}


Ref<StyleRuleSupports> StyleRuleSupports::create(const String& conditionText, bool conditionIsSupported, Vector<RefPtr<StyleRuleBase>>&& rules)
{
    return adoptRef(*new StyleRuleSupports(conditionText, conditionIsSupported, WTFMove(rules)));
}

Ref<StyleRuleSupports> StyleRuleSupports::create(const String& conditionText, bool conditionIsSupported, std::unique_ptr<DeferredStyleGroupRuleList>&& rules)
{
    return adoptRef(*new StyleRuleSupports(conditionText, conditionIsSupported, WTFMove(rules)));
}

StyleRuleLayer::StyleRuleLayer(Vector<CascadeLayerName>&& nameList)
    : StyleRuleGroup(StyleRuleType::LayerStatement, Vector<RefPtr<StyleRuleBase>> { })
    , m_nameVariant(WTFMove(nameList))
{
}

StyleRuleLayer::StyleRuleLayer(CascadeLayerName&& name, Vector<RefPtr<StyleRuleBase>>&& rules)
    : StyleRuleGroup(StyleRuleType::LayerBlock, WTFMove(rules))
    , m_nameVariant(WTFMove(name))
{
}

StyleRuleLayer::StyleRuleLayer(CascadeLayerName&& name, std::unique_ptr<DeferredStyleGroupRuleList>&& rules)
    : StyleRuleGroup(StyleRuleType::LayerBlock, WTFMove(rules))
    , m_nameVariant(WTFMove(name))
{
}

StyleRuleLayer::StyleRuleLayer(const StyleRuleLayer& other)
    : StyleRuleGroup(other)
    , m_nameVariant(other.m_nameVariant)
{
}

Ref<StyleRuleLayer> StyleRuleLayer::createStatement(Vector<CascadeLayerName>&& nameList)
{
    return adoptRef(*new StyleRuleLayer(WTFMove(nameList)));
}

Ref<StyleRuleLayer> StyleRuleLayer::createBlock(CascadeLayerName&& name, Vector<RefPtr<StyleRuleBase>>&& rules)
{
    return adoptRef(*new StyleRuleLayer(WTFMove(name), WTFMove(rules)));
}

Ref<StyleRuleLayer> StyleRuleLayer::createBlock(CascadeLayerName&& name, std::unique_ptr<DeferredStyleGroupRuleList>&& rules)
{
    return adoptRef(*new StyleRuleLayer(WTFMove(name), WTFMove(rules)));
}

StyleRuleContainer::StyleRuleContainer(FilteredContainerQuery&& query, Vector<RefPtr<StyleRuleBase>>&& rules)
    : StyleRuleGroup(StyleRuleType::Container, WTFMove(rules))
    , m_filteredQuery(WTFMove(query))
{
}

StyleRuleContainer::StyleRuleContainer(FilteredContainerQuery&& query, std::unique_ptr<DeferredStyleGroupRuleList>&& rules)
    : StyleRuleGroup(StyleRuleType::Container, WTFMove(rules))
    , m_filteredQuery(WTFMove(query))
{
}

StyleRuleContainer::StyleRuleContainer(const StyleRuleContainer& other)
    : StyleRuleGroup(other)
    , m_filteredQuery(other.m_filteredQuery)
{
}

Ref<StyleRuleContainer> StyleRuleContainer::create(FilteredContainerQuery&& query, Vector<RefPtr<StyleRuleBase>>&& rules)
{
    return adoptRef(*new StyleRuleContainer(WTFMove(query), WTFMove(rules)));
}

Ref<StyleRuleContainer> StyleRuleContainer::create(FilteredContainerQuery&& query, std::unique_ptr<DeferredStyleGroupRuleList>&& rules)
{
    return adoptRef(*new StyleRuleContainer(WTFMove(query), WTFMove(rules)));
}

StyleRuleCharset::StyleRuleCharset()
    : StyleRuleBase(StyleRuleType::Charset)
{
}

StyleRuleCharset::StyleRuleCharset(const StyleRuleCharset& o)
    : StyleRuleBase(o)
{
}

StyleRuleNamespace::StyleRuleNamespace(const AtomString& prefix, const AtomString& uri)
    : StyleRuleBase(StyleRuleType::Namespace)
    , m_prefix(prefix)
    , m_uri(uri)
{
}

StyleRuleNamespace::StyleRuleNamespace(const StyleRuleNamespace& o)
    : StyleRuleBase(o)
    , m_prefix(o.m_prefix)
    , m_uri(o.m_uri)
{
}

StyleRuleNamespace::~StyleRuleNamespace() = default;

Ref<StyleRuleNamespace> StyleRuleNamespace::create(const AtomString& prefix, const AtomString& uri)
{
    return adoptRef(*new StyleRuleNamespace(prefix, uri));
}

} // namespace WebCore
