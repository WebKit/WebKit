// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003 Apple Computer, Inc.
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
 *  Foundation, Inc., 51 Franklin Steet, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include <stdio.h>
#include <string.h>

#include "lookup.h"

#ifdef HAVE_CONFIG_H
#endif

using namespace KJS;

static inline bool keysMatch(const UChar *c, unsigned len, const char *s)
{
  const char* end = s + len;
  for (; s != end; c++, s++)
    if (c->uc != (unsigned char)*s)
      return false;
  return *s == 0;
}

static inline const HashEntry* findEntry(const struct HashTable *table, unsigned int hash,
                                         const UChar *c, unsigned int len )
{
#ifndef NDEBUG
  if (table->type != 2) {
    fprintf(stderr, "KJS: Unknown hash table version.\n");
    return 0;
  }
#endif
  hash %= table->hashSize;

  const HashEntry *e = &table->entries[hash];

  // empty bucket ?
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

const HashEntry* Lookup::findEntry(const struct HashTable *table,
                                   const Identifier &s )
{
  const HashEntry* entry = ::findEntry(table, s.ustring().rep()->hash(), s.data(), s.size());
  return entry;
}

int Lookup::find(const struct HashTable *table,
		 const UChar *c, unsigned int len)
{
  const HashEntry *entry = ::findEntry(table, UString::Rep::computeHash(c, len), c, len);
  if (entry)
    return entry->value;
  return -1;
}

int Lookup::find(const struct HashTable *table, const Identifier &s)
{
  //printf("looking for:%s\n", s.ascii());
  const HashEntry *entry = ::findEntry(table, s.ustring().rep()->hash(), s.data(), s.size());
  if (entry)
    return entry->value;
  return -1;
}
