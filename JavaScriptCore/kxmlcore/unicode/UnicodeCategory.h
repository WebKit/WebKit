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

#ifndef KJS_UNICODE_COMMON_H
#define KJS_UNICODE_COMMON_H

namespace KXMLCore {
  namespace Unicode {
    enum CharCategory {
      NoCategory = 0,
      Mark_NonSpacing = 0x00000001,          // Unicode class name Mn
      Mark_SpacingCombining = 0x00000002,    // Unicode class name Mc
      Mark_Enclosing = 0x00000004,           // Unicode class name Me
      Number_DecimalDigit = 0x00000008,      // Unicode class name Nd
      Number_Letter = 0x00000010,            // Unicode class name Nl
      Number_Other = 0x00000020,             // Unicode class name No
      Separator_Space = 0x00000040,          // Unicode class name Zs
      Separator_Line = 0x00000080,           // Unicode class name Zl
      Separator_Paragraph = 0x00000100,      // Unicode class name Zp
      Other_Control = 0x00000200,            // Unicode class name Cc
      Other_Format = 0x00000400,             // Unicode class name Cf
      Other_Surrogate = 0x00000800,          // Unicode class name Cs
      Other_PrivateUse = 0x00001000,         // Unicode class name Co
      Other_NotAssigned = 0x00002000,        // Unicode class name Cn
      Letter_Uppercase = 0x00004000,         // Unicode class name Lu
      Letter_Lowercase = 0x00008000,         // Unicode class name Ll
      Letter_Titlecase = 0x00010000,         // Unicode class name Lt
      Letter_Modifier = 0x00020000,          // Unicode class name Lm
      Letter_Other = 0x00040000,             // Unicode class name Lo
      Punctuation_Connector = 0x00080000,    // Unicode class name Pc
      Punctuation_Dash = 0x00100000,         // Unicode class name Pd
      Punctuation_Open = 0x00200000,         // Unicode class name Ps
      Punctuation_Close = 0x00400000,        // Unicode class name Pe
      Punctuation_InitialQuote = 0x00800000, // Unicode class name Pi
      Punctuation_FinalQuote = 0x01000000,   // Unicode class name Pf
      Punctuation_Other = 0x02000000,        // Unicode class name Po
      Symbol_Math = 0x04000000,              // Unicode class name Sm
      Symbol_Currency = 0x08000000,          // Unicode class name Sc
      Symbol_Modifier = 0x10000000,          // Unicode class name Sk
      Symbol_Other = 0x20000000              // Unicode class name So
    };
  }
}

#endif
// vim: ts=2 sw=2 et
