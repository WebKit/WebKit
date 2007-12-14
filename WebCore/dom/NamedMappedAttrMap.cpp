/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
#include "NamedMappedAttrMap.h"

#include "Document.h"
#include "Element.h"

namespace WebCore {

NamedMappedAttrMap::NamedMappedAttrMap(Element *e)
    : NamedAttrMap(e)
    , m_mappedAttributeCount(0)
{
}

void NamedMappedAttrMap::clearAttributes()
{
    m_classNames.clear();
    m_mappedAttributeCount = 0;
    NamedAttrMap::clearAttributes();
}

bool NamedMappedAttrMap::isMappedAttributeMap() const
{
    return true;
}

int NamedMappedAttrMap::declCount() const
{
    int result = 0;
    for (unsigned i = 0; i < length(); i++) {
        MappedAttribute* attr = attributeItem(i);
        if (attr->decl())
            result++;
    }
    return result;
}

bool NamedMappedAttrMap::mapsEquivalent(const NamedMappedAttrMap* otherMap) const
{
    // The # of decls must match.
    if (declCount() != otherMap->declCount())
        return false;
    
    // The values for each decl must match.
    for (unsigned i = 0; i < length(); i++) {
        MappedAttribute* attr = attributeItem(i);
        if (attr->decl()) {
            Attribute* otherAttr = otherMap->getAttributeItem(attr->name());
            if (!otherAttr || (attr->value() != otherAttr->value()))
                return false;
        }
    }
    return true;
}

void NamedMappedAttrMap::parseClassAttribute(const String& classStr) 
{ 
    if (!element->hasClass()) { 
        m_classNames.clear(); 
        return;
    }

    m_classNames.parseClassAttribute(classStr, element->document()->inCompatMode()); 
}


}
