/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
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
#include "NamedNodeMap.h"

namespace WebCore {

class NamedMappedAttrMap : public NamedNodeMap {
public:
    static PassRefPtr<NamedMappedAttrMap> create(Element* element = 0) { return adoptRef(new NamedMappedAttrMap(element)); }

    void clearClass() { m_classNames.clear(); }
    void setClass(const String&);
    const ClassNames& classNames() const { return m_classNames; }

    bool hasMappedAttributes() const { return m_mappedAttributeCount > 0; }
    void declRemoved() { m_mappedAttributeCount--; }
    void declAdded() { m_mappedAttributeCount++; }

    bool mapsEquivalent(const NamedMappedAttrMap*) const;

private:
    NamedMappedAttrMap(Element* element) : NamedNodeMap(element), m_mappedAttributeCount(0) { }

    virtual void clearAttributes();
    virtual bool isMappedAttributeMap() const;

    int declCount() const;

    ClassNames m_classNames;
    int m_mappedAttributeCount;
};

} //namespace

#endif
