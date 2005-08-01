// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004 Apple Computer, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "fast_malloc.h"
#include "ustring.h"
#include "operations.h"
#include "identifier.h"
#include <math.h>
#include "dtoa.h"

#include <algorithm>

using std::max;

#include <unicode/uchar.h>

namespace KJS {

extern const double NaN;
extern const double Inf;

CString::CString(const char *c)
{
  length = strlen(c);
  data = new char[length+1];
  memcpy(data, c, length + 1);
}

CString::CString(const char *c, int len)
{
  length = len;
  data = new char[len+1];
  memcpy(data, c, len);
  data[len] = 0;
}

CString::CString(const CString &b)
{
  length = b.length;
  if (length > 0 && b.data) {
    data = new char[length+1];
    memcpy(data, b.data, length + 1);
  }
  else {
    data = 0;
  }
}

CString::~CString()
{
  delete [] data;
}

CString &CString::append(const CString &t)
{
  char *n;
  n = new char[length+t.length+1];
  if (length)
    memcpy(n, data, length);
  if (t.length)
    memcpy(n+length, t.data, t.length);
  length += t.length;
  n[length] = 0;

  delete [] data;
  data = n;

  return *this;
}

CString &CString::operator=(const char *c)
{
  if (data)
    delete [] data;
  length = strlen(c);
  data = new char[length+1];
  memcpy(data, c, length + 1);

  return *this;
}

CString &CString::operator=(const CString &str)
{
  if (this == &str)
    return *this;

  if (data)
    delete [] data;
  length = str.length;
  if (length > 0 && str.data) {
    data = new char[length + 1];
    memcpy(data, str.data, length + 1);
  }
  else {
    data = 0;
  }

  return *this;
}

bool operator==(const CString& c1, const CString& c2)
{
  int len = c1.size();
  return len == c2.size() && (len == 0 || memcmp(c1.c_str(), c2.c_str(), len) == 0);
}

// Hack here to avoid a global with a constructor; point to an unsigned short instead of a UChar.
static unsigned short almostUChar;
static UChar *const nonNullUCharPointer = reinterpret_cast<UChar *>(&almostUChar);
UString::Rep UString::Rep::null = { 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
UString::Rep UString::Rep::empty = { 0, 0, 1, 0, 0, 0, nonNullUCharPointer, 0, 0, 0, 0 };
const int normalStatBufferSize = 4096;
static char *statBuffer = 0;
static int statBufferSize = 0;

UChar UChar::toLower() const
{
  return static_cast<unsigned short>(u_tolower(uc));
}

UChar UChar::toUpper() const
{
  return static_cast<unsigned short>(u_toupper(uc));
}

UCharReference& UCharReference::operator=(UChar c)
{
  str->detach();
  if (offset < str->rep->len)
    *(str->rep->data() + offset) = c;
  /* TODO: lengthen string ? */
  return *this;
}

UChar& UCharReference::ref() const
{
  if (offset < str->rep->len)
    return *(str->rep->data() + offset);
  else {
    static UChar callerBetterNotModifyThis('\0');
    return callerBetterNotModifyThis;
  }
}

UString::Rep *UString::Rep::createCopying(const UChar *d, int l)
{
  int sizeInBytes = l * sizeof(UChar);
  UChar *copyD = static_cast<UChar *>(kjs_fast_malloc(sizeInBytes));
  memcpy(copyD, d, sizeInBytes);

  return create(copyD, l);
}

UString::Rep *UString::Rep::create(UChar *d, int l)
{
  Rep *r = new Rep;
  r->offset = 0;
  r->len = l;
  r->rc = 1;
  r->_hash = 0;
  r->isIdentifier = 0;
  r->baseString = 0;
  r->buf = d;
  r->usedCapacity = l;
  r->capacity = l;
  r->usedPreCapacity = 0;
  r->preCapacity = 0;
  return r;
}

UString::Rep *UString::Rep::create(Rep *base, int offset, int length)
{
  assert(base);

  int baseOffset = base->offset;

  if (base->baseString) {
    base = base->baseString;
  }

  assert(-(offset + baseOffset) <= base->usedPreCapacity);
  assert(offset + baseOffset + length <= base->usedCapacity);

  Rep *r = new Rep;
  r->offset = baseOffset + offset;
  r->len = length;
  r->rc = 1;
  r->_hash = 0;
  r->isIdentifier = 0;
  r->baseString = base;
  base->ref();
  r->buf = 0;
  r->usedCapacity = 0;
  r->capacity = 0;
  r->usedPreCapacity = 0;
  r->preCapacity = 0;
  return r;
}

void UString::Rep::destroy()
{
  if (isIdentifier)
    Identifier::remove(this);
  if (baseString) {
    baseString->deref();
  } else {
    kjs_fast_free(buf);
  }
  delete this;
}

// Golden ratio - arbitrary start value to avoid mapping all 0's to all 0's
// or anything like that.
const unsigned PHI = 0x9e3779b9U;

// Paul Hsieh's SuperFastHash
// http://www.azillionmonkeys.com/qed/hash.html
unsigned UString::Rep::computeHash(const UChar *s, int len)
{
  unsigned l = len;
  uint32_t hash = PHI;
  uint32_t tmp;

  int rem = l & 1;
  l >>= 1;

  // Main loop
  for (; l > 0; l--) {
    hash += s[0].uc;
    tmp = (s[1].uc << 11) ^ hash;
    hash = (hash << 16) ^ tmp;
    s += 2;
    hash += hash >> 11;
  }

  // Handle end case
  if (rem) {
    hash += s[0].uc;
    hash ^= hash << 11;
    hash += hash >> 17;
  }

  // Force "avalanching" of final 127 bits
  hash ^= hash << 3;
  hash += hash >> 5;
  hash ^= hash << 2;
  hash += hash >> 15;
  hash ^= hash << 10;

  // this avoids ever returning a hash code of 0, since that is used to
  // signal "hash not computed yet", using a value that is likely to be
  // effectively the same as 0 when the low bits are masked
  if (hash == 0)
    hash = 0x80000000;

  return hash;
}

// Paul Hsieh's SuperFastHash
// http://www.azillionmonkeys.com/qed/hash.html
unsigned UString::Rep::computeHash(const char *s)
{
  // This hash is designed to work on 16-bit chunks at a time. But since the normal case
  // (above) is to hash UTF-16 characters, we just treat the 8-bit chars as if they
  // were 16-bit chunks, which should give matching results

  uint32_t hash = PHI;
  uint32_t tmp;
  unsigned l = strlen(s);
  
  int rem = l & 1;
  l >>= 1;

  // Main loop
  for (; l > 0; l--) {
    hash += (unsigned char)s[0];
    tmp = ((unsigned char)s[1] << 11) ^ hash;
    hash = (hash << 16) ^ tmp;
    s += 2;
    hash += hash >> 11;
  }

  // Handle end case
  if (rem) {
    hash += (unsigned char)s[0];
    hash ^= hash << 11;
    hash += hash >> 17;
  }

  // Force "avalanching" of final 127 bits
  hash ^= hash << 3;
  hash += hash >> 5;
  hash ^= hash << 2;
  hash += hash >> 15;
  hash ^= hash << 10;
  
  // this avoids ever returning a hash code of 0, since that is used to
  // signal "hash not computed yet", using a value that is likely to be
  // effectively the same as 0 when the low bits are masked
  if (hash == 0)
    hash = 0x80000000;

  return hash;
}

// put these early so they can be inlined
inline int UString::expandedSize(int size, int otherSize) const
{
  int s = (size * 11 / 10) + 1 + otherSize;
  return s;
}

inline int UString::usedCapacity() const
{
  return rep->baseString ? rep->baseString->usedCapacity : rep->usedCapacity;
}

inline int UString::usedPreCapacity() const
{
  return rep->baseString ? rep->baseString->usedPreCapacity : rep->usedPreCapacity;
}

void UString::expandCapacity(int requiredLength)
{
  Rep *r = rep->baseString ? rep->baseString : rep;

  if (requiredLength > r->capacity) {
    int newCapacity = expandedSize(requiredLength, r->preCapacity);
    r->buf = static_cast<UChar *>(kjs_fast_realloc(r->buf, newCapacity * sizeof(UChar)));
    r->capacity = newCapacity - r->preCapacity;
  }
  if (requiredLength > r->usedCapacity) {
    r->usedCapacity = requiredLength;
  }
}

void UString::expandPreCapacity(int requiredPreCap)
{
  Rep *r = rep->baseString ? rep->baseString : rep;

  if (requiredPreCap > r->preCapacity) {
    int newCapacity = expandedSize(requiredPreCap, r->capacity);
    int delta = newCapacity - r->capacity - r->preCapacity;

    UChar *newBuf = static_cast<UChar *>(kjs_fast_malloc(newCapacity * sizeof(UChar)));
    memcpy(newBuf + delta, r->buf, (r->capacity + r->preCapacity) * sizeof(UChar));
    kjs_fast_free(r->buf);
    r->buf = newBuf;

    r->preCapacity = newCapacity - r->capacity;
  }
  if (requiredPreCap > r->usedPreCapacity) {
    r->usedPreCapacity = requiredPreCap;
  }
}


UString::UString()
{
  attach(&Rep::null);
}

UString::UString(char c)
{
    UChar *d = static_cast<UChar *>(kjs_fast_malloc(sizeof(UChar)));
    d[0] = c;
    rep = Rep::create(d, 1);
}

UString::UString(const char *c)
{
  if (!c) {
    attach(&Rep::null);
    return;
  }
  int length = strlen(c);
  if (length == 0) {
    attach(&Rep::empty);
    return;
  }
  UChar *d = static_cast<UChar *>(kjs_fast_malloc(sizeof(UChar) * length));
  for (int i = 0; i < length; i++)
    d[i].uc = c[i];
  rep = Rep::create(d, length);
}

UString::UString(const UChar *c, int length)
{
  if (length == 0) {
    attach(&Rep::empty);
    return;
  }
  rep = Rep::createCopying(c, length);
}

UString::UString(UChar *c, int length, bool copy)
{
  if (length == 0) {
    attach(&Rep::empty);
    return;
  }
  if (copy) {
    rep = Rep::createCopying(c, length);
  } else {
    rep = Rep::create(c, length);
  }
}

UString::UString(const UString &a, const UString &b)
{
  int aSize = a.size();
  int aOffset = a.rep->offset;
  int bSize = b.size();
  int bOffset = b.rep->offset;
  int length = aSize + bSize;

  // possible cases:
 
  if (aSize == 0) {
    // a is empty
    attach(b.rep);
  } else if (bSize == 0) {
    // b is empty
    attach(a.rep);
  } else if (aOffset + aSize == a.usedCapacity() && 4 * aSize >= bSize &&
	     (-bOffset != b.usedPreCapacity() || aSize >= bSize)) {
    // - a reaches the end of its buffer so it qualifies for shared append
    // - also, it's at least a quarter the length of b - appending to a much shorter
    //   string does more harm than good
    // - however, if b qualifies for prepend and is longer than a, we'd rather prepend
    UString x(a);
    x.expandCapacity(aOffset + length);
    memcpy(const_cast<UChar *>(a.data() + aSize), b.data(), bSize * sizeof(UChar));
    rep = Rep::create(a.rep, 0, length);
  } else if (-bOffset == b.usedPreCapacity() && 4 * bSize >= aSize) {
    // - b reaches the beginning of its buffer so it qualifies for shared prepend
    // - also, it's at least a quarter the length of a - prepending to a much shorter
    //   string does more harm than good
    UString y(b);
    y.expandPreCapacity(-bOffset + aSize);
    memcpy(const_cast<UChar *>(b.data() - aSize), a.data(), aSize * sizeof(UChar));
    rep = Rep::create(b.rep, -aSize, length);
  } else {
    // a does not qualify for append, and b does not qualify for prepend, gotta make a whole new string
    int newCapacity = expandedSize(length, 0);
    UChar *d = static_cast<UChar *>(kjs_fast_malloc(sizeof(UChar) * newCapacity));
    memcpy(d, a.data(), aSize * sizeof(UChar));
    memcpy(d + aSize, b.data(), bSize * sizeof(UChar));
    rep = Rep::create(d, length);
    rep->capacity = newCapacity;
  }
}

const UString &UString::null()
{
  static UString n;
  return n;
}

UString UString::from(int i)
{
  return from((long)i);
}

UString UString::from(unsigned int u)
{
  UChar buf[20];
  UChar *end = buf + 20;
  UChar *p = end;
  
  if (u == 0) {
    *--p = '0';
  } else {
    while (u) {
      *--p = (unsigned short)((u % 10) + '0');
      u /= 10;
    }
  }
  
  return UString(p, end - p);
}

UString UString::from(long l)
{
  UChar buf[20];
  UChar *end = buf + 20;
  UChar *p = end;
  
  if (l == 0) {
    *--p = '0';
  } else if (l == LONG_MIN) {
    char minBuf[20];
    sprintf(minBuf, "%ld", LONG_MIN);
    return UString(minBuf);
  } else {
    bool negative = false;
    if (l < 0) {
      negative = true;
      l = -l;
    }
    while (l) {
      *--p = (unsigned short)((l % 10) + '0');
      l /= 10;
    }
    if (negative) {
      *--p = '-';
    }
  }
  
  return UString(p, end - p);
}

UString UString::from(double d)
{
  char buf[80];
  int decimalPoint;
  int sign;
  
  char *result = kjs_dtoa(d, 0, 0, &decimalPoint, &sign, NULL);
  int length = strlen(result);
  
  int i = 0;
  if (sign) {
    buf[i++] = '-';
  }
  
  if (decimalPoint <= 0 && decimalPoint > -6) {
    buf[i++] = '0';
    buf[i++] = '.';
    for (int j = decimalPoint; j < 0; j++) {
      buf[i++] = '0';
    }
    strcpy(buf + i, result);
  } else if (decimalPoint <= 21 && decimalPoint > 0) {
    if (length <= decimalPoint) {
      strcpy(buf + i, result);
      i += length;
      for (int j = 0; j < decimalPoint - length; j++) {
	buf[i++] = '0';
      }
      buf[i] = '\0';
    } else {
      strncpy(buf + i, result, decimalPoint);
      i += decimalPoint;
      buf[i++] = '.';
      strcpy(buf + i, result + decimalPoint);
    }
  } else if (result[0] < '0' || result[0] > '9') {
    strcpy(buf + i, result);
  } else {
    buf[i++] = result[0];
    if (length > 1) {
      buf[i++] = '.';
      strcpy(buf + i, result + 1);
      i += length - 1;
    }
    
    buf[i++] = 'e';
    buf[i++] = (decimalPoint >= 0) ? '+' : '-';
    // decimalPoint can't be more than 3 digits decimal given the
    // nature of float representation
    int exponential = decimalPoint - 1;
    if (exponential < 0) {
      exponential = exponential * -1;
    }
    if (exponential >= 100) {
      buf[i++] = '0' + exponential / 100;
    }
    if (exponential >= 10) {
      buf[i++] = '0' + (exponential % 100) / 10;
    }
    buf[i++] = '0' + exponential % 10;
    buf[i++] = '\0';
  }
  
  kjs_freedtoa(result);
  
  return UString(buf);
}

UString UString::spliceSubstringsWithSeparators(const Range *substringRanges, int rangeCount, const UString *separators, int separatorCount) const
{
  int totalLength = 0;

  for (int i = 0; i < rangeCount; i++) {
    totalLength += substringRanges[i].length;
  }
  for (int i = 0; i < separatorCount; i++) {
    totalLength += separators[i].size();
  }

  UChar *buffer = static_cast<UChar *>(kjs_fast_malloc(totalLength * sizeof(UChar)));

  int maxCount = max(rangeCount, separatorCount);
  int bufferPos = 0;
  for (int i = 0; i < maxCount; i++) {
    if (i < rangeCount) {
      memcpy(buffer + bufferPos, data() + substringRanges[i].position, substringRanges[i].length * sizeof(UChar));
      bufferPos += substringRanges[i].length;
    }
    if (i < separatorCount) {
      memcpy(buffer + bufferPos, separators[i].data(), separators[i].size() * sizeof(UChar));
      bufferPos += separators[i].size();
    }
  }

  UString::Rep *rep = UString::Rep::create(buffer, totalLength);
  UString result = UString(rep);
  rep->deref();

  return result;
}



UString &UString::append(const UString &t)
{
  int thisSize = size();
  int thisOffset = rep->offset;
  int tSize = t.size();
  int length = thisSize + tSize;

  // possible cases:
  if (thisSize == 0) {
    // this is empty
    *this = t;
  } else if (tSize == 0) {
    // t is empty
  } else if (!rep->baseString && rep->rc == 1) {
    // this is direct and has refcount of 1 (so we can just alter it directly)
    expandCapacity(thisOffset + length);
    memcpy(const_cast<UChar *>(data() + thisSize), t.data(), tSize * sizeof(UChar));
    rep->len = length;
    rep->_hash = 0;
  } else if (thisOffset + thisSize == usedCapacity()) {
    // this reaches the end of the buffer - extend it
    expandCapacity(thisOffset + length);
    memcpy(const_cast<UChar *>(data() + thisSize), t.data(), tSize * sizeof(UChar));
    Rep *newRep = Rep::create(rep, 0, length);
    release();
    rep = newRep;
  } else {
    // this is shared with someone using more capacity, gotta make a whole new string
    int newCapacity = expandedSize(length, 0);
    UChar *d = static_cast<UChar *>(kjs_fast_malloc(sizeof(UChar) * newCapacity));
    memcpy(d, data(), thisSize * sizeof(UChar));
    memcpy(const_cast<UChar *>(d + thisSize), t.data(), tSize * sizeof(UChar));
    release();
    rep = Rep::create(d, length);
    rep->capacity = newCapacity;
  }

  return *this;
}

UString &UString::append(const char *t)
{
  int thisSize = size();
  int thisOffset = rep->offset;
  int tSize = strlen(t);
  int length = thisSize + tSize;

  // possible cases:
  if (thisSize == 0) {
    // this is empty
    *this = t;
  } else if (tSize == 0) {
    // t is empty, we'll just return *this below.
  } else if (!rep->baseString && rep->rc == 1) {
    // this is direct and has refcount of 1 (so we can just alter it directly)
    expandCapacity(thisOffset + length);
    UChar *d = const_cast<UChar *>(data());
    for (int i = 0; i < tSize; ++i)
      d[thisSize+i] = t[i];
    rep->len = length;
    rep->_hash = 0;
  } else if (thisOffset + thisSize == usedCapacity()) {
    // this string reaches the end of the buffer - extend it
    expandCapacity(thisOffset + length);
    UChar *d = const_cast<UChar *>(data());
    for (int i = 0; i < tSize; ++i)
      d[thisSize+i] = t[i];
    Rep *newRep = Rep::create(rep, 0, length);
    release();
    rep = newRep;
  } else {
    // this is shared with someone using more capacity, gotta make a whole new string
    int newCapacity = expandedSize(length, 0);
    UChar *d = static_cast<UChar *>(kjs_fast_malloc(sizeof(UChar) * newCapacity));
    memcpy(d, data(), thisSize * sizeof(UChar));
    for (int i = 0; i < tSize; ++i)
      d[thisSize+i] = t[i];
    release();
    rep = Rep::create(d, length);
    rep->capacity = newCapacity;
  }

  return *this;
}

UString &UString::append(unsigned short c)
{
  int thisOffset = rep->offset;
  int length = size();

  // possible cases:
  if (length == 0) {
    // this is empty - must make a new rep because we don't want to pollute the shared empty one 
    int newCapacity = expandedSize(1, 0);
    UChar *d = static_cast<UChar *>(kjs_fast_malloc(sizeof(UChar) * newCapacity));
    d[0] = c;
    release();
    rep = Rep::create(d, 1);
    rep->capacity = newCapacity;
  } else if (!rep->baseString && rep->rc == 1) {
    // this is direct and has refcount of 1 (so we can just alter it directly)
    expandCapacity(thisOffset + length + 1);
    UChar *d = const_cast<UChar *>(data());
    d[length] = c;
    rep->len = length + 1;
    rep->_hash = 0;
  } else if (thisOffset + length == usedCapacity()) {
    // this reaches the end of the string - extend it and share
    expandCapacity(thisOffset + length + 1);
    UChar *d = const_cast<UChar *>(data());
    d[length] = c;
    Rep *newRep = Rep::create(rep, 0, length + 1);
    release();
    rep = newRep;
  } else {
    // this is shared with someone using more capacity, gotta make a whole new string
    int newCapacity = expandedSize((length + 1), 0);
    UChar *d = static_cast<UChar *>(kjs_fast_malloc(sizeof(UChar) * newCapacity));
    memcpy(d, data(), length * sizeof(UChar));
    d[length] = c;
    release();
    rep = Rep::create(d, length);
    rep->capacity = newCapacity;
  }

  return *this;
}

CString UString::cstring() const
{
  return ascii();
}

char *UString::ascii() const
{
  // Never make the buffer smaller than normalStatBufferSize.
  // Thus we almost never need to reallocate.
  int length = size();
  int neededSize = length + 1;
  if (neededSize < normalStatBufferSize) {
    neededSize = normalStatBufferSize;
  }
  if (neededSize != statBufferSize) {
    delete [] statBuffer;
    statBuffer = new char [neededSize];
    statBufferSize = neededSize;
  }
  
  const UChar *p = data();
  char *q = statBuffer;
  const UChar *limit = p + length;
  while (p != limit) {
    *q = p->uc;
    ++p;
    ++q;
  }
  *q = '\0';

  return statBuffer;
}

#ifdef KJS_DEBUG_MEM
void UString::globalClear()
{
  delete [] statBuffer;
  statBuffer = 0;
  statBufferSize = 0;
}
#endif

UString &UString::operator=(const char *c)
{
  int l = c ? strlen(c) : 0;
  UChar *d;
  if (rep->rc == 1 && l <= rep->capacity && !rep->baseString && rep->offset == 0 && rep->preCapacity == 0) {
    d = rep->buf;
    rep->_hash = 0;
  } else {
    release();
    d = static_cast<UChar *>(kjs_fast_malloc(sizeof(UChar) * l));
    rep = Rep::create(d, l);
  }
  for (int i = 0; i < l; i++)
    d[i].uc = c[i];

  return *this;
}

UString &UString::operator=(const UString &str)
{
  str.rep->ref();
  release();
  rep = str.rep;

  return *this;
}

bool UString::is8Bit() const
{
  const UChar *u = data();
  const UChar *limit = u + size();
  while (u < limit) {
    if (u->uc > 0xFF)
      return false;
    ++u;
  }

  return true;
}

UChar UString::operator[](int pos) const
{
  if (pos >= size())
    return '\0';
  return data()[pos];
}

UCharReference UString::operator[](int pos)
{
  /* TODO: boundary check */
  return UCharReference(this, pos);
}

double UString::toDouble(bool tolerateTrailingJunk, bool tolerateEmptyString) const
{
  double d;

  // FIXME: If tolerateTrailingJunk is true, then we want to tolerate non-8-bit junk
  // after the number, so is8Bit is too strict a check.
  if (!is8Bit())
    return NaN;

  const char *c = ascii();

  // skip leading white space
  while (isspace(*c))
    c++;

  // empty string ?
  if (*c == '\0')
    return tolerateEmptyString ? 0.0 : NaN;

  // hex number ?
  if (*c == '0' && (*(c+1) == 'x' || *(c+1) == 'X')) {
    c++;
    d = 0.0;
    while (*(++c)) {
      if (*c >= '0' && *c <= '9')
	d = d * 16.0 + *c - '0';
      else if ((*c >= 'A' && *c <= 'F') || (*c >= 'a' && *c <= 'f'))
	d = d * 16.0 + (*c & 0xdf) - 'A' + 10.0;
      else
	break;
    }
  } else {
    // regular number ?
    char *end;
    d = kjs_strtod(c, &end);
    if ((d != 0.0 || end != c) && d != HUGE_VAL && d != -HUGE_VAL) {
      c = end;
    } else {
      // infinity ?
      d = 1.0;
      if (*c == '+')
	c++;
      else if (*c == '-') {
	d = -1.0;
	c++;
      }
      if (strncmp(c, "Infinity", 8) != 0)
	return NaN;
      d = d * Inf;
      c += 8;
    }
  }

  // allow trailing white space
  while (isspace(*c))
    c++;
  // don't allow anything after - unless tolerant=true
  if (!tolerateTrailingJunk && *c != '\0')
    d = NaN;

  return d;
}

double UString::toDouble(bool tolerateTrailingJunk) const
{
  return toDouble(tolerateTrailingJunk, true);
}

double UString::toDouble() const
{
  return toDouble(false, true);
}

unsigned long UString::toULong(bool *ok, bool tolerateEmptyString) const
{
  double d = toDouble(false, tolerateEmptyString);
  bool b = true;

  if (isNaN(d) || d != static_cast<unsigned long>(d)) {
    b = false;
    d = 0;
  }

  if (ok)
    *ok = b;

  return static_cast<unsigned long>(d);
}

unsigned long UString::toULong(bool *ok) const
{
  return toULong(ok, true);
}

uint32_t UString::toUInt32(bool *ok) const
{
  double d = toDouble();
  bool b = true;

  if (isNaN(d) || d != static_cast<uint32_t>(d)) {
    b = false;
    d = 0;
  }

  if (ok)
    *ok = b;

  return static_cast<uint32_t>(d);
}

uint32_t UString::toStrictUInt32(bool *ok) const
{
  if (ok)
    *ok = false;

  // Empty string is not OK.
  int len = rep->len;
  if (len == 0)
    return 0;
  const UChar *p = rep->data();
  unsigned short c = p->unicode();

  // If the first digit is 0, only 0 itself is OK.
  if (c == '0') {
    if (len == 1 && ok)
      *ok = true;
    return 0;
  }
  
  // Convert to UInt32, checking for overflow.
  uint32_t i = 0;
  while (1) {
    // Process character, turning it into a digit.
    if (c < '0' || c > '9')
      return 0;
    const unsigned d = c - '0';
    
    // Multiply by 10, checking for overflow out of 32 bits.
    if (i > 0xFFFFFFFFU / 10)
      return 0;
    i *= 10;
    
    // Add in the digit, checking for overflow out of 32 bits.
    const unsigned max = 0xFFFFFFFFU - d;
    if (i > max)
        return 0;
    i += d;
    
    // Handle end of string.
    if (--len == 0) {
      if (ok)
        *ok = true;
      return i;
    }
    
    // Get next character.
    c = (++p)->unicode();
  }
}

// Rule from ECMA 15.2 about what an array index is.
// Must exactly match string form of an unsigned integer, and be less than 2^32 - 1.
unsigned UString::toArrayIndex(bool *ok) const
{
  unsigned i = toStrictUInt32(ok);
  if (i >= 0xFFFFFFFFU && ok)
    *ok = false;
  return i;
}

int UString::find(const UString &f, int pos) const
{
  int sz = size();
  int fsz = f.size();
  if (sz < fsz)
    return -1;
  if (pos < 0)
    pos = 0;
  if (fsz == 0)
    return pos;
  const UChar *end = data() + sz - fsz;
  long fsizeminusone = (fsz - 1) * sizeof(UChar);
  const UChar *fdata = f.data();
  unsigned short fchar = fdata->uc;
  ++fdata;
  for (const UChar *c = data() + pos; c <= end; c++)
    if (c->uc == fchar && !memcmp(c + 1, fdata, fsizeminusone))
      return (c-data());

  return -1;
}

int UString::find(UChar ch, int pos) const
{
  if (pos < 0)
    pos = 0;
  const UChar *end = data() + size();
  for (const UChar *c = data() + pos; c < end; c++)
    if (*c == ch)
      return (c-data());

  return -1;
}

int UString::rfind(const UString &f, int pos) const
{
  int sz = size();
  int fsz = f.size();
  if (sz < fsz)
    return -1;
  if (pos < 0)
    pos = 0;
  if (pos > sz - fsz)
    pos = sz - fsz;
  if (fsz == 0)
    return pos;
  long fsizeminusone = (fsz - 1) * sizeof(UChar);
  const UChar *fdata = f.data();
  for (const UChar *c = data() + pos; c >= data(); c--) {
    if (*c == *fdata && !memcmp(c + 1, fdata + 1, fsizeminusone))
      return (c-data());
  }

  return -1;
}

int UString::rfind(UChar ch, int pos) const
{
  if (isEmpty())
    return -1;
  if (pos + 1 >= size())
    pos = size() - 1;
  for (const UChar *c = data() + pos; c >= data(); c--) {
    if (*c == ch)
      return (c-data());
  }

  return -1;
}

UString UString::substr(int pos, int len) const
{
  int s = size();

  if (pos < 0)
    pos = 0;
  else if (pos >= s)
    pos = s;
  if (len < 0)
    len = s;
  if (pos + len >= s)
    len = s - pos;

  if (pos == 0 && len == s)
    return *this;

  UString::Rep *newRep = Rep::create(rep, pos, len);
  UString result(newRep);
  newRep->deref();

  return result;
}

void UString::detach()
{
  if (rep->rc > 1 || rep->baseString) {
    int l = size();
    UChar *n = static_cast<UChar *>(kjs_fast_malloc(sizeof(UChar) * l));
    memcpy(n, data(), l * sizeof(UChar));
    release();
    rep = Rep::create(n, l);
  }
}

bool operator==(const UString& s1, const UString& s2)
{
  if (s1.rep->len != s2.rep->len)
    return false;

  return (memcmp(s1.rep->data(), s2.rep->data(),
		 s1.rep->len * sizeof(UChar)) == 0);
}

bool operator==(const UString& s1, const char *s2)
{
  if (s2 == 0) {
    return s1.isEmpty();
  }

  const UChar *u = s1.data();
  const UChar *uend = u + s1.size();
  while (u != uend && *s2) {
    if (u->uc != (unsigned char)*s2)
      return false;
    s2++;
    u++;
  }

  return u == uend && *s2 == 0;
}

bool operator<(const UString& s1, const UString& s2)
{
  const int l1 = s1.size();
  const int l2 = s2.size();
  const int lmin = l1 < l2 ? l1 : l2;
  const UChar *c1 = s1.data();
  const UChar *c2 = s2.data();
  int l = 0;
  while (l < lmin && *c1 == *c2) {
    c1++;
    c2++;
    l++;
  }
  if (l < lmin)
    return (c1->uc < c2->uc);

  return (l1 < l2);
}

int compare(const UString& s1, const UString& s2)
{
  const int l1 = s1.size();
  const int l2 = s2.size();
  const int lmin = l1 < l2 ? l1 : l2;
  const UChar *c1 = s1.data();
  const UChar *c2 = s2.data();
  int l = 0;
  while (l < lmin && *c1 == *c2) {
    c1++;
    c2++;
    l++;
  }
  if (l < lmin)
    return (c1->uc > c2->uc) ? 1 : -1;

  if (l1 == l2) {
    return 0;
  }
  return (l1 < l2) ? 1 : -1;
}

inline int inlineUTF8SequenceLengthNonASCII(char b0)
{
  if ((b0 & 0xC0) != 0xC0)
    return 0;
  if ((b0 & 0xE0) == 0xC0)
    return 2;
  if ((b0 & 0xF0) == 0xE0)
    return 3;
  if ((b0 & 0xF8) == 0xF0)
    return 4;
  return 0;
}

int UTF8SequenceLengthNonASCII(char b0)
{
  return inlineUTF8SequenceLengthNonASCII(b0);
}

inline int inlineUTF8SequenceLength(char b0)
{
  return (b0 & 0x80) == 0 ? 1 : UTF8SequenceLengthNonASCII(b0);
}

// Given a first byte, gives the length of the UTF-8 sequence it begins.
// Returns 0 for bytes that are not legal starts of UTF-8 sequences.
// Only allows sequences of up to 4 bytes, since that works for all Unicode characters (U-00000000 to U-0010FFFF).
int UTF8SequenceLength(char b0)
{
  return (b0 & 0x80) == 0 ? 1 : inlineUTF8SequenceLengthNonASCII(b0);
}

// Takes a null-terminated C-style string with a UTF-8 sequence in it and converts it to a character.
// Only allows Unicode characters (U-00000000 to U-0010FFFF).
// Returns -1 if the sequence is not valid (including presence of extra bytes).
int decodeUTF8Sequence(const char *sequence)
{
  // Handle 0-byte sequences (never valid).
  const unsigned char b0 = sequence[0];
  const int length = inlineUTF8SequenceLength(b0);
  if (length == 0)
    return -1;

  // Handle 1-byte sequences (plain ASCII).
  const unsigned char b1 = sequence[1];
  if (length == 1) {
    if (b1)
      return -1;
    return b0;
  }

  // Handle 2-byte sequences.
  if ((b1 & 0xC0) != 0x80)
    return -1;
  const unsigned char b2 = sequence[2];
  if (length == 2) {
    if (b2)
      return -1;
    const int c = ((b0 & 0x1F) << 6) | (b1 & 0x3F);
    if (c < 0x80)
      return -1;
    return c;
  }

  // Handle 3-byte sequences.
  if ((b2 & 0xC0) != 0x80)
    return -1;
  const unsigned char b3 = sequence[3];
  if (length == 3) {
    if (b3)
      return -1;
    const int c = ((b0 & 0xF) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
    if (c < 0x800)
      return -1;
    // UTF-16 surrogates should never appear in UTF-8 data.
    if (c >= 0xD800 && c <= 0xDFFF)
      return -1;
    // Backwards BOM and U+FFFF should never appear in UTF-8 data.
    if (c == 0xFFFE || c == 0xFFFF)
      return -1;
    return c;
  }

  // Handle 4-byte sequences.
  if ((b3 & 0xC0) != 0x80)
    return -1;
  const unsigned char b4 = sequence[4];
  if (length == 4) {
    if (b4)
      return -1;
    const int c = ((b0 & 0x7) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
    if (c < 0x10000 || c > 0x10FFFF)
      return -1;
    return c;
  }

  return -1;
}

CString UString::UTF8String() const
{
  // Allocate a buffer big enough to hold all the characters.
  const int length = size();
  const unsigned bufferSize = length * 3;
  char fixedSizeBuffer[1024];
  char *buffer;
  if (bufferSize > sizeof(fixedSizeBuffer)) {
    buffer = new char [bufferSize];
  } else {
    buffer = fixedSizeBuffer;
  }

  // Convert to runs of 8-bit characters.
  char *p = buffer;
  const UChar *d = data();
  for (int i = 0; i != length; ++i) {
    unsigned short c = d[i].unicode();
    if (c < 0x80) {
      *p++ = (char)c;
    } else if (c < 0x800) {
      *p++ = (char)((c >> 6) | 0xC0); // C0 is the 2-byte flag for UTF-8
      *p++ = (char)((c | 0x80) & 0xBF); // next 6 bits, with high bit set
    } else if (c >= 0xD800 && c <= 0xDBFF && i < length && d[i+1].uc >= 0xDC00 && d[i+1].uc <= 0xDFFF) {
      unsigned sc = 0x10000 + (((c & 0x3FF) << 10) | (d[i+1].uc & 0x3FF));
      *p++ = (char)((sc >> 18) | 0xF0); // F0 is the 4-byte flag for UTF-8
      *p++ = (char)(((sc >> 12) | 0x80) & 0xBF); // next 6 bits, with high bit set
      *p++ = (char)(((sc >> 6) | 0x80) & 0xBF); // next 6 bits, with high bit set
      *p++ = (char)((sc | 0x80) & 0xBF); // next 6 bits, with high bit set
      ++i;
    } else {
      *p++ = (char)((c >> 12) | 0xE0); // E0 is the 3-byte flag for UTF-8
      *p++ = (char)(((c >> 6) | 0x80) & 0xBF); // next 6 bits, with high bit set
      *p++ = (char)((c | 0x80) & 0xBF); // next 6 bits, with high bit set
    }
  }

  // Return the result as a C string.
  CString result(buffer, p - buffer);
  if (buffer != fixedSizeBuffer) {
    delete [] buffer;
  }
  return result;
}

} // namespace KJS
