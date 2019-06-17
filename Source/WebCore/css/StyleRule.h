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
#include "CompiledSelector.h"
#include "StyleProperties.h"
#include <wtf/RefPtr.h>
#include <wtf/TypeCasts.h>
#include <wtf/UniqueArray.h>

namespace WebCore {

class CSSRule;
class CSSStyleRule;
class CSSStyleSheet;
class MediaQuerySet;
class MutableStyleProperties;
class StyleRuleKeyframe;
class StyleProperties;
class StyleRuleKeyframes;
    
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
        Keyframe, // Not used. These are internally non-rule StyleRuleKeyframe objects.
        Namespace,
        Supports = 12,
#if ENABLE(CSS_DEVICE_ADAPTATION)
        Viewport = 15,
#endif
    };

    Type type() const { return static_cast<Type>(m_type); }
    
    bool isCharsetRule() const { return type() == Charset; }
    bool isFontFaceRule() const { return type() == FontFace; }
    bool isKeyframesRule() const { return type() == Keyframes; }
    bool isKeyframeRule() const { return type() == Keyframe; }
    bool isNamespaceRule() const { return type() == Namespace; }
    bool isMediaRule() const { return type() == Media; }
    bool isPageRule() const { return type() == Page; }
    bool isStyleRule() const { return type() == Style; }
    bool isSupportsRule() const { return type() == Supports; }
#if ENABLE(CSS_DEVICE_ADAPTATION)
    bool isViewportRule() const { return type() == Viewport; }
#endif
    bool isImportRule() const { return type() == Import; }

    Ref<StyleRuleBase> copy() const;

    void deref()
    {
        if (derefBase())
            destroy();
    }

    // FIXME: There shouldn't be any need for the null parent version.
    Ref<CSSRule> createCSSOMWrapper(CSSStyleSheet* parentSheet = nullptr) const;
    Ref<CSSRule> createCSSOMWrapper(CSSRule* parentRule) const;

protected:
    StyleRuleBase(Type type, bool hasDocumentSecurityOrigin = false)
        : m_type(type)
        , m_hasDocumentSecurityOrigin(hasDocumentSecurityOrigin)
    {
    }

    StyleRuleBase(const StyleRuleBase& o)
        : WTF::RefCountedBase()
        , m_type(o.m_type)
        , m_hasDocumentSecurityOrigin(o.m_hasDocumentSecurityOrigin)
    {
    }

    ~StyleRuleBase() = default;

    bool hasDocumentSecurityOrigin() const { return m_hasDocumentSecurityOrigin; }

private:
    WEBCORE_EXPORT void destroy();
    
    Ref<CSSRule> createCSSOMWrapper(CSSStyleSheet* parentSheet, CSSRule* parentRule) const;

    unsigned m_type : 5;
    // This is only needed to support getMatchedCSSRules.
    unsigned m_hasDocumentSecurityOrigin : 1;
};

class StyleRule final : public StyleRuleBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<StyleRule> create(Ref<StylePropertiesBase>&& properties, bool hasDocumentSecurityOrigin, CSSSelectorList&& selectors)
    {
        return adoptRef(*new StyleRule(WTFMove(properties), hasDocumentSecurityOrigin, WTFMove(selectors)));
    }
    
    ~StyleRule();

    const CSSSelectorList& selectorList() const { return m_selectorList; }
    
    const StyleProperties& properties() const;
    MutableStyleProperties& mutableProperties();
    const StyleProperties* propertiesWithoutDeferredParsing() const;

    using StyleRuleBase::hasDocumentSecurityOrigin;

    void wrapperAdoptSelectorList(CSSSelectorList&& selectors)
    {
        m_selectorList = WTFMove(selectors);
#if ENABLE(CSS_SELECTOR_JIT)
        m_compiledSelectors = nullptr;
#endif
    }

    Ref<StyleRule> copy() const { return adoptRef(*new StyleRule(*this)); }

    Vector<RefPtr<StyleRule>> splitIntoMultipleRulesWithMaximumSelectorComponentCount(unsigned) const;

#if ENABLE(CSS_SELECTOR_JIT)
    CompiledSelector& compiledSelectorForListIndex(unsigned index)
    {
        if (!m_compiledSelectors)
            m_compiledSelectors = makeUniqueArray<CompiledSelector>(m_selectorList.listSize());
        return m_compiledSelectors[index];
    }
    void releaseCompiledSelectors() const
    {
        m_compiledSelectors = nullptr;
    }
#endif

    static unsigned averageSizeInBytes();

private:
    StyleRule(Ref<StylePropertiesBase>&&, bool hasDocumentSecurityOrigin, CSSSelectorList&&);
    StyleRule(const StyleRule&);

    static Ref<StyleRule> createForSplitting(const Vector<const CSSSelector*>&, Ref<StyleProperties>&&, bool hasDocumentSecurityOrigin);

    mutable Ref<StylePropertiesBase> m_properties;
    CSSSelectorList m_selectorList;

#if ENABLE(CSS_SELECTOR_JIT)
    mutable UniqueArray<CompiledSelector> m_compiledSelectors;
#endif
};

inline const StyleProperties* StyleRule::propertiesWithoutDeferredParsing() const
{
    return m_properties->type() != DeferredPropertiesType ? &downcast<StyleProperties>(m_properties.get()) : nullptr;
}

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
    static Ref<StyleRulePage> create(Ref<StyleProperties>&& properties, CSSSelectorList&& selectors) { return adoptRef(*new StyleRulePage(WTFMove(properties), WTFMove(selectors))); }

    ~StyleRulePage();

    const CSSSelector* selector() const { return m_selectorList.first(); }    
    const StyleProperties& properties() const { return m_properties; }
    MutableStyleProperties& mutableProperties();

    void wrapperAdoptSelectorList(CSSSelectorList&& selectors) { m_selectorList = WTFMove(selectors); }

    Ref<StyleRulePage> copy() const { return adoptRef(*new StyleRulePage(*this)); }

private:
    explicit StyleRulePage(Ref<StyleProperties>&&, CSSSelectorList&&);
    StyleRulePage(const StyleRulePage&);
    
    Ref<StyleProperties> m_properties;
    CSSSelectorList m_selectorList;
};

class DeferredStyleGroupRuleList final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DeferredStyleGroupRuleList(const CSSParserTokenRange&, CSSDeferredParser&);
    
    void parseDeferredRules(Vector<RefPtr<StyleRuleBase>>&);
    void parseDeferredKeyframes(StyleRuleKeyframes&);

private:
    Vector<CSSParserToken> m_tokens;
    Ref<CSSDeferredParser> m_parser;
};
    
class StyleRuleGroup : public StyleRuleBase {
public:
    const Vector<RefPtr<StyleRuleBase>>& childRules() const;
    const Vector<RefPtr<StyleRuleBase>>* childRulesWithoutDeferredParsing() const;

    void wrapperInsertRule(unsigned, Ref<StyleRuleBase>&&);
    void wrapperRemoveRule(unsigned);
    
protected:
    StyleRuleGroup(Type, Vector<RefPtr<StyleRuleBase>>&);
    StyleRuleGroup(Type, std::unique_ptr<DeferredStyleGroupRuleList>&&);
    StyleRuleGroup(const StyleRuleGroup&);
    
private:
    void parseDeferredRulesIfNeeded() const;

    mutable Vector<RefPtr<StyleRuleBase>> m_childRules;
    mutable std::unique_ptr<DeferredStyleGroupRuleList> m_deferredRules;
};

inline const Vector<RefPtr<StyleRuleBase>>* StyleRuleGroup::childRulesWithoutDeferredParsing() const
{
    return !m_deferredRules ? &m_childRules : nullptr;
}

class StyleRuleMedia final : public StyleRuleGroup {
public:
    static Ref<StyleRuleMedia> create(Ref<MediaQuerySet>&& media, Vector<RefPtr<StyleRuleBase>>& adoptRules)
    {
        return adoptRef(*new StyleRuleMedia(WTFMove(media), adoptRules));
    }

    static Ref<StyleRuleMedia> create(Ref<MediaQuerySet>&& media, std::unique_ptr<DeferredStyleGroupRuleList>&& deferredChildRules)
    {
        return adoptRef(*new StyleRuleMedia(WTFMove(media), WTFMove(deferredChildRules)));
    }

    MediaQuerySet* mediaQueries() const { return m_mediaQueries.get(); }

    Ref<StyleRuleMedia> copy() const { return adoptRef(*new StyleRuleMedia(*this)); }

private:
    StyleRuleMedia(Ref<MediaQuerySet>&&, Vector<RefPtr<StyleRuleBase>>& adoptRules);
    StyleRuleMedia(Ref<MediaQuerySet>&&, std::unique_ptr<DeferredStyleGroupRuleList>&&);
    StyleRuleMedia(const StyleRuleMedia&);

    RefPtr<MediaQuerySet> m_mediaQueries;
};

class StyleRuleSupports final : public StyleRuleGroup {
public:
    static Ref<StyleRuleSupports> create(const String& conditionText, bool conditionIsSupported, Vector<RefPtr<StyleRuleBase>>& adoptRules)
    {
        return adoptRef(*new StyleRuleSupports(conditionText, conditionIsSupported, adoptRules));
    }
    
    static Ref<StyleRuleSupports> create(const String& conditionText, bool conditionIsSupported, std::unique_ptr<DeferredStyleGroupRuleList>&& deferredChildRules)
    {
        return adoptRef(*new StyleRuleSupports(conditionText, conditionIsSupported, WTFMove(deferredChildRules)));
    }

    String conditionText() const { return m_conditionText; }
    bool conditionIsSupported() const { return m_conditionIsSupported; }
    Ref<StyleRuleSupports> copy() const { return adoptRef(*new StyleRuleSupports(*this)); }

private:
    StyleRuleSupports(const String& conditionText, bool conditionIsSupported, Vector<RefPtr<StyleRuleBase>>& adoptRules);
    StyleRuleSupports(const String& conditionText, bool conditionIsSupported, std::unique_ptr<DeferredStyleGroupRuleList>&&);
    
    StyleRuleSupports(const StyleRuleSupports&);

    String m_conditionText;
    bool m_conditionIsSupported;
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

// This is only used by the CSS parser.
class StyleRuleCharset final : public StyleRuleBase {
public:
    static Ref<StyleRuleCharset> create() { return adoptRef(*new StyleRuleCharset()); }
    
    ~StyleRuleCharset();
    
    Ref<StyleRuleCharset> copy() const { return adoptRef(*new StyleRuleCharset(*this)); }

private:
    explicit StyleRuleCharset();
    StyleRuleCharset(const StyleRuleCharset&);
};

class StyleRuleNamespace final : public StyleRuleBase {
public:
    static Ref<StyleRuleNamespace> create(AtomString prefix, AtomString uri)
    {
        return adoptRef(*new StyleRuleNamespace(prefix, uri));
    }
    
    ~StyleRuleNamespace();

    Ref<StyleRuleNamespace> copy() const { return adoptRef(*new StyleRuleNamespace(*this)); }
    
    AtomString prefix() const { return m_prefix; }
    AtomString uri() const { return m_uri; }

private:
    StyleRuleNamespace(AtomString prefix, AtomString uri);
    StyleRuleNamespace(const StyleRuleNamespace&);
    
    AtomString m_prefix;
    AtomString m_uri;
};
    
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

#if ENABLE(CSS_DEVICE_ADAPTATION)
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleRuleViewport)
    static bool isType(const WebCore::StyleRuleBase& rule) { return rule.isViewportRule(); }
SPECIALIZE_TYPE_TRAITS_END()
#endif // ENABLE(CSS_DEVICE_ADAPTATION)

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleRuleNamespace)
    static bool isType(const WebCore::StyleRuleBase& rule) { return rule.isNamespaceRule(); }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleRuleKeyframe)
static bool isType(const WebCore::StyleRuleBase& rule) { return rule.isKeyframeRule(); }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleRuleCharset)
static bool isType(const WebCore::StyleRuleBase& rule) { return rule.isCharsetRule(); }
SPECIALIZE_TYPE_TRAITS_END()

