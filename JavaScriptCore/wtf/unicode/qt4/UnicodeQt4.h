/*
 *  Copyright (C) 2006 George Staikos <staikos@kde.org>
 *  Copyright (C) 2006 Alexey Proskuryakov <ap@nypop.com>
 *  Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef WTF_UNICODE_QT4_H
#define WTF_UNICODE_QT4_H

#include <QChar>
#include <QString>

#include <config.h>

#include <stdint.h>

QT_BEGIN_NAMESPACE
namespace QUnicodeTables {
    struct Properties {
        ushort category : 8;
        ushort line_break_class : 8;
        ushort direction : 8;
        ushort combiningClass :8;
        ushort joining : 2;
        signed short digitValue : 6; /* 5 needed */
        ushort unicodeVersion : 4;
        ushort lowerCaseSpecial : 1;
        ushort upperCaseSpecial : 1;
        ushort titleCaseSpecial : 1;
        ushort caseFoldSpecial : 1; /* currently unused */
        signed short mirrorDiff : 16;
        signed short lowerCaseDiff : 16;
        signed short upperCaseDiff : 16;
        signed short titleCaseDiff : 16;
        signed short caseFoldDiff : 16;
    };
    Q_CORE_EXPORT const Properties * QT_FASTCALL properties(uint ucs4);
    Q_CORE_EXPORT const Properties * QT_FASTCALL properties(ushort ucs2);
}
QT_END_NAMESPACE

// ugly hack to make UChar compatible with JSChar in API/JSStringRef.h
#if defined(Q_OS_WIN) || COMPILER(WINSCW) || (COMPILER(RVCT) && OS(SYMBIAN))
typedef wchar_t UChar;
#else
typedef uint16_t UChar;
#endif
typedef uint32_t UChar32;

// some defines from ICU

#define U16_IS_LEAD(c) (((c)&0xfffffc00)==0xd800)
#define U16_IS_TRAIL(c) (((c)&0xfffffc00)==0xdc00)
#define U16_SURROGATE_OFFSET ((0xd800<<10UL)+0xdc00-0x10000)
#define U16_GET_SUPPLEMENTARY(lead, trail) \
    (((UChar32)(lead)<<10UL)+(UChar32)(trail)-U16_SURROGATE_OFFSET)

#define U16_LEAD(supplementary) (UChar)(((supplementary)>>10)+0xd7c0)
#define U16_TRAIL(supplementary) (UChar)(((supplementary)&0x3ff)|0xdc00)

#define U_IS_SURROGATE(c) (((c)&0xfffff800)==0xd800)
#define U16_IS_SINGLE(c) !U_IS_SURROGATE(c)
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

#define U16_PREV(s, start, i, c) { \
    (c)=(s)[--(i)]; \
    if(U16_IS_TRAIL(c)) { \
        uint16_t __c2; \
        if((i)>(start) && U16_IS_LEAD(__c2=(s)[(i)-1])) { \
            --(i); \
            (c)=U16_GET_SUPPLEMENTARY(__c2, (c)); \
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
    Symbol_Other = U_MASK(QChar::Symbol_Other)
};


// FIXME: handle surrogates correctly in all methods

inline UChar32 toLower(UChar32 ch)
{
    return QChar::toLower(ch);
}

inline int toLower(UChar* result, int resultLength, const UChar* src, int srcLength,  bool* error)
{
    const UChar *e = src + srcLength;
    const UChar *s = src;
    UChar *r = result;
    uint rindex = 0;

    // this avoids one out of bounds check in the loop
    if (s < e && QChar(*s).isLowSurrogate()) {
        if (r)
            r[rindex] = *s++;
        ++rindex;
    }

    int needed = 0;
    while (s < e && (rindex < uint(resultLength) || !r)) {
        uint c = *s;
        if (QChar(c).isLowSurrogate() && QChar(*(s - 1)).isHighSurrogate())
            c = QChar::surrogateToUcs4(*(s - 1), c);
        const QUnicodeTables::Properties *prop = QUnicodeTables::properties(c);
        if (prop->lowerCaseSpecial) {
            QString qstring;
            if (c < 0x10000) {
                qstring += QChar(c);
            } else {
                qstring += QChar(*(s-1));
                qstring += QChar(*s);
            }
            qstring = qstring.toLower();
            for (int i = 0; i < qstring.length(); ++i) {
                if (rindex >= uint(resultLength)) {
                    needed += qstring.length() - i;
                    break;
                }
                if (r)
                    r[rindex] = qstring.at(i).unicode();
                ++rindex;
            }
        } else {
            if (r)
                r[rindex] = *s + prop->lowerCaseDiff;
            ++rindex;
        }
        ++s;
    }
    if (s < e)
        needed += e - s;
    *error = (needed != 0);
    if (rindex < uint(resultLength))
        r[rindex] = 0;
    return rindex + needed;
}

inline UChar32 toUpper(UChar32 ch)
{
    return QChar::toUpper(ch);
}

inline int toUpper(UChar* result, int resultLength, const UChar* src, int srcLength,  bool* error)
{
    const UChar *e = src + srcLength;
    const UChar *s = src;
    UChar *r = result;
    int rindex = 0;

    // this avoids one out of bounds check in the loop
    if (s < e && QChar(*s).isLowSurrogate()) {
        if (r)
            r[rindex] = *s++;
        ++rindex;
    }

    int needed = 0;
    while (s < e && (rindex < resultLength || !r)) {
        uint c = *s;
        if (QChar(c).isLowSurrogate() && QChar(*(s - 1)).isHighSurrogate())
            c = QChar::surrogateToUcs4(*(s - 1), c);
        const QUnicodeTables::Properties *prop = QUnicodeTables::properties(c);
        if (prop->upperCaseSpecial) {
            QString qstring;
            if (c < 0x10000) {
                qstring += QChar(c);
            } else {
                qstring += QChar(*(s-1));
                qstring += QChar(*s);
            }
            qstring = qstring.toUpper();
            for (int i = 0; i < qstring.length(); ++i) {
                if (rindex >= resultLength) {
                    needed += qstring.length() - i;
                    break;
                }
                if (r)
                    r[rindex] = qstring.at(i).unicode();
                ++rindex;
            }
        } else {
            if (r)
                r[rindex] = *s + prop->upperCaseDiff;
            ++rindex;
        }
        ++s;
    }
    if (s < e)
        needed += e - s;
    *error = (needed != 0);
    if (rindex < resultLength)
        r[rindex] = 0;
    return rindex + needed;
}

inline int toTitleCase(UChar32 c)
{
    return QChar::toTitleCase(c);
}

inline UChar32 foldCase(UChar32 c)
{
    return QChar::toCaseFolded(c);
}

inline int foldCase(UChar* result, int resultLength, const UChar* src, int srcLength,  bool* error)
{
    // FIXME: handle special casing. Easiest with some low level API in Qt
    *error = false;
    if (resultLength < srcLength) {
        *error = true;
        return srcLength;
    }
    for (int i = 0; i < srcLength; ++i)
        result[i] = QChar::toCaseFolded(ushort(src[i]));
    return srcLength;
}

inline bool isArabicChar(UChar32 c)
{
    return c >= 0x0600 && c <= 0x06FF;
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

inline bool isLower(UChar32 c)
{
    return QChar::category(c) == QChar::Letter_Lowercase;
}

inline bool hasLineBreakingPropertyComplexContext(UChar32)
{
    // FIXME: Implement this to return whether the character has line breaking property SA (Complex Context).
    return false;
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
        uint c1 = QChar::toCaseFolded(ushort(a[i]));
        uint c2 = QChar::toCaseFolded(ushort(b[i]));
        if (c1 != c2)
            return c1 - c2;
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

} }

#endif // WTF_UNICODE_QT4_H
