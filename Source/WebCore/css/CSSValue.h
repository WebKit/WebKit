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

    Type cssValueType() const;

    virtual String cssText() const = 0;
    void setCssText(const String&, ExceptionCode&) { } // FIXME: Not implemented.

    bool isMutableValue() const { return m_isMutable; }
    bool isPrimitiveValue() const { return m_isPrimitive; }
    bool isValueList() const { return m_isList; }
    bool isInitialValue() const { return m_isInitial; }
    bool isInheritedValue() const { return m_isInherited; }

    bool isBorderImageValue() const { return m_classType == BorderImageClass; }
    bool isBorderImageSliceValue() const { return m_classType == BorderImageSliceClass; }
    bool isCursorImageValue() const { return m_classType == CursorImageClass; }
    bool isFontFamilyValue() const { return m_classType == FontFamilyClass; }
    bool isFontFeatureValue() const { return m_classType == FontFeatureClass; }
    bool isFontValue() const { return m_classType == FontClass; }
    bool isImageGeneratorValue() const { return m_classType == CanvasClass || m_classType == CrossfadeClass || m_classType == LinearGradientClass || m_classType == RadialGradientClass; }
    bool isImageValue() const { return m_classType == ImageClass || m_classType == CursorImageClass; }
    bool isImplicitInitialValue() const { return m_classType == ImplicitInitialClass; }
    bool isReflectValue() const { return m_classType == ReflectClass; }
    bool isShadowValue() const { return m_classType == ShadowClass; }
    bool isTimingFunctionValue() const { return m_classType == CubicBezierTimingFunctionClass || m_classType == LinearTimingFunctionClass || m_classType == StepsTimingFunctionClass; }
    bool isWebKitCSSTransformValue() const { return m_classType == WebKitCSSTransformClass; }
    bool isCSSLineBoxContainValue() const { return m_classType == LineBoxContainClass; }
#if ENABLE(CSS3_FLEXBOX)
    bool isFlexValue() const { return m_classType == FlexClass; }
#endif
#if ENABLE(CSS_FILTERS)
    bool isWebKitCSSFilterValue() const { return m_classType == WebKitCSSFilterClass; }
#endif
#if ENABLE(SVG)
    bool isSVGColor() const { return m_classType == SVGColorClass || m_classType == SVGPaintClass; }
    bool isSVGPaint() const { return m_classType == SVGPaintClass; }
#endif

    virtual void addSubresourceStyleURLs(ListHashSet<KURL>&, const CSSStyleSheet*) { }

protected:

    enum ClassType {
        AspectRatioClass,
        BorderImageClass,
        BorderImageSliceClass,
        CanvasClass,
        CursorImageClass,
        FontFamilyClass,
        FontFeatureClass,
        FontClass,
        FontFaceSrcClass,
        FunctionClass,
        LinearGradientClass,
        RadialGradientClass,
        CrossfadeClass,
        ImageClass,
        InheritedClass,
        InitialClass,
        ImplicitInitialClass,
        PrimitiveClass,
        ReflectClass,
        ShadowClass,
        LinearTimingFunctionClass,
        CubicBezierTimingFunctionClass,
        StepsTimingFunctionClass,
        UnicodeRangeClass,
        ValueListClass,
        WebKitCSSTransformClass,
        LineBoxContainClass,
#if ENABLE(CSS3_FLEXBOX)
        FlexClass,
#endif
#if ENABLE(CSS_FILTERS)
        WebKitCSSFilterClass,
#endif
#if ENABLE(SVG)
        SVGColorClass,
        SVGPaintClass,
#endif
    };

    ClassType classType() const { return static_cast<ClassType>(m_classType); }

    CSSValue(ClassType classType)
        : m_classType(classType)
        , m_isPrimitive(isPrimitiveType(classType))
        , m_isMutable(isMutableType(classType))
        , m_isList(isListType(classType))
        , m_isInitial(isInitialType(classType))
        , m_isInherited(isInheritedType(classType))
    {
    }

private:
    static bool isPrimitiveType(ClassType type)
    {
        return type == PrimitiveClass
            || type == ImageClass
            || type == CursorImageClass
            || type == FontFamilyClass;
    }

    static bool isListType(ClassType type)
    {
        return type == ValueListClass
#if ENABLE(CSS_FILTERS)
            || type == WebKitCSSFilterClass
#endif
            || type == WebKitCSSTransformClass;
    }

    static bool isMutableType(ClassType type)
    {
#if ENABLE(SVG)
        return type == SVGColorClass
            || type == SVGPaintClass;
#else
        UNUSED_PARAM(type);
        return false;
#endif
    }

    static bool isInheritedType(ClassType type)
    {
        return type == InheritedClass;
    }

    static bool isInitialType(ClassType type)
    {
        return type == InitialClass || type == ImplicitInitialClass;
    }

    // FIXME: This class is currently a little bloated, but that will change.
    //        See <http://webkit.org/b/71666> for more information.
    unsigned m_classType : 5; // ClassType
    bool m_isPrimitive : 1;
    bool m_isMutable : 1;
    bool m_isList : 1;
    bool m_isInitial : 1;
    bool m_isInherited : 1;
};

} // namespace WebCore

#endif // CSSValue_h
