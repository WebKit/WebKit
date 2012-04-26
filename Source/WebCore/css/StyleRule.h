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
        Region
    };
    Type type() const { return static_cast<Type>(m_type); }
    
    bool isCharsetRule() const { return type() == Charset; }
    bool isFontFaceRule() const { return type() == FontFace; }
    bool isKeyframesRule() const { return type() == Keyframes; }
    bool isMediaRule() const { return type() == Media; }
    bool isPageRule() const { return type() == Page; }
    bool isStyleRule() const { return type() == Style; }
    bool isRegionRule() const { return type() == Region; }
    bool isImportRule() const { return type() == Import; }

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

protected:
    StyleRuleBase(Type type, signed sourceLine = 0) : m_type(type), m_sourceLine(sourceLine) { }
    StyleRuleBase(const StyleRuleBase& o) : WTF::RefCountedBase(), m_type(o.m_type), m_sourceLine(o.m_sourceLine) { }

    ~StyleRuleBase() { }

private:
    void destroy();
    
    PassRefPtr<CSSRule> createCSSOMWrapper(CSSStyleSheet* parentSheet, CSSRule* parentRule) const;

    unsigned m_type : 4;
    signed m_sourceLine : 28;
};

class StyleRule : public StyleRuleBase {
public:
    static PassRefPtr<StyleRule> create(int sourceLine) { return adoptRef(new StyleRule(sourceLine)); }
    
    ~StyleRule();

    const CSSSelectorList& selectorList() const { return m_selectorList; }
    StylePropertySet* properties() const { return m_properties.get(); }
    
    void parserAdoptSelectorVector(Vector<OwnPtr<CSSParserSelector> >& selectors) { m_selectorList.adoptSelectorVector(selectors); }
    void wrapperAdoptSelectorList(CSSSelectorList& selectors) { m_selectorList.adopt(selectors); }
    void setProperties(PassRefPtr<StylePropertySet>);

    PassRefPtr<StyleRule> copy() const { return adoptRef(new StyleRule(*this)); }

    static unsigned averageSizeInBytes();

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

    StylePropertySet* properties() const { return m_properties.get(); }

    void setProperties(PassRefPtr<StylePropertySet>);

    PassRefPtr<StyleRuleFontFace> copy() const { return adoptRef(new StyleRuleFontFace(*this)); }

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
    StylePropertySet* properties() const { return m_properties.get(); }

    void parserAdoptSelectorVector(Vector<OwnPtr<CSSParserSelector> >& selectors) { m_selectorList.adoptSelectorVector(selectors); }
    void wrapperAdoptSelectorList(CSSSelectorList& selectors) { m_selectorList.adopt(selectors); }
    void setProperties(PassRefPtr<StylePropertySet>);

    PassRefPtr<StyleRulePage> copy() const { return adoptRef(new StyleRulePage(*this)); }

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

private:
    StyleRuleMedia(PassRefPtr<MediaQuerySet>, Vector<RefPtr<StyleRuleBase> >& adoptRules);
    StyleRuleMedia(const StyleRuleMedia&);

    RefPtr<MediaQuerySet> m_mediaQueries;
};

class StyleRuleRegion : public StyleRuleBlock {
public:
    static PassRefPtr<StyleRuleRegion> create(Vector<OwnPtr<CSSParserSelector> >* selectors, Vector<RefPtr<StyleRuleBase> >& adoptRules)
    {
        return adoptRef(new StyleRuleRegion(selectors, adoptRules));
    }

    const CSSSelectorList& selectorList() const { return m_selectorList; }

    PassRefPtr<StyleRuleRegion> copy() const { return adoptRef(new StyleRuleRegion(*this)); }

private:
    StyleRuleRegion(Vector<OwnPtr<CSSParserSelector> >*, Vector<RefPtr<StyleRuleBase> >& adoptRules);
    StyleRuleRegion(const StyleRuleRegion&);
    
    CSSSelectorList m_selectorList;
};

} // namespace WebCore

#endif // StyleRule_h
