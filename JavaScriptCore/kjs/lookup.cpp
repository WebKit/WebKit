// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  $Id$
 */

#include <stdio.h>
#include <string.h>

#include "lookup.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

using namespace KJS;

const HashEntry* Lookup::findEntry( const struct HashTable *table,
                              const UChar *c, unsigned int len )
{
  if (table->type != 2) {
    fprintf(stderr, "KJS: Unknown hash table version.\n");
    return 0;
  }
  char *ascii = new char[len+1];
  unsigned int i;
  for(i = 0; i < len; i++, c++) {
    if (!c->high())
      ascii[i] = c->low();
    else
      break;
  }
  ascii[i] = '\0';

  int h = hash(ascii) % table->hashSize;
  const HashEntry *e = &table->entries[h];

  // empty bucket ?
  if (!e->s) {
    delete [] ascii;
    return 0;
  }

  do {
    // compare strings
    if (strcmp(ascii, e->s) == 0) {
      delete [] ascii;
      return e;
    }
    // try next bucket
    e = e->next;
  } while (e);

  delete [] ascii;
  return 0;
}

const HashEntry* Lookup::findEntry( const struct HashTable *table,
                                const UString &s )
{
  return findEntry( table, s.data(), s.size() );
}

int Lookup::find(const struct HashTable *table,
		 const UChar *c, unsigned int len)
{
  const HashEntry *entry = findEntry( table, c, len );
  if (entry)
    return entry->value;
  return -1;
}

int Lookup::find(const struct HashTable *table, const UString &s)
{
  return find(table, s.data(), s.size());
}

unsigned int Lookup::hash(const UChar *c, unsigned int len)
{
  unsigned int val = 0;
  // ignoring higher byte
  for (unsigned int i = 0; i < len; i++, c++)
    val += c->low();

  return val;
}

unsigned int Lookup::hash(const UString &key)
{
  return hash(key.data(), key.size());
}

unsigned int Lookup::hash(const char *s)
{
  unsigned int val = 0;
  while (*s)
    val += *s++;

  return val;
}
