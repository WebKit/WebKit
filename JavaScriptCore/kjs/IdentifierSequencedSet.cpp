// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2005 Apple Computer, Inc
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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "IdentifierSequencedSet.h"

namespace KJS {

IdentifierSequencedSet::IdentifierSequencedSet()
    : m_vector(0),
      m_vectorLength(0),
      m_vectorCapacity(0)
{
}

void IdentifierSequencedSet::deallocateVector()
{
    for (int i = 0; i < m_vectorLength; ++i) {
        (m_vector + i)->~Identifier();
    }
    fastFree(m_vector);
}

IdentifierSequencedSet::~IdentifierSequencedSet()
{
    deallocateVector();
}

void IdentifierSequencedSet::insert(const Identifier& ident)
{
    if (!m_set.insert(ident.ustring().impl()).second)
        return;
    
    if (m_vectorLength == m_vectorCapacity) {
        m_vectorCapacity = m_vectorCapacity == 0 ? 16 : m_vectorCapacity * 11 / 10;
        Identifier *newVector = reinterpret_cast<Identifier *>(fastMalloc(m_vectorCapacity * sizeof(Identifier)));
        for (int i = 0; i < m_vectorLength; ++i) {
            new (newVector + i) Identifier(m_vector[i]);
        }
        deallocateVector();
        m_vector = newVector;
    }
    new (m_vector + m_vectorLength) Identifier(ident);
    ++m_vectorLength;
}

} // namespace KJS

