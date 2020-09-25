/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002-2020 Apple Inc. All rights reserved.
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
#include "StyleRuleType.h"
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
    
DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleRuleBase);
class StyleRuleBase : public WTF::RefCountedBase {
    WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleRuleBase);
public:
    StyleRuleType type() const { return static_cast<StyleRuleType>(m_type); }
    
    bool isCharsetRule() const { return type() == StyleRuleType::Charset; }
    bool isFontFaceRule() const { return type() == StyleRuleType::FontFace; }
    bool isKeyframesRule() const { return type() == StyleRuleType::Keyframes; }
    bool isKeyframeRule() const { return type() == StyleRuleType::Keyframe; }
    bool isNamespaceRule() const { return type() == StyleRuleType::Namespace; }
    bool isMediaRule() const { return type() == StyleRuleType::Media; }
    bool isPageRule() const { return type() == StyleRuleType::Page; }
    bool isStyleRule() const { return type() == StyleRuleType::Style; }
    bool isSupportsRule() const { return type() == StyleRuleType::Supports; }
    bool isImportRule() const { return type() == StyleRuleType::Import; }

    Ref<StyleRuleBase> copy() const;

    void deref() const;

    // FIXME: There shouldn't be any need for the null parent version.
    Ref<CSSRule> createCSSOMWrapper(CSSStyleSheet* parentSheet = nullptr) const;
    Ref<CSSRule> createCSSOMWrapper(CSSRule* parentRule) const;

protected:
    explicit StyleRuleBase(StyleRuleType, bool hasDocumentSecurityOrigin = false);
    StyleRuleBase(const StyleRuleBase&);
    ~StyleRuleBase() = default;

    bool hasDocumentSecurityOrigin() const { return m_hasDocumentSecurityOrigin; }

private:
    WEBCORE_EXPORT void destroy();
    
    Ref<CSSRule> createCSSOMWrapper(CSSStyleSheet* parentSheet, CSSRule* parentRule) const;

    unsigned m_type : 5; // StyleRuleType
    // This is only needed to support getMatchedCSSRules.
    unsigned m_hasDocumentSecurityOrigin : 1;
};

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleRule);
class StyleRule final : public StyleRuleBase {
    WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleRule);
public:
    static Ref<StyleRule> create(Ref<StylePropertiesBase>&&, bool hasDocumentSecurityOrigin, CSSSelectorList&&);
    Ref<StyleRule> copy() const;
    ~StyleRule();

    const CSSSelectorList& selectorList() const { return m_selectorList; }

    const StyleProperties& properties() const;
    MutableStyleProperties& mutableProperties();
    const StyleProperties* propertiesWithoutDeferredParsing() const;

    using StyleRuleBase::hasDocumentSecurityOrigin;

    void wrapperAdoptSelectorList(CSSSelectorList&&);

    Vector<RefPtr<StyleRule>> splitIntoMultipleRulesWithMaximumSelectorComponentCount(unsigned) const;

#if ENABLE(CSS_SELECTOR_JIT)
    CompiledSelector& compiledSelectorForListIndex(unsigned index) const;
    void releaseCompiledSelectors() const { m_compiledSelectors = nullptr; }
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
    static Ref<StyleRulePage> create(Ref<StyleProperties>&&, CSSSelectorList&&);

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
    StyleRuleGroup(StyleRuleType, Vector<RefPtr<StyleRuleBase>>&&);
    StyleRuleGroup(StyleRuleType, std::unique_ptr<DeferredStyleGroupRuleList>&&);
    StyleRuleGroup(const StyleRuleGroup&);
    
private:
    void parseDeferredRulesIfNeeded() const;

    mutable Vector<RefPtr<StyleRuleBase>> m_childRules;
    mutable std::unique_ptr<DeferredStyleGroupRuleList> m_deferredRules;
};

class StyleRuleMedia final : public StyleRuleGroup {
public:
    static Ref<StyleRuleMedia> create(Ref<MediaQuerySet>&&, Vector<RefPtr<StyleRuleBase>>&&);
    static Ref<StyleRuleMedia> create(Ref<MediaQuerySet>&&, std::unique_ptr<DeferredStyleGroupRuleList>&&);
    Ref<StyleRuleMedia> copy() const;

    MediaQuerySet& mediaQueries() const { return m_mediaQueries; }

private:
    StyleRuleMedia(Ref<MediaQuerySet>&&, Vector<RefPtr<StyleRuleBase>>&&);
    StyleRuleMedia(Ref<MediaQuerySet>&&, std::unique_ptr<DeferredStyleGroupRuleList>&&);
    StyleRuleMedia(const StyleRuleMedia&);

    Ref<MediaQuerySet> m_mediaQueries;
};

class StyleRuleSupports final : public StyleRuleGroup {
public:
    static Ref<StyleRuleSupports> create(const String& conditionText, bool conditionIsSupported, Vector<RefPtr<StyleRuleBase>>&&);
    static Ref<StyleRuleSupports> create(const String& conditionText, bool conditionIsSupported, std::unique_ptr<DeferredStyleGroupRuleList>&&);
    Ref<StyleRuleSupports> copy() const { return adoptRef(*new StyleRuleSupports(*this)); }

    String conditionText() const { return m_conditionText; }
    bool conditionIsSupported() const { return m_conditionIsSupported; }

private:
    StyleRuleSupports(const String& conditionText, bool conditionIsSupported, Vector<RefPtr<StyleRuleBase>>&&);
    StyleRuleSupports(const String& conditionText, bool conditionIsSupported, std::unique_ptr<DeferredStyleGroupRuleList>&&);
    StyleRuleSupports(const StyleRuleSupports&);

    String m_conditionText;
    bool m_conditionIsSupported;
};

// This is only used by the CSS parser.
class StyleRuleCharset final : public StyleRuleBase {
public:
    static Ref<StyleRuleCharset> create() { return adoptRef(*new StyleRuleCharset); }
    Ref<StyleRuleCharset> copy() const { return adoptRef(*new StyleRuleCharset(*this)); }

private:
    StyleRuleCharset();
    StyleRuleCharset(const StyleRuleCharset&);
};

class StyleRuleNamespace final : public StyleRuleBase {
public:
    static Ref<StyleRuleNamespace> create(const AtomString& prefix, const AtomString& uri);
    
    ~StyleRuleNamespace();

    Ref<StyleRuleNamespace> copy() const { return adoptRef(*new StyleRuleNamespace(*this)); }
    
    AtomString prefix() const { return m_prefix; }
    AtomString uri() const { return m_uri; }

private:
    StyleRuleNamespace(const AtomString& prefix, const AtomString& uri);
    StyleRuleNamespace(const StyleRuleNamespace&);

    AtomString m_prefix;
    AtomString m_uri;
};
    
inline StyleRuleBase::StyleRuleBase(StyleRuleType type, bool hasDocumentSecurityOrigin)
    : m_type(static_cast<unsigned>(type))
    , m_hasDocumentSecurityOrigin(hasDocumentSecurityOrigin)
{
}

inline StyleRuleBase::StyleRuleBase(const StyleRuleBase& o)
    : WTF::RefCountedBase()
    , m_type(o.m_type)
    , m_hasDocumentSecurityOrigin(o.m_hasDocumentSecurityOrigin)
{
}

inline void StyleRuleBase::deref() const
{
    if (derefBase())
        const_cast<StyleRuleBase&>(*this).destroy();
}

inline void StyleRule::wrapperAdoptSelectorList(CSSSelectorList&& selectors)
{
    m_selectorList = WTFMove(selectors);
#if ENABLE(CSS_SELECTOR_JIT)
    m_compiledSelectors = nullptr;
#endif
}

#if ENABLE(CSS_SELECTOR_JIT)

inline CompiledSelector& StyleRule::compiledSelectorForListIndex(unsigned index) const
{
    if (!m_compiledSelectors)
        m_compiledSelectors = makeUniqueArray<CompiledSelector>(m_selectorList.listSize());
    return m_compiledSelectors[index];
}

#endif

inline const StyleProperties* StyleRule::propertiesWithoutDeferredParsing() const
{
    return m_properties->type() != DeferredPropertiesType ? &downcast<StyleProperties>(m_properties.get()) : nullptr;
}

inline const Vector<RefPtr<StyleRuleBase>>* StyleRuleGroup::childRulesWithoutDeferredParsing() const
{
    return !m_deferredRules ? &m_childRules : nullptr;
}

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

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleRuleNamespace)
    static bool isType(const WebCore::StyleRuleBase& rule) { return rule.isNamespaceRule(); }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleRuleKeyframe)
    static bool isType(const WebCore::StyleRuleBase& rule) { return rule.isKeyframeRule(); }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleRuleCharset)
    static bool isType(const WebCore::StyleRuleBase& rule) { return rule.isCharsetRule(); }
SPECIALIZE_TYPE_TRAITS_END()
