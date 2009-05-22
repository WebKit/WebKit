/*
 *  Copyright (C) 2008 Jürg Billeter <j@bitron.ch>
 *  Copyright (C) 2008 Dominik Röttsches <dominik.roettsches@access-company.com>
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

#include "UnicodeGLib.h"

namespace WTF {
namespace Unicode {

UChar32 foldCase(UChar32 ch)
{
    GOwnPtr<GError> gerror;

    GOwnPtr<char> utf8char;
    utf8char.set(g_ucs4_to_utf8(reinterpret_cast<gunichar*>(&ch), 1, 0, 0, &gerror.outPtr()));
    if (gerror)
        return ch;

    GOwnPtr<char> utf8caseFolded;
    utf8caseFolded.set(g_utf8_casefold(utf8char.get(), -1));

    GOwnPtr<gunichar> ucs4Result;
    ucs4Result.set(g_utf8_to_ucs4_fast(utf8caseFolded.get(), -1, 0));

    return *ucs4Result;
}

int foldCase(UChar* result, int resultLength, const UChar* src, int srcLength, bool* error)
{
    *error = false;
    GOwnPtr<GError> gerror;

    GOwnPtr<char> utf8src;
    utf8src.set(g_utf16_to_utf8(src, srcLength, 0, 0, &gerror.outPtr()));
    if (gerror) {
        *error = true;
        return -1;
    }

    GOwnPtr<char> utf8result;
    utf8result.set(g_utf8_casefold(utf8src.get(), -1));

    long utf16resultLength = -1;
    GOwnPtr<UChar> utf16result;
    utf16result.set(g_utf8_to_utf16(utf8result.get(), -1, 0, &utf16resultLength, &gerror.outPtr()));
    if (gerror) {
        *error = true;
        return -1;
    }

    if (utf16resultLength > resultLength) {
        *error = true;
        return utf16resultLength;
    }
    memcpy(result, utf16result.get(), utf16resultLength * sizeof(UChar));

    return utf16resultLength;
}

int toLower(UChar* result, int resultLength, const UChar* src, int srcLength, bool* error)
{
    *error = false;
    GOwnPtr<GError> gerror;

    GOwnPtr<char> utf8src;
    utf8src.set(g_utf16_to_utf8(src, srcLength, 0, 0, &gerror.outPtr()));
    if (gerror) {
        *error = true;
        return -1;
    }

    GOwnPtr<char> utf8result;
    utf8result.set(g_utf8_strdown(utf8src.get(), -1));

    long utf16resultLength = -1;
    GOwnPtr<UChar> utf16result;
    utf16result.set(g_utf8_to_utf16(utf8result.get(), -1, 0, &utf16resultLength, &gerror.outPtr()));
    if (gerror) {
        *error = true;
        return -1;
    }

    if (utf16resultLength > resultLength) {
        *error = true;
        return utf16resultLength;
    }
    memcpy(result, utf16result.get(), utf16resultLength * sizeof(UChar));

    return utf16resultLength;
}

int toUpper(UChar* result, int resultLength, const UChar* src, int srcLength, bool* error)
{
    *error = false;
    GOwnPtr<GError> gerror;

    GOwnPtr<char> utf8src;
    utf8src.set(g_utf16_to_utf8(src, srcLength, 0, 0, &gerror.outPtr()));
    if (gerror) {
        *error = true;
        return -1;
    }

    GOwnPtr<char> utf8result;
    utf8result.set(g_utf8_strup(utf8src.get(), -1));

    long utf16resultLength = -1;
    GOwnPtr<UChar> utf16result;
    utf16result.set(g_utf8_to_utf16(utf8result.get(), -1, 0, &utf16resultLength, &gerror.outPtr()));
    if (gerror) {
        *error = true;
        return -1;
    }

    if (utf16resultLength > resultLength) {
        *error = true;
        return utf16resultLength;
    }
    memcpy(result, utf16result.get(), utf16resultLength * sizeof(UChar));

    return utf16resultLength;
}

Direction direction(UChar32 c)
{
    PangoBidiType type = pango_bidi_type_for_unichar(c);
    switch (type) {
    case PANGO_BIDI_TYPE_L:
        return LeftToRight;
    case PANGO_BIDI_TYPE_R:
        return RightToLeft;
    case PANGO_BIDI_TYPE_AL:
        return RightToLeftArabic;
    case PANGO_BIDI_TYPE_LRE:
        return LeftToRightEmbedding;
    case PANGO_BIDI_TYPE_RLE:
        return RightToLeftEmbedding;
    case PANGO_BIDI_TYPE_LRO:
        return LeftToRightOverride;
    case PANGO_BIDI_TYPE_RLO:
        return RightToLeftOverride;
    case PANGO_BIDI_TYPE_PDF:
        return PopDirectionalFormat;
    case PANGO_BIDI_TYPE_EN:
        return EuropeanNumber;
    case PANGO_BIDI_TYPE_AN:
        return ArabicNumber;
    case PANGO_BIDI_TYPE_ES:
        return EuropeanNumberSeparator;
    case PANGO_BIDI_TYPE_ET:
        return EuropeanNumberTerminator;
    case PANGO_BIDI_TYPE_CS:
        return CommonNumberSeparator;
    case PANGO_BIDI_TYPE_NSM:
        return NonSpacingMark;
    case PANGO_BIDI_TYPE_BN:
        return BoundaryNeutral;
    case PANGO_BIDI_TYPE_B:
        return BlockSeparator;
    case PANGO_BIDI_TYPE_S:
        return SegmentSeparator;
    case PANGO_BIDI_TYPE_WS:
        return WhiteSpaceNeutral;
    default:
        return OtherNeutral;
    }
}

int umemcasecmp(const UChar* a, const UChar* b, int len)
{
    GOwnPtr<char> utf8a;
    GOwnPtr<char> utf8b;

    utf8a.set(g_utf16_to_utf8(a, len, 0, 0, 0));
    utf8b.set(g_utf16_to_utf8(b, len, 0, 0, 0));

    GOwnPtr<char> foldedA;
    GOwnPtr<char> foldedB;

    foldedA.set(g_utf8_casefold(utf8a.get(), -1));
    foldedB.set(g_utf8_casefold(utf8b.get(), -1));

    // FIXME: umemcasecmp needs to mimic u_memcasecmp of icu
    // from the ICU docs:
    // "Compare two strings case-insensitively using full case folding.
    // his is equivalent to u_strcmp(u_strFoldCase(s1, n, options), u_strFoldCase(s2, n, options))."
    //
    // So it looks like we don't need the full g_utf8_collate here,
    // but really a bitwise comparison of casefolded unicode chars (not utf-8 bytes).
    // As there is no direct equivalent to this icu function in GLib, for now
    // we'll use g_utf8_collate():

    return g_utf8_collate(foldedA.get(), foldedB.get());
}

}
}
