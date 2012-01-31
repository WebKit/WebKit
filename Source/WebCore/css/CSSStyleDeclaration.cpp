/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "CSSStyleDeclaration.h"

#include "CSSMutableStyleDeclaration.h"
#include "CSSParser.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSRule.h"
#include "Node.h"
#include "SVGElement.h"
#include "StyledElement.h"
#include <wtf/ASCIICType.h>
#include <wtf/text/CString.h>
#ifndef NDEBUG
#include <stdio.h>
#endif

using namespace WTF;

namespace WebCore {

CSSStyleDeclaration::CSSStyleDeclaration(CSSRule* parentRule)
    : m_strictParsing(!parentRule || parentRule->useStrictParsing())
    , m_isInlineStyleDeclaration(false)
    , m_parent(parentRule)
{
}

CSSStyleDeclaration::CSSStyleDeclaration(StyledElement* parentElement)
    : m_strictParsing(false)
    , m_isInlineStyleDeclaration(true)
    , m_parent(parentElement)
{
}

CSSStyleSheet* CSSStyleDeclaration::parentStyleSheet() const
{
    if (m_isInlineStyleDeclaration) {
        Document* document = m_parent.element ? m_parent.element->document() : 0;
        return document ? document->elementSheet() : 0;
    }
    return m_parent.rule ? m_parent.rule->parentStyleSheet() : 0;
}

bool CSSStyleDeclaration::isPropertyName(const String& propertyName)
{
    return cssPropertyID(propertyName);
}

#ifndef NDEBUG
void CSSStyleDeclaration::showStyle()
{
    fprintf(stderr, "%s\n", cssText().ascii().data());
}
#endif

} // namespace WebCore
