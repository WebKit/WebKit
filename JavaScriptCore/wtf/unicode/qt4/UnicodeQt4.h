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

#include <stdint.h>

typedef uint16_t UChar;
typedef uint32_t UChar32;

// some defines from ICU needed one or two places

#define U16_IS_LEAD(c) (((c)&0xfffffc00)==0xd800)
#define U16_IS_TRAIL(c) (((c)&0xfffffc00)==0xdc00)
#define U16_SURROGATE_OFFSET ((0xd800<<10UL)+0xdc00-0x10000)
#define U16_GET_SUPPLEMENTARY(lead, trail) \
    (((UChar32)(lead)<<10UL)+(UChar32)(trail)-U16_SURROGATE_OFFSET)

#define U16_LEAD(supplementary) (UChar)(((supplementary)>>10)+0xd7c0)
#define U16_TRAIL(supplementary) (UChar)(((supplementary)&0x3ff)|0xdc00)

#define U_IS_SURROGATE(c) (((c)&0xfffff800)==0xd800)
#define U16_IS_SURROGATE(c) U_IS_SURROGATE(c)
#define U16_IS_SURROGATE_LEAD(c) (((c)&0x400)==0)

#define U16_NEXT(s, i, length, c) { \
    (c)=(s)[(i)++]; \
    if(U16_IS_LEAD(c)) { \
        uint16_t __c2; \
        if((i)<(length) && U16_IS_TRAIL(__c2=(s)[(i)])) { \
            ++(i); \
            (c)=U16_GET_SUPPLEMENTARY((c), __c2); \
        } \
    } \
}

#define U_MASK(x) ((uint32_t)1<<(x))

namespace WTF {
  namespace Unicode {

    enum Direction {
      LeftToRight = QChar::DirL,
      RightToLeft = QChar::DirR,
      EuropeanNumber = QChar::DirEN,
      EuropeanNumberSeparator = QChar::DirES,
      EuropeanNumberTerminator = QChar::DirET,
      ArabicNumber = QChar::DirAN,
      CommonNumberSeparator = QChar::DirCS,
      BlockSeparator = QChar::DirB,
      SegmentSeparator = QChar::DirS,
      WhiteSpaceNeutral = QChar::DirWS,
      OtherNeutral = QChar::DirON,
      LeftToRightEmbedding = QChar::DirLRE,
      LeftToRightOverride = QChar::DirLRO,
      RightToLeftArabic = QChar::DirAL,
      RightToLeftEmbedding = QChar::DirRLE,
      RightToLeftOverride = QChar::DirRLO,
      PopDirectionalFormat = QChar::DirPDF,
      NonSpacingMark = QChar::DirNSM,
      BoundaryNeutral = QChar::DirBN
    };

    enum DecompositionType {
      DecompositionNone = QChar::NoDecomposition,
      DecompositionCanonical = QChar::Canonical,
      DecompositionCompat = QChar::Compat,
      DecompositionCircle = QChar::Circle,
      DecompositionFinal = QChar::Final,
      DecompositionFont = QChar::Font,
      DecompositionFraction = QChar::Fraction,
      DecompositionInitial = QChar::Initial,
      DecompositionIsolated = QChar::Isolated,
      DecompositionMedial = QChar::Medial,
      DecompositionNarrow = QChar::Narrow,
      DecompositionNoBreak = QChar::NoBreak,
      DecompositionSmall = QChar::Small,
      DecompositionSquare = QChar::Square,
      DecompositionSub = QChar::Sub,
      DecompositionSuper = QChar::Super,
      DecompositionVertical = QChar::Vertical,
      DecompositionWide = QChar::Wide
    };

    enum CharCategory {
      NoCategory = 0,
      Mark_NonSpacing = U_MASK(QChar::Mark_NonSpacing),
      Mark_SpacingCombining = U_MASK(QChar::Mark_SpacingCombining),
      Mark_Enclosing = U_MASK(QChar::Mark_Enclosing),
      Number_DecimalDigit = U_MASK(QChar::Number_DecimalDigit),
      Number_Letter = U_MASK(QChar::Number_Letter),
      Number_Other = U_MASK(QChar::Number_Other),
      Separator_Space = U_MASK(QChar::Separator_Space),
      Separator_Line = U_MASK(QChar::Separator_Line),
      Separator_Paragraph = U_MASK(QChar::Separator_Paragraph),
      Other_Control = U_MASK(QChar::Other_Control),
      Other_Format = U_MASK(QChar::Other_Format),
      Other_Surrogate = U_MASK(QChar::Other_Surrogate),
      Other_PrivateUse = U_MASK(QChar::Other_PrivateUse),
      Other_NotAssigned = U_MASK(QChar::Other_NotAssigned),
      Letter_Uppercase = U_MASK(QChar::Letter_Uppercase),
      Letter_Lowercase = U_MASK(QChar::Letter_Lowercase),
      Letter_Titlecase = U_MASK(QChar::Letter_Titlecase),
      Letter_Modifier = U_MASK(QChar::Letter_Modifier),
      Letter_Other = U_MASK(QChar::Letter_Other),
      Punctuation_Connector = U_MASK(QChar::Punctuation_Connector),
      Punctuation_Dash = U_MASK(QChar::Punctuation_Dash),
      Punctuation_Open = U_MASK(QChar::Punctuation_Open),
      Punctuation_Close = U_MASK(QChar::Punctuation_Close),
      Punctuation_InitialQuote = U_MASK(QChar::Punctuation_InitialQuote),
      Punctuation_FinalQuote = U_MASK(QChar::Punctuation_FinalQuote),
      Punctuation_Other = U_MASK(QChar::Punctuation_Other),
      Symbol_Math = U_MASK(QChar::Symbol_Math),
      Symbol_Currency = U_MASK(QChar::Symbol_Currency),
      Symbol_Modifier = U_MASK(QChar::Symbol_Modifier),
      Symbol_Other = U_MASK(QChar::Symbol_Other),
    };


#if QT_VERSION >= 0x040300
    // FIXME: handle surrogates correctly in all methods
    
    inline int toLower(UChar* str, int strLength, UChar*& destIfNeeded)
    {
      destIfNeeded = 0;

      // FIXME: handle special casing. Easiest with some low level API in Qt
      for (int i = 0; i < strLength; ++i)
        str[i] = QChar::toLower(str[i]);

      return strLength;
    }

    inline UChar32 toLower(UChar32 ch)
    {
      return QChar::toLower(ch);
    }

    inline int toLower(UChar* result, int resultLength, const UChar* src, int srcLength,  bool* error)
    {
      // FIXME: handle special casing. Easiest with some low level API in Qt
      *error = false;
      if (resultLength < srcLength) {
        *error = true;
        return srcLength;
      }
      for (int i = 0; i < srcLength; ++i)
        result[i] = QChar::toLower(src[i]);
      return srcLength;
    }

    inline int toUpper(UChar* str, int strLength, UChar*& destIfNeeded)
    {
      // FIXME: handle special casing. Easiest with some low level API in Qt
      destIfNeeded = 0;

      for (int i = 0; i < strLength; ++i)
        str[i] = QChar::toUpper(str[i]);

      return strLength;
    }

    inline UChar32 toUpper(UChar32 ch)
    {
      return QChar::toUpper(ch);
    }

    inline int toUpper(UChar* result, int resultLength, UChar* src, int srcLength,  bool* error)
    {
      // FIXME: handle special casing. Easiest with some low level API in Qt
      *error = false;
      if (resultLength < srcLength) {
        *error = true;
        return srcLength;
      }
      for (int i = 0; i < srcLength; ++i)
        result[i] = QChar::toUpper(src[i]);
      return srcLength;
    }

    inline int toTitleCase(UChar32 c)
    {
      return QChar::toTitleCase(c);
    }

    inline UChar32 foldCase(UChar32 c)
    {
      return QChar::toCaseFolded(c);
    }

    inline int foldCase(UChar* result, int resultLength, UChar* src, int srcLength,  bool* error)
    {
      // FIXME: handle special casing. Easiest with some low level API in Qt
      *error = false;
      if (resultLength < srcLength) {
        *error = true;
        return srcLength;
      }
      for (int i = 0; i < srcLength; ++i)
        result[i] = QChar::toCaseFolded(src[i]);
      return srcLength;
    }

    inline bool isFormatChar(UChar32 c)
    {
      return QChar::category(c) == QChar::Other_Format;
    }

    inline bool isPrintableChar(UChar32 c)
    {
      const uint test = U_MASK(QChar::Other_Control) |
                        U_MASK(QChar::Other_NotAssigned);
      return !(U_MASK(QChar::category(c)) & test);
    }

    inline bool isSeparatorSpace(UChar32 c)
    {
      return QChar::category(c) == QChar::Separator_Space;
    }

    inline bool isPunct(UChar32 c)
    {
      const uint test = U_MASK(QChar::Punctuation_Connector) |
                        U_MASK(QChar::Punctuation_Dash) |
                        U_MASK(QChar::Punctuation_Open) |
                        U_MASK(QChar::Punctuation_Close) |
                        U_MASK(QChar::Punctuation_InitialQuote) |
                        U_MASK(QChar::Punctuation_FinalQuote) |
                        U_MASK(QChar::Punctuation_Other);
      return U_MASK(QChar::category(c)) & test;
    }

    inline bool isDigit(UChar32 c)
    {
      return QChar::category(c) == QChar::Number_DecimalDigit;
    }

    inline bool isLower(UChar32 c)
    {
      return QChar::category(c) == QChar::Letter_Lowercase;
    }

    inline bool isUpper(UChar32 c)
    {
      return QChar::category(c) == QChar::Letter_Uppercase;
    }

    inline int digitValue(UChar32 c)
    {
      return QChar::digitValue(c);
    }

    inline UChar32 mirroredChar(UChar32 c)
    {
      return QChar::mirroredChar(c);
    }

    inline uint8_t combiningClass(UChar32 c)
    {
      return QChar::combiningClass(c);
    }

    inline DecompositionType decompositionType(UChar32 c)
    {
      return (DecompositionType)QChar::decompositionTag(c);
    }

    inline int umemcasecmp(const UChar* a, const UChar* b, int len)
    {
      // handle surrogates correctly
      for (int i = 0; i < len; ++i) {
        QChar c1 = QChar(a[i]).toCaseFolded();
        QChar c2 = QChar(b[i]).toCaseFolded();
        if (c1 != c2)
          return c1 < c2;
      }
      return 0;
    }

    inline Direction direction(UChar32 c)
    {
      return (Direction)QChar::direction(c);
    }

    inline CharCategory category(UChar32 c)
    {
      return (CharCategory) U_MASK(QChar::category(c));
    }
    
#else

    inline int toLower(UChar* str, int strLength, UChar*& destIfNeeded)
    {
      destIfNeeded = 0;

      for (int i = 0; i < strLength; ++i)
        str[i] = QChar(str[i]).toLower().unicode();

      return strLength;
    }

    inline UChar32 toLower(UChar32 ch)
    {
      if (ch > 0xffff)
        return ch;
      return QChar((unsigned short)ch).toLower().unicode();
    }

    inline int toLower(UChar* result, int resultLength, const UChar* src, int srcLength,  bool* error)
    {
      *error = false;
      if (resultLength < srcLength) {
        *error = true;
        return srcLength;
      }
      for (int i = 0; i < srcLength; ++i)
        result[i] = QChar(src[i]).toLower().unicode();
      return srcLength;
    }

    inline int toUpper(UChar* str, int strLength, UChar*& destIfNeeded)
    {
      destIfNeeded = 0;

      for (int i = 0; i < strLength; ++i)
        str[i] = QChar(str[i]).toUpper().unicode();

      return strLength;
    }

    inline UChar32 toUpper(UChar32 ch)
    {
      if (ch > 0xffff)
        return ch;
      return QChar((unsigned short)ch).toUpper().unicode();
    }

    inline int toUpper(UChar* result, int resultLength, UChar* src, int srcLength,  bool* error)
    {
      *error = false;
      if (resultLength < srcLength) {
        *error = true;
        return srcLength;
      }
      for (int i = 0; i < srcLength; ++i)
        result[i] = QChar(src[i]).toUpper().unicode();
      return srcLength;
    }

    inline int toTitleCase(UChar32 c)
    {
      if (c > 0xffff)
        return c;
      return QChar((unsigned short)c).toUpper().unicode();
    }

    inline UChar32 foldCase(UChar32 c)
    {
      if (c > 0xffff)
        return c;
      return QChar((unsigned short)c).toLower().unicode();
    }

    inline int foldCase(UChar* result, int resultLength, UChar* src, int srcLength,  bool* error)
    {
      return toLower(result, resultLength, src, srcLength, error);
    }

    inline bool isFormatChar(UChar32 c)
    {
      return (c & 0xffff0000) == 0 && QChar((unsigned short)c).category() == QChar::Other_Format;
    }

    inline bool isPrintableChar(UChar32 c)
    {
      return (c & 0xffff0000) == 0 && QChar((unsigned short)c).isPrint();
    }

    inline bool isSeparatorSpace(UChar32 c)
    {
      return (c & 0xffff0000) == 0 && QChar((unsigned short)c).category() == QChar::Separator_Space;
    }

    inline bool isPunct(UChar32 c)
    {
      return (c & 0xffff0000) == 0 && QChar((unsigned short)c).isPunct();
    }

    inline bool isDigit(UChar32 c)
    {
      return (c & 0xffff0000) == 0 && QChar((unsigned short)c).isDigit();
    }

    inline bool isLower(UChar32 c)
    {
      return (c & 0xffff0000) == 0 && QChar((unsigned short)c).category() == QChar::Letter_Lowercase;
    }

    inline bool isUpper(UChar32 c)
    {
      return (c & 0xffff0000) == 0 && QChar((unsigned short)c).category() == QChar::Letter_Uppercase;
    }

    inline int digitValue(UChar32 c)
    {
      if (c > 0xffff)
        return 0;
      return QChar(c).digitValue();
    }

    inline UChar32 mirroredChar(UChar32 c)
    {
      if (c > 0xffff)
        return c;
      return QChar(c).mirroredChar().unicode();
    }

    inline uint8_t combiningClass(UChar32 c)
    {
      if (c > 0xffff)
        return 0;
      return QChar((unsigned short)c).combiningClass();
    }

    inline DecompositionType decompositionType(UChar32 c)
    {
      if (c > 0xffff)
        return DecompositionNone;
      return (DecompositionType)QChar(c).decompositionTag();
    }

    inline int umemcasecmp(const UChar* a, const UChar* b, int len)
    {
      for (int i = 0; i < len; ++i) {
        QChar c1 = QChar(a[i]).toLower();
        QChar c2 = QChar(b[i]).toLower();
        if (c1 != c2)
          return c1 < c2;
      }
      return 0;
    }

    inline Direction direction(UChar32 c)
    {
      if (c > 0xffff)
        return LeftToRight;
      return (Direction)QChar(c).direction();
    }

    inline CharCategory category(UChar32 c)
    {
      if (c > 0xffff)
        return NoCategory;
      return (CharCategory) U_MASK(QChar(c).category());
    }

#endif
    
  }
}

#endif
// vim: ts=2 sw=2 et
