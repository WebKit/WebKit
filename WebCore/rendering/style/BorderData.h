/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef BorderData_h
#define BorderData_h

#include "BorderValue.h"
#include "IntSize.h"
#include "NinePieceImage.h"

namespace WebCore {

class BorderData {
public:
    BorderValue left;
    BorderValue right;
    BorderValue top;
    BorderValue bottom;

    NinePieceImage image;

    IntSize topLeft;
    IntSize topRight;
    IntSize bottomLeft;
    IntSize bottomRight;

    bool hasBorder() const
    {
        bool haveImage = image.hasImage();
        return left.nonZero(!haveImage) || right.nonZero(!haveImage) || top.nonZero(!haveImage) || bottom.nonZero(!haveImage);
    }

    bool hasBorderRadius() const
    {
        if (topLeft.width() > 0)
            return true;
        if (topRight.width() > 0)
            return true;
        if (bottomLeft.width() > 0)
            return true;
        if (bottomRight.width() > 0)
            return true;
        return false;
    }
    
    unsigned short borderLeftWidth() const
    {
        if (!image.hasImage() && (left.style() == BNONE || left.style() == BHIDDEN))
            return 0; 
        return left.width;
    }
    
    unsigned short borderRightWidth() const
    {
        if (!image.hasImage() && (right.style() == BNONE || right.style() == BHIDDEN))
            return 0;
        return right.width;
    }
    
    unsigned short borderTopWidth() const
    {
        if (!image.hasImage() && (top.style() == BNONE || top.style() == BHIDDEN))
            return 0;
        return top.width;
    }
    
    unsigned short borderBottomWidth() const
    {
        if (!image.hasImage() && (bottom.style() == BNONE || bottom.style() == BHIDDEN))
            return 0;
        return bottom.width;
    }
    
    bool operator==(const BorderData& o) const
    {
        return left == o.left && right == o.right && top == o.top && bottom == o.bottom && image == o.image &&
               topLeft == o.topLeft && topRight == o.topRight && bottomLeft == o.bottomLeft && bottomRight == o.bottomRight;
    }
    
    bool operator!=(const BorderData& o) const
    {
        return !(*this == o);
    }
};

} // namespace WebCore

#endif // BorderData_h
