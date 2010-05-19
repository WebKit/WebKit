/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef MappedAttribute_h
#define MappedAttribute_h

#include "Attribute.h"

namespace WebCore {

class MappedAttribute : public Attribute {
public:
    static PassRefPtr<MappedAttribute> create(const QualifiedName& name, const AtomicString& value)
    {
        return adoptRef(new MappedAttribute(name, value));
    }
    static PassRefPtr<MappedAttribute> create(const AtomicString& name, const AtomicString& value)
    {
        return adoptRef(new MappedAttribute(name, value));
    }

private:
    MappedAttribute(const QualifiedName& name, const AtomicString& value)
        : Attribute(name, value, true, 0)
    {
    }

    MappedAttribute(const AtomicString& name, const AtomicString& value)
        : Attribute(name, value, true, 0)
    {
    }
};

inline MappedAttribute* toMappedAttribute(Attribute* attribute)
{
    ASSERT(!attribute || attribute->isMappedAttribute());
    return static_cast<MappedAttribute*>(attribute);
}

} //namespace

#endif
