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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "regexp.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace KJS {

RegExp::RegExp(const UString &p, int flags)
  : _flags(flags), _numSubPatterns(0)
{
#ifdef HAVE_PCREPOSIX

  int options = PCRE_UTF8;
  // Note: the Global flag is already handled by RegExpProtoFunc::execute.
  // FIXME: That last comment is dubious. Not all RegExps get run through RegExpProtoFunc::execute.
  if (flags & IgnoreCase)
    options |= PCRE_CASELESS;
  if (flags & Multiline)
    options |= PCRE_MULTILINE;

  const char *errorMessage;
  int errorOffset;
  UString nullTerminated(p);
  char null(0);
  nullTerminated.append(null);
  _regex = pcre_compile(reinterpret_cast<const uint16_t *>(nullTerminated.data()), options, &errorMessage, &errorOffset, NULL);
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

  const int numMatches = pcre_exec(_regex, NULL, reinterpret_cast<const uint16_t *>(s.data()), s.size(), i, 0, offsetVector, offsetVectorSize);

  if (numMatches < 0) {
#ifndef NDEBUG
    if (numMatches != PCRE_ERROR_NOMATCH)
      fprintf(stderr, "KJS: pcre_exec() failed with result %d\n", numMatches);
#endif
    if (offsetVector != fixedSizeOffsetVector)
      delete [] offsetVector;
    return UString::null();
  }

  *pos = offsetVector[0];
  if (ovector)
    *ovector = offsetVector;
  return s.substr(offsetVector[0], offsetVector[1] - offsetVector[0]);

#else

  const unsigned maxMatch = 10;
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
  for(unsigned j = 1; j < maxMatch && rmatch[j].rm_so >= 0 ; j++)
      _numSubPatterns++;
  int ovecsize = (_numSubPatterns+1)*3; // see above
  *ovector = new int[ovecsize];
  for (unsigned j = 0; j < _numSubPatterns + 1; j++) {
    if (j>maxMatch)
      break;
    (*ovector)[2*j] = rmatch[j].rm_so + i;
    (*ovector)[2*j+1] = rmatch[j].rm_eo + i;
  }

  *pos = (*ovector)[0];
  return s.substr((*ovector)[0], (*ovector)[1] - (*ovector)[0]);

#endif
}

} // namespace KJS
