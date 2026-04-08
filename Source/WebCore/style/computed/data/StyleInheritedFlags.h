/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2021 Google Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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

#pragma once

#include <WebCore/StyleNonInheritedFlags.h>
#include <WebCore/WritingMode.h>

namespace WebCore {

enum class BorderCollapse : bool;
enum class BoxDirection : bool;
enum class CaptionSide : uint8_t;
enum class CursorType : uint8_t;
enum class CursorVisibility : bool;
enum class EmptyCell : bool;
enum class InsideLink : uint8_t;
enum class ListStylePosition : bool;
enum class Order : bool;
enum class PointerEvents : uint8_t;
enum class PrintColorAdjust : bool;
enum class TextWrapMode : bool;
enum class TextWrapStyle : uint8_t;
enum class Visibility : uint8_t;
enum class WhiteSpaceCollapse : uint8_t;

namespace Style {

enum class TextAlign : uint8_t;

constexpr auto TextTransformBits = 6;

struct InheritedFlags {
    bool operator==(const InheritedFlags&) const = default;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const InheritedFlags&) const;
#endif

    // Writing Mode = 8 bits (can be packed into 6 if needed)
    WritingMode writingMode;

    // Text Formatting = 19 bits aligned onto 2 bytes + 4 trailing bits
    PREFERRED_TYPE(WhiteSpaceCollapse) unsigned char whiteSpaceCollapse : 3;
    PREFERRED_TYPE(TextWrapMode) unsigned char textWrapMode : 1;
    PREFERRED_TYPE(TextAlign) unsigned char textAlign : 4;
    PREFERRED_TYPE(TextWrapStyle) unsigned char textWrapStyle : 2;
    unsigned char textTransform : TextTransformBits; // PREFERRED_TYPE elided to avoid header inclusion.
    unsigned char : 1; // byte alignment
    unsigned char textDecorationLineInEffect : TextDecorationLineBits; // PREFERRED_TYPE elided to avoid header inclusion.

    // Cursors and Visibility = 13 bits aligned onto 4 bits + 1 byte + 1 bit
    PREFERRED_TYPE(PointerEvents) unsigned char pointerEvents : 4;
    PREFERRED_TYPE(Visibility) unsigned char visibility : 2;
    PREFERRED_TYPE(CursorType) unsigned char cursorType : 6;
#if ENABLE(CURSOR_VISIBILITY)
    PREFERRED_TYPE(CursorVisibility) unsigned char cursorVisibility : 1;
#endif

    // Display Type-Specific = 5 bits
    PREFERRED_TYPE(ListStylePosition) unsigned char listStylePosition : 1;
    PREFERRED_TYPE(EmptyCell) unsigned char emptyCells : 1;
    PREFERRED_TYPE(BorderCollapse) unsigned char borderCollapse : 1;
    PREFERRED_TYPE(CaptionSide) unsigned char captionSide : 2;

    // -webkit- Stuff = 2 bits
    PREFERRED_TYPE(BoxDirection) unsigned char boxDirection : 1;
    PREFERRED_TYPE(WebCore::Order) unsigned char rtlOrdering : 1;

    // Color Stuff = 4 bits
    PREFERRED_TYPE(bool) unsigned char hasExplicitlySetColor : 1;
    PREFERRED_TYPE(PrintColorAdjust) unsigned char printColorAdjust : 1;
    PREFERRED_TYPE(InsideLink) unsigned char insideLink : 2;

    PREFERRED_TYPE(bool) unsigned char isZoomed : 1;

#if ENABLE(TEXT_AUTOSIZING)
    unsigned autosizeStatus : 5;
#endif
    // Total = 63 bits (fits in 8 bytes)
};

} // namespace Style
} // namespace WebCore
