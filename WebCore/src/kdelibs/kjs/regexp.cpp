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
 */

#include <stdio.h>

#include "object.h"
#include "regexp.h"

using namespace KJS;

RegExp::RegExp(const UString &p, int f)
 : pattern(p), flags(f)
{
#ifdef REG_EXTENDED    
  regcomp(&preg, p.ascii(), REG_EXTENDED);
#else
  regcomp(&preg, p.ascii(), 0);  
#endif  
  /* TODO use flags, check for errors */
}

RegExp::~RegExp()
{
  /* TODO: is this really okay after an error ? */
  regfree(&preg);
}

UString RegExp::match(const UString &s, int i, int *pos)
{
  regmatch_t rmatch[10];

  if (i < 0)
    i = 0;

  if (i > s.size() || s.isNull() ||
      regexec(&preg, s.ascii() + i, 10, rmatch, 0)) {
    if (pos)
      *pos = -1;
    return UString::null;
  }

  if (pos)
    *pos = rmatch[0].rm_so + i;
  return s.substr(rmatch[0].rm_so + i, rmatch[0].rm_eo - rmatch[0].rm_so);
}

bool RegExp::test(const UString &s, int)
{
  int r = regexec(&preg, s.ascii(), 0, 0, 0);

  return r == 0;
}
