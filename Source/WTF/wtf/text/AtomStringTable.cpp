/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include <wtf/text/AtomStringTable.h>

#include <wtf/NeverDestroyed.h>
#include <wtf/text/AtomStringImpl.h>

namespace WTF {

struct AtomStringTableRemovalHashTranslator {
    static unsigned hash(const StringImpl* string) { return string->existingHash(); }
    static bool equal(const AtomStringTable::StringEntry& a, const StringImpl* b) { return a == b; }
};

AtomStringTable& AtomStringTable::singleton()
{
    static LazyNeverDestroyed<AtomStringTable> table;
    static std::once_flag flag;
    std::call_once(flag, [&] {
        table.construct();
    });
    return table;
}

bool AtomStringTable::releaseAndRemoveIfNeeded(AtomStringImpl* string)
{
    ASSERT(string->isAtom());
    auto& table = singleton();
    Locker locker { table.m_lock };

    // Double check that the refcount is still s_refCountIncrement.
    // Because Add() could have added a new reference after the load
    // in StringImpl::deref().
    auto oldRefCount = string->m_refCount.fetch_sub(
        StringImpl::s_refCountIncrement, std::memory_order_acq_rel);

    if (oldRefCount != StringImpl::s_refCountIncrement) {
        // Someone called ref() (via Add()) while we were waiting for the lock.
        // The decrement brought count from N to N-1 where N > 1.
        // The string lives on.
        return false;
    }

    // Last ref truly gone (refcount is now 0). Remove from table.
    if (string->length()) {
        auto iterator = table.m_table.find<AtomStringTableRemovalHashTranslator>(string);
        ASSERT(iterator != table.m_table.end());
        table.m_table.remove(iterator);
    }
    return true; // Caller should destroy.
}

} // namespace WTF
