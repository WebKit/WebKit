/*
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef SVGException_h
#define SVGException_h

#if ENABLE(SVG)

#include "ExceptionBase.h"

namespace WebCore {

    class SVGException : public ExceptionBase {
    public:
        SVGException(const ExceptionCodeDescription& description)
            : ExceptionBase(description)
        {
        }

        static const int SVGExceptionOffset = 300;
        static const int SVGExceptionMax = 399;

        enum SVGExceptionCode {
            SVG_WRONG_TYPE_ERR          = SVGExceptionOffset,
            SVG_INVALID_VALUE_ERR       = SVGExceptionOffset + 1,
            SVG_MATRIX_NOT_INVERTABLE   = SVGExceptionOffset + 2
        };
    };

} // namespace WebCore

#endif // ENABLE(SVG)

#endif // SVGException_h
