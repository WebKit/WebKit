// -*- c-basic-offset: 2 -*-
/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
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

#include "config.h"
#include "ustring.h"

#include "JSLock.h"
#include "collector.h"
#include "dtoa.h"
#include "function.h"
#include "identifier.h"
#include "operations.h"
#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <wtf/Assertions.h>
#include <wtf/ASCIICType.h>
#include <wtf/MathExtras.h>
#include <wtf/Vector.h>
#include <wtf/unicode/UTF8.h>

#if HAVE(STRING_H)
#include <string.h>
#endif
#if HAVE(STRINGS_H)
#include <strings.h>
#endif

using namespace WTF;
using namespace WTF::Unicode;
using namespace std;

namespace KJS {

extern const double NaN;
extern const double Inf;

static inline const size_t overflowIndicator() { return std::numeric_limits<size_t>::max(); }
static inline const size_t maxUChars() { return std::numeric_limits<size_t>::max() / sizeof(UChar); }

static inline UChar* allocChars(size_t length)
{
    ASSERT(length);
    if (length > maxUChars())
        return 0;
    return static_cast<UChar*>(fastMalloc(sizeof(UChar) * length));
}

static inline UChar* reallocChars(UChar* buffer, size_t length)
{
    ASSERT(length);
    if (length > maxUChars())
        return 0;
    return static_cast<UChar*>(fastRealloc(buffer, sizeof(UChar) * length));
}

COMPILE_ASSERT(sizeof(UChar) == 2, uchar_is_2_bytes)

CString::CString(const char *c)
{
  length = strlen(c);
  data = new char[length+1];
  memcpy(data, c, length + 1);
}

CString::CString(const char *c, size_t len)
{
  length = len;
  data = new char[len+1];
  memcpy(data, c, len);
  data[len] = 0;
}

CString::CString(const CString &b)
{
  length = b.length;
  if (b.data) {
    data = new char[length+1];
    memcpy(data, b.data, length + 1);
  }
  else
    data = 0;
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
  if (str.data) {
    data = new char[length + 1];
    memcpy(data, str.data, length + 1);
  }
  else
    data = 0;

  return *this;
}

bool operator==(const CString& c1, const CString& c2)
{
  size_t len = c1.size();
  return len == c2.size() && (len == 0 || memcmp(c1.c_str(), c2.c_str(), len) == 0);
}

// Hack here to avoid a global with a constructor; point to an unsigned short instead of a UChar.
static unsigned short almostUChar;
UString::Rep UString::Rep::null = { 0, 0, 1, 0, 0, &UString::Rep::null, 0, 0, 0, 0, 0, 0 };
UString::Rep UString::Rep::empty = { 0, 0, 1, 0, 0, &UString::Rep::empty, 0, reinterpret_cast<UChar*>(&almostUChar), 0, 0, 0, 0 };
const int normalStatBufferSize = 4096;
static char *statBuffer = 0; // FIXME: This buffer is never deallocated.
static int statBufferSize = 0;

PassRefPtr<UString::Rep> UString::Rep::createCopying(const UChar *d, int l)
{
  ASSERT(JSLock::lockCount() > 0);

  int sizeInBytes = l * sizeof(UChar);
  UChar *copyD = static_cast<UChar *>(fastMalloc(sizeInBytes));
  memcpy(copyD, d, sizeInBytes);

  return create(copyD, l);
}

PassRefPtr<UString::Rep> UString::Rep::create(UChar *d, int l)
{
  ASSERT(JSLock::lockCount() > 0);

  Rep* r = new Rep;
  r->offset = 0;
  r->len = l;
  r->rc = 1;
  r->_hash = 0;
  r->isIdentifier = 0;
  r->baseString = r;
  r->reportedCost = 0;
  r->buf = d;
  r->usedCapacity = l;
  r->capacity = l;
  r->usedPreCapacity = 0;
  r->preCapacity = 0;

  // steal the single reference this Rep was created with
  return adoptRef(r);
}

PassRefPtr<UString::Rep> UString::Rep::create(PassRefPtr<Rep> base, int offset, int length)
{
  ASSERT(JSLock::lockCount() > 0);
  ASSERT(base);

  int baseOffset = base->offset;

  base = base->baseString;

  ASSERT(-(offset + baseOffset) <= base->usedPreCapacity);
  ASSERT(offset + baseOffset + length <= base->usedCapacity);

  Rep *r = new Rep;
  r->offset = baseOffset + offset;
  r->len = length;
  r->rc = 1;
  r->_hash = 0;
  r->isIdentifier = 0;
  r->baseString = base.releaseRef();
  r->reportedCost = 0;
  r->buf = 0;
  r->usedCapacity = 0;
  r->capacity = 0;
  r->usedPreCapacity = 0;
  r->preCapacity = 0;

  // steal the single reference this Rep was created with
  return adoptRef(r);
}

void UString::Rep::destroy()
{
  ASSERT(JSLock::lockCount() > 0);

  if (isIdentifier)
    Identifier::remove(this);
  if (baseString != this) {
    baseString->deref();
  } else {
    fastFree(buf);
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
  size_t l = strlen(s);
  
  size_t rem = l & 1;
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
inline size_t UString::expandedSize(size_t size, size_t otherSize) const
{
    // Do the size calculation in two parts, returning overflowIndicator if
    // we overflow the maximum value that we can handle.

    if (size > maxUChars())
        return overflowIndicator();

    size_t expandedSize = ((size + 10) / 10 * 11) + 1;
    if (maxUChars() - expandedSize < otherSize)
        return overflowIndicator();

    return expandedSize + otherSize;
}

inline int UString::usedCapacity() const
{
  return m_rep->baseString->usedCapacity;
}

inline int UString::usedPreCapacity() const
{
  return m_rep->baseString->usedPreCapacity;
}

void UString::expandCapacity(int requiredLength)
{
  Rep* r = m_rep->baseString;

  if (requiredLength > r->capacity) {
    size_t newCapacity = expandedSize(requiredLength, r->preCapacity);
    UChar* oldBuf = r->buf;
    r->buf = reallocChars(r->buf, newCapacity);
    if (!r->buf) {
        r->buf = oldBuf;
        m_rep = &Rep::null;
        return;
    }
    r->capacity = newCapacity - r->preCapacity;
  }
  if (requiredLength > r->usedCapacity) {
    r->usedCapacity = requiredLength;
  }
}

void UString::expandPreCapacity(int requiredPreCap)
{
  Rep* r = m_rep->baseString;

  if (requiredPreCap > r->preCapacity) {
    size_t newCapacity = expandedSize(requiredPreCap, r->capacity);
    int delta = newCapacity - r->capacity - r->preCapacity;

    UChar* newBuf = allocChars(newCapacity);
    if (!newBuf) {
        m_rep = &Rep::null;
        return;
    }
    memcpy(newBuf + delta, r->buf, (r->capacity + r->preCapacity) * sizeof(UChar));
    fastFree(r->buf);
    r->buf = newBuf;

    r->preCapacity = newCapacity - r->capacity;
  }
  if (requiredPreCap > r->usedPreCapacity) {
    r->usedPreCapacity = requiredPreCap;
  }
}

UString::UString(const char *c)
{
  if (!c) {
    m_rep = &Rep::null;
    return;
  }

  if (!c[0]) {
    m_rep = &Rep::empty;
    return;
  }

  size_t length = strlen(c);
  UChar *d = allocChars(length);
  if (!d)
      m_rep = &Rep::null;
  else {
      for (size_t i = 0; i < length; i++)
          d[i].uc = c[i];
      m_rep = Rep::create(d, static_cast<int>(length));
  }
}

UString::UString(const UChar *c, int length)
{
  if (length == 0) 
    m_rep = &Rep::empty;
  else
    m_rep = Rep::createCopying(c, length);
}

UString::UString(UChar *c, int length, bool copy)
{
  if (length == 0)
    m_rep = &Rep::empty;
  else if (copy)
    m_rep = Rep::createCopying(c, length);
  else
    m_rep = Rep::create(c, length);
}

UString::UString(const Vector<UChar>& buffer)
{
    if (!buffer.size())
        m_rep = &Rep::empty;
    else
        m_rep = Rep::createCopying(buffer.data(), buffer.size());
}


UString::UString(const UString &a, const UString &b)
{
  int aSize = a.size();
  int aOffset = a.m_rep->offset;
  int bSize = b.size();
  int bOffset = b.m_rep->offset;
  int length = aSize + bSize;

  // possible cases:
 
  if (aSize == 0) {
    // a is empty
    m_rep = b.m_rep;
  } else if (bSize == 0) {
    // b is empty
    m_rep = a.m_rep;
  } else if (aOffset + aSize == a.usedCapacity() && aSize >= minShareSize && 4 * aSize >= bSize &&
             (-bOffset != b.usedPreCapacity() || aSize >= bSize)) {
    // - a reaches the end of its buffer so it qualifies for shared append
    // - also, it's at least a quarter the length of b - appending to a much shorter
    //   string does more harm than good
    // - however, if b qualifies for prepend and is longer than a, we'd rather prepend
    UString x(a);
    x.expandCapacity(aOffset + length);
    if (a.data() && x.data()) {
        memcpy(const_cast<UChar *>(a.data() + aSize), b.data(), bSize * sizeof(UChar));
        m_rep = Rep::create(a.m_rep, 0, length);
    } else
        m_rep = &Rep::null;
  } else if (-bOffset == b.usedPreCapacity() && bSize >= minShareSize  && 4 * bSize >= aSize) {
    // - b reaches the beginning of its buffer so it qualifies for shared prepend
    // - also, it's at least a quarter the length of a - prepending to a much shorter
    //   string does more harm than good
    UString y(b);
    y.expandPreCapacity(-bOffset + aSize);
    if (b.data() && y.data()) {
        memcpy(const_cast<UChar *>(b.data() - aSize), a.data(), aSize * sizeof(UChar));
        m_rep = Rep::create(b.m_rep, -aSize, length);
    } else
        m_rep = &Rep::null;
  } else {
    // a does not qualify for append, and b does not qualify for prepend, gotta make a whole new string
    size_t newCapacity = expandedSize(length, 0);
    UChar* d = allocChars(newCapacity);
    if (!d)
        m_rep = &Rep::null;
    else {
        memcpy(d, a.data(), aSize * sizeof(UChar));
        memcpy(d + aSize, b.data(), bSize * sizeof(UChar));
        m_rep = Rep::create(d, length);
        m_rep->capacity = newCapacity;
    }
  }
}

const UString& UString::null()
{
  static UString* n = new UString;
  return *n;
}

UString UString::from(int i)
{
  UChar buf[1 + sizeof(i) * 3];
  UChar *end = buf + sizeof(buf) / sizeof(UChar);
  UChar *p = end;
  
  if (i == 0) {
    *--p = '0';
  } else if (i == INT_MIN) {
    char minBuf[1 + sizeof(i) * 3];
    sprintf(minBuf, "%d", INT_MIN);
    return UString(minBuf);
  } else {
    bool negative = false;
    if (i < 0) {
      negative = true;
      i = -i;
    }
    while (i) {
      *--p = (unsigned short)((i % 10) + '0');
      i /= 10;
    }
    if (negative) {
      *--p = '-';
    }
  }
  
  return UString(p, static_cast<int>(end - p));
}

UString UString::from(unsigned int u)
{
  UChar buf[sizeof(u) * 3];
  UChar *end = buf + sizeof(buf) / sizeof(UChar);
  UChar *p = end;
  
  if (u == 0) {
    *--p = '0';
  } else {
    while (u) {
      *--p = (unsigned short)((u % 10) + '0');
      u /= 10;
    }
  }
  
  return UString(p, static_cast<int>(end - p));
}

UString UString::from(long l)
{
  UChar buf[1 + sizeof(l) * 3];
  UChar *end = buf + sizeof(buf) / sizeof(UChar);
  UChar *p = end;
  
  if (l == 0) {
    *--p = '0';
  } else if (l == LONG_MIN) {
    char minBuf[1 + sizeof(l) * 3];
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
  
  return UString(p, static_cast<int>(end - p));
}

UString UString::from(double d)
{
  // avoid ever printing -NaN, in JS conceptually there is only one NaN value
  if (isnan(d))
    return "NaN";

  char buf[80];
  int decimalPoint;
  int sign;
  
  char *result = kjs_dtoa(d, 0, 0, &decimalPoint, &sign, NULL);
  int length = static_cast<int>(strlen(result));
  
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
    if (exponential < 0)
      exponential = -exponential;
    if (exponential >= 100)
      buf[i++] = static_cast<char>('0' + exponential / 100);
    if (exponential >= 10)
      buf[i++] = static_cast<char>('0' + (exponential % 100) / 10);
    buf[i++] = static_cast<char>('0' + exponential % 10);
    buf[i++] = '\0';
  }
  
  kjs_freedtoa(result);
  
  return UString(buf);
}

UString UString::spliceSubstringsWithSeparators(const Range* substringRanges, int rangeCount, const UString* separators, int separatorCount) const
{
  if (rangeCount == 1 && separatorCount == 0) {
    int thisSize = size();
    int position = substringRanges[0].position;
    int length = substringRanges[0].length;
    if (position <= 0 && length >= thisSize)
      return *this;
    return UString::Rep::create(m_rep, max(0, position), min(thisSize, length));
  }

  int totalLength = 0;
  for (int i = 0; i < rangeCount; i++)
    totalLength += substringRanges[i].length;
  for (int i = 0; i < separatorCount; i++)
    totalLength += separators[i].size();

  if (totalLength == 0)
    return "";

  UChar* buffer = allocChars(totalLength);
  if (!buffer)
      return null();

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

  return UString::Rep::create(buffer, totalLength);
}

UString &UString::append(const UString &t)
{
  int thisSize = size();
  int thisOffset = m_rep->offset;
  int tSize = t.size();
  int length = thisSize + tSize;

  // possible cases:
  if (thisSize == 0) {
    // this is empty
    *this = t;
  } else if (tSize == 0) {
    // t is empty
  } else if (m_rep->baseIsSelf() && m_rep->rc == 1) {
    // this is direct and has refcount of 1 (so we can just alter it directly)
    expandCapacity(thisOffset + length);
    if (data()) {
        memcpy(const_cast<UChar*>(data() + thisSize), t.data(), tSize * sizeof(UChar));
        m_rep->len = length;
        m_rep->_hash = 0;
    }
  } else if (thisOffset + thisSize == usedCapacity() && thisSize >= minShareSize) {
    // this reaches the end of the buffer - extend it if it's long enough to append to
    expandCapacity(thisOffset + length);
    if (data()) {
        memcpy(const_cast<UChar*>(data() + thisSize), t.data(), tSize * sizeof(UChar));
        m_rep = Rep::create(m_rep, 0, length);
    }
  } else {
    // this is shared with someone using more capacity, gotta make a whole new string
    size_t newCapacity = expandedSize(length, 0);
    UChar* d = allocChars(newCapacity);
    if (!d)
        m_rep = &Rep::null;
    else {
        memcpy(d, data(), thisSize * sizeof(UChar));
        memcpy(const_cast<UChar*>(d + thisSize), t.data(), tSize * sizeof(UChar));
        m_rep = Rep::create(d, length);
        m_rep->capacity = newCapacity;
    }
  }

  return *this;
}

UString &UString::append(const char *t)
{
  int thisSize = size();
  int thisOffset = m_rep->offset;
  int tSize = static_cast<int>(strlen(t));
  int length = thisSize + tSize;

  // possible cases:
  if (thisSize == 0) {
    // this is empty
    *this = t;
  } else if (tSize == 0) {
    // t is empty, we'll just return *this below.
  } else if (m_rep->baseIsSelf() && m_rep->rc == 1) {
    // this is direct and has refcount of 1 (so we can just alter it directly)
    expandCapacity(thisOffset + length);
    UChar *d = const_cast<UChar *>(data());
    if (d) {
        for (int i = 0; i < tSize; ++i)
            d[thisSize + i] = t[i];
        m_rep->len = length;
        m_rep->_hash = 0;
    }
  } else if (thisOffset + thisSize == usedCapacity() && thisSize >= minShareSize) {
    // this string reaches the end of the buffer - extend it
    expandCapacity(thisOffset + length);
    UChar *d = const_cast<UChar *>(data());
    if (d) {
        for (int i = 0; i < tSize; ++i)
            d[thisSize + i] = t[i];
        m_rep = Rep::create(m_rep, 0, length);
    }
  } else {
    // this is shared with someone using more capacity, gotta make a whole new string
    size_t newCapacity = expandedSize(length, 0);
    UChar* d = allocChars(newCapacity);
    if (!d)
        m_rep = &Rep::null;
    else {
        memcpy(d, data(), thisSize * sizeof(UChar));
        for (int i = 0; i < tSize; ++i)
            d[thisSize + i] = t[i];
        m_rep = Rep::create(d, length);
        m_rep->capacity = newCapacity;
    }
  }

  return *this;
}

UString &UString::append(unsigned short c)
{
  int thisOffset = m_rep->offset;
  int length = size();

  // possible cases:
  if (length == 0) {
    // this is empty - must make a new m_rep because we don't want to pollute the shared empty one 
    size_t newCapacity = expandedSize(1, 0);
    UChar* d = allocChars(newCapacity);
    if (!d)
        m_rep = &Rep::null;
    else {
        d[0] = c;
        m_rep = Rep::create(d, 1);
        m_rep->capacity = newCapacity;
    }
  } else if (m_rep->baseIsSelf() && m_rep->rc == 1) {
    // this is direct and has refcount of 1 (so we can just alter it directly)
    expandCapacity(thisOffset + length + 1);
    UChar *d = const_cast<UChar *>(data());
    if (d) {
        d[length] = c;
        m_rep->len = length + 1;
        m_rep->_hash = 0;
    }
  } else if (thisOffset + length == usedCapacity() && length >= minShareSize) {
    // this reaches the end of the string - extend it and share
    expandCapacity(thisOffset + length + 1);
    UChar *d = const_cast<UChar *>(data());
    if (d) {
        d[length] = c;
        m_rep = Rep::create(m_rep, 0, length + 1);
    }
  } else {
    // this is shared with someone using more capacity, gotta make a whole new string
    size_t newCapacity = expandedSize(length + 1, 0);
    UChar* d = allocChars(newCapacity);
    if (!d)
        m_rep = &Rep::null;
    else {
        memcpy(d, data(), length * sizeof(UChar));
        d[length] = c;
        m_rep = Rep::create(d, length + 1);
        m_rep->capacity = newCapacity;
    }
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
    *q = static_cast<char>(p->uc);
    ++p;
    ++q;
  }
  *q = '\0';

  return statBuffer;
}

UString &UString::operator=(const char *c)
{
    if (!c) {
        m_rep = &Rep::null;
        return *this;
    }

    if (!c[0]) {
        m_rep = &Rep::empty;
        return *this;
    }

  int l = static_cast<int>(strlen(c));
  UChar *d;
  if (m_rep->rc == 1 && l <= m_rep->capacity && m_rep->baseIsSelf() && m_rep->offset == 0 && m_rep->preCapacity == 0) {
    d = m_rep->buf;
    m_rep->_hash = 0;
    m_rep->len = l;
  } else {
    d = allocChars(l);
    if (!d) {
        m_rep = &Rep::null;
        return *this;
    }
    m_rep = Rep::create(d, l);
  }
  for (int i = 0; i < l; i++)
    d[i].uc = c[i];

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

const UChar UString::operator[](int pos) const
{
  if (pos >= size())
    return '\0';
  return data()[pos];
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
  while (isASCIISpace(*c))
    c++;

  // empty string ?
  if (*c == '\0')
    return tolerateEmptyString ? 0.0 : NaN;

  // hex number ?
  if (*c == '0' && (*(c+1) == 'x' || *(c+1) == 'X')) {
    const char* firstDigitPosition = c + 2;
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

    if (d >= mantissaOverflowLowerBound)
        d = parseIntOverflow(firstDigitPosition, c - firstDigitPosition, 16);
  } else {
    // regular number ?
    char *end;
    d = kjs_strtod(c, &end);
    if ((d != 0.0 || end != c) && d != Inf && d != -Inf) {
      c = end;
    } else {
      double sign = 1.0;

      if (*c == '+')
        c++;
      else if (*c == '-') {
        sign = -1.0;
        c++;
      }

      // We used strtod() to do the conversion. However, strtod() handles
      // infinite values slightly differently than JavaScript in that it
      // converts the string "inf" with any capitalization to infinity,
      // whereas the ECMA spec requires that it be converted to NaN.

      if (c[0] == 'I' && c[1] == 'n' && c[2] == 'f' && c[3] == 'i' && c[4] == 'n' && c[5] == 'i' && c[6] == 't' && c[7] == 'y') {
        d = sign * Inf;
        c += 8;
      } else if ((d == Inf || d == -Inf) && *c != 'I' && *c != 'i')
        c = end;
      else
        return NaN;
    }
  }

  // allow trailing white space
  while (isASCIISpace(*c))
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

uint32_t UString::toUInt32(bool *ok) const
{
  double d = toDouble();
  bool b = true;

  if (d != static_cast<uint32_t>(d)) {
    b = false;
    d = 0;
  }

  if (ok)
    *ok = b;

  return static_cast<uint32_t>(d);
}

uint32_t UString::toUInt32(bool *ok, bool tolerateEmptyString) const
{
  double d = toDouble(false, tolerateEmptyString);
  bool b = true;

  if (d != static_cast<uint32_t>(d)) {
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
  int len = m_rep->len;
  if (len == 0)
    return 0;
  const UChar *p = m_rep->data();
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
  int fsizeminusone = (fsz - 1) * sizeof(UChar);
  const UChar *fdata = f.data();
  unsigned short fchar = fdata->uc;
  ++fdata;
  for (const UChar *c = data() + pos; c <= end; c++)
    if (c->uc == fchar && !memcmp(c + 1, fdata, fsizeminusone))
      return static_cast<int>(c - data());

  return -1;
}

int UString::find(UChar ch, int pos) const
{
  if (pos < 0)
    pos = 0;
  const UChar *end = data() + size();
  for (const UChar *c = data() + pos; c < end; c++)
    if (*c == ch)
      return static_cast<int>(c - data());

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
  int fsizeminusone = (fsz - 1) * sizeof(UChar);
  const UChar *fdata = f.data();
  for (const UChar *c = data() + pos; c >= data(); c--) {
    if (*c == *fdata && !memcmp(c + 1, fdata + 1, fsizeminusone))
      return static_cast<int>(c - data());
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
      return static_cast<int>(c-data());
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

  return UString(Rep::create(m_rep, pos, len));
}

bool operator==(const UString& s1, const UString& s2)
{
  if (s1.m_rep->len != s2.m_rep->len)
    return false;

  return (memcmp(s1.m_rep->data(), s2.m_rep->data(),
                 s1.m_rep->len * sizeof(UChar)) == 0);
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

  if (l1 == l2)
    return 0;

  return (l1 > l2) ? 1 : -1;
}

CString UString::UTF8String(bool strict) const
{
  // Allocate a buffer big enough to hold all the characters.
  const int length = size();
  Vector<char, 1024> buffer(length * 3);

  // Convert to runs of 8-bit characters.
  char* p = buffer.data();
  const ::UChar* d = reinterpret_cast<const ::UChar*>(&data()->uc);
  ConversionResult result = convertUTF16ToUTF8(&d, d + length, &p, p + buffer.size(), strict);
  if (result != conversionOK)
    return CString();

  return CString(buffer.data(), p - buffer.data());
}

} // namespace KJS
