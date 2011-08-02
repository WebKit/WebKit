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

#include "CSSCursorImageValue.h"
#include "CSSFlexValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSStyleSelector.h"
#include "CSSValueList.h"
#include "CursorList.h"
#include "Document.h"
#include "Element.h"
#include "Pair.h"
#include "RenderStyle.h"
#include <wtf/StdLibExtras.h>
#include <wtf/UnusedParam.h>

using namespace std;

namespace WebCore {

enum ExpandValueBehavior {SuppressValue = 0, ExpandValue};
template <ExpandValueBehavior expandValue>
class ApplyPropertyExpanding : public ApplyPropertyBase {
public:
    ApplyPropertyExpanding(ApplyPropertyBase* one = 0, ApplyPropertyBase* two = 0, ApplyPropertyBase *three = 0, ApplyPropertyBase* four = 0)
    {
        m_propertyMap[0] = one;
        m_propertyMap[1] = two;
        m_propertyMap[2] = three;
        m_propertyMap[3] = four;
        m_propertyMap[4] = 0; // always null terminated
    }

    virtual void applyInheritValue(CSSStyleSelector* selector) const
    {
        for (ApplyPropertyBase* const* e = m_propertyMap; *e; e++)
            (*e)->applyInheritValue(selector);
    }

    virtual void applyInitialValue(CSSStyleSelector* selector) const
    {
        for (ApplyPropertyBase* const* e = m_propertyMap; *e; e++)
            (*e)->applyInitialValue(selector);
    }

    virtual void applyValue(CSSStyleSelector* selector, CSSValue* value) const
    {
        if (!expandValue)
            return;

        for (ApplyPropertyBase* const* e = m_propertyMap; *e; e++)
            (*e)->applyValue(selector, value);
    }
private:
    ApplyPropertyBase* m_propertyMap[5];
};

template <typename T>
class ApplyPropertyDefaultBase : public ApplyPropertyBase {
public:
    typedef T (RenderStyle::*GetterFunction)() const;
    typedef void (RenderStyle::*SetterFunction)(T);
    typedef T (*InitialFunction)();

    ApplyPropertyDefaultBase(GetterFunction getter, SetterFunction setter, InitialFunction initial)
        : m_getter(getter)
        , m_setter(setter)
        , m_initial(initial)
    {
    }

private:
    virtual void applyInheritValue(CSSStyleSelector* selector) const
    {
        setValue(selector->style(), value(selector->parentStyle()));
    }

    virtual void applyInitialValue(CSSStyleSelector* selector) const
    {
        setValue(selector->style(), initial());
    }

protected:
    void setValue(RenderStyle* style, T value) const
    {
        (style->*m_setter)(value);
    }

    T value(RenderStyle* style) const
    {
        return (style->*m_getter)();
    }

    T initial() const
    {
        return (*m_initial)();
    }

    GetterFunction m_getter;
    SetterFunction m_setter;
    InitialFunction m_initial;
};


template <typename T>
class ApplyPropertyDefault : public ApplyPropertyDefaultBase<T> {
public:
    ApplyPropertyDefault(typename ApplyPropertyDefaultBase<T>::GetterFunction getter, typename ApplyPropertyDefaultBase<T>::SetterFunction setter, typename ApplyPropertyDefaultBase<T>::InitialFunction initial)
        : ApplyPropertyDefaultBase<T>(getter, setter, initial)
    {
    }

protected:
    virtual void applyValue(CSSStyleSelector* selector, CSSValue* value) const
    {
        if (value->isPrimitiveValue())
            ApplyPropertyDefaultBase<T>::setValue(selector->style(), *static_cast<CSSPrimitiveValue*>(value));
    }
};

enum AutoValueType {Number = 0, ComputeLength};
template <typename T, AutoValueType valueType = Number, int autoIdentity = CSSValueAuto>
class ApplyPropertyAuto : public ApplyPropertyDefaultBase<T> {
public:
    typedef bool (RenderStyle::*HasAutoFunction)() const;
    typedef void (RenderStyle::*SetAutoFunction)();

    ApplyPropertyAuto(typename ApplyPropertyDefaultBase<T>::GetterFunction getter, typename ApplyPropertyDefaultBase<T>::SetterFunction setter, HasAutoFunction hasAuto, SetAutoFunction setAuto)
        : ApplyPropertyDefaultBase<T>(getter, setter, 0)
        , m_hasAuto(hasAuto)
        , m_setAuto(setAuto)
    {
    }

protected:
    virtual void applyInheritValue(CSSStyleSelector* selector) const
    {
        if (hasAuto(selector->parentStyle()))
            setAuto(selector->style());
        else
            ApplyPropertyDefaultBase<T>::setValue(selector->style(), ApplyPropertyDefaultBase<T>::value(selector->parentStyle()));
    }

    virtual void applyInitialValue(CSSStyleSelector* selector) const
    {
        setAuto(selector->style());
    }

    virtual void applyValue(CSSStyleSelector* selector, CSSValue* value) const
    {
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        if (primitiveValue->getIdent() == autoIdentity)
            setAuto(selector->style());
        else if (valueType == Number)
            ApplyPropertyDefaultBase<T>::setValue(selector->style(), *primitiveValue);
        else if (valueType == ComputeLength)
            ApplyPropertyDefaultBase<T>::setValue(selector->style(), primitiveValue->computeLength<T>(selector->style(), selector->rootElementStyle(), selector->style()->effectiveZoom()));
    }

    bool hasAuto(RenderStyle* style) const
    {
        return (style->*m_hasAuto)();
    }

    void setAuto(RenderStyle* style) const
    {
        (style->*m_setAuto)();
    }

    HasAutoFunction m_hasAuto;
    SetAutoFunction m_setAuto;
};

enum ColorInherit {NoInheritFromParent = 0, InheritFromParent};
template <ColorInherit inheritColorFromParent>
class ApplyPropertyColor : public ApplyPropertyBase {
public:
    typedef const Color& (RenderStyle::*GetterFunction)() const;
    typedef void (RenderStyle::*SetterFunction)(const Color&);
    typedef const Color& (RenderStyle::*DefaultFunction)() const;
    typedef Color (*InitialFunction)();

    ApplyPropertyColor(GetterFunction getter, SetterFunction setter, DefaultFunction defaultFunction, InitialFunction initialFunction = 0)
        : m_getter(getter)
        , m_setter(setter)
        , m_default(defaultFunction)
        , m_initial(initialFunction)
    {
    }

private:
    virtual void applyInheritValue(CSSStyleSelector* selector) const
    {
        const Color& color = (selector->parentStyle()->*m_getter)();
        if (m_default && !color.isValid())
            (selector->style()->*m_setter)((selector->parentStyle()->*m_default)());
        else
            (selector->style()->*m_setter)(color);
    }

    virtual void applyInitialValue(CSSStyleSelector* selector) const
    {
        (selector->style()->*m_setter)(m_initial ? m_initial() : Color());
    }

    virtual void applyValue(CSSStyleSelector* selector, CSSValue* value) const
    {
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        if (inheritColorFromParent && primitiveValue->getIdent() == CSSValueCurrentcolor)
            applyInheritValue(selector);
        else
            (selector->style()->*m_setter)(selector->getColorFromPrimitiveValue(primitiveValue));
    }

    GetterFunction m_getter;
    SetterFunction m_setter;
    DefaultFunction m_default;
    InitialFunction m_initial;
};

class ApplyPropertyDirection : public ApplyPropertyDefault<TextDirection> {
public:
    ApplyPropertyDirection(GetterFunction getter, SetterFunction setter, InitialFunction initial)
        : ApplyPropertyDefault<TextDirection>(getter, setter, initial)
    {
    }

private:
    virtual void applyValue(CSSStyleSelector* selector, CSSValue* value) const
    {
        ApplyPropertyDefault<TextDirection>::applyValue(selector, value);
        Element* element = selector->element();
        if (element && selector->element() == element->document()->documentElement())
            element->document()->setDirectionSetOnDocumentElement(true);
    }
};

enum LengthAuto {AutoDisabled = 0, AutoEnabled = 1};
enum LengthIntrinsic {IntrinsicDisabled = 0, IntrinsicEnabled = 1};
enum LengthMinIntrinsic {MinIntrinsicDisabled = 0, MinIntrinsicEnabled = 1};
enum LengthNone {NoneDisabled = 0, NoneEnabled = 1};
enum LengthUndefined {UndefinedDisabled = 0, UndefinedEnabled = 1};
enum LengthFlexDirection {FlexDirectionDisabled = 0, FlexWidth = 1, FlexHeight};
template <LengthAuto autoEnabled = AutoDisabled,
          LengthIntrinsic intrinsicEnabled = IntrinsicDisabled,
          LengthMinIntrinsic minIntrinsicEnabled = MinIntrinsicDisabled,
          LengthNone noneEnabled = NoneDisabled,
          LengthUndefined noneUndefined = UndefinedDisabled,
          LengthFlexDirection flexDirection = FlexDirectionDisabled>
class ApplyPropertyLength : public ApplyPropertyDefaultBase<Length> {
public:
    ApplyPropertyLength(GetterFunction getter, SetterFunction setter, InitialFunction initial)
        : ApplyPropertyDefaultBase<Length>(getter, setter, initial)
    {
    }

private:
    virtual void applyValue(CSSStyleSelector* selector, CSSValue* value) const
    {
#if ENABLE(CSS3_FLEXBOX)
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
#else
        if (!value->isPrimitiveValue())
            return;
#endif

        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        if (noneEnabled && primitiveValue->getIdent() == CSSValueNone)
            if (noneUndefined)
                setValue(selector->style(), Length(undefinedLength, Fixed));
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
};

class ApplyPropertyBorderRadius : public ApplyPropertyDefaultBase<LengthSize> {
public:
    ApplyPropertyBorderRadius(GetterFunction getter, SetterFunction setter, InitialFunction initial)
        : ApplyPropertyDefaultBase<LengthSize>(getter, setter, initial)
    {
    }
private:
    virtual void applyValue(CSSStyleSelector* selector, CSSValue* value) const
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
};

template <typename T>
class ApplyPropertyFillLayer : public ApplyPropertyBase {
public:
    ApplyPropertyFillLayer(CSSPropertyID propertyId, EFillLayerType fillLayerType, FillLayer* (RenderStyle::*accessLayers)(),
                           const FillLayer* (RenderStyle::*layers)() const, bool (FillLayer::*test)() const, T (FillLayer::*get)() const,
                           void (FillLayer::*set)(T), void (FillLayer::*clear)(), T (*initial)(EFillLayerType),
                           void (CSSStyleSelector::*mapFill)(CSSPropertyID, FillLayer*, CSSValue*))
        : m_propertyId(propertyId)
        , m_fillLayerType(fillLayerType)
        , m_accessLayers(accessLayers)
        , m_layers(layers)
        , m_test(test)
        , m_get(get)
        , m_set(set)
        , m_clear(clear)
        , m_initial(initial)
        , m_mapFill(mapFill)
    {
    }

private:
    virtual void applyInheritValue(CSSStyleSelector* selector) const
    {
        FillLayer* currChild = (selector->style()->*m_accessLayers)();
        FillLayer* prevChild = 0;
        const FillLayer* currParent = (selector->parentStyle()->*m_layers)();
        while (currParent && (currParent->*m_test)()) {
            if (!currChild) {
                /* Need to make a new layer.*/
                currChild = new FillLayer(m_fillLayerType);
                prevChild->setNext(currChild);
            }
            (currChild->*m_set)((currParent->*m_get)());
            prevChild = currChild;
            currChild = prevChild->next();
            currParent = currParent->next();
        }

        while (currChild) {
            /* Reset any remaining layers to not have the property set. */
            (currChild->*m_clear)();
            currChild = currChild->next();
        }
    }

    virtual void applyInitialValue(CSSStyleSelector* selector) const
    {
        FillLayer* currChild = (selector->style()->*m_accessLayers)();
        (currChild->*m_set)((*m_initial)(m_fillLayerType));
        for (currChild = currChild->next(); currChild; currChild = currChild->next())
            (currChild->*m_clear)();
    }

    virtual void applyValue(CSSStyleSelector* selector, CSSValue* value) const
    {
        FillLayer* currChild = (selector->style()->*m_accessLayers)();
        FillLayer* prevChild = 0;
        if (value->isValueList()) {
            /* Walk each value and put it into a layer, creating new layers as needed. */
            CSSValueList* valueList = static_cast<CSSValueList*>(value);
            for (unsigned int i = 0; i < valueList->length(); i++) {
                if (!currChild) {
                    /* Need to make a new layer to hold this value */
                    currChild = new FillLayer(m_fillLayerType);
                    prevChild->setNext(currChild);
                }
                (selector->*m_mapFill)(m_propertyId, currChild, valueList->itemWithoutBoundsCheck(i));
                prevChild = currChild;
                currChild = currChild->next();
            }
        } else {
            (selector->*m_mapFill)(m_propertyId, currChild, value);
            currChild = currChild->next();
        }
        while (currChild) {
            /* Reset all remaining layers to not have the property set. */
            (currChild->*m_clear)();
            currChild = currChild->next();
        }
    }

protected:
    CSSPropertyID m_propertyId;
    EFillLayerType m_fillLayerType;
    FillLayer* (RenderStyle::*m_accessLayers)();
    const FillLayer* (RenderStyle::*m_layers)() const;
    bool (FillLayer::*m_test)() const;
    T (FillLayer::*m_get)() const;
    void (FillLayer::*m_set)(T);
    void (FillLayer::*m_clear)();
    T (*m_initial)(EFillLayerType);
    void (CSSStyleSelector::*m_mapFill)(CSSPropertyID, FillLayer*, CSSValue*);
};

enum ComputeLengthNormal {NormalDisabled = 0, NormalEnabled};
enum ComputeLengthThickness {ThicknessDisabled = 0, ThicknessEnabled};
enum ComputeLengthSVGZoom {SVGZoomDisabled = 0, SVGZoomEnabled};
template <typename T,
          ComputeLengthNormal normalEnabled = NormalDisabled,
          ComputeLengthThickness thicknessEnabled = ThicknessDisabled,
          ComputeLengthSVGZoom svgZoomEnabled = SVGZoomDisabled>
class ApplyPropertyComputeLength : public ApplyPropertyDefaultBase<T> {
public:
    ApplyPropertyComputeLength(typename ApplyPropertyDefaultBase<T>::GetterFunction getter, typename ApplyPropertyDefaultBase<T>::SetterFunction setter, typename ApplyPropertyDefaultBase<T>::InitialFunction initial)
        : ApplyPropertyDefaultBase<T>(getter, setter, initial)
    {
    }

private:
    virtual void applyValue(CSSStyleSelector* selector, CSSValue* value) const
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

        this->setValue(selector->style(), length);
    }
};

template <typename T>
class ApplyPropertyFont : public ApplyPropertyBase {
public:
    typedef T (FontDescription::*GetterFunction)() const;
    typedef void (FontDescription::*SetterFunction)(T);

    ApplyPropertyFont(GetterFunction getter, SetterFunction setter, T initial)
        : m_getter(getter)
        , m_setter(setter)
        , m_initial(initial)
    {
    }

private:
    virtual void applyInheritValue(CSSStyleSelector* selector) const
    {
        FontDescription fontDescription = selector->fontDescription();
        (fontDescription.*m_setter)((selector->parentFontDescription().*m_getter)());
        selector->setFontDescription(fontDescription);
    }

    virtual void applyInitialValue(CSSStyleSelector* selector) const
    {
        FontDescription fontDescription = selector->fontDescription();
        (fontDescription.*m_setter)(m_initial);
        selector->setFontDescription(fontDescription);
    }

    virtual void applyValue(CSSStyleSelector* selector, CSSValue* value) const
    {
        if (!value->isPrimitiveValue())
            return;
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        FontDescription fontDescription = selector->fontDescription();
        (fontDescription.*m_setter)(*primitiveValue);
        selector->setFontDescription(fontDescription);
    }

    GetterFunction m_getter;
    SetterFunction m_setter;
    T m_initial;
};

class ApplyPropertyFontWeight : public ApplyPropertyFont<FontWeight> {
public:
    ApplyPropertyFontWeight()
        : ApplyPropertyFont<FontWeight>(&FontDescription::weight, &FontDescription::setWeight, FontWeightNormal)
    {
    }

    virtual void applyValue(CSSStyleSelector* selector, CSSValue* value) const
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
};

enum CounterBehavior {Increment = 0, Reset};
template <CounterBehavior counterBehavior>
class ApplyPropertyCounter : public ApplyPropertyBase {
private:
    virtual void applyInheritValue(CSSStyleSelector*) const { }
    virtual void applyInitialValue(CSSStyleSelector*) const { }

    virtual void applyValue(CSSStyleSelector* selector, CSSValue* value) const
    {
        if (!value->isValueList())
            return;

        CSSValueList* list = static_cast<CSSValueList*>(value);

        CounterDirectiveMap& map = selector->style()->accessCounterDirectives();
        typedef CounterDirectiveMap::iterator Iterator;

        Iterator end = map.end();
        for (Iterator it = map.begin(); it != end; ++it)
            if (counterBehavior)
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
            // FIXME: What about overflow?
            int value = static_cast<CSSPrimitiveValue*>(pair->second())->getIntValue();
            CounterDirectives& directives = map.add(identifier.impl(), CounterDirectives()).first->second;
            if (counterBehavior) {
                directives.m_reset = true;
                directives.m_resetValue = value;
            } else {
                if (directives.m_increment)
                    directives.m_incrementValue += value;
                else {
                    directives.m_increment = true;
                    directives.m_incrementValue = value;
                }
            }
        }
    }
};


class ApplyPropertyCursor : public ApplyPropertyBase {
private:
    virtual void applyInheritValue(CSSStyleSelector* selector) const
    {
        selector->style()->setCursor(selector->parentStyle()->cursor());
        selector->style()->setCursorList(selector->parentStyle()->cursors());
    }

    virtual void applyInitialValue(CSSStyleSelector* selector) const
    {
        selector->style()->clearCursorList();
        selector->style()->setCursor(RenderStyle::initialCursor());
    }

    virtual void applyValue(CSSStyleSelector* selector, CSSValue* value) const
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
};

class ApplyPropertyTextEmphasisStyle : public ApplyPropertyBase {
private:
    virtual void applyInheritValue(CSSStyleSelector* selector) const
    {
        selector->style()->setTextEmphasisFill(selector->parentStyle()->textEmphasisFill());
        selector->style()->setTextEmphasisMark(selector->parentStyle()->textEmphasisMark());
        selector->style()->setTextEmphasisCustomMark(selector->parentStyle()->textEmphasisCustomMark());
    }

    virtual void applyInitialValue(CSSStyleSelector* selector) const
    {
        selector->style()->setTextEmphasisFill(RenderStyle::initialTextEmphasisFill());
        selector->style()->setTextEmphasisMark(RenderStyle::initialTextEmphasisMark());
        selector->style()->setTextEmphasisCustomMark(RenderStyle::initialTextEmphasisCustomMark());
    }

    virtual void applyValue(CSSStyleSelector* selector, CSSValue* value) const
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
};

const CSSStyleApplyProperty& CSSStyleApplyProperty::sharedCSSStyleApplyProperty()
{
    DEFINE_STATIC_LOCAL(CSSStyleApplyProperty, cssStyleApplyPropertyInstance, ());
    return cssStyleApplyPropertyInstance;
}

CSSStyleApplyProperty::CSSStyleApplyProperty()
{
    for (int i = 0; i < numCSSProperties; ++i)
       m_propertyMap[i] = 0;

    setPropertyHandler(CSSPropertyColor, new ApplyPropertyColor<InheritFromParent>(&RenderStyle::color, &RenderStyle::setColor, 0, RenderStyle::initialColor));
    setPropertyHandler(CSSPropertyDirection, new ApplyPropertyDirection(&RenderStyle::direction, &RenderStyle::setDirection, RenderStyle::initialDirection));

    setPropertyHandler(CSSPropertyBackgroundAttachment, new ApplyPropertyFillLayer<EFillAttachment>(CSSPropertyBackgroundAttachment, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
            &FillLayer::isAttachmentSet, &FillLayer::attachment, &FillLayer::setAttachment, &FillLayer::clearAttachment, &FillLayer::initialFillAttachment, &CSSStyleSelector::mapFillAttachment));
    setPropertyHandler(CSSPropertyBackgroundClip, new ApplyPropertyFillLayer<EFillBox>(CSSPropertyBackgroundClip, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
            &FillLayer::isClipSet, &FillLayer::clip, &FillLayer::setClip, &FillLayer::clearClip, &FillLayer::initialFillClip, &CSSStyleSelector::mapFillClip));
    setPropertyHandler(CSSPropertyWebkitBackgroundClip, CSSPropertyBackgroundClip);
    setPropertyHandler(CSSPropertyWebkitBackgroundComposite, new ApplyPropertyFillLayer<CompositeOperator>(CSSPropertyWebkitBackgroundComposite, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
            &FillLayer::isCompositeSet, &FillLayer::composite, &FillLayer::setComposite, &FillLayer::clearComposite, &FillLayer::initialFillComposite, &CSSStyleSelector::mapFillComposite));

    setPropertyHandler(CSSPropertyBackgroundImage, new ApplyPropertyFillLayer<StyleImage*>(CSSPropertyBackgroundImage, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
                &FillLayer::isImageSet, &FillLayer::image, &FillLayer::setImage, &FillLayer::clearImage, &FillLayer::initialFillImage, &CSSStyleSelector::mapFillImage));

    setPropertyHandler(CSSPropertyBackgroundOrigin, new ApplyPropertyFillLayer<EFillBox>(CSSPropertyBackgroundOrigin, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
            &FillLayer::isOriginSet, &FillLayer::origin, &FillLayer::setOrigin, &FillLayer::clearOrigin, &FillLayer::initialFillOrigin, &CSSStyleSelector::mapFillOrigin));
    setPropertyHandler(CSSPropertyWebkitBackgroundOrigin, CSSPropertyBackgroundOrigin);

    setPropertyHandler(CSSPropertyBackgroundPositionX, new ApplyPropertyFillLayer<Length>(CSSPropertyBackgroundPositionX, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
                &FillLayer::isXPositionSet, &FillLayer::xPosition, &FillLayer::setXPosition, &FillLayer::clearXPosition, &FillLayer::initialFillXPosition, &CSSStyleSelector::mapFillXPosition));
    setPropertyHandler(CSSPropertyBackgroundPositionY, new ApplyPropertyFillLayer<Length>(CSSPropertyBackgroundPositionY, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
                    &FillLayer::isYPositionSet, &FillLayer::yPosition, &FillLayer::setYPosition, &FillLayer::clearYPosition, &FillLayer::initialFillYPosition, &CSSStyleSelector::mapFillYPosition));
    setPropertyHandler(CSSPropertyBackgroundPosition, new ApplyPropertyExpanding<SuppressValue>(propertyHandler(CSSPropertyBackgroundPositionX), propertyHandler(CSSPropertyBackgroundPositionY)));

    setPropertyHandler(CSSPropertyBackgroundRepeatX, new ApplyPropertyFillLayer<EFillRepeat>(CSSPropertyBackgroundRepeatX, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
                &FillLayer::isRepeatXSet, &FillLayer::repeatX, &FillLayer::setRepeatX, &FillLayer::clearRepeatX, &FillLayer::initialFillRepeatX, &CSSStyleSelector::mapFillRepeatX));
    setPropertyHandler(CSSPropertyBackgroundRepeatY, new ApplyPropertyFillLayer<EFillRepeat>(CSSPropertyBackgroundRepeatY, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
                    &FillLayer::isRepeatYSet, &FillLayer::repeatY, &FillLayer::setRepeatY, &FillLayer::clearRepeatY, &FillLayer::initialFillRepeatY, &CSSStyleSelector::mapFillRepeatY));
    setPropertyHandler(CSSPropertyBackgroundRepeat, new ApplyPropertyExpanding<SuppressValue>(propertyHandler(CSSPropertyBackgroundRepeatX), propertyHandler(CSSPropertyBackgroundRepeatY)));

    setPropertyHandler(CSSPropertyBackgroundSize, new ApplyPropertyFillLayer<FillSize>(CSSPropertyBackgroundSize, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
            &FillLayer::isSizeSet, &FillLayer::size, &FillLayer::setSize, &FillLayer::clearSize, &FillLayer::initialFillSize, &CSSStyleSelector::mapFillSize));
    setPropertyHandler(CSSPropertyWebkitBackgroundSize, CSSPropertyBackgroundSize);

    setPropertyHandler(CSSPropertyWebkitMaskAttachment, new ApplyPropertyFillLayer<EFillAttachment>(CSSPropertyWebkitMaskAttachment, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
            &FillLayer::isAttachmentSet, &FillLayer::attachment, &FillLayer::setAttachment, &FillLayer::clearAttachment, &FillLayer::initialFillAttachment, &CSSStyleSelector::mapFillAttachment));
    setPropertyHandler(CSSPropertyWebkitMaskClip, new ApplyPropertyFillLayer<EFillBox>(CSSPropertyWebkitMaskClip, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
            &FillLayer::isClipSet, &FillLayer::clip, &FillLayer::setClip, &FillLayer::clearClip, &FillLayer::initialFillClip, &CSSStyleSelector::mapFillClip));
    setPropertyHandler(CSSPropertyWebkitMaskComposite, new ApplyPropertyFillLayer<CompositeOperator>(CSSPropertyWebkitMaskComposite, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
            &FillLayer::isCompositeSet, &FillLayer::composite, &FillLayer::setComposite, &FillLayer::clearComposite, &FillLayer::initialFillComposite, &CSSStyleSelector::mapFillComposite));

    setPropertyHandler(CSSPropertyWebkitMaskImage, new ApplyPropertyFillLayer<StyleImage*>(CSSPropertyWebkitMaskImage, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
                &FillLayer::isImageSet, &FillLayer::image, &FillLayer::setImage, &FillLayer::clearImage, &FillLayer::initialFillImage, &CSSStyleSelector::mapFillImage));

    setPropertyHandler(CSSPropertyWebkitMaskOrigin, new ApplyPropertyFillLayer<EFillBox>(CSSPropertyWebkitMaskOrigin, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
            &FillLayer::isOriginSet, &FillLayer::origin, &FillLayer::setOrigin, &FillLayer::clearOrigin, &FillLayer::initialFillOrigin, &CSSStyleSelector::mapFillOrigin));
    setPropertyHandler(CSSPropertyWebkitMaskSize, new ApplyPropertyFillLayer<FillSize>(CSSPropertyWebkitMaskSize, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
            &FillLayer::isSizeSet, &FillLayer::size, &FillLayer::setSize, &FillLayer::clearSize, &FillLayer::initialFillSize, &CSSStyleSelector::mapFillSize));

    setPropertyHandler(CSSPropertyWebkitMaskPositionX, new ApplyPropertyFillLayer<Length>(CSSPropertyWebkitMaskPositionX, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
                &FillLayer::isXPositionSet, &FillLayer::xPosition, &FillLayer::setXPosition, &FillLayer::clearXPosition, &FillLayer::initialFillXPosition, &CSSStyleSelector::mapFillXPosition));
    setPropertyHandler(CSSPropertyWebkitMaskPositionY, new ApplyPropertyFillLayer<Length>(CSSPropertyWebkitMaskPositionY, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
                    &FillLayer::isYPositionSet, &FillLayer::yPosition, &FillLayer::setYPosition, &FillLayer::clearYPosition, &FillLayer::initialFillYPosition, &CSSStyleSelector::mapFillYPosition));
    setPropertyHandler(CSSPropertyWebkitMaskPosition, new ApplyPropertyExpanding<SuppressValue>(propertyHandler(CSSPropertyWebkitMaskPositionX), propertyHandler(CSSPropertyWebkitMaskPositionY)));

    setPropertyHandler(CSSPropertyWebkitMaskRepeatX, new ApplyPropertyFillLayer<EFillRepeat>(CSSPropertyWebkitMaskRepeatX, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
                &FillLayer::isRepeatXSet, &FillLayer::repeatX, &FillLayer::setRepeatX, &FillLayer::clearRepeatX, &FillLayer::initialFillRepeatX, &CSSStyleSelector::mapFillRepeatX));
    setPropertyHandler(CSSPropertyWebkitMaskRepeatY, new ApplyPropertyFillLayer<EFillRepeat>(CSSPropertyWebkitMaskRepeatY, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
                    &FillLayer::isRepeatYSet, &FillLayer::repeatY, &FillLayer::setRepeatY, &FillLayer::clearRepeatY, &FillLayer::initialFillRepeatY, &CSSStyleSelector::mapFillRepeatY));
    setPropertyHandler(CSSPropertyWebkitMaskRepeat, new ApplyPropertyExpanding<SuppressValue>(propertyHandler(CSSPropertyBackgroundRepeatX), propertyHandler(CSSPropertyBackgroundRepeatY)));

    setPropertyHandler(CSSPropertyBackgroundColor, new ApplyPropertyColor<NoInheritFromParent>(&RenderStyle::backgroundColor, &RenderStyle::setBackgroundColor, 0));
    setPropertyHandler(CSSPropertyBorderBottomColor, new ApplyPropertyColor<NoInheritFromParent>(&RenderStyle::borderBottomColor, &RenderStyle::setBorderBottomColor, &RenderStyle::color));
    setPropertyHandler(CSSPropertyBorderLeftColor, new ApplyPropertyColor<NoInheritFromParent>(&RenderStyle::borderLeftColor, &RenderStyle::setBorderLeftColor, &RenderStyle::color));
    setPropertyHandler(CSSPropertyBorderRightColor, new ApplyPropertyColor<NoInheritFromParent>(&RenderStyle::borderRightColor, &RenderStyle::setBorderRightColor, &RenderStyle::color));
    setPropertyHandler(CSSPropertyBorderTopColor, new ApplyPropertyColor<NoInheritFromParent>(&RenderStyle::borderTopColor, &RenderStyle::setBorderTopColor, &RenderStyle::color));

    setPropertyHandler(CSSPropertyBorderTopStyle, new ApplyPropertyDefault<EBorderStyle>(&RenderStyle::borderTopStyle, &RenderStyle::setBorderTopStyle, &RenderStyle::initialBorderStyle));
    setPropertyHandler(CSSPropertyBorderRightStyle, new ApplyPropertyDefault<EBorderStyle>(&RenderStyle::borderRightStyle, &RenderStyle::setBorderRightStyle, &RenderStyle::initialBorderStyle));
    setPropertyHandler(CSSPropertyBorderBottomStyle, new ApplyPropertyDefault<EBorderStyle>(&RenderStyle::borderBottomStyle, &RenderStyle::setBorderBottomStyle, &RenderStyle::initialBorderStyle));
    setPropertyHandler(CSSPropertyBorderLeftStyle, new ApplyPropertyDefault<EBorderStyle>(&RenderStyle::borderLeftStyle, &RenderStyle::setBorderLeftStyle, &RenderStyle::initialBorderStyle));

    setPropertyHandler(CSSPropertyBorderTopWidth, new ApplyPropertyComputeLength<unsigned short, NormalDisabled, ThicknessEnabled>(&RenderStyle::borderTopWidth, &RenderStyle::setBorderTopWidth, &RenderStyle::initialBorderWidth));
    setPropertyHandler(CSSPropertyBorderRightWidth, new ApplyPropertyComputeLength<unsigned short, NormalDisabled, ThicknessEnabled>(&RenderStyle::borderRightWidth, &RenderStyle::setBorderRightWidth, &RenderStyle::initialBorderWidth));
    setPropertyHandler(CSSPropertyBorderBottomWidth, new ApplyPropertyComputeLength<unsigned short, NormalDisabled, ThicknessEnabled>(&RenderStyle::borderBottomWidth, &RenderStyle::setBorderBottomWidth, &RenderStyle::initialBorderWidth));
    setPropertyHandler(CSSPropertyBorderLeftWidth, new ApplyPropertyComputeLength<unsigned short, NormalDisabled, ThicknessEnabled>(&RenderStyle::borderLeftWidth, &RenderStyle::setBorderLeftWidth, &RenderStyle::initialBorderWidth));
    setPropertyHandler(CSSPropertyOutlineWidth, new ApplyPropertyComputeLength<unsigned short, NormalDisabled, ThicknessEnabled>(&RenderStyle::outlineWidth, &RenderStyle::setOutlineWidth, &RenderStyle::initialBorderWidth));
    setPropertyHandler(CSSPropertyWebkitColumnRuleWidth, new ApplyPropertyComputeLength<unsigned short, NormalDisabled, ThicknessEnabled>(&RenderStyle::columnRuleWidth, &RenderStyle::setColumnRuleWidth, &RenderStyle::initialBorderWidth));

    setPropertyHandler(CSSPropertyBorderTop, new ApplyPropertyExpanding<SuppressValue>(propertyHandler(CSSPropertyBorderTopColor), propertyHandler(CSSPropertyBorderTopStyle), propertyHandler(CSSPropertyBorderTopWidth)));
    setPropertyHandler(CSSPropertyBorderRight, new ApplyPropertyExpanding<SuppressValue>(propertyHandler(CSSPropertyBorderRightColor), propertyHandler(CSSPropertyBorderRightStyle), propertyHandler(CSSPropertyBorderRightWidth)));
    setPropertyHandler(CSSPropertyBorderBottom, new ApplyPropertyExpanding<SuppressValue>(propertyHandler(CSSPropertyBorderBottomColor), propertyHandler(CSSPropertyBorderBottomStyle), propertyHandler(CSSPropertyBorderBottomWidth)));
    setPropertyHandler(CSSPropertyBorderLeft, new ApplyPropertyExpanding<SuppressValue>(propertyHandler(CSSPropertyBorderLeftColor), propertyHandler(CSSPropertyBorderLeftStyle), propertyHandler(CSSPropertyBorderLeftWidth)));

    setPropertyHandler(CSSPropertyBorderStyle, new ApplyPropertyExpanding<SuppressValue>(propertyHandler(CSSPropertyBorderTopStyle), propertyHandler(CSSPropertyBorderRightStyle), propertyHandler(CSSPropertyBorderBottomStyle), propertyHandler(CSSPropertyBorderLeftStyle)));
    setPropertyHandler(CSSPropertyBorderWidth, new ApplyPropertyExpanding<SuppressValue>(propertyHandler(CSSPropertyBorderTopWidth), propertyHandler(CSSPropertyBorderRightWidth), propertyHandler(CSSPropertyBorderBottomWidth), propertyHandler(CSSPropertyBorderLeftWidth)));
    setPropertyHandler(CSSPropertyBorderColor, new ApplyPropertyExpanding<SuppressValue>(propertyHandler(CSSPropertyBorderTopColor), propertyHandler(CSSPropertyBorderRightColor), propertyHandler(CSSPropertyBorderBottomColor), propertyHandler(CSSPropertyBorderLeftColor)));
    setPropertyHandler(CSSPropertyBorder, new ApplyPropertyExpanding<SuppressValue>(propertyHandler(CSSPropertyBorderStyle), propertyHandler(CSSPropertyBorderWidth), propertyHandler(CSSPropertyBorderColor)));

    setPropertyHandler(CSSPropertyBorderTopLeftRadius, new ApplyPropertyBorderRadius(&RenderStyle::borderTopLeftRadius, &RenderStyle::setBorderTopLeftRadius, &RenderStyle::initialBorderRadius));
    setPropertyHandler(CSSPropertyBorderTopRightRadius, new ApplyPropertyBorderRadius(&RenderStyle::borderTopRightRadius, &RenderStyle::setBorderTopRightRadius, &RenderStyle::initialBorderRadius));
    setPropertyHandler(CSSPropertyBorderBottomLeftRadius, new ApplyPropertyBorderRadius(&RenderStyle::borderBottomLeftRadius, &RenderStyle::setBorderBottomLeftRadius, &RenderStyle::initialBorderRadius));
    setPropertyHandler(CSSPropertyBorderBottomRightRadius, new ApplyPropertyBorderRadius(&RenderStyle::borderBottomRightRadius, &RenderStyle::setBorderBottomRightRadius, &RenderStyle::initialBorderRadius));
    setPropertyHandler(CSSPropertyBorderRadius, new ApplyPropertyExpanding<ExpandValue>(propertyHandler(CSSPropertyBorderTopLeftRadius), propertyHandler(CSSPropertyBorderTopRightRadius), propertyHandler(CSSPropertyBorderBottomLeftRadius), propertyHandler(CSSPropertyBorderBottomRightRadius)));
    setPropertyHandler(CSSPropertyWebkitBorderRadius, CSSPropertyBorderRadius);

    setPropertyHandler(CSSPropertyWebkitBorderHorizontalSpacing, new ApplyPropertyComputeLength<short>(&RenderStyle::horizontalBorderSpacing, &RenderStyle::setHorizontalBorderSpacing, &RenderStyle::initialHorizontalBorderSpacing));
    setPropertyHandler(CSSPropertyWebkitBorderVerticalSpacing, new ApplyPropertyComputeLength<short>(&RenderStyle::verticalBorderSpacing, &RenderStyle::setVerticalBorderSpacing, &RenderStyle::initialVerticalBorderSpacing));
    setPropertyHandler(CSSPropertyBorderSpacing, new ApplyPropertyExpanding<SuppressValue>(propertyHandler(CSSPropertyWebkitBorderHorizontalSpacing), propertyHandler(CSSPropertyWebkitBorderVerticalSpacing)));

    setPropertyHandler(CSSPropertyLetterSpacing, new ApplyPropertyComputeLength<int, NormalEnabled, ThicknessDisabled, SVGZoomEnabled>(&RenderStyle::letterSpacing, &RenderStyle::setLetterSpacing, &RenderStyle::initialLetterWordSpacing));
    setPropertyHandler(CSSPropertyWordSpacing, new ApplyPropertyComputeLength<int, NormalEnabled, ThicknessDisabled, SVGZoomEnabled>(&RenderStyle::wordSpacing, &RenderStyle::setWordSpacing, &RenderStyle::initialLetterWordSpacing));

    setPropertyHandler(CSSPropertyCursor, new ApplyPropertyCursor());

    setPropertyHandler(CSSPropertyCounterIncrement, new ApplyPropertyCounter<Increment>());
    setPropertyHandler(CSSPropertyCounterReset, new ApplyPropertyCounter<Reset>());

    setPropertyHandler(CSSPropertyFontStyle, new ApplyPropertyFont<FontItalic>(&FontDescription::italic, &FontDescription::setItalic, FontItalicOff));
    setPropertyHandler(CSSPropertyFontVariant, new ApplyPropertyFont<FontSmallCaps>(&FontDescription::smallCaps, &FontDescription::setSmallCaps, FontSmallCapsOff));
    setPropertyHandler(CSSPropertyTextRendering, new ApplyPropertyFont<TextRenderingMode>(&FontDescription::textRenderingMode, &FontDescription::setTextRenderingMode, AutoTextRendering));
    setPropertyHandler(CSSPropertyWebkitFontSmoothing, new ApplyPropertyFont<FontSmoothingMode>(&FontDescription::fontSmoothing, &FontDescription::setFontSmoothing, AutoSmoothing));
    setPropertyHandler(CSSPropertyWebkitTextOrientation, new ApplyPropertyFont<TextOrientation>(&FontDescription::textOrientation, &FontDescription::setTextOrientation, RenderStyle::initialTextOrientation()));
    setPropertyHandler(CSSPropertyFontWeight, new ApplyPropertyFontWeight());

    setPropertyHandler(CSSPropertyOutlineStyle, new ApplyPropertyExpanding<ExpandValue>(new ApplyPropertyDefault<OutlineIsAuto>(&RenderStyle::outlineStyleIsAuto, &RenderStyle::setOutlineStyleIsAuto, &RenderStyle::initialOutlineStyleIsAuto), new ApplyPropertyDefault<EBorderStyle>(&RenderStyle::outlineStyle, &RenderStyle::setOutlineStyle, &RenderStyle::initialBorderStyle)));
    setPropertyHandler(CSSPropertyOutlineColor, new ApplyPropertyColor<InheritFromParent>(&RenderStyle::outlineColor, &RenderStyle::setOutlineColor, &RenderStyle::color));
    setPropertyHandler(CSSPropertyOutlineOffset, new ApplyPropertyComputeLength<int>(&RenderStyle::outlineOffset, &RenderStyle::setOutlineOffset, &RenderStyle::initialOutlineOffset));

    setPropertyHandler(CSSPropertyOverflowX, new ApplyPropertyDefault<EOverflow>(&RenderStyle::overflowX, &RenderStyle::setOverflowX, &RenderStyle::initialOverflowX));
    setPropertyHandler(CSSPropertyOverflowY, new ApplyPropertyDefault<EOverflow>(&RenderStyle::overflowY, &RenderStyle::setOverflowY, &RenderStyle::initialOverflowY));
    setPropertyHandler(CSSPropertyOverflow, new ApplyPropertyExpanding<ExpandValue>(propertyHandler(CSSPropertyOverflowX), propertyHandler(CSSPropertyOverflowY)));

    setPropertyHandler(CSSPropertyWebkitColumnRuleColor, new ApplyPropertyColor<NoInheritFromParent>(&RenderStyle::columnRuleColor, &RenderStyle::setColumnRuleColor, &RenderStyle::color));
    setPropertyHandler(CSSPropertyWebkitTextEmphasisColor, new ApplyPropertyColor<NoInheritFromParent>(&RenderStyle::textEmphasisColor, &RenderStyle::setTextEmphasisColor, &RenderStyle::color));
    setPropertyHandler(CSSPropertyWebkitTextFillColor, new ApplyPropertyColor<NoInheritFromParent>(&RenderStyle::textFillColor, &RenderStyle::setTextFillColor, &RenderStyle::color));
    setPropertyHandler(CSSPropertyWebkitTextStrokeColor, new ApplyPropertyColor<NoInheritFromParent>(&RenderStyle::textStrokeColor, &RenderStyle::setTextStrokeColor, &RenderStyle::color));

    setPropertyHandler(CSSPropertyTop, new ApplyPropertyLength<AutoEnabled>(&RenderStyle::top, &RenderStyle::setTop, &RenderStyle::initialOffset));
    setPropertyHandler(CSSPropertyRight, new ApplyPropertyLength<AutoEnabled>(&RenderStyle::right, &RenderStyle::setRight, &RenderStyle::initialOffset));
    setPropertyHandler(CSSPropertyBottom, new ApplyPropertyLength<AutoEnabled>(&RenderStyle::bottom, &RenderStyle::setBottom, &RenderStyle::initialOffset));
    setPropertyHandler(CSSPropertyLeft, new ApplyPropertyLength<AutoEnabled>(&RenderStyle::left, &RenderStyle::setLeft, &RenderStyle::initialOffset));

    setPropertyHandler(CSSPropertyWidth, new ApplyPropertyLength<AutoEnabled, IntrinsicEnabled, MinIntrinsicEnabled, NoneDisabled, UndefinedDisabled, FlexWidth>(&RenderStyle::width, &RenderStyle::setWidth, &RenderStyle::initialSize));
    setPropertyHandler(CSSPropertyHeight, new ApplyPropertyLength<AutoEnabled, IntrinsicEnabled, MinIntrinsicEnabled, NoneDisabled, UndefinedDisabled, FlexHeight>(&RenderStyle::height, &RenderStyle::setHeight, &RenderStyle::initialSize));

    setPropertyHandler(CSSPropertyTextIndent, new ApplyPropertyLength<>(&RenderStyle::textIndent, &RenderStyle::setTextIndent, &RenderStyle::initialTextIndent));

    setPropertyHandler(CSSPropertyMaxHeight, new ApplyPropertyLength<AutoEnabled, IntrinsicEnabled, MinIntrinsicEnabled, NoneEnabled, UndefinedEnabled>(&RenderStyle::maxHeight, &RenderStyle::setMaxHeight, &RenderStyle::initialMaxSize));
    setPropertyHandler(CSSPropertyMaxWidth, new ApplyPropertyLength<AutoEnabled, IntrinsicEnabled, MinIntrinsicEnabled, NoneEnabled, UndefinedEnabled>(&RenderStyle::maxWidth, &RenderStyle::setMaxWidth, &RenderStyle::initialMaxSize));
    setPropertyHandler(CSSPropertyMinHeight, new ApplyPropertyLength<AutoEnabled, IntrinsicEnabled, MinIntrinsicEnabled>(&RenderStyle::minHeight, &RenderStyle::setMinHeight, &RenderStyle::initialMinSize));
    setPropertyHandler(CSSPropertyMinWidth, new ApplyPropertyLength<AutoEnabled, IntrinsicEnabled, MinIntrinsicEnabled>(&RenderStyle::minWidth, &RenderStyle::setMinWidth, &RenderStyle::initialMinSize));

    setPropertyHandler(CSSPropertyMarginTop, new ApplyPropertyLength<AutoEnabled>(&RenderStyle::marginTop, &RenderStyle::setMarginTop, &RenderStyle::initialMargin));
    setPropertyHandler(CSSPropertyMarginRight, new ApplyPropertyLength<AutoEnabled>(&RenderStyle::marginRight, &RenderStyle::setMarginRight, &RenderStyle::initialMargin));
    setPropertyHandler(CSSPropertyMarginBottom, new ApplyPropertyLength<AutoEnabled>(&RenderStyle::marginBottom, &RenderStyle::setMarginBottom, &RenderStyle::initialMargin));
    setPropertyHandler(CSSPropertyMarginLeft, new ApplyPropertyLength<AutoEnabled>(&RenderStyle::marginLeft, &RenderStyle::setMarginLeft, &RenderStyle::initialMargin));
    setPropertyHandler(CSSPropertyMargin, new ApplyPropertyExpanding<SuppressValue>(propertyHandler(CSSPropertyMarginTop), propertyHandler(CSSPropertyMarginRight), propertyHandler(CSSPropertyMarginBottom), propertyHandler(CSSPropertyMarginLeft)));

    setPropertyHandler(CSSPropertyWebkitMarginBeforeCollapse, new ApplyPropertyDefault<EMarginCollapse>(&RenderStyle::marginBeforeCollapse, &RenderStyle::setMarginBeforeCollapse, &RenderStyle::initialMarginBeforeCollapse));
    setPropertyHandler(CSSPropertyWebkitMarginAfterCollapse, new ApplyPropertyDefault<EMarginCollapse>(&RenderStyle::marginAfterCollapse, &RenderStyle::setMarginAfterCollapse, &RenderStyle::initialMarginAfterCollapse));
    setPropertyHandler(CSSPropertyWebkitMarginTopCollapse, CSSPropertyWebkitMarginBeforeCollapse);
    setPropertyHandler(CSSPropertyWebkitMarginBottomCollapse, CSSPropertyWebkitMarginAfterCollapse);
    setPropertyHandler(CSSPropertyWebkitMarginCollapse, new ApplyPropertyExpanding<SuppressValue>(propertyHandler(CSSPropertyWebkitMarginBeforeCollapse), propertyHandler(CSSPropertyWebkitMarginAfterCollapse)));

    setPropertyHandler(CSSPropertyPaddingTop, new ApplyPropertyLength<>(&RenderStyle::paddingTop, &RenderStyle::setPaddingTop, &RenderStyle::initialPadding));
    setPropertyHandler(CSSPropertyPaddingRight, new ApplyPropertyLength<>(&RenderStyle::paddingRight, &RenderStyle::setPaddingRight, &RenderStyle::initialPadding));
    setPropertyHandler(CSSPropertyPaddingBottom, new ApplyPropertyLength<>(&RenderStyle::paddingBottom, &RenderStyle::setPaddingBottom, &RenderStyle::initialPadding));
    setPropertyHandler(CSSPropertyPaddingLeft, new ApplyPropertyLength<>(&RenderStyle::paddingLeft, &RenderStyle::setPaddingLeft, &RenderStyle::initialPadding));
    setPropertyHandler(CSSPropertyPadding, new ApplyPropertyExpanding<SuppressValue>(propertyHandler(CSSPropertyPaddingTop), propertyHandler(CSSPropertyPaddingRight), propertyHandler(CSSPropertyPaddingBottom), propertyHandler(CSSPropertyPaddingLeft)));

    setPropertyHandler(CSSPropertyWebkitPerspectiveOriginX, new ApplyPropertyLength<>(&RenderStyle::perspectiveOriginX, &RenderStyle::setPerspectiveOriginX, &RenderStyle::initialPerspectiveOriginX));
    setPropertyHandler(CSSPropertyWebkitPerspectiveOriginY, new ApplyPropertyLength<>(&RenderStyle::perspectiveOriginY, &RenderStyle::setPerspectiveOriginY, &RenderStyle::initialPerspectiveOriginY));
    setPropertyHandler(CSSPropertyWebkitPerspectiveOrigin, new ApplyPropertyExpanding<SuppressValue>(propertyHandler(CSSPropertyWebkitPerspectiveOriginX), propertyHandler(CSSPropertyWebkitPerspectiveOriginY)));
    setPropertyHandler(CSSPropertyWebkitTransformOriginX, new ApplyPropertyLength<>(&RenderStyle::transformOriginX, &RenderStyle::setTransformOriginX, &RenderStyle::initialTransformOriginX));
    setPropertyHandler(CSSPropertyWebkitTransformOriginY, new ApplyPropertyLength<>(&RenderStyle::transformOriginY, &RenderStyle::setTransformOriginY, &RenderStyle::initialTransformOriginY));
    setPropertyHandler(CSSPropertyWebkitTransformOriginZ, new ApplyPropertyComputeLength<float>(&RenderStyle::transformOriginZ, &RenderStyle::setTransformOriginZ, &RenderStyle::initialTransformOriginZ));
    setPropertyHandler(CSSPropertyWebkitTransformOrigin, new ApplyPropertyExpanding<SuppressValue>(propertyHandler(CSSPropertyWebkitTransformOriginX), propertyHandler(CSSPropertyWebkitTransformOriginY), propertyHandler(CSSPropertyWebkitTransformOriginZ)));

    setPropertyHandler(CSSPropertyWebkitColumnCount, new ApplyPropertyAuto<unsigned short>(&RenderStyle::columnCount, &RenderStyle::setColumnCount, &RenderStyle::hasAutoColumnCount, &RenderStyle::setHasAutoColumnCount));
    setPropertyHandler(CSSPropertyWebkitColumnGap, new ApplyPropertyAuto<float, ComputeLength, CSSValueNormal>(&RenderStyle::columnGap, &RenderStyle::setColumnGap, &RenderStyle::hasNormalColumnGap, &RenderStyle::setHasNormalColumnGap));
    setPropertyHandler(CSSPropertyWebkitColumnWidth, new ApplyPropertyAuto<float, ComputeLength>(&RenderStyle::columnWidth, &RenderStyle::setColumnWidth, &RenderStyle::hasAutoColumnWidth, &RenderStyle::setHasAutoColumnWidth));

    setPropertyHandler(CSSPropertyWebkitTextCombine, new ApplyPropertyDefault<TextCombine>(&RenderStyle::textCombine, &RenderStyle::setTextCombine, &RenderStyle::initialTextCombine));
    setPropertyHandler(CSSPropertyWebkitTextEmphasisPosition, new ApplyPropertyDefault<TextEmphasisPosition>(&RenderStyle::textEmphasisPosition, &RenderStyle::setTextEmphasisPosition, &RenderStyle::initialTextEmphasisPosition));
    setPropertyHandler(CSSPropertyWebkitTextEmphasisStyle, new ApplyPropertyTextEmphasisStyle());

    setPropertyHandler(CSSPropertyZIndex, new ApplyPropertyAuto<int>(&RenderStyle::zIndex, &RenderStyle::setZIndex, &RenderStyle::hasAutoZIndex, &RenderStyle::setHasAutoZIndex));
}


}
