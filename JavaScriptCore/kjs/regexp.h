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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _KJS_REGEXP_H_
#define _KJS_REGEXP_H_

#include <sys/types.h>

#include "config.h"

#if HAVE(PCREPOSIX)
#include <pcre.h>
#else  // POSIX regex - not so good...
extern "C" { // bug with some libc5 distributions
#include <regex.h>
}
#endif // HAVE(PCREPOSIX)

#include "ustring.h"

namespace KJS {

  class RegExp {
  public:
    enum { None = 0, Global = 1, IgnoreCase = 2, Multiline = 4 };

    RegExp(const UString &pattern, int flags = None);
    ~RegExp();

    int flags() const { return _flags; }

    UString match(const UString &s, int i, int *pos = 0, int **ovector = 0);
    unsigned subPatterns() const { return _numSubPatterns; }

  private:
#if HAVE(PCREPOSIX)
    pcre *_regex;
#else
    regex_t _regex;
#endif
    int _flags;
    unsigned _numSubPatterns;

    RegExp(const RegExp &);
    RegExp &operator=(const RegExp &);
  };

} // namespace

#endif
