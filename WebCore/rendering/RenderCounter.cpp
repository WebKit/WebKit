/**
 * This file is part of the HTML rendering engine for KDE.
 *
 * Copyright (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "RenderCounter.h"

#include "CounterNode.h"
#include "CounterResetNode.h"
#include "Document.h"
#include "RenderStyle.h"

namespace WebCore {

const int cMarkerPadding = 7;

RenderCounter::RenderCounter(Node* node, CounterData* counter)
    : RenderText(node, 0)
    , m_counter(counter)
    , m_counterNode(0)
{
}

void RenderCounter::layout()
{
    ASSERT(needsLayout());

    if (!minMaxKnown())
        calcMinMaxWidth();

    RenderText::layout();
}

static String toRoman(int number, bool upper)
{ 
    String roman;
    const UChar ldigits[] = { 'i', 'v', 'x', 'l', 'c', 'd', 'm' };
    const UChar udigits[] = { 'I', 'V', 'X', 'L', 'C', 'D', 'M' };
    const UChar* digits = upper ? udigits : ldigits;
    int d = 0;
    do {
        int num = number % 10;
        if (num % 5 < 4) {
            for (int i = num % 5; i > 0; i--)
                roman.insert(&digits[d], 1, 0);
        }
        if (num >= 4 && num <= 8)
            roman.insert(&digits[d + 1], 1, 0);
        if (num == 9)
            roman.insert(&digits[d + 2], 1, 0);
        if (num % 5 == 4)
            roman.insert(&digits[d], 1, 0);
        number /= 10;
        d += 2;
    } while (number);
    return roman;
}

static String toLetterString(int number, UChar letterA)
{ 
    if (number < 2)
        return String(&letterA, 1); // match WinIE (A.) not FireFox (0.)

    String letterString;
    UChar c;
    while (number > 0) {
        int onesDigit = (number - 1) % 26;
        c = letterA + onesDigit;
        letterString = String(&c, 1) + letterString;
        number -= onesDigit;
        number /= 26;
    }

    return letterString;
}

static String toHebrew(int number)
{
    if (number < 1)
        return String::number(number);

    static const UChar tenDigit[] = {1497, 1499, 1500, 1502, 1504, 1505, 1506, 1508, 1510};

    String letter;
    UChar c;
    if (number > 999) {
        letter = toHebrew(number / 1000) + "'";
        number = number % 1000;
    }
    int fourHundreds = number / 400;
    for (int i = 0; i < fourHundreds; i++) {
        c = 1511 + 3;
        letter.append(c);
    }
    number %= 400;
    if (number / 100) {
        c = 1511 + (number / 100) - 1;
        letter.append(c);
    }
    number %= 100;
    if (number == 15 || number == 16) {
        c = 1487 + 9;
        letter.append(c);
        c = 1487 + number - 9;
        letter.append(c);
    } else {
        int tens = number / 10;
        if (tens) {
            c = tenDigit[tens - 1];
            letter.append(c);
        }
        number = number % 10;
        if (number) {
            c = 1487 + number;
            letter.append(c);
        }
    }
    return letter;
}

String RenderCounter::convertValueToType(int value, int total, EListStyleType type)
{
    String item;
    UChar c;
    switch(type) {
        case LNONE:
            break;
        // Glyphs:
        case DISC:
            c = 0x2022;
            item = String(&c, 1);
            break;
        case CIRCLE:
            c = 0x25e6;
            item = String(&c, 1);
            break;
        case SQUARE:
            c = 0x25a0;
            item = String(&c, 1);
            break;
        case ARMENIAN:
        case GEORGIAN:
        case CJK_IDEOGRAPHIC:
        case HIRAGANA:
        case KATAKANA:
        case HIRAGANA_IROHA:
        case KATAKANA_IROHA:
        case DECIMAL_LEADING_ZERO:
            // FIXME: These are currently unsupported. For now we'll use decimal instead.
        case LDECIMAL:
            item = String::number(value);
            break;
        // Algoritmic:
        case LOWER_ROMAN:
            item = toRoman(value, false);
            break;
        case UPPER_ROMAN:
            item = toRoman(value, true);
            break;
        case LOWER_GREEK: {
            int number = value - 1;
            int l = (number % 24);
            if (l > 16)
                l++; // Skip GREEK SMALL LETTER FINAL SIGMA
            c = 945 + l;
            item = String(&c, 1);
            for (int i = 0; i < (number / 24); i++)
                item += "'";
            break;
        }
        case HEBREW:
            item = toHebrew(value);
            break;
        case LOWER_ALPHA:
        case LOWER_LATIN:
            item = toLetterString(value, 'a');
            break;
        case UPPER_ALPHA:
        case UPPER_LATIN:
            item = toLetterString(value, 'A');
            break;
        default:
            item = String::number(value);
            break;
    }
    return item;
}

void RenderCounter::calcMinMaxWidth()
{
    ASSERT(!minMaxKnown());

    if (!style())
        return;

    bool hasSeparator = !m_counter->separator().isNull();

    if (!m_counterNode && parent()->parent())
        m_counterNode = parent()->parent()->findCounter(m_counter->identifier(), true, hasSeparator);

    int value = m_counterNode->count();
    if (m_counterNode->isReset())
        value = m_counterNode->value();

    int total = value;
    if (m_counterNode->parent())
        total = m_counterNode->parent()->total();

    m_item = convertValueToType(value, total, m_counter->listStyle());

    if (hasSeparator) {
        CounterNode* counter = m_counterNode->parent();
        bool first = true;
        while  (counter->parent() && (first || !(counter->isReset() && counter->parent()->isRoot()))) {
            value = counter->count() ? counter->count() : 1;
            total = counter->parent()->total();
            m_item = convertValueToType(value, total, m_counter->listStyle()) + m_counter->separator() + m_item;
            counter = counter->parent();
            first = false;
        }
    }

    m_str = new StringImpl(m_item.characters(), m_item.length());

    RenderText::calcMinMaxWidth();
}

} // namespace WebCore
