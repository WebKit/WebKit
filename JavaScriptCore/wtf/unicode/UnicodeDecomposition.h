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

#ifndef KJS_UNICODE_DECOMPOSITION_H
#define KJS_UNICODE_DECOMPOSITION_H

namespace WTF {
  namespace Unicode {
    enum Decomposition {
      None = 0,
      Canonical = 0x00000001,
      Font = 0x00000002,
      NoBreak = 0x00000004,
      Initial = 0x00000008,
      Medial = 0x00000010,
      Final = 0x00000020,
      Isolated = 0x00000040,
      Circle = 0x00000080,
      Super = 0x00000100,
      Sub = 0x00000200,
      Vertical = 0x00000400,
      Wide = 0x00000800,
      Narrow = 0x00001000,
      Small = 0x00002000,
      Square = 0x00004000,
      Compat = 0x00008000,
      Fraction = 0x00010000
    };
  }
}

#endif
// vim: ts=2 sw=2 et
