// -*- c-basic-offset: 2 -*-
/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007 Apple Inc. All rights reserved.
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

#ifndef KJS_REGEXP_H
#define KJS_REGEXP_H

#include <sys/types.h>

#if USE(PCRE16)
#include <pcre.h>
#else
// POSIX regex - not so good.
extern "C" { // bug with some libc5 distributions
#include <regex.h>
}
#endif

#include "ustring.h"
#include <wtf/Vector.h>

namespace KJS {

  class RegExp : Noncopyable {
  public:
    enum { None = 0, Global = 1, IgnoreCase = 2, Multiline = 4 };

    RegExp(const UString& pattern, int flags = None);
    ~RegExp();

    int flags() const { return m_flags; }
    bool isValid() const { return !m_constructionError; }
    const char* errorMessage() const { return m_constructionError; }

    int match(const UString&, int offset, OwnArrayPtr<int>* ovector = 0);
    unsigned subPatterns() const { return m_numSubPatterns; }

  private:
#if USE(PCRE16)
    pcre* m_regex;
#else
    regex_t m_regex;
#endif
    int m_flags;
    char* m_constructionError;
    unsigned m_numSubPatterns;
  };

} // namespace

#endif
