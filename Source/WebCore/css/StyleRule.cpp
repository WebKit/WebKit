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

#include "CSSCharsetRule.h"
#include "CSSFontFaceRule.h"
#include "CSSImportRule.h"
#include "CSSMediaRule.h"
#include "CSSPageRule.h"
#include "CSSStyleRule.h"
#include "MemoryInstrumentation.h"
#include "StyleRuleImport.h"
#include "WebKitCSSKeyframeRule.h"
#include "WebKitCSSKeyframesRule.h"
#include "WebKitCSSRegionRule.h"

namespace WebCore {

struct SameSizeAsStyleRuleBase : public WTF::RefCountedBase {
    unsigned bitfields;
};

COMPILE_ASSERT(sizeof(StyleRuleBase) == sizeof(SameSizeAsStyleRuleBase), StyleRuleBase_should_stay_small);

PassRefPtr<CSSRule> StyleRuleBase::createCSSOMWrapper(CSSStyleSheet* parentSheet) const
{
    return createCSSOMWrapper(parentSheet, 0);
}

PassRefPtr<CSSRule> StyleRuleBase::createCSSOMWrapper(CSSRule* parentRule) const
{ 
    return createCSSOMWrapper(0, parentRule);
}

void StyleRuleBase::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    switch (type()) {
    case Style:
        static_cast<const StyleRule*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case Page:
        static_cast<const StyleRulePage*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case FontFace:
        static_cast<const StyleRuleFontFace*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case Media:
        static_cast<const StyleRuleMedia*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
#if ENABLE(CSS_REGIONS)
    case Region:
        static_cast<const StyleRuleRegion*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
#endif
    case Import:
        static_cast<const StyleRuleImport*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case Keyframes:
        static_cast<const StyleRuleKeyframes*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case Unknown:
    case Charset:
    case Keyframe:
        MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::CSS);
        return;
    }
    ASSERT_NOT_REACHED();
}

void StyleRuleBase::destroy()
{
    switch (type()) {
    case Style:
        delete static_cast<StyleRule*>(this);
        return;
    case Page:
        delete static_cast<StyleRulePage*>(this);
        return;
    case FontFace:
        delete static_cast<StyleRuleFontFace*>(this);
        return;
    case Media:
        delete static_cast<StyleRuleMedia*>(this);
        return;
#if ENABLE(CSS_REGIONS)
    case Region:
        delete static_cast<StyleRuleRegion*>(this);
        return;
#endif
    case Import:
        delete static_cast<StyleRuleImport*>(this);
        return;
    case Keyframes:
        delete static_cast<StyleRuleKeyframes*>(this);
        return;
    case Unknown:
    case Charset:
    case Keyframe:
#if !ENABLE(CSS_REGIONS)
    case Region:
#endif
        ASSERT_NOT_REACHED();
        return;
    }
    ASSERT_NOT_REACHED();
}

PassRefPtr<StyleRuleBase> StyleRuleBase::copy() const
{
    switch (type()) {
    case Style:
        return static_cast<const StyleRule*>(this)->copy();
    case Page:
        return static_cast<const StyleRulePage*>(this)->copy();
    case FontFace:
        return static_cast<const StyleRuleFontFace*>(this)->copy();
    case Media:
        return static_cast<const StyleRuleMedia*>(this)->copy();
#if ENABLE(CSS_REGIONS)
    case Region:
        return static_cast<const StyleRuleRegion*>(this)->copy();
#endif
    case Import:
        // FIXME: Copy import rules.
        ASSERT_NOT_REACHED();
        return 0;
    case Keyframes:
        return static_cast<const StyleRuleKeyframes*>(this)->copy();
    case Unknown:
    case Charset:
    case Keyframe:
#if !ENABLE(CSS_REGIONS)
    case Region:
#endif
        ASSERT_NOT_REACHED();
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

PassRefPtr<CSSRule> StyleRuleBase::createCSSOMWrapper(CSSStyleSheet* parentSheet, CSSRule* parentRule) const
{
    RefPtr<CSSRule> rule;
    StyleRuleBase* self = const_cast<StyleRuleBase*>(this);
    switch (type()) {
    case Style:
        rule = CSSStyleRule::create(static_cast<StyleRule*>(self), parentSheet);
        break;
    case Page:
        rule = CSSPageRule::create(static_cast<StyleRulePage*>(self), parentSheet);
        break;
    case FontFace:
        rule = CSSFontFaceRule::create(static_cast<StyleRuleFontFace*>(self), parentSheet);
        break;
    case Media:
        rule = CSSMediaRule::create(static_cast<StyleRuleMedia*>(self), parentSheet);
        break;
#if ENABLE(CSS_REGIONS)
    case Region:
        rule = WebKitCSSRegionRule::create(static_cast<StyleRuleRegion*>(self), parentSheet);
        break;
#endif
    case Import:
        rule = CSSImportRule::create(static_cast<StyleRuleImport*>(self), parentSheet);
        break;
    case Keyframes:
        rule = WebKitCSSKeyframesRule::create(static_cast<StyleRuleKeyframes*>(self), parentSheet);
        break;
    case Unknown:
    case Charset:
    case Keyframe:
#if !ENABLE(CSS_REGIONS)
    case Region:
#endif
        ASSERT_NOT_REACHED();
        return 0;
    }
    if (parentRule)
        rule->setParentRule(parentRule);
    return rule.release();
}

unsigned StyleRule::averageSizeInBytes()
{
    return sizeof(StyleRule) + StylePropertySet::averageSizeInBytes();
}

void StyleRule::reportDescendantMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::CSS);
    info.addInstrumentedMember(m_properties);
    info.addInstrumentedMember(m_selectorList);
}

StyleRule::StyleRule(int sourceLine)
    : StyleRuleBase(Style, sourceLine)
{
}

StyleRule::StyleRule(const StyleRule& o)
    : StyleRuleBase(o)
    , m_properties(o.m_properties->copy())
    , m_selectorList(o.m_selectorList)
{
}

StyleRule::~StyleRule()
{
}

StylePropertySet* StyleRule::mutableProperties()
{
    if (!m_properties->isMutable())
        m_properties = m_properties->copy();
    return m_properties.get();
}

void StyleRule::setProperties(PassRefPtr<StylePropertySet> properties)
{ 
    m_properties = properties;
}

StyleRulePage::StyleRulePage()
    : StyleRuleBase(Page)
{
}

StyleRulePage::StyleRulePage(const StyleRulePage& o)
    : StyleRuleBase(o)
    , m_properties(o.m_properties->copy())
    , m_selectorList(o.m_selectorList)
{
}

StyleRulePage::~StyleRulePage()
{
}

StylePropertySet* StyleRulePage::mutableProperties()
{
    if (!m_properties->isMutable())
        m_properties = m_properties->copy();
    return m_properties.get();
}

void StyleRulePage::setProperties(PassRefPtr<StylePropertySet> properties)
{ 
    m_properties = properties;
}

void StyleRulePage::reportDescendantMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::CSS);
    info.addInstrumentedMember(m_properties);
    info.addInstrumentedMember(m_selectorList);
}

StyleRuleFontFace::StyleRuleFontFace()
    : StyleRuleBase(FontFace, 0)
{
}

StyleRuleFontFace::StyleRuleFontFace(const StyleRuleFontFace& o)
    : StyleRuleBase(o)
    , m_properties(o.m_properties->copy())
{
}

StyleRuleFontFace::~StyleRuleFontFace()
{
}

StylePropertySet* StyleRuleFontFace::mutableProperties()
{
    if (!m_properties->isMutable())
        m_properties = m_properties->copy();
    return m_properties.get();
}

void StyleRuleFontFace::setProperties(PassRefPtr<StylePropertySet> properties)
{ 
    m_properties = properties;
}

void StyleRuleFontFace::reportDescendantMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::CSS);
    info.addInstrumentedMember(m_properties);
}


StyleRuleBlock::StyleRuleBlock(Type type, Vector<RefPtr<StyleRuleBase> >& adoptRule)
    : StyleRuleBase(type, 0)
{
    m_childRules.swap(adoptRule);
}

StyleRuleBlock::StyleRuleBlock(const StyleRuleBlock& o)
    : StyleRuleBase(o)
    , m_childRules(o.m_childRules.size())
{
    for (unsigned i = 0; i < m_childRules.size(); ++i)
        m_childRules[i] = o.m_childRules[i]->copy();
}

void StyleRuleBlock::wrapperInsertRule(unsigned index, PassRefPtr<StyleRuleBase> rule)
{
    m_childRules.insert(index, rule);
}
    
void StyleRuleBlock::wrapperRemoveRule(unsigned index)
{
    m_childRules.remove(index);
}

void StyleRuleBlock::reportDescendantMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::CSS);
    info.addInstrumentedVector(m_childRules);
}

StyleRuleMedia::StyleRuleMedia(PassRefPtr<MediaQuerySet> media, Vector<RefPtr<StyleRuleBase> >& adoptRules)
    : StyleRuleBlock(Media, adoptRules)
    , m_mediaQueries(media)
{
}

StyleRuleMedia::StyleRuleMedia(const StyleRuleMedia& o)
    : StyleRuleBlock(o)
{
    if (o.m_mediaQueries)
        m_mediaQueries = o.m_mediaQueries->copy();
}

void StyleRuleMedia::reportDescendantMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::CSS);
    info.addInstrumentedMember(m_mediaQueries);
}

StyleRuleRegion::StyleRuleRegion(CSSSelectorVector* selectors, Vector<RefPtr<StyleRuleBase> >& adoptRules)
    : StyleRuleBlock(Region, adoptRules)
{
    m_selectorList.adoptSelectorVector(*selectors);
}

StyleRuleRegion::StyleRuleRegion(const StyleRuleRegion& o)
    : StyleRuleBlock(o)
    , m_selectorList(o.m_selectorList)
{
}

void StyleRuleRegion::reportDescendantMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::CSS);
    info.addInstrumentedMember(m_selectorList);
}

} // namespace WebCore
