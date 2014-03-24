/*
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2010 François Sausset (sausset@gmail.com). All rights reserved.
 * Copyright (C) 2013 Igalia S.L.
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

#if ENABLE(MATHML)

#include "RenderMathMLOperator.h"

#include "FontCache.h"
#include "FontSelector.h"
#include "MathMLNames.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderText.h"
#include "ScaleTransformOperation.h"
#include "TransformOperations.h"
#include <wtf/MathExtras.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {
    
using namespace MathMLNames;

// FIXME: The OpenType MATH table contains information that should override this table (http://wkbug/122297).
struct StretchyCharacter {
    UChar character;
    UChar topChar;
    UChar extensionChar;
    UChar bottomChar;
    UChar middleChar;
};
static const StretchyCharacter stretchyCharacters[14] = {
    { 0x28  , 0x239b, 0x239c, 0x239d, 0x0    }, // left parenthesis
    { 0x29  , 0x239e, 0x239f, 0x23a0, 0x0    }, // right parenthesis
    { 0x5b  , 0x23a1, 0x23a2, 0x23a3, 0x0    }, // left square bracket
    { 0x2308, 0x23a1, 0x23a2, 0x23a2, 0x0    }, // left ceiling
    { 0x230a, 0x23a2, 0x23a2, 0x23a3, 0x0    }, // left floor
    { 0x5d  , 0x23a4, 0x23a5, 0x23a6, 0x0    }, // right square bracket
    { 0x2309, 0x23a4, 0x23a5, 0x23a5, 0x0    }, // right ceiling
    { 0x230b, 0x23a5, 0x23a5, 0x23a6, 0x0    }, // right floor
    { 0x7b  , 0x23a7, 0x23aa, 0x23a9, 0x23a8 }, // left curly bracket
    { 0x7c  , 0x7c,   0x7c,   0x7c,   0x0    }, // vertical bar
    { 0x2016, 0x2016, 0x2016, 0x2016, 0x0    }, // double vertical line
    { 0x2225, 0x2225, 0x2225, 0x2225, 0x0    }, // parallel to
    { 0x7d  , 0x23ab, 0x23aa, 0x23ad, 0x23ac }, // right curly bracket
    { 0x222b, 0x2320, 0x23ae, 0x2321, 0x0    } // integral sign
};

namespace MathMLOperatorDictionary {

typedef std::pair<UChar, Form> Key;
inline Key ExtractKey(const Entry* entry) { return Key(entry->character, entry->form); }
inline UChar ExtractChar(const Entry* entry) { return entry->character; }

// This table has been automatically generated from the Operator Dictionary of the MathML3 specification (appendix C).
// Some people use the binary operator "U+2225 PARALLEL TO" as an opening and closing delimiter, so we add the corresponding stretchy prefix and postfix forms.
#define MATHML_OPDICT_SIZE 1041
static const Entry dictionary[MATHML_OPDICT_SIZE] = {
    { 0x21, Postfix, 1, 0, 0}, // EXCLAMATION MARK
    { 0x25, Infix, 3, 3, 0}, // PERCENT SIGN
    { 0x26, Postfix, 0, 0, 0}, // AMPERSAND
    { 0x27, Postfix, 0, 0, Accent}, // APOSTROPHE
    { 0x28, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // LEFT PARENTHESIS
    { 0x29, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // RIGHT PARENTHESIS
    { 0x2A, Infix, 3, 3, 0}, // ASTERISK
    { 0x2B, Infix, 4, 4, 0}, // PLUS SIGN
    { 0x2B, Prefix, 0, 1, 0}, // PLUS SIGN
    { 0x2C, Infix, 0, 3, Separator}, // COMMA
    { 0x2D, Infix, 4, 4, 0}, // HYPHEN-MINUS
    { 0x2D, Prefix, 0, 1, 0}, // HYPHEN-MINUS
    { 0x2E, Infix, 3, 3, 0}, // FULL STOP
    { 0x2F, Infix, 1, 1, 0}, // SOLIDUS
    { 0x3A, Infix, 1, 2, 0}, // COLON
    { 0x3B, Infix, 0, 3, Separator}, // SEMICOLON
    { 0x3C, Infix, 5, 5, 0}, // LESS-THAN SIGN
    { 0x3D, Infix, 5, 5, 0}, // EQUALS SIGN
    { 0x3E, Infix, 5, 5, 0}, // GREATER-THAN SIGN
    { 0x3F, Infix, 1, 1, 0}, // QUESTION MARK
    { 0x40, Infix, 1, 1, 0}, // COMMERCIAL AT
    { 0x5B, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // LEFT SQUARE BRACKET
    { 0x5C, Infix, 0, 0, 0}, // REVERSE SOLIDUS
    { 0x5D, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // RIGHT SQUARE BRACKET
    { 0x5E, Postfix, 0, 0, Accent | Stretchy}, // CIRCUMFLEX ACCENT
    { 0x5E, Infix, 1, 1, 0}, // CIRCUMFLEX ACCENT
    { 0x5F, Postfix, 0, 0, Accent | Stretchy}, // LOW LINE
    { 0x5F, Infix, 1, 1, 0}, // LOW LINE
    { 0x60, Postfix, 0, 0, Accent}, // GRAVE ACCENT
    { 0x7B, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // LEFT CURLY BRACKET
    { 0x7C, Infix, 2, 2, Stretchy | Symmetric | Fence}, // VERTICAL LINE
    { 0x7C, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // VERTICAL LINE
    { 0x7C, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // VERTICAL LINE
    { 0x7D, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // RIGHT CURLY BRACKET
    { 0x7E, Postfix, 0, 0, Accent | Stretchy}, // TILDE
    { 0xA8, Postfix, 0, 0, Accent}, // DIAERESIS
    { 0xAC, Prefix, 2, 1, 0}, // NOT SIGN
    { 0xAF, Postfix, 0, 0, Accent | Stretchy}, // MACRON
    { 0xB0, Postfix, 0, 0, 0}, // DEGREE SIGN
    { 0xB1, Infix, 4, 4, 0}, // PLUS-MINUS SIGN
    { 0xB1, Prefix, 0, 1, 0}, // PLUS-MINUS SIGN
    { 0xB4, Postfix, 0, 0, Accent}, // ACUTE ACCENT
    { 0xB7, Infix, 4, 4, 0}, // MIDDLE DOT
    { 0xB8, Postfix, 0, 0, Accent}, // CEDILLA
    { 0xD7, Infix, 4, 4, 0}, // MULTIPLICATION SIGN
    { 0xF7, Infix, 4, 4, 0}, // DIVISION SIGN
    { 0x2C6, Postfix, 0, 0, Accent | Stretchy}, // MODIFIER LETTER CIRCUMFLEX ACCENT
    { 0x2C7, Postfix, 0, 0, Accent | Stretchy}, // CARON
    { 0x2C9, Postfix, 0, 0, Accent | Stretchy}, // MODIFIER LETTER MACRON
    { 0x2CA, Postfix, 0, 0, Accent}, // MODIFIER LETTER ACUTE ACCENT
    { 0x2CB, Postfix, 0, 0, Accent}, // MODIFIER LETTER GRAVE ACCENT
    { 0x2CD, Postfix, 0, 0, Accent | Stretchy}, // MODIFIER LETTER LOW MACRON
    { 0x2D8, Postfix, 0, 0, Accent}, // BREVE
    { 0x2D9, Postfix, 0, 0, Accent}, // DOT ABOVE
    { 0x2DA, Postfix, 0, 0, Accent}, // RING ABOVE
    { 0x2DC, Postfix, 0, 0, Accent | Stretchy}, // SMALL TILDE
    { 0x2DD, Postfix, 0, 0, Accent}, // DOUBLE ACUTE ACCENT
    { 0x2F7, Postfix, 0, 0, Accent | Stretchy}, // MODIFIER LETTER LOW TILDE
    { 0x302, Postfix, 0, 0, Accent | Stretchy}, // COMBINING CIRCUMFLEX ACCENT
    { 0x311, Postfix, 0, 0, Accent}, // COMBINING INVERTED BREVE
    { 0x3F6, Infix, 5, 5, 0}, // GREEK REVERSED LUNATE EPSILON SYMBOL
    { 0x2016, Prefix, 0, 0, Fence | Stretchy}, // DOUBLE VERTICAL LINE
    { 0x2016, Postfix, 0, 0, Fence | Stretchy}, // DOUBLE VERTICAL LINE
    { 0x2018, Prefix, 0, 0, Fence}, // LEFT SINGLE QUOTATION MARK
    { 0x2019, Postfix, 0, 0, Fence}, // RIGHT SINGLE QUOTATION MARK
    { 0x201C, Prefix, 0, 0, Fence}, // LEFT DOUBLE QUOTATION MARK
    { 0x201D, Postfix, 0, 0, Fence}, // RIGHT DOUBLE QUOTATION MARK
    { 0x2022, Infix, 4, 4, 0}, // BULLET
    { 0x2026, Infix, 0, 0, 0}, // HORIZONTAL ELLIPSIS
    { 0x2032, Postfix, 0, 2, 0}, // PRIME
    { 0x203E, Postfix, 0, 0, Accent | Stretchy}, // OVERLINE
    { 0x2044, Infix, 4, 4, Stretchy}, // FRACTION SLASH
    { 0x2061, Infix, 0, 0, 0}, // FUNCTION APPLICATION
    { 0x2062, Infix, 0, 0, 0}, // INVISIBLE TIMES
    { 0x2063, Infix, 0, 0, Separator}, // INVISIBLE SEPARATOR
    { 0x2064, Infix, 0, 0, 0}, // INVISIBLE PLUS
    { 0x20DB, Postfix, 0, 0, Accent}, // COMBINING THREE DOTS ABOVE
    { 0x20DC, Postfix, 0, 0, Accent}, // COMBINING FOUR DOTS ABOVE
    { 0x2145, Prefix, 2, 1, 0}, // DOUBLE-STRUCK ITALIC CAPITAL D
    { 0x2146, Prefix, 2, 0, 0}, // DOUBLE-STRUCK ITALIC SMALL D
    { 0x2190, Infix, 5, 5, Accent | Stretchy}, // LEFTWARDS ARROW
    { 0x2191, Infix, 5, 5, Stretchy}, // UPWARDS ARROW
    { 0x2192, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS ARROW
    { 0x2193, Infix, 5, 5, Stretchy}, // DOWNWARDS ARROW
    { 0x2194, Infix, 5, 5, Stretchy | Accent}, // LEFT RIGHT ARROW
    { 0x2195, Infix, 5, 5, Stretchy}, // UP DOWN ARROW
    { 0x2196, Infix, 5, 5, Stretchy}, // NORTH WEST ARROW
    { 0x2197, Infix, 5, 5, Stretchy}, // NORTH EAST ARROW
    { 0x2198, Infix, 5, 5, Stretchy}, // SOUTH EAST ARROW
    { 0x2199, Infix, 5, 5, Stretchy}, // SOUTH WEST ARROW
    { 0x219A, Infix, 5, 5, Accent}, // LEFTWARDS ARROW WITH STROKE
    { 0x219B, Infix, 5, 5, Accent}, // RIGHTWARDS ARROW WITH STROKE
    { 0x219C, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS WAVE ARROW
    { 0x219D, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS WAVE ARROW
    { 0x219E, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS TWO HEADED ARROW
    { 0x219F, Infix, 5, 5, Stretchy | Accent}, // UPWARDS TWO HEADED ARROW
    { 0x21A0, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS TWO HEADED ARROW
    { 0x21A1, Infix, 5, 5, Stretchy}, // DOWNWARDS TWO HEADED ARROW
    { 0x21A2, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS ARROW WITH TAIL
    { 0x21A3, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS ARROW WITH TAIL
    { 0x21A4, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS ARROW FROM BAR
    { 0x21A5, Infix, 5, 5, Stretchy}, // UPWARDS ARROW FROM BAR
    { 0x21A6, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS ARROW FROM BAR
    { 0x21A7, Infix, 5, 5, Stretchy}, // DOWNWARDS ARROW FROM BAR
    { 0x21A8, Infix, 5, 5, Stretchy}, // UP DOWN ARROW WITH BASE
    { 0x21A9, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS ARROW WITH HOOK
    { 0x21AA, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS ARROW WITH HOOK
    { 0x21AB, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS ARROW WITH LOOP
    { 0x21AC, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS ARROW WITH LOOP
    { 0x21AD, Infix, 5, 5, Stretchy | Accent}, // LEFT RIGHT WAVE ARROW
    { 0x21AE, Infix, 5, 5, Accent}, // LEFT RIGHT ARROW WITH STROKE
    { 0x21AF, Infix, 5, 5, Stretchy}, // DOWNWARDS ZIGZAG ARROW
    { 0x21B0, Infix, 5, 5, Stretchy}, // UPWARDS ARROW WITH TIP LEFTWARDS
    { 0x21B1, Infix, 5, 5, Stretchy}, // UPWARDS ARROW WITH TIP RIGHTWARDS
    { 0x21B2, Infix, 5, 5, Stretchy}, // DOWNWARDS ARROW WITH TIP LEFTWARDS
    { 0x21B3, Infix, 5, 5, Stretchy}, // DOWNWARDS ARROW WITH TIP RIGHTWARDS
    { 0x21B4, Infix, 5, 5, Stretchy}, // RIGHTWARDS ARROW WITH CORNER DOWNWARDS
    { 0x21B5, Infix, 5, 5, Stretchy}, // DOWNWARDS ARROW WITH CORNER LEFTWARDS
    { 0x21B6, Infix, 5, 5, Accent}, // ANTICLOCKWISE TOP SEMICIRCLE ARROW
    { 0x21B7, Infix, 5, 5, Accent}, // CLOCKWISE TOP SEMICIRCLE ARROW
    { 0x21B8, Infix, 5, 5, 0}, // NORTH WEST ARROW TO LONG BAR
    { 0x21B9, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS ARROW TO BAR OVER RIGHTWARDS ARROW TO BAR
    { 0x21BA, Infix, 5, 5, 0}, // ANTICLOCKWISE OPEN CIRCLE ARROW
    { 0x21BB, Infix, 5, 5, 0}, // CLOCKWISE OPEN CIRCLE ARROW
    { 0x21BC, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS HARPOON WITH BARB UPWARDS
    { 0x21BD, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS HARPOON WITH BARB DOWNWARDS
    { 0x21BE, Infix, 5, 5, Stretchy}, // UPWARDS HARPOON WITH BARB RIGHTWARDS
    { 0x21BF, Infix, 5, 5, Stretchy}, // UPWARDS HARPOON WITH BARB LEFTWARDS
    { 0x21C0, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS HARPOON WITH BARB UPWARDS
    { 0x21C1, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS HARPOON WITH BARB DOWNWARDS
    { 0x21C2, Infix, 5, 5, Stretchy}, // DOWNWARDS HARPOON WITH BARB RIGHTWARDS
    { 0x21C3, Infix, 5, 5, Stretchy}, // DOWNWARDS HARPOON WITH BARB LEFTWARDS
    { 0x21C4, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS ARROW OVER LEFTWARDS ARROW
    { 0x21C5, Infix, 5, 5, Stretchy}, // UPWARDS ARROW LEFTWARDS OF DOWNWARDS ARROW
    { 0x21C6, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS ARROW OVER RIGHTWARDS ARROW
    { 0x21C7, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS PAIRED ARROWS
    { 0x21C8, Infix, 5, 5, Stretchy}, // UPWARDS PAIRED ARROWS
    { 0x21C9, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS PAIRED ARROWS
    { 0x21CA, Infix, 5, 5, Stretchy}, // DOWNWARDS PAIRED ARROWS
    { 0x21CB, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS HARPOON OVER RIGHTWARDS HARPOON
    { 0x21CC, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS HARPOON OVER LEFTWARDS HARPOON
    { 0x21CD, Infix, 5, 5, Accent}, // LEFTWARDS DOUBLE ARROW WITH STROKE
    { 0x21CE, Infix, 5, 5, Accent}, // LEFT RIGHT DOUBLE ARROW WITH STROKE
    { 0x21CF, Infix, 5, 5, Accent}, // RIGHTWARDS DOUBLE ARROW WITH STROKE
    { 0x21D0, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS DOUBLE ARROW
    { 0x21D1, Infix, 5, 5, Stretchy}, // UPWARDS DOUBLE ARROW
    { 0x21D2, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS DOUBLE ARROW
    { 0x21D3, Infix, 5, 5, Stretchy}, // DOWNWARDS DOUBLE ARROW
    { 0x21D4, Infix, 5, 5, Stretchy | Accent}, // LEFT RIGHT DOUBLE ARROW
    { 0x21D5, Infix, 5, 5, Stretchy}, // UP DOWN DOUBLE ARROW
    { 0x21D6, Infix, 5, 5, Stretchy}, // NORTH WEST DOUBLE ARROW
    { 0x21D7, Infix, 5, 5, Stretchy}, // NORTH EAST DOUBLE ARROW
    { 0x21D8, Infix, 5, 5, Stretchy}, // SOUTH EAST DOUBLE ARROW
    { 0x21D9, Infix, 5, 5, Stretchy}, // SOUTH WEST DOUBLE ARROW
    { 0x21DA, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS TRIPLE ARROW
    { 0x21DB, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS TRIPLE ARROW
    { 0x21DC, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS SQUIGGLE ARROW
    { 0x21DD, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS SQUIGGLE ARROW
    { 0x21DE, Infix, 5, 5, 0}, // UPWARDS ARROW WITH DOUBLE STROKE
    { 0x21DF, Infix, 5, 5, 0}, // DOWNWARDS ARROW WITH DOUBLE STROKE
    { 0x21E0, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS DASHED ARROW
    { 0x21E1, Infix, 5, 5, Stretchy}, // UPWARDS DASHED ARROW
    { 0x21E2, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS DASHED ARROW
    { 0x21E3, Infix, 5, 5, Stretchy}, // DOWNWARDS DASHED ARROW
    { 0x21E4, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS ARROW TO BAR
    { 0x21E5, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS ARROW TO BAR
    { 0x21E6, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS WHITE ARROW
    { 0x21E7, Infix, 5, 5, Stretchy}, // UPWARDS WHITE ARROW
    { 0x21E8, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS WHITE ARROW
    { 0x21E9, Infix, 5, 5, Stretchy}, // DOWNWARDS WHITE ARROW
    { 0x21EA, Infix, 5, 5, Stretchy}, // UPWARDS WHITE ARROW FROM BAR
    { 0x21EB, Infix, 5, 5, Stretchy}, // UPWARDS WHITE ARROW ON PEDESTAL
    { 0x21EC, Infix, 5, 5, Stretchy}, // UPWARDS WHITE ARROW ON PEDESTAL WITH HORIZONTAL BAR
    { 0x21ED, Infix, 5, 5, Stretchy}, // UPWARDS WHITE ARROW ON PEDESTAL WITH VERTICAL BAR
    { 0x21EE, Infix, 5, 5, Stretchy}, // UPWARDS WHITE DOUBLE ARROW
    { 0x21EF, Infix, 5, 5, Stretchy}, // UPWARDS WHITE DOUBLE ARROW ON PEDESTAL
    { 0x21F0, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS WHITE ARROW FROM WALL
    { 0x21F1, Infix, 5, 5, 0}, // NORTH WEST ARROW TO CORNER
    { 0x21F2, Infix, 5, 5, 0}, // SOUTH EAST ARROW TO CORNER
    { 0x21F3, Infix, 5, 5, Stretchy}, // UP DOWN WHITE ARROW
    { 0x21F4, Infix, 5, 5, Accent}, // RIGHT ARROW WITH SMALL CIRCLE
    { 0x21F5, Infix, 5, 5, Stretchy}, // DOWNWARDS ARROW LEFTWARDS OF UPWARDS ARROW
    { 0x21F6, Infix, 5, 5, Stretchy | Accent}, // THREE RIGHTWARDS ARROWS
    { 0x21F7, Infix, 5, 5, Accent}, // LEFTWARDS ARROW WITH VERTICAL STROKE
    { 0x21F8, Infix, 5, 5, Accent}, // RIGHTWARDS ARROW WITH VERTICAL STROKE
    { 0x21F9, Infix, 5, 5, Accent}, // LEFT RIGHT ARROW WITH VERTICAL STROKE
    { 0x21FA, Infix, 5, 5, Accent}, // LEFTWARDS ARROW WITH DOUBLE VERTICAL STROKE
    { 0x21FB, Infix, 5, 5, Accent}, // RIGHTWARDS ARROW WITH DOUBLE VERTICAL STROKE
    { 0x21FC, Infix, 5, 5, Accent}, // LEFT RIGHT ARROW WITH DOUBLE VERTICAL STROKE
    { 0x21FD, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS OPEN-HEADED ARROW
    { 0x21FE, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS OPEN-HEADED ARROW
    { 0x21FF, Infix, 5, 5, Stretchy | Accent}, // LEFT RIGHT OPEN-HEADED ARROW
    { 0x2200, Prefix, 2, 1, 0}, // FOR ALL
    { 0x2201, Infix, 1, 2, 0}, // COMPLEMENT
    { 0x2202, Prefix, 2, 1, 0}, // PARTIAL DIFFERENTIAL
    { 0x2203, Prefix, 2, 1, 0}, // THERE EXISTS
    { 0x2204, Prefix, 2, 1, 0}, // THERE DOES NOT EXIST
    { 0x2206, Infix, 3, 3, 0}, // INCREMENT
    { 0x2207, Prefix, 2, 1, 0}, // NABLA
    { 0x2208, Infix, 5, 5, 0}, // ELEMENT OF
    { 0x2209, Infix, 5, 5, 0}, // NOT AN ELEMENT OF
    { 0x220A, Infix, 5, 5, 0}, // SMALL ELEMENT OF
    { 0x220B, Infix, 5, 5, 0}, // CONTAINS AS MEMBER
    { 0x220C, Infix, 5, 5, 0}, // DOES NOT CONTAIN AS MEMBER
    { 0x220D, Infix, 5, 5, 0}, // SMALL CONTAINS AS MEMBER
    { 0x220E, Infix, 3, 3, 0}, // END OF PROOF
    { 0x220F, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // N-ARY PRODUCT
    { 0x2210, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // N-ARY COPRODUCT
    { 0x2211, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // N-ARY SUMMATION
    { 0x2212, Infix, 4, 4, 0}, // MINUS SIGN
    { 0x2212, Prefix, 0, 1, 0}, // MINUS SIGN
    { 0x2213, Infix, 4, 4, 0}, // MINUS-OR-PLUS SIGN
    { 0x2213, Prefix, 0, 1, 0}, // MINUS-OR-PLUS SIGN
    { 0x2214, Infix, 4, 4, 0}, // DOT PLUS
    { 0x2215, Infix, 4, 4, Stretchy}, // DIVISION SLASH
    { 0x2216, Infix, 4, 4, 0}, // SET MINUS
    { 0x2217, Infix, 4, 4, 0}, // ASTERISK OPERATOR
    { 0x2218, Infix, 4, 4, 0}, // RING OPERATOR
    { 0x2219, Infix, 4, 4, 0}, // BULLET OPERATOR
    { 0x221A, Prefix, 1, 1, Stretchy}, // SQUARE ROOT
    { 0x221B, Prefix, 1, 1, 0}, // CUBE ROOT
    { 0x221C, Prefix, 1, 1, 0}, // FOURTH ROOT
    { 0x221D, Infix, 5, 5, 0}, // PROPORTIONAL TO
    { 0x221F, Infix, 5, 5, 0}, // RIGHT ANGLE
    { 0x2220, Prefix, 0, 0, 0}, // ANGLE
    { 0x2221, Prefix, 0, 0, 0}, // MEASURED ANGLE
    { 0x2222, Prefix, 0, 0, 0}, // SPHERICAL ANGLE
    { 0x2223, Infix, 5, 5, 0}, // DIVIDES
    { 0x2224, Infix, 5, 5, 0}, // DOES NOT DIVIDE
    { 0x2225, Infix, 5, 5, 0}, // PARALLEL TO
    { 0x2225, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // PARALLEL TO
    { 0x2225, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // PARALLEL TO
    { 0x2226, Infix, 5, 5, 0}, // NOT PARALLEL TO
    { 0x2227, Infix, 4, 4, 0}, // LOGICAL AND
    { 0x2228, Infix, 4, 4, 0}, // LOGICAL OR
    { 0x2229, Infix, 4, 4, 0}, // INTERSECTION
    { 0x222A, Infix, 4, 4, 0}, // UNION
    { 0x222B, Prefix, 0, 1, Symmetric | LargeOp}, // INTEGRAL
    { 0x222C, Prefix, 0, 1, Symmetric | LargeOp}, // DOUBLE INTEGRAL
    { 0x222D, Prefix, 0, 1, Symmetric | LargeOp}, // TRIPLE INTEGRAL
    { 0x222E, Prefix, 0, 1, Symmetric | LargeOp}, // CONTOUR INTEGRAL
    { 0x222F, Prefix, 0, 1, Symmetric | LargeOp}, // SURFACE INTEGRAL
    { 0x2230, Prefix, 0, 1, Symmetric | LargeOp}, // VOLUME INTEGRAL
    { 0x2231, Prefix, 0, 1, Symmetric | LargeOp}, // CLOCKWISE INTEGRAL
    { 0x2232, Prefix, 0, 1, Symmetric | LargeOp}, // CLOCKWISE CONTOUR INTEGRAL
    { 0x2233, Prefix, 0, 1, Symmetric | LargeOp}, // ANTICLOCKWISE CONTOUR INTEGRAL
    { 0x2234, Infix, 5, 5, 0}, // THEREFORE
    { 0x2235, Infix, 5, 5, 0}, // BECAUSE
    { 0x2236, Infix, 5, 5, 0}, // RATIO
    { 0x2237, Infix, 5, 5, 0}, // PROPORTION
    { 0x2238, Infix, 4, 4, 0}, // DOT MINUS
    { 0x2239, Infix, 5, 5, 0}, // EXCESS
    { 0x223A, Infix, 4, 4, 0}, // GEOMETRIC PROPORTION
    { 0x223B, Infix, 5, 5, 0}, // HOMOTHETIC
    { 0x223C, Infix, 5, 5, 0}, // TILDE OPERATOR
    { 0x223D, Infix, 5, 5, 0}, // REVERSED TILDE
    { 0x223E, Infix, 5, 5, 0}, // INVERTED LAZY S
    { 0x223F, Infix, 3, 3, 0}, // SINE WAVE
    { 0x2240, Infix, 4, 4, 0}, // WREATH PRODUCT
    { 0x2241, Infix, 5, 5, 0}, // NOT TILDE
    { 0x2242, Infix, 5, 5, 0}, // MINUS TILDE
    { 0x2243, Infix, 5, 5, 0}, // ASYMPTOTICALLY EQUAL TO
    { 0x2244, Infix, 5, 5, 0}, // NOT ASYMPTOTICALLY EQUAL TO
    { 0x2245, Infix, 5, 5, 0}, // APPROXIMATELY EQUAL TO
    { 0x2246, Infix, 5, 5, 0}, // APPROXIMATELY BUT NOT ACTUALLY EQUAL TO
    { 0x2247, Infix, 5, 5, 0}, // NEITHER APPROXIMATELY NOR ACTUALLY EQUAL TO
    { 0x2248, Infix, 5, 5, 0}, // ALMOST EQUAL TO
    { 0x2249, Infix, 5, 5, 0}, // NOT ALMOST EQUAL TO
    { 0x224A, Infix, 5, 5, 0}, // ALMOST EQUAL OR EQUAL TO
    { 0x224B, Infix, 5, 5, 0}, // TRIPLE TILDE
    { 0x224C, Infix, 5, 5, 0}, // ALL EQUAL TO
    { 0x224D, Infix, 5, 5, 0}, // EQUIVALENT TO
    { 0x224E, Infix, 5, 5, 0}, // GEOMETRICALLY EQUIVALENT TO
    { 0x224F, Infix, 5, 5, 0}, // DIFFERENCE BETWEEN
    { 0x2250, Infix, 5, 5, 0}, // APPROACHES THE LIMIT
    { 0x2251, Infix, 5, 5, 0}, // GEOMETRICALLY EQUAL TO
    { 0x2252, Infix, 5, 5, 0}, // APPROXIMATELY EQUAL TO OR THE IMAGE OF
    { 0x2253, Infix, 5, 5, 0}, // IMAGE OF OR APPROXIMATELY EQUAL TO
    { 0x2254, Infix, 5, 5, 0}, // COLON EQUALS
    { 0x2255, Infix, 5, 5, 0}, // EQUALS COLON
    { 0x2256, Infix, 5, 5, 0}, // RING IN EQUAL TO
    { 0x2257, Infix, 5, 5, 0}, // RING EQUAL TO
    { 0x2258, Infix, 5, 5, 0}, // CORRESPONDS TO
    { 0x2259, Infix, 5, 5, 0}, // ESTIMATES
    { 0x225A, Infix, 5, 5, 0}, // EQUIANGULAR TO
    { 0x225C, Infix, 5, 5, 0}, // DELTA EQUAL TO
    { 0x225D, Infix, 5, 5, 0}, // EQUAL TO BY DEFINITION
    { 0x225E, Infix, 5, 5, 0}, // MEASURED BY
    { 0x225F, Infix, 5, 5, 0}, // QUESTIONED EQUAL TO
    { 0x2260, Infix, 5, 5, 0}, // NOT EQUAL TO
    { 0x2261, Infix, 5, 5, 0}, // IDENTICAL TO
    { 0x2262, Infix, 5, 5, 0}, // NOT IDENTICAL TO
    { 0x2263, Infix, 5, 5, 0}, // STRICTLY EQUIVALENT TO
    { 0x2264, Infix, 5, 5, 0}, // LESS-THAN OR EQUAL TO
    { 0x2265, Infix, 5, 5, 0}, // GREATER-THAN OR EQUAL TO
    { 0x2266, Infix, 5, 5, 0}, // LESS-THAN OVER EQUAL TO
    { 0x2267, Infix, 5, 5, 0}, // GREATER-THAN OVER EQUAL TO
    { 0x2268, Infix, 5, 5, 0}, // LESS-THAN BUT NOT EQUAL TO
    { 0x2269, Infix, 5, 5, 0}, // GREATER-THAN BUT NOT EQUAL TO
    { 0x226A, Infix, 5, 5, 0}, // MUCH LESS-THAN
    { 0x226B, Infix, 5, 5, 0}, // MUCH GREATER-THAN
    { 0x226C, Infix, 5, 5, 0}, // BETWEEN
    { 0x226D, Infix, 5, 5, 0}, // NOT EQUIVALENT TO
    { 0x226E, Infix, 5, 5, 0}, // NOT LESS-THAN
    { 0x226F, Infix, 5, 5, 0}, // NOT GREATER-THAN
    { 0x2270, Infix, 5, 5, 0}, // NEITHER LESS-THAN NOR EQUAL TO
    { 0x2271, Infix, 5, 5, 0}, // NEITHER GREATER-THAN NOR EQUAL TO
    { 0x2272, Infix, 5, 5, 0}, // LESS-THAN OR EQUIVALENT TO
    { 0x2273, Infix, 5, 5, 0}, // GREATER-THAN OR EQUIVALENT TO
    { 0x2274, Infix, 5, 5, 0}, // NEITHER LESS-THAN NOR EQUIVALENT TO
    { 0x2275, Infix, 5, 5, 0}, // NEITHER GREATER-THAN NOR EQUIVALENT TO
    { 0x2276, Infix, 5, 5, 0}, // LESS-THAN OR GREATER-THAN
    { 0x2277, Infix, 5, 5, 0}, // GREATER-THAN OR LESS-THAN
    { 0x2278, Infix, 5, 5, 0}, // NEITHER LESS-THAN NOR GREATER-THAN
    { 0x2279, Infix, 5, 5, 0}, // NEITHER GREATER-THAN NOR LESS-THAN
    { 0x227A, Infix, 5, 5, 0}, // PRECEDES
    { 0x227B, Infix, 5, 5, 0}, // SUCCEEDS
    { 0x227C, Infix, 5, 5, 0}, // PRECEDES OR EQUAL TO
    { 0x227D, Infix, 5, 5, 0}, // SUCCEEDS OR EQUAL TO
    { 0x227E, Infix, 5, 5, 0}, // PRECEDES OR EQUIVALENT TO
    { 0x227F, Infix, 5, 5, 0}, // SUCCEEDS OR EQUIVALENT TO
    { 0x2280, Infix, 5, 5, 0}, // DOES NOT PRECEDE
    { 0x2281, Infix, 5, 5, 0}, // DOES NOT SUCCEED
    { 0x2282, Infix, 5, 5, 0}, // SUBSET OF
    { 0x2283, Infix, 5, 5, 0}, // SUPERSET OF
    { 0x2284, Infix, 5, 5, 0}, // NOT A SUBSET OF
    { 0x2285, Infix, 5, 5, 0}, // NOT A SUPERSET OF
    { 0x2286, Infix, 5, 5, 0}, // SUBSET OF OR EQUAL TO
    { 0x2287, Infix, 5, 5, 0}, // SUPERSET OF OR EQUAL TO
    { 0x2288, Infix, 5, 5, 0}, // NEITHER A SUBSET OF NOR EQUAL TO
    { 0x2289, Infix, 5, 5, 0}, // NEITHER A SUPERSET OF NOR EQUAL TO
    { 0x228A, Infix, 5, 5, 0}, // SUBSET OF WITH NOT EQUAL TO
    { 0x228B, Infix, 5, 5, 0}, // SUPERSET OF WITH NOT EQUAL TO
    { 0x228C, Infix, 4, 4, 0}, // MULTISET
    { 0x228D, Infix, 4, 4, 0}, // MULTISET MULTIPLICATION
    { 0x228E, Infix, 4, 4, 0}, // MULTISET UNION
    { 0x228F, Infix, 5, 5, 0}, // SQUARE IMAGE OF
    { 0x2290, Infix, 5, 5, 0}, // SQUARE ORIGINAL OF
    { 0x2291, Infix, 5, 5, 0}, // SQUARE IMAGE OF OR EQUAL TO
    { 0x2292, Infix, 5, 5, 0}, // SQUARE ORIGINAL OF OR EQUAL TO
    { 0x2293, Infix, 4, 4, 0}, // SQUARE CAP
    { 0x2294, Infix, 4, 4, 0}, // SQUARE CUP
    { 0x2295, Infix, 4, 4, 0}, // CIRCLED PLUS
    { 0x2296, Infix, 4, 4, 0}, // CIRCLED MINUS
    { 0x2297, Infix, 4, 4, 0}, // CIRCLED TIMES
    { 0x2298, Infix, 4, 4, 0}, // CIRCLED DIVISION SLASH
    { 0x2299, Infix, 4, 4, 0}, // CIRCLED DOT OPERATOR
    { 0x229A, Infix, 4, 4, 0}, // CIRCLED RING OPERATOR
    { 0x229B, Infix, 4, 4, 0}, // CIRCLED ASTERISK OPERATOR
    { 0x229C, Infix, 4, 4, 0}, // CIRCLED EQUALS
    { 0x229D, Infix, 4, 4, 0}, // CIRCLED DASH
    { 0x229E, Infix, 4, 4, 0}, // SQUARED PLUS
    { 0x229F, Infix, 4, 4, 0}, // SQUARED MINUS
    { 0x22A0, Infix, 4, 4, 0}, // SQUARED TIMES
    { 0x22A1, Infix, 4, 4, 0}, // SQUARED DOT OPERATOR
    { 0x22A2, Infix, 5, 5, 0}, // RIGHT TACK
    { 0x22A3, Infix, 5, 5, 0}, // LEFT TACK
    { 0x22A4, Infix, 5, 5, 0}, // DOWN TACK
    { 0x22A5, Infix, 5, 5, 0}, // UP TACK
    { 0x22A6, Infix, 5, 5, 0}, // ASSERTION
    { 0x22A7, Infix, 5, 5, 0}, // MODELS
    { 0x22A8, Infix, 5, 5, 0}, // TRUE
    { 0x22A9, Infix, 5, 5, 0}, // FORCES
    { 0x22AA, Infix, 5, 5, 0}, // TRIPLE VERTICAL BAR RIGHT TURNSTILE
    { 0x22AB, Infix, 5, 5, 0}, // DOUBLE VERTICAL BAR DOUBLE RIGHT TURNSTILE
    { 0x22AC, Infix, 5, 5, 0}, // DOES NOT PROVE
    { 0x22AD, Infix, 5, 5, 0}, // NOT TRUE
    { 0x22AE, Infix, 5, 5, 0}, // DOES NOT FORCE
    { 0x22AF, Infix, 5, 5, 0}, // NEGATED DOUBLE VERTICAL BAR DOUBLE RIGHT TURNSTILE
    { 0x22B0, Infix, 5, 5, 0}, // PRECEDES UNDER RELATION
    { 0x22B1, Infix, 5, 5, 0}, // SUCCEEDS UNDER RELATION
    { 0x22B2, Infix, 5, 5, 0}, // NORMAL SUBGROUP OF
    { 0x22B3, Infix, 5, 5, 0}, // CONTAINS AS NORMAL SUBGROUP
    { 0x22B4, Infix, 5, 5, 0}, // NORMAL SUBGROUP OF OR EQUAL TO
    { 0x22B5, Infix, 5, 5, 0}, // CONTAINS AS NORMAL SUBGROUP OR EQUAL TO
    { 0x22B6, Infix, 5, 5, 0}, // ORIGINAL OF
    { 0x22B7, Infix, 5, 5, 0}, // IMAGE OF
    { 0x22B8, Infix, 5, 5, 0}, // MULTIMAP
    { 0x22B9, Infix, 5, 5, 0}, // HERMITIAN CONJUGATE MATRIX
    { 0x22BA, Infix, 4, 4, 0}, // INTERCALATE
    { 0x22BB, Infix, 4, 4, 0}, // XOR
    { 0x22BC, Infix, 4, 4, 0}, // NAND
    { 0x22BD, Infix, 4, 4, 0}, // NOR
    { 0x22BE, Infix, 3, 3, 0}, // RIGHT ANGLE WITH ARC
    { 0x22BF, Infix, 3, 3, 0}, // RIGHT TRIANGLE
    { 0x22C0, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // N-ARY LOGICAL AND
    { 0x22C1, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // N-ARY LOGICAL OR
    { 0x22C2, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // N-ARY INTERSECTION
    { 0x22C3, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // N-ARY UNION
    { 0x22C4, Infix, 4, 4, 0}, // DIAMOND OPERATOR
    { 0x22C5, Infix, 4, 4, 0}, // DOT OPERATOR
    { 0x22C6, Infix, 4, 4, 0}, // STAR OPERATOR
    { 0x22C7, Infix, 4, 4, 0}, // DIVISION TIMES
    { 0x22C8, Infix, 5, 5, 0}, // BOWTIE
    { 0x22C9, Infix, 4, 4, 0}, // LEFT NORMAL FACTOR SEMIDIRECT PRODUCT
    { 0x22CA, Infix, 4, 4, 0}, // RIGHT NORMAL FACTOR SEMIDIRECT PRODUCT
    { 0x22CB, Infix, 4, 4, 0}, // LEFT SEMIDIRECT PRODUCT
    { 0x22CC, Infix, 4, 4, 0}, // RIGHT SEMIDIRECT PRODUCT
    { 0x22CD, Infix, 5, 5, 0}, // REVERSED TILDE EQUALS
    { 0x22CE, Infix, 4, 4, 0}, // CURLY LOGICAL OR
    { 0x22CF, Infix, 4, 4, 0}, // CURLY LOGICAL AND
    { 0x22D0, Infix, 5, 5, 0}, // DOUBLE SUBSET
    { 0x22D1, Infix, 5, 5, 0}, // DOUBLE SUPERSET
    { 0x22D2, Infix, 4, 4, 0}, // DOUBLE INTERSECTION
    { 0x22D3, Infix, 4, 4, 0}, // DOUBLE UNION
    { 0x22D4, Infix, 5, 5, 0}, // PITCHFORK
    { 0x22D5, Infix, 5, 5, 0}, // EQUAL AND PARALLEL TO
    { 0x22D6, Infix, 5, 5, 0}, // LESS-THAN WITH DOT
    { 0x22D7, Infix, 5, 5, 0}, // GREATER-THAN WITH DOT
    { 0x22D8, Infix, 5, 5, 0}, // VERY MUCH LESS-THAN
    { 0x22D9, Infix, 5, 5, 0}, // VERY MUCH GREATER-THAN
    { 0x22DA, Infix, 5, 5, 0}, // LESS-THAN EQUAL TO OR GREATER-THAN
    { 0x22DB, Infix, 5, 5, 0}, // GREATER-THAN EQUAL TO OR LESS-THAN
    { 0x22DC, Infix, 5, 5, 0}, // EQUAL TO OR LESS-THAN
    { 0x22DD, Infix, 5, 5, 0}, // EQUAL TO OR GREATER-THAN
    { 0x22DE, Infix, 5, 5, 0}, // EQUAL TO OR PRECEDES
    { 0x22DF, Infix, 5, 5, 0}, // EQUAL TO OR SUCCEEDS
    { 0x22E0, Infix, 5, 5, 0}, // DOES NOT PRECEDE OR EQUAL
    { 0x22E1, Infix, 5, 5, 0}, // DOES NOT SUCCEED OR EQUAL
    { 0x22E2, Infix, 5, 5, 0}, // NOT SQUARE IMAGE OF OR EQUAL TO
    { 0x22E3, Infix, 5, 5, 0}, // NOT SQUARE ORIGINAL OF OR EQUAL TO
    { 0x22E4, Infix, 5, 5, 0}, // SQUARE IMAGE OF OR NOT EQUAL TO
    { 0x22E5, Infix, 5, 5, 0}, // SQUARE ORIGINAL OF OR NOT EQUAL TO
    { 0x22E6, Infix, 5, 5, 0}, // LESS-THAN BUT NOT EQUIVALENT TO
    { 0x22E7, Infix, 5, 5, 0}, // GREATER-THAN BUT NOT EQUIVALENT TO
    { 0x22E8, Infix, 5, 5, 0}, // PRECEDES BUT NOT EQUIVALENT TO
    { 0x22E9, Infix, 5, 5, 0}, // SUCCEEDS BUT NOT EQUIVALENT TO
    { 0x22EA, Infix, 5, 5, 0}, // NOT NORMAL SUBGROUP OF
    { 0x22EB, Infix, 5, 5, 0}, // DOES NOT CONTAIN AS NORMAL SUBGROUP
    { 0x22EC, Infix, 5, 5, 0}, // NOT NORMAL SUBGROUP OF OR EQUAL TO
    { 0x22ED, Infix, 5, 5, 0}, // DOES NOT CONTAIN AS NORMAL SUBGROUP OR EQUAL
    { 0x22EE, Infix, 5, 5, 0}, // VERTICAL ELLIPSIS
    { 0x22EF, Infix, 0, 0, 0}, // MIDLINE HORIZONTAL ELLIPSIS
    { 0x22F0, Infix, 5, 5, 0}, // UP RIGHT DIAGONAL ELLIPSIS
    { 0x22F1, Infix, 5, 5, 0}, // DOWN RIGHT DIAGONAL ELLIPSIS
    { 0x22F2, Infix, 5, 5, 0}, // ELEMENT OF WITH LONG HORIZONTAL STROKE
    { 0x22F3, Infix, 5, 5, 0}, // ELEMENT OF WITH VERTICAL BAR AT END OF HORIZONTAL STROKE
    { 0x22F4, Infix, 5, 5, 0}, // SMALL ELEMENT OF WITH VERTICAL BAR AT END OF HORIZONTAL STROKE
    { 0x22F5, Infix, 5, 5, 0}, // ELEMENT OF WITH DOT ABOVE
    { 0x22F6, Infix, 5, 5, 0}, // ELEMENT OF WITH OVERBAR
    { 0x22F7, Infix, 5, 5, 0}, // SMALL ELEMENT OF WITH OVERBAR
    { 0x22F8, Infix, 5, 5, 0}, // ELEMENT OF WITH UNDERBAR
    { 0x22F9, Infix, 5, 5, 0}, // ELEMENT OF WITH TWO HORIZONTAL STROKES
    { 0x22FA, Infix, 5, 5, 0}, // CONTAINS WITH LONG HORIZONTAL STROKE
    { 0x22FB, Infix, 5, 5, 0}, // CONTAINS WITH VERTICAL BAR AT END OF HORIZONTAL STROKE
    { 0x22FC, Infix, 5, 5, 0}, // SMALL CONTAINS WITH VERTICAL BAR AT END OF HORIZONTAL STROKE
    { 0x22FD, Infix, 5, 5, 0}, // CONTAINS WITH OVERBAR
    { 0x22FE, Infix, 5, 5, 0}, // SMALL CONTAINS WITH OVERBAR
    { 0x22FF, Infix, 5, 5, 0}, // Z NOTATION BAG MEMBERSHIP
    { 0x2308, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // LEFT CEILING
    { 0x2309, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // RIGHT CEILING
    { 0x230A, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // LEFT FLOOR
    { 0x230B, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // RIGHT FLOOR
    { 0x23B4, Postfix, 0, 0, Accent | Stretchy}, // TOP SQUARE BRACKET
    { 0x23B5, Postfix, 0, 0, Accent | Stretchy}, // BOTTOM SQUARE BRACKET
    { 0x23DC, Postfix, 0, 0, Accent | Stretchy}, // TOP PARENTHESIS
    { 0x23DD, Postfix, 0, 0, Accent | Stretchy}, // BOTTOM PARENTHESIS
    { 0x23DE, Postfix, 0, 0, Accent | Stretchy}, // TOP CURLY BRACKET
    { 0x23DF, Postfix, 0, 0, Accent | Stretchy}, // BOTTOM CURLY BRACKET
    { 0x23E0, Postfix, 0, 0, Accent | Stretchy}, // TOP TORTOISE SHELL BRACKET
    { 0x23E1, Postfix, 0, 0, Accent | Stretchy}, // BOTTOM TORTOISE SHELL BRACKET
    { 0x25A0, Infix, 3, 3, 0}, // BLACK SQUARE
    { 0x25A1, Infix, 3, 3, 0}, // WHITE SQUARE
    { 0x25AA, Infix, 3, 3, 0}, // BLACK SMALL SQUARE
    { 0x25AB, Infix, 3, 3, 0}, // WHITE SMALL SQUARE
    { 0x25AD, Infix, 3, 3, 0}, // WHITE RECTANGLE
    { 0x25AE, Infix, 3, 3, 0}, // BLACK VERTICAL RECTANGLE
    { 0x25AF, Infix, 3, 3, 0}, // WHITE VERTICAL RECTANGLE
    { 0x25B0, Infix, 3, 3, 0}, // BLACK PARALLELOGRAM
    { 0x25B1, Infix, 3, 3, 0}, // WHITE PARALLELOGRAM
    { 0x25B2, Infix, 4, 4, 0}, // BLACK UP-POINTING TRIANGLE
    { 0x25B3, Infix, 4, 4, 0}, // WHITE UP-POINTING TRIANGLE
    { 0x25B4, Infix, 4, 4, 0}, // BLACK UP-POINTING SMALL TRIANGLE
    { 0x25B5, Infix, 4, 4, 0}, // WHITE UP-POINTING SMALL TRIANGLE
    { 0x25B6, Infix, 4, 4, 0}, // BLACK RIGHT-POINTING TRIANGLE
    { 0x25B7, Infix, 4, 4, 0}, // WHITE RIGHT-POINTING TRIANGLE
    { 0x25B8, Infix, 4, 4, 0}, // BLACK RIGHT-POINTING SMALL TRIANGLE
    { 0x25B9, Infix, 4, 4, 0}, // WHITE RIGHT-POINTING SMALL TRIANGLE
    { 0x25BC, Infix, 4, 4, 0}, // BLACK DOWN-POINTING TRIANGLE
    { 0x25BD, Infix, 4, 4, 0}, // WHITE DOWN-POINTING TRIANGLE
    { 0x25BE, Infix, 4, 4, 0}, // BLACK DOWN-POINTING SMALL TRIANGLE
    { 0x25BF, Infix, 4, 4, 0}, // WHITE DOWN-POINTING SMALL TRIANGLE
    { 0x25C0, Infix, 4, 4, 0}, // BLACK LEFT-POINTING TRIANGLE
    { 0x25C1, Infix, 4, 4, 0}, // WHITE LEFT-POINTING TRIANGLE
    { 0x25C2, Infix, 4, 4, 0}, // BLACK LEFT-POINTING SMALL TRIANGLE
    { 0x25C3, Infix, 4, 4, 0}, // WHITE LEFT-POINTING SMALL TRIANGLE
    { 0x25C4, Infix, 4, 4, 0}, // BLACK LEFT-POINTING POINTER
    { 0x25C5, Infix, 4, 4, 0}, // WHITE LEFT-POINTING POINTER
    { 0x25C6, Infix, 4, 4, 0}, // BLACK DIAMOND
    { 0x25C7, Infix, 4, 4, 0}, // WHITE DIAMOND
    { 0x25C8, Infix, 4, 4, 0}, // WHITE DIAMOND CONTAINING BLACK SMALL DIAMOND
    { 0x25C9, Infix, 4, 4, 0}, // FISHEYE
    { 0x25CC, Infix, 4, 4, 0}, // DOTTED CIRCLE
    { 0x25CD, Infix, 4, 4, 0}, // CIRCLE WITH VERTICAL FILL
    { 0x25CE, Infix, 4, 4, 0}, // BULLSEYE
    { 0x25CF, Infix, 4, 4, 0}, // BLACK CIRCLE
    { 0x25D6, Infix, 4, 4, 0}, // LEFT HALF BLACK CIRCLE
    { 0x25D7, Infix, 4, 4, 0}, // RIGHT HALF BLACK CIRCLE
    { 0x25E6, Infix, 4, 4, 0}, // WHITE BULLET
    { 0x266D, Postfix, 0, 2, 0}, // MUSIC FLAT SIGN
    { 0x266E, Postfix, 0, 2, 0}, // MUSIC NATURAL SIGN
    { 0x266F, Postfix, 0, 2, 0}, // MUSIC SHARP SIGN
    { 0x2758, Infix, 5, 5, 0}, // LIGHT VERTICAL BAR
    { 0x2772, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // LIGHT LEFT TORTOISE SHELL BRACKET ORNAMENT
    { 0x2773, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // LIGHT RIGHT TORTOISE SHELL BRACKET ORNAMENT
    { 0x27E6, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // MATHEMATICAL LEFT WHITE SQUARE BRACKET
    { 0x27E7, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // MATHEMATICAL RIGHT WHITE SQUARE BRACKET
    { 0x27E8, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // MATHEMATICAL LEFT ANGLE BRACKET
    { 0x27E9, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // MATHEMATICAL RIGHT ANGLE BRACKET
    { 0x27EA, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // MATHEMATICAL LEFT DOUBLE ANGLE BRACKET
    { 0x27EB, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // MATHEMATICAL RIGHT DOUBLE ANGLE BRACKET
    { 0x27EC, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // MATHEMATICAL LEFT WHITE TORTOISE SHELL BRACKET
    { 0x27ED, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // MATHEMATICAL RIGHT WHITE TORTOISE SHELL BRACKET
    { 0x27EE, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // MATHEMATICAL LEFT FLATTENED PARENTHESIS
    { 0x27EF, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // MATHEMATICAL RIGHT FLATTENED PARENTHESIS
    { 0x27F0, Infix, 5, 5, Stretchy}, // UPWARDS QUADRUPLE ARROW
    { 0x27F1, Infix, 5, 5, Stretchy}, // DOWNWARDS QUADRUPLE ARROW
    { 0x27F5, Infix, 5, 5, Stretchy | Accent}, // LONG LEFTWARDS ARROW
    { 0x27F6, Infix, 5, 5, Stretchy | Accent}, // LONG RIGHTWARDS ARROW
    { 0x27F7, Infix, 5, 5, Stretchy | Accent}, // LONG LEFT RIGHT ARROW
    { 0x27F8, Infix, 5, 5, Stretchy | Accent}, // LONG LEFTWARDS DOUBLE ARROW
    { 0x27F9, Infix, 5, 5, Stretchy | Accent}, // LONG RIGHTWARDS DOUBLE ARROW
    { 0x27FA, Infix, 5, 5, Stretchy | Accent}, // LONG LEFT RIGHT DOUBLE ARROW
    { 0x27FB, Infix, 5, 5, Stretchy | Accent}, // LONG LEFTWARDS ARROW FROM BAR
    { 0x27FC, Infix, 5, 5, Stretchy | Accent}, // LONG RIGHTWARDS ARROW FROM BAR
    { 0x27FD, Infix, 5, 5, Stretchy | Accent}, // LONG LEFTWARDS DOUBLE ARROW FROM BAR
    { 0x27FE, Infix, 5, 5, Stretchy | Accent}, // LONG RIGHTWARDS DOUBLE ARROW FROM BAR
    { 0x27FF, Infix, 5, 5, Stretchy | Accent}, // LONG RIGHTWARDS SQUIGGLE ARROW
    { 0x2900, Infix, 5, 5, Accent}, // RIGHTWARDS TWO-HEADED ARROW WITH VERTICAL STROKE
    { 0x2901, Infix, 5, 5, Accent}, // RIGHTWARDS TWO-HEADED ARROW WITH DOUBLE VERTICAL STROKE
    { 0x2902, Infix, 5, 5, Accent}, // LEFTWARDS DOUBLE ARROW WITH VERTICAL STROKE
    { 0x2903, Infix, 5, 5, Accent}, // RIGHTWARDS DOUBLE ARROW WITH VERTICAL STROKE
    { 0x2904, Infix, 5, 5, Accent}, // LEFT RIGHT DOUBLE ARROW WITH VERTICAL STROKE
    { 0x2905, Infix, 5, 5, Accent}, // RIGHTWARDS TWO-HEADED ARROW FROM BAR
    { 0x2906, Infix, 5, 5, Accent}, // LEFTWARDS DOUBLE ARROW FROM BAR
    { 0x2907, Infix, 5, 5, Accent}, // RIGHTWARDS DOUBLE ARROW FROM BAR
    { 0x2908, Infix, 5, 5, 0}, // DOWNWARDS ARROW WITH HORIZONTAL STROKE
    { 0x2909, Infix, 5, 5, 0}, // UPWARDS ARROW WITH HORIZONTAL STROKE
    { 0x290A, Infix, 5, 5, Stretchy}, // UPWARDS TRIPLE ARROW
    { 0x290B, Infix, 5, 5, Stretchy}, // DOWNWARDS TRIPLE ARROW
    { 0x290C, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS DOUBLE DASH ARROW
    { 0x290D, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS DOUBLE DASH ARROW
    { 0x290E, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS TRIPLE DASH ARROW
    { 0x290F, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS TRIPLE DASH ARROW
    { 0x2910, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS TWO-HEADED TRIPLE DASH ARROW
    { 0x2911, Infix, 5, 5, Accent}, // RIGHTWARDS ARROW WITH DOTTED STEM
    { 0x2912, Infix, 5, 5, Stretchy}, // UPWARDS ARROW TO BAR
    { 0x2913, Infix, 5, 5, Stretchy}, // DOWNWARDS ARROW TO BAR
    { 0x2914, Infix, 5, 5, Accent}, // RIGHTWARDS ARROW WITH TAIL WITH VERTICAL STROKE
    { 0x2915, Infix, 5, 5, Accent}, // RIGHTWARDS ARROW WITH TAIL WITH DOUBLE VERTICAL STROKE
    { 0x2916, Infix, 5, 5, Accent}, // RIGHTWARDS TWO-HEADED ARROW WITH TAIL
    { 0x2917, Infix, 5, 5, Accent}, // RIGHTWARDS TWO-HEADED ARROW WITH TAIL WITH VERTICAL STROKE
    { 0x2918, Infix, 5, 5, Accent}, // RIGHTWARDS TWO-HEADED ARROW WITH TAIL WITH DOUBLE VERTICAL STROKE
    { 0x2919, Infix, 5, 5, Accent}, // LEFTWARDS ARROW-TAIL
    { 0x291A, Infix, 5, 5, Accent}, // RIGHTWARDS ARROW-TAIL
    { 0x291B, Infix, 5, 5, Accent}, // LEFTWARDS DOUBLE ARROW-TAIL
    { 0x291C, Infix, 5, 5, Accent}, // RIGHTWARDS DOUBLE ARROW-TAIL
    { 0x291D, Infix, 5, 5, Accent}, // LEFTWARDS ARROW TO BLACK DIAMOND
    { 0x291E, Infix, 5, 5, Accent}, // RIGHTWARDS ARROW TO BLACK DIAMOND
    { 0x291F, Infix, 5, 5, Accent}, // LEFTWARDS ARROW FROM BAR TO BLACK DIAMOND
    { 0x2920, Infix, 5, 5, Accent}, // RIGHTWARDS ARROW FROM BAR TO BLACK DIAMOND
    { 0x2921, Infix, 5, 5, Stretchy}, // NORTH WEST AND SOUTH EAST ARROW
    { 0x2922, Infix, 5, 5, Stretchy}, // NORTH EAST AND SOUTH WEST ARROW
    { 0x2923, Infix, 5, 5, 0}, // NORTH WEST ARROW WITH HOOK
    { 0x2924, Infix, 5, 5, 0}, // NORTH EAST ARROW WITH HOOK
    { 0x2925, Infix, 5, 5, 0}, // SOUTH EAST ARROW WITH HOOK
    { 0x2926, Infix, 5, 5, 0}, // SOUTH WEST ARROW WITH HOOK
    { 0x2927, Infix, 5, 5, 0}, // NORTH WEST ARROW AND NORTH EAST ARROW
    { 0x2928, Infix, 5, 5, 0}, // NORTH EAST ARROW AND SOUTH EAST ARROW
    { 0x2929, Infix, 5, 5, 0}, // SOUTH EAST ARROW AND SOUTH WEST ARROW
    { 0x292A, Infix, 5, 5, 0}, // SOUTH WEST ARROW AND NORTH WEST ARROW
    { 0x292B, Infix, 5, 5, 0}, // RISING DIAGONAL CROSSING FALLING DIAGONAL
    { 0x292C, Infix, 5, 5, 0}, // FALLING DIAGONAL CROSSING RISING DIAGONAL
    { 0x292D, Infix, 5, 5, 0}, // SOUTH EAST ARROW CROSSING NORTH EAST ARROW
    { 0x292E, Infix, 5, 5, 0}, // NORTH EAST ARROW CROSSING SOUTH EAST ARROW
    { 0x292F, Infix, 5, 5, 0}, // FALLING DIAGONAL CROSSING NORTH EAST ARROW
    { 0x2930, Infix, 5, 5, 0}, // RISING DIAGONAL CROSSING SOUTH EAST ARROW
    { 0x2931, Infix, 5, 5, 0}, // NORTH EAST ARROW CROSSING NORTH WEST ARROW
    { 0x2932, Infix, 5, 5, 0}, // NORTH WEST ARROW CROSSING NORTH EAST ARROW
    { 0x2933, Infix, 5, 5, Accent}, // WAVE ARROW POINTING DIRECTLY RIGHT
    { 0x2934, Infix, 5, 5, 0}, // ARROW POINTING RIGHTWARDS THEN CURVING UPWARDS
    { 0x2935, Infix, 5, 5, 0}, // ARROW POINTING RIGHTWARDS THEN CURVING DOWNWARDS
    { 0x2936, Infix, 5, 5, 0}, // ARROW POINTING DOWNWARDS THEN CURVING LEFTWARDS
    { 0x2937, Infix, 5, 5, 0}, // ARROW POINTING DOWNWARDS THEN CURVING RIGHTWARDS
    { 0x2938, Infix, 5, 5, 0}, // RIGHT-SIDE ARC CLOCKWISE ARROW
    { 0x2939, Infix, 5, 5, 0}, // LEFT-SIDE ARC ANTICLOCKWISE ARROW
    { 0x293A, Infix, 5, 5, Accent}, // TOP ARC ANTICLOCKWISE ARROW
    { 0x293B, Infix, 5, 5, Accent}, // BOTTOM ARC ANTICLOCKWISE ARROW
    { 0x293C, Infix, 5, 5, Accent}, // TOP ARC CLOCKWISE ARROW WITH MINUS
    { 0x293D, Infix, 5, 5, Accent}, // TOP ARC ANTICLOCKWISE ARROW WITH PLUS
    { 0x293E, Infix, 5, 5, 0}, // LOWER RIGHT SEMICIRCULAR CLOCKWISE ARROW
    { 0x293F, Infix, 5, 5, 0}, // LOWER LEFT SEMICIRCULAR ANTICLOCKWISE ARROW
    { 0x2940, Infix, 5, 5, 0}, // ANTICLOCKWISE CLOSED CIRCLE ARROW
    { 0x2941, Infix, 5, 5, 0}, // CLOCKWISE CLOSED CIRCLE ARROW
    { 0x2942, Infix, 5, 5, Accent}, // RIGHTWARDS ARROW ABOVE SHORT LEFTWARDS ARROW
    { 0x2943, Infix, 5, 5, Accent}, // LEFTWARDS ARROW ABOVE SHORT RIGHTWARDS ARROW
    { 0x2944, Infix, 5, 5, Accent}, // SHORT RIGHTWARDS ARROW ABOVE LEFTWARDS ARROW
    { 0x2945, Infix, 5, 5, Accent}, // RIGHTWARDS ARROW WITH PLUS BELOW
    { 0x2946, Infix, 5, 5, Accent}, // LEFTWARDS ARROW WITH PLUS BELOW
    { 0x2947, Infix, 5, 5, Accent}, // RIGHTWARDS ARROW THROUGH X
    { 0x2948, Infix, 5, 5, Accent}, // LEFT RIGHT ARROW THROUGH SMALL CIRCLE
    { 0x2949, Infix, 5, 5, 0}, // UPWARDS TWO-HEADED ARROW FROM SMALL CIRCLE
    { 0x294A, Infix, 5, 5, Accent}, // LEFT BARB UP RIGHT BARB DOWN HARPOON
    { 0x294B, Infix, 5, 5, Accent}, // LEFT BARB DOWN RIGHT BARB UP HARPOON
    { 0x294C, Infix, 5, 5, 0}, // UP BARB RIGHT DOWN BARB LEFT HARPOON
    { 0x294D, Infix, 5, 5, 0}, // UP BARB LEFT DOWN BARB RIGHT HARPOON
    { 0x294E, Infix, 5, 5, Stretchy | Accent}, // LEFT BARB UP RIGHT BARB UP HARPOON
    { 0x294F, Infix, 5, 5, Stretchy}, // UP BARB RIGHT DOWN BARB RIGHT HARPOON
    { 0x2950, Infix, 5, 5, Stretchy | Accent}, // LEFT BARB DOWN RIGHT BARB DOWN HARPOON
    { 0x2951, Infix, 5, 5, Stretchy}, // UP BARB LEFT DOWN BARB LEFT HARPOON
    { 0x2952, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS HARPOON WITH BARB UP TO BAR
    { 0x2953, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS HARPOON WITH BARB UP TO BAR
    { 0x2954, Infix, 5, 5, Stretchy}, // UPWARDS HARPOON WITH BARB RIGHT TO BAR
    { 0x2955, Infix, 5, 5, Stretchy}, // DOWNWARDS HARPOON WITH BARB RIGHT TO BAR
    { 0x2956, Infix, 5, 5, Stretchy}, // LEFTWARDS HARPOON WITH BARB DOWN TO BAR
    { 0x2957, Infix, 5, 5, Stretchy}, // RIGHTWARDS HARPOON WITH BARB DOWN TO BAR
    { 0x2958, Infix, 5, 5, Stretchy}, // UPWARDS HARPOON WITH BARB LEFT TO BAR
    { 0x2959, Infix, 5, 5, Stretchy}, // DOWNWARDS HARPOON WITH BARB LEFT TO BAR
    { 0x295A, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS HARPOON WITH BARB UP FROM BAR
    { 0x295B, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS HARPOON WITH BARB UP FROM BAR
    { 0x295C, Infix, 5, 5, Stretchy}, // UPWARDS HARPOON WITH BARB RIGHT FROM BAR
    { 0x295D, Infix, 5, 5, Stretchy}, // DOWNWARDS HARPOON WITH BARB RIGHT FROM BAR
    { 0x295E, Infix, 5, 5, Stretchy | Accent}, // LEFTWARDS HARPOON WITH BARB DOWN FROM BAR
    { 0x295F, Infix, 5, 5, Stretchy | Accent}, // RIGHTWARDS HARPOON WITH BARB DOWN FROM BAR
    { 0x2960, Infix, 5, 5, Stretchy}, // UPWARDS HARPOON WITH BARB LEFT FROM BAR
    { 0x2961, Infix, 5, 5, Stretchy}, // DOWNWARDS HARPOON WITH BARB LEFT FROM BAR
    { 0x2962, Infix, 5, 5, Accent}, // LEFTWARDS HARPOON WITH BARB UP ABOVE LEFTWARDS HARPOON WITH BARB DOWN
    { 0x2963, Infix, 5, 5, 0}, // UPWARDS HARPOON WITH BARB LEFT BESIDE UPWARDS HARPOON WITH BARB RIGHT
    { 0x2964, Infix, 5, 5, Accent}, // RIGHTWARDS HARPOON WITH BARB UP ABOVE RIGHTWARDS HARPOON WITH BARB DOWN
    { 0x2965, Infix, 5, 5, 0}, // DOWNWARDS HARPOON WITH BARB LEFT BESIDE DOWNWARDS HARPOON WITH BARB RIGHT
    { 0x2966, Infix, 5, 5, Accent}, // LEFTWARDS HARPOON WITH BARB UP ABOVE RIGHTWARDS HARPOON WITH BARB UP
    { 0x2967, Infix, 5, 5, Accent}, // LEFTWARDS HARPOON WITH BARB DOWN ABOVE RIGHTWARDS HARPOON WITH BARB DOWN
    { 0x2968, Infix, 5, 5, Accent}, // RIGHTWARDS HARPOON WITH BARB UP ABOVE LEFTWARDS HARPOON WITH BARB UP
    { 0x2969, Infix, 5, 5, Accent}, // RIGHTWARDS HARPOON WITH BARB DOWN ABOVE LEFTWARDS HARPOON WITH BARB DOWN
    { 0x296A, Infix, 5, 5, Accent}, // LEFTWARDS HARPOON WITH BARB UP ABOVE LONG DASH
    { 0x296B, Infix, 5, 5, Accent}, // LEFTWARDS HARPOON WITH BARB DOWN BELOW LONG DASH
    { 0x296C, Infix, 5, 5, Accent}, // RIGHTWARDS HARPOON WITH BARB UP ABOVE LONG DASH
    { 0x296D, Infix, 5, 5, Accent}, // RIGHTWARDS HARPOON WITH BARB DOWN BELOW LONG DASH
    { 0x296E, Infix, 5, 5, Stretchy}, // UPWARDS HARPOON WITH BARB LEFT BESIDE DOWNWARDS HARPOON WITH BARB RIGHT
    { 0x296F, Infix, 5, 5, Stretchy}, // DOWNWARDS HARPOON WITH BARB LEFT BESIDE UPWARDS HARPOON WITH BARB RIGHT
    { 0x2970, Infix, 5, 5, Accent}, // RIGHT DOUBLE ARROW WITH ROUNDED HEAD
    { 0x2971, Infix, 5, 5, Accent}, // EQUALS SIGN ABOVE RIGHTWARDS ARROW
    { 0x2972, Infix, 5, 5, Accent}, // TILDE OPERATOR ABOVE RIGHTWARDS ARROW
    { 0x2973, Infix, 5, 5, Accent}, // LEFTWARDS ARROW ABOVE TILDE OPERATOR
    { 0x2974, Infix, 5, 5, Accent}, // RIGHTWARDS ARROW ABOVE TILDE OPERATOR
    { 0x2975, Infix, 5, 5, Accent}, // RIGHTWARDS ARROW ABOVE ALMOST EQUAL TO
    { 0x2976, Infix, 5, 5, Accent}, // LESS-THAN ABOVE LEFTWARDS ARROW
    { 0x2977, Infix, 5, 5, Accent}, // LEFTWARDS ARROW THROUGH LESS-THAN
    { 0x2978, Infix, 5, 5, Accent}, // GREATER-THAN ABOVE RIGHTWARDS ARROW
    { 0x2979, Infix, 5, 5, Accent}, // SUBSET ABOVE RIGHTWARDS ARROW
    { 0x297A, Infix, 5, 5, Accent}, // LEFTWARDS ARROW THROUGH SUBSET
    { 0x297B, Infix, 5, 5, Accent}, // SUPERSET ABOVE LEFTWARDS ARROW
    { 0x297C, Infix, 5, 5, Accent}, // LEFT FISH TAIL
    { 0x297D, Infix, 5, 5, Accent}, // RIGHT FISH TAIL
    { 0x297E, Infix, 5, 5, 0}, // UP FISH TAIL
    { 0x297F, Infix, 5, 5, 0}, // DOWN FISH TAIL
    { 0x2980, Prefix, 0, 0, Fence | Stretchy}, // TRIPLE VERTICAL BAR DELIMITER
    { 0x2980, Postfix, 0, 0, Fence | Stretchy}, // TRIPLE VERTICAL BAR DELIMITER
    { 0x2981, Infix, 3, 3, 0}, // Z NOTATION SPOT
    { 0x2982, Infix, 3, 3, 0}, // Z NOTATION TYPE COLON
    { 0x2983, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // LEFT WHITE CURLY BRACKET
    { 0x2984, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // RIGHT WHITE CURLY BRACKET
    { 0x2985, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // LEFT WHITE PARENTHESIS
    { 0x2986, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // RIGHT WHITE PARENTHESIS
    { 0x2987, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // Z NOTATION LEFT IMAGE BRACKET
    { 0x2988, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // Z NOTATION RIGHT IMAGE BRACKET
    { 0x2989, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // Z NOTATION LEFT BINDING BRACKET
    { 0x298A, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // Z NOTATION RIGHT BINDING BRACKET
    { 0x298B, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // LEFT SQUARE BRACKET WITH UNDERBAR
    { 0x298C, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // RIGHT SQUARE BRACKET WITH UNDERBAR
    { 0x298D, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // LEFT SQUARE BRACKET WITH TICK IN TOP CORNER
    { 0x298E, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // RIGHT SQUARE BRACKET WITH TICK IN BOTTOM CORNER
    { 0x298F, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // LEFT SQUARE BRACKET WITH TICK IN BOTTOM CORNER
    { 0x2990, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // RIGHT SQUARE BRACKET WITH TICK IN TOP CORNER
    { 0x2991, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // LEFT ANGLE BRACKET WITH DOT
    { 0x2992, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // RIGHT ANGLE BRACKET WITH DOT
    { 0x2993, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // LEFT ARC LESS-THAN BRACKET
    { 0x2994, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // RIGHT ARC GREATER-THAN BRACKET
    { 0x2995, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // DOUBLE LEFT ARC GREATER-THAN BRACKET
    { 0x2996, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // DOUBLE RIGHT ARC LESS-THAN BRACKET
    { 0x2997, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // LEFT BLACK TORTOISE SHELL BRACKET
    { 0x2998, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // RIGHT BLACK TORTOISE SHELL BRACKET
    { 0x2999, Infix, 3, 3, 0}, // DOTTED FENCE
    { 0x299A, Infix, 3, 3, 0}, // VERTICAL ZIGZAG LINE
    { 0x299B, Infix, 3, 3, 0}, // MEASURED ANGLE OPENING LEFT
    { 0x299C, Infix, 3, 3, 0}, // RIGHT ANGLE VARIANT WITH SQUARE
    { 0x299D, Infix, 3, 3, 0}, // MEASURED RIGHT ANGLE WITH DOT
    { 0x299E, Infix, 3, 3, 0}, // ANGLE WITH S INSIDE
    { 0x299F, Infix, 3, 3, 0}, // ACUTE ANGLE
    { 0x29A0, Infix, 3, 3, 0}, // SPHERICAL ANGLE OPENING LEFT
    { 0x29A1, Infix, 3, 3, 0}, // SPHERICAL ANGLE OPENING UP
    { 0x29A2, Infix, 3, 3, 0}, // TURNED ANGLE
    { 0x29A3, Infix, 3, 3, 0}, // REVERSED ANGLE
    { 0x29A4, Infix, 3, 3, 0}, // ANGLE WITH UNDERBAR
    { 0x29A5, Infix, 3, 3, 0}, // REVERSED ANGLE WITH UNDERBAR
    { 0x29A6, Infix, 3, 3, 0}, // OBLIQUE ANGLE OPENING UP
    { 0x29A7, Infix, 3, 3, 0}, // OBLIQUE ANGLE OPENING DOWN
    { 0x29A8, Infix, 3, 3, 0}, // MEASURED ANGLE WITH OPEN ARM ENDING IN ARROW POINTING UP AND RIGHT
    { 0x29A9, Infix, 3, 3, 0}, // MEASURED ANGLE WITH OPEN ARM ENDING IN ARROW POINTING UP AND LEFT
    { 0x29AA, Infix, 3, 3, 0}, // MEASURED ANGLE WITH OPEN ARM ENDING IN ARROW POINTING DOWN AND RIGHT
    { 0x29AB, Infix, 3, 3, 0}, // MEASURED ANGLE WITH OPEN ARM ENDING IN ARROW POINTING DOWN AND LEFT
    { 0x29AC, Infix, 3, 3, 0}, // MEASURED ANGLE WITH OPEN ARM ENDING IN ARROW POINTING RIGHT AND UP
    { 0x29AD, Infix, 3, 3, 0}, // MEASURED ANGLE WITH OPEN ARM ENDING IN ARROW POINTING LEFT AND UP
    { 0x29AE, Infix, 3, 3, 0}, // MEASURED ANGLE WITH OPEN ARM ENDING IN ARROW POINTING RIGHT AND DOWN
    { 0x29AF, Infix, 3, 3, 0}, // MEASURED ANGLE WITH OPEN ARM ENDING IN ARROW POINTING LEFT AND DOWN
    { 0x29B0, Infix, 3, 3, 0}, // REVERSED EMPTY SET
    { 0x29B1, Infix, 3, 3, 0}, // EMPTY SET WITH OVERBAR
    { 0x29B2, Infix, 3, 3, 0}, // EMPTY SET WITH SMALL CIRCLE ABOVE
    { 0x29B3, Infix, 3, 3, 0}, // EMPTY SET WITH RIGHT ARROW ABOVE
    { 0x29B4, Infix, 3, 3, 0}, // EMPTY SET WITH LEFT ARROW ABOVE
    { 0x29B5, Infix, 3, 3, 0}, // CIRCLE WITH HORIZONTAL BAR
    { 0x29B6, Infix, 4, 4, 0}, // CIRCLED VERTICAL BAR
    { 0x29B7, Infix, 4, 4, 0}, // CIRCLED PARALLEL
    { 0x29B8, Infix, 4, 4, 0}, // CIRCLED REVERSE SOLIDUS
    { 0x29B9, Infix, 4, 4, 0}, // CIRCLED PERPENDICULAR
    { 0x29BA, Infix, 4, 4, 0}, // CIRCLE DIVIDED BY HORIZONTAL BAR AND TOP HALF DIVIDED BY VERTICAL BAR
    { 0x29BB, Infix, 4, 4, 0}, // CIRCLE WITH SUPERIMPOSED X
    { 0x29BC, Infix, 4, 4, 0}, // CIRCLED ANTICLOCKWISE-ROTATED DIVISION SIGN
    { 0x29BD, Infix, 4, 4, 0}, // UP ARROW THROUGH CIRCLE
    { 0x29BE, Infix, 4, 4, 0}, // CIRCLED WHITE BULLET
    { 0x29BF, Infix, 4, 4, 0}, // CIRCLED BULLET
    { 0x29C0, Infix, 5, 5, 0}, // CIRCLED LESS-THAN
    { 0x29C1, Infix, 5, 5, 0}, // CIRCLED GREATER-THAN
    { 0x29C2, Infix, 3, 3, 0}, // CIRCLE WITH SMALL CIRCLE TO THE RIGHT
    { 0x29C3, Infix, 3, 3, 0}, // CIRCLE WITH TWO HORIZONTAL STROKES TO THE RIGHT
    { 0x29C4, Infix, 4, 4, 0}, // SQUARED RISING DIAGONAL SLASH
    { 0x29C5, Infix, 4, 4, 0}, // SQUARED FALLING DIAGONAL SLASH
    { 0x29C6, Infix, 4, 4, 0}, // SQUARED ASTERISK
    { 0x29C7, Infix, 4, 4, 0}, // SQUARED SMALL CIRCLE
    { 0x29C8, Infix, 4, 4, 0}, // SQUARED SQUARE
    { 0x29C9, Infix, 3, 3, 0}, // TWO JOINED SQUARES
    { 0x29CA, Infix, 3, 3, 0}, // TRIANGLE WITH DOT ABOVE
    { 0x29CB, Infix, 3, 3, 0}, // TRIANGLE WITH UNDERBAR
    { 0x29CC, Infix, 3, 3, 0}, // S IN TRIANGLE
    { 0x29CD, Infix, 3, 3, 0}, // TRIANGLE WITH SERIFS AT BOTTOM
    { 0x29CE, Infix, 5, 5, 0}, // RIGHT TRIANGLE ABOVE LEFT TRIANGLE
    { 0x29CF, Infix, 5, 5, 0}, // LEFT TRIANGLE BESIDE VERTICAL BAR
    { 0x29D0, Infix, 5, 5, 0}, // VERTICAL BAR BESIDE RIGHT TRIANGLE
    { 0x29D1, Infix, 5, 5, 0}, // BOWTIE WITH LEFT HALF BLACK
    { 0x29D2, Infix, 5, 5, 0}, // BOWTIE WITH RIGHT HALF BLACK
    { 0x29D3, Infix, 5, 5, 0}, // BLACK BOWTIE
    { 0x29D4, Infix, 5, 5, 0}, // TIMES WITH LEFT HALF BLACK
    { 0x29D5, Infix, 5, 5, 0}, // TIMES WITH RIGHT HALF BLACK
    { 0x29D6, Infix, 4, 4, 0}, // WHITE HOURGLASS
    { 0x29D7, Infix, 4, 4, 0}, // BLACK HOURGLASS
    { 0x29D8, Infix, 3, 3, 0}, // LEFT WIGGLY FENCE
    { 0x29D9, Infix, 3, 3, 0}, // RIGHT WIGGLY FENCE
    { 0x29DB, Infix, 3, 3, 0}, // RIGHT DOUBLE WIGGLY FENCE
    { 0x29DC, Infix, 3, 3, 0}, // INCOMPLETE INFINITY
    { 0x29DD, Infix, 3, 3, 0}, // TIE OVER INFINITY
    { 0x29DE, Infix, 5, 5, 0}, // INFINITY NEGATED WITH VERTICAL BAR
    { 0x29DF, Infix, 3, 3, 0}, // DOUBLE-ENDED MULTIMAP
    { 0x29E0, Infix, 3, 3, 0}, // SQUARE WITH CONTOURED OUTLINE
    { 0x29E1, Infix, 5, 5, 0}, // INCREASES AS
    { 0x29E2, Infix, 4, 4, 0}, // SHUFFLE PRODUCT
    { 0x29E3, Infix, 5, 5, 0}, // EQUALS SIGN AND SLANTED PARALLEL
    { 0x29E4, Infix, 5, 5, 0}, // EQUALS SIGN AND SLANTED PARALLEL WITH TILDE ABOVE
    { 0x29E5, Infix, 5, 5, 0}, // IDENTICAL TO AND SLANTED PARALLEL
    { 0x29E6, Infix, 5, 5, 0}, // GLEICH STARK
    { 0x29E7, Infix, 3, 3, 0}, // THERMODYNAMIC
    { 0x29E8, Infix, 3, 3, 0}, // DOWN-POINTING TRIANGLE WITH LEFT HALF BLACK
    { 0x29E9, Infix, 3, 3, 0}, // DOWN-POINTING TRIANGLE WITH RIGHT HALF BLACK
    { 0x29EA, Infix, 3, 3, 0}, // BLACK DIAMOND WITH DOWN ARROW
    { 0x29EB, Infix, 3, 3, 0}, // BLACK LOZENGE
    { 0x29EC, Infix, 3, 3, 0}, // WHITE CIRCLE WITH DOWN ARROW
    { 0x29ED, Infix, 3, 3, 0}, // BLACK CIRCLE WITH DOWN ARROW
    { 0x29EE, Infix, 3, 3, 0}, // ERROR-BARRED WHITE SQUARE
    { 0x29EF, Infix, 3, 3, 0}, // ERROR-BARRED BLACK SQUARE
    { 0x29F0, Infix, 3, 3, 0}, // ERROR-BARRED WHITE DIAMOND
    { 0x29F1, Infix, 3, 3, 0}, // ERROR-BARRED BLACK DIAMOND
    { 0x29F2, Infix, 3, 3, 0}, // ERROR-BARRED WHITE CIRCLE
    { 0x29F3, Infix, 3, 3, 0}, // ERROR-BARRED BLACK CIRCLE
    { 0x29F4, Infix, 5, 5, 0}, // RULE-DELAYED
    { 0x29F5, Infix, 4, 4, 0}, // REVERSE SOLIDUS OPERATOR
    { 0x29F6, Infix, 4, 4, 0}, // SOLIDUS WITH OVERBAR
    { 0x29F7, Infix, 4, 4, 0}, // REVERSE SOLIDUS WITH HORIZONTAL STROKE
    { 0x29F8, Infix, 3, 3, 0}, // BIG SOLIDUS
    { 0x29F9, Infix, 3, 3, 0}, // BIG REVERSE SOLIDUS
    { 0x29FA, Infix, 3, 3, 0}, // DOUBLE PLUS
    { 0x29FB, Infix, 3, 3, 0}, // TRIPLE PLUS
    { 0x29FC, Prefix, 0, 0, Symmetric | Fence | Stretchy}, // LEFT-POINTING CURVED ANGLE BRACKET
    { 0x29FD, Postfix, 0, 0, Symmetric | Fence | Stretchy}, // RIGHT-POINTING CURVED ANGLE BRACKET
    { 0x29FE, Infix, 4, 4, 0}, // TINY
    { 0x29FF, Infix, 4, 4, 0}, // MINY
    { 0x2A00, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // N-ARY CIRCLED DOT OPERATOR
    { 0x2A01, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // N-ARY CIRCLED PLUS OPERATOR
    { 0x2A02, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // N-ARY CIRCLED TIMES OPERATOR
    { 0x2A03, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // N-ARY UNION OPERATOR WITH DOT
    { 0x2A04, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // N-ARY UNION OPERATOR WITH PLUS
    { 0x2A05, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // N-ARY SQUARE INTERSECTION OPERATOR
    { 0x2A06, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // N-ARY SQUARE UNION OPERATOR
    { 0x2A07, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // TWO LOGICAL AND OPERATOR
    { 0x2A08, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // TWO LOGICAL OR OPERATOR
    { 0x2A09, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // N-ARY TIMES OPERATOR
    { 0x2A0A, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // MODULO TWO SUM
    { 0x2A0B, Prefix, 1, 2, Symmetric | LargeOp}, // SUMMATION WITH INTEGRAL
    { 0x2A0C, Prefix, 0, 1, Symmetric | LargeOp}, // QUADRUPLE INTEGRAL OPERATOR
    { 0x2A0D, Prefix, 1, 2, Symmetric | LargeOp}, // FINITE PART INTEGRAL
    { 0x2A0E, Prefix, 1, 2, Symmetric | LargeOp}, // INTEGRAL WITH DOUBLE STROKE
    { 0x2A0F, Prefix, 1, 2, Symmetric | LargeOp}, // INTEGRAL AVERAGE WITH SLASH
    { 0x2A10, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // CIRCULATION FUNCTION
    { 0x2A11, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // ANTICLOCKWISE INTEGRATION
    { 0x2A12, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // LINE INTEGRATION WITH RECTANGULAR PATH AROUND POLE
    { 0x2A13, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // LINE INTEGRATION WITH SEMICIRCULAR PATH AROUND POLE
    { 0x2A14, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // LINE INTEGRATION NOT INCLUDING THE POLE
    { 0x2A15, Prefix, 1, 2, Symmetric | LargeOp}, // INTEGRAL AROUND A POINT OPERATOR
    { 0x2A16, Prefix, 1, 2, Symmetric | LargeOp}, // QUATERNION INTEGRAL OPERATOR
    { 0x2A17, Prefix, 1, 2, Symmetric | LargeOp}, // INTEGRAL WITH LEFTWARDS ARROW WITH HOOK
    { 0x2A18, Prefix, 1, 2, Symmetric | LargeOp}, // INTEGRAL WITH TIMES SIGN
    { 0x2A19, Prefix, 1, 2, Symmetric | LargeOp}, // INTEGRAL WITH INTERSECTION
    { 0x2A1A, Prefix, 1, 2, Symmetric | LargeOp}, // INTEGRAL WITH UNION
    { 0x2A1B, Prefix, 1, 2, Symmetric | LargeOp}, // INTEGRAL WITH OVERBAR
    { 0x2A1C, Prefix, 1, 2, Symmetric | LargeOp}, // INTEGRAL WITH UNDERBAR
    { 0x2A1D, Infix, 3, 3, 0}, // JOIN
    { 0x2A1E, Infix, 3, 3, 0}, // LARGE LEFT TRIANGLE OPERATOR
    { 0x2A1F, Infix, 3, 3, 0}, // Z NOTATION SCHEMA COMPOSITION
    { 0x2A20, Infix, 3, 3, 0}, // Z NOTATION SCHEMA PIPING
    { 0x2A21, Infix, 3, 3, 0}, // Z NOTATION SCHEMA PROJECTION
    { 0x2A22, Infix, 4, 4, 0}, // PLUS SIGN WITH SMALL CIRCLE ABOVE
    { 0x2A23, Infix, 4, 4, 0}, // PLUS SIGN WITH CIRCUMFLEX ACCENT ABOVE
    { 0x2A24, Infix, 4, 4, 0}, // PLUS SIGN WITH TILDE ABOVE
    { 0x2A25, Infix, 4, 4, 0}, // PLUS SIGN WITH DOT BELOW
    { 0x2A26, Infix, 4, 4, 0}, // PLUS SIGN WITH TILDE BELOW
    { 0x2A27, Infix, 4, 4, 0}, // PLUS SIGN WITH SUBSCRIPT TWO
    { 0x2A28, Infix, 4, 4, 0}, // PLUS SIGN WITH BLACK TRIANGLE
    { 0x2A29, Infix, 4, 4, 0}, // MINUS SIGN WITH COMMA ABOVE
    { 0x2A2A, Infix, 4, 4, 0}, // MINUS SIGN WITH DOT BELOW
    { 0x2A2B, Infix, 4, 4, 0}, // MINUS SIGN WITH FALLING DOTS
    { 0x2A2C, Infix, 4, 4, 0}, // MINUS SIGN WITH RISING DOTS
    { 0x2A2D, Infix, 4, 4, 0}, // PLUS SIGN IN LEFT HALF CIRCLE
    { 0x2A2E, Infix, 4, 4, 0}, // PLUS SIGN IN RIGHT HALF CIRCLE
    { 0x2A2F, Infix, 4, 4, 0}, // VECTOR OR CROSS PRODUCT
    { 0x2A30, Infix, 4, 4, 0}, // MULTIPLICATION SIGN WITH DOT ABOVE
    { 0x2A31, Infix, 4, 4, 0}, // MULTIPLICATION SIGN WITH UNDERBAR
    { 0x2A32, Infix, 4, 4, 0}, // SEMIDIRECT PRODUCT WITH BOTTOM CLOSED
    { 0x2A33, Infix, 4, 4, 0}, // SMASH PRODUCT
    { 0x2A34, Infix, 4, 4, 0}, // MULTIPLICATION SIGN IN LEFT HALF CIRCLE
    { 0x2A35, Infix, 4, 4, 0}, // MULTIPLICATION SIGN IN RIGHT HALF CIRCLE
    { 0x2A36, Infix, 4, 4, 0}, // CIRCLED MULTIPLICATION SIGN WITH CIRCUMFLEX ACCENT
    { 0x2A37, Infix, 4, 4, 0}, // MULTIPLICATION SIGN IN DOUBLE CIRCLE
    { 0x2A38, Infix, 4, 4, 0}, // CIRCLED DIVISION SIGN
    { 0x2A39, Infix, 4, 4, 0}, // PLUS SIGN IN TRIANGLE
    { 0x2A3A, Infix, 4, 4, 0}, // MINUS SIGN IN TRIANGLE
    { 0x2A3B, Infix, 4, 4, 0}, // MULTIPLICATION SIGN IN TRIANGLE
    { 0x2A3C, Infix, 4, 4, 0}, // INTERIOR PRODUCT
    { 0x2A3D, Infix, 4, 4, 0}, // RIGHTHAND INTERIOR PRODUCT
    { 0x2A3E, Infix, 4, 4, 0}, // Z NOTATION RELATIONAL COMPOSITION
    { 0x2A3F, Infix, 4, 4, 0}, // AMALGAMATION OR COPRODUCT
    { 0x2A40, Infix, 4, 4, 0}, // INTERSECTION WITH DOT
    { 0x2A41, Infix, 4, 4, 0}, // UNION WITH MINUS SIGN
    { 0x2A42, Infix, 4, 4, 0}, // UNION WITH OVERBAR
    { 0x2A43, Infix, 4, 4, 0}, // INTERSECTION WITH OVERBAR
    { 0x2A44, Infix, 4, 4, 0}, // INTERSECTION WITH LOGICAL AND
    { 0x2A45, Infix, 4, 4, 0}, // UNION WITH LOGICAL OR
    { 0x2A46, Infix, 4, 4, 0}, // UNION ABOVE INTERSECTION
    { 0x2A47, Infix, 4, 4, 0}, // INTERSECTION ABOVE UNION
    { 0x2A48, Infix, 4, 4, 0}, // UNION ABOVE BAR ABOVE INTERSECTION
    { 0x2A49, Infix, 4, 4, 0}, // INTERSECTION ABOVE BAR ABOVE UNION
    { 0x2A4A, Infix, 4, 4, 0}, // UNION BESIDE AND JOINED WITH UNION
    { 0x2A4B, Infix, 4, 4, 0}, // INTERSECTION BESIDE AND JOINED WITH INTERSECTION
    { 0x2A4C, Infix, 4, 4, 0}, // CLOSED UNION WITH SERIFS
    { 0x2A4D, Infix, 4, 4, 0}, // CLOSED INTERSECTION WITH SERIFS
    { 0x2A4E, Infix, 4, 4, 0}, // DOUBLE SQUARE INTERSECTION
    { 0x2A4F, Infix, 4, 4, 0}, // DOUBLE SQUARE UNION
    { 0x2A50, Infix, 4, 4, 0}, // CLOSED UNION WITH SERIFS AND SMASH PRODUCT
    { 0x2A51, Infix, 4, 4, 0}, // LOGICAL AND WITH DOT ABOVE
    { 0x2A52, Infix, 4, 4, 0}, // LOGICAL OR WITH DOT ABOVE
    { 0x2A53, Infix, 4, 4, 0}, // DOUBLE LOGICAL AND
    { 0x2A54, Infix, 4, 4, 0}, // DOUBLE LOGICAL OR
    { 0x2A55, Infix, 4, 4, 0}, // TWO INTERSECTING LOGICAL AND
    { 0x2A56, Infix, 4, 4, 0}, // TWO INTERSECTING LOGICAL OR
    { 0x2A57, Infix, 4, 4, 0}, // SLOPING LARGE OR
    { 0x2A58, Infix, 4, 4, 0}, // SLOPING LARGE AND
    { 0x2A59, Infix, 5, 5, 0}, // LOGICAL OR OVERLAPPING LOGICAL AND
    { 0x2A5A, Infix, 4, 4, 0}, // LOGICAL AND WITH MIDDLE STEM
    { 0x2A5B, Infix, 4, 4, 0}, // LOGICAL OR WITH MIDDLE STEM
    { 0x2A5C, Infix, 4, 4, 0}, // LOGICAL AND WITH HORIZONTAL DASH
    { 0x2A5D, Infix, 4, 4, 0}, // LOGICAL OR WITH HORIZONTAL DASH
    { 0x2A5E, Infix, 4, 4, 0}, // LOGICAL AND WITH DOUBLE OVERBAR
    { 0x2A5F, Infix, 4, 4, 0}, // LOGICAL AND WITH UNDERBAR
    { 0x2A60, Infix, 4, 4, 0}, // LOGICAL AND WITH DOUBLE UNDERBAR
    { 0x2A61, Infix, 4, 4, 0}, // SMALL VEE WITH UNDERBAR
    { 0x2A62, Infix, 4, 4, 0}, // LOGICAL OR WITH DOUBLE OVERBAR
    { 0x2A63, Infix, 4, 4, 0}, // LOGICAL OR WITH DOUBLE UNDERBAR
    { 0x2A64, Infix, 4, 4, 0}, // Z NOTATION DOMAIN ANTIRESTRICTION
    { 0x2A65, Infix, 4, 4, 0}, // Z NOTATION RANGE ANTIRESTRICTION
    { 0x2A66, Infix, 5, 5, 0}, // EQUALS SIGN WITH DOT BELOW
    { 0x2A67, Infix, 5, 5, 0}, // IDENTICAL WITH DOT ABOVE
    { 0x2A68, Infix, 5, 5, 0}, // TRIPLE HORIZONTAL BAR WITH DOUBLE VERTICAL STROKE
    { 0x2A69, Infix, 5, 5, 0}, // TRIPLE HORIZONTAL BAR WITH TRIPLE VERTICAL STROKE
    { 0x2A6A, Infix, 5, 5, 0}, // TILDE OPERATOR WITH DOT ABOVE
    { 0x2A6B, Infix, 5, 5, 0}, // TILDE OPERATOR WITH RISING DOTS
    { 0x2A6C, Infix, 5, 5, 0}, // SIMILAR MINUS SIMILAR
    { 0x2A6D, Infix, 5, 5, 0}, // CONGRUENT WITH DOT ABOVE
    { 0x2A6E, Infix, 5, 5, 0}, // EQUALS WITH ASTERISK
    { 0x2A6F, Infix, 5, 5, 0}, // ALMOST EQUAL TO WITH CIRCUMFLEX ACCENT
    { 0x2A70, Infix, 5, 5, 0}, // APPROXIMATELY EQUAL OR EQUAL TO
    { 0x2A71, Infix, 4, 4, 0}, // EQUALS SIGN ABOVE PLUS SIGN
    { 0x2A72, Infix, 4, 4, 0}, // PLUS SIGN ABOVE EQUALS SIGN
    { 0x2A73, Infix, 5, 5, 0}, // EQUALS SIGN ABOVE TILDE OPERATOR
    { 0x2A74, Infix, 5, 5, 0}, // DOUBLE COLON EQUAL
    { 0x2A75, Infix, 5, 5, 0}, // TWO CONSECUTIVE EQUALS SIGNS
    { 0x2A76, Infix, 5, 5, 0}, // THREE CONSECUTIVE EQUALS SIGNS
    { 0x2A77, Infix, 5, 5, 0}, // EQUALS SIGN WITH TWO DOTS ABOVE AND TWO DOTS BELOW
    { 0x2A78, Infix, 5, 5, 0}, // EQUIVALENT WITH FOUR DOTS ABOVE
    { 0x2A79, Infix, 5, 5, 0}, // LESS-THAN WITH CIRCLE INSIDE
    { 0x2A7A, Infix, 5, 5, 0}, // GREATER-THAN WITH CIRCLE INSIDE
    { 0x2A7B, Infix, 5, 5, 0}, // LESS-THAN WITH QUESTION MARK ABOVE
    { 0x2A7C, Infix, 5, 5, 0}, // GREATER-THAN WITH QUESTION MARK ABOVE
    { 0x2A7D, Infix, 5, 5, 0}, // LESS-THAN OR SLANTED EQUAL TO
    { 0x2A7E, Infix, 5, 5, 0}, // GREATER-THAN OR SLANTED EQUAL TO
    { 0x2A7F, Infix, 5, 5, 0}, // LESS-THAN OR SLANTED EQUAL TO WITH DOT INSIDE
    { 0x2A80, Infix, 5, 5, 0}, // GREATER-THAN OR SLANTED EQUAL TO WITH DOT INSIDE
    { 0x2A81, Infix, 5, 5, 0}, // LESS-THAN OR SLANTED EQUAL TO WITH DOT ABOVE
    { 0x2A82, Infix, 5, 5, 0}, // GREATER-THAN OR SLANTED EQUAL TO WITH DOT ABOVE
    { 0x2A83, Infix, 5, 5, 0}, // LESS-THAN OR SLANTED EQUAL TO WITH DOT ABOVE RIGHT
    { 0x2A84, Infix, 5, 5, 0}, // GREATER-THAN OR SLANTED EQUAL TO WITH DOT ABOVE LEFT
    { 0x2A85, Infix, 5, 5, 0}, // LESS-THAN OR APPROXIMATE
    { 0x2A86, Infix, 5, 5, 0}, // GREATER-THAN OR APPROXIMATE
    { 0x2A87, Infix, 5, 5, 0}, // LESS-THAN AND SINGLE-LINE NOT EQUAL TO
    { 0x2A88, Infix, 5, 5, 0}, // GREATER-THAN AND SINGLE-LINE NOT EQUAL TO
    { 0x2A89, Infix, 5, 5, 0}, // LESS-THAN AND NOT APPROXIMATE
    { 0x2A8A, Infix, 5, 5, 0}, // GREATER-THAN AND NOT APPROXIMATE
    { 0x2A8B, Infix, 5, 5, 0}, // LESS-THAN ABOVE DOUBLE-LINE EQUAL ABOVE GREATER-THAN
    { 0x2A8C, Infix, 5, 5, 0}, // GREATER-THAN ABOVE DOUBLE-LINE EQUAL ABOVE LESS-THAN
    { 0x2A8D, Infix, 5, 5, 0}, // LESS-THAN ABOVE SIMILAR OR EQUAL
    { 0x2A8E, Infix, 5, 5, 0}, // GREATER-THAN ABOVE SIMILAR OR EQUAL
    { 0x2A8F, Infix, 5, 5, 0}, // LESS-THAN ABOVE SIMILAR ABOVE GREATER-THAN
    { 0x2A90, Infix, 5, 5, 0}, // GREATER-THAN ABOVE SIMILAR ABOVE LESS-THAN
    { 0x2A91, Infix, 5, 5, 0}, // LESS-THAN ABOVE GREATER-THAN ABOVE DOUBLE-LINE EQUAL
    { 0x2A92, Infix, 5, 5, 0}, // GREATER-THAN ABOVE LESS-THAN ABOVE DOUBLE-LINE EQUAL
    { 0x2A93, Infix, 5, 5, 0}, // LESS-THAN ABOVE SLANTED EQUAL ABOVE GREATER-THAN ABOVE SLANTED EQUAL
    { 0x2A94, Infix, 5, 5, 0}, // GREATER-THAN ABOVE SLANTED EQUAL ABOVE LESS-THAN ABOVE SLANTED EQUAL
    { 0x2A95, Infix, 5, 5, 0}, // SLANTED EQUAL TO OR LESS-THAN
    { 0x2A96, Infix, 5, 5, 0}, // SLANTED EQUAL TO OR GREATER-THAN
    { 0x2A97, Infix, 5, 5, 0}, // SLANTED EQUAL TO OR LESS-THAN WITH DOT INSIDE
    { 0x2A98, Infix, 5, 5, 0}, // SLANTED EQUAL TO OR GREATER-THAN WITH DOT INSIDE
    { 0x2A99, Infix, 5, 5, 0}, // DOUBLE-LINE EQUAL TO OR LESS-THAN
    { 0x2A9A, Infix, 5, 5, 0}, // DOUBLE-LINE EQUAL TO OR GREATER-THAN
    { 0x2A9B, Infix, 5, 5, 0}, // DOUBLE-LINE SLANTED EQUAL TO OR LESS-THAN
    { 0x2A9C, Infix, 5, 5, 0}, // DOUBLE-LINE SLANTED EQUAL TO OR GREATER-THAN
    { 0x2A9D, Infix, 5, 5, 0}, // SIMILAR OR LESS-THAN
    { 0x2A9E, Infix, 5, 5, 0}, // SIMILAR OR GREATER-THAN
    { 0x2A9F, Infix, 5, 5, 0}, // SIMILAR ABOVE LESS-THAN ABOVE EQUALS SIGN
    { 0x2AA0, Infix, 5, 5, 0}, // SIMILAR ABOVE GREATER-THAN ABOVE EQUALS SIGN
    { 0x2AA1, Infix, 5, 5, 0}, // DOUBLE NESTED LESS-THAN
    { 0x2AA2, Infix, 5, 5, 0}, // DOUBLE NESTED GREATER-THAN
    { 0x2AA3, Infix, 5, 5, 0}, // DOUBLE NESTED LESS-THAN WITH UNDERBAR
    { 0x2AA4, Infix, 5, 5, 0}, // GREATER-THAN OVERLAPPING LESS-THAN
    { 0x2AA5, Infix, 5, 5, 0}, // GREATER-THAN BESIDE LESS-THAN
    { 0x2AA6, Infix, 5, 5, 0}, // LESS-THAN CLOSED BY CURVE
    { 0x2AA7, Infix, 5, 5, 0}, // GREATER-THAN CLOSED BY CURVE
    { 0x2AA8, Infix, 5, 5, 0}, // LESS-THAN CLOSED BY CURVE ABOVE SLANTED EQUAL
    { 0x2AA9, Infix, 5, 5, 0}, // GREATER-THAN CLOSED BY CURVE ABOVE SLANTED EQUAL
    { 0x2AAA, Infix, 5, 5, 0}, // SMALLER THAN
    { 0x2AAB, Infix, 5, 5, 0}, // LARGER THAN
    { 0x2AAC, Infix, 5, 5, 0}, // SMALLER THAN OR EQUAL TO
    { 0x2AAD, Infix, 5, 5, 0}, // LARGER THAN OR EQUAL TO
    { 0x2AAE, Infix, 5, 5, 0}, // EQUALS SIGN WITH BUMPY ABOVE
    { 0x2AAF, Infix, 5, 5, 0}, // PRECEDES ABOVE SINGLE-LINE EQUALS SIGN
    { 0x2AB0, Infix, 5, 5, 0}, // SUCCEEDS ABOVE SINGLE-LINE EQUALS SIGN
    { 0x2AB1, Infix, 5, 5, 0}, // PRECEDES ABOVE SINGLE-LINE NOT EQUAL TO
    { 0x2AB2, Infix, 5, 5, 0}, // SUCCEEDS ABOVE SINGLE-LINE NOT EQUAL TO
    { 0x2AB3, Infix, 5, 5, 0}, // PRECEDES ABOVE EQUALS SIGN
    { 0x2AB4, Infix, 5, 5, 0}, // SUCCEEDS ABOVE EQUALS SIGN
    { 0x2AB5, Infix, 5, 5, 0}, // PRECEDES ABOVE NOT EQUAL TO
    { 0x2AB6, Infix, 5, 5, 0}, // SUCCEEDS ABOVE NOT EQUAL TO
    { 0x2AB7, Infix, 5, 5, 0}, // PRECEDES ABOVE ALMOST EQUAL TO
    { 0x2AB8, Infix, 5, 5, 0}, // SUCCEEDS ABOVE ALMOST EQUAL TO
    { 0x2AB9, Infix, 5, 5, 0}, // PRECEDES ABOVE NOT ALMOST EQUAL TO
    { 0x2ABA, Infix, 5, 5, 0}, // SUCCEEDS ABOVE NOT ALMOST EQUAL TO
    { 0x2ABB, Infix, 5, 5, 0}, // DOUBLE PRECEDES
    { 0x2ABC, Infix, 5, 5, 0}, // DOUBLE SUCCEEDS
    { 0x2ABD, Infix, 5, 5, 0}, // SUBSET WITH DOT
    { 0x2ABE, Infix, 5, 5, 0}, // SUPERSET WITH DOT
    { 0x2ABF, Infix, 5, 5, 0}, // SUBSET WITH PLUS SIGN BELOW
    { 0x2AC0, Infix, 5, 5, 0}, // SUPERSET WITH PLUS SIGN BELOW
    { 0x2AC1, Infix, 5, 5, 0}, // SUBSET WITH MULTIPLICATION SIGN BELOW
    { 0x2AC2, Infix, 5, 5, 0}, // SUPERSET WITH MULTIPLICATION SIGN BELOW
    { 0x2AC3, Infix, 5, 5, 0}, // SUBSET OF OR EQUAL TO WITH DOT ABOVE
    { 0x2AC4, Infix, 5, 5, 0}, // SUPERSET OF OR EQUAL TO WITH DOT ABOVE
    { 0x2AC5, Infix, 5, 5, 0}, // SUBSET OF ABOVE EQUALS SIGN
    { 0x2AC6, Infix, 5, 5, 0}, // SUPERSET OF ABOVE EQUALS SIGN
    { 0x2AC7, Infix, 5, 5, 0}, // SUBSET OF ABOVE TILDE OPERATOR
    { 0x2AC8, Infix, 5, 5, 0}, // SUPERSET OF ABOVE TILDE OPERATOR
    { 0x2AC9, Infix, 5, 5, 0}, // SUBSET OF ABOVE ALMOST EQUAL TO
    { 0x2ACA, Infix, 5, 5, 0}, // SUPERSET OF ABOVE ALMOST EQUAL TO
    { 0x2ACB, Infix, 5, 5, 0}, // SUBSET OF ABOVE NOT EQUAL TO
    { 0x2ACC, Infix, 5, 5, 0}, // SUPERSET OF ABOVE NOT EQUAL TO
    { 0x2ACD, Infix, 5, 5, 0}, // SQUARE LEFT OPEN BOX OPERATOR
    { 0x2ACE, Infix, 5, 5, 0}, // SQUARE RIGHT OPEN BOX OPERATOR
    { 0x2ACF, Infix, 5, 5, 0}, // CLOSED SUBSET
    { 0x2AD0, Infix, 5, 5, 0}, // CLOSED SUPERSET
    { 0x2AD1, Infix, 5, 5, 0}, // CLOSED SUBSET OR EQUAL TO
    { 0x2AD2, Infix, 5, 5, 0}, // CLOSED SUPERSET OR EQUAL TO
    { 0x2AD3, Infix, 5, 5, 0}, // SUBSET ABOVE SUPERSET
    { 0x2AD4, Infix, 5, 5, 0}, // SUPERSET ABOVE SUBSET
    { 0x2AD5, Infix, 5, 5, 0}, // SUBSET ABOVE SUBSET
    { 0x2AD6, Infix, 5, 5, 0}, // SUPERSET ABOVE SUPERSET
    { 0x2AD7, Infix, 5, 5, 0}, // SUPERSET BESIDE SUBSET
    { 0x2AD8, Infix, 5, 5, 0}, // SUPERSET BESIDE AND JOINED BY DASH WITH SUBSET
    { 0x2AD9, Infix, 5, 5, 0}, // ELEMENT OF OPENING DOWNWARDS
    { 0x2ADA, Infix, 5, 5, 0}, // PITCHFORK WITH TEE TOP
    { 0x2ADB, Infix, 5, 5, 0}, // TRANSVERSAL INTERSECTION
    { 0x2ADD, Infix, 5, 5, 0}, // NONFORKING
    { 0x2ADE, Infix, 5, 5, 0}, // SHORT LEFT TACK
    { 0x2ADF, Infix, 5, 5, 0}, // SHORT DOWN TACK
    { 0x2AE0, Infix, 5, 5, 0}, // SHORT UP TACK
    { 0x2AE1, Infix, 5, 5, 0}, // PERPENDICULAR WITH S
    { 0x2AE2, Infix, 5, 5, 0}, // VERTICAL BAR TRIPLE RIGHT TURNSTILE
    { 0x2AE3, Infix, 5, 5, 0}, // DOUBLE VERTICAL BAR LEFT TURNSTILE
    { 0x2AE4, Infix, 5, 5, 0}, // VERTICAL BAR DOUBLE LEFT TURNSTILE
    { 0x2AE5, Infix, 5, 5, 0}, // DOUBLE VERTICAL BAR DOUBLE LEFT TURNSTILE
    { 0x2AE6, Infix, 5, 5, 0}, // LONG DASH FROM LEFT MEMBER OF DOUBLE VERTICAL
    { 0x2AE7, Infix, 5, 5, 0}, // SHORT DOWN TACK WITH OVERBAR
    { 0x2AE8, Infix, 5, 5, 0}, // SHORT UP TACK WITH UNDERBAR
    { 0x2AE9, Infix, 5, 5, 0}, // SHORT UP TACK ABOVE SHORT DOWN TACK
    { 0x2AEA, Infix, 5, 5, 0}, // DOUBLE DOWN TACK
    { 0x2AEB, Infix, 5, 5, 0}, // DOUBLE UP TACK
    { 0x2AEC, Infix, 5, 5, 0}, // DOUBLE STROKE NOT SIGN
    { 0x2AED, Infix, 5, 5, 0}, // REVERSED DOUBLE STROKE NOT SIGN
    { 0x2AEE, Infix, 5, 5, 0}, // DOES NOT DIVIDE WITH REVERSED NEGATION SLASH
    { 0x2AEF, Infix, 5, 5, 0}, // VERTICAL LINE WITH CIRCLE ABOVE
    { 0x2AF0, Infix, 5, 5, 0}, // VERTICAL LINE WITH CIRCLE BELOW
    { 0x2AF1, Infix, 5, 5, 0}, // DOWN TACK WITH CIRCLE BELOW
    { 0x2AF2, Infix, 5, 5, 0}, // PARALLEL WITH HORIZONTAL STROKE
    { 0x2AF3, Infix, 5, 5, 0}, // PARALLEL WITH TILDE OPERATOR
    { 0x2AF4, Infix, 4, 4, 0}, // TRIPLE VERTICAL BAR BINARY RELATION
    { 0x2AF5, Infix, 4, 4, 0}, // TRIPLE VERTICAL BAR WITH HORIZONTAL STROKE
    { 0x2AF6, Infix, 4, 4, 0}, // TRIPLE COLON OPERATOR
    { 0x2AF7, Infix, 5, 5, 0}, // TRIPLE NESTED LESS-THAN
    { 0x2AF8, Infix, 5, 5, 0}, // TRIPLE NESTED GREATER-THAN
    { 0x2AF9, Infix, 5, 5, 0}, // DOUBLE-LINE SLANTED LESS-THAN OR EQUAL TO
    { 0x2AFA, Infix, 5, 5, 0}, // DOUBLE-LINE SLANTED GREATER-THAN OR EQUAL TO
    { 0x2AFB, Infix, 4, 4, 0}, // TRIPLE SOLIDUS BINARY RELATION
    { 0x2AFC, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // LARGE TRIPLE VERTICAL BAR OPERATOR
    { 0x2AFD, Infix, 4, 4, 0}, // DOUBLE SOLIDUS OPERATOR
    { 0x2AFE, Infix, 3, 3, 0}, // WHITE VERTICAL BAR
    { 0x2AFF, Prefix, 1, 2, Symmetric | LargeOp | MovableLimits}, // N-ARY WHITE VERTICAL BAR
    { 0x2B45, Infix, 5, 5, Stretchy}, // LEFTWARDS QUADRUPLE ARROW
    { 0x2B46, Infix, 5, 5, Stretchy} // RIGHTWARDS QUADRUPLE ARROW
};

}

RenderMathMLOperator::RenderMathMLOperator(MathMLElement& element, PassRef<RenderStyle> style)
    : RenderMathMLToken(element, std::move(style))
    , m_stretchHeightAboveBaseline(0)
    , m_stretchDepthBelowBaseline(0)
    , m_operator(0)
{
    updateTokenContent();
}

RenderMathMLOperator::RenderMathMLOperator(Document& document, PassRef<RenderStyle> style, const String& operatorString, MathMLOperatorDictionary::Form form, MathMLOperatorDictionary::Flag flag)
    : RenderMathMLToken(document, std::move(style))
    , m_stretchHeightAboveBaseline(0)
    , m_stretchDepthBelowBaseline(0)
    , m_operator(0)
    , m_operatorForm(form)
    , m_operatorFlags(flag)
{
    updateTokenContent(operatorString);
}

void RenderMathMLOperator::setOperatorFlagFromAttribute(MathMLOperatorDictionary::Flag flag, const QualifiedName& name)
{
    ASSERT(!isFencedOperator());
    const AtomicString& attributeValue = element().fastGetAttribute(name);
    if (attributeValue == "true")
        m_operatorFlags |= flag;
    else if (attributeValue == "false")
        m_operatorFlags &= ~flag;
    // We ignore absent or invalid attributes.
}

void RenderMathMLOperator::setOperatorPropertiesFromOpDictEntry(const MathMLOperatorDictionary::Entry* entry)
{
    // If this operator has been created by RenderMathMLFenced, we preserve the Fence and Separator properties.
    if (isFencedOperator())
        m_operatorFlags = (m_operatorFlags & (MathMLOperatorDictionary::Fence | MathMLOperatorDictionary::Separator)) | entry->flags;
    else
        m_operatorFlags = entry->flags;

    // Leading and trailing space is specified as multiple of 1/18em.
    m_leadingSpace = entry->lspace * style().font().size() / 18;
    m_trailingSpace = entry->rspace * style().font().size() / 18;
}

void RenderMathMLOperator::SetOperatorProperties()
{
    // We determine the form of the operator.
    bool explicitForm = true;
    if (!isFencedOperator()) {
        const AtomicString& form = element().fastGetAttribute(MathMLNames::formAttr);
        if (form == "prefix")
            m_operatorForm = MathMLOperatorDictionary::Prefix;
        else if (form == "infix")
            m_operatorForm = MathMLOperatorDictionary::Infix;
        else if (form == "postfix")
            m_operatorForm = MathMLOperatorDictionary::Postfix;
        else {
            // FIXME: We should use more advanced heuristics indicated in the specification to determine the operator form (https://bugs.webkit.org/show_bug.cgi?id=124829).
            explicitForm = false;
            if (!element().previousSibling() && element().nextSibling())
                m_operatorForm = MathMLOperatorDictionary::Prefix;
            else if (element().previousSibling() && !element().nextSibling())
                m_operatorForm = MathMLOperatorDictionary::Postfix;
            else
                m_operatorForm = MathMLOperatorDictionary::Infix;
        }
    }

    // We determine the default values of the operator properties.

    // First we initialize with the default values for unknown operators.
    if (isFencedOperator())
        m_operatorFlags &= MathMLOperatorDictionary::Fence | MathMLOperatorDictionary::Separator; // This resets all but the Fence and Separator properties.
    else
        m_operatorFlags = 0; // This resets all the operator properties.
    m_leadingSpace = 5 * style().font().size() / 18; // This sets leading space to "thickmathspace".
    m_trailingSpace = 5 * style().font().size() / 18; // This sets trailing space to "thickmathspace".
    m_minSize = style().font().size(); // This sets minsize to "1em".
    m_maxSize = intMaxForLayoutUnit; // This sets maxsize to "infinity".

    if (m_operator) {
        // Then we try to find the default values from the operator dictionary.
        if (const MathMLOperatorDictionary::Entry* entry = tryBinarySearch<const MathMLOperatorDictionary::Entry, MathMLOperatorDictionary::Key>(MathMLOperatorDictionary::dictionary, MATHML_OPDICT_SIZE, MathMLOperatorDictionary::Key(m_operator, m_operatorForm), MathMLOperatorDictionary::ExtractKey))
            setOperatorPropertiesFromOpDictEntry(entry);
        else if (!explicitForm) {
            // If we did not find the desired operator form and if it was not set explicitely, we use the first one in the following order: Infix, Prefix, Postfix.
            // This is to handle bad MathML markup without explicit <mrow> delimiters like "<mo>(</mo><mi>a</mi><mo>)</mo><mo>(</mo><mi>b</mi><mo>)</mo>" where the inner parenthesis should not be considered infix.
            if (const MathMLOperatorDictionary::Entry* entry = tryBinarySearch<const MathMLOperatorDictionary::Entry, UChar>(MathMLOperatorDictionary::dictionary, MATHML_OPDICT_SIZE, m_operator, MathMLOperatorDictionary::ExtractChar)) {
                // If the previous entry is another form for that operator, we move to that entry. Note that it only remains at most two forms so we don't need to move any further.
                if (entry != MathMLOperatorDictionary::dictionary && (entry-1)->character == m_operator)
                    entry--;
                m_operatorForm = entry->form; // We override the form previously determined.
                setOperatorPropertiesFromOpDictEntry(entry);
            }
        }
    }
#undef MATHML_OPDICT_SIZE

    if (!isFencedOperator()) {
        // Finally, we make the attribute values override the default.

        setOperatorFlagFromAttribute(MathMLOperatorDictionary::Fence, MathMLNames::fenceAttr);
        setOperatorFlagFromAttribute(MathMLOperatorDictionary::Separator, MathMLNames::separatorAttr);
        setOperatorFlagFromAttribute(MathMLOperatorDictionary::Stretchy, MathMLNames::stretchyAttr);
        setOperatorFlagFromAttribute(MathMLOperatorDictionary::Symmetric, MathMLNames::symmetricAttr);
        setOperatorFlagFromAttribute(MathMLOperatorDictionary::LargeOp, MathMLNames::largeopAttr);
        setOperatorFlagFromAttribute(MathMLOperatorDictionary::MovableLimits, MathMLNames::movablelimitsAttr);
        setOperatorFlagFromAttribute(MathMLOperatorDictionary::Accent, MathMLNames::accentAttr);

        parseMathMLLength(element().fastGetAttribute(MathMLNames::lspaceAttr), m_leadingSpace, &style(), false); // FIXME: Negative leading space must be implemented (https://bugs.webkit.org/show_bug.cgi?id=124830).
        parseMathMLLength(element().fastGetAttribute(MathMLNames::rspaceAttr), m_trailingSpace, &style(), false); // FIXME: Negative trailing space must be implemented (https://bugs.webkit.org/show_bug.cgi?id=124830).

        parseMathMLLength(element().fastGetAttribute(MathMLNames::minsizeAttr), m_minSize, &style(), false);
        const AtomicString& maxsize = element().fastGetAttribute(MathMLNames::maxsizeAttr);
        if (maxsize != "infinity")
            parseMathMLLength(maxsize, m_maxSize, &style(), false);
    }
}

bool RenderMathMLOperator::isChildAllowed(const RenderObject&, const RenderStyle&) const
{
    return false;
}

void RenderMathMLOperator::stretchTo(LayoutUnit heightAboveBaseline, LayoutUnit depthBelowBaseline)
{
    if (heightAboveBaseline == m_stretchHeightAboveBaseline && depthBelowBaseline == m_stretchDepthBelowBaseline)
        return;

    m_stretchHeightAboveBaseline = heightAboveBaseline;
    m_stretchDepthBelowBaseline = depthBelowBaseline;

    SetOperatorProperties();
    if (hasOperatorFlag(MathMLOperatorDictionary::Symmetric)) {
        // We make the operator stretch symmetrically above and below the axis.
        // FIXME: We should read the axis from the MATH table (https://bugs.webkit.org/show_bug.cgi?id=122297). For now, we use the same value as in RenderMathMLFraction::firstLineBaseline().
        LayoutUnit axis = style().fontMetrics().xHeight() / 2;
        LayoutUnit halfStretchSize = std::max(m_stretchHeightAboveBaseline - axis, m_stretchDepthBelowBaseline + axis);
        m_stretchHeightAboveBaseline = halfStretchSize + axis;
        m_stretchDepthBelowBaseline = halfStretchSize - axis;
    }
    // We try to honor the minsize/maxsize condition by increasing or decreasing both height and depth proportionately.
    // The MathML specification does not indicate what to do when maxsize < minsize, so we follow Gecko and make minsize take precedence.
    LayoutUnit size = stretchSize();
    float aspect = 1.0;
    if (size > 0) {
        if (size < m_minSize)
            aspect = float(m_minSize) / size;
        else if (m_maxSize < size)
            aspect = float(m_maxSize) / size;
    }
    m_stretchHeightAboveBaseline *= aspect;
    m_stretchDepthBelowBaseline *= aspect;
    updateStyle();
}

FloatRect RenderMathMLOperator::boundsForGlyph(const GlyphData& data)
{
    return data.fontData->boundsForGlyph(data.glyph);
}

float RenderMathMLOperator::heightForGlyph(const GlyphData& data)
{
    return boundsForGlyph(data).height();
}

float RenderMathMLOperator::advanceForGlyph(const GlyphData& data)
{
    return data.fontData->widthForGlyph(data.glyph);
}

void RenderMathMLOperator::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    SetOperatorProperties();
    UChar stretchedCharacter;
    bool allowStretching = shouldAllowStretching(stretchedCharacter);
    if (!allowStretching) {
        RenderMathMLToken::computePreferredLogicalWidths();
        if (isInvisibleOperator()) {
            // In some fonts, glyphs for invisible operators have nonzero width. Consequently, we subtract that width here to avoid wide gaps.
            GlyphData data = style().font().glyphDataForCharacter(m_operator, false);
            float glyphWidth = advanceForGlyph(data);
            ASSERT(glyphWidth <= m_minPreferredLogicalWidth);
            m_minPreferredLogicalWidth -= glyphWidth;
            m_maxPreferredLogicalWidth -= glyphWidth;
        }
        return;
    }

    GlyphData data = style().font().glyphDataForCharacter(stretchedCharacter, false);
    float maximumGlyphWidth = advanceForGlyph(data);
    findStretchyData(stretchedCharacter, &maximumGlyphWidth);
    m_maxPreferredLogicalWidth = m_minPreferredLogicalWidth = m_leadingSpace + maximumGlyphWidth + m_trailingSpace;
}

void RenderMathMLOperator::rebuildTokenContent(const String& operatorString)
{
    // We collapse the whitespace and replace the hyphens by minus signs.
    AtomicString textContent = operatorString.stripWhiteSpace().simplifyWhiteSpace().replace(hyphenMinus, minusSign).impl();

    // We destroy the wrapper and rebuild it.
    // FIXME: Using this RenderText make the text inaccessible to the dumpAsText/selection code (https://bugs.webkit.org/show_bug.cgi?id=125597).
    if (firstChild())
        toRenderElement(firstChild())->destroy();
    createWrapperIfNeeded();
    RenderPtr<RenderText> text = createRenderer<RenderText>(document(), textContent);
    toRenderElement(firstChild())->addChild(text.leakPtr());

    // We verify whether the operator text can be represented by a single UChar.
    // FIXME: This does not handle surrogate pairs (https://bugs.webkit.org/show_bug.cgi?id=122296).
    // FIXME: This does not handle <mo> operators with multiple characters (https://bugs.webkit.org/show_bug.cgi?id=124828).
    m_operator = textContent.length() == 1 ? textContent[0] : 0;
    SetOperatorProperties();
    updateStyle();
    setNeedsLayoutAndPrefWidthsRecalc();
}

void RenderMathMLOperator::updateTokenContent(const String& operatorString)
{
    ASSERT(isFencedOperator());
    rebuildTokenContent(operatorString);
}

void RenderMathMLOperator::updateTokenContent()
{
    ASSERT(!isFencedOperator());
    rebuildTokenContent(element().textContent());
}

void RenderMathMLOperator::updateFromElement()
{
    SetOperatorProperties();
    RenderMathMLToken::updateFromElement();
}

void RenderMathMLOperator::updateOperatorProperties()
{
    SetOperatorProperties();
    if (!isEmpty())
        updateStyle();
    setNeedsLayoutAndPrefWidthsRecalc();
}

bool RenderMathMLOperator::shouldAllowStretching(UChar& stretchedCharacter)
{
    if (!hasOperatorFlag(MathMLOperatorDictionary::Stretchy))
        return false;

    stretchedCharacter = m_operator;
    return stretchedCharacter;
}

// FIXME: We should also look at alternate characters defined in the OpenType MATH table (http://wkbug/122297).
RenderMathMLOperator::StretchyData RenderMathMLOperator::findStretchyData(UChar character, float* maximumGlyphWidth)
{
    // FIXME: This function should first try size variants.
    StretchyData data;

    const StretchyCharacter* stretchyCharacter = 0;
    const int maxIndex = WTF_ARRAY_LENGTH(stretchyCharacters);
    for (int index = 0; index < maxIndex; ++index) {
        if (stretchyCharacters[index].character == character) {
            stretchyCharacter = &stretchyCharacters[index];
            break;
        }
    }

    // If we didn't find a stretchy character set for this character, we don't know how to stretch it.
    if (!stretchyCharacter)
        return data;

    // We convert the list of Unicode characters into a list of glyph data.
    GlyphData top = style().font().glyphDataForCharacter(stretchyCharacter->topChar, false);
    GlyphData extension = style().font().glyphDataForCharacter(stretchyCharacter->extensionChar, false);
    GlyphData bottom = style().font().glyphDataForCharacter(stretchyCharacter->bottomChar, false);
    GlyphData middle;
    if (stretchyCharacter->middleChar)
        middle = style().font().glyphDataForCharacter(stretchyCharacter->middleChar, false);

    // If we are measuring the maximum width, verify each component.
    if (maximumGlyphWidth) {
        *maximumGlyphWidth = std::max(*maximumGlyphWidth, advanceForGlyph(top));
        *maximumGlyphWidth = std::max(*maximumGlyphWidth, advanceForGlyph(extension));
        if (middle.glyph)
            *maximumGlyphWidth = std::max(*maximumGlyphWidth, advanceForGlyph(middle));
        *maximumGlyphWidth = std::max(*maximumGlyphWidth, advanceForGlyph(bottom));
        return data;
    }

    float height = heightForGlyph(top) + heightForGlyph(bottom);
    if (middle.glyph)
        height += heightForGlyph(middle);
    if (height > stretchSize())
        return data;

    data.setGlyphAssemblyMode(top, extension, bottom, middle);
    return data;
}

void RenderMathMLOperator::updateStyle()
{
    ASSERT(firstChild());
    if (!firstChild())
        return;

    UChar stretchedCharacter;
    bool allowStretching = shouldAllowStretching(stretchedCharacter);

    float stretchedCharacterHeight = style().fontMetrics().floatHeight();
    m_stretchyData.setNormalMode();

    // Sometimes we cannot stretch an operator properly, so in that case, we should just use the original size.
    if (allowStretching && stretchSize() > stretchedCharacterHeight)
        m_stretchyData = findStretchyData(stretchedCharacter, nullptr);

    // We add spacing around the operator.
    // FIXME: The spacing should be added to the whole embellished operator (https://bugs.webkit.org/show_bug.cgi?id=124831).
    // FIXME: The spacing should only be added inside (perhaps inferred) mrow (http://www.w3.org/TR/MathML/chapter3.html#presm.opspacing).
    const auto& wrapper = toRenderElement(firstChild());
    auto newStyle = RenderStyle::createAnonymousStyleWithDisplay(&style(), FLEX);
    newStyle.get().setMarginStart(Length(m_leadingSpace, Fixed));
    newStyle.get().setMarginEnd(Length(m_trailingSpace, Fixed));
    wrapper->setStyle(std::move(newStyle));
    wrapper->setNeedsLayoutAndPrefWidthsRecalc();
}

int RenderMathMLOperator::firstLineBaseline() const
{
    if (m_stretchyData.mode() != DrawNormal)
        return m_stretchHeightAboveBaseline;
    return RenderMathMLToken::firstLineBaseline();
}

void RenderMathMLOperator::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    if (m_stretchyData.mode() != DrawNormal)
        logicalHeight = stretchSize();
    RenderBox::computeLogicalHeight(logicalHeight, logicalTop, computedValues);
}

LayoutRect RenderMathMLOperator::paintGlyph(PaintInfo& info, const GlyphData& data, const LayoutPoint& origin, GlyphPaintTrimming trim)
{
    FloatRect glyphBounds = boundsForGlyph(data);

    LayoutRect glyphPaintRect(origin, LayoutSize(glyphBounds.x() + glyphBounds.width(), glyphBounds.height()));
    glyphPaintRect.setY(origin.y() + glyphBounds.y());

    // In order to have glyphs fit snugly with one another we snap the connecting edges to pixel boundaries
    // and trim off one pixel. The pixel trim is to account for fonts that have edge pixels that have less
    // than full coverage. These edge pixels can introduce small seams between connected glyphs
    FloatRect clipBounds = info.rect;
    switch (trim) {
    case TrimTop:
        glyphPaintRect.shiftYEdgeTo(glyphPaintRect.y().ceil() + 1);
        clipBounds.shiftYEdgeTo(glyphPaintRect.y());
        break;
    case TrimBottom:
        glyphPaintRect.shiftMaxYEdgeTo(glyphPaintRect.maxY().floor() - 1);
        clipBounds.shiftMaxYEdgeTo(glyphPaintRect.maxY());
        break;
    case TrimTopAndBottom:
        LayoutUnit temp = glyphPaintRect.y() + 1;
        glyphPaintRect.shiftYEdgeTo(temp.ceil());
        glyphPaintRect.shiftMaxYEdgeTo(glyphPaintRect.maxY().floor() - 1);
        clipBounds.shiftYEdgeTo(glyphPaintRect.y());
        clipBounds.shiftMaxYEdgeTo(glyphPaintRect.maxY());
        break;
    }

    // Clipping the enclosing IntRect avoids any potential issues at joined edges.
    GraphicsContextStateSaver stateSaver(*info.context);
    info.context->clip(clipBounds);

    GlyphBuffer buffer;
    buffer.add(data.glyph, data.fontData, advanceForGlyph(data));
    info.context->drawGlyphs(style().font(), *data.fontData, buffer, 0, 1, origin);

    return glyphPaintRect;
}

void RenderMathMLOperator::fillWithExtensionGlyph(PaintInfo& info, const LayoutPoint& from, const LayoutPoint& to)
{
    ASSERT(m_stretchyData.mode() == DrawGlyphAssembly);
    ASSERT(m_stretchyData.extension().glyph);
    ASSERT(from.y() <= to.y());

    // If there is no space for the extension glyph, we don't need to do anything.
    if (from.y() == to.y())
        return;

    GraphicsContextStateSaver stateSaver(*info.context);

    FloatRect glyphBounds = boundsForGlyph(m_stretchyData.extension());

    // Clipping the extender region here allows us to draw the bottom extender glyph into the
    // regions of the bottom glyph without worrying about overdraw (hairy pixels) and simplifies later clipping.
    LayoutRect clipBounds = info.rect;
    clipBounds.shiftYEdgeTo(from.y());
    clipBounds.shiftMaxYEdgeTo(to.y());
    info.context->clip(clipBounds);

    // Trimming may remove up to two pixels from the top of the extender glyph, so we move it up by two pixels.
    float offsetToGlyphTop = glyphBounds.y() + 2;
    LayoutPoint glyphOrigin = LayoutPoint(from.x(), from.y() - offsetToGlyphTop);
    FloatRect lastPaintedGlyphRect(from, FloatSize());

    while (lastPaintedGlyphRect.maxY() < to.y()) {
        lastPaintedGlyphRect = paintGlyph(info, m_stretchyData.extension(), glyphOrigin, TrimTopAndBottom);
        glyphOrigin.setY(glyphOrigin.y() + lastPaintedGlyphRect.height());

        // There's a chance that if the font size is small enough the glue glyph has been reduced to an empty rectangle
        // with trimming. In that case we just draw nothing.
        if (lastPaintedGlyphRect.isEmpty())
            break;
    }
}

void RenderMathMLOperator::paint(PaintInfo& info, const LayoutPoint& paintOffset)
{
    RenderMathMLToken::paint(info, paintOffset);

    if (info.context->paintingDisabled() || info.phase != PaintPhaseForeground || style().visibility() != VISIBLE || m_stretchyData.mode() == DrawNormal)
        return;

    // FIXME: This painting should work in RTL mode too (https://bugs.webkit.org/show_bug.cgi?id=123018).

    GraphicsContextStateSaver stateSaver(*info.context);
    info.context->setFillColor(style().visitedDependentColor(CSSPropertyColor), style().colorSpace());

    ASSERT(m_stretchyData.mode() == DrawGlyphAssembly);
    ASSERT(m_stretchyData.top().glyph);
    ASSERT(m_stretchyData.bottom().glyph);

    // We are positioning the glyphs so that the edge of the tight glyph bounds line up exactly with the edges of our paint box.
    LayoutPoint operatorTopLeft = paintOffset + location();
    operatorTopLeft.move(m_leadingSpace, 0);
    operatorTopLeft = ceiledIntPoint(operatorTopLeft);
    FloatRect topGlyphBounds = boundsForGlyph(m_stretchyData.top());
    LayoutPoint topGlyphOrigin(operatorTopLeft.x(), operatorTopLeft.y() - topGlyphBounds.y());
    LayoutRect topGlyphPaintRect = paintGlyph(info, m_stretchyData.top(), topGlyphOrigin, TrimBottom);

    FloatRect bottomGlyphBounds = boundsForGlyph(m_stretchyData.bottom());
    LayoutPoint bottomGlyphOrigin(operatorTopLeft.x(), operatorTopLeft.y() + offsetHeight() - (bottomGlyphBounds.height() + bottomGlyphBounds.y()));
    LayoutRect bottomGlyphPaintRect = paintGlyph(info, m_stretchyData.bottom(), bottomGlyphOrigin, TrimTop);

    if (m_stretchyData.middle().glyph) {
        // Center the glyph origin between the start and end glyph paint extents. Then shift it half the paint height toward the bottom glyph.
        FloatRect middleGlyphBounds = boundsForGlyph(m_stretchyData.middle());
        LayoutPoint middleGlyphOrigin(operatorTopLeft.x(), topGlyphOrigin.y());
        middleGlyphOrigin.moveBy(LayoutPoint(0, (bottomGlyphPaintRect.y() - topGlyphPaintRect.maxY()) / 2.0));
        middleGlyphOrigin.moveBy(LayoutPoint(0, middleGlyphBounds.height() / 2.0));

        LayoutRect middleGlyphPaintRect = paintGlyph(info, m_stretchyData.middle(), middleGlyphOrigin, TrimTopAndBottom);
        fillWithExtensionGlyph(info, topGlyphPaintRect.minXMaxYCorner(), middleGlyphPaintRect.minXMinYCorner());
        fillWithExtensionGlyph(info, middleGlyphPaintRect.minXMaxYCorner(), bottomGlyphPaintRect.minXMinYCorner());
    } else
        fillWithExtensionGlyph(info, topGlyphPaintRect.minXMaxYCorner(), bottomGlyphPaintRect.minXMinYCorner());
}

void RenderMathMLOperator::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect)
{
    // We skip painting for invisible operators too to avoid some "missing character" glyph to appear if appropriate math fonts are not available.
    if (m_stretchyData.mode() != DrawNormal || isInvisibleOperator())
        return;
    RenderMathMLToken::paintChildren(paintInfo, paintOffset, paintInfoForChild, usePrintRect);
}
    
}

#endif
