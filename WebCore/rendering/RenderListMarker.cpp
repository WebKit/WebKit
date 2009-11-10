/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
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

#include "config.h"
#include "RenderListMarker.h"

#include "CachedImage.h"
#include "CharacterNames.h"
#include "Document.h"
#include "GraphicsContext.h"
#include "RenderLayer.h"
#include "RenderListItem.h"
#include "RenderView.h"

using namespace std;
using namespace WTF;
using namespace Unicode;

namespace WebCore {

const int cMarkerPadding = 7;

static String toRoman(int number, bool upper)
{
    // FIXME: CSS3 describes how to make this work for much larger numbers,
    // using overbars and special characters. It also specifies the characters
    // in the range U+2160 to U+217F instead of standard ASCII ones.
    if (number < 1 || number > 3999)
        return String::number(number);

    const int lettersSize = 12; // big enough for three each of I, X, C, and M
    UChar letters[lettersSize];

    int length = 0;
    const UChar ldigits[] = { 'i', 'v', 'x', 'l', 'c', 'd', 'm' };
    const UChar udigits[] = { 'I', 'V', 'X', 'L', 'C', 'D', 'M' };
    const UChar* digits = upper ? udigits : ldigits;
    int d = 0;
    do {
        int num = number % 10;
        if (num % 5 < 4)
            for (int i = num % 5; i > 0; i--)
                letters[lettersSize - ++length] = digits[d];
        if (num >= 4 && num <= 8)
            letters[lettersSize - ++length] = digits[d + 1];
        if (num == 9)
            letters[lettersSize - ++length] = digits[d + 2];
        if (num % 5 == 4)
            letters[lettersSize - ++length] = digits[d];
        number /= 10;
        d += 2;
    } while (number);

    ASSERT(length <= lettersSize);
    return String(&letters[lettersSize - length], length);
}

static String toAlphabetic(int number, const UChar* alphabet, int alphabetSize)
{
    ASSERT(alphabetSize >= 10);

    if (number < 1)
        return String::number(number);

    const int lettersSize = 10; // big enough for a 32-bit int, with a 10-letter alphabet
    UChar letters[lettersSize];

    --number;
    letters[lettersSize - 1] = alphabet[number % alphabetSize];
    int length = 1;
    while ((number /= alphabetSize) > 0)
        letters[lettersSize - ++length] = alphabet[number % alphabetSize - 1];

    ASSERT(length <= lettersSize);
    return String(&letters[lettersSize - length], length);
}

static int toHebrewUnder1000(int number, UChar letters[5])
{
    // FIXME: CSS3 mentions various refinements not implemented here.
    // FIXME: Should take a look at Mozilla's HebrewToText function (in nsBulletFrame).
    ASSERT(number >= 0 && number < 1000);
    int length = 0;
    int fourHundreds = number / 400;
    for (int i = 0; i < fourHundreds; i++)
        letters[length++] = 1511 + 3;
    number %= 400;
    if (number / 100)
        letters[length++] = 1511 + (number / 100) - 1;
    number %= 100;
    if (number == 15 || number == 16) {
        letters[length++] = 1487 + 9;
        letters[length++] = 1487 + number - 9;
    } else {
        if (int tens = number / 10) {
            static const UChar hebrewTens[9] = { 1497, 1499, 1500, 1502, 1504, 1505, 1506, 1508, 1510 };
            letters[length++] = hebrewTens[tens - 1];
        }
        if (int ones = number % 10)
            letters[length++] = 1487 + ones;
    }
    ASSERT(length <= 5);
    return length;
}

static String toHebrew(int number)
{
    // FIXME: CSS3 mentions ways to make this work for much larger numbers.
    if (number < 0 || number > 999999)
        return String::number(number);

    if (number == 0) {
        static const UChar hebrewZero[3] = { 0x05D0, 0x05E4, 0x05E1 };
        return String(hebrewZero, 3);
    }

    const int lettersSize = 11; // big enough for two 5-digit sequences plus a quote mark between
    UChar letters[lettersSize];

    int length;
    if (number < 1000)
        length = 0;
    else {
        length = toHebrewUnder1000(number / 1000, letters);
        letters[length++] = '\'';
        number = number % 1000;
    }
    length += toHebrewUnder1000(number, letters + length);

    ASSERT(length <= lettersSize);
    return String(letters, length);
}

static int toArmenianUnder10000(int number, bool upper, bool addCircumflex, UChar letters[9])
{
    ASSERT(number >= 0 && number < 10000);
    int length = 0;

    int lowerOffset = upper ? 0 : 0x0030;

    if (int thousands = number / 1000) {
        if (thousands == 7) {
            letters[length++] = 0x0548 + lowerOffset;
            letters[length++] = 0x0552 + lowerOffset;
            if (addCircumflex)
                letters[length++] = 0x0302;
        } else {
            letters[length++] = (0x054C - 1 + lowerOffset) + thousands;
            if (addCircumflex)
                letters[length++] = 0x0302;
        }
    }

    if (int hundreds = (number / 100) % 10) {
        letters[length++] = (0x0543 - 1 + lowerOffset) + hundreds;
        if (addCircumflex)
            letters[length++] = 0x0302;
    }

    if (int tens = (number / 10) % 10) {
        letters[length++] = (0x053A - 1 + lowerOffset) + tens;
        if (addCircumflex)
            letters[length++] = 0x0302;
    }

    if (int ones = number % 10) {
        letters[length++] = (0x531 - 1 + lowerOffset) + ones;
        if (addCircumflex)
            letters[length++] = 0x0302;
    }

    return length;
}

static String toArmenian(int number, bool upper)
{
    if (number < 1 || number > 99999999)
        return String::number(number);

    const int lettersSize = 18; // twice what toArmenianUnder10000 needs
    UChar letters[lettersSize];

    int length = toArmenianUnder10000(number / 10000, upper, true, letters);
    length += toArmenianUnder10000(number % 10000, upper, false, letters + length);

    ASSERT(length <= lettersSize);
    return String(letters, length);
}

static String toGeorgian(int number)
{
    if (number < 1 || number > 19999)
        return String::number(number);

    const int lettersSize = 5;
    UChar letters[lettersSize];

    int length = 0;

    if (number > 9999)
        letters[length++] = 0x10F5;

    if (int thousands = (number / 1000) % 10) {
        static const UChar georgianThousands[9] = {
            0x10E9, 0x10EA, 0x10EB, 0x10EC, 0x10ED, 0x10EE, 0x10F4, 0x10EF, 0x10F0
        };
        letters[length++] = georgianThousands[thousands - 1];
    }

    if (int hundreds = (number / 100) % 10) {
        static const UChar georgianHundreds[9] = {
            0x10E0, 0x10E1, 0x10E2, 0x10F3, 0x10E4, 0x10E5, 0x10E6, 0x10E7, 0x10E8
        };
        letters[length++] = georgianHundreds[hundreds - 1];
    }

    if (int tens = (number / 10) % 10) {
        static const UChar georgianTens[9] = {
            0x10D8, 0x10D9, 0x10DA, 0x10DB, 0x10DC, 0x10F2, 0x10DD, 0x10DE, 0x10DF
        };
        letters[length++] = georgianTens[tens - 1];
    }

    if (int ones = number % 10) {
        static const UChar georgianOnes[9] = {
            0x10D0, 0x10D1, 0x10D2, 0x10D3, 0x10D4, 0x10D5, 0x10D6, 0x10F1, 0x10D7
        };
        letters[length++] = georgianOnes[ones - 1];
    }

    ASSERT(length <= lettersSize);
    return String(letters, length);
}

// The table uses the order from the CSS3 specification:
// first 3 group markers, then 3 digit markers, then ten digits.
static String toCJKIdeographic(int number, const UChar table[16])
{
    if (number < 0)
        return String::number(number);

    enum AbstractCJKChar {
        noChar,
        secondGroupMarker, thirdGroupMarker, fourthGroupMarker,
        secondDigitMarker, thirdDigitMarker, fourthDigitMarker,
        digit0, digit1, digit2, digit3, digit4,
        digit5, digit6, digit7, digit8, digit9
    };

    if (number == 0)
        return String(&table[digit0 - 1], 1);

    const int groupLength = 8; // 4 digits, 3 digit markers, and a group marker
    const int bufferLength = 4 * groupLength;
    AbstractCJKChar buffer[bufferLength] = { noChar };

    for (int i = 0; i < 4; ++i) {
        int groupValue = number % 10000;
        number /= 10000;

        // Process least-significant group first, but put it in the buffer last.
        AbstractCJKChar* group = &buffer[(3 - i) * groupLength];

        if (groupValue && i)
            group[7] = static_cast<AbstractCJKChar>(secondGroupMarker - 1 + i);

        // Put in the four digits and digit markers for any non-zero digits.
        group[6] = static_cast<AbstractCJKChar>(digit0 + (groupValue % 10));
        if (number != 0 || groupValue > 9) {
            int digitValue = ((groupValue / 10) % 10);
            group[4] = static_cast<AbstractCJKChar>(digit0 + digitValue);
            if (digitValue)
                group[5] = secondDigitMarker;
        }
        if (number != 0 || groupValue > 99) {
            int digitValue = ((groupValue / 100) % 10);
            group[2] = static_cast<AbstractCJKChar>(digit0 + digitValue);
            if (digitValue)
                group[3] = thirdDigitMarker;
        }
        if (number != 0 || groupValue > 999) {
            int digitValue = groupValue / 1000;
            group[0] = static_cast<AbstractCJKChar>(digit0 + digitValue);
            if (digitValue)
                group[1] = fourthDigitMarker;
        }

        // Remove the tens digit, but leave the marker, for any group that has
        // a value of less than 20.
        if (groupValue < 20) {
            ASSERT(group[4] == noChar || group[4] == digit0 || group[4] == digit1);
            group[4] = noChar;
        }

        if (number == 0)
            break;
    }

    // Convert into characters, omitting consecutive runs of digit0 and
    // any trailing digit0.
    int length = 0;
    UChar characters[bufferLength];
    AbstractCJKChar last = noChar;
    for (int i = 0; i < bufferLength; ++i) {
        AbstractCJKChar a = buffer[i];
        if (a != noChar) {
            if (a != digit0 || last != digit0)
                characters[length++] = table[a - 1];
            last = a;
        }
    }
    if (last == digit0)
        --length;

    return String(characters, length);
}

String listMarkerText(EListStyleType type, int value)
{
    switch (type) {
        case LNONE:
            return "";

        // We use the same characters for text security.
        // See RenderText::setInternalString.
        case CIRCLE:
            return String(&whiteBullet, 1);
        case DISC:
            return String(&bullet, 1);
        case SQUARE:
            // The CSS 2.1 test suite uses U+25EE BLACK MEDIUM SMALL SQUARE
            // instead, but I think this looks better.
            return String(&blackSquare, 1);

        case LDECIMAL:
            return String::number(value);
        case DECIMAL_LEADING_ZERO:
            if (value < -9 || value > 9)
                return String::number(value);
            if (value < 0)
                return "-0" + String::number(-value); // -01 to -09
            return "0" + String::number(value); // 00 to 09

        case LOWER_ALPHA:
        case LOWER_LATIN: {
            static const UChar lowerLatinAlphabet[26] = {
                'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
                'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'
            };
            return toAlphabetic(value, lowerLatinAlphabet, 26);
        }
        case UPPER_ALPHA:
        case UPPER_LATIN: {
            static const UChar upperLatinAlphabet[26] = {
                'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
                'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'
            };
            return toAlphabetic(value, upperLatinAlphabet, 26);
        }
        case LOWER_GREEK: {
            static const UChar lowerGreekAlphabet[24] = {
                0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5, 0x03B6, 0x03B7, 0x03B8,
                0x03B9, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BE, 0x03BF, 0x03C0,
                0x03C1, 0x03C3, 0x03C4, 0x03C5, 0x03C6, 0x03C7, 0x03C8, 0x03C9
            };
            return toAlphabetic(value, lowerGreekAlphabet, 24);
        }

        case HIRAGANA: {
            // FIXME: This table comes from the CSS3 draft, and is probably
            // incorrect, given the comments in that draft.
            static const UChar hiraganaAlphabet[48] = {
                0x3042, 0x3044, 0x3046, 0x3048, 0x304A, 0x304B, 0x304D, 0x304F,
                0x3051, 0x3053, 0x3055, 0x3057, 0x3059, 0x305B, 0x305D, 0x305F,
                0x3061, 0x3064, 0x3066, 0x3068, 0x306A, 0x306B, 0x306C, 0x306D,
                0x306E, 0x306F, 0x3072, 0x3075, 0x3078, 0x307B, 0x307E, 0x307F,
                0x3080, 0x3081, 0x3082, 0x3084, 0x3086, 0x3088, 0x3089, 0x308A,
                0x308B, 0x308C, 0x308D, 0x308F, 0x3090, 0x3091, 0x3092, 0x3093
            };
            return toAlphabetic(value, hiraganaAlphabet, 48);
        }
        case HIRAGANA_IROHA: {
            // FIXME: This table comes from the CSS3 draft, and is probably
            // incorrect, given the comments in that draft.
            static const UChar hiraganaIrohaAlphabet[47] = {
                0x3044, 0x308D, 0x306F, 0x306B, 0x307B, 0x3078, 0x3068, 0x3061,
                0x308A, 0x306C, 0x308B, 0x3092, 0x308F, 0x304B, 0x3088, 0x305F,
                0x308C, 0x305D, 0x3064, 0x306D, 0x306A, 0x3089, 0x3080, 0x3046,
                0x3090, 0x306E, 0x304A, 0x304F, 0x3084, 0x307E, 0x3051, 0x3075,
                0x3053, 0x3048, 0x3066, 0x3042, 0x3055, 0x304D, 0x3086, 0x3081,
                0x307F, 0x3057, 0x3091, 0x3072, 0x3082, 0x305B, 0x3059
            };
            return toAlphabetic(value, hiraganaIrohaAlphabet, 47);
        }
        case KATAKANA: {
            // FIXME: This table comes from the CSS3 draft, and is probably
            // incorrect, given the comments in that draft.
            static const UChar katakanaAlphabet[48] = {
                0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, 0x30AB, 0x30AD, 0x30AF,
                0x30B1, 0x30B3, 0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD, 0x30BF,
                0x30C1, 0x30C4, 0x30C6, 0x30C8, 0x30CA, 0x30CB, 0x30CC, 0x30CD,
                0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, 0x30DE, 0x30DF,
                0x30E0, 0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8, 0x30E9, 0x30EA,
                0x30EB, 0x30EC, 0x30ED, 0x30EF, 0x30F0, 0x30F1, 0x30F2, 0x30F3
            };
            return toAlphabetic(value, katakanaAlphabet, 48);
        }
        case KATAKANA_IROHA: {
            // FIXME: This table comes from the CSS3 draft, and is probably
            // incorrect, given the comments in that draft.
            static const UChar katakanaIrohaAlphabet[47] = {
                0x30A4, 0x30ED, 0x30CF, 0x30CB, 0x30DB, 0x30D8, 0x30C8, 0x30C1,
                0x30EA, 0x30CC, 0x30EB, 0x30F2, 0x30EF, 0x30AB, 0x30E8, 0x30BF,
                0x30EC, 0x30BD, 0x30C4, 0x30CD, 0x30CA, 0x30E9, 0x30E0, 0x30A6,
                0x30F0, 0x30CE, 0x30AA, 0x30AF, 0x30E4, 0x30DE, 0x30B1, 0x30D5,
                0x30B3, 0x30A8, 0x30C6, 0x30A2, 0x30B5, 0x30AD, 0x30E6, 0x30E1,
                0x30DF, 0x30B7, 0x30F1, 0x30D2, 0x30E2, 0x30BB, 0x30B9
            };
            return toAlphabetic(value, katakanaIrohaAlphabet, 47);
        }

        case CJK_IDEOGRAPHIC: {
            static const UChar traditionalChineseInformalTable[16] = {
                0x842C, 0x5104, 0x5146,
                0x5341, 0x767E, 0x5343,
                0x96F6, 0x4E00, 0x4E8C, 0x4E09, 0x56DB,
                0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D
            };
            return toCJKIdeographic(value, traditionalChineseInformalTable);
        }

        case LOWER_ROMAN:
            return toRoman(value, false);
        case UPPER_ROMAN:
            return toRoman(value, true);

        case ARMENIAN:
            // CSS3 says "armenian" means "lower-armenian".
            // But the CSS2.1 test suite contains uppercase test results for "armenian",
            // so we'll match the test suite.
            return toArmenian(value, true);
        case GEORGIAN:
            return toGeorgian(value);
        case HEBREW:
            return toHebrew(value);
    }

    ASSERT_NOT_REACHED();
    return "";
}

RenderListMarker::RenderListMarker(RenderListItem* item)
    : RenderBox(item->document())
    , m_listItem(item)
{
    // init RenderObject attributes
    setInline(true);   // our object is Inline
    setReplaced(true); // pretend to be replaced
}

RenderListMarker::~RenderListMarker()
{
    if (m_image)
        m_image->removeClient(this);
}

void RenderListMarker::styleWillChange(StyleDifference diff, const RenderStyle* newStyle)
{
    if (style() && (newStyle->listStylePosition() != style()->listStylePosition() || newStyle->listStyleType() != style()->listStyleType()))
        setNeedsLayoutAndPrefWidthsRecalc();
    
    RenderBox::styleWillChange(diff, newStyle);
}

void RenderListMarker::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBox::styleDidChange(diff, oldStyle);

    if (m_image != style()->listStyleImage()) {
        if (m_image)
            m_image->removeClient(this);
        m_image = style()->listStyleImage();
        if (m_image)
            m_image->addClient(this);
    }
}

InlineBox* RenderListMarker::createInlineBox()
{
    InlineBox* result = RenderBox::createInlineBox();
    result->setIsText(isText());
    return result;
}

bool RenderListMarker::isImage() const
{
    return m_image && !m_image->errorOccurred();
}

void RenderListMarker::paint(PaintInfo& paintInfo, int tx, int ty)
{
    if (paintInfo.phase != PaintPhaseForeground)
        return;
    
    if (style()->visibility() != VISIBLE)
        return;

    IntRect marker = getRelativeMarkerRect();
    marker.move(tx, ty);

    IntRect box(tx + x(), ty + y(), width(), height());

    if (box.y() > paintInfo.rect.bottom() || box.y() + box.height() < paintInfo.rect.y())
        return;

    if (hasBoxDecorations()) 
        paintBoxDecorations(paintInfo, box.x(), box.y());

    GraphicsContext* context = paintInfo.context;

    if (isImage()) {
#if PLATFORM(MAC)
        if (style()->highlight() != nullAtom && !paintInfo.context->paintingDisabled())
            paintCustomHighlight(tx, ty, style()->highlight(), true);
#endif
        context->drawImage(m_image->image(this, marker.size()), marker.location());
        if (selectionState() != SelectionNone) {
            // FIXME: selectionRect() is in absolute, not painting coordinates.
            context->fillRect(selectionRect(), selectionBackgroundColor(), style()->colorSpace());
        }
        return;
    }

#if PLATFORM(MAC)
    // FIXME: paint gap between marker and list item proper
    if (style()->highlight() != nullAtom && !paintInfo.context->paintingDisabled())
        paintCustomHighlight(tx, ty, style()->highlight(), true);
#endif

    if (selectionState() != SelectionNone) {
        // FIXME: selectionRect() is in absolute, not painting coordinates.
        context->fillRect(selectionRect(), selectionBackgroundColor(), style()->colorSpace());
    }

    const Color color(style()->color());
    context->setStrokeColor(color, style()->colorSpace());
    context->setStrokeStyle(SolidStroke);
    context->setStrokeThickness(1.0f);
    context->setFillColor(color, style()->colorSpace());

    switch (style()->listStyleType()) {
        case DISC:
            context->drawEllipse(marker);
            return;
        case CIRCLE:
            context->setFillColor(Color::transparent, DeviceColorSpace);
            context->drawEllipse(marker);
            return;
        case SQUARE:
            context->drawRect(marker);
            return;
        case LNONE:
            return;
        case ARMENIAN:
        case CJK_IDEOGRAPHIC:
        case DECIMAL_LEADING_ZERO:
        case GEORGIAN:
        case HEBREW:
        case HIRAGANA:
        case HIRAGANA_IROHA:
        case KATAKANA:
        case KATAKANA_IROHA:
        case LDECIMAL:
        case LOWER_ALPHA:
        case LOWER_GREEK:
        case LOWER_LATIN:
        case LOWER_ROMAN:
        case UPPER_ALPHA:
        case UPPER_LATIN:
        case UPPER_ROMAN:
            break;
    }
    if (m_text.isEmpty())
        return;

    TextRun textRun(m_text);

    // Text is not arbitrary. We can judge whether it's RTL from the first character,
    // and we only need to handle the direction RightToLeft for now.
    bool textNeedsReversing = direction(m_text[0]) == RightToLeft;
    Vector<UChar> reversedText;
    if (textNeedsReversing) {
        int length = m_text.length();
        reversedText.grow(length);
        for (int i = 0; i < length; ++i)
            reversedText[length - i - 1] = m_text[i];
        textRun = TextRun(reversedText.data(), length);
    }

    const Font& font = style()->font();
    if (style()->direction() == LTR) {
        int width = font.width(textRun);
        context->drawText(style()->font(), textRun, marker.location());
        const UChar periodSpace[2] = { '.', ' ' };
        context->drawText(style()->font(), TextRun(periodSpace, 2), marker.location() + IntSize(width, 0));
    } else {
        const UChar spacePeriod[2] = { ' ', '.' };
        TextRun spacePeriodRun(spacePeriod, 2);
        int width = font.width(spacePeriodRun);
        context->drawText(style()->font(), spacePeriodRun, marker.location());
        context->drawText(style()->font(), textRun, marker.location() + IntSize(width, 0));
    }
}

void RenderListMarker::layout()
{
    ASSERT(needsLayout());
    ASSERT(!prefWidthsDirty());

    if (isImage()) {
        setWidth(m_image->imageSize(this, style()->effectiveZoom()).width());
        setHeight(m_image->imageSize(this, style()->effectiveZoom()).height());
    } else {
        setWidth(minPrefWidth());
        setHeight(style()->font().height());
    }

    m_marginLeft = m_marginRight = 0;

    Length leftMargin = style()->marginLeft();
    Length rightMargin = style()->marginRight();
    if (leftMargin.isFixed())
        m_marginLeft = leftMargin.value();
    if (rightMargin.isFixed())
        m_marginRight = rightMargin.value();

    setNeedsLayout(false);
}

void RenderListMarker::imageChanged(WrappedImagePtr o, const IntRect*)
{
    // A list marker can't have a background or border image, so no need to call the base class method.
    if (o != m_image->data())
        return;

    if (width() != m_image->imageSize(this, style()->effectiveZoom()).width() || height() != m_image->imageSize(this, style()->effectiveZoom()).height() || m_image->errorOccurred())
        setNeedsLayoutAndPrefWidthsRecalc();
    else
        repaint();
}

void RenderListMarker::calcPrefWidths()
{
    ASSERT(prefWidthsDirty());

    m_text = "";

    const Font& font = style()->font();

    if (isImage()) {
        // FIXME: This is a somewhat arbitrary width.  Generated images for markers really won't become particularly useful
        // until we support the CSS3 marker pseudoclass to allow control over the width and height of the marker box.
        int bulletWidth = font.ascent() / 2;
        m_image->setImageContainerSize(IntSize(bulletWidth, bulletWidth));
        m_minPrefWidth = m_maxPrefWidth = m_image->imageSize(this, style()->effectiveZoom()).width();
        setPrefWidthsDirty(false);
        updateMargins();
        return;
    }

    int width = 0;
    EListStyleType type = style()->listStyleType();
    switch (type) {
        case LNONE:
            break;
        case CIRCLE:
        case DISC:
        case SQUARE:
            m_text = listMarkerText(type, 0); // value is ignored for these types
            width = (font.ascent() * 2 / 3 + 1) / 2 + 2;
            break;
        case ARMENIAN:
        case CJK_IDEOGRAPHIC:
        case DECIMAL_LEADING_ZERO:
        case GEORGIAN:
        case HEBREW:
        case HIRAGANA:
        case HIRAGANA_IROHA:
        case KATAKANA:
        case KATAKANA_IROHA:
        case LDECIMAL:
        case LOWER_ALPHA:
        case LOWER_GREEK:
        case LOWER_LATIN:
        case LOWER_ROMAN:
        case UPPER_ALPHA:
        case UPPER_LATIN:
        case UPPER_ROMAN:
            m_text = listMarkerText(type, m_listItem->value());
            if (m_text.isEmpty())
                width = 0;
            else {
                int itemWidth = font.width(m_text);
                const UChar periodSpace[2] = { '.', ' ' };
                int periodSpaceWidth = font.width(TextRun(periodSpace, 2));
                width = itemWidth + periodSpaceWidth;
            }
            break;
    }

    m_minPrefWidth = width;
    m_maxPrefWidth = width;

    setPrefWidthsDirty(false);
    
    updateMargins();
}

void RenderListMarker::updateMargins()
{
    const Font& font = style()->font();

    int marginLeft = 0;
    int marginRight = 0;

    if (isInside()) {
        if (isImage()) {
            if (style()->direction() == LTR)
                marginRight = cMarkerPadding;
            else
                marginLeft = cMarkerPadding;
        } else switch (style()->listStyleType()) {
            case DISC:
            case CIRCLE:
            case SQUARE:
                if (style()->direction() == LTR) {
                    marginLeft = -1;
                    marginRight = font.ascent() - minPrefWidth() + 1;
                } else {
                    marginLeft = font.ascent() - minPrefWidth() + 1;
                    marginRight = -1;
                }
                break;
            default:
                break;
        }
    } else {
        if (style()->direction() == LTR) {
            if (isImage())
                marginLeft = -minPrefWidth() - cMarkerPadding;
            else {
                int offset = font.ascent() * 2 / 3;
                switch (style()->listStyleType()) {
                    case DISC:
                    case CIRCLE:
                    case SQUARE:
                        marginLeft = -offset - cMarkerPadding - 1;
                        break;
                    case LNONE:
                        break;
                    default:
                        marginLeft = m_text.isEmpty() ? 0 : -minPrefWidth() - offset / 2;
                }
            }
        } else {
            if (isImage())
                marginLeft = cMarkerPadding;
            else {
                int offset = font.ascent() * 2 / 3;
                switch (style()->listStyleType()) {
                    case DISC:
                    case CIRCLE:
                    case SQUARE:
                        marginLeft = offset + cMarkerPadding + 1 - minPrefWidth();
                        break;
                    case LNONE:
                        break;
                    default:
                        marginLeft = m_text.isEmpty() ? 0 : offset / 2;
                }
            }
        }
        marginRight = -marginLeft - minPrefWidth();
    }

    style()->setMarginLeft(Length(marginLeft, Fixed));
    style()->setMarginRight(Length(marginRight, Fixed));
}

int RenderListMarker::lineHeight(bool, bool) const
{
    if (!isImage())
        return m_listItem->lineHeight(false, true);
    return height();
}

int RenderListMarker::baselinePosition(bool, bool) const
{
    if (!isImage()) {
        const Font& font = style()->font();
        return font.ascent() + (lineHeight(false) - font.height())/2;
    }
    return height();
}

bool RenderListMarker::isInside() const
{
    return m_listItem->notInList() || style()->listStylePosition() == INSIDE;
}

IntRect RenderListMarker::getRelativeMarkerRect()
{
    if (isImage())
        return IntRect(x(), y(), m_image->imageSize(this, style()->effectiveZoom()).width(), m_image->imageSize(this, style()->effectiveZoom()).height());

    switch (style()->listStyleType()) {
        case DISC:
        case CIRCLE:
        case SQUARE: {
            // FIXME: Are these particular rounding rules necessary?
            const Font& font = style()->font();
            int ascent = font.ascent();
            int bulletWidth = (ascent * 2 / 3 + 1) / 2;
            return IntRect(x() + 1, y() + 3 * (ascent - ascent * 2 / 3) / 2, bulletWidth, bulletWidth);
        }
        case LNONE:
            return IntRect();
        case ARMENIAN:
        case CJK_IDEOGRAPHIC:
        case DECIMAL_LEADING_ZERO:
        case GEORGIAN:
        case HEBREW:
        case HIRAGANA:
        case HIRAGANA_IROHA:
        case KATAKANA:
        case KATAKANA_IROHA:
        case LDECIMAL:
        case LOWER_ALPHA:
        case LOWER_GREEK:
        case LOWER_LATIN:
        case LOWER_ROMAN:
        case UPPER_ALPHA:
        case UPPER_LATIN:
        case UPPER_ROMAN:
            if (m_text.isEmpty())
                return IntRect();
            const Font& font = style()->font();
            int itemWidth = font.width(m_text);
            const UChar periodSpace[2] = { '.', ' ' };
            int periodSpaceWidth = font.width(TextRun(periodSpace, 2));
            return IntRect(x(), y() + font.ascent(), itemWidth + periodSpaceWidth, font.height());
    }

    return IntRect();
}

void RenderListMarker::setSelectionState(SelectionState state)
{
    RenderBox::setSelectionState(state);
    if (InlineBox* box = inlineBoxWrapper())
        if (RootInlineBox* root = box->root())
            root->setHasSelectedChildren(state != SelectionNone);
    containingBlock()->setSelectionState(state);
}

IntRect RenderListMarker::selectionRectForRepaint(RenderBoxModelObject* repaintContainer, bool clipToVisibleContent)
{
    ASSERT(!needsLayout());

    if (selectionState() == SelectionNone || !inlineBoxWrapper())
        return IntRect();

    RootInlineBox* root = inlineBoxWrapper()->root();
    IntRect rect(0, root->selectionTop() - y(), width(), root->selectionHeight());
            
    if (clipToVisibleContent)
        computeRectForRepaint(repaintContainer, rect);
    else
        rect = localToContainerQuad(FloatRect(rect), repaintContainer).enclosingBoundingBox();
    
    return rect;
}

} // namespace WebCore
