/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
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
 *
 */

#include "config.h"
#include "TransformOperations.h"

#include "IdentityTransformOperation.h"

namespace WebCore {

TransformOperations::TransformOperations(bool makeIdentity)
{
    if (makeIdentity)
        m_operations.append(IdentityTransformOperation::create());
}

bool TransformOperations::operator==(const TransformOperations& o) const
{
    if (m_operations.size() != o.m_operations.size())
        return false;
        
    unsigned s = m_operations.size();
    for (unsigned i = 0; i < s; i++) {
        if (*m_operations[i] != *o.m_operations[i])
            return false;
    }
    
    return true;
}

bool TransformOperations::operationsMatch(const TransformOperations& other) const
{
    size_t numOperations = operations().size();
    // If the sizes of the function lists don't match, the lists don't match
    if (numOperations != other.operations().size())
        return false;
    
    // If the types of each function are not the same, the lists don't match
    for (size_t i = 0; i < numOperations; ++i) {
        if (!operations()[i]->isSameType(*other.operations()[i]))
            return false;
    }
    return true;
}

} // namespace WebCore
