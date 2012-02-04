/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2011 Apple Inc. All rights reserved.
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
#include "CSSMappedAttributeDeclaration.h"

#include "CSSImageValue.h"
#include "CSSParser.h"
#include "CSSValuePool.h"
#include "StyledElement.h"

namespace WebCore {

inline void CSSMappedAttributeDeclaration::setNeedsStyleRecalc(StyledElement* element)
{
    ASSERT(element);
    element->setNeedsStyleRecalc(FullStyleChange);
}

CSSMappedAttributeDeclaration::~CSSMappedAttributeDeclaration()
{
}

void CSSMappedAttributeDeclaration::setMappedImageProperty(StyledElement* element, int propertyId, const String& url)
{
    m_declaration->setProperty(CSSProperty(propertyId, CSSImageValue::create(url)));
    setNeedsStyleRecalc(element);
}

void CSSMappedAttributeDeclaration::setMappedLengthProperty(StyledElement* element, int propertyId, const String& value)
{
    setMappedProperty(element, propertyId, value);
}

void CSSMappedAttributeDeclaration::setMappedProperty(StyledElement* element, int propertyId, int value)
{
    ASSERT(element->document());
    m_declaration->setProperty(CSSProperty(propertyId, element->document()->cssValuePool()->createIdentifierValue(value)));
    setNeedsStyleRecalc(element);
}

void CSSMappedAttributeDeclaration::setMappedProperty(StyledElement* element, int propertyId, const String& value)
{
    if (value.isEmpty()) {
        removeMappedProperty(element, propertyId);
        return;
    }

    if (!CSSParser::parseMappedAttributeValue(this, element, propertyId, value)) {
        removeMappedProperty(element, propertyId);
        return;
    }

    setNeedsStyleRecalc(element);
}

void CSSMappedAttributeDeclaration::removeMappedProperty(StyledElement* element, int propertyId)
{
    m_declaration->removeProperty(propertyId, false, false);
    setNeedsStyleRecalc(element);
}

}
