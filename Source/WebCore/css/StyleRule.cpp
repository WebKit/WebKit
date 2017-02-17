/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
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

#include "CSSDeferredParser.h"
#include "CSSFontFaceRule.h"
#include "CSSImportRule.h"
#include "CSSKeyframeRule.h"
#include "CSSKeyframesRule.h"
#include "CSSMediaRule.h"
#include "CSSNamespaceRule.h"
#include "CSSPageRule.h"
#include "CSSStyleRule.h"
#include "CSSSupportsRule.h"
#include "CSSUnknownRule.h"
#include "MediaList.h"
#include "StyleProperties.h"
#include "StyleRuleImport.h"
#include "WebKitCSSRegionRule.h"
#include "WebKitCSSViewportRule.h"

namespace WebCore {

struct SameSizeAsStyleRuleBase : public WTF::RefCountedBase {
    unsigned bitfields : 5;
};

COMPILE_ASSERT(sizeof(StyleRuleBase) == sizeof(SameSizeAsStyleRuleBase), StyleRuleBase_should_stay_small);

RefPtr<CSSRule> StyleRuleBase::createCSSOMWrapper(CSSStyleSheet* parentSheet) const
{
    return createCSSOMWrapper(parentSheet, nullptr);
}

RefPtr<CSSRule> StyleRuleBase::createCSSOMWrapper(CSSRule* parentRule) const
{ 
    return createCSSOMWrapper(nullptr, parentRule);
}

void StyleRuleBase::destroy()
{
    switch (type()) {
    case Style:
        delete downcast<StyleRule>(this);
        return;
    case Page:
        delete downcast<StyleRulePage>(this);
        return;
    case FontFace:
        delete downcast<StyleRuleFontFace>(this);
        return;
    case Media:
        delete downcast<StyleRuleMedia>(this);
        return;
    case Supports:
        delete downcast<StyleRuleSupports>(this);
        return;
#if ENABLE(CSS_REGIONS)
    case Region:
        delete downcast<StyleRuleRegion>(this);
        return;
#endif
    case Import:
        delete downcast<StyleRuleImport>(this);
        return;
    case Keyframes:
        delete downcast<StyleRuleKeyframes>(this);
        return;
#if ENABLE(CSS_DEVICE_ADAPTATION)
    case Viewport:
        delete downcast<StyleRuleViewport>(this);
        return;
#endif
    case Namespace:
        delete downcast<StyleRuleNamespace>(this);
        return;
    case Keyframe:
        delete downcast<StyleRuleKeyframe>(this);
        return;
    case Charset:
        delete downcast<StyleRuleCharset>(this);
        return;
    case Unknown:
#if !ENABLE(CSS_REGIONS)
    case Region:
#endif
        ASSERT_NOT_REACHED();
        return;
    }
    ASSERT_NOT_REACHED();
}

Ref<StyleRuleBase> StyleRuleBase::copy() const
{
    switch (type()) {
    case Style:
        return downcast<StyleRule>(*this).copy();
    case Page:
        return downcast<StyleRulePage>(*this).copy();
    case FontFace:
        return downcast<StyleRuleFontFace>(*this).copy();
    case Media:
        return downcast<StyleRuleMedia>(*this).copy();
    case Supports:
        return downcast<StyleRuleSupports>(*this).copy();
#if ENABLE(CSS_REGIONS)
    case Region:
        return downcast<StyleRuleRegion>(*this).copy();
#endif
    case Keyframes:
        return downcast<StyleRuleKeyframes>(*this).copy();
#if ENABLE(CSS_DEVICE_ADAPTATION)
    case Viewport:
        return downcast<StyleRuleViewport>(*this).copy();
#endif
    case Import:
    case Namespace:
        // FIXME: Copy import and namespace rules.
        break;
    case Unknown:
    case Charset:
    case Keyframe:
#if !ENABLE(CSS_REGIONS)
    case Region:
#endif
        break;
    }
    CRASH();
}

RefPtr<CSSRule> StyleRuleBase::createCSSOMWrapper(CSSStyleSheet* parentSheet, CSSRule* parentRule) const
{
    RefPtr<CSSRule> rule;
    StyleRuleBase& self = const_cast<StyleRuleBase&>(*this);
    switch (type()) {
    case Style:
        rule = CSSStyleRule::create(downcast<StyleRule>(self), parentSheet);
        break;
    case Page:
        rule = CSSPageRule::create(downcast<StyleRulePage>(self), parentSheet);
        break;
    case FontFace:
        rule = CSSFontFaceRule::create(downcast<StyleRuleFontFace>(self), parentSheet);
        break;
    case Media:
        rule = CSSMediaRule::create(downcast<StyleRuleMedia>(self), parentSheet);
        break;
    case Supports:
        rule = CSSSupportsRule::create(downcast<StyleRuleSupports>(self), parentSheet);
        break;
#if ENABLE(CSS_REGIONS)
    case Region:
        rule = WebKitCSSRegionRule::create(downcast<StyleRuleRegion>(self), parentSheet);
        break;
#endif
    case Import:
        rule = CSSImportRule::create(downcast<StyleRuleImport>(self), parentSheet);
        break;
    case Keyframes:
        rule = CSSKeyframesRule::create(downcast<StyleRuleKeyframes>(self), parentSheet);
        break;
#if ENABLE(CSS_DEVICE_ADAPTATION)
    case Viewport:
        rule = WebKitCSSViewportRule::create(downcast<StyleRuleViewport>(self), parentSheet);
        break;
#endif
    case Namespace:
        rule = CSSNamespaceRule::create(downcast<StyleRuleNamespace>(self), parentSheet);
        break;
    case Unknown:
    case Charset:
    case Keyframe:
#if !ENABLE(CSS_REGIONS)
    case Region:
#endif
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    if (parentRule)
        rule->setParentRule(parentRule);
    return rule;
}

unsigned StyleRule::averageSizeInBytes()
{
    return sizeof(StyleRule) + sizeof(CSSSelector) + StyleProperties::averageSizeInBytes();
}

StyleRule::StyleRule(Ref<StylePropertiesBase>&& properties)
    : StyleRuleBase(Style)
    , m_properties(WTFMove(properties))
{
}

StyleRule::StyleRule(const StyleRule& o)
    : StyleRuleBase(o)
    , m_properties(o.properties().mutableCopy())
    , m_selectorList(o.m_selectorList)
{
}

StyleRule::~StyleRule()
{
}

const StyleProperties& StyleRule::properties() const
{
    if (m_properties->type() == DeferredPropertiesType)
        m_properties = downcast<DeferredStyleProperties>(m_properties.get()).parseDeferredProperties();
    return downcast<StyleProperties>(m_properties.get());
}

MutableStyleProperties& StyleRule::mutableProperties()
{
    if (!is<MutableStyleProperties>(m_properties.get()))
        m_properties = properties().mutableCopy();
    return downcast<MutableStyleProperties>(m_properties.get());
}

Ref<StyleRule> StyleRule::create(const Vector<const CSSSelector*>& selectors, Ref<StyleProperties>&& properties)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!selectors.isEmpty());
    CSSSelector* selectorListArray = reinterpret_cast<CSSSelector*>(fastMalloc(sizeof(CSSSelector) * selectors.size()));
    for (unsigned i = 0; i < selectors.size(); ++i)
        new (NotNull, &selectorListArray[i]) CSSSelector(*selectors.at(i));
    selectorListArray[selectors.size() - 1].setLastInSelectorList();
    auto rule = StyleRule::create(WTFMove(properties));
    rule.get().parserAdoptSelectorArray(selectorListArray);
    return rule;
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
            rules.append(create(componentsSinceLastSplit, const_cast<StyleProperties&>(properties())));
            componentsSinceLastSplit.clear();
        }

        componentsSinceLastSplit.appendVector(componentsInThisSelector);
    }

    if (!componentsSinceLastSplit.isEmpty())
        rules.append(create(componentsSinceLastSplit, const_cast<StyleProperties&>(properties())));

    return rules;
}

StyleRulePage::StyleRulePage(Ref<StyleProperties>&& properties)
    : StyleRuleBase(Page)
    , m_properties(WTFMove(properties))
{
}

StyleRulePage::StyleRulePage(const StyleRulePage& o)
    : StyleRuleBase(o)
    , m_properties(o.m_properties->mutableCopy())
    , m_selectorList(o.m_selectorList)
{
}

StyleRulePage::~StyleRulePage()
{
}

MutableStyleProperties& StyleRulePage::mutableProperties()
{
    if (!is<MutableStyleProperties>(m_properties.get()))
        m_properties = m_properties->mutableCopy();
    return downcast<MutableStyleProperties>(m_properties.get());
}

StyleRuleFontFace::StyleRuleFontFace(Ref<StyleProperties>&& properties)
    : StyleRuleBase(FontFace)
    , m_properties(WTFMove(properties))
{
}

StyleRuleFontFace::StyleRuleFontFace(const StyleRuleFontFace& o)
    : StyleRuleBase(o)
    , m_properties(o.m_properties->mutableCopy())
{
}

StyleRuleFontFace::~StyleRuleFontFace()
{
}

MutableStyleProperties& StyleRuleFontFace::mutableProperties()
{
    if (!is<MutableStyleProperties>(m_properties.get()))
        m_properties = m_properties->mutableCopy();
    return downcast<MutableStyleProperties>(m_properties.get());
}

DeferredStyleGroupRuleList::DeferredStyleGroupRuleList(const CSSParserTokenRange& range, CSSDeferredParser& parser)
    : m_parser(parser)
{
    size_t length = range.end() - range.begin();
    m_tokens.reserveCapacity(length);
    m_tokens.append(range.begin(), length);
}

void DeferredStyleGroupRuleList::parseDeferredRules(Vector<RefPtr<StyleRuleBase>>& childRules)
{
    m_parser->parseRuleList(m_tokens, childRules);
}

void DeferredStyleGroupRuleList::parseDeferredKeyframes(StyleRuleKeyframes& keyframesRule)
{
    m_parser->parseKeyframeList(m_tokens, keyframesRule);
}
    
StyleRuleGroup::StyleRuleGroup(Type type, Vector<RefPtr<StyleRuleBase>>& adoptRule)
    : StyleRuleBase(type)
{
    m_childRules.swap(adoptRule);
}

StyleRuleGroup::StyleRuleGroup(Type type, std::unique_ptr<DeferredStyleGroupRuleList>&& deferredRules)
    : StyleRuleBase(type)
    , m_deferredRules(WTFMove(deferredRules))
{
}

StyleRuleGroup::StyleRuleGroup(const StyleRuleGroup& o)
    : StyleRuleBase(o)
{
    m_childRules.reserveInitialCapacity(o.childRules().size());
    for (auto& childRule : o.childRules())
        m_childRules.uncheckedAppend(childRule->copy());
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
    
StyleRuleMedia::StyleRuleMedia(Ref<MediaQuerySet>&& media, Vector<RefPtr<StyleRuleBase>>& adoptRules)
    : StyleRuleGroup(Media, adoptRules)
    , m_mediaQueries(WTFMove(media))
{
}

StyleRuleMedia::StyleRuleMedia(Ref<MediaQuerySet>&& media, std::unique_ptr<DeferredStyleGroupRuleList>&& deferredRules)
    : StyleRuleGroup(Media, WTFMove(deferredRules))
    , m_mediaQueries(WTFMove(media))
{
}

StyleRuleMedia::StyleRuleMedia(const StyleRuleMedia& o)
    : StyleRuleGroup(o)
{
    if (o.m_mediaQueries)
        m_mediaQueries = o.m_mediaQueries->copy();
}


StyleRuleSupports::StyleRuleSupports(const String& conditionText, bool conditionIsSupported, Vector<RefPtr<StyleRuleBase>>& adoptRules)
    : StyleRuleGroup(Supports, adoptRules)
    , m_conditionText(conditionText)
    , m_conditionIsSupported(conditionIsSupported)
{
}

StyleRuleSupports::StyleRuleSupports(const String& conditionText, bool conditionIsSupported,  std::unique_ptr<DeferredStyleGroupRuleList>&& deferredRules)
    : StyleRuleGroup(Supports, WTFMove(deferredRules))
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

StyleRuleRegion::StyleRuleRegion(Vector<std::unique_ptr<CSSParserSelector>>* selectors, Vector<RefPtr<StyleRuleBase>>& adoptRules)
    : StyleRuleGroup(Region, adoptRules)
{
    m_selectorList.adoptSelectorVector(*selectors);
}

StyleRuleRegion::StyleRuleRegion(CSSSelectorList& selectors, Vector<RefPtr<StyleRuleBase>>& adoptRules)
    : StyleRuleGroup(Region, adoptRules)
    , m_selectorList(WTFMove(selectors))
{
}

StyleRuleRegion::StyleRuleRegion(const StyleRuleRegion& o)
    : StyleRuleGroup(o)
    , m_selectorList(o.m_selectorList)
{
}


#if ENABLE(CSS_DEVICE_ADAPTATION)
StyleRuleViewport::StyleRuleViewport(Ref<StyleProperties>&& properties)
    : StyleRuleBase(Viewport, 0)
    , m_properties(WTFMove(properties))
{
}

StyleRuleViewport::StyleRuleViewport(const StyleRuleViewport& o)
    : StyleRuleBase(o)
    , m_properties(o.m_properties->mutableCopy())
{
}

StyleRuleViewport::~StyleRuleViewport()
{
}

MutableStyleProperties& StyleRuleViewport::mutableProperties()
{
    if (!m_properties->isMutable())
        m_properties = m_properties->mutableCopy();
    return static_cast<MutableStyleProperties&>(m_properties.get());
}
#endif // ENABLE(CSS_DEVICE_ADAPTATION)

StyleRuleCharset::StyleRuleCharset()
    : StyleRuleBase(Charset)
{
}

StyleRuleCharset::StyleRuleCharset(const StyleRuleCharset& o)
    : StyleRuleBase(o)
{
}

StyleRuleCharset::~StyleRuleCharset()
{
}

StyleRuleNamespace::StyleRuleNamespace(AtomicString prefix, AtomicString uri)
    : StyleRuleBase(Namespace)
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

StyleRuleNamespace::~StyleRuleNamespace()
{
}

} // namespace WebCore
