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
#include "CSSFontPaletteValuesRule.h"
#include "CSSGroupingRule.h"
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

static_assert(sizeof(StyleRuleBase) == sizeof(SameSizeAsStyleRuleBase), "StyleRuleBase should stay small");

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleRuleBase);
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleRule);

Ref<CSSRule> StyleRuleBase::createCSSOMWrapper(CSSStyleSheet& parentSheet) const
{
    return createCSSOMWrapper(&parentSheet, nullptr);
}

Ref<CSSRule> StyleRuleBase::createCSSOMWrapper(CSSGroupingRule& parentRule) const
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
        return std::invoke(std::forward<Visitor>(visitor), downcast<StyleRule>(*this));
    case StyleRuleType::Page:
        return std::invoke(std::forward<Visitor>(visitor), downcast<StyleRulePage>(*this));
    case StyleRuleType::FontFace:
        return std::invoke(std::forward<Visitor>(visitor), downcast<StyleRuleFontFace>(*this));
    case StyleRuleType::FontPaletteValues:
        return std::invoke(std::forward<Visitor>(visitor), downcast<StyleRuleFontPaletteValues>(*this));
    case StyleRuleType::Media:
        return std::invoke(std::forward<Visitor>(visitor), downcast<StyleRuleMedia>(*this));
    case StyleRuleType::Supports:
        return std::invoke(std::forward<Visitor>(visitor), downcast<StyleRuleSupports>(*this));
    case StyleRuleType::Import:
        return std::invoke(std::forward<Visitor>(visitor), downcast<StyleRuleImport>(*this));
    case StyleRuleType::Keyframes:
        return std::invoke(std::forward<Visitor>(visitor), downcast<StyleRuleKeyframes>(*this));
    case StyleRuleType::Namespace:
        return std::invoke(std::forward<Visitor>(visitor), downcast<StyleRuleNamespace>(*this));
    case StyleRuleType::Keyframe:
        return std::invoke(std::forward<Visitor>(visitor), downcast<StyleRuleKeyframe>(*this));
    case StyleRuleType::Charset:
        return std::invoke(std::forward<Visitor>(visitor), downcast<StyleRuleCharset>(*this));
    case StyleRuleType::CounterStyle:
        return std::invoke(std::forward<Visitor>(visitor), downcast<StyleRuleCounterStyle>(*this));
    case StyleRuleType::LayerBlock:
    case StyleRuleType::LayerStatement:
        return std::invoke(std::forward<Visitor>(visitor), downcast<StyleRuleLayer>(*this));
    case StyleRuleType::Container:
        return std::invoke(std::forward<Visitor>(visitor), downcast<StyleRuleContainer>(*this));
    case StyleRuleType::Margin:
        RELEASE_ASSERT_NOT_REACHED();
    case StyleRuleType::Unknown:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

template<typename Visitor> constexpr decltype(auto) StyleRuleBase::visitDerived(Visitor&& visitor) const
{
    return const_cast<StyleRuleBase&>(*this).visitDerived([&](auto& value) {
        return std::invoke(std::forward<Visitor>(visitor), std::as_const(value));
    });
}

void StyleRuleBase::operator delete(StyleRuleBase* rule, std::destroying_delete_t)
{
    rule->visitDerived([](auto& rule) {
        std::destroy_at(&rule);
        std::decay_t<decltype(rule)>::freeAfterDestruction(&rule);
    });
}

Ref<StyleRuleBase> StyleRuleBase::copy() const
{
    return visitDerived([](auto& rule) -> Ref<StyleRuleBase> {
        // Check at compile time for a mistake where this function would call itself, leading to infinite recursion.
        // We can do this with the types of pointers to member functions because they includes the type of the class.
        static_assert(!std::is_same_v<decltype(&std::decay_t<decltype(rule)>::copy), decltype(&StyleRuleBase::copy)>);
        return rule.copy();
    });
}

Ref<CSSRule> StyleRuleBase::createCSSOMWrapper(CSSStyleSheet* parentSheet, CSSGroupingRule* parentRule) const
{
    // FIXME: const_cast is required here because a wrapper for a style rule can be used to *modify* the style rule's selector; use of const in the style system is thus inaccurate.
    auto wrapper = const_cast<StyleRuleBase&>(*this).visitDerived(WTF::makeVisitor(
        [&](StyleRule& rule) -> Ref<CSSRule> {
            return CSSStyleRule::create(rule, parentSheet);
        },
        [&](StyleRulePage& rule) -> Ref<CSSRule> {
            return CSSPageRule::create(rule, parentSheet);
        },
        [&](StyleRuleFontFace& rule) -> Ref<CSSRule> {
            return CSSFontFaceRule::create(rule, parentSheet);
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
    return sizeof(StyleRule) + sizeof(CSSSelector) + StyleProperties::averageSizeInBytes();
}

StyleRule::StyleRule(Ref<StyleProperties>&& properties, bool hasDocumentSecurityOrigin, CSSSelectorList&& selectors)
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

Ref<StyleRule> StyleRule::create(Ref<StyleProperties>&& properties, bool hasDocumentSecurityOrigin, CSSSelectorList&& selectors)
{
    return adoptRef(*new StyleRule(WTFMove(properties), hasDocumentSecurityOrigin, WTFMove(selectors)));
}

Ref<StyleRule> StyleRule::copy() const
{
    return adoptRef(*new StyleRule(*this));
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

Ref<StyleRuleFontPaletteValues> StyleRuleFontPaletteValues::create(const AtomString& name, const AtomString& fontFamily, std::optional<FontPaletteIndex> basePalette, Vector<FontPaletteValues::OverriddenColor>&& overrideColors)
{
    return adoptRef(*new StyleRuleFontPaletteValues(name, fontFamily, basePalette, WTFMove(overrideColors)));
}

StyleRuleFontPaletteValues::StyleRuleFontPaletteValues(const AtomString& name, const AtomString& fontFamily, std::optional<FontPaletteIndex> basePalette, Vector<FontPaletteValues::OverriddenColor>&& overrideColors)
    : StyleRuleBase(StyleRuleType::FontPaletteValues)
    , m_name(name)
    , m_fontFamily(fontFamily)
    , m_fontPaletteValues(basePalette, WTFMove(overrideColors))
{
}
    
StyleRuleGroup::StyleRuleGroup(StyleRuleType type, Vector<RefPtr<StyleRuleBase>>&& rules)
    : StyleRuleBase(type)
    , m_childRules(WTFMove(rules))
{
}

StyleRuleGroup::StyleRuleGroup(const StyleRuleGroup& o)
    : StyleRuleBase(o)
    , m_childRules(o.childRules().map([](auto& rule) -> RefPtr<StyleRuleBase> { return rule->copy(); }))
{
}

const Vector<RefPtr<StyleRuleBase>>& StyleRuleGroup::childRules() const
{
    return m_childRules;
}

void StyleRuleGroup::wrapperInsertRule(unsigned index, Ref<StyleRuleBase>&& rule)
{
    m_childRules.insert(index, WTFMove(rule));
}
    
void StyleRuleGroup::wrapperRemoveRule(unsigned index)
{
    m_childRules.remove(index);
}

StyleRuleMedia::StyleRuleMedia(Ref<MediaQuerySet>&& media, Vector<RefPtr<StyleRuleBase>>&& rules)
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

Ref<StyleRuleSupports> StyleRuleSupports::create(const String& conditionText, bool conditionIsSupported, Vector<RefPtr<StyleRuleBase>>&& rules)
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

Ref<StyleRuleLayer> StyleRuleLayer::createStatement(Vector<CascadeLayerName>&& nameList)
{
    return adoptRef(*new StyleRuleLayer(WTFMove(nameList)));
}

Ref<StyleRuleLayer> StyleRuleLayer::createBlock(CascadeLayerName&& name, Vector<RefPtr<StyleRuleBase>>&& rules)
{
    return adoptRef(*new StyleRuleLayer(WTFMove(name), WTFMove(rules)));
}

StyleRuleContainer::StyleRuleContainer(CQ::ContainerQuery&& query, Vector<RefPtr<StyleRuleBase>>&& rules)
    : StyleRuleGroup(StyleRuleType::Container, WTFMove(rules))
    , m_containerQuery(WTFMove(query))
{
}

Ref<StyleRuleContainer> StyleRuleContainer::create(CQ::ContainerQuery&& query, Vector<RefPtr<StyleRuleBase>>&& rules)
{
    return adoptRef(*new StyleRuleContainer(WTFMove(query), WTFMove(rules)));
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

} // namespace WebCore
