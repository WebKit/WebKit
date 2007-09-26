/*
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef CSSBorderImageValue_h
#define CSSBorderImageValue_h

#include "CSSValue.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSImageValue;
class Rect;

class CSSBorderImageValue : public CSSValue {
public:
    CSSBorderImageValue();
    CSSBorderImageValue(PassRefPtr<CSSImageValue>, PassRefPtr<Rect>, int horizontalRule, int verticalRule);

    virtual String cssText() const;

public:
    // The border image.
    RefPtr<CSSImageValue> m_image;

    // These four values are used to make "cuts" in the image.  They can be numbers
    // or percentages.
    RefPtr<Rect> m_imageSliceRect;

    // Values for how to handle the scaling/stretching/tiling of the image slices.
    int m_horizontalSizeRule; // Rule for how to adjust the widths of the top/middle/bottom
    int m_verticalSizeRule; // Rule for how to adjust the heights of the left/middle/right
};

} // namespace WebCore

#endif // CSSBorderImageValue_h
