/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2008, 2012 Apple Inc. All rights reserved.
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

#ifndef StyleRule_h
#define StyleRule_h

#include "CSSSelectorList.h"
#include "MediaList.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSRule;
class CSSStyleRule;
class CSSStyleSheet;
class StylePropertySet;

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
#if ENABLE(SHADOW_DOM)
        Host,
#endif
#if ENABLE(CSS3_CONDITIONAL_RULES)
        Supports = 12,
#endif
#if ENABLE(CSS_DEVICE_ADAPTATION)
        Viewport = 15,
#endif
        Region = 16
    };
    Type type() const { return static_cast<Type>(m_type); }
    
    bool isCharsetRule() const { return type() == Charset; }
    bool isFontFaceRule() const { return type() == FontFace; }
    bool isKeyframesRule() const { return type() == Keyframes; }
    bool isMediaRule() const { return type() == Media; }
    bool isPageRule() const { return type() == Page; }
    bool isStyleRule() const { return type() == Style; }
    bool isRegionRule() const { return type() == Region; }
#if ENABLE(CSS3_CONDITIONAL_RULES)
    bool isSupportsRule() const { return type() == Supports; }
#endif
#if ENABLE(CSS_DEVICE_ADAPTATION)
    bool isViewportRule() const { return type() == Viewport; }
#endif
    bool isImportRule() const { return type() == Import; }
#if ENABLE(SHADOW_DOM)
    bool isHostRule() const { return type() == Host; }
#endif

    PassRefPtr<StyleRuleBase> copy() const;

    int sourceLine() const { return m_sourceLine; }

    void deref()
    {
        if (derefBase())
            destroy();
    }

    // FIXME: There shouldn't be any need for the null parent version.
    PassRefPtr<CSSRule> createCSSOMWrapper(CSSStyleSheet* parentSheet = 0) const;
    PassRefPtr<CSSRule> createCSSOMWrapper(CSSRule* parentRule) const;

    void reportMemoryUsage(MemoryObjectInfo*) const;

protected:
    StyleRuleBase(Type type, signed sourceLine = 0) : m_type(type), m_sourceLine(sourceLine) { }
    StyleRuleBase(const StyleRuleBase& o) : WTF::RefCountedBase(), m_type(o.m_type), m_sourceLine(o.m_sourceLine) { }

    ~StyleRuleBase() { }

private:
    void destroy();
    
    PassRefPtr<CSSRule> createCSSOMWrapper(CSSStyleSheet* parentSheet, CSSRule* parentRule) const;

    unsigned m_type : 5;
    signed m_sourceLine : 27;
};

class StyleRule : public StyleRuleBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassRefPtr<StyleRule> create(int sourceLine) { return adoptRef(new StyleRule(sourceLine)); }
    
    ~StyleRule();

    const CSSSelectorList& selectorList() const { return m_selectorList; }
    const StylePropertySet* properties() const { return m_properties.get(); }
    StylePropertySet* mutableProperties();
    
    void parserAdoptSelectorVector(Vector<OwnPtr<CSSParserSelector> >& selectors) { m_selectorList.adoptSelectorVector(selectors); }
    void wrapperAdoptSelectorList(CSSSelectorList& selectors) { m_selectorList.adopt(selectors); }
    void setProperties(PassRefPtr<StylePropertySet>);

    PassRefPtr<StyleRule> copy() const { return adoptRef(new StyleRule(*this)); }

    static unsigned averageSizeInBytes();
    void reportDescendantMemoryUsage(MemoryObjectInfo*) const;

private:
    StyleRule(int sourceLine);
    StyleRule(const StyleRule&);

    RefPtr<StylePropertySet> m_properties;
    CSSSelectorList m_selectorList;
};

class StyleRuleFontFace : public StyleRuleBase {
public:
    static PassRefPtr<StyleRuleFontFace> create() { return adoptRef(new StyleRuleFontFace); }
    
    ~StyleRuleFontFace();

    const StylePropertySet* properties() const { return m_properties.get(); }
    StylePropertySet* mutableProperties();

    void setProperties(PassRefPtr<StylePropertySet>);

    PassRefPtr<StyleRuleFontFace> copy() const { return adoptRef(new StyleRuleFontFace(*this)); }

    void reportDescendantMemoryUsage(MemoryObjectInfo*) const;

private:
    StyleRuleFontFace();
    StyleRuleFontFace(const StyleRuleFontFace&);

    RefPtr<StylePropertySet> m_properties;
};

class StyleRulePage : public StyleRuleBase {
public:
    static PassRefPtr<StyleRulePage> create() { return adoptRef(new StyleRulePage); }

    ~StyleRulePage();

    const CSSSelector* selector() const { return m_selectorList.first(); }    
    const StylePropertySet* properties() const { return m_properties.get(); }
    StylePropertySet* mutableProperties();

    void parserAdoptSelectorVector(Vector<OwnPtr<CSSParserSelector> >& selectors) { m_selectorList.adoptSelectorVector(selectors); }
    void wrapperAdoptSelectorList(CSSSelectorList& selectors) { m_selectorList.adopt(selectors); }
    void setProperties(PassRefPtr<StylePropertySet>);

    PassRefPtr<StyleRulePage> copy() const { return adoptRef(new StyleRulePage(*this)); }

    void reportDescendantMemoryUsage(MemoryObjectInfo*) const;

private:
    StyleRulePage();
    StyleRulePage(const StyleRulePage&);
    
    RefPtr<StylePropertySet> m_properties;
    CSSSelectorList m_selectorList;
};

class StyleRuleBlock : public StyleRuleBase {
public:
    const Vector<RefPtr<StyleRuleBase> >& childRules() const { return m_childRules; }
    
    void wrapperInsertRule(unsigned, PassRefPtr<StyleRuleBase>);
    void wrapperRemoveRule(unsigned);

    void reportDescendantMemoryUsage(MemoryObjectInfo*) const;
    
protected:
    StyleRuleBlock(Type, Vector<RefPtr<StyleRuleBase> >& adoptRule);
    StyleRuleBlock(const StyleRuleBlock&);
    
private:
    Vector<RefPtr<StyleRuleBase> > m_childRules;
};

class StyleRuleMedia : public StyleRuleBlock {
public:
    static PassRefPtr<StyleRuleMedia> create(PassRefPtr<MediaQuerySet> media, Vector<RefPtr<StyleRuleBase> >& adoptRules)
    {
        return adoptRef(new StyleRuleMedia(media, adoptRules));
    }

    MediaQuerySet* mediaQueries() const { return m_mediaQueries.get(); }

    PassRefPtr<StyleRuleMedia> copy() const { return adoptRef(new StyleRuleMedia(*this)); }

    void reportDescendantMemoryUsage(MemoryObjectInfo*) const;

private:
    StyleRuleMedia(PassRefPtr<MediaQuerySet>, Vector<RefPtr<StyleRuleBase> >& adoptRules);
    StyleRuleMedia(const StyleRuleMedia&);

    RefPtr<MediaQuerySet> m_mediaQueries;
};

#if ENABLE(CSS3_CONDITIONAL_RULES)
class StyleRuleSupports : public StyleRuleBlock {
public:
    static PassRefPtr<StyleRuleSupports> create(const String& conditionText, bool conditionIsSupported, Vector<RefPtr<StyleRuleBase> >& adoptRules)
    {
        return adoptRef(new StyleRuleSupports(conditionText, conditionIsSupported, adoptRules));
    }

    String conditionText() const { return m_conditionText; }
    bool conditionIsSupported() const { return m_conditionIsSupported; }
    PassRefPtr<StyleRuleSupports> copy() const { return adoptRef(new StyleRuleSupports(*this)); }

private:
    StyleRuleSupports(const String& conditionText, bool conditionIsSupported, Vector<RefPtr<StyleRuleBase> >& adoptRules);
    StyleRuleSupports(const StyleRuleSupports&);

    String m_conditionText;
    bool m_conditionIsSupported;
};
#endif

class StyleRuleRegion : public StyleRuleBlock {
public:
    static PassRefPtr<StyleRuleRegion> create(Vector<OwnPtr<CSSParserSelector> >* selectors, Vector<RefPtr<StyleRuleBase> >& adoptRules)
    {
        return adoptRef(new StyleRuleRegion(selectors, adoptRules));
    }

    const CSSSelectorList& selectorList() const { return m_selectorList; }

    PassRefPtr<StyleRuleRegion> copy() const { return adoptRef(new StyleRuleRegion(*this)); }

    void reportDescendantMemoryUsage(MemoryObjectInfo*) const;

private:
    StyleRuleRegion(Vector<OwnPtr<CSSParserSelector> >*, Vector<RefPtr<StyleRuleBase> >& adoptRules);
    StyleRuleRegion(const StyleRuleRegion&);
    
    CSSSelectorList m_selectorList;
};

#if ENABLE(SHADOW_DOM)
class StyleRuleHost : public StyleRuleBlock {
public:
    static PassRefPtr<StyleRuleHost> create(Vector<RefPtr<StyleRuleBase> >& adoptRules)
    {
        return adoptRef(new StyleRuleHost(adoptRules));
    }

    PassRefPtr<StyleRuleHost> copy() const { return adoptRef(new StyleRuleHost(*this)); }

private:
    StyleRuleHost(Vector<RefPtr<StyleRuleBase> >& adoptRules) : StyleRuleBlock(Host, adoptRules) { }
    StyleRuleHost(const StyleRuleHost& o) : StyleRuleBlock(o) { }
};
#endif

#if ENABLE(CSS_DEVICE_ADAPTATION)
class StyleRuleViewport : public StyleRuleBase {
public:
    static PassRefPtr<StyleRuleViewport> create() { return adoptRef(new StyleRuleViewport); }

    ~StyleRuleViewport();

    const StylePropertySet* properties() const { return m_properties.get(); }
    StylePropertySet* mutableProperties();

    void setProperties(PassRefPtr<StylePropertySet>);

    PassRefPtr<StyleRuleViewport> copy() const { return adoptRef(new StyleRuleViewport(*this)); }

    void reportDescendantMemoryUsage(MemoryObjectInfo*) const;

private:
    StyleRuleViewport();
    StyleRuleViewport(const StyleRuleViewport&);

    RefPtr<StylePropertySet> m_properties;
};
#endif // ENABLE(CSS_DEVICE_ADAPTATION)

inline const StyleRuleMedia* toStyleRuleMedia(const StyleRuleBlock* rule)
{
    ASSERT(!rule || rule->isMediaRule());
    return static_cast<const StyleRuleMedia*>(rule);
}

#if ENABLE(CSS3_CONDITIONAL_RULES)
inline const StyleRuleSupports* toStyleRuleSupports(const StyleRuleBlock* rule)
{
    ASSERT(!rule || rule->isSupportsRule());
    return static_cast<const StyleRuleSupports*>(rule);
}
#endif

inline const StyleRuleRegion* toStyleRuleRegion(const StyleRuleBlock* rule)
{
    ASSERT(!rule || rule->isRegionRule());
    return static_cast<const StyleRuleRegion*>(rule);
}

} // namespace WebCore

#endif // StyleRule_h
