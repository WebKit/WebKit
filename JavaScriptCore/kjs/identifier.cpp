/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2002 Apple Computer, Inc
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
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */

#include "identifier.h"

namespace KJS {

Identifier Identifier::null;

extern const Identifier argumentsPropertyName("arguments");
extern const Identifier calleePropertyName("callee");
extern const Identifier constructorPropertyName("constructor");
extern const Identifier lengthPropertyName("length");
extern const Identifier messagePropertyName("message");
extern const Identifier namePropertyName("name");
extern const Identifier prototypePropertyName("prototype");
extern const Identifier specialPrototypePropertyName("__proto__");
extern const Identifier toLocaleStringPropertyName("toLocaleString");
extern const Identifier toStringPropertyName("toString");
extern const Identifier valueOfPropertyName("valueOf");

bool operator==(const Identifier &a, const char *b)
{
    return a._ustring == b;
}

UString::Rep *Identifier::add(const char *c)
{
  if (!c)
    return &UString::Rep::null;
  int length = strlen(c);
  if (length == 0)
    return &UString::Rep::empty;

  // Here's where we compute a hash and find it or put it in the hash table.
  UChar *d = new UChar[length];
  for (int i = 0; i < length; i++)
    d[i] = c[i];

  UString::Rep *r = new UString::Rep;
  r->dat = d;
  r->len = length;
  r->capacity = length;
  r->rc = 0;
  r->_hash = 0;
  return r;
}

UString::Rep *Identifier::add(const UChar *s, int length)
{
  // Here's where we compute a hash and find it or put it in the hash table.

  UChar *d = new UChar[length];
  for (int i = 0; i < length; i++)
    d[i] = s[i];

  UString::Rep *r = new UString::Rep;
  r->dat = d;
  r->len = length;
  r->capacity = length;
  r->rc = 0;
  r->_hash = 0;
  return r;
}

UString::Rep *Identifier::add(const UString &s)
{
  // Here's where we compute a hash and find it or put it in the hash table.
  // Don't forget to check for the case of a string that's already in the table by looking at capacity.
  
  return s.rep;
}

void Identifier::remove(UString::Rep *)
{
  // Here's where we find the string already in the hash table, and remove it.
}

} // namespace KJS
