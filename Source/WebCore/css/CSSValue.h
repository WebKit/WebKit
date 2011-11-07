/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
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
 */

#ifndef CSSValue_h
#define CSSValue_h

#include "KURLHash.h"
#include <wtf/ListHashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSStyleSheet;

typedef int ExceptionCode;

class CSSValue : public RefCounted<CSSValue> {
public:
    enum Type {
        CSS_INHERIT = 0,
        CSS_PRIMITIVE_VALUE = 1,
        CSS_VALUE_LIST = 2,
        CSS_CUSTOM = 3,
        CSS_INITIAL = 4
    };

    virtual ~CSSValue() { }

    Type cssValueType() const { return static_cast<Type>(m_type); }

    virtual String cssText() const = 0;
    void setCssText(const String&, ExceptionCode&) { } // FIXME: Not implemented.

    virtual bool isMutableValue() const { return false; }

    virtual bool isBorderImageValue() const { return false; }
    virtual bool isBorderImageSliceValue() const { return false; }
    virtual bool isCursorImageValue() const { return false; }
#if ENABLE(CSS3_FLEXBOX)
    virtual bool isFlexValue() const { return false; }
#endif
    virtual bool isFontFamilyValue() const { return false; }
    virtual bool isFontFeatureValue() const { return false; }
    virtual bool isFontValue() const { return false; }
    virtual bool isImageGeneratorValue() const { return false; }
    virtual bool isImageValue() const { return false; }
    virtual bool isInheritedValue() const { return false; }
    virtual bool isInitialValue() const { return false; }
    virtual bool isImplicitInitialValue() const { return false; }
    virtual bool isPrimitiveValue() const { return false; }
    virtual bool isReflectValue() const { return false; }
    virtual bool isShadowValue() const { return false; }
    virtual bool isTimingFunctionValue() const { return false; }
    virtual bool isValueList() const { return false; }
    virtual bool isWebKitCSSTransformValue() const { return false; }
    virtual bool isCSSLineBoxContainValue() const { return false; }
#if ENABLE(CSS_FILTERS)
    virtual bool isWebKitCSSFilterValue() const { return false; }
#endif
#if ENABLE(SVG)
    virtual bool isSVGColor() const { return false; }
    virtual bool isSVGPaint() const { return false; }
#endif

    virtual void addSubresourceStyleURLs(ListHashSet<KURL>&, const CSSStyleSheet*) { }

protected:
    CSSValue(Type type = CSS_CUSTOM)
        : m_type(type)
    {
    }

private:
    // FIXME: This class is currently a little bloated, but that will change.
    //        See <http://webkit.org/b/71666> for more information.
    unsigned m_type : 3; // Type
};

} // namespace WebCore

#endif // CSSValue_h
