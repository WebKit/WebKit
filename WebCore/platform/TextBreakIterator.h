/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2006 Lars Knoll <lars@trolltech.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
#ifndef TextBreakIterator_h
#define TextBreakIterator_h

#include "wtf/unicode/Unicode.h"

#if USE(ICU_UNICODE)
#include <unicode/ubrk.h>
typedef UBreakIterator TextBreakIterator;
#elif USE(QT4_UNICODE)
namespace WebCore {
    class TextBreakIterator;
}
#endif


namespace WebCore {
    TextBreakIterator* wordBreakIterator(const UChar* string, int length);
    TextBreakIterator* characterBreakIterator(const UChar* string, int length);

    int textBreakFirst(TextBreakIterator*);
    int textBreakNext(TextBreakIterator*);
    int textBreakCurrent(TextBreakIterator*);
    int textBreakPreceding(TextBreakIterator*, int);
    int textBreakFollowing(TextBreakIterator*, int);
    enum { TextBreakDone = -1 };
}
#endif
