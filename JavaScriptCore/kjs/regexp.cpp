// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
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
 */

#include "regexp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using KJS::CString;
using KJS::RegExp;
using KJS::UString;

#ifdef HAVE_PCREPOSIX

static CString convertToUTF8(const UString &s)
{
    // Allocate a buffer big enough to hold all the characters.
    const int length = s.size();
    const unsigned bufferSize = length * 3 + 1;
    char fixedSizeBuffer[1024];
    char *buffer;
    if (bufferSize > sizeof(fixedSizeBuffer)) {
        buffer = new char [bufferSize];
    } else {
        buffer = fixedSizeBuffer;
    }

    // Convert to runs of 8-bit characters.
    char *p = buffer;
    for (int i = 0; i != length; ++i) {
        unsigned short c = s[i].unicode();
        if (c < 0x80) {
            *p++ = (char)c;
        } else if (c < 0x800) {
            *p++ = (char)((c >> 6) | 0xC0); // C0 is the 2-byte flag for UTF-8
            *p++ = (char)((c | 0x80) & 0xBF); // next 6 bits, with high bit set
        } else {
            *p++ = (char)((c >> 12) | 0xE0); // E0 is the 3-byte flag for UTF-8
            *p++ = (char)(((c >> 6) | 0x80) & 0xBF); // next 6 bits, with high bit set
            *p++ = (char)((c | 0x80) & 0xBF); // next 6 bits, with high bit set
        }
    }
    *p = 0;

    // Return the result as a C string.
    CString result(buffer);
    if (buffer != fixedSizeBuffer) {
        delete [] buffer;
    }
    return result;
}

struct StringOffset {
    int offset;
    int locationInOffsetsArray;
};

static int compareStringOffsets(const void *a, const void *b)
{
    const StringOffset *oa = static_cast<const StringOffset *>(a);
    const StringOffset *ob = static_cast<const StringOffset *>(b);
    
    if (oa->offset < ob->offset) {
        return -1;
    }
    if (oa->offset > ob->offset) {
        return +1;
    }
    return 0;
}

const int sortedOffsetsFixedBufferSize = 128;

static StringOffset *createSortedOffsetsArray(const int offsets[], int numOffsets,
    StringOffset sortedOffsetsFixedBuffer[sortedOffsetsFixedBufferSize])
{
    // Allocate the sorted offsets.
    StringOffset *sortedOffsets;
    if (numOffsets <= sortedOffsetsFixedBufferSize) {
        sortedOffsets = sortedOffsetsFixedBuffer;
    } else {
        sortedOffsets = new StringOffset [numOffsets];
    }

    // Copy offsets.
    for (int i = 0; i != numOffsets; ++i) {
        sortedOffsets[i].offset = offsets[i];
        sortedOffsets[i].locationInOffsetsArray = i;
    }

    // Sort them.
    qsort(sortedOffsets, numOffsets, sizeof(StringOffset), compareStringOffsets);

    return sortedOffsets;
}

static void convertCharacterOffsetsToUTF8ByteOffsets(const char *s, int *offsets, int numOffsets)
{
    // Allocate buffer.
    StringOffset fixedBuffer[sortedOffsetsFixedBufferSize];
    StringOffset *sortedOffsets = createSortedOffsetsArray(offsets, numOffsets, fixedBuffer);

    // Walk through sorted offsets and string, adjusting all the offests.
    // Offsets that are off the ends of the string map to the edges of the string.
    int characterOffset = 0;
    const char *p = s;
    for (int oi = 0; oi != numOffsets; ++oi) {
        const int nextOffset = sortedOffsets[oi].offset;
        while (*p && characterOffset < nextOffset) {
            // Skip to the next character.
            ++characterOffset;
            do ++p; while ((*p & 0xC0) == 0x80); // if 1 of the 2 high bits is set, it's not the start of a character
        }
        offsets[sortedOffsets[oi].locationInOffsetsArray] = p - s;
    }

    // Free buffer.
    if (sortedOffsets != fixedBuffer) {
        delete [] sortedOffsets;
    }
}

static void convertUTF8ByteOffsetsToCharacterOffsets(const char *s, int *offsets, int numOffsets)
{
    // Allocate buffer.
    StringOffset fixedBuffer[sortedOffsetsFixedBufferSize];
    StringOffset *sortedOffsets = createSortedOffsetsArray(offsets, numOffsets, fixedBuffer);

    // Walk through sorted offsets and string, adjusting all the offests.
    // Offsets that are off the end of the string map to the edges of the string.
    int characterOffset = 0;
    const char *p = s;
    for (int oi = 0; oi != numOffsets; ++oi) {
        const int nextOffset = sortedOffsets[oi].offset;
        while (*p && (p - s) < nextOffset) {
            // Skip to the next character.
            ++characterOffset;
            do ++p; while ((*p & 0xC0) == 0x80); // if 1 of the 2 high bits is set, it's not the start of a character
        }
        offsets[sortedOffsets[oi].locationInOffsetsArray] = characterOffset;
    }

    // Free buffer.
    if (sortedOffsets != fixedBuffer) {
        delete [] sortedOffsets;
    }
}

#endif // HAVE_PCREPOSIX

RegExp::RegExp(const UString &p, int flags)
  : _flags(flags), _numSubPatterns(0)
{
#ifdef HAVE_PCREPOSIX

  int options = PCRE_UTF8;
  // Note: the Global flag is already handled by RegExpProtoFunc::execute.
  if (flags & IgnoreCase)
    options |= PCRE_CASELESS;
  if (flags & Multiline)
    options |= PCRE_MULTILINE;

  const char *errorMessage;
  int errorOffset;
  _regex = pcre_compile(convertToUTF8(p).c_str(), options, &errorMessage, &errorOffset, NULL);
  if (!_regex) {
#ifndef NDEBUG
    fprintf(stderr, "KJS: pcre_compile() failed with '%s'\n", errorMessage);
#endif
    return;
  }

#ifdef PCRE_INFO_CAPTURECOUNT
  // Get number of subpatterns that will be returned.
  pcre_fullinfo(_regex, NULL, PCRE_INFO_CAPTURECOUNT, &_numSubPatterns);
#endif

#else /* HAVE_PCREPOSIX */

  int regflags = 0;
#ifdef REG_EXTENDED
  regflags |= REG_EXTENDED;
#endif
#ifdef REG_ICASE
  if ( f & IgnoreCase )
    regflags |= REG_ICASE;
#endif

  //NOTE: Multiline is not feasible with POSIX regex.
  //if ( f & Multiline )
  //    ;
  // Note: the Global flag is already handled by RegExpProtoFunc::execute

  regcomp(&_regex, p.ascii(), regflags);
  /* TODO check for errors */

#endif
}

RegExp::~RegExp()
{
#ifdef HAVE_PCREPOSIX
  pcre_free(_regex);
#else
  /* TODO: is this really okay after an error ? */
  regfree(&_regex);
#endif
}

UString RegExp::match(const UString &s, int i, int *pos, int **ovector)
{
  if (i < 0)
    i = 0;
  int dummyPos;
  if (!pos)
    pos = &dummyPos;
  *pos = -1;
  if (ovector)
    *ovector = 0;

  if (i > s.size() || s.isNull())
    return UString::null();

#ifdef HAVE_PCREPOSIX

  if (!_regex)
    return UString::null();

  // Set up the offset vector for the result.
  // First 2/3 used for result, the last third used by PCRE.
  int *offsetVector;
  int offsetVectorSize;
  int fixedSizeOffsetVector[3];
  if (!ovector) {
    offsetVectorSize = 3;
    offsetVector = fixedSizeOffsetVector;
  } else {
    offsetVectorSize = (_numSubPatterns + 1) * 3;
    offsetVector = new int [offsetVectorSize];
  }

  const CString buffer(convertToUTF8(s));
  convertCharacterOffsetsToUTF8ByteOffsets(buffer.c_str(), &i, 1);
  const int numMatches = pcre_exec(_regex, NULL, buffer.c_str(), buffer.size(), i, 0, offsetVector, offsetVectorSize);

  if (numMatches < 0) {
#ifndef NDEBUG
    if (numMatches != PCRE_ERROR_NOMATCH)
      fprintf(stderr, "KJS: pcre_exec() failed with result %d\n", numMatches);
#endif
    if (offsetVector != fixedSizeOffsetVector)
      delete [] offsetVector;
    return UString::null();
  }

  convertUTF8ByteOffsetsToCharacterOffsets(buffer.c_str(), offsetVector, (numMatches == 0 ? 1 : numMatches) * 2);

  *pos = offsetVector[0];
  if (ovector)
    *ovector = offsetVector;
  return s.substr(offsetVector[0], offsetVector[1] - offsetVector[0]);

#else

  const uint maxMatch = 10;
  regmatch_t rmatch[maxMatch];

  char *str = strdup(s.ascii()); // TODO: why ???
  if (regexec(&_regex, str + i, maxMatch, rmatch, 0)) {
    free(str);
    return UString::null();
  }
  free(str);

  if (!ovector) {
    *pos = rmatch[0].rm_so + i;
    return s.substr(rmatch[0].rm_so + i, rmatch[0].rm_eo - rmatch[0].rm_so);
  }

  // map rmatch array to ovector used in PCRE case
  _numSubPatterns = 0;
  for(uint j = 1; j < maxMatch && rmatch[j].rm_so >= 0 ; j++)
      _numSubPatterns++;
  int ovecsize = (_numSubPatterns+1)*3; // see above
  *ovector = new int[ovecsize];
  for (uint j = 0; j < _numSubPatterns + 1; j++) {
    if (j>maxMatch)
      break;
    (*ovector)[2*j] = rmatch[j].rm_so + i;
    (*ovector)[2*j+1] = rmatch[j].rm_eo + i;
  }

  *pos = (*ovector)[0];
  return s.substr((*ovector)[0], (*ovector)[1] - (*ovector)[0]);

#endif
}
