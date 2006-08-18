// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2006 George Staikos <staikos@kde.org>
 *  Copyright (C) 2006 Alexey Proskuryakov <ap@nypop.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef KJS_UNICODE_QT4_H
#define KJS_UNICODE_QT4_H

#include <QChar>
#include <QString>

#include <config.h>
#include <kjs/ustring.h>

#include "../UnicodeCategory.h"
#include "../UnicodeDecomposition.h"
#include "../UnicodeDirection.h"

#include <stdint.h>

namespace WTF {
  namespace Unicode {
    inline int toLower(uint16_t* str, int strLength, uint16_t*& destIfNeeded)
    {
      destIfNeeded = 0;

      for (int i = 0; i < strLength; ++i)
        str[i] = QChar(str[i]).toLower().unicode();

      return strLength;
    }

    inline uint16_t toLower(uint16_t ch)
    {
      return QChar(ch).toLower().unicode();
    }

    inline int toUpper(uint16_t* str, int strLength, uint16_t*& destIfNeeded)
    {
      destIfNeeded = 0;

      for (int i = 0; i < strLength; ++i)
        str[i] = QChar(str[i]).toUpper().unicode();

      return strLength;
    }

    inline uint16_t toUpper(uint16_t ch)
    {
      return QChar(ch).toUpper().unicode();
    }

    inline bool isFormatChar(int32_t c)
    {
      return (c & 0xffff0000) == 0 && QChar((unsigned short)c).category() == QChar::Other_Format;
    }

    inline bool isPrintableChar(int32_t c)
    {
      return (c & 0xffff0000) == 0 && QChar((unsigned short)c).isPrint(); 
    }

    inline bool isSeparatorSpace(int32_t c)
    {
      return (c & 0xffff0000) == 0 && QChar((unsigned short)c).category() == QChar::Separator_Space;
    }

    inline bool isSpace(int32_t c)
    {
      return (c & 0xffff0000) == 0 && QChar((unsigned short)c).isSpace();
    }

    inline bool isPunct(int32_t c)
    {
      return (c & 0xffff0000) == 0 && QChar((unsigned short)c).isPunct();
    }

    inline bool isDigit(int32_t c)
    {
      return (c & 0xffff0000) == 0 && QChar((unsigned short)c).isDigit();
    }

    inline uint16_t mirroredChar(uint16_t c)
    {
      return QChar(c).mirroredChar().unicode();
    }

    inline int compare(const KJS::UChar *a, const KJS::UChar *b, int len, bool caseSensitive = true)
    {
      const QString left(reinterpret_cast<const QChar *>(a), len);
      const QString right(reinterpret_cast<const QChar *>(b), len);
      return left.compare(right, caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
    }

    Direction direction(int c);

    CharCategory category(int c);

    Decomposition decomposition(int c);
  }
}

#endif
// vim: ts=2 sw=2 et
