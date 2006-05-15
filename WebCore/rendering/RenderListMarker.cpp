/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
#include "config.h"
#include "RenderListMarker.h"

#include "CachedImage.h"
#include "Document.h"
#include "GraphicsContext.h"
#include "ListMarkerBox.h"
#include "RenderCanvas.h"
#include "RenderListItem.h"

using namespace std;

namespace WebCore {

const int cMarkerPadding = 7;

static DeprecatedString toRoman(int number, bool upper)
{
    DeprecatedString roman;
    const UChar ldigits[] = { 'i', 'v', 'x', 'l', 'c', 'd', 'm' };
    const UChar udigits[] = { 'I', 'V', 'X', 'L', 'C', 'D', 'M' };
    const UChar* digits = upper ? udigits : ldigits;
    int d = 0;
    do {
        int num = number % 10;
        if (num % 5 < 4)
            for (int i = num % 5; i > 0; i--)
                roman.insert(0, digits[d]);
        if (num >= 4 && num <= 8)
            roman.insert(0, digits[d + 1]);
        if (num == 9)
            roman.insert(0, digits[d + 2]);
        if (num % 5 == 4)
            roman.insert(0, digits[d]);
        number /= 10;
        d += 2;
    } while (number);
    return roman;
}

static DeprecatedString toLetterString(int number, int letterA)
{
    if (number < 2)
        return QChar(letterA); // match WinIE (A.) not FireFox (0.)

    DeprecatedString letterString;
    while (number > 0) {
        int onesDigit = (number - 1) % 26;
        letterString = QChar(letterA + onesDigit) + letterString;
        number -= onesDigit;
        number /= 26;
    }

    return letterString;
}

static DeprecatedString toHebrew(int number)
{
    const QChar tenDigit[] = {1497, 1499, 1500, 1502, 1504, 1505, 1506, 1508, 1510};

    DeprecatedString letter;
    if (number > 999) {
        letter = toHebrew(number / 1000) + "'";
        number = number % 1000;
    }
    int fourHundreds = number / 400;
    for (int i = 0; i < fourHundreds; i++)
        letter += QChar(1511 + 3);
    number %= 400;
    if (number / 100)
        letter += QChar(1511 + (number / 100) - 1);
    number %= 100;
    if (number == 15 || number == 16) {
        letter += QChar(1487 + 9);
        letter += QChar(1487 + number - 9);
    } else {
        int tens = number / 10;
        if (tens)
            letter += tenDigit[tens - 1];
        number = number % 10;
        if (number)
            letter += QChar(1487 + number);
    }
    return letter;
}

RenderListMarker::RenderListMarker(Document* document)
    : RenderBox(document)
    , m_listImage(0)
    , m_selectionState(SelectionNone)
{
    // init RenderObject attributes
    setInline(true);   // our object is Inline
    setReplaced(true); // pretend to be replaced
}

RenderListMarker::~RenderListMarker()
{
    if (m_listImage)
        m_listImage->deref(this);
}

void RenderListMarker::setStyle(RenderStyle* s)
{
    if (s && style() && s->listStylePosition() != style()->listStylePosition())
        setNeedsLayoutAndMinMaxRecalc();
    
    RenderBox::setStyle(s);

    if (m_listImage != style()->listStyleImage()) {
        if (m_listImage)
            m_listImage->deref(this);
        m_listImage = style()->listStyleImage();
        if (m_listImage)
            m_listImage->ref(this);
    }
}

InlineBox* RenderListMarker::createInlineBox(bool, bool isRootLineBox, bool)
{
    KHTMLAssert(!isRootLineBox);
    ListMarkerBox* box = new (renderArena()) ListMarkerBox(this);
    m_inlineBoxWrapper = box;
    return box;
}

void RenderListMarker::paint(PaintInfo& i, int _tx, int _ty)
{
    if (i.phase != PaintPhaseForeground)
        return;
    
    if (style()->visibility() != VISIBLE)
        return;

    IntRect marker = getRelativeMarkerRect();
    marker.move(_tx, _ty);

    IntRect box(_tx + m_x, _ty + m_y, m_width, m_height);

    if (box.y() > i.r.bottom() || box.y() + box.height() < i.r.y())
        return;

    if (shouldPaintBackgroundOrBorder()) 
        paintBoxDecorations(i, box.x(), box.y());

    GraphicsContext* p = i.p;
    p->setFont(style()->font());

    if (document()->printing()) {
        if (box.y() < i.r.y())
            // This has been printed already we suppose.
            return;
        
        RenderCanvas* c = canvas();
        if (box.y() + box.height() + paddingBottom() + borderBottom() >= c->printRect().bottom()) {
            if (box.y() < c->truncatedAt())
                c->setBestTruncatedAt(box.y(), this);
            // Let's print this on the next page.
            return; 
        }
    }

    if (m_listImage && !m_listImage->isErrorImage()) {
        p->drawImage(m_listImage->image(), marker.location());
        if (selectionState() != SelectionNone)
            p->fillRect(selectionRect(), selectionColor(p));
        return;
    }

#ifdef BOX_DEBUG
    p->setPen(red);
    p->drawRect(box.x(), box.y(), box.width(), box.height());
#endif

    if (selectionState() != SelectionNone)
        p->fillRect(selectionRect(), selectionColor(p));

    const Color color(style()->color());
    p->setPen(color);

    switch(style()->listStyleType()) {
    case DISC:
        p->setFillColor(color.rgb());
        p->drawEllipse(marker);
        return;
    case CIRCLE:
        p->setFillColor(Color::transparent);
        p->drawEllipse(marker);
        return;
    case SQUARE:
        p->setFillColor(color.rgb());
        p->drawRect(marker);
        return;
    case LNONE:
        return;
    default:
        if (!m_item.isEmpty()) {
            const Font& font = style()->font();
            TextRun textRun(reinterpret_cast<const UChar*>(m_item.unicode()), m_item.length());
            if (style()->direction() == LTR) {
                int width = font.width(textRun);
                p->drawText(textRun, marker.location());
                UChar periodSpace[2] = { '.', ' ' };
                p->drawText(TextRun(periodSpace, 2), marker.location() + IntSize(width, 0));
            } else {
                UChar spacePeriod[2] = { ' ', '.' };
                TextRun spacePeriodRun(spacePeriod, 2);
                int width = font.width(spacePeriodRun);
                p->drawText(spacePeriodRun, marker.location());
                p->drawText(textRun, marker.location() + IntSize(width, 0));
            }
        }
    }
}

void RenderListMarker::layout()
{
    KHTMLAssert(needsLayout());
    // ### KHTMLAssert(minMaxKnown());
    if (!minMaxKnown())
        calcMinMaxWidth();
    setNeedsLayout(false);
}

void RenderListMarker::imageChanged(CachedImage *o)
{
    if (o != m_listImage) {
        RenderBox::imageChanged(o);
        return;
    }

    if (m_width != m_listImage->imageSize().width() || m_height != m_listImage->imageSize().height())
        setNeedsLayoutAndMinMaxRecalc();
    else
        repaint();
}

void RenderListMarker::calcMinMaxWidth()
{
    KHTMLAssert(!minMaxKnown());

    if (m_listImage) {
        m_width = m_listImage->image()->width();
        m_height = m_listImage->image()->height();
        m_minWidth = m_maxWidth = m_width;
        setMinMaxKnown();
        return;
    }

    if (m_listItem->value() < 0) // not yet calculated
        m_listItem->calcValue();

    const Font& font = style()->font();
    m_height = font.height();

    switch(style()->listStyleType())
    {
    case DISC:
    case CIRCLE:
    case SQUARE:
        m_width = (font.ascent() * 2 / 3 + 1) / 2 + 2;
        goto end;
    case ARMENIAN:
    case GEORGIAN:
    case CJK_IDEOGRAPHIC:
    case HIRAGANA:
    case KATAKANA:
    case HIRAGANA_IROHA:
    case KATAKANA_IROHA:
    case DECIMAL_LEADING_ZERO:
        // ### unsupported, we use decimal instead
    case LDECIMAL:
        m_item.sprintf("%d", m_listItem->value());
        break;
    case LOWER_ROMAN:
        m_item = toRoman(m_listItem->value(), false);
        break;
    case UPPER_ROMAN:
        m_item = toRoman(m_listItem->value(), true);
        break;
    case LOWER_GREEK:
     {
        int number = m_listItem->value() - 1;
        int l = (number % 24);

        if (l>16) {l++;} // Skip GREEK SMALL LETTER FINAL SIGMA

        m_item = QChar(945 + l);
        for (int i = 0; i < (number / 24); i++) {
            m_item += "'";
        }
        break;
     }
    case HEBREW:
        m_item = toHebrew(m_listItem->value());
        break;
    case LOWER_ALPHA:
    case LOWER_LATIN:
        m_item = toLetterString(m_listItem->value(), 'a');
        break;
    case UPPER_ALPHA:
    case UPPER_LATIN:
        m_item = toLetterString(m_listItem->value(), 'A');
        break;
    case LNONE:
        break;
    }

    {
        int itemWidth = font.width(TextRun(reinterpret_cast<const UChar*>(m_item.unicode()), m_item.length()));
        UChar periodSpace[2] = { '.', ' ' };
        int periodSpaceWidth = font.width(TextRun(periodSpace, 2));
        m_width = itemWidth + periodSpaceWidth;
    }

end:

    m_minWidth = m_width;
    m_maxWidth = m_width;

    setMinMaxKnown();
}

void RenderListMarker::calcWidth()
{
    // m_width is set in calcMinMaxWidth()
    bool haveImage = m_listImage && !m_listImage->isErrorImage();
    const Font& font = style()->font();

    if (isInside()) {
        if (haveImage) {
            if (style()->direction() == LTR) {
                m_marginLeft = 0;
                m_marginRight = cMarkerPadding;
            } else {
                m_marginLeft = cMarkerPadding;
                m_marginRight = 0;
            }
        } else switch (style()->listStyleType()) {
            case DISC:
            case CIRCLE:
            case SQUARE:
                if (style()->direction() == LTR) {
                    m_marginLeft = -1;
                    m_marginRight = font.ascent() - m_width + 1;
                } else {
                    m_marginLeft = font.ascent() - m_width + 1;
                    m_marginRight = -1;
                }
                break;
            default:
                m_marginLeft = 0;
                m_marginRight = 0;
        }
    } else {
        if (style()->direction() == LTR) {
            if (haveImage)
                m_marginLeft = -m_width - cMarkerPadding;
            else {
                int offset = font.ascent() * 2 / 3;
                switch (style()->listStyleType()) {
                    case DISC:
                    case CIRCLE:
                    case SQUARE:
                        m_marginLeft = -offset - cMarkerPadding - 1;
                        break;
                    case LNONE:
                        m_marginLeft = 0;
                        break;
                    default:
                        m_marginLeft = m_item.isEmpty() ? 0 : -m_width - offset / 2;
                }
            }
        } else {
            if (haveImage)
                m_marginLeft = cMarkerPadding;
            else {
                int offset = font.ascent() * 2 / 3;
                switch (style()->listStyleType()) {
                    case DISC:
                    case CIRCLE:
                    case SQUARE:
                        m_marginLeft = offset + cMarkerPadding + 1 - m_width;
                        break;
                    case LNONE:
                        m_marginLeft = 0;
                        break;
                    default:
                        m_marginLeft = m_item.isEmpty() ? 0 : offset / 2;
                }
            }
        }
        m_marginRight = -m_marginLeft - m_width;
    }
}

short RenderListMarker::lineHeight(bool, bool) const
{
    if (!m_listImage)
        return m_listItem->lineHeight(false, true);
    return height();
}

short RenderListMarker::baselinePosition(bool, bool) const
{
    if (!m_listImage) {
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
    if (m_listImage && !m_listImage->isErrorImage())
        return IntRect(m_x, m_y,  m_listImage->imageSize().width(), m_listImage->imageSize().height());

    const Font& font = style()->font();
    int ascent = font.ascent();
    
    switch (style()->listStyleType()) {
        case DISC:
        case CIRCLE:
        case SQUARE: {
            // FIXME: Are these particular rounding rules necessary?
            int bulletWidth = (ascent * 2 / 3 + 1) / 2;
            return IntRect(m_x + 1, m_y + 3 * (ascent - ascent * 2 / 3) / 2, bulletWidth, bulletWidth);
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
            break;
    }

    if (m_item.isEmpty())
        return IntRect();

    int itemWidth = font.width(TextRun(reinterpret_cast<const UChar*>(m_item.unicode()), m_item.length()));
    UChar periodSpace[2] = { '.', ' ' };
    int periodSpaceWidth = font.width(TextRun(periodSpace, 2));
    return IntRect(m_x, m_y + ascent, itemWidth + periodSpaceWidth, font.height());
}

void RenderListMarker::setSelectionState(SelectionState state)
{
    m_selectionState = state;
    RootInlineBox* root = inlineBoxWrapper()->root();
    if (root)
        root->setHasSelectedChildren(state != SelectionNone);
    containingBlock()->setSelectionState(state);
}

IntRect RenderListMarker::selectionRect()
{
    if (selectionState() == SelectionNone)
        return IntRect();

    int absx, absy;
    RenderBlock* cb = containingBlock();
    cb->absolutePosition(absx, absy);
    if (cb->hasOverflowClip())
        cb->layer()->subtractScrollOffset(absx, absy);

    RootInlineBox* root = inlineBoxWrapper()->root();
    return IntRect(absx + xPos(), absy + root->selectionTop(), width(), root->selectionHeight());
}

Color RenderListMarker::selectionColor(GraphicsContext* p) const
{
    Color color = RenderBox::selectionColor(p);
    if (!m_listImage || m_listImage->isErrorImage())
        return color;
    // Limit the opacity so that no user-specified selection color can obscure selected images.
    if (color.alpha() > selectionColorImageOverlayAlpha)
        color = Color(color.red(), color.green(), color.blue(), selectionColorImageOverlayAlpha);
    return color;
}

}
