/*
 *  Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef KXMLCORE_HASH_FORWARD_H
#define KXMLCORE_HASH_FORWARD_H

#include "HashTraits.h"

namespace KXMLCore {
    template<typename Value, typename HashFunctions = typename DefaultHash<Value>::Hash,
        typename Traits = HashTraits<Value> > class HashCountedSet;
    template<typename Key, typename Mapped, typename Hash = typename DefaultHash<Key>::Hash,
        typename KeyTraits = HashTraits<Key>, typename MappedTraits = HashTraits<Mapped> > class HashMap;
    template<typename Value, typename Hash = typename DefaultHash<Value>::Hash,
        typename Traits = HashTraits<Value> > class HashSet;
}

using KXMLCore::HashCountedSet;
using KXMLCore::HashMap;
using KXMLCore::HashSet;

#endif
