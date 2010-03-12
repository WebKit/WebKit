/*
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "TextBreakIterator.h"

#include <QtCore/qtextboundaryfinder.h>
#include <qdebug.h>

// #define DEBUG_TEXT_ITERATORS
#ifdef DEBUG_TEXT_ITERATORS
#define DEBUG qDebug
#else
#define DEBUG if (1) {} else qDebug
#endif

namespace WebCore {

    class TextBreakIterator : public QTextBoundaryFinder {
    };
    static QTextBoundaryFinder* iterator = 0;
    static unsigned char buffer[1024];

    TextBreakIterator* wordBreakIterator(const UChar* string, int length)
    {
        if (!string || !length)
            return 0;
        if (!iterator)
            iterator = new QTextBoundaryFinder;

        *iterator = QTextBoundaryFinder(QTextBoundaryFinder::Word, (const QChar *)string, length, buffer, sizeof(buffer));
        return static_cast<TextBreakIterator*>(iterator);
    }

    TextBreakIterator* characterBreakIterator(const UChar* string, int length)
    {
        if (!string || !length)
            return 0;
        if (!iterator)
            iterator = new QTextBoundaryFinder;

        *iterator = QTextBoundaryFinder(QTextBoundaryFinder::Grapheme, (const QChar *)string, length, buffer, sizeof(buffer));
        return static_cast<TextBreakIterator*>(iterator);
    }

    TextBreakIterator* cursorMovementIterator(const UChar* string, int length)
    {
        return characterBreakIterator(string, length);
    }

    TextBreakIterator* lineBreakIterator(const UChar* string, int length)
    {
        static QTextBoundaryFinder *iterator = 0;
        if (!string || !length)
            return 0;
        if (!iterator)
            iterator = new QTextBoundaryFinder;

        *iterator = QTextBoundaryFinder(QTextBoundaryFinder::Line, (const QChar *)string, length, buffer, sizeof(buffer));
        return static_cast<TextBreakIterator*>(iterator);
    }

    TextBreakIterator* sentenceBreakIterator(const UChar* string, int length)
    {
        if (!string || !length)
            return 0;
        if (!iterator)
            iterator = new QTextBoundaryFinder;

        *iterator = QTextBoundaryFinder(QTextBoundaryFinder::Sentence, (const QChar *)string, length, buffer, sizeof(buffer));
        return static_cast<TextBreakIterator*>(iterator);
    }

    int textBreakFirst(TextBreakIterator* bi)
    {
        bi->toStart();
        DEBUG() << "textBreakFirst" << bi->position();
        return bi->position();
    }

    int textBreakNext(TextBreakIterator* bi)
    {
        int pos = bi->toNextBoundary();
        DEBUG() << "textBreakNext" << pos;
        return pos;
    }

    int textBreakPreceding(TextBreakIterator* bi, int pos)
    {
        bi->setPosition(pos);
        int newpos = bi->toPreviousBoundary();
        DEBUG() << "textBreakPreceding" << pos << newpos;
        return newpos;
    }

    int textBreakFollowing(TextBreakIterator* bi, int pos)
    {
        bi->setPosition(pos);
        int newpos = bi->toNextBoundary();
        DEBUG() << "textBreakFollowing" << pos << newpos;
        return newpos;
    }

    int textBreakCurrent(TextBreakIterator* bi)
    {
        return bi->position();
    }

    bool isTextBreak(TextBreakIterator*, int)
    {
        return true;
    }

}
