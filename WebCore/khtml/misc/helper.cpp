/*
 * This file is part of the CSS implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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

#include "config.h"
#include "helper.h"

#if APPLE_CHANGES
#include "KWQTextUtilities.h"
#endif

namespace khtml {

QPainter *printpainter = 0;

void setPrintPainter( QPainter *printer )
{
    printpainter = printer;
}

void findWordBoundary(const QChar *chars, int len, int position, int *start, int *end)
{
#if APPLE_CHANGES
    KWQFindWordBoundary(chars, len, position, start, end);
#else
    // KDE implementation
#endif
}

int nextWordFromIndex(const QChar *chars, int len, int position, bool forward)
{
#if APPLE_CHANGES
    return KWQFindNextWordFromIndex(chars, len, position, forward);
#else
    // KDE implementation
#endif
}

void findSentenceBoundary(const QChar *chars, int len, int position, int *start, int *end)
{
#if APPLE_CHANGES
    KWQFindSentenceBoundary(chars, len, position, start, end);
#else
    // KDE implementation
#endif
}

int nextSentenceFromIndex(const QChar *chars, int len, int position, bool forward)
{
#if APPLE_CHANGES
    return KWQFindNextSentenceFromIndex(chars, len, position, forward);
#else
    // KDE implementation
#endif
}

}
