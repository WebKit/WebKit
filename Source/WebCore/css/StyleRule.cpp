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
#include "WebKitCSSKeyframeRule.h"
#include "WebKitCSSKeyframesRule.h"
#include "WebKitCSSRegionRule.h"

namespace WebCore {

PassRefPtr<CSSRule> StyleRuleBase::createCSSOMWrapper(CSSStyleSheet* parentSheet) const
{
    return createCSSOMWrapper(parentSheet, 0);
}

PassRefPtr<CSSRule> StyleRuleBase::createCSSOMWrapper(CSSRule* parentRule) const
{ 
    return createCSSOMWrapper(0, parentRule);
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
    case Region:
        delete static_cast<StyleRuleRegion*>(this);
        return;
    case Import:
        delete static_cast<StyleRuleImport*>(this);
        return;
    case Keyframes:
        delete static_cast<StyleRuleKeyframes*>(this);
        return;
    case Unknown:
    case Charset:
    case Keyframe:
        ASSERT_NOT_REACHED();
        return;
    }
    ASSERT_NOT_REACHED();
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
    case Region:
        rule = WebKitCSSRegionRule::create(static_cast<StyleRuleRegion*>(self), parentSheet);
        break;
    case Import:
        rule = CSSImportRule::create(static_cast<StyleRuleImport*>(self), parentSheet);
        break;
    case Keyframes:
        rule = WebKitCSSKeyframesRule::create(static_cast<StyleRuleKeyframes*>(self), parentSheet);
        break;
    case Unknown:
    case Charset:
    case Keyframe:
        ASSERT_NOT_REACHED();
        return 0;
    }
    if (parentRule)
        rule->setParentRule(parentRule);
    return rule.release();
}

StyleRule::StyleRule(int sourceLine)
    : StyleRuleBase(Style, sourceLine)
{
}

StyleRule::~StyleRule()
{
}

void StyleRule::setProperties(PassRefPtr<StylePropertySet> properties)
{ 
    m_properties = properties;
}

StyleRulePage::StyleRulePage()
    : StyleRuleBase(Page)
{
}

StyleRulePage::~StyleRulePage()
{
}

void StyleRulePage::setProperties(PassRefPtr<StylePropertySet> properties)
{ 
    m_properties = properties;
}

StyleRuleFontFace::StyleRuleFontFace()
    : StyleRuleBase(FontFace, 0)
{
}

StyleRuleFontFace::~StyleRuleFontFace()
{
}

void StyleRuleFontFace::setProperties(PassRefPtr<StylePropertySet> properties)
{ 
    m_properties = properties;
}

StyleRuleBlock::StyleRuleBlock(Type type, Vector<RefPtr<StyleRuleBase> >& adoptRule)
    : StyleRuleBase(type, 0)
{
    m_childRules.swap(adoptRule);
}

void StyleRuleBlock::wrapperInsertRule(unsigned index, PassRefPtr<StyleRuleBase> rule)
{
    m_childRules.insert(index, rule);
}
    
void StyleRuleBlock::wrapperRemoveRule(unsigned index)
{
    m_childRules.remove(index);
}

StyleRuleMedia::StyleRuleMedia(PassRefPtr<MediaQuerySet> media, Vector<RefPtr<StyleRuleBase> >& adoptRules)
    : StyleRuleBlock(Media, adoptRules)
    , m_mediaQueries(media)
{
}

StyleRuleRegion::StyleRuleRegion(Vector<OwnPtr<CSSParserSelector> >* selectors, Vector<RefPtr<StyleRuleBase> >& adoptRules)
    : StyleRuleBlock(Region, adoptRules)
{
    m_selectorList.adoptSelectorVector(*selectors);
}

} // namespace WebCore
