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
 */

#ifndef _KJS_REGEXP_H_
#define _KJS_REGEXP_H_

#include <sys/types.h>

#include "config.h"

#ifdef HAVE_PCREPOSIX
#include <pcre.h>
#else  // POSIX regex - not so good...
extern "C" { // bug with some libc5 distributions
#include <regex.h>
}
#endif //HAVE_PCREPOSIX

#include "ustring.h"

namespace KJS {

  class RegExp {
  public:
    enum { None = 0, Global = 1, IgnoreCase = 2, Multiline = 4 };
    RegExp(const UString &p, int f = None);
    ~RegExp();
    int flags() const { return flgs; }
    UString match(const UString &s, int i = -1, int *pos = 0L, int **ovector = 0L);
    // test is unused. The JS spec says that RegExp.test should use
    // RegExp.exec, so it has to store $1 etc.
    // bool test(const UString &s, int i = -1);
    uint subPatterns() const { return nrSubPatterns; }
  private:
    const UString &pattern;
    int flgs;

#ifndef HAVE_PCREPOSIX
    regex_t preg;
#else
    pcre *pcregex;
#endif
    uint nrSubPatterns;

    RegExp();
  };

}; // namespace

#endif
