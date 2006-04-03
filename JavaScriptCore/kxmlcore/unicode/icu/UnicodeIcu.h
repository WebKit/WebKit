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

#ifndef KJS_UNICODE_ICU_H
#define KJS_UNICODE_ICU_H

#include <unicode/uchar.h>

#include "../UnicodeCategory.h"

namespace KXMLCore {
  namespace Unicode {
    inline unsigned short toLower(unsigned short c) {
      return u_tolower(c);
    }

    inline unsigned short toUpper(unsigned short c) {
      return u_toupper(c);
    }

    inline bool isFormatChar(unsigned short c) {
      return u_charType(c) == U_FORMAT_CHAR;
    }

    inline bool isSeparatorSpace(unsigned short c) {
      return u_charType(c) == U_SPACE_SEPARATOR;
    }

    inline CharCategory category(unsigned short c) {
      switch (u_charType(c)) {
        case U_NON_SPACING_MARK:
          return Mark_NonSpacing;
        case U_COMBINING_SPACING_MARK:
          return Mark_SpacingCombining;
        case U_ENCLOSING_MARK:
          return Mark_Enclosing;
        case U_DECIMAL_DIGIT_NUMBER:
          return Number_DecimalDigit;
        case U_LETTER_NUMBER:
          return Number_Letter;
        case U_OTHER_NUMBER:
          return Number_Other;
        case U_SPACE_SEPARATOR:
          return Separator_Space;
        case U_LINE_SEPARATOR:
          return Separator_Line;
        case U_PARAGRAPH_SEPARATOR:
          return Separator_Paragraph;
        case U_CONTROL_CHAR:
          return Other_Control;
        case U_FORMAT_CHAR:
          return Other_Format;
        case U_SURROGATE:
          return Other_Surrogate;
        case U_PRIVATE_USE_CHAR:
          return Other_PrivateUse;
        case U_GENERAL_OTHER_TYPES:
          return Other_NotAssigned;
        case U_UPPERCASE_LETTER:
          return Letter_Uppercase;
        case U_LOWERCASE_LETTER:
          return Letter_Lowercase;
        case U_TITLECASE_LETTER:
          return Letter_Titlecase;
        case U_MODIFIER_LETTER:
          return Letter_Modifier;
        case U_OTHER_LETTER:
          return Letter_Other;
        case U_CONNECTOR_PUNCTUATION:
          return Punctuation_Connector;
        case U_DASH_PUNCTUATION:
          return Punctuation_Dash;
        case U_START_PUNCTUATION:
          return Punctuation_Open;
        case U_END_PUNCTUATION:
          return Punctuation_Close;
        case U_INITIAL_PUNCTUATION:
          return Punctuation_InitialQuote;
        case U_FINAL_PUNCTUATION:
          return Punctuation_FinalQuote;
        case U_OTHER_PUNCTUATION:
          return Punctuation_Other;
        case U_MATH_SYMBOL:
          return Symbol_Math;
        case U_CURRENCY_SYMBOL:
          return Symbol_Currency;
        case U_MODIFIER_SYMBOL:
          return Symbol_Modifier;
        case U_OTHER_SYMBOL:
          return Symbol_Other;
        default:
          return NoCategory;
      }
    }
  }
}

#endif
// vim: ts=2 sw=2 et
