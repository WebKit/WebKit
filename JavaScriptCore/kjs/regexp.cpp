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

using namespace KJS;

RegExp::RegExp(const UString &p, int f)
  : pattern(p), flags(f)
{
#ifdef HAVE_PCREPOSIX
  int pcreflags = 0;
  const char *perrormsg;
  int errorOffset;

  if (flags & IgnoreCase)
    pcreflags |= PCRE_CASELESS;

  if (flags & Multiline)
    pcreflags |= PCRE_MULTILINE;

  pcregex = pcre_compile(p.ascii(), pcreflags,
			 &perrormsg, &errorOffset, NULL);
#ifndef NDEBUG
  if (!pcregex)
    fprintf(stderr, "KJS: pcre_compile() failed with '%s'\n", perrormsg);
#endif

#ifdef PCRE_INFO_CAPTURECOUNT
  // Get number of subpatterns that will be returned
  int rc = pcre_fullinfo( pcregex, NULL, PCRE_INFO_CAPTURECOUNT, &nrSubPatterns);
  if (rc != 0)
#endif
    nrSubPatterns = 0; // fallback. We always need the first pair of offsets.

#else /* HAVE_PCREPOSIX */

  nrSubPatterns = 0; // determined in match() with POSIX regex.
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

  regcomp(&preg, p.ascii(), regflags);
  /* TODO check for errors */
#endif

}

RegExp::~RegExp()
{
#ifdef HAVE_PCREPOSIX
  if (pcregex)
    pcre_free(pcregex);
#else
  /* TODO: is this really okay after an error ? */
  regfree(&preg);
#endif
}

UString RegExp::match(const UString &s, int i, int *pos, int **ovector)
{
  if (i < 0)
    i = 0;
  if (ovector)
    *ovector = 0L;
  int dummyPos;
  if (!pos)
    pos = &dummyPos;
  *pos = -1;
  if (i > s.size() || s.isNull())
    return UString::null;

#ifdef HAVE_PCREPOSIX
  CString buffer(s.cstring());
  int ovecsize = (nrSubPatterns+1)*3; // see pcre docu
  if (ovector) *ovector = new int[ovecsize];

  if (!pcregex || pcre_exec(pcregex, NULL, buffer.c_str(), buffer.size(), i,
		  0, ovector ? *ovector : 0L, ovecsize) == PCRE_ERROR_NOMATCH)
    return UString::null;

  if (!ovector)
    return UString::null; // don't rely on the return value if you pass ovector==0
#else
#ifdef APPLE_CHANGES
  const uint maxMatch = 10;
#else
  const int maxMatch = 10;
#endif
  regmatch_t rmatch[maxMatch];

  char *str = strdup(s.ascii()); // TODO: why ???
  if (regexec(&preg, str + i, maxMatch, rmatch, 0)) {
    free(str);
    return UString::null;
  }
  free(str);

  if (!ovector) {
    *pos = rmatch[0].rm_so + i;
    return s.substr(rmatch[0].rm_so + i, rmatch[0].rm_eo - rmatch[0].rm_so);
  }

  // map rmatch array to ovector used in PCRE case
  nrSubPatterns = 0;
#ifdef APPLE_CHANGES
  for(uint j = 1; j < maxMatch && rmatch[j].rm_so >= 0 ; j++)
#else
  for(int j = 1; j < maxMatch && rmatch[j].rm_so >= 0 ; j++)
#endif
      nrSubPatterns++;
  int ovecsize = (nrSubPatterns+1)*3; // see above
  *ovector = new int[ovecsize];
#ifdef APPLE_CHANGES
  for (uint j = 0; j < nrSubPatterns + 1; j++) {
#else
  for (int j = 0; j < nrSubPatterns + 1; j++) {
#endif
    if (j>maxMatch)
      break;
    (*ovector)[2*j] = rmatch[j].rm_so + i;
    (*ovector)[2*j+1] = rmatch[j].rm_eo + i;
#ifdef APPLE_CHANGES
  } // balance extra { so we don't confuse prepare-ChangeLog
#else
  }
#endif
#endif

  *pos = (*ovector)[0];
  return s.substr((*ovector)[0], (*ovector)[1] - (*ovector)[0]);
}

#if 0 // unused
bool RegExp::test(const UString &s, int)
{
#ifdef HAVE_PCREPOSIX
  int ovector[300];
  CString buffer(s.cstring());

  if (s.isNull() ||
      pcre_exec(pcregex, NULL, buffer.c_str(), buffer.size(), 0,
		0, ovector, 300) == PCRE_ERROR_NOMATCH)
    return false;
  else
    return true;

#else

  char *str = strdup(s.ascii());
  int r = regexec(&preg, str, 0, 0, 0);
  free(str);

  return r == 0;
#endif
}
#endif
