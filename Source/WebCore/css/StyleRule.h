/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2008, 2012, 2013 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSSelectorList.h"
#include "StyleProperties.h"
#include <wtf/RefPtr.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

class CSSRule;
class CSSStyleRule;
class CSSStyleSheet;
class MediaQuerySet;
class MutableStyleProperties;
class StyleProperties;

class StyleRuleBase : public WTF::RefCountedBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum Type {
        Unknown, // Not used.
        Style,
        Charset, // Not used. These are internally strings owned by the style sheet.
        Import,
        Media,
        FontFace,
        Page,
        Keyframes,
        Keyframe, // Not used. These are internally non-rule StyleKeyframe objects.
        Supports = 12,
#if ENABLE(CSS_DEVICE_ADAPTATION)
        Viewport = 15,
#endif
        Region = 16,
    };

    Type type() const { return static_cast<Type>(m_type); }
    
    bool isCharsetRule() const { return type() == Charset; }
    bool isFontFaceRule() const { return type() == FontFace; }
    bool isKeyframesRule() const { return type() == Keyframes; }
    bool isMediaRule() const { return type() == Media; }
    bool isPageRule() const { return type() == Page; }
    bool isStyleRule() const { return type() == Style; }
    bool isRegionRule() const { return type() == Region; }
    bool isSupportsRule() const { return type() == Supports; }
#if ENABLE(CSS_DEVICE_ADAPTATION)
    bool isViewportRule() const { return type() == Viewport; }
#endif
    bool isImportRule() const { return type() == Import; }

    Ref<StyleRuleBase> copy() const;

    int sourceLine() const { return m_sourceLine; }

    void deref()
    {
        if (derefBase())
            destroy();
    }

    // FIXME: There shouldn't be any need for the null parent version.
    RefPtr<CSSRule> createCSSOMWrapper(CSSStyleSheet* parentSheet = nullptr) const;
    RefPtr<CSSRule> createCSSOMWrapper(CSSRule* parentRule) const;

protected:
    StyleRuleBase(Type type, signed sourceLine = 0) : m_type(type), m_sourceLine(sourceLine) { }
    StyleRuleBase(const StyleRuleBase& o) : WTF::RefCountedBase(), m_type(o.m_type), m_sourceLine(o.m_sourceLine) { }

    ~StyleRuleBase() { }

private:
    void destroy();
    
    RefPtr<CSSRule> createCSSOMWrapper(CSSStyleSheet* parentSheet, CSSRule* parentRule) const;

    unsigned m_type : 5;
    signed m_sourceLine : 27;
};

class StyleRule final : public StyleRuleBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<StyleRule> create(int sourceLine, Ref<StyleProperties>&& properties)
    {
        return adoptRef(*new StyleRule(sourceLine, WTFMove(properties)));
    }
    
    ~StyleRule();

    const CSSSelectorList& selectorList() const { return m_selectorList; }
    const StyleProperties& properties() const { return m_properties; }
    MutableStyleProperties& mutableProperties();
    
    void parserAdoptSelectorVector(Vector<std::unique_ptr<CSSParserSelector>>& selectors) { m_selectorList.adoptSelectorVector(selectors); }
    void wrapperAdoptSelectorList(CSSSelectorList& selectors) { m_selectorList = WTFMove(selectors); }
    void parserAdoptSelectorArray(CSSSelector* selectors) { m_selectorList.adoptSelectorArray(selectors); }

    Ref<StyleRule> copy() const { return adoptRef(*new StyleRule(*this)); }

    Vector<RefPtr<StyleRule>> splitIntoMultipleRulesWithMaximumSelectorComponentCount(unsigned) const;

    static unsigned averageSizeInBytes();

private:
    StyleRule(int sourceLine, Ref<StyleProperties>&&);
    StyleRule(const StyleRule&);

    static Ref<StyleRule> create(int sourceLine, const Vector<const CSSSelector*>&, Ref<StyleProperties>&&);

    Ref<StyleProperties> m_properties;
    CSSSelectorList m_selectorList;
};

class StyleRuleFontFace final : public StyleRuleBase {
public:
    static Ref<StyleRuleFontFace> create(Ref<StyleProperties>&& properties) { return adoptRef(*new StyleRuleFontFace(WTFMove(properties))); }
    
    ~StyleRuleFontFace();

    const StyleProperties& properties() const { return m_properties; }
    MutableStyleProperties& mutableProperties();

    Ref<StyleRuleFontFace> copy() const { return adoptRef(*new StyleRuleFontFace(*this)); }


private:
    explicit StyleRuleFontFace(Ref<StyleProperties>&&);
    StyleRuleFontFace(const StyleRuleFontFace&);

    Ref<StyleProperties> m_properties;
};

class StyleRulePage final : public StyleRuleBase {
public:
    static Ref<StyleRulePage> create(Ref<StyleProperties>&& properties) { return adoptRef(*new StyleRulePage(WTFMove(properties))); }

    ~StyleRulePage();

    const CSSSelector* selector() const { return m_selectorList.first(); }    
    const StyleProperties& properties() const { return m_properties; }
    MutableStyleProperties& mutableProperties();

    void parserAdoptSelectorVector(Vector<std::unique_ptr<CSSParserSelector>>& selectors) { m_selectorList.adoptSelectorVector(selectors); }
    void wrapperAdoptSelectorList(CSSSelectorList& selectors) { m_selectorList = WTFMove(selectors); }

    Ref<StyleRulePage> copy() const { return adoptRef(*new StyleRulePage(*this)); }

private:
    explicit StyleRulePage(Ref<StyleProperties>&&);
    StyleRulePage(const StyleRulePage&);
    
    Ref<StyleProperties> m_properties;
    CSSSelectorList m_selectorList;
};

class StyleRuleGroup : public StyleRuleBase {
public:
    const Vector<RefPtr<StyleRuleBase>>& childRules() const { return m_childRules; }
    
    void wrapperInsertRule(unsigned, Ref<StyleRuleBase>&&);
    void wrapperRemoveRule(unsigned);
    
protected:
    StyleRuleGroup(Type, Vector<RefPtr<StyleRuleBase>>& adoptRule);
    StyleRuleGroup(const StyleRuleGroup&);
    
private:
    Vector<RefPtr<StyleRuleBase>> m_childRules;
};

class StyleRuleMedia final : public StyleRuleGroup {
public:
    static Ref<StyleRuleMedia> create(Ref<MediaQuerySet>&& media, Vector<RefPtr<StyleRuleBase>>& adoptRules)
    {
        return adoptRef(*new StyleRuleMedia(WTFMove(media), adoptRules));
    }

    MediaQuerySet* mediaQueries() const { return m_mediaQueries.get(); }

    Ref<StyleRuleMedia> copy() const { return adoptRef(*new StyleRuleMedia(*this)); }

private:
    StyleRuleMedia(Ref<MediaQuerySet>&&, Vector<RefPtr<StyleRuleBase>>& adoptRules);
    StyleRuleMedia(const StyleRuleMedia&);

    RefPtr<MediaQuerySet> m_mediaQueries;
};

class StyleRuleSupports final : public StyleRuleGroup {
public:
    static Ref<StyleRuleSupports> create(const String& conditionText, bool conditionIsSupported, Vector<RefPtr<StyleRuleBase>>& adoptRules)
    {
        return adoptRef(*new StyleRuleSupports(conditionText, conditionIsSupported, adoptRules));
    }

    String conditionText() const { return m_conditionText; }
    bool conditionIsSupported() const { return m_conditionIsSupported; }
    Ref<StyleRuleSupports> copy() const { return adoptRef(*new StyleRuleSupports(*this)); }

private:
    StyleRuleSupports(const String& conditionText, bool conditionIsSupported, Vector<RefPtr<StyleRuleBase>>& adoptRules);
    StyleRuleSupports(const StyleRuleSupports&);

    String m_conditionText;
    bool m_conditionIsSupported;
};

class StyleRuleRegion final : public StyleRuleGroup {
public:
    static Ref<StyleRuleRegion> create(Vector<std::unique_ptr<CSSParserSelector>>* selectors, Vector<RefPtr<StyleRuleBase>>& adoptRules)
    {
        return adoptRef(*new StyleRuleRegion(selectors, adoptRules));
    }

    const CSSSelectorList& selectorList() const { return m_selectorList; }

    Ref<StyleRuleRegion> copy() const { return adoptRef(*new StyleRuleRegion(*this)); }

private:
    StyleRuleRegion(Vector<std::unique_ptr<CSSParserSelector>>*, Vector<RefPtr<StyleRuleBase>>& adoptRules);
    StyleRuleRegion(const StyleRuleRegion&);
    
    CSSSelectorList m_selectorList;
};

#if ENABLE(CSS_DEVICE_ADAPTATION)
class StyleRuleViewport final : public StyleRuleBase {
public:
    static Ref<StyleRuleViewport> create(Ref<StyleProperties>&& properties) { return adoptRef(*new StyleRuleViewport(WTFMove(properties))); }

    ~StyleRuleViewport();

    const StyleProperties& properties() const { return m_properties.get(); }
    MutableStyleProperties& mutableProperties();

    Ref<StyleRuleViewport> copy() const { return adoptRef(*new StyleRuleViewport(*this)); }

private:
    explicit StyleRuleViewport(Ref<StyleProperties>&&);
    StyleRuleViewport(const StyleRuleViewport&);

    Ref<StyleProperties> m_properties;
};
#endif // ENABLE(CSS_DEVICE_ADAPTATION)

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleRule)
    static bool isType(const WebCore::StyleRuleBase& rule) { return rule.isStyleRule(); }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleRuleFontFace)
    static bool isType(const WebCore::StyleRuleBase& rule) { return rule.isFontFaceRule(); }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleRuleMedia)
    static bool isType(const WebCore::StyleRuleBase& rule) { return rule.isMediaRule(); }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleRulePage)
    static bool isType(const WebCore::StyleRuleBase& rule) { return rule.isPageRule(); }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleRuleSupports)
    static bool isType(const WebCore::StyleRuleBase& rule) { return rule.isSupportsRule(); }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleRuleRegion)
    static bool isType(const WebCore::StyleRuleBase& rule) { return rule.isRegionRule(); }
SPECIALIZE_TYPE_TRAITS_END()

#if ENABLE(CSS_DEVICE_ADAPTATION)
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleRuleViewport)
    static bool isType(const WebCore::StyleRuleBase& rule) { return rule.isViewportRule(); }
SPECIALIZE_TYPE_TRAITS_END()
#endif // ENABLE(CSS_DEVICE_ADAPTATION)
