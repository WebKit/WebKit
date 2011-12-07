/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
 */

#include "config.h"
#include "CSSBorderImageValue.h"

#include "PlatformString.h"
#include "Rect.h"

namespace WebCore {

CSSBorderImageValue::CSSBorderImageValue(PassRefPtr<CSSValue> image, PassRefPtr<CSSBorderImageSliceValue> imageSlice,
    PassRefPtr<CSSValue> borderSlice, PassRefPtr<CSSValue> outset, PassRefPtr<CSSValue> repeat)
    : m_image(image)
    , m_imageSlice(imageSlice)
    , m_borderSlice(borderSlice)
    , m_outset(outset)
    , m_repeat(repeat)
{
}

CSSBorderImageValue::~CSSBorderImageValue()
{
}

String CSSBorderImageValue::cssText() const
{
    // Image first.
    String text;
    
    if (m_image)
        text += m_image->cssText();

    // Now the slices.
    if (m_imageSlice) {
        if (!text.isEmpty())
            text += " ";
        text += m_imageSlice->cssText();
    }

    // Now the border widths.
    if (m_borderSlice) {
        text += " / ";
        text += m_borderSlice->cssText();
    }

    if (m_outset) {
        text += " / ";
        text += m_outset->cssText();
    }

    if (m_repeat) {
        // Now the keywords.
        if (!text.isEmpty())
            text += " ";
        text += m_repeat->cssText();
    }

    return text;
}

void CSSBorderImageValue::addSubresourceStyleURLs(ListHashSet<KURL>& urls, const CSSStyleSheet* styleSheet)
{
    m_image->addSubresourceStyleURLs(urls, styleSheet);
}

} // namespace WebCore
