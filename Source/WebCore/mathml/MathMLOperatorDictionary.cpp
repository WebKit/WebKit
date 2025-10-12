/*
 * Copyright (C) 2015 Frederic Wang (fred.wang@free.fr). All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MathMLOperatorDictionary.h"

#if ENABLE(MATHML)

#include <array>

namespace WebCore {

using namespace MathMLOperatorDictionary;

typedef std::pair<char32_t, Form> Key;
struct Entry {
    char32_t character;
    unsigned form : 2;
    unsigned lspace : 3;
    unsigned rspace : 3;
    unsigned flags : 8;
};
static inline Key ExtractKey(const Entry* entry) { return Key(entry->character, static_cast<Form>(entry->form)); }
static inline char32_t ExtractChar(const Entry* entry) { return entry->character; }
static inline Property ExtractProperty(const Entry& entry)
{
    Property property;
    property.form = static_cast<Form>(entry.form);
    property.leadingSpaceInMathUnit = entry.lspace;
    property.trailingSpaceInMathUnit = entry.rspace;
    property.flags = entry.flags;
    return property;
}

// This table has been automatically generated from the Operator Dictionary of the MathML3 specification (appendix C).
// Some people use the binary operator "U+2225 PARALLEL TO" as an opening and closing delimiter, so we add the corresponding stretchy prefix and postfix forms.
static constexpr unsigned dictionarySize = 1061;
static constexpr std::array<Entry, dictionarySize> dictionary {
    Entry { 0x21, Postfix, 1, 0, 0 }, // EXCLAMATION MARK
    Entry { 0x22, Postfix, 1, 0, Accent }, // QUOTATION MARK
    Entry { 0x25, Infix, 3, 3, 0 }, // PERCENT SIGN
    Entry { 0x26, Postfix, 0, 0, 0 }, // AMPERSAND
    Entry { 0x27, Postfix, 0, 0, Accent }, // APOSTROPHE
    Entry { 0x28, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // LEFT PARENTHESIS
    Entry { 0x29, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // RIGHT PARENTHESIS
    Entry { 0x2A, Infix, 3, 3, 0 }, // ASTERISK
    Entry { 0x2B, Infix, 4, 4, 0 }, // PLUS SIGN
    Entry { 0x2B, Prefix, 0, 1, 0 }, // PLUS SIGN
    Entry { 0x2C, Infix, 0, 3, Separator }, // COMMA
    Entry { 0x2D, Infix, 4, 4, 0 }, // HYPHEN-MINUS
    Entry { 0x2D, Prefix, 0, 1, 0 }, // HYPHEN-MINUS
    Entry { 0x2E, Infix, 3, 3, 0 }, // FULL STOP
    Entry { 0x2F, Infix, 1, 1, 0 }, // SOLIDUS
    Entry { 0x3A, Infix, 1, 2, 0 }, // COLON
    Entry { 0x3B, Infix, 0, 3, Separator }, // SEMICOLON
    Entry { 0x3C, Infix, 5, 5, 0 }, // LESS-THAN SIGN
    Entry { 0x3D, Infix, 5, 5, 0 }, // EQUALS SIGN
    Entry { 0x3E, Infix, 5, 5, 0 }, // GREATER-THAN SIGN
    Entry { 0x3F, Infix, 1, 1, 0 }, // QUESTION MARK
    Entry { 0x40, Infix, 1, 1, 0 }, // COMMERCIAL AT
    Entry { 0x5B, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // LEFT SQUARE BRACKET
    Entry { 0x5C, Infix, 0, 0, 0 }, // REVERSE SOLIDUS
    Entry { 0x5D, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // RIGHT SQUARE BRACKET
    Entry { 0x5E, Postfix, 0, 0, Accent | Stretchy }, // CIRCUMFLEX ACCENT
    Entry { 0x5E, Infix, 1, 1, 0 }, // CIRCUMFLEX ACCENT
    Entry { 0x5F, Postfix, 0, 0, Accent | Stretchy }, // LOW LINE
    Entry { 0x5F, Infix, 1, 1, 0 }, // LOW LINE
    Entry { 0x60, Postfix, 0, 0, Accent }, // GRAVE ACCENT
    Entry { 0x7B, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // LEFT CURLY BRACKET
    Entry { 0x7C, Infix, 2, 2, Stretchy | Symmetric | Fence }, // VERTICAL LINE
    Entry { 0x7C, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // VERTICAL LINE
    Entry { 0x7C, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // VERTICAL LINE
    Entry { 0x7D, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // RIGHT CURLY BRACKET
    Entry { 0x7E, Postfix, 0, 0, Accent | Stretchy }, // TILDE
    Entry { 0xA8, Postfix, 0, 0, Accent }, // DIAERESIS
    Entry { 0xAA, Postfix, 0, 0, Accent }, // FEMININE ORDINAL INDICATOR
    Entry { 0xAC, Prefix, 2, 1, 0 }, // NOT SIGN
    Entry { 0xAF, Postfix, 0, 0, Accent | Stretchy }, // MACRON
    Entry { 0xB0, Postfix, 0, 0, 0 }, // DEGREE SIGN
    Entry { 0xB1, Infix, 4, 4, 0 }, // PLUS-MINUS SIGN
    Entry { 0xB1, Prefix, 0, 1, 0 }, // PLUS-MINUS SIGN
    Entry { 0xB2, Postfix, 0, 0, Accent }, // SUPERSCRIPT TWO
    Entry { 0xB3, Postfix, 0, 0, Accent }, // SUPERSCRIPT THREE
    Entry { 0xB4, Postfix, 0, 0, Accent }, // ACUTE ACCENT
    Entry { 0xB7, Infix, 4, 4, 0 }, // MIDDLE DOT
    Entry { 0xB8, Postfix, 0, 0, Accent }, // CEDILLA
    Entry { 0xB9, Postfix, 0, 0, Accent }, // SUPERSCRIPT ONE
    Entry { 0xBA, Postfix, 0, 0, Accent }, // MASCULINE ORDINAL INDICATOR
    Entry { 0xD7, Infix, 4, 4, 0 }, // MULTIPLICATION SIGN
    Entry { 0xF7, Infix, 4, 4, 0 }, // DIVISION SIGN
    Entry { 0x2C6, Postfix, 0, 0, Accent | Stretchy }, // MODIFIER LETTER CIRCUMFLEX ACCENT
    Entry { 0x2C7, Postfix, 0, 0, Accent | Stretchy }, // CARON
    Entry { 0x2C9, Postfix, 0, 0, Accent | Stretchy }, // MODIFIER LETTER MACRON
    Entry { 0x2CA, Postfix, 0, 0, Accent }, // MODIFIER LETTER ACUTE ACCENT
    Entry { 0x2CB, Postfix, 0, 0, Accent }, // MODIFIER LETTER GRAVE ACCENT
    Entry { 0x2CD, Postfix, 0, 0, Accent | Stretchy }, // MODIFIER LETTER LOW MACRON
    Entry { 0x2D8, Postfix, 0, 0, Accent }, // BREVE
    Entry { 0x2D9, Postfix, 0, 0, Accent }, // DOT ABOVE
    Entry { 0x2DA, Postfix, 0, 0, Accent }, // RING ABOVE
    Entry { 0x2DC, Postfix, 0, 0, Accent | Stretchy }, // SMALL TILDE
    Entry { 0x2DD, Postfix, 0, 0, Accent }, // DOUBLE ACUTE ACCENT
    Entry { 0x2F7, Postfix, 0, 0, Accent | Stretchy }, // MODIFIER LETTER LOW TILDE
    Entry { 0x302, Postfix, 0, 0, Accent | Stretchy }, // COMBINING CIRCUMFLEX ACCENT
    Entry { 0x311, Postfix, 0, 0, Accent }, // COMBINING INVERTED BREVE
    Entry { 0x3F6, Infix, 5, 5, 0 }, // GREEK REVERSED LUNATE EPSILON SYMBOL
    Entry { 0x2016, Prefix, 0, 0, Fence | Stretchy }, // DOUBLE VERTICAL LINE
    Entry { 0x2016, Postfix, 0, 0, Fence | Stretchy }, // DOUBLE VERTICAL LINE
    Entry { 0x2018, Prefix, 0, 0, Fence }, // LEFT SINGLE QUOTATION MARK
    Entry { 0x2019, Postfix, 0, 0, Fence }, // RIGHT SINGLE QUOTATION MARK
    Entry { 0x201A, Postfix, 0, 0, Accent }, // SINGLE LOW-9 QUOTATION MARK
    Entry { 0x201B, Postfix, 0, 0, Accent }, // SINGLE HIGH-REVERSED-9 QUOTATION MARK
    Entry { 0x201C, Prefix, 0, 0, Fence }, // LEFT DOUBLE QUOTATION MARK
    Entry { 0x201D, Postfix, 0, 0, Fence }, // RIGHT DOUBLE QUOTATION MARK
    Entry { 0x201E, Postfix, 0, 0, Accent }, // DOUBLE HIGH-REVERSED-9 QUOTATION MARK
    Entry { 0x201F, Postfix, 0, 0, Accent }, // DOUBLE LOW-9 QUOTATION MARK
    Entry { 0x2022, Infix, 4, 4, 0 }, // BULLET
    Entry { 0x2026, Infix, 0, 0, 0 }, // HORIZONTAL ELLIPSIS
    Entry { 0x2032, Postfix, 0, 0, 0 }, // PRIME
    Entry { 0x2033, Postfix, 0, 0, Accent }, // DOUBLE PRIME
    Entry { 0x2034, Postfix, 0, 0, Accent }, // TRIPLE PRIME
    Entry { 0x2035, Postfix, 0, 0, Accent }, // REVERSED PRIME
    Entry { 0x2036, Postfix, 0, 0, Accent }, // REVERSED DOUBLE PRIME
    Entry { 0x2037, Postfix, 0, 0, Accent }, // REVERSED TRIPLE PRIME
    Entry { 0x203E, Postfix, 0, 0, Accent | Stretchy }, // OVERLINE
    Entry { 0x2044, Infix, 4, 4, Stretchy }, // FRACTION SLASH
    Entry { 0x2057, Postfix, 0, 0, Accent }, // QUADRUPLE PRIME
    Entry { 0x2061, Infix, 0, 0, 0 }, // FUNCTION APPLICATION
    Entry { 0x2062, Infix, 0, 0, 0 }, // INVISIBLE TIMES
    Entry { 0x2063, Infix, 0, 0, Separator }, // INVISIBLE SEPARATOR
    Entry { 0x2064, Infix, 0, 0, 0 }, // INVISIBLE PLUS
    Entry { 0x20DB, Postfix, 0, 0, Accent }, // COMBINING THREE DOTS ABOVE
    Entry { 0x20DC, Postfix, 0, 0, Accent }, // COMBINING FOUR DOTS ABOVE
    Entry { 0x2145, Prefix, 2, 1, 0 }, // DOUBLE-STRUCK ITALIC CAPITAL D
    Entry { 0x2146, Prefix, 2, 0, 0 }, // DOUBLE-STRUCK ITALIC SMALL D
    Entry { 0x2190, Infix, 5, 5, Accent | Stretchy }, // LEFTWARDS ARROW
    Entry { 0x2191, Infix, 5, 5, Stretchy }, // UPWARDS ARROW
    Entry { 0x2192, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS ARROW
    Entry { 0x2193, Infix, 5, 5, Stretchy }, // DOWNWARDS ARROW
    Entry { 0x2194, Infix, 5, 5, Stretchy | Accent }, // LEFT RIGHT ARROW
    Entry { 0x2195, Infix, 5, 5, Stretchy }, // UP DOWN ARROW
    Entry { 0x2196, Infix, 5, 5, Stretchy }, // NORTH WEST ARROW
    Entry { 0x2197, Infix, 5, 5, Stretchy }, // NORTH EAST ARROW
    Entry { 0x2198, Infix, 5, 5, Stretchy }, // SOUTH EAST ARROW
    Entry { 0x2199, Infix, 5, 5, Stretchy }, // SOUTH WEST ARROW
    Entry { 0x219A, Infix, 5, 5, Accent }, // LEFTWARDS ARROW WITH STROKE
    Entry { 0x219B, Infix, 5, 5, Accent }, // RIGHTWARDS ARROW WITH STROKE
    Entry { 0x219C, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS WAVE ARROW
    Entry { 0x219D, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS WAVE ARROW
    Entry { 0x219E, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS TWO HEADED ARROW
    Entry { 0x219F, Infix, 5, 5, Stretchy | Accent }, // UPWARDS TWO HEADED ARROW
    Entry { 0x21A0, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS TWO HEADED ARROW
    Entry { 0x21A1, Infix, 5, 5, Stretchy }, // DOWNWARDS TWO HEADED ARROW
    Entry { 0x21A2, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS ARROW WITH TAIL
    Entry { 0x21A3, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS ARROW WITH TAIL
    Entry { 0x21A4, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS ARROW FROM BAR
    Entry { 0x21A5, Infix, 5, 5, Stretchy }, // UPWARDS ARROW FROM BAR
    Entry { 0x21A6, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS ARROW FROM BAR
    Entry { 0x21A7, Infix, 5, 5, Stretchy }, // DOWNWARDS ARROW FROM BAR
    Entry { 0x21A8, Infix, 5, 5, Stretchy }, // UP DOWN ARROW WITH BASE
    Entry { 0x21A9, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS ARROW WITH HOOK
    Entry { 0x21AA, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS ARROW WITH HOOK
    Entry { 0x21AB, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS ARROW WITH LOOP
    Entry { 0x21AC, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS ARROW WITH LOOP
    Entry { 0x21AD, Infix, 5, 5, Stretchy | Accent }, // LEFT RIGHT WAVE ARROW
    Entry { 0x21AE, Infix, 5, 5, Accent }, // LEFT RIGHT ARROW WITH STROKE
    Entry { 0x21AF, Infix, 5, 5, Stretchy }, // DOWNWARDS ZIGZAG ARROW
    Entry { 0x21B0, Infix, 5, 5, Stretchy }, // UPWARDS ARROW WITH TIP LEFTWARDS
    Entry { 0x21B1, Infix, 5, 5, Stretchy }, // UPWARDS ARROW WITH TIP RIGHTWARDS
    Entry { 0x21B2, Infix, 5, 5, Stretchy }, // DOWNWARDS ARROW WITH TIP LEFTWARDS
    Entry { 0x21B3, Infix, 5, 5, Stretchy }, // DOWNWARDS ARROW WITH TIP RIGHTWARDS
    Entry { 0x21B4, Infix, 5, 5, Stretchy }, // RIGHTWARDS ARROW WITH CORNER DOWNWARDS
    Entry { 0x21B5, Infix, 5, 5, Stretchy }, // DOWNWARDS ARROW WITH CORNER LEFTWARDS
    Entry { 0x21B6, Infix, 5, 5, Accent }, // ANTICLOCKWISE TOP SEMICIRCLE ARROW
    Entry { 0x21B7, Infix, 5, 5, Accent }, // CLOCKWISE TOP SEMICIRCLE ARROW
    Entry { 0x21B8, Infix, 5, 5, 0 }, // NORTH WEST ARROW TO LONG BAR
    Entry { 0x21B9, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS ARROW TO BAR OVER RIGHTWARDS ARROW TO BAR
    Entry { 0x21BA, Infix, 5, 5, 0 }, // ANTICLOCKWISE OPEN CIRCLE ARROW
    Entry { 0x21BB, Infix, 5, 5, 0 }, // CLOCKWISE OPEN CIRCLE ARROW
    Entry { 0x21BC, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS HARPOON WITH BARB UPWARDS
    Entry { 0x21BD, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS HARPOON WITH BARB DOWNWARDS
    Entry { 0x21BE, Infix, 5, 5, Stretchy }, // UPWARDS HARPOON WITH BARB RIGHTWARDS
    Entry { 0x21BF, Infix, 5, 5, Stretchy }, // UPWARDS HARPOON WITH BARB LEFTWARDS
    Entry { 0x21C0, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS HARPOON WITH BARB UPWARDS
    Entry { 0x21C1, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS HARPOON WITH BARB DOWNWARDS
    Entry { 0x21C2, Infix, 5, 5, Stretchy }, // DOWNWARDS HARPOON WITH BARB RIGHTWARDS
    Entry { 0x21C3, Infix, 5, 5, Stretchy }, // DOWNWARDS HARPOON WITH BARB LEFTWARDS
    Entry { 0x21C4, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS ARROW OVER LEFTWARDS ARROW
    Entry { 0x21C5, Infix, 5, 5, Stretchy }, // UPWARDS ARROW LEFTWARDS OF DOWNWARDS ARROW
    Entry { 0x21C6, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS ARROW OVER RIGHTWARDS ARROW
    Entry { 0x21C7, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS PAIRED ARROWS
    Entry { 0x21C8, Infix, 5, 5, Stretchy }, // UPWARDS PAIRED ARROWS
    Entry { 0x21C9, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS PAIRED ARROWS
    Entry { 0x21CA, Infix, 5, 5, Stretchy }, // DOWNWARDS PAIRED ARROWS
    Entry { 0x21CB, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS HARPOON OVER RIGHTWARDS HARPOON
    Entry { 0x21CC, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS HARPOON OVER LEFTWARDS HARPOON
    Entry { 0x21CD, Infix, 5, 5, Accent }, // LEFTWARDS DOUBLE ARROW WITH STROKE
    Entry { 0x21CE, Infix, 5, 5, Accent }, // LEFT RIGHT DOUBLE ARROW WITH STROKE
    Entry { 0x21CF, Infix, 5, 5, Accent }, // RIGHTWARDS DOUBLE ARROW WITH STROKE
    Entry { 0x21D0, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS DOUBLE ARROW
    Entry { 0x21D1, Infix, 5, 5, Stretchy }, // UPWARDS DOUBLE ARROW
    Entry { 0x21D2, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS DOUBLE ARROW
    Entry { 0x21D3, Infix, 5, 5, Stretchy }, // DOWNWARDS DOUBLE ARROW
    Entry { 0x21D4, Infix, 5, 5, Stretchy | Accent }, // LEFT RIGHT DOUBLE ARROW
    Entry { 0x21D5, Infix, 5, 5, Stretchy }, // UP DOWN DOUBLE ARROW
    Entry { 0x21D6, Infix, 5, 5, Stretchy }, // NORTH WEST DOUBLE ARROW
    Entry { 0x21D7, Infix, 5, 5, Stretchy }, // NORTH EAST DOUBLE ARROW
    Entry { 0x21D8, Infix, 5, 5, Stretchy }, // SOUTH EAST DOUBLE ARROW
    Entry { 0x21D9, Infix, 5, 5, Stretchy }, // SOUTH WEST DOUBLE ARROW
    Entry { 0x21DA, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS TRIPLE ARROW
    Entry { 0x21DB, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS TRIPLE ARROW
    Entry { 0x21DC, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS SQUIGGLE ARROW
    Entry { 0x21DD, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS SQUIGGLE ARROW
    Entry { 0x21DE, Infix, 5, 5, 0 }, // UPWARDS ARROW WITH DOUBLE STROKE
    Entry { 0x21DF, Infix, 5, 5, 0 }, // DOWNWARDS ARROW WITH DOUBLE STROKE
    Entry { 0x21E0, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS DASHED ARROW
    Entry { 0x21E1, Infix, 5, 5, Stretchy }, // UPWARDS DASHED ARROW
    Entry { 0x21E2, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS DASHED ARROW
    Entry { 0x21E3, Infix, 5, 5, Stretchy }, // DOWNWARDS DASHED ARROW
    Entry { 0x21E4, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS ARROW TO BAR
    Entry { 0x21E5, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS ARROW TO BAR
    Entry { 0x21E6, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS WHITE ARROW
    Entry { 0x21E7, Infix, 5, 5, Stretchy }, // UPWARDS WHITE ARROW
    Entry { 0x21E8, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS WHITE ARROW
    Entry { 0x21E9, Infix, 5, 5, Stretchy }, // DOWNWARDS WHITE ARROW
    Entry { 0x21EA, Infix, 5, 5, Stretchy }, // UPWARDS WHITE ARROW FROM BAR
    Entry { 0x21EB, Infix, 5, 5, Stretchy }, // UPWARDS WHITE ARROW ON PEDESTAL
    Entry { 0x21EC, Infix, 5, 5, Stretchy }, // UPWARDS WHITE ARROW ON PEDESTAL WITH HORIZONTAL BAR
    Entry { 0x21ED, Infix, 5, 5, Stretchy }, // UPWARDS WHITE ARROW ON PEDESTAL WITH VERTICAL BAR
    Entry { 0x21EE, Infix, 5, 5, Stretchy }, // UPWARDS WHITE DOUBLE ARROW
    Entry { 0x21EF, Infix, 5, 5, Stretchy }, // UPWARDS WHITE DOUBLE ARROW ON PEDESTAL
    Entry { 0x21F0, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS WHITE ARROW FROM WALL
    Entry { 0x21F1, Infix, 5, 5, 0 }, // NORTH WEST ARROW TO CORNER
    Entry { 0x21F2, Infix, 5, 5, 0 }, // SOUTH EAST ARROW TO CORNER
    Entry { 0x21F3, Infix, 5, 5, Stretchy }, // UP DOWN WHITE ARROW
    Entry { 0x21F4, Infix, 5, 5, Accent }, // RIGHT ARROW WITH SMALL CIRCLE
    Entry { 0x21F5, Infix, 5, 5, Stretchy }, // DOWNWARDS ARROW LEFTWARDS OF UPWARDS ARROW
    Entry { 0x21F6, Infix, 5, 5, Stretchy | Accent }, // THREE RIGHTWARDS ARROWS
    Entry { 0x21F7, Infix, 5, 5, Accent }, // LEFTWARDS ARROW WITH VERTICAL STROKE
    Entry { 0x21F8, Infix, 5, 5, Accent }, // RIGHTWARDS ARROW WITH VERTICAL STROKE
    Entry { 0x21F9, Infix, 5, 5, Accent }, // LEFT RIGHT ARROW WITH VERTICAL STROKE
    Entry { 0x21FA, Infix, 5, 5, Accent }, // LEFTWARDS ARROW WITH DOUBLE VERTICAL STROKE
    Entry { 0x21FB, Infix, 5, 5, Accent }, // RIGHTWARDS ARROW WITH DOUBLE VERTICAL STROKE
    Entry { 0x21FC, Infix, 5, 5, Accent }, // LEFT RIGHT ARROW WITH DOUBLE VERTICAL STROKE
    Entry { 0x21FD, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS OPEN-HEADED ARROW
    Entry { 0x21FE, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS OPEN-HEADED ARROW
    Entry { 0x21FF, Infix, 5, 5, Stretchy | Accent }, // LEFT RIGHT OPEN-HEADED ARROW
    Entry { 0x2200, Prefix, 2, 1, 0 }, // FOR ALL
    Entry { 0x2201, Infix, 1, 2, 0 }, // COMPLEMENT
    Entry { 0x2202, Prefix, 2, 1, 0 }, // PARTIAL DIFFERENTIAL
    Entry { 0x2203, Prefix, 2, 1, 0 }, // THERE EXISTS
    Entry { 0x2204, Prefix, 2, 1, 0 }, // THERE DOES NOT EXIST
    Entry { 0x2206, Infix, 3, 3, 0 }, // INCREMENT
    Entry { 0x2207, Prefix, 2, 1, 0 }, // NABLA
    Entry { 0x2208, Infix, 5, 5, 0 }, // ELEMENT OF
    Entry { 0x2209, Infix, 5, 5, 0 }, // NOT AN ELEMENT OF
    Entry { 0x220A, Infix, 5, 5, 0 }, // SMALL ELEMENT OF
    Entry { 0x220B, Infix, 5, 5, 0 }, // CONTAINS AS MEMBER
    Entry { 0x220C, Infix, 5, 5, 0 }, // DOES NOT CONTAIN AS MEMBER
    Entry { 0x220D, Infix, 5, 5, 0 }, // SMALL CONTAINS AS MEMBER
    Entry { 0x220E, Infix, 3, 3, 0 }, // END OF PROOF
    Entry { 0x220F, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // N-ARY PRODUCT
    Entry { 0x2210, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // N-ARY COPRODUCT
    Entry { 0x2211, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // N-ARY SUMMATION
    Entry { 0x2212, Infix, 4, 4, 0 }, // MINUS SIGN
    Entry { 0x2212, Prefix, 0, 1, 0 }, // MINUS SIGN
    Entry { 0x2213, Infix, 4, 4, 0 }, // MINUS-OR-PLUS SIGN
    Entry { 0x2213, Prefix, 0, 1, 0 }, // MINUS-OR-PLUS SIGN
    Entry { 0x2214, Infix, 4, 4, 0 }, // DOT PLUS
    Entry { 0x2215, Infix, 4, 4, Stretchy }, // DIVISION SLASH
    Entry { 0x2216, Infix, 4, 4, 0 }, // SET MINUS
    Entry { 0x2217, Infix, 4, 4, 0 }, // ASTERISK OPERATOR
    Entry { 0x2218, Infix, 4, 4, 0 }, // RING OPERATOR
    Entry { 0x2219, Infix, 4, 4, 0 }, // BULLET OPERATOR
    Entry { 0x221A, Prefix, 1, 1, Stretchy }, // SQUARE ROOT
    Entry { 0x221B, Prefix, 1, 1, 0 }, // CUBE ROOT
    Entry { 0x221C, Prefix, 1, 1, 0 }, // FOURTH ROOT
    Entry { 0x221D, Infix, 5, 5, 0 }, // PROPORTIONAL TO
    Entry { 0x221F, Infix, 5, 5, 0 }, // RIGHT ANGLE
    Entry { 0x2220, Prefix, 0, 0, 0 }, // ANGLE
    Entry { 0x2221, Prefix, 0, 0, 0 }, // MEASURED ANGLE
    Entry { 0x2222, Prefix, 0, 0, 0 }, // SPHERICAL ANGLE
    Entry { 0x2223, Infix, 5, 5, 0 }, // DIVIDES
    Entry { 0x2224, Infix, 5, 5, 0 }, // DOES NOT DIVIDE
    Entry { 0x2225, Infix, 5, 5, 0 }, // PARALLEL TO
    Entry { 0x2225, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // PARALLEL TO
    Entry { 0x2225, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // PARALLEL TO
    Entry { 0x2226, Infix, 5, 5, 0 }, // NOT PARALLEL TO
    Entry { 0x2227, Infix, 4, 4, 0 }, // LOGICAL AND
    Entry { 0x2228, Infix, 4, 4, 0 }, // LOGICAL OR
    Entry { 0x2229, Infix, 4, 4, 0 }, // INTERSECTION
    Entry { 0x222A, Infix, 4, 4, 0 }, // UNION
    Entry { 0x222B, Prefix, 0, 1, Symmetric | LargeOp }, // INTEGRAL
    Entry { 0x222C, Prefix, 0, 1, Symmetric | LargeOp }, // DOUBLE INTEGRAL
    Entry { 0x222D, Prefix, 0, 1, Symmetric | LargeOp }, // TRIPLE INTEGRAL
    Entry { 0x222E, Prefix, 0, 1, Symmetric | LargeOp }, // CONTOUR INTEGRAL
    Entry { 0x222F, Prefix, 0, 1, Symmetric | LargeOp }, // SURFACE INTEGRAL
    Entry { 0x2230, Prefix, 0, 1, Symmetric | LargeOp }, // VOLUME INTEGRAL
    Entry { 0x2231, Prefix, 0, 1, Symmetric | LargeOp }, // CLOCKWISE INTEGRAL
    Entry { 0x2232, Prefix, 0, 1, Symmetric | LargeOp }, // CLOCKWISE CONTOUR INTEGRAL
    Entry { 0x2233, Prefix, 0, 1, Symmetric | LargeOp }, // ANTICLOCKWISE CONTOUR INTEGRAL
    Entry { 0x2234, Infix, 5, 5, 0 }, // THEREFORE
    Entry { 0x2235, Infix, 5, 5, 0 }, // BECAUSE
    Entry { 0x2236, Infix, 5, 5, 0 }, // RATIO
    Entry { 0x2237, Infix, 5, 5, 0 }, // PROPORTION
    Entry { 0x2238, Infix, 4, 4, 0 }, // DOT MINUS
    Entry { 0x2239, Infix, 5, 5, 0 }, // EXCESS
    Entry { 0x223A, Infix, 4, 4, 0 }, // GEOMETRIC PROPORTION
    Entry { 0x223B, Infix, 5, 5, 0 }, // HOMOTHETIC
    Entry { 0x223C, Infix, 5, 5, 0 }, // TILDE OPERATOR
    Entry { 0x223D, Infix, 5, 5, 0 }, // REVERSED TILDE
    Entry { 0x223E, Infix, 5, 5, 0 }, // INVERTED LAZY S
    Entry { 0x223F, Infix, 3, 3, 0 }, // SINE WAVE
    Entry { 0x2240, Infix, 4, 4, 0 }, // WREATH PRODUCT
    Entry { 0x2241, Infix, 5, 5, 0 }, // NOT TILDE
    Entry { 0x2242, Infix, 5, 5, 0 }, // MINUS TILDE
    Entry { 0x2243, Infix, 5, 5, 0 }, // ASYMPTOTICALLY EQUAL TO
    Entry { 0x2244, Infix, 5, 5, 0 }, // NOT ASYMPTOTICALLY EQUAL TO
    Entry { 0x2245, Infix, 5, 5, 0 }, // APPROXIMATELY EQUAL TO
    Entry { 0x2246, Infix, 5, 5, 0 }, // APPROXIMATELY BUT NOT ACTUALLY EQUAL TO
    Entry { 0x2247, Infix, 5, 5, 0 }, // NEITHER APPROXIMATELY NOR ACTUALLY EQUAL TO
    Entry { 0x2248, Infix, 5, 5, 0 }, // ALMOST EQUAL TO
    Entry { 0x2249, Infix, 5, 5, 0 }, // NOT ALMOST EQUAL TO
    Entry { 0x224A, Infix, 5, 5, 0 }, // ALMOST EQUAL OR EQUAL TO
    Entry { 0x224B, Infix, 5, 5, 0 }, // TRIPLE TILDE
    Entry { 0x224C, Infix, 5, 5, 0 }, // ALL EQUAL TO
    Entry { 0x224D, Infix, 5, 5, 0 }, // EQUIVALENT TO
    Entry { 0x224E, Infix, 5, 5, 0 }, // GEOMETRICALLY EQUIVALENT TO
    Entry { 0x224F, Infix, 5, 5, 0 }, // DIFFERENCE BETWEEN
    Entry { 0x2250, Infix, 5, 5, 0 }, // APPROACHES THE LIMIT
    Entry { 0x2251, Infix, 5, 5, 0 }, // GEOMETRICALLY EQUAL TO
    Entry { 0x2252, Infix, 5, 5, 0 }, // APPROXIMATELY EQUAL TO OR THE IMAGE OF
    Entry { 0x2253, Infix, 5, 5, 0 }, // IMAGE OF OR APPROXIMATELY EQUAL TO
    Entry { 0x2254, Infix, 5, 5, 0 }, // COLON EQUALS
    Entry { 0x2255, Infix, 5, 5, 0 }, // EQUALS COLON
    Entry { 0x2256, Infix, 5, 5, 0 }, // RING IN EQUAL TO
    Entry { 0x2257, Infix, 5, 5, 0 }, // RING EQUAL TO
    Entry { 0x2258, Infix, 5, 5, 0 }, // CORRESPONDS TO
    Entry { 0x2259, Infix, 5, 5, 0 }, // ESTIMATES
    Entry { 0x225A, Infix, 5, 5, 0 }, // EQUIANGULAR TO
    Entry { 0x225C, Infix, 5, 5, 0 }, // DELTA EQUAL TO
    Entry { 0x225D, Infix, 5, 5, 0 }, // EQUAL TO BY DEFINITION
    Entry { 0x225E, Infix, 5, 5, 0 }, // MEASURED BY
    Entry { 0x225F, Infix, 5, 5, 0 }, // QUESTIONED EQUAL TO
    Entry { 0x2260, Infix, 5, 5, 0 }, // NOT EQUAL TO
    Entry { 0x2261, Infix, 5, 5, 0 }, // IDENTICAL TO
    Entry { 0x2262, Infix, 5, 5, 0 }, // NOT IDENTICAL TO
    Entry { 0x2263, Infix, 5, 5, 0 }, // STRICTLY EQUIVALENT TO
    Entry { 0x2264, Infix, 5, 5, 0 }, // LESS-THAN OR EQUAL TO
    Entry { 0x2265, Infix, 5, 5, 0 }, // GREATER-THAN OR EQUAL TO
    Entry { 0x2266, Infix, 5, 5, 0 }, // LESS-THAN OVER EQUAL TO
    Entry { 0x2267, Infix, 5, 5, 0 }, // GREATER-THAN OVER EQUAL TO
    Entry { 0x2268, Infix, 5, 5, 0 }, // LESS-THAN BUT NOT EQUAL TO
    Entry { 0x2269, Infix, 5, 5, 0 }, // GREATER-THAN BUT NOT EQUAL TO
    Entry { 0x226A, Infix, 5, 5, 0 }, // MUCH LESS-THAN
    Entry { 0x226B, Infix, 5, 5, 0 }, // MUCH GREATER-THAN
    Entry { 0x226C, Infix, 5, 5, 0 }, // BETWEEN
    Entry { 0x226D, Infix, 5, 5, 0 }, // NOT EQUIVALENT TO
    Entry { 0x226E, Infix, 5, 5, 0 }, // NOT LESS-THAN
    Entry { 0x226F, Infix, 5, 5, 0 }, // NOT GREATER-THAN
    Entry { 0x2270, Infix, 5, 5, 0 }, // NEITHER LESS-THAN NOR EQUAL TO
    Entry { 0x2271, Infix, 5, 5, 0 }, // NEITHER GREATER-THAN NOR EQUAL TO
    Entry { 0x2272, Infix, 5, 5, 0 }, // LESS-THAN OR EQUIVALENT TO
    Entry { 0x2273, Infix, 5, 5, 0 }, // GREATER-THAN OR EQUIVALENT TO
    Entry { 0x2274, Infix, 5, 5, 0 }, // NEITHER LESS-THAN NOR EQUIVALENT TO
    Entry { 0x2275, Infix, 5, 5, 0 }, // NEITHER GREATER-THAN NOR EQUIVALENT TO
    Entry { 0x2276, Infix, 5, 5, 0 }, // LESS-THAN OR GREATER-THAN
    Entry { 0x2277, Infix, 5, 5, 0 }, // GREATER-THAN OR LESS-THAN
    Entry { 0x2278, Infix, 5, 5, 0 }, // NEITHER LESS-THAN NOR GREATER-THAN
    Entry { 0x2279, Infix, 5, 5, 0 }, // NEITHER GREATER-THAN NOR LESS-THAN
    Entry { 0x227A, Infix, 5, 5, 0 }, // PRECEDES
    Entry { 0x227B, Infix, 5, 5, 0 }, // SUCCEEDS
    Entry { 0x227C, Infix, 5, 5, 0 }, // PRECEDES OR EQUAL TO
    Entry { 0x227D, Infix, 5, 5, 0 }, // SUCCEEDS OR EQUAL TO
    Entry { 0x227E, Infix, 5, 5, 0 }, // PRECEDES OR EQUIVALENT TO
    Entry { 0x227F, Infix, 5, 5, 0 }, // SUCCEEDS OR EQUIVALENT TO
    Entry { 0x2280, Infix, 5, 5, 0 }, // DOES NOT PRECEDE
    Entry { 0x2281, Infix, 5, 5, 0 }, // DOES NOT SUCCEED
    Entry { 0x2282, Infix, 5, 5, 0 }, // SUBSET OF
    Entry { 0x2283, Infix, 5, 5, 0 }, // SUPERSET OF
    Entry { 0x2284, Infix, 5, 5, 0 }, // NOT A SUBSET OF
    Entry { 0x2285, Infix, 5, 5, 0 }, // NOT A SUPERSET OF
    Entry { 0x2286, Infix, 5, 5, 0 }, // SUBSET OF OR EQUAL TO
    Entry { 0x2287, Infix, 5, 5, 0 }, // SUPERSET OF OR EQUAL TO
    Entry { 0x2288, Infix, 5, 5, 0 }, // NEITHER A SUBSET OF NOR EQUAL TO
    Entry { 0x2289, Infix, 5, 5, 0 }, // NEITHER A SUPERSET OF NOR EQUAL TO
    Entry { 0x228A, Infix, 5, 5, 0 }, // SUBSET OF WITH NOT EQUAL TO
    Entry { 0x228B, Infix, 5, 5, 0 }, // SUPERSET OF WITH NOT EQUAL TO
    Entry { 0x228C, Infix, 4, 4, 0 }, // MULTISET
    Entry { 0x228D, Infix, 4, 4, 0 }, // MULTISET MULTIPLICATION
    Entry { 0x228E, Infix, 4, 4, 0 }, // MULTISET UNION
    Entry { 0x228F, Infix, 5, 5, 0 }, // SQUARE IMAGE OF
    Entry { 0x2290, Infix, 5, 5, 0 }, // SQUARE ORIGINAL OF
    Entry { 0x2291, Infix, 5, 5, 0 }, // SQUARE IMAGE OF OR EQUAL TO
    Entry { 0x2292, Infix, 5, 5, 0 }, // SQUARE ORIGINAL OF OR EQUAL TO
    Entry { 0x2293, Infix, 4, 4, 0 }, // SQUARE CAP
    Entry { 0x2294, Infix, 4, 4, 0 }, // SQUARE CUP
    Entry { 0x2295, Infix, 4, 4, 0 }, // CIRCLED PLUS
    Entry { 0x2296, Infix, 4, 4, 0 }, // CIRCLED MINUS
    Entry { 0x2297, Infix, 4, 4, 0 }, // CIRCLED TIMES
    Entry { 0x2298, Infix, 4, 4, 0 }, // CIRCLED DIVISION SLASH
    Entry { 0x2299, Infix, 4, 4, 0 }, // CIRCLED DOT OPERATOR
    Entry { 0x229A, Infix, 4, 4, 0 }, // CIRCLED RING OPERATOR
    Entry { 0x229B, Infix, 4, 4, 0 }, // CIRCLED ASTERISK OPERATOR
    Entry { 0x229C, Infix, 4, 4, 0 }, // CIRCLED EQUALS
    Entry { 0x229D, Infix, 4, 4, 0 }, // CIRCLED DASH
    Entry { 0x229E, Infix, 4, 4, 0 }, // SQUARED PLUS
    Entry { 0x229F, Infix, 4, 4, 0 }, // SQUARED MINUS
    Entry { 0x22A0, Infix, 4, 4, 0 }, // SQUARED TIMES
    Entry { 0x22A1, Infix, 4, 4, 0 }, // SQUARED DOT OPERATOR
    Entry { 0x22A2, Infix, 5, 5, 0 }, // RIGHT TACK
    Entry { 0x22A3, Infix, 5, 5, 0 }, // LEFT TACK
    Entry { 0x22A4, Infix, 5, 5, 0 }, // DOWN TACK
    Entry { 0x22A5, Infix, 5, 5, 0 }, // UP TACK
    Entry { 0x22A6, Infix, 5, 5, 0 }, // ASSERTION
    Entry { 0x22A7, Infix, 5, 5, 0 }, // MODELS
    Entry { 0x22A8, Infix, 5, 5, 0 }, // TRUE
    Entry { 0x22A9, Infix, 5, 5, 0 }, // FORCES
    Entry { 0x22AA, Infix, 5, 5, 0 }, // TRIPLE VERTICAL BAR RIGHT TURNSTILE
    Entry { 0x22AB, Infix, 5, 5, 0 }, // DOUBLE VERTICAL BAR DOUBLE RIGHT TURNSTILE
    Entry { 0x22AC, Infix, 5, 5, 0 }, // DOES NOT PROVE
    Entry { 0x22AD, Infix, 5, 5, 0 }, // NOT TRUE
    Entry { 0x22AE, Infix, 5, 5, 0 }, // DOES NOT FORCE
    Entry { 0x22AF, Infix, 5, 5, 0 }, // NEGATED DOUBLE VERTICAL BAR DOUBLE RIGHT TURNSTILE
    Entry { 0x22B0, Infix, 5, 5, 0 }, // PRECEDES UNDER RELATION
    Entry { 0x22B1, Infix, 5, 5, 0 }, // SUCCEEDS UNDER RELATION
    Entry { 0x22B2, Infix, 5, 5, 0 }, // NORMAL SUBGROUP OF
    Entry { 0x22B3, Infix, 5, 5, 0 }, // CONTAINS AS NORMAL SUBGROUP
    Entry { 0x22B4, Infix, 5, 5, 0 }, // NORMAL SUBGROUP OF OR EQUAL TO
    Entry { 0x22B5, Infix, 5, 5, 0 }, // CONTAINS AS NORMAL SUBGROUP OR EQUAL TO
    Entry { 0x22B6, Infix, 5, 5, 0 }, // ORIGINAL OF
    Entry { 0x22B7, Infix, 5, 5, 0 }, // IMAGE OF
    Entry { 0x22B8, Infix, 5, 5, 0 }, // MULTIMAP
    Entry { 0x22B9, Infix, 5, 5, 0 }, // HERMITIAN CONJUGATE MATRIX
    Entry { 0x22BA, Infix, 4, 4, 0 }, // INTERCALATE
    Entry { 0x22BB, Infix, 4, 4, 0 }, // XOR
    Entry { 0x22BC, Infix, 4, 4, 0 }, // NAND
    Entry { 0x22BD, Infix, 4, 4, 0 }, // NOR
    Entry { 0x22BE, Infix, 3, 3, 0 }, // RIGHT ANGLE WITH ARC
    Entry { 0x22BF, Infix, 3, 3, 0 }, // RIGHT TRIANGLE
    Entry { 0x22C0, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // N-ARY LOGICAL AND
    Entry { 0x22C1, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // N-ARY LOGICAL OR
    Entry { 0x22C2, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // N-ARY INTERSECTION
    Entry { 0x22C3, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // N-ARY UNION
    Entry { 0x22C4, Infix, 4, 4, 0 }, // DIAMOND OPERATOR
    Entry { 0x22C5, Infix, 4, 4, 0 }, // DOT OPERATOR
    Entry { 0x22C6, Infix, 4, 4, 0 }, // STAR OPERATOR
    Entry { 0x22C7, Infix, 4, 4, 0 }, // DIVISION TIMES
    Entry { 0x22C8, Infix, 5, 5, 0 }, // BOWTIE
    Entry { 0x22C9, Infix, 4, 4, 0 }, // LEFT NORMAL FACTOR SEMIDIRECT PRODUCT
    Entry { 0x22CA, Infix, 4, 4, 0 }, // RIGHT NORMAL FACTOR SEMIDIRECT PRODUCT
    Entry { 0x22CB, Infix, 4, 4, 0 }, // LEFT SEMIDIRECT PRODUCT
    Entry { 0x22CC, Infix, 4, 4, 0 }, // RIGHT SEMIDIRECT PRODUCT
    Entry { 0x22CD, Infix, 5, 5, 0 }, // REVERSED TILDE EQUALS
    Entry { 0x22CE, Infix, 4, 4, 0 }, // CURLY LOGICAL OR
    Entry { 0x22CF, Infix, 4, 4, 0 }, // CURLY LOGICAL AND
    Entry { 0x22D0, Infix, 5, 5, 0 }, // DOUBLE SUBSET
    Entry { 0x22D1, Infix, 5, 5, 0 }, // DOUBLE SUPERSET
    Entry { 0x22D2, Infix, 4, 4, 0 }, // DOUBLE INTERSECTION
    Entry { 0x22D3, Infix, 4, 4, 0 }, // DOUBLE UNION
    Entry { 0x22D4, Infix, 5, 5, 0 }, // PITCHFORK
    Entry { 0x22D5, Infix, 5, 5, 0 }, // EQUAL AND PARALLEL TO
    Entry { 0x22D6, Infix, 5, 5, 0 }, // LESS-THAN WITH DOT
    Entry { 0x22D7, Infix, 5, 5, 0 }, // GREATER-THAN WITH DOT
    Entry { 0x22D8, Infix, 5, 5, 0 }, // VERY MUCH LESS-THAN
    Entry { 0x22D9, Infix, 5, 5, 0 }, // VERY MUCH GREATER-THAN
    Entry { 0x22DA, Infix, 5, 5, 0 }, // LESS-THAN EQUAL TO OR GREATER-THAN
    Entry { 0x22DB, Infix, 5, 5, 0 }, // GREATER-THAN EQUAL TO OR LESS-THAN
    Entry { 0x22DC, Infix, 5, 5, 0 }, // EQUAL TO OR LESS-THAN
    Entry { 0x22DD, Infix, 5, 5, 0 }, // EQUAL TO OR GREATER-THAN
    Entry { 0x22DE, Infix, 5, 5, 0 }, // EQUAL TO OR PRECEDES
    Entry { 0x22DF, Infix, 5, 5, 0 }, // EQUAL TO OR SUCCEEDS
    Entry { 0x22E0, Infix, 5, 5, 0 }, // DOES NOT PRECEDE OR EQUAL
    Entry { 0x22E1, Infix, 5, 5, 0 }, // DOES NOT SUCCEED OR EQUAL
    Entry { 0x22E2, Infix, 5, 5, 0 }, // NOT SQUARE IMAGE OF OR EQUAL TO
    Entry { 0x22E3, Infix, 5, 5, 0 }, // NOT SQUARE ORIGINAL OF OR EQUAL TO
    Entry { 0x22E4, Infix, 5, 5, 0 }, // SQUARE IMAGE OF OR NOT EQUAL TO
    Entry { 0x22E5, Infix, 5, 5, 0 }, // SQUARE ORIGINAL OF OR NOT EQUAL TO
    Entry { 0x22E6, Infix, 5, 5, 0 }, // LESS-THAN BUT NOT EQUIVALENT TO
    Entry { 0x22E7, Infix, 5, 5, 0 }, // GREATER-THAN BUT NOT EQUIVALENT TO
    Entry { 0x22E8, Infix, 5, 5, 0 }, // PRECEDES BUT NOT EQUIVALENT TO
    Entry { 0x22E9, Infix, 5, 5, 0 }, // SUCCEEDS BUT NOT EQUIVALENT TO
    Entry { 0x22EA, Infix, 5, 5, 0 }, // NOT NORMAL SUBGROUP OF
    Entry { 0x22EB, Infix, 5, 5, 0 }, // DOES NOT CONTAIN AS NORMAL SUBGROUP
    Entry { 0x22EC, Infix, 5, 5, 0 }, // NOT NORMAL SUBGROUP OF OR EQUAL TO
    Entry { 0x22ED, Infix, 5, 5, 0 }, // DOES NOT CONTAIN AS NORMAL SUBGROUP OR EQUAL
    Entry { 0x22EE, Infix, 5, 5, 0 }, // VERTICAL ELLIPSIS
    Entry { 0x22EF, Infix, 0, 0, 0 }, // MIDLINE HORIZONTAL ELLIPSIS
    Entry { 0x22F0, Infix, 5, 5, 0 }, // UP RIGHT DIAGONAL ELLIPSIS
    Entry { 0x22F1, Infix, 5, 5, 0 }, // DOWN RIGHT DIAGONAL ELLIPSIS
    Entry { 0x22F2, Infix, 5, 5, 0 }, // ELEMENT OF WITH LONG HORIZONTAL STROKE
    Entry { 0x22F3, Infix, 5, 5, 0 }, // ELEMENT OF WITH VERTICAL BAR AT END OF HORIZONTAL STROKE
    Entry { 0x22F4, Infix, 5, 5, 0 }, // SMALL ELEMENT OF WITH VERTICAL BAR AT END OF HORIZONTAL STROKE
    Entry { 0x22F5, Infix, 5, 5, 0 }, // ELEMENT OF WITH DOT ABOVE
    Entry { 0x22F6, Infix, 5, 5, 0 }, // ELEMENT OF WITH OVERBAR
    Entry { 0x22F7, Infix, 5, 5, 0 }, // SMALL ELEMENT OF WITH OVERBAR
    Entry { 0x22F8, Infix, 5, 5, 0 }, // ELEMENT OF WITH UNDERBAR
    Entry { 0x22F9, Infix, 5, 5, 0 }, // ELEMENT OF WITH TWO HORIZONTAL STROKES
    Entry { 0x22FA, Infix, 5, 5, 0 }, // CONTAINS WITH LONG HORIZONTAL STROKE
    Entry { 0x22FB, Infix, 5, 5, 0 }, // CONTAINS WITH VERTICAL BAR AT END OF HORIZONTAL STROKE
    Entry { 0x22FC, Infix, 5, 5, 0 }, // SMALL CONTAINS WITH VERTICAL BAR AT END OF HORIZONTAL STROKE
    Entry { 0x22FD, Infix, 5, 5, 0 }, // CONTAINS WITH OVERBAR
    Entry { 0x22FE, Infix, 5, 5, 0 }, // SMALL CONTAINS WITH OVERBAR
    Entry { 0x22FF, Infix, 5, 5, 0 }, // Z NOTATION BAG MEMBERSHIP
    Entry { 0x2308, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // LEFT CEILING
    Entry { 0x2309, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // RIGHT CEILING
    Entry { 0x230A, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // LEFT FLOOR
    Entry { 0x230B, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // RIGHT FLOOR
    Entry { 0x2329, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // LEFT-POINTING ANGLE BRACKET
    Entry { 0x232A, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // RIGHT-POINTING ANGLE BRACKET
    Entry { 0x23B4, Postfix, 0, 0, Accent | Stretchy }, // TOP SQUARE BRACKET
    Entry { 0x23B5, Postfix, 0, 0, Accent | Stretchy }, // BOTTOM SQUARE BRACKET
    Entry { 0x23DC, Postfix, 0, 0, Accent | Stretchy }, // TOP PARENTHESIS
    Entry { 0x23DD, Postfix, 0, 0, Accent | Stretchy }, // BOTTOM PARENTHESIS
    Entry { 0x23DE, Postfix, 0, 0, Accent | Stretchy }, // TOP CURLY BRACKET
    Entry { 0x23DF, Postfix, 0, 0, Accent | Stretchy }, // BOTTOM CURLY BRACKET
    Entry { 0x23E0, Postfix, 0, 0, Accent | Stretchy }, // TOP TORTOISE SHELL BRACKET
    Entry { 0x23E1, Postfix, 0, 0, Accent | Stretchy }, // BOTTOM TORTOISE SHELL BRACKET
    Entry { 0x25A0, Infix, 3, 3, 0 }, // BLACK SQUARE
    Entry { 0x25A1, Infix, 3, 3, 0 }, // WHITE SQUARE
    Entry { 0x25AA, Infix, 3, 3, 0 }, // BLACK SMALL SQUARE
    Entry { 0x25AB, Infix, 3, 3, 0 }, // WHITE SMALL SQUARE
    Entry { 0x25AD, Infix, 3, 3, 0 }, // WHITE RECTANGLE
    Entry { 0x25AE, Infix, 3, 3, 0 }, // BLACK VERTICAL RECTANGLE
    Entry { 0x25AF, Infix, 3, 3, 0 }, // WHITE VERTICAL RECTANGLE
    Entry { 0x25B0, Infix, 3, 3, 0 }, // BLACK PARALLELOGRAM
    Entry { 0x25B1, Infix, 3, 3, 0 }, // WHITE PARALLELOGRAM
    Entry { 0x25B2, Infix, 4, 4, 0 }, // BLACK UP-POINTING TRIANGLE
    Entry { 0x25B3, Infix, 4, 4, 0 }, // WHITE UP-POINTING TRIANGLE
    Entry { 0x25B4, Infix, 4, 4, 0 }, // BLACK UP-POINTING SMALL TRIANGLE
    Entry { 0x25B5, Infix, 4, 4, 0 }, // WHITE UP-POINTING SMALL TRIANGLE
    Entry { 0x25B6, Infix, 4, 4, 0 }, // BLACK RIGHT-POINTING TRIANGLE
    Entry { 0x25B7, Infix, 4, 4, 0 }, // WHITE RIGHT-POINTING TRIANGLE
    Entry { 0x25B8, Infix, 4, 4, 0 }, // BLACK RIGHT-POINTING SMALL TRIANGLE
    Entry { 0x25B9, Infix, 4, 4, 0 }, // WHITE RIGHT-POINTING SMALL TRIANGLE
    Entry { 0x25BC, Infix, 4, 4, 0 }, // BLACK DOWN-POINTING TRIANGLE
    Entry { 0x25BD, Infix, 4, 4, 0 }, // WHITE DOWN-POINTING TRIANGLE
    Entry { 0x25BE, Infix, 4, 4, 0 }, // BLACK DOWN-POINTING SMALL TRIANGLE
    Entry { 0x25BF, Infix, 4, 4, 0 }, // WHITE DOWN-POINTING SMALL TRIANGLE
    Entry { 0x25C0, Infix, 4, 4, 0 }, // BLACK LEFT-POINTING TRIANGLE
    Entry { 0x25C1, Infix, 4, 4, 0 }, // WHITE LEFT-POINTING TRIANGLE
    Entry { 0x25C2, Infix, 4, 4, 0 }, // BLACK LEFT-POINTING SMALL TRIANGLE
    Entry { 0x25C3, Infix, 4, 4, 0 }, // WHITE LEFT-POINTING SMALL TRIANGLE
    Entry { 0x25C4, Infix, 4, 4, 0 }, // BLACK LEFT-POINTING POINTER
    Entry { 0x25C5, Infix, 4, 4, 0 }, // WHITE LEFT-POINTING POINTER
    Entry { 0x25C6, Infix, 4, 4, 0 }, // BLACK DIAMOND
    Entry { 0x25C7, Infix, 4, 4, 0 }, // WHITE DIAMOND
    Entry { 0x25C8, Infix, 4, 4, 0 }, // WHITE DIAMOND CONTAINING BLACK SMALL DIAMOND
    Entry { 0x25C9, Infix, 4, 4, 0 }, // FISHEYE
    Entry { 0x25CC, Infix, 4, 4, 0 }, // DOTTED CIRCLE
    Entry { 0x25CD, Infix, 4, 4, 0 }, // CIRCLE WITH VERTICAL FILL
    Entry { 0x25CE, Infix, 4, 4, 0 }, // BULLSEYE
    Entry { 0x25CF, Infix, 4, 4, 0 }, // BLACK CIRCLE
    Entry { 0x25D6, Infix, 4, 4, 0 }, // LEFT HALF BLACK CIRCLE
    Entry { 0x25D7, Infix, 4, 4, 0 }, // RIGHT HALF BLACK CIRCLE
    Entry { 0x25E6, Infix, 4, 4, 0 }, // WHITE BULLET
    Entry { 0x266D, Postfix, 0, 2, 0 }, // MUSIC FLAT SIGN
    Entry { 0x266E, Postfix, 0, 2, 0 }, // MUSIC NATURAL SIGN
    Entry { 0x266F, Postfix, 0, 2, 0 }, // MUSIC SHARP SIGN
    Entry { 0x2758, Infix, 5, 5, 0 }, // LIGHT VERTICAL BAR
    Entry { 0x2772, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // LIGHT LEFT TORTOISE SHELL BRACKET ORNAMENT
    Entry { 0x2773, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // LIGHT RIGHT TORTOISE SHELL BRACKET ORNAMENT
    Entry { 0x27E6, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // MATHEMATICAL LEFT WHITE SQUARE BRACKET
    Entry { 0x27E7, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // MATHEMATICAL RIGHT WHITE SQUARE BRACKET
    Entry { 0x27E8, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // MATHEMATICAL LEFT ANGLE BRACKET
    Entry { 0x27E9, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // MATHEMATICAL RIGHT ANGLE BRACKET
    Entry { 0x27EA, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // MATHEMATICAL LEFT DOUBLE ANGLE BRACKET
    Entry { 0x27EB, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // MATHEMATICAL RIGHT DOUBLE ANGLE BRACKET
    Entry { 0x27EC, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // MATHEMATICAL LEFT WHITE TORTOISE SHELL BRACKET
    Entry { 0x27ED, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // MATHEMATICAL RIGHT WHITE TORTOISE SHELL BRACKET
    Entry { 0x27EE, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // MATHEMATICAL LEFT FLATTENED PARENTHESIS
    Entry { 0x27EF, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // MATHEMATICAL RIGHT FLATTENED PARENTHESIS
    Entry { 0x27F0, Infix, 5, 5, Stretchy }, // UPWARDS QUADRUPLE ARROW
    Entry { 0x27F1, Infix, 5, 5, Stretchy }, // DOWNWARDS QUADRUPLE ARROW
    Entry { 0x27F5, Infix, 5, 5, Stretchy | Accent }, // LONG LEFTWARDS ARROW
    Entry { 0x27F6, Infix, 5, 5, Stretchy | Accent }, // LONG RIGHTWARDS ARROW
    Entry { 0x27F7, Infix, 5, 5, Stretchy | Accent }, // LONG LEFT RIGHT ARROW
    Entry { 0x27F8, Infix, 5, 5, Stretchy | Accent }, // LONG LEFTWARDS DOUBLE ARROW
    Entry { 0x27F9, Infix, 5, 5, Stretchy | Accent }, // LONG RIGHTWARDS DOUBLE ARROW
    Entry { 0x27FA, Infix, 5, 5, Stretchy | Accent }, // LONG LEFT RIGHT DOUBLE ARROW
    Entry { 0x27FB, Infix, 5, 5, Stretchy | Accent }, // LONG LEFTWARDS ARROW FROM BAR
    Entry { 0x27FC, Infix, 5, 5, Stretchy | Accent }, // LONG RIGHTWARDS ARROW FROM BAR
    Entry { 0x27FD, Infix, 5, 5, Stretchy | Accent }, // LONG LEFTWARDS DOUBLE ARROW FROM BAR
    Entry { 0x27FE, Infix, 5, 5, Stretchy | Accent }, // LONG RIGHTWARDS DOUBLE ARROW FROM BAR
    Entry { 0x27FF, Infix, 5, 5, Stretchy | Accent }, // LONG RIGHTWARDS SQUIGGLE ARROW
    Entry { 0x2900, Infix, 5, 5, Accent }, // RIGHTWARDS TWO-HEADED ARROW WITH VERTICAL STROKE
    Entry { 0x2901, Infix, 5, 5, Accent }, // RIGHTWARDS TWO-HEADED ARROW WITH DOUBLE VERTICAL STROKE
    Entry { 0x2902, Infix, 5, 5, Accent }, // LEFTWARDS DOUBLE ARROW WITH VERTICAL STROKE
    Entry { 0x2903, Infix, 5, 5, Accent }, // RIGHTWARDS DOUBLE ARROW WITH VERTICAL STROKE
    Entry { 0x2904, Infix, 5, 5, Accent }, // LEFT RIGHT DOUBLE ARROW WITH VERTICAL STROKE
    Entry { 0x2905, Infix, 5, 5, Accent }, // RIGHTWARDS TWO-HEADED ARROW FROM BAR
    Entry { 0x2906, Infix, 5, 5, Accent }, // LEFTWARDS DOUBLE ARROW FROM BAR
    Entry { 0x2907, Infix, 5, 5, Accent }, // RIGHTWARDS DOUBLE ARROW FROM BAR
    Entry { 0x2908, Infix, 5, 5, 0 }, // DOWNWARDS ARROW WITH HORIZONTAL STROKE
    Entry { 0x2909, Infix, 5, 5, 0 }, // UPWARDS ARROW WITH HORIZONTAL STROKE
    Entry { 0x290A, Infix, 5, 5, Stretchy }, // UPWARDS TRIPLE ARROW
    Entry { 0x290B, Infix, 5, 5, Stretchy }, // DOWNWARDS TRIPLE ARROW
    Entry { 0x290C, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS DOUBLE DASH ARROW
    Entry { 0x290D, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS DOUBLE DASH ARROW
    Entry { 0x290E, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS TRIPLE DASH ARROW
    Entry { 0x290F, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS TRIPLE DASH ARROW
    Entry { 0x2910, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS TWO-HEADED TRIPLE DASH ARROW
    Entry { 0x2911, Infix, 5, 5, Accent }, // RIGHTWARDS ARROW WITH DOTTED STEM
    Entry { 0x2912, Infix, 5, 5, Stretchy }, // UPWARDS ARROW TO BAR
    Entry { 0x2913, Infix, 5, 5, Stretchy }, // DOWNWARDS ARROW TO BAR
    Entry { 0x2914, Infix, 5, 5, Accent }, // RIGHTWARDS ARROW WITH TAIL WITH VERTICAL STROKE
    Entry { 0x2915, Infix, 5, 5, Accent }, // RIGHTWARDS ARROW WITH TAIL WITH DOUBLE VERTICAL STROKE
    Entry { 0x2916, Infix, 5, 5, Accent }, // RIGHTWARDS TWO-HEADED ARROW WITH TAIL
    Entry { 0x2917, Infix, 5, 5, Accent }, // RIGHTWARDS TWO-HEADED ARROW WITH TAIL WITH VERTICAL STROKE
    Entry { 0x2918, Infix, 5, 5, Accent }, // RIGHTWARDS TWO-HEADED ARROW WITH TAIL WITH DOUBLE VERTICAL STROKE
    Entry { 0x2919, Infix, 5, 5, Accent }, // LEFTWARDS ARROW-TAIL
    Entry { 0x291A, Infix, 5, 5, Accent }, // RIGHTWARDS ARROW-TAIL
    Entry { 0x291B, Infix, 5, 5, Accent }, // LEFTWARDS DOUBLE ARROW-TAIL
    Entry { 0x291C, Infix, 5, 5, Accent }, // RIGHTWARDS DOUBLE ARROW-TAIL
    Entry { 0x291D, Infix, 5, 5, Accent }, // LEFTWARDS ARROW TO BLACK DIAMOND
    Entry { 0x291E, Infix, 5, 5, Accent }, // RIGHTWARDS ARROW TO BLACK DIAMOND
    Entry { 0x291F, Infix, 5, 5, Accent }, // LEFTWARDS ARROW FROM BAR TO BLACK DIAMOND
    Entry { 0x2920, Infix, 5, 5, Accent }, // RIGHTWARDS ARROW FROM BAR TO BLACK DIAMOND
    Entry { 0x2921, Infix, 5, 5, Stretchy }, // NORTH WEST AND SOUTH EAST ARROW
    Entry { 0x2922, Infix, 5, 5, Stretchy }, // NORTH EAST AND SOUTH WEST ARROW
    Entry { 0x2923, Infix, 5, 5, 0 }, // NORTH WEST ARROW WITH HOOK
    Entry { 0x2924, Infix, 5, 5, 0 }, // NORTH EAST ARROW WITH HOOK
    Entry { 0x2925, Infix, 5, 5, 0 }, // SOUTH EAST ARROW WITH HOOK
    Entry { 0x2926, Infix, 5, 5, 0 }, // SOUTH WEST ARROW WITH HOOK
    Entry { 0x2927, Infix, 5, 5, 0 }, // NORTH WEST ARROW AND NORTH EAST ARROW
    Entry { 0x2928, Infix, 5, 5, 0 }, // NORTH EAST ARROW AND SOUTH EAST ARROW
    Entry { 0x2929, Infix, 5, 5, 0 }, // SOUTH EAST ARROW AND SOUTH WEST ARROW
    Entry { 0x292A, Infix, 5, 5, 0 }, // SOUTH WEST ARROW AND NORTH WEST ARROW
    Entry { 0x292B, Infix, 5, 5, 0 }, // RISING DIAGONAL CROSSING FALLING DIAGONAL
    Entry { 0x292C, Infix, 5, 5, 0 }, // FALLING DIAGONAL CROSSING RISING DIAGONAL
    Entry { 0x292D, Infix, 5, 5, 0 }, // SOUTH EAST ARROW CROSSING NORTH EAST ARROW
    Entry { 0x292E, Infix, 5, 5, 0 }, // NORTH EAST ARROW CROSSING SOUTH EAST ARROW
    Entry { 0x292F, Infix, 5, 5, 0 }, // FALLING DIAGONAL CROSSING NORTH EAST ARROW
    Entry { 0x2930, Infix, 5, 5, 0 }, // RISING DIAGONAL CROSSING SOUTH EAST ARROW
    Entry { 0x2931, Infix, 5, 5, 0 }, // NORTH EAST ARROW CROSSING NORTH WEST ARROW
    Entry { 0x2932, Infix, 5, 5, 0 }, // NORTH WEST ARROW CROSSING NORTH EAST ARROW
    Entry { 0x2933, Infix, 5, 5, Accent }, // WAVE ARROW POINTING DIRECTLY RIGHT
    Entry { 0x2934, Infix, 5, 5, 0 }, // ARROW POINTING RIGHTWARDS THEN CURVING UPWARDS
    Entry { 0x2935, Infix, 5, 5, 0 }, // ARROW POINTING RIGHTWARDS THEN CURVING DOWNWARDS
    Entry { 0x2936, Infix, 5, 5, 0 }, // ARROW POINTING DOWNWARDS THEN CURVING LEFTWARDS
    Entry { 0x2937, Infix, 5, 5, 0 }, // ARROW POINTING DOWNWARDS THEN CURVING RIGHTWARDS
    Entry { 0x2938, Infix, 5, 5, 0 }, // RIGHT-SIDE ARC CLOCKWISE ARROW
    Entry { 0x2939, Infix, 5, 5, 0 }, // LEFT-SIDE ARC ANTICLOCKWISE ARROW
    Entry { 0x293A, Infix, 5, 5, Accent }, // TOP ARC ANTICLOCKWISE ARROW
    Entry { 0x293B, Infix, 5, 5, Accent }, // BOTTOM ARC ANTICLOCKWISE ARROW
    Entry { 0x293C, Infix, 5, 5, Accent }, // TOP ARC CLOCKWISE ARROW WITH MINUS
    Entry { 0x293D, Infix, 5, 5, Accent }, // TOP ARC ANTICLOCKWISE ARROW WITH PLUS
    Entry { 0x293E, Infix, 5, 5, 0 }, // LOWER RIGHT SEMICIRCULAR CLOCKWISE ARROW
    Entry { 0x293F, Infix, 5, 5, 0 }, // LOWER LEFT SEMICIRCULAR ANTICLOCKWISE ARROW
    Entry { 0x2940, Infix, 5, 5, 0 }, // ANTICLOCKWISE CLOSED CIRCLE ARROW
    Entry { 0x2941, Infix, 5, 5, 0 }, // CLOCKWISE CLOSED CIRCLE ARROW
    Entry { 0x2942, Infix, 5, 5, Accent }, // RIGHTWARDS ARROW ABOVE SHORT LEFTWARDS ARROW
    Entry { 0x2943, Infix, 5, 5, Accent }, // LEFTWARDS ARROW ABOVE SHORT RIGHTWARDS ARROW
    Entry { 0x2944, Infix, 5, 5, Accent }, // SHORT RIGHTWARDS ARROW ABOVE LEFTWARDS ARROW
    Entry { 0x2945, Infix, 5, 5, Accent }, // RIGHTWARDS ARROW WITH PLUS BELOW
    Entry { 0x2946, Infix, 5, 5, Accent }, // LEFTWARDS ARROW WITH PLUS BELOW
    Entry { 0x2947, Infix, 5, 5, Accent }, // RIGHTWARDS ARROW THROUGH X
    Entry { 0x2948, Infix, 5, 5, Accent }, // LEFT RIGHT ARROW THROUGH SMALL CIRCLE
    Entry { 0x2949, Infix, 5, 5, 0 }, // UPWARDS TWO-HEADED ARROW FROM SMALL CIRCLE
    Entry { 0x294A, Infix, 5, 5, Accent }, // LEFT BARB UP RIGHT BARB DOWN HARPOON
    Entry { 0x294B, Infix, 5, 5, Accent }, // LEFT BARB DOWN RIGHT BARB UP HARPOON
    Entry { 0x294C, Infix, 5, 5, 0 }, // UP BARB RIGHT DOWN BARB LEFT HARPOON
    Entry { 0x294D, Infix, 5, 5, 0 }, // UP BARB LEFT DOWN BARB RIGHT HARPOON
    Entry { 0x294E, Infix, 5, 5, Stretchy | Accent }, // LEFT BARB UP RIGHT BARB UP HARPOON
    Entry { 0x294F, Infix, 5, 5, Stretchy }, // UP BARB RIGHT DOWN BARB RIGHT HARPOON
    Entry { 0x2950, Infix, 5, 5, Stretchy | Accent }, // LEFT BARB DOWN RIGHT BARB DOWN HARPOON
    Entry { 0x2951, Infix, 5, 5, Stretchy }, // UP BARB LEFT DOWN BARB LEFT HARPOON
    Entry { 0x2952, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS HARPOON WITH BARB UP TO BAR
    Entry { 0x2953, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS HARPOON WITH BARB UP TO BAR
    Entry { 0x2954, Infix, 5, 5, Stretchy }, // UPWARDS HARPOON WITH BARB RIGHT TO BAR
    Entry { 0x2955, Infix, 5, 5, Stretchy }, // DOWNWARDS HARPOON WITH BARB RIGHT TO BAR
    Entry { 0x2956, Infix, 5, 5, Stretchy }, // LEFTWARDS HARPOON WITH BARB DOWN TO BAR
    Entry { 0x2957, Infix, 5, 5, Stretchy }, // RIGHTWARDS HARPOON WITH BARB DOWN TO BAR
    Entry { 0x2958, Infix, 5, 5, Stretchy }, // UPWARDS HARPOON WITH BARB LEFT TO BAR
    Entry { 0x2959, Infix, 5, 5, Stretchy }, // DOWNWARDS HARPOON WITH BARB LEFT TO BAR
    Entry { 0x295A, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS HARPOON WITH BARB UP FROM BAR
    Entry { 0x295B, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS HARPOON WITH BARB UP FROM BAR
    Entry { 0x295C, Infix, 5, 5, Stretchy }, // UPWARDS HARPOON WITH BARB RIGHT FROM BAR
    Entry { 0x295D, Infix, 5, 5, Stretchy }, // DOWNWARDS HARPOON WITH BARB RIGHT FROM BAR
    Entry { 0x295E, Infix, 5, 5, Stretchy | Accent }, // LEFTWARDS HARPOON WITH BARB DOWN FROM BAR
    Entry { 0x295F, Infix, 5, 5, Stretchy | Accent }, // RIGHTWARDS HARPOON WITH BARB DOWN FROM BAR
    Entry { 0x2960, Infix, 5, 5, Stretchy }, // UPWARDS HARPOON WITH BARB LEFT FROM BAR
    Entry { 0x2961, Infix, 5, 5, Stretchy }, // DOWNWARDS HARPOON WITH BARB LEFT FROM BAR
    Entry { 0x2962, Infix, 5, 5, Accent }, // LEFTWARDS HARPOON WITH BARB UP ABOVE LEFTWARDS HARPOON WITH BARB DOWN
    Entry { 0x2963, Infix, 5, 5, 0 }, // UPWARDS HARPOON WITH BARB LEFT BESIDE UPWARDS HARPOON WITH BARB RIGHT
    Entry { 0x2964, Infix, 5, 5, Accent }, // RIGHTWARDS HARPOON WITH BARB UP ABOVE RIGHTWARDS HARPOON WITH BARB DOWN
    Entry { 0x2965, Infix, 5, 5, 0 }, // DOWNWARDS HARPOON WITH BARB LEFT BESIDE DOWNWARDS HARPOON WITH BARB RIGHT
    Entry { 0x2966, Infix, 5, 5, Accent }, // LEFTWARDS HARPOON WITH BARB UP ABOVE RIGHTWARDS HARPOON WITH BARB UP
    Entry { 0x2967, Infix, 5, 5, Accent }, // LEFTWARDS HARPOON WITH BARB DOWN ABOVE RIGHTWARDS HARPOON WITH BARB DOWN
    Entry { 0x2968, Infix, 5, 5, Accent }, // RIGHTWARDS HARPOON WITH BARB UP ABOVE LEFTWARDS HARPOON WITH BARB UP
    Entry { 0x2969, Infix, 5, 5, Accent }, // RIGHTWARDS HARPOON WITH BARB DOWN ABOVE LEFTWARDS HARPOON WITH BARB DOWN
    Entry { 0x296A, Infix, 5, 5, Accent }, // LEFTWARDS HARPOON WITH BARB UP ABOVE LONG DASH
    Entry { 0x296B, Infix, 5, 5, Accent }, // LEFTWARDS HARPOON WITH BARB DOWN BELOW LONG DASH
    Entry { 0x296C, Infix, 5, 5, Accent }, // RIGHTWARDS HARPOON WITH BARB UP ABOVE LONG DASH
    Entry { 0x296D, Infix, 5, 5, Accent }, // RIGHTWARDS HARPOON WITH BARB DOWN BELOW LONG DASH
    Entry { 0x296E, Infix, 5, 5, Stretchy }, // UPWARDS HARPOON WITH BARB LEFT BESIDE DOWNWARDS HARPOON WITH BARB RIGHT
    Entry { 0x296F, Infix, 5, 5, Stretchy }, // DOWNWARDS HARPOON WITH BARB LEFT BESIDE UPWARDS HARPOON WITH BARB RIGHT
    Entry { 0x2970, Infix, 5, 5, Accent }, // RIGHT DOUBLE ARROW WITH ROUNDED HEAD
    Entry { 0x2971, Infix, 5, 5, Accent }, // EQUALS SIGN ABOVE RIGHTWARDS ARROW
    Entry { 0x2972, Infix, 5, 5, Accent }, // TILDE OPERATOR ABOVE RIGHTWARDS ARROW
    Entry { 0x2973, Infix, 5, 5, Accent }, // LEFTWARDS ARROW ABOVE TILDE OPERATOR
    Entry { 0x2974, Infix, 5, 5, Accent }, // RIGHTWARDS ARROW ABOVE TILDE OPERATOR
    Entry { 0x2975, Infix, 5, 5, Accent }, // RIGHTWARDS ARROW ABOVE ALMOST EQUAL TO
    Entry { 0x2976, Infix, 5, 5, Accent }, // LESS-THAN ABOVE LEFTWARDS ARROW
    Entry { 0x2977, Infix, 5, 5, Accent }, // LEFTWARDS ARROW THROUGH LESS-THAN
    Entry { 0x2978, Infix, 5, 5, Accent }, // GREATER-THAN ABOVE RIGHTWARDS ARROW
    Entry { 0x2979, Infix, 5, 5, Accent }, // SUBSET ABOVE RIGHTWARDS ARROW
    Entry { 0x297A, Infix, 5, 5, Accent }, // LEFTWARDS ARROW THROUGH SUBSET
    Entry { 0x297B, Infix, 5, 5, Accent }, // SUPERSET ABOVE LEFTWARDS ARROW
    Entry { 0x297C, Infix, 5, 5, Accent }, // LEFT FISH TAIL
    Entry { 0x297D, Infix, 5, 5, Accent }, // RIGHT FISH TAIL
    Entry { 0x297E, Infix, 5, 5, 0 }, // UP FISH TAIL
    Entry { 0x297F, Infix, 5, 5, 0 }, // DOWN FISH TAIL
    Entry { 0x2980, Prefix, 0, 0, Fence | Stretchy }, // TRIPLE VERTICAL BAR DELIMITER
    Entry { 0x2980, Postfix, 0, 0, Fence | Stretchy }, // TRIPLE VERTICAL BAR DELIMITER
    Entry { 0x2981, Infix, 3, 3, 0 }, // Z NOTATION SPOT
    Entry { 0x2982, Infix, 3, 3, 0 }, // Z NOTATION TYPE COLON
    Entry { 0x2983, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // LEFT WHITE CURLY BRACKET
    Entry { 0x2984, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // RIGHT WHITE CURLY BRACKET
    Entry { 0x2985, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // LEFT WHITE PARENTHESIS
    Entry { 0x2986, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // RIGHT WHITE PARENTHESIS
    Entry { 0x2987, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // Z NOTATION LEFT IMAGE BRACKET
    Entry { 0x2988, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // Z NOTATION RIGHT IMAGE BRACKET
    Entry { 0x2989, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // Z NOTATION LEFT BINDING BRACKET
    Entry { 0x298A, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // Z NOTATION RIGHT BINDING BRACKET
    Entry { 0x298B, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // LEFT SQUARE BRACKET WITH UNDERBAR
    Entry { 0x298C, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // RIGHT SQUARE BRACKET WITH UNDERBAR
    Entry { 0x298D, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // LEFT SQUARE BRACKET WITH TICK IN TOP CORNER
    Entry { 0x298E, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // RIGHT SQUARE BRACKET WITH TICK IN BOTTOM CORNER
    Entry { 0x298F, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // LEFT SQUARE BRACKET WITH TICK IN BOTTOM CORNER
    Entry { 0x2990, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // RIGHT SQUARE BRACKET WITH TICK IN TOP CORNER
    Entry { 0x2991, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // LEFT ANGLE BRACKET WITH DOT
    Entry { 0x2992, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // RIGHT ANGLE BRACKET WITH DOT
    Entry { 0x2993, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // LEFT ARC LESS-THAN BRACKET
    Entry { 0x2994, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // RIGHT ARC GREATER-THAN BRACKET
    Entry { 0x2995, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // DOUBLE LEFT ARC GREATER-THAN BRACKET
    Entry { 0x2996, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // DOUBLE RIGHT ARC LESS-THAN BRACKET
    Entry { 0x2997, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // LEFT BLACK TORTOISE SHELL BRACKET
    Entry { 0x2998, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // RIGHT BLACK TORTOISE SHELL BRACKET
    Entry { 0x2999, Infix, 3, 3, 0 }, // DOTTED FENCE
    Entry { 0x299A, Infix, 3, 3, 0 }, // VERTICAL ZIGZAG LINE
    Entry { 0x299B, Infix, 3, 3, 0 }, // MEASURED ANGLE OPENING LEFT
    Entry { 0x299C, Infix, 3, 3, 0 }, // RIGHT ANGLE VARIANT WITH SQUARE
    Entry { 0x299D, Infix, 3, 3, 0 }, // MEASURED RIGHT ANGLE WITH DOT
    Entry { 0x299E, Infix, 3, 3, 0 }, // ANGLE WITH S INSIDE
    Entry { 0x299F, Infix, 3, 3, 0 }, // ACUTE ANGLE
    Entry { 0x29A0, Infix, 3, 3, 0 }, // SPHERICAL ANGLE OPENING LEFT
    Entry { 0x29A1, Infix, 3, 3, 0 }, // SPHERICAL ANGLE OPENING UP
    Entry { 0x29A2, Infix, 3, 3, 0 }, // TURNED ANGLE
    Entry { 0x29A3, Infix, 3, 3, 0 }, // REVERSED ANGLE
    Entry { 0x29A4, Infix, 3, 3, 0 }, // ANGLE WITH UNDERBAR
    Entry { 0x29A5, Infix, 3, 3, 0 }, // REVERSED ANGLE WITH UNDERBAR
    Entry { 0x29A6, Infix, 3, 3, 0 }, // OBLIQUE ANGLE OPENING UP
    Entry { 0x29A7, Infix, 3, 3, 0 }, // OBLIQUE ANGLE OPENING DOWN
    Entry { 0x29A8, Infix, 3, 3, 0 }, // MEASURED ANGLE WITH OPEN ARM ENDING IN ARROW POINTING UP AND RIGHT
    Entry { 0x29A9, Infix, 3, 3, 0 }, // MEASURED ANGLE WITH OPEN ARM ENDING IN ARROW POINTING UP AND LEFT
    Entry { 0x29AA, Infix, 3, 3, 0 }, // MEASURED ANGLE WITH OPEN ARM ENDING IN ARROW POINTING DOWN AND RIGHT
    Entry { 0x29AB, Infix, 3, 3, 0 }, // MEASURED ANGLE WITH OPEN ARM ENDING IN ARROW POINTING DOWN AND LEFT
    Entry { 0x29AC, Infix, 3, 3, 0 }, // MEASURED ANGLE WITH OPEN ARM ENDING IN ARROW POINTING RIGHT AND UP
    Entry { 0x29AD, Infix, 3, 3, 0 }, // MEASURED ANGLE WITH OPEN ARM ENDING IN ARROW POINTING LEFT AND UP
    Entry { 0x29AE, Infix, 3, 3, 0 }, // MEASURED ANGLE WITH OPEN ARM ENDING IN ARROW POINTING RIGHT AND DOWN
    Entry { 0x29AF, Infix, 3, 3, 0 }, // MEASURED ANGLE WITH OPEN ARM ENDING IN ARROW POINTING LEFT AND DOWN
    Entry { 0x29B0, Infix, 3, 3, 0 }, // REVERSED EMPTY SET
    Entry { 0x29B1, Infix, 3, 3, 0 }, // EMPTY SET WITH OVERBAR
    Entry { 0x29B2, Infix, 3, 3, 0 }, // EMPTY SET WITH SMALL CIRCLE ABOVE
    Entry { 0x29B3, Infix, 3, 3, 0 }, // EMPTY SET WITH RIGHT ARROW ABOVE
    Entry { 0x29B4, Infix, 3, 3, 0 }, // EMPTY SET WITH LEFT ARROW ABOVE
    Entry { 0x29B5, Infix, 3, 3, 0 }, // CIRCLE WITH HORIZONTAL BAR
    Entry { 0x29B6, Infix, 4, 4, 0 }, // CIRCLED VERTICAL BAR
    Entry { 0x29B7, Infix, 4, 4, 0 }, // CIRCLED PARALLEL
    Entry { 0x29B8, Infix, 4, 4, 0 }, // CIRCLED REVERSE SOLIDUS
    Entry { 0x29B9, Infix, 4, 4, 0 }, // CIRCLED PERPENDICULAR
    Entry { 0x29BA, Infix, 4, 4, 0 }, // CIRCLE DIVIDED BY HORIZONTAL BAR AND TOP HALF DIVIDED BY VERTICAL BAR
    Entry { 0x29BB, Infix, 4, 4, 0 }, // CIRCLE WITH SUPERIMPOSED X
    Entry { 0x29BC, Infix, 4, 4, 0 }, // CIRCLED ANTICLOCKWISE-ROTATED DIVISION SIGN
    Entry { 0x29BD, Infix, 4, 4, 0 }, // UP ARROW THROUGH CIRCLE
    Entry { 0x29BE, Infix, 4, 4, 0 }, // CIRCLED WHITE BULLET
    Entry { 0x29BF, Infix, 4, 4, 0 }, // CIRCLED BULLET
    Entry { 0x29C0, Infix, 5, 5, 0 }, // CIRCLED LESS-THAN
    Entry { 0x29C1, Infix, 5, 5, 0 }, // CIRCLED GREATER-THAN
    Entry { 0x29C2, Infix, 3, 3, 0 }, // CIRCLE WITH SMALL CIRCLE TO THE RIGHT
    Entry { 0x29C3, Infix, 3, 3, 0 }, // CIRCLE WITH TWO HORIZONTAL STROKES TO THE RIGHT
    Entry { 0x29C4, Infix, 4, 4, 0 }, // SQUARED RISING DIAGONAL SLASH
    Entry { 0x29C5, Infix, 4, 4, 0 }, // SQUARED FALLING DIAGONAL SLASH
    Entry { 0x29C6, Infix, 4, 4, 0 }, // SQUARED ASTERISK
    Entry { 0x29C7, Infix, 4, 4, 0 }, // SQUARED SMALL CIRCLE
    Entry { 0x29C8, Infix, 4, 4, 0 }, // SQUARED SQUARE
    Entry { 0x29C9, Infix, 3, 3, 0 }, // TWO JOINED SQUARES
    Entry { 0x29CA, Infix, 3, 3, 0 }, // TRIANGLE WITH DOT ABOVE
    Entry { 0x29CB, Infix, 3, 3, 0 }, // TRIANGLE WITH UNDERBAR
    Entry { 0x29CC, Infix, 3, 3, 0 }, // S IN TRIANGLE
    Entry { 0x29CD, Infix, 3, 3, 0 }, // TRIANGLE WITH SERIFS AT BOTTOM
    Entry { 0x29CE, Infix, 5, 5, 0 }, // RIGHT TRIANGLE ABOVE LEFT TRIANGLE
    Entry { 0x29CF, Infix, 5, 5, 0 }, // LEFT TRIANGLE BESIDE VERTICAL BAR
    Entry { 0x29D0, Infix, 5, 5, 0 }, // VERTICAL BAR BESIDE RIGHT TRIANGLE
    Entry { 0x29D1, Infix, 5, 5, 0 }, // BOWTIE WITH LEFT HALF BLACK
    Entry { 0x29D2, Infix, 5, 5, 0 }, // BOWTIE WITH RIGHT HALF BLACK
    Entry { 0x29D3, Infix, 5, 5, 0 }, // BLACK BOWTIE
    Entry { 0x29D4, Infix, 5, 5, 0 }, // TIMES WITH LEFT HALF BLACK
    Entry { 0x29D5, Infix, 5, 5, 0 }, // TIMES WITH RIGHT HALF BLACK
    Entry { 0x29D6, Infix, 4, 4, 0 }, // WHITE HOURGLASS
    Entry { 0x29D7, Infix, 4, 4, 0 }, // BLACK HOURGLASS
    Entry { 0x29D8, Infix, 3, 3, 0 }, // LEFT WIGGLY FENCE
    Entry { 0x29D9, Infix, 3, 3, 0 }, // RIGHT WIGGLY FENCE
    Entry { 0x29DB, Infix, 3, 3, 0 }, // RIGHT DOUBLE WIGGLY FENCE
    Entry { 0x29DC, Infix, 3, 3, 0 }, // INCOMPLETE INFINITY
    Entry { 0x29DD, Infix, 3, 3, 0 }, // TIE OVER INFINITY
    Entry { 0x29DE, Infix, 5, 5, 0 }, // INFINITY NEGATED WITH VERTICAL BAR
    Entry { 0x29DF, Infix, 3, 3, 0 }, // DOUBLE-ENDED MULTIMAP
    Entry { 0x29E0, Infix, 3, 3, 0 }, // SQUARE WITH CONTOURED OUTLINE
    Entry { 0x29E1, Infix, 5, 5, 0 }, // INCREASES AS
    Entry { 0x29E2, Infix, 4, 4, 0 }, // SHUFFLE PRODUCT
    Entry { 0x29E3, Infix, 5, 5, 0 }, // EQUALS SIGN AND SLANTED PARALLEL
    Entry { 0x29E4, Infix, 5, 5, 0 }, // EQUALS SIGN AND SLANTED PARALLEL WITH TILDE ABOVE
    Entry { 0x29E5, Infix, 5, 5, 0 }, // IDENTICAL TO AND SLANTED PARALLEL
    Entry { 0x29E6, Infix, 5, 5, 0 }, // GLEICH STARK
    Entry { 0x29E7, Infix, 3, 3, 0 }, // THERMODYNAMIC
    Entry { 0x29E8, Infix, 3, 3, 0 }, // DOWN-POINTING TRIANGLE WITH LEFT HALF BLACK
    Entry { 0x29E9, Infix, 3, 3, 0 }, // DOWN-POINTING TRIANGLE WITH RIGHT HALF BLACK
    Entry { 0x29EA, Infix, 3, 3, 0 }, // BLACK DIAMOND WITH DOWN ARROW
    Entry { 0x29EB, Infix, 3, 3, 0 }, // BLACK LOZENGE
    Entry { 0x29EC, Infix, 3, 3, 0 }, // WHITE CIRCLE WITH DOWN ARROW
    Entry { 0x29ED, Infix, 3, 3, 0 }, // BLACK CIRCLE WITH DOWN ARROW
    Entry { 0x29EE, Infix, 3, 3, 0 }, // ERROR-BARRED WHITE SQUARE
    Entry { 0x29EF, Infix, 3, 3, 0 }, // ERROR-BARRED BLACK SQUARE
    Entry { 0x29F0, Infix, 3, 3, 0 }, // ERROR-BARRED WHITE DIAMOND
    Entry { 0x29F1, Infix, 3, 3, 0 }, // ERROR-BARRED BLACK DIAMOND
    Entry { 0x29F2, Infix, 3, 3, 0 }, // ERROR-BARRED WHITE CIRCLE
    Entry { 0x29F3, Infix, 3, 3, 0 }, // ERROR-BARRED BLACK CIRCLE
    Entry { 0x29F4, Infix, 5, 5, 0 }, // RULE-DELAYED
    Entry { 0x29F5, Infix, 4, 4, 0 }, // REVERSE SOLIDUS OPERATOR
    Entry { 0x29F6, Infix, 4, 4, 0 }, // SOLIDUS WITH OVERBAR
    Entry { 0x29F7, Infix, 4, 4, 0 }, // REVERSE SOLIDUS WITH HORIZONTAL STROKE
    Entry { 0x29F8, Infix, 3, 3, 0 }, // BIG SOLIDUS
    Entry { 0x29F9, Infix, 3, 3, 0 }, // BIG REVERSE SOLIDUS
    Entry { 0x29FA, Infix, 3, 3, 0 }, // DOUBLE PLUS
    Entry { 0x29FB, Infix, 3, 3, 0 }, // TRIPLE PLUS
    Entry { 0x29FC, Prefix, 0, 0, Symmetric | Fence | Stretchy }, // LEFT-POINTING CURVED ANGLE BRACKET
    Entry { 0x29FD, Postfix, 0, 0, Symmetric | Fence | Stretchy }, // RIGHT-POINTING CURVED ANGLE BRACKET
    Entry { 0x29FE, Infix, 4, 4, 0 }, // TINY
    Entry { 0x29FF, Infix, 4, 4, 0 }, // MINY
    Entry { 0x2A00, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // N-ARY CIRCLED DOT OPERATOR
    Entry { 0x2A01, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // N-ARY CIRCLED PLUS OPERATOR
    Entry { 0x2A02, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // N-ARY CIRCLED TIMES OPERATOR
    Entry { 0x2A03, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // N-ARY UNION OPERATOR WITH DOT
    Entry { 0x2A04, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // N-ARY UNION OPERATOR WITH PLUS
    Entry { 0x2A05, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // N-ARY SQUARE INTERSECTION OPERATOR
    Entry { 0x2A06, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // N-ARY SQUARE UNION OPERATOR
    Entry { 0x2A07, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // TWO LOGICAL AND OPERATOR
    Entry { 0x2A08, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // TWO LOGICAL OR OPERATOR
    Entry { 0x2A09, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // N-ARY TIMES OPERATOR
    Entry { 0x2A0A, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // MODULO TWO SUM
    Entry { 0x2A0B, Prefix, 1, 2, Symmetric | LargeOp }, // SUMMATION WITH INTEGRAL
    Entry { 0x2A0C, Prefix, 0, 1, Symmetric | LargeOp }, // QUADRUPLE INTEGRAL OPERATOR
    Entry { 0x2A0D, Prefix, 1, 2, Symmetric | LargeOp }, // FINITE PART INTEGRAL
    Entry { 0x2A0E, Prefix, 1, 2, Symmetric | LargeOp }, // INTEGRAL WITH DOUBLE STROKE
    Entry { 0x2A0F, Prefix, 1, 2, Symmetric | LargeOp }, // INTEGRAL AVERAGE WITH SLASH
    Entry { 0x2A10, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // CIRCULATION FUNCTION
    Entry { 0x2A11, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // ANTICLOCKWISE INTEGRATION
    Entry { 0x2A12, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // LINE INTEGRATION WITH RECTANGULAR PATH AROUND POLE
    Entry { 0x2A13, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // LINE INTEGRATION WITH SEMICIRCULAR PATH AROUND POLE
    Entry { 0x2A14, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // LINE INTEGRATION NOT INCLUDING THE POLE
    Entry { 0x2A15, Prefix, 1, 2, Symmetric | LargeOp }, // INTEGRAL AROUND A POINT OPERATOR
    Entry { 0x2A16, Prefix, 1, 2, Symmetric | LargeOp }, // QUATERNION INTEGRAL OPERATOR
    Entry { 0x2A17, Prefix, 1, 2, Symmetric | LargeOp }, // INTEGRAL WITH LEFTWARDS ARROW WITH HOOK
    Entry { 0x2A18, Prefix, 1, 2, Symmetric | LargeOp }, // INTEGRAL WITH TIMES SIGN
    Entry { 0x2A19, Prefix, 1, 2, Symmetric | LargeOp }, // INTEGRAL WITH INTERSECTION
    Entry { 0x2A1A, Prefix, 1, 2, Symmetric | LargeOp }, // INTEGRAL WITH UNION
    Entry { 0x2A1B, Prefix, 1, 2, Symmetric | LargeOp }, // INTEGRAL WITH OVERBAR
    Entry { 0x2A1C, Prefix, 1, 2, Symmetric | LargeOp }, // INTEGRAL WITH UNDERBAR
    Entry { 0x2A1D, Infix, 3, 3, 0 }, // JOIN
    Entry { 0x2A1E, Infix, 3, 3, 0 }, // LARGE LEFT TRIANGLE OPERATOR
    Entry { 0x2A1F, Infix, 3, 3, 0 }, // Z NOTATION SCHEMA COMPOSITION
    Entry { 0x2A20, Infix, 3, 3, 0 }, // Z NOTATION SCHEMA PIPING
    Entry { 0x2A21, Infix, 3, 3, 0 }, // Z NOTATION SCHEMA PROJECTION
    Entry { 0x2A22, Infix, 4, 4, 0 }, // PLUS SIGN WITH SMALL CIRCLE ABOVE
    Entry { 0x2A23, Infix, 4, 4, 0 }, // PLUS SIGN WITH CIRCUMFLEX ACCENT ABOVE
    Entry { 0x2A24, Infix, 4, 4, 0 }, // PLUS SIGN WITH TILDE ABOVE
    Entry { 0x2A25, Infix, 4, 4, 0 }, // PLUS SIGN WITH DOT BELOW
    Entry { 0x2A26, Infix, 4, 4, 0 }, // PLUS SIGN WITH TILDE BELOW
    Entry { 0x2A27, Infix, 4, 4, 0 }, // PLUS SIGN WITH SUBSCRIPT TWO
    Entry { 0x2A28, Infix, 4, 4, 0 }, // PLUS SIGN WITH BLACK TRIANGLE
    Entry { 0x2A29, Infix, 4, 4, 0 }, // MINUS SIGN WITH COMMA ABOVE
    Entry { 0x2A2A, Infix, 4, 4, 0 }, // MINUS SIGN WITH DOT BELOW
    Entry { 0x2A2B, Infix, 4, 4, 0 }, // MINUS SIGN WITH FALLING DOTS
    Entry { 0x2A2C, Infix, 4, 4, 0 }, // MINUS SIGN WITH RISING DOTS
    Entry { 0x2A2D, Infix, 4, 4, 0 }, // PLUS SIGN IN LEFT HALF CIRCLE
    Entry { 0x2A2E, Infix, 4, 4, 0 }, // PLUS SIGN IN RIGHT HALF CIRCLE
    Entry { 0x2A2F, Infix, 4, 4, 0 }, // VECTOR OR CROSS PRODUCT
    Entry { 0x2A30, Infix, 4, 4, 0 }, // MULTIPLICATION SIGN WITH DOT ABOVE
    Entry { 0x2A31, Infix, 4, 4, 0 }, // MULTIPLICATION SIGN WITH UNDERBAR
    Entry { 0x2A32, Infix, 4, 4, 0 }, // SEMIDIRECT PRODUCT WITH BOTTOM CLOSED
    Entry { 0x2A33, Infix, 4, 4, 0 }, // SMASH PRODUCT
    Entry { 0x2A34, Infix, 4, 4, 0 }, // MULTIPLICATION SIGN IN LEFT HALF CIRCLE
    Entry { 0x2A35, Infix, 4, 4, 0 }, // MULTIPLICATION SIGN IN RIGHT HALF CIRCLE
    Entry { 0x2A36, Infix, 4, 4, 0 }, // CIRCLED MULTIPLICATION SIGN WITH CIRCUMFLEX ACCENT
    Entry { 0x2A37, Infix, 4, 4, 0 }, // MULTIPLICATION SIGN IN DOUBLE CIRCLE
    Entry { 0x2A38, Infix, 4, 4, 0 }, // CIRCLED DIVISION SIGN
    Entry { 0x2A39, Infix, 4, 4, 0 }, // PLUS SIGN IN TRIANGLE
    Entry { 0x2A3A, Infix, 4, 4, 0 }, // MINUS SIGN IN TRIANGLE
    Entry { 0x2A3B, Infix, 4, 4, 0 }, // MULTIPLICATION SIGN IN TRIANGLE
    Entry { 0x2A3C, Infix, 4, 4, 0 }, // INTERIOR PRODUCT
    Entry { 0x2A3D, Infix, 4, 4, 0 }, // RIGHTHAND INTERIOR PRODUCT
    Entry { 0x2A3E, Infix, 4, 4, 0 }, // Z NOTATION RELATIONAL COMPOSITION
    Entry { 0x2A3F, Infix, 4, 4, 0 }, // AMALGAMATION OR COPRODUCT
    Entry { 0x2A40, Infix, 4, 4, 0 }, // INTERSECTION WITH DOT
    Entry { 0x2A41, Infix, 4, 4, 0 }, // UNION WITH MINUS SIGN
    Entry { 0x2A42, Infix, 4, 4, 0 }, // UNION WITH OVERBAR
    Entry { 0x2A43, Infix, 4, 4, 0 }, // INTERSECTION WITH OVERBAR
    Entry { 0x2A44, Infix, 4, 4, 0 }, // INTERSECTION WITH LOGICAL AND
    Entry { 0x2A45, Infix, 4, 4, 0 }, // UNION WITH LOGICAL OR
    Entry { 0x2A46, Infix, 4, 4, 0 }, // UNION ABOVE INTERSECTION
    Entry { 0x2A47, Infix, 4, 4, 0 }, // INTERSECTION ABOVE UNION
    Entry { 0x2A48, Infix, 4, 4, 0 }, // UNION ABOVE BAR ABOVE INTERSECTION
    Entry { 0x2A49, Infix, 4, 4, 0 }, // INTERSECTION ABOVE BAR ABOVE UNION
    Entry { 0x2A4A, Infix, 4, 4, 0 }, // UNION BESIDE AND JOINED WITH UNION
    Entry { 0x2A4B, Infix, 4, 4, 0 }, // INTERSECTION BESIDE AND JOINED WITH INTERSECTION
    Entry { 0x2A4C, Infix, 4, 4, 0 }, // CLOSED UNION WITH SERIFS
    Entry { 0x2A4D, Infix, 4, 4, 0 }, // CLOSED INTERSECTION WITH SERIFS
    Entry { 0x2A4E, Infix, 4, 4, 0 }, // DOUBLE SQUARE INTERSECTION
    Entry { 0x2A4F, Infix, 4, 4, 0 }, // DOUBLE SQUARE UNION
    Entry { 0x2A50, Infix, 4, 4, 0 }, // CLOSED UNION WITH SERIFS AND SMASH PRODUCT
    Entry { 0x2A51, Infix, 4, 4, 0 }, // LOGICAL AND WITH DOT ABOVE
    Entry { 0x2A52, Infix, 4, 4, 0 }, // LOGICAL OR WITH DOT ABOVE
    Entry { 0x2A53, Infix, 4, 4, 0 }, // DOUBLE LOGICAL AND
    Entry { 0x2A54, Infix, 4, 4, 0 }, // DOUBLE LOGICAL OR
    Entry { 0x2A55, Infix, 4, 4, 0 }, // TWO INTERSECTING LOGICAL AND
    Entry { 0x2A56, Infix, 4, 4, 0 }, // TWO INTERSECTING LOGICAL OR
    Entry { 0x2A57, Infix, 4, 4, 0 }, // SLOPING LARGE OR
    Entry { 0x2A58, Infix, 4, 4, 0 }, // SLOPING LARGE AND
    Entry { 0x2A59, Infix, 5, 5, 0 }, // LOGICAL OR OVERLAPPING LOGICAL AND
    Entry { 0x2A5A, Infix, 4, 4, 0 }, // LOGICAL AND WITH MIDDLE STEM
    Entry { 0x2A5B, Infix, 4, 4, 0 }, // LOGICAL OR WITH MIDDLE STEM
    Entry { 0x2A5C, Infix, 4, 4, 0 }, // LOGICAL AND WITH HORIZONTAL DASH
    Entry { 0x2A5D, Infix, 4, 4, 0 }, // LOGICAL OR WITH HORIZONTAL DASH
    Entry { 0x2A5E, Infix, 4, 4, 0 }, // LOGICAL AND WITH DOUBLE OVERBAR
    Entry { 0x2A5F, Infix, 4, 4, 0 }, // LOGICAL AND WITH UNDERBAR
    Entry { 0x2A60, Infix, 4, 4, 0 }, // LOGICAL AND WITH DOUBLE UNDERBAR
    Entry { 0x2A61, Infix, 4, 4, 0 }, // SMALL VEE WITH UNDERBAR
    Entry { 0x2A62, Infix, 4, 4, 0 }, // LOGICAL OR WITH DOUBLE OVERBAR
    Entry { 0x2A63, Infix, 4, 4, 0 }, // LOGICAL OR WITH DOUBLE UNDERBAR
    Entry { 0x2A64, Infix, 4, 4, 0 }, // Z NOTATION DOMAIN ANTIRESTRICTION
    Entry { 0x2A65, Infix, 4, 4, 0 }, // Z NOTATION RANGE ANTIRESTRICTION
    Entry { 0x2A66, Infix, 5, 5, 0 }, // EQUALS SIGN WITH DOT BELOW
    Entry { 0x2A67, Infix, 5, 5, 0 }, // IDENTICAL WITH DOT ABOVE
    Entry { 0x2A68, Infix, 5, 5, 0 }, // TRIPLE HORIZONTAL BAR WITH DOUBLE VERTICAL STROKE
    Entry { 0x2A69, Infix, 5, 5, 0 }, // TRIPLE HORIZONTAL BAR WITH TRIPLE VERTICAL STROKE
    Entry { 0x2A6A, Infix, 5, 5, 0 }, // TILDE OPERATOR WITH DOT ABOVE
    Entry { 0x2A6B, Infix, 5, 5, 0 }, // TILDE OPERATOR WITH RISING DOTS
    Entry { 0x2A6C, Infix, 5, 5, 0 }, // SIMILAR MINUS SIMILAR
    Entry { 0x2A6D, Infix, 5, 5, 0 }, // CONGRUENT WITH DOT ABOVE
    Entry { 0x2A6E, Infix, 5, 5, 0 }, // EQUALS WITH ASTERISK
    Entry { 0x2A6F, Infix, 5, 5, 0 }, // ALMOST EQUAL TO WITH CIRCUMFLEX ACCENT
    Entry { 0x2A70, Infix, 5, 5, 0 }, // APPROXIMATELY EQUAL OR EQUAL TO
    Entry { 0x2A71, Infix, 4, 4, 0 }, // EQUALS SIGN ABOVE PLUS SIGN
    Entry { 0x2A72, Infix, 4, 4, 0 }, // PLUS SIGN ABOVE EQUALS SIGN
    Entry { 0x2A73, Infix, 5, 5, 0 }, // EQUALS SIGN ABOVE TILDE OPERATOR
    Entry { 0x2A74, Infix, 5, 5, 0 }, // DOUBLE COLON EQUAL
    Entry { 0x2A75, Infix, 5, 5, 0 }, // TWO CONSECUTIVE EQUALS SIGNS
    Entry { 0x2A76, Infix, 5, 5, 0 }, // THREE CONSECUTIVE EQUALS SIGNS
    Entry { 0x2A77, Infix, 5, 5, 0 }, // EQUALS SIGN WITH TWO DOTS ABOVE AND TWO DOTS BELOW
    Entry { 0x2A78, Infix, 5, 5, 0 }, // EQUIVALENT WITH FOUR DOTS ABOVE
    Entry { 0x2A79, Infix, 5, 5, 0 }, // LESS-THAN WITH CIRCLE INSIDE
    Entry { 0x2A7A, Infix, 5, 5, 0 }, // GREATER-THAN WITH CIRCLE INSIDE
    Entry { 0x2A7B, Infix, 5, 5, 0 }, // LESS-THAN WITH QUESTION MARK ABOVE
    Entry { 0x2A7C, Infix, 5, 5, 0 }, // GREATER-THAN WITH QUESTION MARK ABOVE
    Entry { 0x2A7D, Infix, 5, 5, 0 }, // LESS-THAN OR SLANTED EQUAL TO
    Entry { 0x2A7E, Infix, 5, 5, 0 }, // GREATER-THAN OR SLANTED EQUAL TO
    Entry { 0x2A7F, Infix, 5, 5, 0 }, // LESS-THAN OR SLANTED EQUAL TO WITH DOT INSIDE
    Entry { 0x2A80, Infix, 5, 5, 0 }, // GREATER-THAN OR SLANTED EQUAL TO WITH DOT INSIDE
    Entry { 0x2A81, Infix, 5, 5, 0 }, // LESS-THAN OR SLANTED EQUAL TO WITH DOT ABOVE
    Entry { 0x2A82, Infix, 5, 5, 0 }, // GREATER-THAN OR SLANTED EQUAL TO WITH DOT ABOVE
    Entry { 0x2A83, Infix, 5, 5, 0 }, // LESS-THAN OR SLANTED EQUAL TO WITH DOT ABOVE RIGHT
    Entry { 0x2A84, Infix, 5, 5, 0 }, // GREATER-THAN OR SLANTED EQUAL TO WITH DOT ABOVE LEFT
    Entry { 0x2A85, Infix, 5, 5, 0 }, // LESS-THAN OR APPROXIMATE
    Entry { 0x2A86, Infix, 5, 5, 0 }, // GREATER-THAN OR APPROXIMATE
    Entry { 0x2A87, Infix, 5, 5, 0 }, // LESS-THAN AND SINGLE-LINE NOT EQUAL TO
    Entry { 0x2A88, Infix, 5, 5, 0 }, // GREATER-THAN AND SINGLE-LINE NOT EQUAL TO
    Entry { 0x2A89, Infix, 5, 5, 0 }, // LESS-THAN AND NOT APPROXIMATE
    Entry { 0x2A8A, Infix, 5, 5, 0 }, // GREATER-THAN AND NOT APPROXIMATE
    Entry { 0x2A8B, Infix, 5, 5, 0 }, // LESS-THAN ABOVE DOUBLE-LINE EQUAL ABOVE GREATER-THAN
    Entry { 0x2A8C, Infix, 5, 5, 0 }, // GREATER-THAN ABOVE DOUBLE-LINE EQUAL ABOVE LESS-THAN
    Entry { 0x2A8D, Infix, 5, 5, 0 }, // LESS-THAN ABOVE SIMILAR OR EQUAL
    Entry { 0x2A8E, Infix, 5, 5, 0 }, // GREATER-THAN ABOVE SIMILAR OR EQUAL
    Entry { 0x2A8F, Infix, 5, 5, 0 }, // LESS-THAN ABOVE SIMILAR ABOVE GREATER-THAN
    Entry { 0x2A90, Infix, 5, 5, 0 }, // GREATER-THAN ABOVE SIMILAR ABOVE LESS-THAN
    Entry { 0x2A91, Infix, 5, 5, 0 }, // LESS-THAN ABOVE GREATER-THAN ABOVE DOUBLE-LINE EQUAL
    Entry { 0x2A92, Infix, 5, 5, 0 }, // GREATER-THAN ABOVE LESS-THAN ABOVE DOUBLE-LINE EQUAL
    Entry { 0x2A93, Infix, 5, 5, 0 }, // LESS-THAN ABOVE SLANTED EQUAL ABOVE GREATER-THAN ABOVE SLANTED EQUAL
    Entry { 0x2A94, Infix, 5, 5, 0 }, // GREATER-THAN ABOVE SLANTED EQUAL ABOVE LESS-THAN ABOVE SLANTED EQUAL
    Entry { 0x2A95, Infix, 5, 5, 0 }, // SLANTED EQUAL TO OR LESS-THAN
    Entry { 0x2A96, Infix, 5, 5, 0 }, // SLANTED EQUAL TO OR GREATER-THAN
    Entry { 0x2A97, Infix, 5, 5, 0 }, // SLANTED EQUAL TO OR LESS-THAN WITH DOT INSIDE
    Entry { 0x2A98, Infix, 5, 5, 0 }, // SLANTED EQUAL TO OR GREATER-THAN WITH DOT INSIDE
    Entry { 0x2A99, Infix, 5, 5, 0 }, // DOUBLE-LINE EQUAL TO OR LESS-THAN
    Entry { 0x2A9A, Infix, 5, 5, 0 }, // DOUBLE-LINE EQUAL TO OR GREATER-THAN
    Entry { 0x2A9B, Infix, 5, 5, 0 }, // DOUBLE-LINE SLANTED EQUAL TO OR LESS-THAN
    Entry { 0x2A9C, Infix, 5, 5, 0 }, // DOUBLE-LINE SLANTED EQUAL TO OR GREATER-THAN
    Entry { 0x2A9D, Infix, 5, 5, 0 }, // SIMILAR OR LESS-THAN
    Entry { 0x2A9E, Infix, 5, 5, 0 }, // SIMILAR OR GREATER-THAN
    Entry { 0x2A9F, Infix, 5, 5, 0 }, // SIMILAR ABOVE LESS-THAN ABOVE EQUALS SIGN
    Entry { 0x2AA0, Infix, 5, 5, 0 }, // SIMILAR ABOVE GREATER-THAN ABOVE EQUALS SIGN
    Entry { 0x2AA1, Infix, 5, 5, 0 }, // DOUBLE NESTED LESS-THAN
    Entry { 0x2AA2, Infix, 5, 5, 0 }, // DOUBLE NESTED GREATER-THAN
    Entry { 0x2AA3, Infix, 5, 5, 0 }, // DOUBLE NESTED LESS-THAN WITH UNDERBAR
    Entry { 0x2AA4, Infix, 5, 5, 0 }, // GREATER-THAN OVERLAPPING LESS-THAN
    Entry { 0x2AA5, Infix, 5, 5, 0 }, // GREATER-THAN BESIDE LESS-THAN
    Entry { 0x2AA6, Infix, 5, 5, 0 }, // LESS-THAN CLOSED BY CURVE
    Entry { 0x2AA7, Infix, 5, 5, 0 }, // GREATER-THAN CLOSED BY CURVE
    Entry { 0x2AA8, Infix, 5, 5, 0 }, // LESS-THAN CLOSED BY CURVE ABOVE SLANTED EQUAL
    Entry { 0x2AA9, Infix, 5, 5, 0 }, // GREATER-THAN CLOSED BY CURVE ABOVE SLANTED EQUAL
    Entry { 0x2AAA, Infix, 5, 5, 0 }, // SMALLER THAN
    Entry { 0x2AAB, Infix, 5, 5, 0 }, // LARGER THAN
    Entry { 0x2AAC, Infix, 5, 5, 0 }, // SMALLER THAN OR EQUAL TO
    Entry { 0x2AAD, Infix, 5, 5, 0 }, // LARGER THAN OR EQUAL TO
    Entry { 0x2AAE, Infix, 5, 5, 0 }, // EQUALS SIGN WITH BUMPY ABOVE
    Entry { 0x2AAF, Infix, 5, 5, 0 }, // PRECEDES ABOVE SINGLE-LINE EQUALS SIGN
    Entry { 0x2AB0, Infix, 5, 5, 0 }, // SUCCEEDS ABOVE SINGLE-LINE EQUALS SIGN
    Entry { 0x2AB1, Infix, 5, 5, 0 }, // PRECEDES ABOVE SINGLE-LINE NOT EQUAL TO
    Entry { 0x2AB2, Infix, 5, 5, 0 }, // SUCCEEDS ABOVE SINGLE-LINE NOT EQUAL TO
    Entry { 0x2AB3, Infix, 5, 5, 0 }, // PRECEDES ABOVE EQUALS SIGN
    Entry { 0x2AB4, Infix, 5, 5, 0 }, // SUCCEEDS ABOVE EQUALS SIGN
    Entry { 0x2AB5, Infix, 5, 5, 0 }, // PRECEDES ABOVE NOT EQUAL TO
    Entry { 0x2AB6, Infix, 5, 5, 0 }, // SUCCEEDS ABOVE NOT EQUAL TO
    Entry { 0x2AB7, Infix, 5, 5, 0 }, // PRECEDES ABOVE ALMOST EQUAL TO
    Entry { 0x2AB8, Infix, 5, 5, 0 }, // SUCCEEDS ABOVE ALMOST EQUAL TO
    Entry { 0x2AB9, Infix, 5, 5, 0 }, // PRECEDES ABOVE NOT ALMOST EQUAL TO
    Entry { 0x2ABA, Infix, 5, 5, 0 }, // SUCCEEDS ABOVE NOT ALMOST EQUAL TO
    Entry { 0x2ABB, Infix, 5, 5, 0 }, // DOUBLE PRECEDES
    Entry { 0x2ABC, Infix, 5, 5, 0 }, // DOUBLE SUCCEEDS
    Entry { 0x2ABD, Infix, 5, 5, 0 }, // SUBSET WITH DOT
    Entry { 0x2ABE, Infix, 5, 5, 0 }, // SUPERSET WITH DOT
    Entry { 0x2ABF, Infix, 5, 5, 0 }, // SUBSET WITH PLUS SIGN BELOW
    Entry { 0x2AC0, Infix, 5, 5, 0 }, // SUPERSET WITH PLUS SIGN BELOW
    Entry { 0x2AC1, Infix, 5, 5, 0 }, // SUBSET WITH MULTIPLICATION SIGN BELOW
    Entry { 0x2AC2, Infix, 5, 5, 0 }, // SUPERSET WITH MULTIPLICATION SIGN BELOW
    Entry { 0x2AC3, Infix, 5, 5, 0 }, // SUBSET OF OR EQUAL TO WITH DOT ABOVE
    Entry { 0x2AC4, Infix, 5, 5, 0 }, // SUPERSET OF OR EQUAL TO WITH DOT ABOVE
    Entry { 0x2AC5, Infix, 5, 5, 0 }, // SUBSET OF ABOVE EQUALS SIGN
    Entry { 0x2AC6, Infix, 5, 5, 0 }, // SUPERSET OF ABOVE EQUALS SIGN
    Entry { 0x2AC7, Infix, 5, 5, 0 }, // SUBSET OF ABOVE TILDE OPERATOR
    Entry { 0x2AC8, Infix, 5, 5, 0 }, // SUPERSET OF ABOVE TILDE OPERATOR
    Entry { 0x2AC9, Infix, 5, 5, 0 }, // SUBSET OF ABOVE ALMOST EQUAL TO
    Entry { 0x2ACA, Infix, 5, 5, 0 }, // SUPERSET OF ABOVE ALMOST EQUAL TO
    Entry { 0x2ACB, Infix, 5, 5, 0 }, // SUBSET OF ABOVE NOT EQUAL TO
    Entry { 0x2ACC, Infix, 5, 5, 0 }, // SUPERSET OF ABOVE NOT EQUAL TO
    Entry { 0x2ACD, Infix, 5, 5, 0 }, // SQUARE LEFT OPEN BOX OPERATOR
    Entry { 0x2ACE, Infix, 5, 5, 0 }, // SQUARE RIGHT OPEN BOX OPERATOR
    Entry { 0x2ACF, Infix, 5, 5, 0 }, // CLOSED SUBSET
    Entry { 0x2AD0, Infix, 5, 5, 0 }, // CLOSED SUPERSET
    Entry { 0x2AD1, Infix, 5, 5, 0 }, // CLOSED SUBSET OR EQUAL TO
    Entry { 0x2AD2, Infix, 5, 5, 0 }, // CLOSED SUPERSET OR EQUAL TO
    Entry { 0x2AD3, Infix, 5, 5, 0 }, // SUBSET ABOVE SUPERSET
    Entry { 0x2AD4, Infix, 5, 5, 0 }, // SUPERSET ABOVE SUBSET
    Entry { 0x2AD5, Infix, 5, 5, 0 }, // SUBSET ABOVE SUBSET
    Entry { 0x2AD6, Infix, 5, 5, 0 }, // SUPERSET ABOVE SUPERSET
    Entry { 0x2AD7, Infix, 5, 5, 0 }, // SUPERSET BESIDE SUBSET
    Entry { 0x2AD8, Infix, 5, 5, 0 }, // SUPERSET BESIDE AND JOINED BY DASH WITH SUBSET
    Entry { 0x2AD9, Infix, 5, 5, 0 }, // ELEMENT OF OPENING DOWNWARDS
    Entry { 0x2ADA, Infix, 5, 5, 0 }, // PITCHFORK WITH TEE TOP
    Entry { 0x2ADB, Infix, 5, 5, 0 }, // TRANSVERSAL INTERSECTION
    Entry { 0x2ADD, Infix, 5, 5, 0 }, // NONFORKING
    Entry { 0x2ADE, Infix, 5, 5, 0 }, // SHORT LEFT TACK
    Entry { 0x2ADF, Infix, 5, 5, 0 }, // SHORT DOWN TACK
    Entry { 0x2AE0, Infix, 5, 5, 0 }, // SHORT UP TACK
    Entry { 0x2AE1, Infix, 5, 5, 0 }, // PERPENDICULAR WITH S
    Entry { 0x2AE2, Infix, 5, 5, 0 }, // VERTICAL BAR TRIPLE RIGHT TURNSTILE
    Entry { 0x2AE3, Infix, 5, 5, 0 }, // DOUBLE VERTICAL BAR LEFT TURNSTILE
    Entry { 0x2AE4, Infix, 5, 5, 0 }, // VERTICAL BAR DOUBLE LEFT TURNSTILE
    Entry { 0x2AE5, Infix, 5, 5, 0 }, // DOUBLE VERTICAL BAR DOUBLE LEFT TURNSTILE
    Entry { 0x2AE6, Infix, 5, 5, 0 }, // LONG DASH FROM LEFT MEMBER OF DOUBLE VERTICAL
    Entry { 0x2AE7, Infix, 5, 5, 0 }, // SHORT DOWN TACK WITH OVERBAR
    Entry { 0x2AE8, Infix, 5, 5, 0 }, // SHORT UP TACK WITH UNDERBAR
    Entry { 0x2AE9, Infix, 5, 5, 0 }, // SHORT UP TACK ABOVE SHORT DOWN TACK
    Entry { 0x2AEA, Infix, 5, 5, 0 }, // DOUBLE DOWN TACK
    Entry { 0x2AEB, Infix, 5, 5, 0 }, // DOUBLE UP TACK
    Entry { 0x2AEC, Infix, 5, 5, 0 }, // DOUBLE STROKE NOT SIGN
    Entry { 0x2AED, Infix, 5, 5, 0 }, // REVERSED DOUBLE STROKE NOT SIGN
    Entry { 0x2AEE, Infix, 5, 5, 0 }, // DOES NOT DIVIDE WITH REVERSED NEGATION SLASH
    Entry { 0x2AEF, Infix, 5, 5, 0 }, // VERTICAL LINE WITH CIRCLE ABOVE
    Entry { 0x2AF0, Infix, 5, 5, 0 }, // VERTICAL LINE WITH CIRCLE BELOW
    Entry { 0x2AF1, Infix, 5, 5, 0 }, // DOWN TACK WITH CIRCLE BELOW
    Entry { 0x2AF2, Infix, 5, 5, 0 }, // PARALLEL WITH HORIZONTAL STROKE
    Entry { 0x2AF3, Infix, 5, 5, 0 }, // PARALLEL WITH TILDE OPERATOR
    Entry { 0x2AF4, Infix, 4, 4, 0 }, // TRIPLE VERTICAL BAR BINARY RELATION
    Entry { 0x2AF5, Infix, 4, 4, 0 }, // TRIPLE VERTICAL BAR WITH HORIZONTAL STROKE
    Entry { 0x2AF6, Infix, 4, 4, 0 }, // TRIPLE COLON OPERATOR
    Entry { 0x2AF7, Infix, 5, 5, 0 }, // TRIPLE NESTED LESS-THAN
    Entry { 0x2AF8, Infix, 5, 5, 0 }, // TRIPLE NESTED GREATER-THAN
    Entry { 0x2AF9, Infix, 5, 5, 0 }, // DOUBLE-LINE SLANTED LESS-THAN OR EQUAL TO
    Entry { 0x2AFA, Infix, 5, 5, 0 }, // DOUBLE-LINE SLANTED GREATER-THAN OR EQUAL TO
    Entry { 0x2AFB, Infix, 4, 4, 0 }, // TRIPLE SOLIDUS BINARY RELATION
    Entry { 0x2AFC, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // LARGE TRIPLE VERTICAL BAR OPERATOR
    Entry { 0x2AFD, Infix, 4, 4, 0 }, // DOUBLE SOLIDUS OPERATOR
    Entry { 0x2AFE, Infix, 3, 3, 0 }, // WHITE VERTICAL BAR
    Entry { 0x2AFF, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits }, // N-ARY WHITE VERTICAL BAR
    Entry { 0x2B45, Infix, 5, 5, Stretchy }, // LEFTWARDS QUADRUPLE ARROW
    Entry { 0x2B46, Infix, 5, 5, Stretchy }, // RIGHTWARDS QUADRUPLE ARROW
    Entry { 0x1EEF0, Prefix, 0, 0, Stretchy }, // ARABIC MATHEMATICAL OPERATOR MEEM WITH HAH WITH TATWEEL
    Entry { 0x1EEF1, Prefix, 0, 0, Stretchy } // ARABIC MATHEMATICAL OPERATOR HAH WITH DAL
};

// A list of operators that stretch in the horizontal direction. This has been generated from Mozilla's MathML operator dictionary.
static inline char32_t ExtractKeyHorizontal(const char32_t* entry) { return *entry; }
static const char32_t horizontalOperators[] = {
    0x003D, 0x005E, 0x005F, 0x007E, 0x00AF, 0x02C6, 0x02C7, 0x02C9, 0x02CD, 0x02DC, 0x02F7, 0x0302, 0x0332, 0x203E, 0x20D0, 0x20D1, 0x20D6, 0x20D7, 0x20E1, 0x2190, 0x2192, 0x2194, 0x2198, 0x2199, 0x219C, 0x219D, 0x219E, 0x21A0, 0x21A2, 0x21A3, 0x21A4, 0x21A6, 0x21A9, 0x21AA, 0x21AB, 0x21AC, 0x21AD, 0x21B4, 0x21B9, 0x21BC, 0x21BD, 0x21C0, 0x21C1, 0x21C4, 0x21C6, 0x21C7, 0x21C9, 0x21CB, 0x21CC, 0x21D0, 0x21D2, 0x21D4, 0x21DA, 0x21DB, 0x21DC, 0x21DD, 0x21E0, 0x21E2, 0x21E4, 0x21E5, 0x21E6, 0x21E8, 0x21F0, 0x21F6, 0x21FD, 0x21FE, 0x21FF, 0x23B4, 0x23B5, 0x23DC, 0x23DD, 0x23DE, 0x23DF, 0x23E0, 0x23E1, 0x2500, 0x27F5, 0x27F6, 0x27F7, 0x27F8, 0x27F9, 0x27FA, 0x27FB, 0x27FC, 0x27FD, 0x27FE, 0x27FF, 0x290C, 0x290D, 0x290E, 0x290F, 0x2910, 0x294E, 0x2950, 0x2952, 0x2953, 0x2956, 0x2957, 0x295A, 0x295B, 0x295E, 0x295F, 0x2B45, 0x2B46, 0xFE35, 0xFE36, 0xFE37, 0xFE38, 0x1EEF0, 0x1EEF1
};

std::optional<Property> MathMLOperatorDictionary::search(char32_t character, Form form, bool explicitForm)
{
    if (!character)
        return std::nullopt;

    // We try and find the default values from the operator dictionary.
    if (auto* entry = tryBinarySearch<const Entry, Key>(dictionary, dictionarySize, Key(character, form), ExtractKey))
        return ExtractProperty(*entry);

    if (explicitForm)
        return std::nullopt;

    // If we did not find the desired operator form and if it was not set explicitly, we use the first one in the following order: Infix, Prefix, Postfix.
    // This is to handle bad MathML markup without explicit <mrow> delimiters like "<mo>(</mo><mi>a</mi><mo>)</mo><mo>(</mo><mi>b</mi><mo>)</mo>" where innerfences should not be considered infix.
    if (auto* entry = tryBinarySearch<const Entry, char32_t>(dictionary, dictionarySize, character, ExtractChar)) {
        size_t index = entry - dictionary.data();
        // There are at most two other entries before the one found.
        if (index && dictionary[index - 1].character == character)
            --index;
        if (index && dictionary[index - 1].character == character)
            --index;
        return ExtractProperty(dictionary[index]);
    }

    return std::nullopt;
}

bool MathMLOperatorDictionary::isVertical(char32_t textContent)
{
    return !tryBinarySearch<const char32_t, char32_t>(horizontalOperators, std::size(horizontalOperators), textContent, ExtractKeyHorizontal);
}

}

#endif // ENABLE(MATHML)
