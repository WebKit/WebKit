/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2011 Apple Inc. All rights reserved.
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
#include "CollectionCache.h"

#include <wtf/PassOwnPtr.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

CollectionCache::CollectionCache()
    : version(0)
{
    reset();
}

void CollectionCache::reset()
{
    current = 0;
    position = 0;
    length = 0;
    hasLength = false;
    elementsArrayPosition = 0;
    idCache.clear();
    nameCache.clear();
    hasNameCache = false;
}

#if !ASSERT_DISABLED
void CollectionCache::checkConsistency()
{
    idCache.checkConsistency();
    nameCache.checkConsistency();
}
#endif

void append(CollectionCache::NodeCacheMap& map, const AtomicString& key, Element* element)
{
    OwnPtr<Vector<Element*> >& vector = map.add(key.impl(), nullptr).first->second;
    if (!vector)
        vector = adoptPtr(new Vector<Element*>);
    vector->append(element);
}

} // namespace WebCore
