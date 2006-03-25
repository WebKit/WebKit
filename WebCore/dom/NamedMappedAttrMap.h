/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef NamedMappedAttrMap_h
#define NamedMappedAttrMap_h

#include "NamedAttrMap.h"
#include "AtomicStringList.h"
#include "MappedAttribute.h"

namespace WebCore {

class NamedMappedAttrMap : public NamedAttrMap
{
public:
    NamedMappedAttrMap(Element *e);

    virtual void clearAttributes();
    
    virtual bool isMappedAttributeMap() const;
    
    virtual void parseClassAttribute(const String& classAttr);
    const AtomicStringList* getClassList() const { return &m_classList; }
    
    virtual bool hasMappedAttributes() const { return m_mappedAttributeCount > 0; }
    void declRemoved() { m_mappedAttributeCount--; }
    void declAdded() { m_mappedAttributeCount++; }
    
    bool mapsEquivalent(const NamedMappedAttrMap* otherMap) const;
    int declCount() const;

    MappedAttribute* attributeItem(unsigned index) const
    { return attrs ? static_cast<MappedAttribute*>(attrs[index]) : 0; }
    
private:
    AtomicStringList m_classList;
    int m_mappedAttributeCount;
};

} //namespace

#endif
