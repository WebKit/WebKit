/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 */
#include "config.h"
#include "CSSBorderImageValue.h"

#include "CSSImageValue.h"
#include "PlatformString.h"
#include "RectImpl.h"

namespace WebCore {

CSSBorderImageValue::CSSBorderImageValue(PassRefPtr<CSSImageValue> image,
    PassRefPtr<RectImpl> imageRect, int horizontalRule, int verticalRule)
    : m_image(image)
    , m_imageSliceRect(imageRect)
    , m_horizontalSizeRule(horizontalRule)
    , m_verticalSizeRule(verticalRule)
{
}

String CSSBorderImageValue::cssText() const
{
    // Image first.
    String text(m_image->cssText());
    text += " ";
    
    // Now the rect, but it isn't really a rect, so we dump manually
    text += m_imageSliceRect->top()->cssText();
    text += " ";
    text += m_imageSliceRect->right()->cssText();
    text += " ";
    text += m_imageSliceRect->bottom()->cssText();
    text += " ";
    text += m_imageSliceRect->left()->cssText();
    
    // Now the keywords.
    text += " ";
    text += CSSPrimitiveValue(m_horizontalSizeRule).cssText();
    text += " ";
    text += CSSPrimitiveValue(m_verticalSizeRule).cssText();

    return text;
}

}
