/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "IndexToNameMap.h"

#include "ArgList.h"
#include "JSFunction.h"
#include "identifier.h"

namespace KJS {

// We map indexes in the arguments array to their corresponding argument names. 
// Example: function f(x, y, z): arguments[0] = x, so we map 0 to Identifier("x"). 

// Once we have an argument name, we can get and set the argument's value in the 
// activation object.

// We use Identifier::null to indicate that a given argument's value
// isn't stored in the activation object.

IndexToNameMap::IndexToNameMap(JSFunction* func, const ArgList& args)
    : m_size(args.size())
    , m_map(new Identifier[args.size()])
{
    unsigned i = 0;
    ArgList::const_iterator end = args.end();
    for (ArgList::const_iterator it = args.begin(); it != end; ++i, ++it)
        m_map[i] = func->getParameterName(i); // null if there is no corresponding parameter
}

IndexToNameMap::~IndexToNameMap()
{
    delete [] m_map;
}

bool IndexToNameMap::isMapped(const Identifier& index) const
{
    bool indexIsNumber;
    unsigned indexAsNumber = index.toStrictUInt32(&indexIsNumber);

    if (!indexIsNumber)
        return false;

    if (indexAsNumber >= m_size)
        return false;

    if (m_map[indexAsNumber].isNull())
        return false;

    return true;
}

void IndexToNameMap::unMap(ExecState* exec, const Identifier& index)
{
    bool indexIsNumber;
    unsigned indexAsNumber = index.toStrictUInt32(&indexIsNumber);

    ASSERT(indexIsNumber && indexAsNumber < m_size);

    m_map[indexAsNumber] = exec->propertyNames().nullIdentifier;
}

Identifier& IndexToNameMap::operator[](const Identifier& index)
{
    bool indexIsNumber;
    unsigned indexAsNumber = index.toStrictUInt32(&indexIsNumber);

    ASSERT(indexIsNumber && indexAsNumber < m_size);

    return m_map[indexAsNumber];
}

} // namespace KJS
