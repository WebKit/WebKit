/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "TransformFunctionType.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

TextStream& operator<<(TextStream& ts, TransformFunctionType type)
{
    switch (type) {
    case TransformFunctionType::Matrix3D:
        ts << "Matrix3D";
        break;
    case TransformFunctionType::Matrix:
        ts << "Matrix";
        break;
    case TransformFunctionType::Scale3D:
        ts << "Scale3D";
        break;
    case TransformFunctionType::Scale:
        ts << "Scale";
        break;
    case TransformFunctionType::ScaleX:
        ts << "ScaleX";
        break;
    case TransformFunctionType::ScaleY:
        ts << "ScaleY";
        break;
    case TransformFunctionType::ScaleZ:
        ts << "ScaleZ";
        break;
    case TransformFunctionType::Translate3D:
        ts << "Translate3D";
        break;
    case TransformFunctionType::Translate:
        ts << "Translate";
        break;
    case TransformFunctionType::TranslateX:
        ts << "TranslateX";
        break;
    case TransformFunctionType::TranslateY:
        ts << "TranslateY";
        break;
    case TransformFunctionType::TranslateZ:
        ts << "TranslateZ";
        break;
    case TransformFunctionType::Rotate3D:
        ts << "Rotate3D";
        break;
    case TransformFunctionType::Rotate:
        ts << "Rotate";
        break;
    case TransformFunctionType::RotateX:
        ts << "RotateX";
        break;
    case TransformFunctionType::RotateY:
        ts << "RotateY";
        break;
    case TransformFunctionType::RotateZ:
        ts << "RotateZ";
        break;
    case TransformFunctionType::Skew:
        ts << "Skew";
        break;
    case TransformFunctionType::SkewX:
        ts << "SkewX";
        break;
    case TransformFunctionType::SkewY:
        ts << "SkewY";
        break;
    case TransformFunctionType::Perspective:
        ts << "Perspective";
        break;
    }

    return ts;
}

} // namespace WebCore
