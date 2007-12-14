/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef CSSValue_h
#define CSSValue_h

#include "StyleBase.h"

namespace WebCore {

typedef int ExceptionCode;

class CSSValue : public StyleBase {
public:
    enum UnitTypes {
        CSS_INHERIT = 0,
        CSS_PRIMITIVE_VALUE = 1,
        CSS_VALUE_LIST = 2,
        CSS_CUSTOM = 3,
        CSS_INITIAL = 4
    };

    CSSValue() : StyleBase(0) { }

    virtual unsigned short cssValueType() const { return CSS_CUSTOM; }

    virtual String cssText() const = 0;
    void setCssText(const String&, ExceptionCode&) { } // FIXME: Not implemented.

    virtual bool isValue() { return true; }
    virtual bool isFontValue() { return false; }
    virtual bool isImplicitInitialValue() const { return false; }
    virtual bool isTransitionTimingFunctionValue() { return false; }
};

} // namespace WebCore

#endif // CSSValue_h
