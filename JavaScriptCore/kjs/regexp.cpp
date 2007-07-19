// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001,2004 Harri Porten (porten@kde.org)
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

#include "lexer.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace KJS {

RegExp::RegExp(const UString &p, int flags)
  : m_flags(flags), m_constructionError(0), m_numSubPatterns(0)
{
#if HAVE(PCREPOSIX)

  int options = PCRE_UTF8;
  // Note: the Global flag is already handled by RegExpProtoFunc::execute.
  // FIXME: That last comment is dubious. Not all RegExps get run through RegExpProtoFunc::execute.
  if (flags & IgnoreCase)
    options |= PCRE_CASELESS;
  if (flags & Multiline)
    options |= PCRE_MULTILINE;

  const char *errorMessage;
  int errorOffset;
  
  m_regex = pcre_compile(reinterpret_cast<const uint16_t*>(p.data()), p.size(),
                        options, &errorMessage, &errorOffset, NULL);
  if (!m_regex) {
    m_constructionError = strdup(errorMessage);
    return;
  }

#ifdef PCRE_INFO_CAPTURECOUNT
  // Get number of subpatterns that will be returned.
  pcre_fullinfo(m_regex, NULL, PCRE_INFO_CAPTURECOUNT, &m_numSubPatterns);
#endif

#else /* HAVE(PCREPOSIX) */

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

  // FIXME: support \u Unicode escapes.

  int errorCode = regcomp(&m_regex, intern.ascii(), regflags);
  if (errorCode != 0) {
    char errorMessage[80];
    regerror(errorCode, &m_regex, errorMessage, sizeof errorMessage);
    m_constructionError = strdup(errorMessage);
  }

#endif
}

RegExp::~RegExp()
{
#if HAVE(PCREPOSIX)
  pcre_free(m_regex);
#else
  /* TODO: is this really okay after an error ? */
  regfree(&m_regex);
#endif
  free(m_constructionError);
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

#if HAVE(PCREPOSIX)

  if (!m_regex)
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
    offsetVectorSize = (m_numSubPatterns + 1) * 3;
    offsetVector = new int [offsetVectorSize];
  }

  const int numMatches = pcre_exec(m_regex, NULL, reinterpret_cast<const uint16_t *>(s.data()), s.size(), i, 0, offsetVector, offsetVectorSize);

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
  if (regexec(&m_regex, str + i, maxMatch, rmatch, 0)) {
    free(str);
    return UString::null();
  }
  free(str);

  if (!ovector) {
    *pos = rmatch[0].rm_so + i;
    return s.substr(rmatch[0].rm_so + i, rmatch[0].rm_eo - rmatch[0].rm_so);
  }

  // map rmatch array to ovector used in PCRE case
  m_numSubPatterns = 0;
  for(unsigned j = 1; j < maxMatch && rmatch[j].rm_so >= 0 ; j++)
      m_numSubPatterns++;
  int ovecsize = (m_numSubPatterns+1)*3; // see above
  *ovector = new int[ovecsize];
  for (unsigned j = 0; j < m_numSubPatterns + 1; j++) {
    if (j>maxMatch)
      break;
    (*ovector)[2*j] = rmatch[j].rm_so + i;
    (*ovector)[2*j+1] = rmatch[j].rm_eo + i;
  }

  *pos = (*ovector)[0];
  return s.substr((*ovector)[0], (*ovector)[1] - (*ovector)[0]);

#endif
}

bool RegExp::isHexDigit(UChar uc)
{
  int c = uc.unicode();
  return (c >= '0' && c <= '9' ||
          c >= 'a' && c <= 'f' ||
          c >= 'A' && c <= 'F');
}

unsigned char RegExp::convertHex(int c)
{
  if (c >= '0' && c <= '9')
    return static_cast<unsigned char>(c - '0');
  if (c >= 'a' && c <= 'f')
    return static_cast<unsigned char>(c - 'a' + 10);
  return static_cast<unsigned char>(c - 'A' + 10);
}

unsigned char RegExp::convertHex(int c1, int c2)
{
  return ((convertHex(c1) << 4) + convertHex(c2));
}

UChar RegExp::convertUnicode(UChar uc1, UChar uc2, UChar uc3, UChar uc4)
{
  int c1 = uc1.unicode();
  int c2 = uc2.unicode();
  int c3 = uc3.unicode();
  int c4 = uc4.unicode();
  return UChar((convertHex(c1) << 4) + convertHex(c2),
               (convertHex(c3) << 4) + convertHex(c4));
}

} // namespace KJS
