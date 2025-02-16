/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "CSSNestedDeclarations.h"

#include "CSSSerializationContext.h"
#include "DeclaredStylePropertyMap.h"
#include "MutableStyleProperties.h"
#include "PropertySetCSSStyleDeclaration.h"
#include "StyleProperties.h"
#include "StyleRule.h"

#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSNestedDeclarations::CSSNestedDeclarations(StyleRuleNestedDeclarations& rule, CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_styleRule(rule)
{
}

CSSNestedDeclarations::~CSSNestedDeclarations() = default;

CSSStyleDeclaration& CSSNestedDeclarations::style()
{
    if (!m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper = StyleRuleCSSStyleDeclaration::create(m_styleRule->mutableProperties(), *this);
    return *m_propertiesCSSOMWrapper;
}

String CSSNestedDeclarations::cssText() const
{
    return m_styleRule->properties().asText(CSS::defaultSerializationContext());
}

void CSSNestedDeclarations::reattach(StyleRuleBase& rule)
{
    m_styleRule = downcast<StyleRuleNestedDeclarations>(rule);

    if (m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper->reattach(m_styleRule->mutableProperties());
}

} // namespace WebCore
