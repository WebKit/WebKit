// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2006 George Staikos <staikos@kde.org>
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

#include "UnicodeQt4.h"

using namespace WTF::Unicode;

Direction WTF::Unicode::direction(int c)
{
  // FIXME: implement support for non-BMP code points
  if ((c & 0xffff0000) != 0)
    return LeftToRight;

  switch (QChar((unsigned short)c).direction()) {
    case QChar::DirL:
      return LeftToRight;
    case QChar::DirR:
      return RightToLeft;
    case QChar::DirNSM:
      return NonSpacingMark;
    case QChar::DirBN:
      return BoundaryNeutral;
    case QChar::DirEN:
      return EuropeanNumber;
    case QChar::DirES:
      return EuropeanNumberSeparator;
    case QChar::DirET:
      return EuropeanNumberTerminator;
    case QChar::DirAN:
      return ArabicNumber;
    case QChar::DirCS:
 *  This file is part of the KDE libraries
 *  Copyright (C) 2006 George Staikos <staikos@kde.org>
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

#ifndef KJS_UNICODE_DIRECTION_H
#define KJS_UNICODE_DIRECTION_H

namespace WTF {
  namespace Unicode {
    enum Direction {
      LeftToRight = 0,
      RightToLeft = 0x00000001,
      EuropeanNumber = 0x00000002,
      EuropeanNumberSeparator = 0x00000004,
      EuropeanNumberTerminator = 0x00000008,
      ArabicNumber = 0x00000010,
      CommonNumberSeparator = 0x00000020,
      BlockSeparator = 0x00000040,
      SegmentSeparator = 0x00000080,
      WhiteSpaceNeutral = 0x00000100,
      OtherNeutral = 0x00000200,
      LeftToRightEmbedding = 0x00000400,
      LeftToRightOverride = 0x00000800,
      RightToLeftArabic = 0x00001000,
      RightToLeftEmbedding = 0x00002000,
      RightToLeftOverride = 0x00004000,
      PopDirectionalFormat = 0x00008000,
      NonSpacingMark = 0x00010000,
      BoundaryNeutral = 0x00020000
    };
  }
}

#endif
// vim: ts=2 sw=2 et
