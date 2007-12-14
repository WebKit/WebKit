/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef NamedMappedAttrMap_h
#define NamedMappedAttrMap_h

#include "ClassNames.h"
#include "MappedAttribute.h"
#include "NamedAttrMap.h"

namespace WebCore {

class NamedMappedAttrMap : public NamedAttrMap
{
public:
    NamedMappedAttrMap(Element *e);

    virtual void clearAttributes();
    
    virtual bool isMappedAttributeMap() const;
    
    void parseClassAttribute(const String&);

    const ClassNames* getClassNames() const { return &m_classNames; }
    
    virtual bool hasMappedAttributes() const { return m_mappedAttributeCount > 0; }
    void declRemoved() { m_mappedAttributeCount--; }
    void declAdded() { m_mappedAttributeCount++; }
    
    bool mapsEquivalent(const NamedMappedAttrMap* otherMap) const;
    int declCount() const;

    MappedAttribute* attributeItem(unsigned index) const
        { return static_cast<MappedAttribute*>(NamedAttrMap::attributeItem(index)); }
    MappedAttribute* getAttributeItem(const QualifiedName& name) const
        { return static_cast<MappedAttribute*>(NamedAttrMap::getAttributeItem(name)); }
    MappedAttribute* getAttributeItem(const String& name) const
        { return static_cast<MappedAttribute*>(NamedAttrMap::getAttributeItem(name)); }
    
private:
    ClassNames m_classNames;
    int m_mappedAttributeCount;
};

} //namespace

#endif
