/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002-2020 Apple Inc.
 * Copyright (C) 2006 Samuel Weinig (sam@webkit.org)
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
#include "CSSMediaRule.h"

#include "CSSStyleSheet.h"
#include "MediaList.h"
#include "MediaQueryParser.h"
#include "StyleRule.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSMediaRule::CSSMediaRule(StyleRuleMedia& mediaRule, CSSStyleSheet* parent)
    : CSSConditionRule(mediaRule, parent)
{
}

CSSMediaRule::~CSSMediaRule()
{
    if (m_mediaCSSOMWrapper)
        m_mediaCSSOMWrapper->detachFromParent();
}

const MQ::MediaQueryList& CSSMediaRule::mediaQueries() const
{
    return downcast<StyleRuleMedia>(groupRule()).mediaQueries();
}

void CSSMediaRule::setMediaQueries(MQ::MediaQueryList&& queries)
{
    downcast<StyleRuleMedia>(groupRule()).setMediaQueries(WTFMove(queries));
}

String CSSMediaRule::cssText() const
{
    StringBuilder result;
    result.append("@media ", conditionText(), " {\n");
    appendCSSTextForItems(result);
    result.append('}');
    return result.toString();
}

String CSSMediaRule::conditionText() const
{
    StringBuilder builder;
    MQ::serialize(builder, mediaQueries());
    return builder.toString();
}

MediaList* CSSMediaRule::media() const
{
    if (!m_mediaCSSOMWrapper)
        m_mediaCSSOMWrapper = MediaList::create(const_cast<CSSMediaRule*>(this));
    return m_mediaCSSOMWrapper.get();
}

} // namespace WebCore
