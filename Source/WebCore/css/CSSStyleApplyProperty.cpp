/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSStyleApplyProperty.h"

#include "CSSAspectRatioValue.h"
#include "CSSCursorImageValue.h"
#include "CSSFlexValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSStyleSelector.h"
#include "CSSValueList.h"
#include "CursorList.h"
#include "Document.h"
#include "Element.h"
#include "Pair.h"
#include "RenderObject.h"
#include "RenderStyle.h"
#include "Settings.h"
#include <wtf/StdLibExtras.h>
#include <wtf/UnusedParam.h>

using namespace std;

namespace WebCore {

enum ExpandValueBehavior {SuppressValue = 0, ExpandValue};
template <ExpandValueBehavior expandValue, CSSPropertyID one = CSSPropertyInvalid, CSSPropertyID two = CSSPropertyInvalid, CSSPropertyID three = CSSPropertyInvalid, CSSPropertyID four = CSSPropertyInvalid>
class ApplyPropertyExpanding {
public:

    template <CSSPropertyID id>
    static inline void applyInheritValue(CSSStyleSelector* selector)
    {
        if (id == CSSPropertyInvalid)
            return;

        const CSSStyleApplyProperty& table = CSSStyleApplyProperty::sharedCSSStyleApplyProperty();
        const PropertyHandler& handler = table.propertyHandler(id);
        if (handler.isValid())
            handler.applyInheritValue(selector);
    }

    static void applyInheritValue(CSSStyleSelector* selector)
    {
        applyInheritValue<one>(selector);
        applyInheritValue<two>(selector);
        applyInheritValue<three>(selector);
        applyInheritValue<four>(selector);
    }

    template <CSSPropertyID id>
    static inline void applyInitialValue(CSSStyleSelector* selector)
    {
        if (id == CSSPropertyInvalid)
            return;

        const CSSStyleApplyProperty& table = CSSStyleApplyProperty::sharedCSSStyleApplyProperty();
        const PropertyHandler& handler = table.propertyHandler(id);
        if (handler.isValid())
            handler.applyInitialValue(selector);
    }

    static void applyInitialValue(CSSStyleSelector* selector)
    {
        applyInitialValue<one>(selector);
        applyInitialValue<two>(selector);
        applyInitialValue<three>(selector);
        applyInitialValue<four>(selector);
    }

    template <CSSPropertyID id>
    static inline void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        if (id == CSSPropertyInvalid)
            return;

        const CSSStyleApplyProperty& table = CSSStyleApplyProperty::sharedCSSStyleApplyProperty();
        const PropertyHandler& handler = table.propertyHandler(id);
        if (handler.isValid())
            handler.applyValue(selector, value);
    }

    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        if (!expandValue)
            return;

        applyValue<one>(selector, value);
        applyValue<two>(selector, value);
        applyValue<three>(selector, value);
        applyValue<four>(selector, value);
    }
    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

template <typename GetterType, GetterType (RenderStyle::*getterFunction)() const, typename SetterType, void (RenderStyle::*setterFunction)(SetterType), typename InitialType, InitialType (*initialFunction)()>
class ApplyPropertyDefaultBase {
public:
    static void setValue(RenderStyle* style, SetterType value) { (style->*setterFunction)(value); }
    static GetterType value(RenderStyle* style) { return (style->*getterFunction)(); }
    static InitialType initial() { return (*initialFunction)(); }
    static void applyInheritValue(CSSStyleSelector* selector) { setValue(selector->style(), value(selector->parentStyle())); }
    static void applyInitialValue(CSSStyleSelector* selector) { setValue(selector->style(), initial()); }
    static void applyValue(CSSStyleSelector*, CSSValue*) { }
    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

template <typename GetterType, GetterType (RenderStyle::*getterFunction)() const, typename SetterType, void (RenderStyle::*setterFunction)(SetterType), typename InitialType, InitialType (*initialFunction)()>
class ApplyPropertyDefault {
public:
    static void setValue(RenderStyle* style, SetterType value) { (style->*setterFunction)(value); }
    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        if (value->isPrimitiveValue())
            setValue(selector->style(), *static_cast<CSSPrimitiveValue*>(value));
    }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<GetterType, getterFunction, SetterType, setterFunction, InitialType, initialFunction>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

template <typename NumberType, NumberType (RenderStyle::*getterFunction)() const, void (RenderStyle::*setterFunction)(NumberType), NumberType (*initialFunction)(), int idMapsToMinusOne = CSSValueAuto>
class ApplyPropertyNumber {
public:
    static void setValue(RenderStyle* style, NumberType value) { (style->*setterFunction)(value); }
    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        if (primitiveValue->getIdent() == idMapsToMinusOne)
            setValue(selector->style(), -1);
        else
            setValue(selector->style(), primitiveValue->getValue<NumberType>(CSSPrimitiveValue::CSS_NUMBER));
    }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<NumberType, getterFunction, NumberType, setterFunction, NumberType, initialFunction>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

template <StyleImage* (RenderStyle::*getterFunction)() const, void (RenderStyle::*setterFunction)(PassRefPtr<StyleImage>), StyleImage* (*initialFunction)(), CSSPropertyID property>
class ApplyPropertyStyleImage {
public:
    static void applyValue(CSSStyleSelector* selector, CSSValue* value) { (selector->style()->*setterFunction)(selector->styleImage(property, value)); }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<StyleImage*, getterFunction, PassRefPtr<StyleImage>, setterFunction, StyleImage*, initialFunction>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

enum AutoValueType {Number = 0, ComputeLength};
template <typename T, T (RenderStyle::*getterFunction)() const, void (RenderStyle::*setterFunction)(T), bool (RenderStyle::*hasAutoFunction)() const, void (RenderStyle::*setAutoFunction)(), AutoValueType valueType = Number, int autoIdentity = CSSValueAuto>
class ApplyPropertyAuto {
public:
    static void setValue(RenderStyle* style, T value) { (style->*setterFunction)(value); }
    static T value(RenderStyle* style) { return (style->*getterFunction)(); }
    static bool hasAuto(RenderStyle* style) { return (style->*hasAutoFunction)(); }
    static void setAuto(RenderStyle* style) { (style->*setAutoFunction)(); }

    static void applyInheritValue(CSSStyleSelector* selector)
    {
        if (hasAuto(selector->parentStyle()))
            setAuto(selector->style());
        else
            setValue(selector->style(), value(selector->parentStyle()));
    }

    static void applyInitialValue(CSSStyleSelector* selector) { setAuto(selector->style()); }

    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        if (primitiveValue->getIdent() == autoIdentity)
            setAuto(selector->style());
        else if (valueType == Number)
            setValue(selector->style(), *primitiveValue);
        else if (valueType == ComputeLength)
            setValue(selector->style(), primitiveValue->computeLength<T>(selector->style(), selector->rootElementStyle(), selector->style()->effectiveZoom()));
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

enum ColorInherit {NoInheritFromParent = 0, InheritFromParent};
Color defaultInitialColor();
Color defaultInitialColor() { return Color(); }
template <ColorInherit inheritColorFromParent,
          const Color& (RenderStyle::*getterFunction)() const,
          void (RenderStyle::*setterFunction)(const Color&),
          void (RenderStyle::*visitedLinkSetterFunction)(const Color&),
          const Color& (RenderStyle::*defaultFunction)() const,
          Color (*initialFunction)() = &defaultInitialColor>
class ApplyPropertyColor {
public:
    static void applyInheritValue(CSSStyleSelector* selector)
    {
        // Visited link style can never explicitly inherit from parent visited link style so no separate getters are needed.
        const Color& color = (selector->parentStyle()->*getterFunction)();
        applyColorValue(selector, color.isValid() ? color : (selector->parentStyle()->*defaultFunction)());
    }

    static void applyInitialValue(CSSStyleSelector* selector)
    {
        applyColorValue(selector, initialFunction());
    }

    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        if (inheritColorFromParent && primitiveValue->getIdent() == CSSValueCurrentcolor)
            applyInheritValue(selector);
        else {
            if (selector->applyPropertyToRegularStyle())
                (selector->style()->*setterFunction)(selector->colorFromPrimitiveValue(primitiveValue));
            if (selector->applyPropertyToVisitedLinkStyle())
                (selector->style()->*visitedLinkSetterFunction)(selector->colorFromPrimitiveValue(primitiveValue, /* forVisitedLink */ true));
        }
    }

    static void applyColorValue(CSSStyleSelector* selector, const Color& color)
    {
        if (selector->applyPropertyToRegularStyle())
            (selector->style()->*setterFunction)(color);
        if (selector->applyPropertyToVisitedLinkStyle())
            (selector->style()->*visitedLinkSetterFunction)(color);
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

template <TextDirection (RenderStyle::*getterFunction)() const, void (RenderStyle::*setterFunction)(TextDirection), TextDirection (*initialFunction)()>
class ApplyPropertyDirection {
public:
    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        ApplyPropertyDefault<TextDirection, getterFunction, TextDirection, setterFunction, TextDirection, initialFunction>::applyValue(selector, value);
        Element* element = selector->element();
        if (element && selector->element() == element->document()->documentElement())
            element->document()->setDirectionSetOnDocumentElement(true);
    }

    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefault<TextDirection, getterFunction, TextDirection, setterFunction, TextDirection, initialFunction>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

enum LengthAuto { AutoDisabled = 0, AutoEnabled };
enum LengthIntrinsic { IntrinsicDisabled = 0, IntrinsicEnabled };
enum LengthMinIntrinsic { MinIntrinsicDisabled = 0, MinIntrinsicEnabled };
enum LengthNone { NoneDisabled = 0, NoneEnabled };
enum LengthUndefined { UndefinedDisabled = 0, UndefinedEnabled };
enum LengthFlexDirection { FlexDirectionDisabled = 0, FlexWidth, FlexHeight };
template <Length (RenderStyle::*getterFunction)() const,
          void (RenderStyle::*setterFunction)(Length),
          Length (*initialFunction)(),
          LengthAuto autoEnabled = AutoDisabled,
          LengthIntrinsic intrinsicEnabled = IntrinsicDisabled,
          LengthMinIntrinsic minIntrinsicEnabled = MinIntrinsicDisabled,
          LengthNone noneEnabled = NoneDisabled,
          LengthUndefined noneUndefined = UndefinedDisabled,
          LengthFlexDirection flexDirection = FlexDirectionDisabled>
class ApplyPropertyLength {
public:
    static void setValue(RenderStyle* style, Length value) { (style->*setterFunction)(value); }
    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        if (!value->isPrimitiveValue()) {
            if (!flexDirection || !value->isFlexValue())
                return;

            CSSFlexValue* flexValue = static_cast<CSSFlexValue*>(value);
            value = flexValue->preferredSize();

            if (flexDirection == FlexWidth) {
                selector->style()->setFlexboxWidthPositiveFlex(flexValue->positiveFlex());
                selector->style()->setFlexboxWidthNegativeFlex(flexValue->negativeFlex());
            } else if (flexDirection == FlexHeight) {
                selector->style()->setFlexboxHeightPositiveFlex(flexValue->positiveFlex());
                selector->style()->setFlexboxHeightNegativeFlex(flexValue->negativeFlex());
            }
        }

        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        if (noneEnabled && primitiveValue->getIdent() == CSSValueNone)
            if (noneUndefined)
                setValue(selector->style(), Length(Undefined));
            else
                setValue(selector->style(), Length());
        else if (intrinsicEnabled && primitiveValue->getIdent() == CSSValueIntrinsic)
            setValue(selector->style(), Length(Intrinsic));
        else if (minIntrinsicEnabled && primitiveValue->getIdent() == CSSValueMinIntrinsic)
            setValue(selector->style(), Length(MinIntrinsic));
        else if (autoEnabled && primitiveValue->getIdent() == CSSValueAuto)
            setValue(selector->style(), Length());
        else {
            int type = primitiveValue->primitiveType();
            if (CSSPrimitiveValue::isUnitTypeLength(type)) {
                Length length = primitiveValue->computeLength<Length>(selector->style(), selector->rootElementStyle(), selector->style()->effectiveZoom());
                length.setQuirk(primitiveValue->isQuirkValue());
                setValue(selector->style(), length);
            } else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
                setValue(selector->style(), Length(primitiveValue->getDoubleValue(), Percent));
        }
    }

    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<Length, getterFunction, Length, setterFunction, Length, initialFunction>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

enum StringIdentBehavior { NothingMapsToNull = 0, MapNoneToNull, MapAutoToNull };
template <StringIdentBehavior identBehavior, const AtomicString& (RenderStyle::*getterFunction)() const, void (RenderStyle::*setterFunction)(const AtomicString&), const AtomicString& (*initialFunction)()>
class ApplyPropertyString {
public:
    static void setValue(RenderStyle* style, const AtomicString& value) { (style->*setterFunction)(value); }
    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        if ((identBehavior == MapNoneToNull && primitiveValue->getIdent() == CSSValueNone)
            || (identBehavior == MapAutoToNull && primitiveValue->getIdent() == CSSValueAuto))
            setValue(selector->style(), nullAtom);
        else
            setValue(selector->style(), primitiveValue->getStringValue());
    }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<const AtomicString&, getterFunction, const AtomicString&, setterFunction, const AtomicString&, initialFunction>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

template <LengthSize (RenderStyle::*getterFunction)() const, void (RenderStyle::*setterFunction)(LengthSize), LengthSize (*initialFunction)()>
class ApplyPropertyBorderRadius {
public:
    static void setValue(RenderStyle* style, LengthSize value) { (style->*setterFunction)(value); }
    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        Pair* pair = primitiveValue->getPairValue();
        if (!pair || !pair->first() || !pair->second())
            return;

        Length radiusWidth;
        Length radiusHeight;
        if (pair->first()->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
            radiusWidth = Length(pair->first()->getDoubleValue(), Percent);
        else
            radiusWidth = Length(max(intMinForLength, min(intMaxForLength, pair->first()->computeLength<int>(selector->style(), selector->rootElementStyle(), selector->style()->effectiveZoom()))), Fixed);
        if (pair->second()->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
            radiusHeight = Length(pair->second()->getDoubleValue(), Percent);
        else
            radiusHeight = Length(max(intMinForLength, min(intMaxForLength, pair->second()->computeLength<int>(selector->style(), selector->rootElementStyle(), selector->style()->effectiveZoom()))), Fixed);
        int width = radiusWidth.value();
        int height = radiusHeight.value();
        if (width < 0 || height < 0)
            return;
        if (!width)
            radiusHeight = radiusWidth; // Null out the other value.
        else if (!height)
            radiusWidth = radiusHeight; // Null out the other value.

        LengthSize size(radiusWidth, radiusHeight);
        setValue(selector->style(), size);
    }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<LengthSize, getterFunction, LengthSize, setterFunction, LengthSize, initialFunction>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

template <typename T>
struct FillLayerAccessorTypes {
    typedef T Setter;
    typedef T Getter;
};

template <>
struct FillLayerAccessorTypes<StyleImage*> {
    typedef PassRefPtr<StyleImage> Setter;
    typedef StyleImage* Getter;
};

template <typename T,
          CSSPropertyID propertyId,
          EFillLayerType fillLayerType,
          FillLayer* (RenderStyle::*accessLayersFunction)(),
          const FillLayer* (RenderStyle::*layersFunction)() const,
          bool (FillLayer::*testFunction)() const,
          typename FillLayerAccessorTypes<T>::Getter (FillLayer::*getFunction)() const,
          void (FillLayer::*setFunction)(typename FillLayerAccessorTypes<T>::Setter),
          void (FillLayer::*clearFunction)(),
          typename FillLayerAccessorTypes<T>::Getter (*initialFunction)(EFillLayerType),
          void (CSSStyleSelector::*mapFillFunction)(CSSPropertyID, FillLayer*, CSSValue*)>
class ApplyPropertyFillLayer {
public:
    static void applyInheritValue(CSSStyleSelector* selector)
    {
        FillLayer* currChild = (selector->style()->*accessLayersFunction)();
        FillLayer* prevChild = 0;
        const FillLayer* currParent = (selector->parentStyle()->*layersFunction)();
        while (currParent && (currParent->*testFunction)()) {
            if (!currChild) {
                /* Need to make a new layer.*/
                currChild = new FillLayer(fillLayerType);
                prevChild->setNext(currChild);
            }
            (currChild->*setFunction)((currParent->*getFunction)());
            prevChild = currChild;
            currChild = prevChild->next();
            currParent = currParent->next();
        }

        while (currChild) {
            /* Reset any remaining layers to not have the property set. */
            (currChild->*clearFunction)();
            currChild = currChild->next();
        }
    }

    static void applyInitialValue(CSSStyleSelector* selector)
    {
        FillLayer* currChild = (selector->style()->*accessLayersFunction)();
        (currChild->*setFunction)((*initialFunction)(fillLayerType));
        for (currChild = currChild->next(); currChild; currChild = currChild->next())
            (currChild->*clearFunction)();
    }

    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        FillLayer* currChild = (selector->style()->*accessLayersFunction)();
        FillLayer* prevChild = 0;
        if (value->isValueList()) {
            /* Walk each value and put it into a layer, creating new layers as needed. */
            CSSValueList* valueList = static_cast<CSSValueList*>(value);
            for (unsigned int i = 0; i < valueList->length(); i++) {
                if (!currChild) {
                    /* Need to make a new layer to hold this value */
                    currChild = new FillLayer(fillLayerType);
                    prevChild->setNext(currChild);
                }
                (selector->*mapFillFunction)(propertyId, currChild, valueList->itemWithoutBoundsCheck(i));
                prevChild = currChild;
                currChild = currChild->next();
            }
        } else {
            (selector->*mapFillFunction)(propertyId, currChild, value);
            currChild = currChild->next();
        }
        while (currChild) {
            /* Reset all remaining layers to not have the property set. */
            (currChild->*clearFunction)();
            currChild = currChild->next();
        }
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

enum ComputeLengthNormal {NormalDisabled = 0, NormalEnabled};
enum ComputeLengthThickness {ThicknessDisabled = 0, ThicknessEnabled};
enum ComputeLengthSVGZoom {SVGZoomDisabled = 0, SVGZoomEnabled};
template <typename T,
          T (RenderStyle::*getterFunction)() const,
          void (RenderStyle::*setterFunction)(T),
          T (*initialFunction)(),
          ComputeLengthNormal normalEnabled = NormalDisabled,
          ComputeLengthThickness thicknessEnabled = ThicknessDisabled,
          ComputeLengthSVGZoom svgZoomEnabled = SVGZoomDisabled>
class ApplyPropertyComputeLength {
public:
    static void setValue(RenderStyle* style, T value) { (style->*setterFunction)(value); }
    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        // note: CSSPropertyLetter/WordSpacing right now sets to zero if it's not a primitive value for some reason...
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);

        int ident = primitiveValue->getIdent();
        T length;
        if (normalEnabled && ident == CSSValueNormal) {
            length = 0;
        } else if (thicknessEnabled && ident == CSSValueThin) {
            length = 1;
        } else if (thicknessEnabled && ident == CSSValueMedium) {
            length = 3;
        } else if (thicknessEnabled && ident == CSSValueThick) {
            length = 5;
        } else if (ident == CSSValueInvalid) {
            float zoom = (svgZoomEnabled && selector->useSVGZoomRules()) ? 1.0f : selector->style()->effectiveZoom();
            length = primitiveValue->computeLength<T>(selector->style(), selector->rootElementStyle(), zoom);
        } else {
            ASSERT_NOT_REACHED();
            length = 0;
        }

        setValue(selector->style(), length);
    }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<T, getterFunction, T, setterFunction, T, initialFunction>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

template <typename T, T (FontDescription::*getterFunction)() const, void (FontDescription::*setterFunction)(T), T initialValue>
class ApplyPropertyFont {
public:
    static void applyInheritValue(CSSStyleSelector* selector)
    {
        FontDescription fontDescription = selector->fontDescription();
        (fontDescription.*setterFunction)((selector->parentFontDescription().*getterFunction)());
        selector->setFontDescription(fontDescription);
    }

    static void applyInitialValue(CSSStyleSelector* selector)
    {
        FontDescription fontDescription = selector->fontDescription();
        (fontDescription.*setterFunction)(initialValue);
        selector->setFontDescription(fontDescription);
    }

    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        FontDescription fontDescription = selector->fontDescription();
        (fontDescription.*setterFunction)(*primitiveValue);
        selector->setFontDescription(fontDescription);
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

class ApplyPropertyFontSize {
private:
    // When the CSS keyword "larger" is used, this function will attempt to match within the keyword
    // table, and failing that, will simply multiply by 1.2.
    static float largerFontSize(float size)
    {
        // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale up to
        // the next size level.
        return size * 1.2f;
    }

    // Like the previous function, but for the keyword "smaller".
    static float smallerFontSize(float size)
    {
        // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale down to
        // the next size level.
        return size / 1.2f;
    }
public:
    static void applyInheritValue(CSSStyleSelector* selector)
    {
        float size = selector->parentStyle()->fontDescription().specifiedSize();

        if (size < 0)
            return;

        FontDescription fontDescription = selector->style()->fontDescription();
        fontDescription.setKeywordSize(selector->parentStyle()->fontDescription().keywordSize());
        selector->setFontSize(fontDescription, size);
        selector->setFontDescription(fontDescription);
        return;
    }

    static void applyInitialValue(CSSStyleSelector* selector)
    {
        FontDescription fontDescription = selector->style()->fontDescription();
        float size = selector->fontSizeForKeyword(selector->document(), CSSValueMedium, fontDescription.useFixedDefaultSize());

        if (size < 0)
            return;

        fontDescription.setKeywordSize(CSSValueMedium - CSSValueXxSmall + 1);
        selector->setFontSize(fontDescription, size);
        selector->setFontDescription(fontDescription);
        return;
    }

    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);

        FontDescription fontDescription = selector->style()->fontDescription();
        fontDescription.setKeywordSize(0);
        float parentSize = 0;
        bool parentIsAbsoluteSize = false;
        float size = 0;

        if (selector->hasParentNode()) {
            parentSize = selector->parentStyle()->fontDescription().specifiedSize();
            parentIsAbsoluteSize = selector->parentStyle()->fontDescription().isAbsoluteSize();
        }

        if (int ident = primitiveValue->getIdent()) {
            // Keywords are being used.
            switch (ident) {
            case CSSValueXxSmall:
            case CSSValueXSmall:
            case CSSValueSmall:
            case CSSValueMedium:
            case CSSValueLarge:
            case CSSValueXLarge:
            case CSSValueXxLarge:
            case CSSValueWebkitXxxLarge:
                size = selector->fontSizeForKeyword(selector->document(), ident, fontDescription.useFixedDefaultSize());
                fontDescription.setKeywordSize(ident - CSSValueXxSmall + 1);
                break;
            case CSSValueLarger:
                size = largerFontSize(parentSize);
                break;
            case CSSValueSmaller:
                size = smallerFontSize(parentSize);
                break;
            default:
                return;
            }

            fontDescription.setIsAbsoluteSize(parentIsAbsoluteSize && (ident == CSSValueLarger || ident == CSSValueSmaller));
        } else {
            int type = primitiveValue->primitiveType();
            fontDescription.setIsAbsoluteSize(parentIsAbsoluteSize
                                              || (type != CSSPrimitiveValue::CSS_PERCENTAGE
                                                  && type != CSSPrimitiveValue::CSS_EMS
                                                  && type != CSSPrimitiveValue::CSS_EXS
                                                  && type != CSSPrimitiveValue::CSS_REMS));
            if (primitiveValue->isLength())
                size = primitiveValue->computeLength<float>(selector->parentStyle(), selector->rootElementStyle(), 1.0, true);
            else if (primitiveValue->isPercentage())
                size = (primitiveValue->getFloatValue() * parentSize) / 100.0f;
            else
                return;
        }

        if (size < 0)
            return;

        selector->setFontSize(fontDescription, size);
        selector->setFontDescription(fontDescription);
        return;
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

class ApplyPropertyFontWeight {
public:
    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        FontDescription fontDescription = selector->fontDescription();
        switch (primitiveValue->getIdent()) {
        case CSSValueInvalid:
            ASSERT_NOT_REACHED();
            break;
        case CSSValueBolder:
            fontDescription.setWeight(fontDescription.bolderWeight());
            break;
        case CSSValueLighter:
            fontDescription.setWeight(fontDescription.lighterWeight());
            break;
        default:
            fontDescription.setWeight(*primitiveValue);
        }
        selector->setFontDescription(fontDescription);
    }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyFont<FontWeight, &FontDescription::weight, &FontDescription::setWeight, FontWeightNormal>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

enum BorderImageType { Image = 0, Mask };
template <BorderImageType borderImageType,
          CSSPropertyID property,
          const NinePieceImage& (RenderStyle::*getterFunction)() const,
          void (RenderStyle::*setterFunction)(const NinePieceImage&)>
class ApplyPropertyBorderImage {
public:
    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        NinePieceImage image;
        if (borderImageType == Mask)
            image.setMaskDefaults();
        selector->mapNinePieceImage(property, value, image);
        (selector->style()->*setterFunction)(image);
    }

    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<const NinePieceImage&, getterFunction, const NinePieceImage&, setterFunction, NinePieceImage, &RenderStyle::initialNinePieceImage>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

enum BorderImageModifierType { Outset, Repeat, Slice, Width };
template <BorderImageType type, BorderImageModifierType modifier>
class ApplyPropertyBorderImageModifier {
private:
    static inline const NinePieceImage& getValue(RenderStyle* style) { return type == Image ? style->borderImage() : style->maskBoxImage(); }
    static inline void setValue(RenderStyle* style, const NinePieceImage& value) { return type == Image ? style->setBorderImage(value) : style->setMaskBoxImage(value); }
public:
    static void applyInheritValue(CSSStyleSelector* selector)
    {
        NinePieceImage image(getValue(selector->style()));
        switch (modifier) {
        case Outset:
            image.copyOutsetFrom(getValue(selector->parentStyle()));
            break;
        case Repeat:
            image.copyRepeatFrom(getValue(selector->parentStyle()));
            break;
        case Slice:
            image.copyImageSlicesFrom(getValue(selector->parentStyle()));
            break;
        case Width:
            image.copyBorderSlicesFrom(getValue(selector->parentStyle()));
            break;
        }
        setValue(selector->style(), image);
    }

    static void applyInitialValue(CSSStyleSelector* selector)
    {
        NinePieceImage image(getValue(selector->style()));
        switch (modifier) {
        case Outset:
            image.setOutset(LengthBox());
            break;
        case Repeat:
            image.setHorizontalRule(StretchImageRule);
            image.setVerticalRule(StretchImageRule);
            break;
        case Slice:
            // Masks have a different initial value for slices. Preserve the value of 0 for backwards compatibility.
            image.setImageSlices(type == Image ? LengthBox(Length(100, Percent), Length(100, Percent), Length(100, Percent), Length(100, Percent)) : LengthBox());
            image.setFill(false);
            break;
        case Width:
            // Masks have a different initial value for widths. They use an 'auto' value rather than trying to fit to the border.
            image.setBorderSlices(type == Image ? LengthBox(Length(1, Relative), Length(1, Relative), Length(1, Relative), Length(1, Relative)) : LengthBox());
            break;
        }
        setValue(selector->style(), image);
    }

    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        NinePieceImage image(getValue(selector->style()));
        switch (modifier) {
        case Outset:
            image.setOutset(selector->mapNinePieceImageQuad(value));
            break;
        case Repeat:
            selector->mapNinePieceImageRepeat(value, image);
            break;
        case Slice:
            selector->mapNinePieceImageSlice(value, image);
            break;
        case Width:
            image.setBorderSlices(selector->mapNinePieceImageQuad(value));
            break;
        }
        setValue(selector->style(), image);
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

template <CSSPropertyID id, StyleImage* (RenderStyle::*getterFunction)() const, void (RenderStyle::*setterFunction)(PassRefPtr<StyleImage>), StyleImage* (*initialFunction)()>
class ApplyPropertyBorderImageSource {
public:
    static void applyValue(CSSStyleSelector* selector, CSSValue* value) { (selector->style()->*setterFunction)(selector->styleImage(id, value)); }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<StyleImage*, getterFunction, PassRefPtr<StyleImage>, setterFunction, StyleImage*, initialFunction>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

enum CounterBehavior {Increment = 0, Reset};
template <CounterBehavior counterBehavior>
class ApplyPropertyCounter {
public:
    static void emptyFunction(CSSStyleSelector*) { }
    static void applyInheritValue(CSSStyleSelector* selector)
    {
        CounterDirectiveMap& map = selector->style()->accessCounterDirectives();
        CounterDirectiveMap& parentMap = selector->parentStyle()->accessCounterDirectives();

        typedef CounterDirectiveMap::iterator Iterator;
        Iterator end = parentMap.end();
        for (Iterator it = parentMap.begin(); it != end; ++it) {
            CounterDirectives& directives = map.add(it->first, CounterDirectives()).first->second;
            if (counterBehavior == Reset) {
                directives.m_reset = it->second.m_reset;
                directives.m_resetValue = it->second.m_resetValue;
            } else {
                // Inheriting a counter-increment means taking the parent's current value for the counter
                // and adding it to itself.
                directives.m_increment = it->second.m_increment;
                directives.m_incrementValue = 0;
                if (directives.m_increment) {
                    float incrementValue = directives.m_incrementValue;
                    directives.m_incrementValue = clampToInteger(incrementValue + it->second.m_incrementValue);
                } else {
                    directives.m_increment = true;
                    directives.m_incrementValue = it->second.m_incrementValue;
                }
            }
        }
    }
    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        if (!value->isValueList())
            return;

        CSSValueList* list = static_cast<CSSValueList*>(value);

        CounterDirectiveMap& map = selector->style()->accessCounterDirectives();
        typedef CounterDirectiveMap::iterator Iterator;

        Iterator end = map.end();
        for (Iterator it = map.begin(); it != end; ++it)
            if (counterBehavior == Reset)
                it->second.m_reset = false;
            else
                it->second.m_increment = false;

        int length = list ? list->length() : 0;
        for (int i = 0; i < length; ++i) {
            CSSValue* currValue = list->itemWithoutBoundsCheck(i);
            if (!currValue->isPrimitiveValue())
                continue;

            Pair* pair = static_cast<CSSPrimitiveValue*>(currValue)->getPairValue();
            if (!pair || !pair->first() || !pair->second())
                continue;

            AtomicString identifier = static_cast<CSSPrimitiveValue*>(pair->first())->getStringValue();
            int value = static_cast<CSSPrimitiveValue*>(pair->second())->getIntValue();
            CounterDirectives& directives = map.add(identifier.impl(), CounterDirectives()).first->second;
            if (counterBehavior == Reset) {
                directives.m_reset = true;
                directives.m_resetValue = value;
            } else {
                if (directives.m_increment) {
                    float incrementValue = directives.m_incrementValue;
                    directives.m_incrementValue = clampToInteger(incrementValue + value);
                } else {
                    directives.m_increment = true;
                    directives.m_incrementValue = value;
                }
            }
            
        }
    }
    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &emptyFunction, &applyValue); }
};


class ApplyPropertyCursor {
public:
    static void applyInheritValue(CSSStyleSelector* selector)
    {
        selector->style()->setCursor(selector->parentStyle()->cursor());
        selector->style()->setCursorList(selector->parentStyle()->cursors());
    }

    static void applyInitialValue(CSSStyleSelector* selector)
    {
        selector->style()->clearCursorList();
        selector->style()->setCursor(RenderStyle::initialCursor());
    }

    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        selector->style()->clearCursorList();
        if (value->isValueList()) {
            CSSValueList* list = static_cast<CSSValueList*>(value);
            int len = list->length();
            selector->style()->setCursor(CURSOR_AUTO);
            for (int i = 0; i < len; i++) {
                CSSValue* item = list->itemWithoutBoundsCheck(i);
                if (!item->isPrimitiveValue())
                    continue;
                CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(item);
                int type = primitiveValue->primitiveType();
                if (type == CSSPrimitiveValue::CSS_URI) {
                    if (primitiveValue->isCursorImageValue()) {
                        CSSCursorImageValue* image = static_cast<CSSCursorImageValue*>(primitiveValue);
                        if (image->updateIfSVGCursorIsUsed(selector->element())) // Elements with SVG cursors are not allowed to share style.
                            selector->style()->setUnique();
                        selector->style()->addCursor(selector->cachedOrPendingFromValue(CSSPropertyCursor, image), image->hotSpot());
                    }
                } else if (type == CSSPrimitiveValue::CSS_IDENT)
                    selector->style()->setCursor(*primitiveValue);
            }
        } else if (value->isPrimitiveValue()) {
            CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_IDENT && selector->style()->cursor() != ECursor(*primitiveValue))
                selector->style()->setCursor(*primitiveValue);
        }
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

class ApplyPropertyTextAlign {
public:
    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);

        if (primitiveValue->getIdent() != CSSValueWebkitMatchParent)
            selector->style()->setTextAlign(*primitiveValue);
        else if (selector->parentStyle()->textAlign() == TASTART)
            selector->style()->setTextAlign(selector->parentStyle()->isLeftToRightDirection() ? LEFT : RIGHT);
        else if (selector->parentStyle()->textAlign() == TAEND)
            selector->style()->setTextAlign(selector->parentStyle()->isLeftToRightDirection() ? RIGHT : LEFT);
        else
            selector->style()->setTextAlign(selector->parentStyle()->textAlign());
    }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<ETextAlign, &RenderStyle::textAlign, ETextAlign, &RenderStyle::setTextAlign, ETextAlign, &RenderStyle::initialTextAlign>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

class ApplyPropertyTextDecoration {
public:
    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        ETextDecoration t = RenderStyle::initialTextDecoration();
        for (CSSValueListIterator i(value); i.hasMore(); i.advance()) {
            CSSValue* item = i.value();
            ASSERT(item->isPrimitiveValue());
            t |= *static_cast<CSSPrimitiveValue*>(item);
        }
        selector->style()->setTextDecoration(t);
    }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<ETextDecoration, &RenderStyle::textDecoration, ETextDecoration, &RenderStyle::setTextDecoration, ETextDecoration, &RenderStyle::initialTextDecoration>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

class ApplyPropertyLineHeight {
public:
    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        Length lineHeight;

        if (primitiveValue->getIdent() == CSSValueNormal)
            lineHeight = Length(-100.0, Percent);
        else if (primitiveValue->isLength()) {
            double multiplier = selector->style()->effectiveZoom();
            if (selector->style()->textSizeAdjust()) {
                if (Frame* frame = selector->document()->frame())
                    multiplier *= frame->textZoomFactor();
            }
            lineHeight = primitiveValue->computeLength<Length>(selector->style(), selector->rootElementStyle(), multiplier);
        } else if (primitiveValue->isPercentage()) {
            // FIXME: percentage should not be restricted to an integer here.
            lineHeight = Length((selector->style()->fontSize() * primitiveValue->getIntValue()) / 100, Fixed);
        } else if (primitiveValue->isNumber()) {
            // FIXME: number and percentage values should produce the same type of Length (ie. Fixed or Percent).
            lineHeight = Length(primitiveValue->getDoubleValue() * 100.0, Percent);
        } else
            return;
        selector->style()->setLineHeight(lineHeight);
    }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<Length, &RenderStyle::lineHeight, Length, &RenderStyle::setLineHeight, Length, &RenderStyle::initialLineHeight>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

class ApplyPropertyPageSize {
private:
    static Length mmLength(double mm) { return CSSPrimitiveValue::create(mm, CSSPrimitiveValue::CSS_MM)->computeLength<Length>(0, 0); }
    static Length inchLength(double inch) { return CSSPrimitiveValue::create(inch, CSSPrimitiveValue::CSS_IN)->computeLength<Length>(0, 0); }
    static bool getPageSizeFromName(CSSPrimitiveValue* pageSizeName, CSSPrimitiveValue* pageOrientation, Length& width, Length& height)
    {
        static const Length a5Width = mmLength(148), a5Height = mmLength(210);
        static const Length a4Width = mmLength(210), a4Height = mmLength(297);
        static const Length a3Width = mmLength(297), a3Height = mmLength(420);
        static const Length b5Width = mmLength(176), b5Height = mmLength(250);
        static const Length b4Width = mmLength(250), b4Height = mmLength(353);
        static const Length letterWidth = inchLength(8.5), letterHeight = inchLength(11);
        static const Length legalWidth = inchLength(8.5), legalHeight = inchLength(14);
        static const Length ledgerWidth = inchLength(11), ledgerHeight = inchLength(17);

        if (!pageSizeName)
            return false;

        switch (pageSizeName->getIdent()) {
        case CSSValueA5:
            width = a5Width;
            height = a5Height;
            break;
        case CSSValueA4:
            width = a4Width;
            height = a4Height;
            break;
        case CSSValueA3:
            width = a3Width;
            height = a3Height;
            break;
        case CSSValueB5:
            width = b5Width;
            height = b5Height;
            break;
        case CSSValueB4:
            width = b4Width;
            height = b4Height;
            break;
        case CSSValueLetter:
            width = letterWidth;
            height = letterHeight;
            break;
        case CSSValueLegal:
            width = legalWidth;
            height = legalHeight;
            break;
        case CSSValueLedger:
            width = ledgerWidth;
            height = ledgerHeight;
            break;
        default:
            return false;
        }

        if (pageOrientation) {
            switch (pageOrientation->getIdent()) {
            case CSSValueLandscape:
                std::swap(width, height);
                break;
            case CSSValuePortrait:
                // Nothing to do.
                break;
            default:
                return false;
            }
        }
        return true;
    }
public:
    static void applyInheritValue(CSSStyleSelector*) { }
    static void applyInitialValue(CSSStyleSelector*) { }
    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        selector->style()->resetPageSizeType();
        Length width;
        Length height;
        PageSizeType pageSizeType = PAGE_SIZE_AUTO;
        CSSValueListInspector inspector(value);
        switch (inspector.length()) {
        case 2: {
            // <length>{2} | <page-size> <orientation>
            if (!inspector.first()->isPrimitiveValue() || !inspector.second()->isPrimitiveValue())
                return;
            CSSPrimitiveValue* first = static_cast<CSSPrimitiveValue*>(inspector.first());
            CSSPrimitiveValue* second = static_cast<CSSPrimitiveValue*>(inspector.second());
            if (first->isLength()) {
                // <length>{2}
                if (!second->isLength())
                    return;
                width = first->computeLength<Length>(selector->style(), selector->rootElementStyle());
                height = second->computeLength<Length>(selector->style(), selector->rootElementStyle());
            } else {
                // <page-size> <orientation>
                // The value order is guaranteed. See CSSParser::parseSizeParameter.
                if (!getPageSizeFromName(first, second, width, height))
                    return;
            }
            pageSizeType = PAGE_SIZE_RESOLVED;
            break;
        }
        case 1: {
            // <length> | auto | <page-size> | [ portrait | landscape]
            if (!inspector.first()->isPrimitiveValue())
                return;
            CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(inspector.first());
            if (primitiveValue->isLength()) {
                // <length>
                pageSizeType = PAGE_SIZE_RESOLVED;
                width = height = primitiveValue->computeLength<Length>(selector->style(), selector->rootElementStyle());
            } else {
                if (primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_IDENT)
                    return;
                switch (primitiveValue->getIdent()) {
                case CSSValueAuto:
                    pageSizeType = PAGE_SIZE_AUTO;
                    break;
                case CSSValuePortrait:
                    pageSizeType = PAGE_SIZE_AUTO_PORTRAIT;
                    break;
                case CSSValueLandscape:
                    pageSizeType = PAGE_SIZE_AUTO_LANDSCAPE;
                    break;
                default:
                    // <page-size>
                    pageSizeType = PAGE_SIZE_RESOLVED;
                    if (!getPageSizeFromName(primitiveValue, 0, width, height))
                        return;
                }
            }
            break;
        }
        default:
            return;
        }
        selector->style()->setPageSizeType(pageSizeType);
        selector->style()->setPageSize(LengthSize(width, height));
    }
    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

class ApplyPropertyTextEmphasisStyle {
public:
    static void applyInheritValue(CSSStyleSelector* selector)
    {
        selector->style()->setTextEmphasisFill(selector->parentStyle()->textEmphasisFill());
        selector->style()->setTextEmphasisMark(selector->parentStyle()->textEmphasisMark());
        selector->style()->setTextEmphasisCustomMark(selector->parentStyle()->textEmphasisCustomMark());
    }

    static void applyInitialValue(CSSStyleSelector* selector)
    {
        selector->style()->setTextEmphasisFill(RenderStyle::initialTextEmphasisFill());
        selector->style()->setTextEmphasisMark(RenderStyle::initialTextEmphasisMark());
        selector->style()->setTextEmphasisCustomMark(RenderStyle::initialTextEmphasisCustomMark());
    }

    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        if (value->isValueList()) {
            CSSValueList* list = static_cast<CSSValueList*>(value);
            ASSERT(list->length() == 2);
            if (list->length() != 2)
                return;
            for (unsigned i = 0; i < 2; ++i) {
                CSSValue* item = list->itemWithoutBoundsCheck(i);
                if (!item->isPrimitiveValue())
                    continue;

                CSSPrimitiveValue* value = static_cast<CSSPrimitiveValue*>(item);
                if (value->getIdent() == CSSValueFilled || value->getIdent() == CSSValueOpen)
                    selector->style()->setTextEmphasisFill(*value);
                else
                    selector->style()->setTextEmphasisMark(*value);
            }
            selector->style()->setTextEmphasisCustomMark(nullAtom);
            return;
        }

        if (!value->isPrimitiveValue())
            return;
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);

        if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_STRING) {
            selector->style()->setTextEmphasisFill(TextEmphasisFillFilled);
            selector->style()->setTextEmphasisMark(TextEmphasisMarkCustom);
            selector->style()->setTextEmphasisCustomMark(primitiveValue->getStringValue());
            return;
        }

        selector->style()->setTextEmphasisCustomMark(nullAtom);

        if (primitiveValue->getIdent() == CSSValueFilled || primitiveValue->getIdent() == CSSValueOpen) {
            selector->style()->setTextEmphasisFill(*primitiveValue);
            selector->style()->setTextEmphasisMark(TextEmphasisMarkAuto);
        } else {
            selector->style()->setTextEmphasisFill(TextEmphasisFillFilled);
            selector->style()->setTextEmphasisMark(*primitiveValue);
        }
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

template <typename T,
          T (Animation::*getterFunction)() const,
          void (Animation::*setterFunction)(T),
          bool (Animation::*testFunction)() const,
          void (Animation::*clearFunction)(),
          T (*initialFunction)(),
          void (CSSStyleSelector::*mapFunction)(Animation*, CSSValue*),
          AnimationList* (RenderStyle::*animationGetterFunction)(),
          const AnimationList* (RenderStyle::*immutableAnimationGetterFunction)() const>
class ApplyPropertyAnimation {
public:
    static void setValue(Animation* animation, T value) { (animation->*setterFunction)(value); }
    static T value(const Animation* animation) { return (animation->*getterFunction)(); }
    static bool test(const Animation* animation) { return (animation->*testFunction)(); }
    static void clear(Animation* animation) { (animation->*clearFunction)(); }
    static T initial() { return (*initialFunction)(); }
    static void map(CSSStyleSelector* selector, Animation* animation, CSSValue* value) { (selector->*mapFunction)(animation, value); }
    static AnimationList* accessAnimations(RenderStyle* style) { return (style->*animationGetterFunction)(); }
    static const AnimationList* animations(RenderStyle* style) { return (style->*immutableAnimationGetterFunction)(); }

    static void applyInheritValue(CSSStyleSelector* selector)
    {
        AnimationList* list = accessAnimations(selector->style());
        const AnimationList* parentList = animations(selector->parentStyle());
        size_t i = 0, parentSize = parentList ? parentList->size() : 0;
        for ( ; i < parentSize && test(parentList->animation(i)); ++i) {
            if (list->size() <= i)
                list->append(Animation::create());
            setValue(list->animation(i), value(parentList->animation(i)));
        }

        /* Reset any remaining animations to not have the property set. */
        for ( ; i < list->size(); ++i)
            clear(list->animation(i));
    }

    static void applyInitialValue(CSSStyleSelector* selector)
    {
        AnimationList* list = accessAnimations(selector->style());
        if (list->isEmpty())
            list->append(Animation::create());
        setValue(list->animation(0), initial());
        for (size_t i = 1; i < list->size(); ++i)
            clear(list->animation(i));
    }

    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        AnimationList* list = accessAnimations(selector->style());
        size_t childIndex = 0;
        if (value->isValueList()) {
            /* Walk each value and put it into an animation, creating new animations as needed. */
            for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
                if (childIndex <= list->size())
                    list->append(Animation::create());
                map(selector, list->animation(childIndex), i.value());
                ++childIndex;
            }
        } else {
            if (list->isEmpty())
                list->append(Animation::create());
            map(selector, list->animation(childIndex), value);
            childIndex = 1;
        }
        for ( ; childIndex < list->size(); ++childIndex) {
            /* Reset all remaining animations to not have the property set. */
            clear(list->animation(childIndex));
        }
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

class ApplyPropertyOutlineStyle {
public:
    static void applyInheritValue(CSSStyleSelector* selector)
    {
        ApplyPropertyDefaultBase<OutlineIsAuto, &RenderStyle::outlineStyleIsAuto, OutlineIsAuto, &RenderStyle::setOutlineStyleIsAuto, OutlineIsAuto, &RenderStyle::initialOutlineStyleIsAuto>::applyInheritValue(selector);
        ApplyPropertyDefaultBase<EBorderStyle, &RenderStyle::outlineStyle, EBorderStyle, &RenderStyle::setOutlineStyle, EBorderStyle, &RenderStyle::initialBorderStyle>::applyInheritValue(selector);
    }

    static void applyInitialValue(CSSStyleSelector* selector)
    {
        ApplyPropertyDefaultBase<OutlineIsAuto, &RenderStyle::outlineStyleIsAuto, OutlineIsAuto, &RenderStyle::setOutlineStyleIsAuto, OutlineIsAuto, &RenderStyle::initialOutlineStyleIsAuto>::applyInitialValue(selector);
        ApplyPropertyDefaultBase<EBorderStyle, &RenderStyle::outlineStyle, EBorderStyle, &RenderStyle::setOutlineStyle, EBorderStyle, &RenderStyle::initialBorderStyle>::applyInitialValue(selector);
    }

    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        ApplyPropertyDefault<OutlineIsAuto, &RenderStyle::outlineStyleIsAuto, OutlineIsAuto, &RenderStyle::setOutlineStyleIsAuto, OutlineIsAuto, &RenderStyle::initialOutlineStyleIsAuto>::applyValue(selector, value);
        ApplyPropertyDefault<EBorderStyle, &RenderStyle::outlineStyle, EBorderStyle, &RenderStyle::setOutlineStyle, EBorderStyle, &RenderStyle::initialBorderStyle>::applyValue(selector, value);
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

class ApplyPropertyResize {
public:
    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);

        EResize r = RESIZE_NONE;
        switch (primitiveValue->getIdent()) {
        case 0:
            return;
        case CSSValueAuto:
            if (Settings* settings = selector->document()->settings())
                r = settings->textAreasAreResizable() ? RESIZE_BOTH : RESIZE_NONE;
            break;
        default:
            r = *primitiveValue;
        }
        selector->style()->setResize(r);
    }

    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<EResize, &RenderStyle::resize, EResize, &RenderStyle::setResize, EResize, &RenderStyle::initialResize>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

class ApplyPropertyVerticalAlign {
public:
    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);

        if (primitiveValue->getIdent())
            return selector->style()->setVerticalAlign(*primitiveValue);

        Length length;
        if (primitiveValue->isLength())
            length = primitiveValue->computeLength<Length>(selector->style(), selector->rootElementStyle(), selector->style()->effectiveZoom());
        else if (primitiveValue->isPercentage())
            length = Length(primitiveValue->getDoubleValue(), Percent);

        selector->style()->setVerticalAlignLength(length);
    }

    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyDefaultBase<EVerticalAlign, &RenderStyle::verticalAlign, EVerticalAlign, &RenderStyle::setVerticalAlign, EVerticalAlign, &RenderStyle::initialVerticalAlign>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

class ApplyPropertyAspectRatio {
public:
    static void applyInheritValue(CSSStyleSelector* selector)
    {
        if (!selector->parentStyle()->hasAspectRatio())
            return;
        selector->style()->setHasAspectRatio(true);
        selector->style()->setAspectRatioDenominator(selector->parentStyle()->aspectRatioDenominator());
        selector->style()->setAspectRatioNumerator(selector->parentStyle()->aspectRatioNumerator());
    }

    static void applyInitialValue(CSSStyleSelector* selector)
    {
        selector->style()->setHasAspectRatio(RenderStyle::initialHasAspectRatio());
        selector->style()->setAspectRatioDenominator(RenderStyle::initialAspectRatioDenominator());
        selector->style()->setAspectRatioNumerator(RenderStyle::initialAspectRatioNumerator());
    }

    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        if (!value->isAspectRatioValue()) {
            selector->style()->setHasAspectRatio(false);
            return;
        }
        CSSAspectRatioValue* aspectRatioValue = static_cast<CSSAspectRatioValue*>(value);
        selector->style()->setHasAspectRatio(true);
        selector->style()->setAspectRatioDenominator(aspectRatioValue->denominatorValue());
        selector->style()->setAspectRatioNumerator(aspectRatioValue->numeratorValue());
    }

    static PropertyHandler createHandler()
    {
        return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue);
    }
};

class ApplyPropertyZoom {
private:
    static void resetEffectiveZoom(CSSStyleSelector* selector)
    {
        // Reset the zoom in effect. This allows the setZoom method to accurately compute a new zoom in effect.
        selector->setEffectiveZoom(selector->parentStyle() ? selector->parentStyle()->effectiveZoom() : RenderStyle::initialZoom());
    }

public:
    static void applyInheritValue(CSSStyleSelector* selector)
    {
        resetEffectiveZoom(selector);
        selector->setZoom(selector->parentStyle()->zoom());
    }

    static void applyInitialValue(CSSStyleSelector* selector)
    {
        resetEffectiveZoom(selector);
        selector->setZoom(RenderStyle::initialZoom());
    }

    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        ASSERT(value->isPrimitiveValue());
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);

        if (primitiveValue->getIdent() == CSSValueNormal) {
            resetEffectiveZoom(selector);
            selector->setZoom(RenderStyle::initialZoom());
        } else if (primitiveValue->getIdent() == CSSValueReset) {
            selector->setEffectiveZoom(RenderStyle::initialZoom());
            selector->setZoom(RenderStyle::initialZoom());
        } else if (primitiveValue->getIdent() == CSSValueDocument) {
            float docZoom = selector->document()->renderer()->style()->zoom();
            selector->setEffectiveZoom(docZoom);
            selector->setZoom(docZoom);
        } else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE) {
            resetEffectiveZoom(selector);
            if (float percent = primitiveValue->getFloatValue())
                selector->setZoom(percent / 100.0f);
        } else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_NUMBER) {
            resetEffectiveZoom(selector);
            if (float number = primitiveValue->getFloatValue())
                selector->setZoom(number);
        }
    }

    static PropertyHandler createHandler()
    {
        return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue);
    }
};

class ApplyPropertyDisplay {
private:
    static inline bool isValidDisplayValue(CSSStyleSelector* selector, EDisplay displayPropertyValue)
    {
#if ENABLE(SVG)
        if (selector->element() && selector->element()->isSVGElement() && selector->style()->styleType() == NOPSEUDO)
            return (displayPropertyValue == INLINE || displayPropertyValue == BLOCK || displayPropertyValue == NONE);
#endif
        return true;
    }
public:
    static void applyInheritValue(CSSStyleSelector* selector)
    {
        EDisplay display = selector->parentStyle()->display();
        if (!isValidDisplayValue(selector, display))
            return;
        selector->style()->setDisplay(display);
    }

    static void applyInitialValue(CSSStyleSelector* selector)
    {
        selector->style()->setDisplay(RenderStyle::initialDisplay());
    }

    static void applyValue(CSSStyleSelector* selector, CSSValue* value)
    {
        if (!value->isPrimitiveValue())
            return;

        EDisplay display = *static_cast<CSSPrimitiveValue*>(value);

        if (!isValidDisplayValue(selector, display))
            return;

        selector->style()->setDisplay(display);
    }

    static PropertyHandler createHandler()
    {
        return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue);
    }
};

const CSSStyleApplyProperty& CSSStyleApplyProperty::sharedCSSStyleApplyProperty()
{
    DEFINE_STATIC_LOCAL(CSSStyleApplyProperty, cssStyleApplyPropertyInstance, ());
    return cssStyleApplyPropertyInstance;
}

CSSStyleApplyProperty::CSSStyleApplyProperty()
{
    for (int i = 0; i < numCSSProperties; ++i)
        m_propertyMap[i] = PropertyHandler();

    setPropertyHandler(CSSPropertyWebkitAspectRatio, ApplyPropertyAspectRatio::createHandler());

    setPropertyHandler(CSSPropertyColor, ApplyPropertyColor<InheritFromParent, &RenderStyle::color, &RenderStyle::setColor, &RenderStyle::setVisitedLinkColor, &RenderStyle::invalidColor, RenderStyle::initialColor>::createHandler());
    setPropertyHandler(CSSPropertyDirection, ApplyPropertyDirection<&RenderStyle::direction, &RenderStyle::setDirection, RenderStyle::initialDirection>::createHandler());

    setPropertyHandler(CSSPropertyBackgroundAttachment, ApplyPropertyFillLayer<EFillAttachment, CSSPropertyBackgroundAttachment, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
                       &FillLayer::isAttachmentSet, &FillLayer::attachment, &FillLayer::setAttachment, &FillLayer::clearAttachment, &FillLayer::initialFillAttachment, &CSSStyleSelector::mapFillAttachment>::createHandler());
    setPropertyHandler(CSSPropertyBackgroundClip, ApplyPropertyFillLayer<EFillBox, CSSPropertyBackgroundClip, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
                       &FillLayer::isClipSet, &FillLayer::clip, &FillLayer::setClip, &FillLayer::clearClip, &FillLayer::initialFillClip, &CSSStyleSelector::mapFillClip>::createHandler());
    setPropertyHandler(CSSPropertyWebkitBackgroundClip, CSSPropertyBackgroundClip);
    setPropertyHandler(CSSPropertyWebkitBackgroundComposite, ApplyPropertyFillLayer<CompositeOperator, CSSPropertyWebkitBackgroundComposite, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
                       &FillLayer::isCompositeSet, &FillLayer::composite, &FillLayer::setComposite, &FillLayer::clearComposite, &FillLayer::initialFillComposite, &CSSStyleSelector::mapFillComposite>::createHandler());

    setPropertyHandler(CSSPropertyDisplay, ApplyPropertyDisplay::createHandler());

    setPropertyHandler(CSSPropertyBackgroundImage, ApplyPropertyFillLayer<StyleImage*, CSSPropertyBackgroundImage, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
                       &FillLayer::isImageSet, &FillLayer::image, &FillLayer::setImage, &FillLayer::clearImage, &FillLayer::initialFillImage, &CSSStyleSelector::mapFillImage>::createHandler());

    setPropertyHandler(CSSPropertyBackgroundOrigin, ApplyPropertyFillLayer<EFillBox, CSSPropertyBackgroundOrigin, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
                       &FillLayer::isOriginSet, &FillLayer::origin, &FillLayer::setOrigin, &FillLayer::clearOrigin, &FillLayer::initialFillOrigin, &CSSStyleSelector::mapFillOrigin>::createHandler());
    setPropertyHandler(CSSPropertyWebkitBackgroundOrigin, CSSPropertyBackgroundOrigin);

    setPropertyHandler(CSSPropertyBackgroundPositionX, ApplyPropertyFillLayer<Length, CSSPropertyBackgroundPositionX, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
                       &FillLayer::isXPositionSet, &FillLayer::xPosition, &FillLayer::setXPosition, &FillLayer::clearXPosition, &FillLayer::initialFillXPosition, &CSSStyleSelector::mapFillXPosition>::createHandler());
    setPropertyHandler(CSSPropertyBackgroundPositionY, ApplyPropertyFillLayer<Length, CSSPropertyBackgroundPositionY, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
                       &FillLayer::isYPositionSet, &FillLayer::yPosition, &FillLayer::setYPosition, &FillLayer::clearYPosition, &FillLayer::initialFillYPosition, &CSSStyleSelector::mapFillYPosition>::createHandler());
    setPropertyHandler(CSSPropertyBackgroundPosition, ApplyPropertyExpanding<SuppressValue, CSSPropertyBackgroundPositionX, CSSPropertyBackgroundPositionY>::createHandler());

    setPropertyHandler(CSSPropertyBackgroundRepeatX, ApplyPropertyFillLayer<EFillRepeat, CSSPropertyBackgroundRepeatX, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
                       &FillLayer::isRepeatXSet, &FillLayer::repeatX, &FillLayer::setRepeatX, &FillLayer::clearRepeatX, &FillLayer::initialFillRepeatX, &CSSStyleSelector::mapFillRepeatX>::createHandler());
    setPropertyHandler(CSSPropertyBackgroundRepeatY, ApplyPropertyFillLayer<EFillRepeat, CSSPropertyBackgroundRepeatY, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
                       &FillLayer::isRepeatYSet, &FillLayer::repeatY, &FillLayer::setRepeatY, &FillLayer::clearRepeatY, &FillLayer::initialFillRepeatY, &CSSStyleSelector::mapFillRepeatY>::createHandler());
    setPropertyHandler(CSSPropertyBackgroundRepeat, ApplyPropertyExpanding<SuppressValue, CSSPropertyBackgroundRepeatX, CSSPropertyBackgroundRepeatY>::createHandler());

    setPropertyHandler(CSSPropertyBackgroundSize, ApplyPropertyFillLayer<FillSize, CSSPropertyBackgroundSize, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
                       &FillLayer::isSizeSet, &FillLayer::size, &FillLayer::setSize, &FillLayer::clearSize, &FillLayer::initialFillSize, &CSSStyleSelector::mapFillSize>::createHandler());
    setPropertyHandler(CSSPropertyWebkitBackgroundSize, CSSPropertyBackgroundSize);

    setPropertyHandler(CSSPropertyWebkitMaskAttachment, ApplyPropertyFillLayer<EFillAttachment, CSSPropertyWebkitMaskAttachment, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
                       &FillLayer::isAttachmentSet, &FillLayer::attachment, &FillLayer::setAttachment, &FillLayer::clearAttachment, &FillLayer::initialFillAttachment, &CSSStyleSelector::mapFillAttachment>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskClip, ApplyPropertyFillLayer<EFillBox, CSSPropertyWebkitMaskClip, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
                       &FillLayer::isClipSet, &FillLayer::clip, &FillLayer::setClip, &FillLayer::clearClip, &FillLayer::initialFillClip, &CSSStyleSelector::mapFillClip>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskComposite, ApplyPropertyFillLayer<CompositeOperator, CSSPropertyWebkitMaskComposite, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
                       &FillLayer::isCompositeSet, &FillLayer::composite, &FillLayer::setComposite, &FillLayer::clearComposite, &FillLayer::initialFillComposite, &CSSStyleSelector::mapFillComposite>::createHandler());

    setPropertyHandler(CSSPropertyWebkitMaskImage, ApplyPropertyFillLayer<StyleImage*, CSSPropertyWebkitMaskImage, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
                       &FillLayer::isImageSet, &FillLayer::image, &FillLayer::setImage, &FillLayer::clearImage, &FillLayer::initialFillImage, &CSSStyleSelector::mapFillImage>::createHandler());

    setPropertyHandler(CSSPropertyWebkitMaskOrigin, ApplyPropertyFillLayer<EFillBox, CSSPropertyWebkitMaskOrigin, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
                       &FillLayer::isOriginSet, &FillLayer::origin, &FillLayer::setOrigin, &FillLayer::clearOrigin, &FillLayer::initialFillOrigin, &CSSStyleSelector::mapFillOrigin>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskSize, ApplyPropertyFillLayer<FillSize, CSSPropertyWebkitMaskSize, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
                       &FillLayer::isSizeSet, &FillLayer::size, &FillLayer::setSize, &FillLayer::clearSize, &FillLayer::initialFillSize, &CSSStyleSelector::mapFillSize>::createHandler());

    setPropertyHandler(CSSPropertyWebkitMaskPositionX, ApplyPropertyFillLayer<Length, CSSPropertyWebkitMaskPositionX, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
                       &FillLayer::isXPositionSet, &FillLayer::xPosition, &FillLayer::setXPosition, &FillLayer::clearXPosition, &FillLayer::initialFillXPosition, &CSSStyleSelector::mapFillXPosition>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskPositionY, ApplyPropertyFillLayer<Length, CSSPropertyWebkitMaskPositionY, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
                       &FillLayer::isYPositionSet, &FillLayer::yPosition, &FillLayer::setYPosition, &FillLayer::clearYPosition, &FillLayer::initialFillYPosition, &CSSStyleSelector::mapFillYPosition>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskPosition, ApplyPropertyExpanding<SuppressValue, CSSPropertyWebkitMaskPositionX, CSSPropertyWebkitMaskPositionY>::createHandler());

    setPropertyHandler(CSSPropertyWebkitMaskRepeatX, ApplyPropertyFillLayer<EFillRepeat, CSSPropertyWebkitMaskRepeatX, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
                       &FillLayer::isRepeatXSet, &FillLayer::repeatX, &FillLayer::setRepeatX, &FillLayer::clearRepeatX, &FillLayer::initialFillRepeatX, &CSSStyleSelector::mapFillRepeatX>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskRepeatY, ApplyPropertyFillLayer<EFillRepeat, CSSPropertyWebkitMaskRepeatY, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
                       &FillLayer::isRepeatYSet, &FillLayer::repeatY, &FillLayer::setRepeatY, &FillLayer::clearRepeatY, &FillLayer::initialFillRepeatY, &CSSStyleSelector::mapFillRepeatY>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskRepeat, ApplyPropertyExpanding<SuppressValue, CSSPropertyBackgroundRepeatX, CSSPropertyBackgroundRepeatY>::createHandler());

    setPropertyHandler(CSSPropertyBackgroundColor, ApplyPropertyColor<NoInheritFromParent, &RenderStyle::backgroundColor, &RenderStyle::setBackgroundColor, &RenderStyle::setVisitedLinkBackgroundColor, &RenderStyle::invalidColor>::createHandler());
    setPropertyHandler(CSSPropertyBorderBottomColor, ApplyPropertyColor<NoInheritFromParent, &RenderStyle::borderBottomColor, &RenderStyle::setBorderBottomColor, &RenderStyle::setVisitedLinkBorderBottomColor, &RenderStyle::color>::createHandler());
    setPropertyHandler(CSSPropertyBorderLeftColor, ApplyPropertyColor<NoInheritFromParent, &RenderStyle::borderLeftColor, &RenderStyle::setBorderLeftColor, &RenderStyle::setVisitedLinkBorderLeftColor, &RenderStyle::color>::createHandler());
    setPropertyHandler(CSSPropertyBorderRightColor, ApplyPropertyColor<NoInheritFromParent, &RenderStyle::borderRightColor, &RenderStyle::setBorderRightColor, &RenderStyle::setVisitedLinkBorderRightColor, &RenderStyle::color>::createHandler());
    setPropertyHandler(CSSPropertyBorderTopColor, ApplyPropertyColor<NoInheritFromParent, &RenderStyle::borderTopColor, &RenderStyle::setBorderTopColor, &RenderStyle::setVisitedLinkBorderTopColor, &RenderStyle::color>::createHandler());

    setPropertyHandler(CSSPropertyBorderTopStyle, ApplyPropertyDefault<EBorderStyle, &RenderStyle::borderTopStyle, EBorderStyle, &RenderStyle::setBorderTopStyle, EBorderStyle, &RenderStyle::initialBorderStyle>::createHandler());
    setPropertyHandler(CSSPropertyBorderRightStyle, ApplyPropertyDefault<EBorderStyle, &RenderStyle::borderRightStyle, EBorderStyle, &RenderStyle::setBorderRightStyle, EBorderStyle, &RenderStyle::initialBorderStyle>::createHandler());
    setPropertyHandler(CSSPropertyBorderBottomStyle, ApplyPropertyDefault<EBorderStyle, &RenderStyle::borderBottomStyle, EBorderStyle, &RenderStyle::setBorderBottomStyle, EBorderStyle, &RenderStyle::initialBorderStyle>::createHandler());
    setPropertyHandler(CSSPropertyBorderLeftStyle, ApplyPropertyDefault<EBorderStyle, &RenderStyle::borderLeftStyle, EBorderStyle, &RenderStyle::setBorderLeftStyle, EBorderStyle, &RenderStyle::initialBorderStyle>::createHandler());

    setPropertyHandler(CSSPropertyBorderTopWidth, ApplyPropertyComputeLength<unsigned short, &RenderStyle::borderTopWidth, &RenderStyle::setBorderTopWidth, &RenderStyle::initialBorderWidth, NormalDisabled, ThicknessEnabled>::createHandler());
    setPropertyHandler(CSSPropertyBorderRightWidth, ApplyPropertyComputeLength<unsigned short, &RenderStyle::borderRightWidth, &RenderStyle::setBorderRightWidth, &RenderStyle::initialBorderWidth, NormalDisabled, ThicknessEnabled>::createHandler());
    setPropertyHandler(CSSPropertyBorderBottomWidth, ApplyPropertyComputeLength<unsigned short, &RenderStyle::borderBottomWidth, &RenderStyle::setBorderBottomWidth, &RenderStyle::initialBorderWidth, NormalDisabled, ThicknessEnabled>::createHandler());
    setPropertyHandler(CSSPropertyBorderLeftWidth, ApplyPropertyComputeLength<unsigned short, &RenderStyle::borderLeftWidth, &RenderStyle::setBorderLeftWidth, &RenderStyle::initialBorderWidth, NormalDisabled, ThicknessEnabled>::createHandler());
    setPropertyHandler(CSSPropertyOutlineWidth, ApplyPropertyComputeLength<unsigned short, &RenderStyle::outlineWidth, &RenderStyle::setOutlineWidth, &RenderStyle::initialBorderWidth, NormalDisabled, ThicknessEnabled>::createHandler());
    setPropertyHandler(CSSPropertyWebkitColumnRuleWidth, ApplyPropertyComputeLength<unsigned short, &RenderStyle::columnRuleWidth, &RenderStyle::setColumnRuleWidth, &RenderStyle::initialBorderWidth, NormalDisabled, ThicknessEnabled>::createHandler());

    setPropertyHandler(CSSPropertyBorderTop, ApplyPropertyExpanding<SuppressValue, CSSPropertyBorderTopColor, CSSPropertyBorderTopStyle, CSSPropertyBorderTopWidth>::createHandler());
    setPropertyHandler(CSSPropertyBorderRight, ApplyPropertyExpanding<SuppressValue, CSSPropertyBorderRightColor, CSSPropertyBorderRightStyle, CSSPropertyBorderRightWidth>::createHandler());
    setPropertyHandler(CSSPropertyBorderBottom, ApplyPropertyExpanding<SuppressValue, CSSPropertyBorderBottomColor, CSSPropertyBorderBottomStyle, CSSPropertyBorderBottomWidth>::createHandler());
    setPropertyHandler(CSSPropertyBorderLeft, ApplyPropertyExpanding<SuppressValue, CSSPropertyBorderLeftColor, CSSPropertyBorderLeftStyle, CSSPropertyBorderLeftWidth>::createHandler());

    setPropertyHandler(CSSPropertyBorderStyle, ApplyPropertyExpanding<SuppressValue, CSSPropertyBorderTopStyle, CSSPropertyBorderRightStyle, CSSPropertyBorderBottomStyle, CSSPropertyBorderLeftStyle>::createHandler());
    setPropertyHandler(CSSPropertyBorderWidth, ApplyPropertyExpanding<SuppressValue, CSSPropertyBorderTopWidth, CSSPropertyBorderRightWidth, CSSPropertyBorderBottomWidth, CSSPropertyBorderLeftWidth>::createHandler());
    setPropertyHandler(CSSPropertyBorderColor, ApplyPropertyExpanding<SuppressValue, CSSPropertyBorderTopColor, CSSPropertyBorderRightColor, CSSPropertyBorderBottomColor, CSSPropertyBorderLeftColor>::createHandler());
    setPropertyHandler(CSSPropertyBorder, ApplyPropertyExpanding<SuppressValue, CSSPropertyBorderStyle, CSSPropertyBorderWidth, CSSPropertyBorderColor>::createHandler());

    setPropertyHandler(CSSPropertyBorderImage, ApplyPropertyBorderImage<Image, CSSPropertyBorderImage, &RenderStyle::borderImage, &RenderStyle::setBorderImage>::createHandler());
    setPropertyHandler(CSSPropertyWebkitBorderImage, ApplyPropertyBorderImage<Image, CSSPropertyWebkitBorderImage, &RenderStyle::borderImage, &RenderStyle::setBorderImage>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskBoxImage, ApplyPropertyBorderImage<Mask, CSSPropertyWebkitMaskBoxImage, &RenderStyle::maskBoxImage, &RenderStyle::setMaskBoxImage>::createHandler());

    setPropertyHandler(CSSPropertyBorderImageOutset, ApplyPropertyBorderImageModifier<Image, Outset>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskBoxImageOutset, ApplyPropertyBorderImageModifier<Mask, Outset>::createHandler());
    setPropertyHandler(CSSPropertyBorderImageRepeat, ApplyPropertyBorderImageModifier<Image, Repeat>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskBoxImageRepeat, ApplyPropertyBorderImageModifier<Mask, Repeat>::createHandler());
    setPropertyHandler(CSSPropertyBorderImageSlice, ApplyPropertyBorderImageModifier<Image, Slice>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskBoxImageSlice, ApplyPropertyBorderImageModifier<Mask, Slice>::createHandler());
    setPropertyHandler(CSSPropertyBorderImageWidth, ApplyPropertyBorderImageModifier<Image, Width>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskBoxImageWidth, ApplyPropertyBorderImageModifier<Mask, Width>::createHandler());

    setPropertyHandler(CSSPropertyBorderImageSource, ApplyPropertyBorderImageSource<CSSPropertyBorderImageSource, &RenderStyle::borderImageSource, &RenderStyle::setBorderImageSource, &RenderStyle::initialBorderImageSource>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskBoxImageSource, ApplyPropertyBorderImageSource<CSSPropertyWebkitMaskBoxImageSource, &RenderStyle::maskBoxImageSource, &RenderStyle::setMaskBoxImageSource, &RenderStyle::initialMaskBoxImageSource>::createHandler());

    setPropertyHandler(CSSPropertyBorderTopLeftRadius, ApplyPropertyBorderRadius<&RenderStyle::borderTopLeftRadius, &RenderStyle::setBorderTopLeftRadius, &RenderStyle::initialBorderRadius>::createHandler());
    setPropertyHandler(CSSPropertyBorderTopRightRadius, ApplyPropertyBorderRadius<&RenderStyle::borderTopRightRadius, &RenderStyle::setBorderTopRightRadius, &RenderStyle::initialBorderRadius>::createHandler());
    setPropertyHandler(CSSPropertyBorderBottomLeftRadius, ApplyPropertyBorderRadius<&RenderStyle::borderBottomLeftRadius, &RenderStyle::setBorderBottomLeftRadius, &RenderStyle::initialBorderRadius>::createHandler());
    setPropertyHandler(CSSPropertyBorderBottomRightRadius, ApplyPropertyBorderRadius<&RenderStyle::borderBottomRightRadius, &RenderStyle::setBorderBottomRightRadius, &RenderStyle::initialBorderRadius>::createHandler());
    setPropertyHandler(CSSPropertyBorderRadius, ApplyPropertyExpanding<ExpandValue, CSSPropertyBorderTopLeftRadius, CSSPropertyBorderTopRightRadius, CSSPropertyBorderBottomLeftRadius, CSSPropertyBorderBottomRightRadius>::createHandler());
    setPropertyHandler(CSSPropertyWebkitBorderRadius, CSSPropertyBorderRadius);

    setPropertyHandler(CSSPropertyWebkitBorderHorizontalSpacing, ApplyPropertyComputeLength<short, &RenderStyle::horizontalBorderSpacing, &RenderStyle::setHorizontalBorderSpacing, &RenderStyle::initialHorizontalBorderSpacing>::createHandler());
    setPropertyHandler(CSSPropertyWebkitBorderVerticalSpacing, ApplyPropertyComputeLength<short, &RenderStyle::verticalBorderSpacing, &RenderStyle::setVerticalBorderSpacing, &RenderStyle::initialVerticalBorderSpacing>::createHandler());
    setPropertyHandler(CSSPropertyBorderSpacing, ApplyPropertyExpanding<SuppressValue, CSSPropertyWebkitBorderHorizontalSpacing, CSSPropertyWebkitBorderVerticalSpacing>::createHandler());

    setPropertyHandler(CSSPropertyLetterSpacing, ApplyPropertyComputeLength<int, &RenderStyle::letterSpacing, &RenderStyle::setLetterSpacing, &RenderStyle::initialLetterWordSpacing, NormalEnabled, ThicknessDisabled, SVGZoomEnabled>::createHandler());
    setPropertyHandler(CSSPropertyWordSpacing, ApplyPropertyComputeLength<int, &RenderStyle::wordSpacing, &RenderStyle::setWordSpacing, &RenderStyle::initialLetterWordSpacing, NormalEnabled, ThicknessDisabled, SVGZoomEnabled>::createHandler());

    setPropertyHandler(CSSPropertyCursor, ApplyPropertyCursor::createHandler());

    setPropertyHandler(CSSPropertyCounterIncrement, ApplyPropertyCounter<Increment>::createHandler());
    setPropertyHandler(CSSPropertyCounterReset, ApplyPropertyCounter<Reset>::createHandler());

    setPropertyHandler(CSSPropertyWebkitFlexOrder, ApplyPropertyDefault<int, &RenderStyle::flexOrder, int, &RenderStyle::setFlexOrder, int, &RenderStyle::initialFlexOrder>::createHandler());
    setPropertyHandler(CSSPropertyWebkitFlexPack, ApplyPropertyDefault<EFlexPack, &RenderStyle::flexPack, EFlexPack, &RenderStyle::setFlexPack, EFlexPack, &RenderStyle::initialFlexPack>::createHandler());
    setPropertyHandler(CSSPropertyWebkitFlexItemAlign, ApplyPropertyDefault<EFlexAlign, &RenderStyle::flexItemAlign, EFlexAlign, &RenderStyle::setFlexItemAlign, EFlexAlign, &RenderStyle::initialFlexItemAlign>::createHandler());
    setPropertyHandler(CSSPropertyWebkitFlexDirection, ApplyPropertyDefault<EFlexDirection, &RenderStyle::flexDirection, EFlexDirection, &RenderStyle::setFlexDirection, EFlexDirection, &RenderStyle::initialFlexDirection>::createHandler());
    setPropertyHandler(CSSPropertyWebkitFlexWrap, ApplyPropertyDefault<EFlexWrap, &RenderStyle::flexWrap, EFlexWrap, &RenderStyle::setFlexWrap, EFlexWrap, &RenderStyle::initialFlexWrap>::createHandler());
    setPropertyHandler(CSSPropertyWebkitFlexFlow, ApplyPropertyExpanding<SuppressValue, CSSPropertyWebkitFlexDirection, CSSPropertyWebkitFlexWrap>::createHandler());

    setPropertyHandler(CSSPropertyFontSize, ApplyPropertyFontSize::createHandler());
    setPropertyHandler(CSSPropertyFontStyle, ApplyPropertyFont<FontItalic, &FontDescription::italic, &FontDescription::setItalic, FontItalicOff>::createHandler());
    setPropertyHandler(CSSPropertyFontVariant, ApplyPropertyFont<FontSmallCaps, &FontDescription::smallCaps, &FontDescription::setSmallCaps, FontSmallCapsOff>::createHandler());
    setPropertyHandler(CSSPropertyTextRendering, ApplyPropertyFont<TextRenderingMode, &FontDescription::textRenderingMode, &FontDescription::setTextRenderingMode, AutoTextRendering>::createHandler());
    setPropertyHandler(CSSPropertyWebkitFontSmoothing, ApplyPropertyFont<FontSmoothingMode, &FontDescription::fontSmoothing, &FontDescription::setFontSmoothing, AutoSmoothing>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTextOrientation, ApplyPropertyFont<TextOrientation, &FontDescription::textOrientation, &FontDescription::setTextOrientation, TextOrientationVerticalRight>::createHandler());
    setPropertyHandler(CSSPropertyFontWeight, ApplyPropertyFontWeight::createHandler());

    setPropertyHandler(CSSPropertyTextAlign, ApplyPropertyTextAlign::createHandler());
    setPropertyHandler(CSSPropertyTextDecoration, ApplyPropertyTextDecoration::createHandler());

    setPropertyHandler(CSSPropertyOutlineStyle, ApplyPropertyOutlineStyle::createHandler());
    setPropertyHandler(CSSPropertyOutlineColor, ApplyPropertyColor<InheritFromParent, &RenderStyle::outlineColor, &RenderStyle::setOutlineColor, &RenderStyle::setVisitedLinkOutlineColor, &RenderStyle::color>::createHandler());
    setPropertyHandler(CSSPropertyOutlineOffset, ApplyPropertyComputeLength<int, &RenderStyle::outlineOffset, &RenderStyle::setOutlineOffset, &RenderStyle::initialOutlineOffset>::createHandler());

    setPropertyHandler(CSSPropertyOutline, ApplyPropertyExpanding<SuppressValue, CSSPropertyOutlineWidth, CSSPropertyOutlineColor, CSSPropertyOutlineStyle>::createHandler());

    setPropertyHandler(CSSPropertyOverflowX, ApplyPropertyDefault<EOverflow, &RenderStyle::overflowX, EOverflow, &RenderStyle::setOverflowX, EOverflow, &RenderStyle::initialOverflowX>::createHandler());
    setPropertyHandler(CSSPropertyOverflowY, ApplyPropertyDefault<EOverflow, &RenderStyle::overflowY, EOverflow, &RenderStyle::setOverflowY, EOverflow, &RenderStyle::initialOverflowY>::createHandler());
    setPropertyHandler(CSSPropertyOverflow, ApplyPropertyExpanding<ExpandValue, CSSPropertyOverflowX, CSSPropertyOverflowY>::createHandler());

    setPropertyHandler(CSSPropertyWebkitColumnRuleColor, ApplyPropertyColor<NoInheritFromParent, &RenderStyle::columnRuleColor, &RenderStyle::setColumnRuleColor, &RenderStyle::setVisitedLinkColumnRuleColor, &RenderStyle::color>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTextEmphasisColor, ApplyPropertyColor<NoInheritFromParent, &RenderStyle::textEmphasisColor, &RenderStyle::setTextEmphasisColor, &RenderStyle::setVisitedLinkTextEmphasisColor, &RenderStyle::color>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTextFillColor, ApplyPropertyColor<NoInheritFromParent, &RenderStyle::textFillColor, &RenderStyle::setTextFillColor, &RenderStyle::setVisitedLinkTextFillColor, &RenderStyle::color>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTextStrokeColor, ApplyPropertyColor<NoInheritFromParent, &RenderStyle::textStrokeColor, &RenderStyle::setTextStrokeColor, &RenderStyle::setVisitedLinkTextStrokeColor, &RenderStyle::color>::createHandler());

    setPropertyHandler(CSSPropertyTop, ApplyPropertyLength<&RenderStyle::top, &RenderStyle::setTop, &RenderStyle::initialOffset, AutoEnabled>::createHandler());
    setPropertyHandler(CSSPropertyRight, ApplyPropertyLength<&RenderStyle::right, &RenderStyle::setRight, &RenderStyle::initialOffset, AutoEnabled>::createHandler());
    setPropertyHandler(CSSPropertyBottom, ApplyPropertyLength<&RenderStyle::bottom, &RenderStyle::setBottom, &RenderStyle::initialOffset, AutoEnabled>::createHandler());
    setPropertyHandler(CSSPropertyLeft, ApplyPropertyLength<&RenderStyle::left, &RenderStyle::setLeft, &RenderStyle::initialOffset, AutoEnabled>::createHandler());

    setPropertyHandler(CSSPropertyWidth, ApplyPropertyLength<&RenderStyle::width, &RenderStyle::setWidth, &RenderStyle::initialSize, AutoEnabled, IntrinsicEnabled, MinIntrinsicEnabled, NoneDisabled, UndefinedDisabled, FlexWidth>::createHandler());
    setPropertyHandler(CSSPropertyHeight, ApplyPropertyLength<&RenderStyle::height, &RenderStyle::setHeight, &RenderStyle::initialSize, AutoEnabled, IntrinsicEnabled, MinIntrinsicEnabled, NoneDisabled, UndefinedDisabled, FlexHeight>::createHandler());

    setPropertyHandler(CSSPropertyTextIndent, ApplyPropertyLength<&RenderStyle::textIndent, &RenderStyle::setTextIndent, &RenderStyle::initialTextIndent>::createHandler());

    setPropertyHandler(CSSPropertyLineHeight, ApplyPropertyLineHeight::createHandler());

    setPropertyHandler(CSSPropertyListStyleImage, ApplyPropertyStyleImage<&RenderStyle::listStyleImage, &RenderStyle::setListStyleImage, &RenderStyle::initialListStyleImage, CSSPropertyListStyleImage>::createHandler());
    setPropertyHandler(CSSPropertyListStylePosition, ApplyPropertyDefault<EListStylePosition, &RenderStyle::listStylePosition, EListStylePosition, &RenderStyle::setListStylePosition, EListStylePosition, &RenderStyle::initialListStylePosition>::createHandler());
    setPropertyHandler(CSSPropertyListStyleType, ApplyPropertyDefault<EListStyleType, &RenderStyle::listStyleType, EListStyleType, &RenderStyle::setListStyleType, EListStyleType, &RenderStyle::initialListStyleType>::createHandler());
    setPropertyHandler(CSSPropertyListStyle, ApplyPropertyExpanding<SuppressValue, CSSPropertyListStyleType, CSSPropertyListStyleImage, CSSPropertyListStylePosition>::createHandler());

    setPropertyHandler(CSSPropertyMaxHeight, ApplyPropertyLength<&RenderStyle::maxHeight, &RenderStyle::setMaxHeight, &RenderStyle::initialMaxSize, AutoEnabled, IntrinsicEnabled, MinIntrinsicEnabled, NoneEnabled, UndefinedEnabled>::createHandler());
    setPropertyHandler(CSSPropertyMaxWidth, ApplyPropertyLength<&RenderStyle::maxWidth, &RenderStyle::setMaxWidth, &RenderStyle::initialMaxSize, AutoEnabled, IntrinsicEnabled, MinIntrinsicEnabled, NoneEnabled, UndefinedEnabled>::createHandler());
    setPropertyHandler(CSSPropertyMinHeight, ApplyPropertyLength<&RenderStyle::minHeight, &RenderStyle::setMinHeight, &RenderStyle::initialMinSize, AutoEnabled, IntrinsicEnabled, MinIntrinsicEnabled>::createHandler());
    setPropertyHandler(CSSPropertyMinWidth, ApplyPropertyLength<&RenderStyle::minWidth, &RenderStyle::setMinWidth, &RenderStyle::initialMinSize, AutoEnabled, IntrinsicEnabled, MinIntrinsicEnabled>::createHandler());

    setPropertyHandler(CSSPropertyMarginTop, ApplyPropertyLength<&RenderStyle::marginTop, &RenderStyle::setMarginTop, &RenderStyle::initialMargin, AutoEnabled>::createHandler());
    setPropertyHandler(CSSPropertyMarginRight, ApplyPropertyLength<&RenderStyle::marginRight, &RenderStyle::setMarginRight, &RenderStyle::initialMargin, AutoEnabled>::createHandler());
    setPropertyHandler(CSSPropertyMarginBottom, ApplyPropertyLength<&RenderStyle::marginBottom, &RenderStyle::setMarginBottom, &RenderStyle::initialMargin, AutoEnabled>::createHandler());
    setPropertyHandler(CSSPropertyMarginLeft, ApplyPropertyLength<&RenderStyle::marginLeft, &RenderStyle::setMarginLeft, &RenderStyle::initialMargin, AutoEnabled>::createHandler());
    setPropertyHandler(CSSPropertyMargin, ApplyPropertyExpanding<SuppressValue, CSSPropertyMarginTop, CSSPropertyMarginRight, CSSPropertyMarginBottom, CSSPropertyMarginLeft>::createHandler());

    setPropertyHandler(CSSPropertyWebkitMarginBeforeCollapse, ApplyPropertyDefault<EMarginCollapse, &RenderStyle::marginBeforeCollapse, EMarginCollapse, &RenderStyle::setMarginBeforeCollapse, EMarginCollapse, &RenderStyle::initialMarginBeforeCollapse>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMarginAfterCollapse, ApplyPropertyDefault<EMarginCollapse, &RenderStyle::marginAfterCollapse, EMarginCollapse, &RenderStyle::setMarginAfterCollapse, EMarginCollapse, &RenderStyle::initialMarginAfterCollapse>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMarginTopCollapse, CSSPropertyWebkitMarginBeforeCollapse);
    setPropertyHandler(CSSPropertyWebkitMarginBottomCollapse, CSSPropertyWebkitMarginAfterCollapse);
    setPropertyHandler(CSSPropertyWebkitMarginCollapse, ApplyPropertyExpanding<SuppressValue, CSSPropertyWebkitMarginBeforeCollapse, CSSPropertyWebkitMarginAfterCollapse>::createHandler());

    setPropertyHandler(CSSPropertyPaddingTop, ApplyPropertyLength<&RenderStyle::paddingTop, &RenderStyle::setPaddingTop, &RenderStyle::initialPadding>::createHandler());
    setPropertyHandler(CSSPropertyPaddingRight, ApplyPropertyLength<&RenderStyle::paddingRight, &RenderStyle::setPaddingRight, &RenderStyle::initialPadding>::createHandler());
    setPropertyHandler(CSSPropertyPaddingBottom, ApplyPropertyLength<&RenderStyle::paddingBottom, &RenderStyle::setPaddingBottom, &RenderStyle::initialPadding>::createHandler());
    setPropertyHandler(CSSPropertyPaddingLeft, ApplyPropertyLength<&RenderStyle::paddingLeft, &RenderStyle::setPaddingLeft, &RenderStyle::initialPadding>::createHandler());
    setPropertyHandler(CSSPropertyPadding, ApplyPropertyExpanding<SuppressValue, CSSPropertyPaddingTop, CSSPropertyPaddingRight, CSSPropertyPaddingBottom, CSSPropertyPaddingLeft>::createHandler());

    setPropertyHandler(CSSPropertyResize, ApplyPropertyResize::createHandler());

    setPropertyHandler(CSSPropertyVerticalAlign, ApplyPropertyVerticalAlign::createHandler());

    setPropertyHandler(CSSPropertySize, ApplyPropertyPageSize::createHandler());

    setPropertyHandler(CSSPropertyWebkitPerspectiveOriginX, ApplyPropertyLength<&RenderStyle::perspectiveOriginX, &RenderStyle::setPerspectiveOriginX, &RenderStyle::initialPerspectiveOriginX>::createHandler());
    setPropertyHandler(CSSPropertyWebkitPerspectiveOriginY, ApplyPropertyLength<&RenderStyle::perspectiveOriginY, &RenderStyle::setPerspectiveOriginY, &RenderStyle::initialPerspectiveOriginY>::createHandler());
    setPropertyHandler(CSSPropertyWebkitPerspectiveOrigin, ApplyPropertyExpanding<SuppressValue, CSSPropertyWebkitPerspectiveOriginX, CSSPropertyWebkitPerspectiveOriginY>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTransformOriginX, ApplyPropertyLength<&RenderStyle::transformOriginX, &RenderStyle::setTransformOriginX, &RenderStyle::initialTransformOriginX>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTransformOriginY, ApplyPropertyLength<&RenderStyle::transformOriginY, &RenderStyle::setTransformOriginY, &RenderStyle::initialTransformOriginY>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTransformOriginZ, ApplyPropertyComputeLength<float, &RenderStyle::transformOriginZ, &RenderStyle::setTransformOriginZ, &RenderStyle::initialTransformOriginZ>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTransformOrigin, ApplyPropertyExpanding<SuppressValue, CSSPropertyWebkitTransformOriginX, CSSPropertyWebkitTransformOriginY, CSSPropertyWebkitTransformOriginZ>::createHandler());

    setPropertyHandler(CSSPropertyWebkitAnimationDelay, ApplyPropertyAnimation<double, &Animation::delay, &Animation::setDelay, &Animation::isDelaySet, &Animation::clearDelay, &Animation::initialAnimationDelay, &CSSStyleSelector::mapAnimationDelay, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyWebkitAnimationDirection, ApplyPropertyAnimation<Animation::AnimationDirection, &Animation::direction, &Animation::setDirection, &Animation::isDirectionSet, &Animation::clearDirection, &Animation::initialAnimationDirection, &CSSStyleSelector::mapAnimationDirection, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyWebkitAnimationDuration, ApplyPropertyAnimation<double, &Animation::duration, &Animation::setDuration, &Animation::isDurationSet, &Animation::clearDuration, &Animation::initialAnimationDuration, &CSSStyleSelector::mapAnimationDuration, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyWebkitAnimationFillMode, ApplyPropertyAnimation<unsigned, &Animation::fillMode, &Animation::setFillMode, &Animation::isFillModeSet, &Animation::clearFillMode, &Animation::initialAnimationFillMode, &CSSStyleSelector::mapAnimationFillMode, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyWebkitAnimationIterationCount, ApplyPropertyAnimation<int, &Animation::iterationCount, &Animation::setIterationCount, &Animation::isIterationCountSet, &Animation::clearIterationCount, &Animation::initialAnimationIterationCount, &CSSStyleSelector::mapAnimationIterationCount, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyWebkitAnimationName, ApplyPropertyAnimation<const String&, &Animation::name, &Animation::setName, &Animation::isNameSet, &Animation::clearName, &Animation::initialAnimationName, &CSSStyleSelector::mapAnimationName, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyWebkitAnimationPlayState, ApplyPropertyAnimation<EAnimPlayState, &Animation::playState, &Animation::setPlayState, &Animation::isPlayStateSet, &Animation::clearPlayState, &Animation::initialAnimationPlayState, &CSSStyleSelector::mapAnimationPlayState, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyWebkitAnimationTimingFunction, ApplyPropertyAnimation<const PassRefPtr<TimingFunction>, &Animation::timingFunction, &Animation::setTimingFunction, &Animation::isTimingFunctionSet, &Animation::clearTimingFunction, &Animation::initialAnimationTimingFunction, &CSSStyleSelector::mapAnimationTimingFunction, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());

    setPropertyHandler(CSSPropertyWebkitTransitionDelay, ApplyPropertyAnimation<double, &Animation::delay, &Animation::setDelay, &Animation::isDelaySet, &Animation::clearDelay, &Animation::initialAnimationDelay, &CSSStyleSelector::mapAnimationDelay, &RenderStyle::accessTransitions, &RenderStyle::transitions>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTransitionDuration, ApplyPropertyAnimation<double, &Animation::duration, &Animation::setDuration, &Animation::isDurationSet, &Animation::clearDuration, &Animation::initialAnimationDuration, &CSSStyleSelector::mapAnimationDuration, &RenderStyle::accessTransitions, &RenderStyle::transitions>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTransitionProperty, ApplyPropertyAnimation<int, &Animation::property, &Animation::setProperty, &Animation::isPropertySet, &Animation::clearProperty, &Animation::initialAnimationProperty, &CSSStyleSelector::mapAnimationProperty, &RenderStyle::accessTransitions, &RenderStyle::transitions>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTransitionTimingFunction, ApplyPropertyAnimation<const PassRefPtr<TimingFunction>, &Animation::timingFunction, &Animation::setTimingFunction, &Animation::isTimingFunctionSet, &Animation::clearTimingFunction, &Animation::initialAnimationTimingFunction, &CSSStyleSelector::mapAnimationTimingFunction, &RenderStyle::accessTransitions, &RenderStyle::transitions>::createHandler());

    setPropertyHandler(CSSPropertyWebkitColumnCount, ApplyPropertyAuto<unsigned short, &RenderStyle::columnCount, &RenderStyle::setColumnCount, &RenderStyle::hasAutoColumnCount, &RenderStyle::setHasAutoColumnCount>::createHandler());
    setPropertyHandler(CSSPropertyWebkitColumnGap, ApplyPropertyAuto<float, &RenderStyle::columnGap, &RenderStyle::setColumnGap, &RenderStyle::hasNormalColumnGap, &RenderStyle::setHasNormalColumnGap, ComputeLength, CSSValueNormal>::createHandler());
    setPropertyHandler(CSSPropertyWebkitColumnWidth, ApplyPropertyAuto<float, &RenderStyle::columnWidth, &RenderStyle::setColumnWidth, &RenderStyle::hasAutoColumnWidth, &RenderStyle::setHasAutoColumnWidth, ComputeLength>::createHandler());
    setPropertyHandler(CSSPropertyWebkitColumns, ApplyPropertyExpanding<SuppressValue, CSSPropertyWebkitColumnWidth, CSSPropertyWebkitColumnCount>::createHandler());

    setPropertyHandler(CSSPropertyWebkitFlowInto, ApplyPropertyString<MapAutoToNull, &RenderStyle::flowThread, &RenderStyle::setFlowThread, &RenderStyle::initialFlowThread>::createHandler());
    setPropertyHandler(CSSPropertyWebkitFlowFrom, ApplyPropertyString<MapNoneToNull, &RenderStyle::regionThread, &RenderStyle::setRegionThread, &RenderStyle::initialRegionThread>::createHandler());

    setPropertyHandler(CSSPropertyWebkitHighlight, ApplyPropertyString<MapNoneToNull, &RenderStyle::highlight, &RenderStyle::setHighlight, &RenderStyle::initialHighlight>::createHandler());
    setPropertyHandler(CSSPropertyWebkitHyphenateCharacter, ApplyPropertyString<MapAutoToNull, &RenderStyle::hyphenationString, &RenderStyle::setHyphenationString, &RenderStyle::initialHyphenationString>::createHandler());

    setPropertyHandler(CSSPropertyWebkitHyphenateLimitAfter, ApplyPropertyNumber<short, &RenderStyle::hyphenationLimitAfter, &RenderStyle::setHyphenationLimitAfter, &RenderStyle::initialHyphenationLimitAfter>::createHandler());
    setPropertyHandler(CSSPropertyWebkitHyphenateLimitBefore, ApplyPropertyNumber<short, &RenderStyle::hyphenationLimitBefore, &RenderStyle::setHyphenationLimitBefore, &RenderStyle::initialHyphenationLimitBefore>::createHandler());
    setPropertyHandler(CSSPropertyWebkitHyphenateLimitLines, ApplyPropertyNumber<short, &RenderStyle::hyphenationLimitLines, &RenderStyle::setHyphenationLimitLines, &RenderStyle::initialHyphenationLimitLines, CSSValueNoLimit>::createHandler());

    setPropertyHandler(CSSPropertyWebkitLineGrid, ApplyPropertyString<MapNoneToNull, &RenderStyle::lineGrid, &RenderStyle::setLineGrid, &RenderStyle::initialLineGrid>::createHandler());
    setPropertyHandler(CSSPropertyWebkitLineGridSnap, ApplyPropertyDefault<LineGridSnap, &RenderStyle::lineGridSnap, LineGridSnap, &RenderStyle::setLineGridSnap, LineGridSnap, &RenderStyle::initialLineGridSnap>::createHandler());

    setPropertyHandler(CSSPropertyWebkitTextCombine, ApplyPropertyDefault<TextCombine, &RenderStyle::textCombine, TextCombine, &RenderStyle::setTextCombine, TextCombine, &RenderStyle::initialTextCombine>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTextEmphasisPosition, ApplyPropertyDefault<TextEmphasisPosition, &RenderStyle::textEmphasisPosition, TextEmphasisPosition, &RenderStyle::setTextEmphasisPosition, TextEmphasisPosition, &RenderStyle::initialTextEmphasisPosition>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTextEmphasisStyle, ApplyPropertyTextEmphasisStyle::createHandler());

    setPropertyHandler(CSSPropertyWebkitWrapMargin, ApplyPropertyLength<&RenderStyle::wrapMargin, &RenderStyle::setWrapMargin, &RenderStyle::initialWrapMargin>::createHandler());
    setPropertyHandler(CSSPropertyWebkitWrapPadding, ApplyPropertyLength<&RenderStyle::wrapPadding, &RenderStyle::setWrapPadding, &RenderStyle::initialWrapPadding>::createHandler());
    setPropertyHandler(CSSPropertyWebkitWrapFlow, ApplyPropertyDefault<WrapFlow, &RenderStyle::wrapFlow, WrapFlow, &RenderStyle::setWrapFlow, WrapFlow, &RenderStyle::initialWrapFlow>::createHandler());
    setPropertyHandler(CSSPropertyWebkitWrapThrough, ApplyPropertyDefault<WrapThrough, &RenderStyle::wrapThrough, WrapThrough, &RenderStyle::setWrapThrough, WrapThrough, &RenderStyle::initialWrapThrough>::createHandler());
    setPropertyHandler(CSSPropertyWebkitWrap, ApplyPropertyExpanding<SuppressValue, CSSPropertyWebkitWrapFlow, CSSPropertyWebkitWrapMargin, CSSPropertyWebkitWrapPadding>::createHandler());

    setPropertyHandler(CSSPropertyZIndex, ApplyPropertyAuto<int, &RenderStyle::zIndex, &RenderStyle::setZIndex, &RenderStyle::hasAutoZIndex, &RenderStyle::setHasAutoZIndex>::createHandler());
    setPropertyHandler(CSSPropertyZoom, ApplyPropertyZoom::createHandler());
}


}
