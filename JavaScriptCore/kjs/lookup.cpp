// -*- c-basic-offset: 2 -*-
/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "lookup.h"

#include <wtf/Assertions.h>

namespace KJS {

static inline bool keysMatch(const UChar* c, unsigned len, const char* s)
{
  // FIXME: This can run off the end of |s| if |c| has a U+0000 character in it.
  const char* end = s + len;
  for (; s != end; c++, s++)
    if (c->uc != *s)
      return false;
  return *s == 0;
}

static inline const HashEntry* findEntry(const struct HashTable* table, unsigned int hash,
                                         const UChar* c, unsigned int len)
{
  ASSERT(table->type == 3);
    
  const HashEntry* e = &table->entries[hash & table->hashSizeMask];

  if (!e->s)
    return 0;

  do {
    // compare strings
    if (keysMatch(c, len, e->s))
      return e;

    // try next bucket
    e = e->next;
  } while (e);
  return 0;
}

const HashEntry* Lookup::findEntry(const struct HashTable* table, const Identifier& s)
{
  return KJS::findEntry(table, s.ustring().rep()->computedHash(), s.data(), s.size());
}

int Lookup::find(const struct HashTable *table, const UChar *c, unsigned int len)
{
  const HashEntry *entry = KJS::findEntry(table, UString::Rep::computeHash(c, len), c, len);
  if (entry)
    return entry->value.intValue;
  return -1;
}

int Lookup::find(const struct HashTable* table, const Identifier& s)
{
  const HashEntry* entry = KJS::findEntry(table, s.ustring().rep()->computedHash(), s.data(), s.size());
  if (entry)
    return entry->value.intValue;
  return -1;
}

}
