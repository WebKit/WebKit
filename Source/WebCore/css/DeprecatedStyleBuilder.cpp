/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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
#include "DeprecatedStyleBuilder.h"

#include "BasicShapeFunctions.h"
#include "BasicShapes.h"
#include "CSSAspectRatioValue.h"
#include "CSSCalculationValue.h"
#include "CSSCursorImageValue.h"
#include "CSSImageGeneratorValue.h"
#include "CSSImageSetValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSToStyleMap.h"
#include "CSSValueList.h"
#include "ClipPathOperation.h"
#include "CursorList.h"
#include "Document.h"
#include "Frame.h"
#include "HTMLElement.h"
#include "Pair.h"
#include "Rect.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include "Settings.h"
#include "StyleFontSizeFunctions.h"
#include "StyleResolver.h"
#include <wtf/StdLibExtras.h>

#if ENABLE(CSS_SHAPES)
#include "ShapeValue.h"
#endif

namespace WebCore {

using namespace HTMLNames;

enum ExpandValueBehavior {SuppressValue = 0, ExpandValue};
template <ExpandValueBehavior expandValue, CSSPropertyID one = CSSPropertyInvalid, CSSPropertyID two = CSSPropertyInvalid, CSSPropertyID three = CSSPropertyInvalid, CSSPropertyID four = CSSPropertyInvalid, CSSPropertyID five = CSSPropertyInvalid>
class ApplyPropertyExpanding {
public:

    template <CSSPropertyID id>
    static inline void applyInheritValue(CSSPropertyID propertyID, StyleResolver* styleResolver)
    {
        if (id == CSSPropertyInvalid)
            return;

        const DeprecatedStyleBuilder& table = DeprecatedStyleBuilder::sharedStyleBuilder();
        const PropertyHandler& handler = table.propertyHandler(id);
        if (handler.isValid())
            handler.applyInheritValue(propertyID, styleResolver);
    }

    static void applyInheritValue(CSSPropertyID propertyID, StyleResolver* styleResolver)
    {
        applyInheritValue<one>(propertyID, styleResolver);
        applyInheritValue<two>(propertyID, styleResolver);
        applyInheritValue<three>(propertyID, styleResolver);
        applyInheritValue<four>(propertyID, styleResolver);
        applyInheritValue<five>(propertyID, styleResolver);
    }

    template <CSSPropertyID id>
    static inline void applyInitialValue(CSSPropertyID propertyID, StyleResolver* styleResolver)
    {
        if (id == CSSPropertyInvalid)
            return;

        const DeprecatedStyleBuilder& table = DeprecatedStyleBuilder::sharedStyleBuilder();
        const PropertyHandler& handler = table.propertyHandler(id);
        if (handler.isValid())
            handler.applyInitialValue(propertyID, styleResolver);
    }

    static void applyInitialValue(CSSPropertyID propertyID, StyleResolver* styleResolver)
    {
        applyInitialValue<one>(propertyID, styleResolver);
        applyInitialValue<two>(propertyID, styleResolver);
        applyInitialValue<three>(propertyID, styleResolver);
        applyInitialValue<four>(propertyID, styleResolver);
        applyInitialValue<five>(propertyID, styleResolver);
    }

    template <CSSPropertyID id>
    static inline void applyValue(CSSPropertyID propertyID, StyleResolver* styleResolver, CSSValue* value)
    {
        if (id == CSSPropertyInvalid)
            return;

        const DeprecatedStyleBuilder& table = DeprecatedStyleBuilder::sharedStyleBuilder();
        const PropertyHandler& handler = table.propertyHandler(id);
        if (handler.isValid())
            handler.applyValue(propertyID, styleResolver, value);
    }

    static void applyValue(CSSPropertyID propertyID, StyleResolver* styleResolver, CSSValue* value)
    {
        if (!expandValue)
            return;

        applyValue<one>(propertyID, styleResolver, value);
        applyValue<two>(propertyID, styleResolver, value);
        applyValue<three>(propertyID, styleResolver, value);
        applyValue<four>(propertyID, styleResolver, value);
        applyValue<five>(propertyID, styleResolver, value);
    }
    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

template <typename GetterType, GetterType (RenderStyle::*getterFunction)() const, typename SetterType, void (RenderStyle::*setterFunction)(SetterType), typename InitialType, InitialType (*initialFunction)()>
class ApplyPropertyDefaultBase {
public:
    static void setValue(RenderStyle* style, SetterType value) { (style->*setterFunction)(value); }
    static GetterType value(RenderStyle* style) { return (style->*getterFunction)(); }
    static InitialType initial() { return (*initialFunction)(); }
    static void applyInheritValue(CSSPropertyID, StyleResolver* styleResolver) { setValue(styleResolver->style(), value(styleResolver->parentStyle())); }
    static void applyInitialValue(CSSPropertyID, StyleResolver* styleResolver) { setValue(styleResolver->style(), initial()); }
    static void applyValue(CSSPropertyID, StyleResolver*, CSSValue*) { }
    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

enum AutoValueType {Number = 0, ComputeLength};
template <typename T, T (RenderStyle::*getterFunction)() const, void (RenderStyle::*setterFunction)(T), bool (RenderStyle::*hasAutoFunction)() const, void (RenderStyle::*setAutoFunction)(), AutoValueType valueType = Number, int autoIdentity = CSSValueAuto>
class ApplyPropertyAuto {
public:
    static void setValue(RenderStyle* style, T value) { (style->*setterFunction)(value); }
    static T value(RenderStyle* style) { return (style->*getterFunction)(); }
    static bool hasAuto(RenderStyle* style) { return (style->*hasAutoFunction)(); }
    static void setAuto(RenderStyle* style) { (style->*setAutoFunction)(); }

    static void applyInheritValue(CSSPropertyID, StyleResolver* styleResolver)
    {
        if (hasAuto(styleResolver->parentStyle()))
            setAuto(styleResolver->style());
        else
            setValue(styleResolver->style(), value(styleResolver->parentStyle()));
    }

    static void applyInitialValue(CSSPropertyID, StyleResolver* styleResolver) { setAuto(styleResolver->style()); }

    static void applyValue(CSSPropertyID, StyleResolver* styleResolver, CSSValue* value)
    {
        if (!is<CSSPrimitiveValue>(*value))
            return;

        CSSPrimitiveValue& primitiveValue = downcast<CSSPrimitiveValue>(*value);
        if (primitiveValue.getValueID() == autoIdentity)
            setAuto(styleResolver->style());
        else if (valueType == Number)
            setValue(styleResolver->style(), primitiveValue);
        else if (valueType == ComputeLength)
            setValue(styleResolver->style(), primitiveValue.computeLength<T>(styleResolver->state().cssToLengthConversionData()));
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

enum ColorInherit {NoInheritFromParent = 0, InheritFromParent};
Color defaultInitialColor();
Color defaultInitialColor() { return Color(); }
template <ColorInherit inheritColorFromParent,
          Color (RenderStyle::*getterFunction)() const,
          void (RenderStyle::*setterFunction)(const Color&),
          void (RenderStyle::*visitedLinkSetterFunction)(const Color&),
          Color (RenderStyle::*defaultFunction)() const,
          Color (*initialFunction)() = &defaultInitialColor>
class ApplyPropertyColor {
public:
    static void applyInheritValue(CSSPropertyID, StyleResolver* styleResolver)
    {
        // Visited link style can never explicitly inherit from parent visited link style so no separate getters are needed.
        Color color = (styleResolver->parentStyle()->*getterFunction)();
        applyColorValue(styleResolver, color.isValid() ? color : (styleResolver->parentStyle()->*defaultFunction)());
    }

    static void applyInitialValue(CSSPropertyID, StyleResolver* styleResolver)
    {
        applyColorValue(styleResolver, initialFunction());
    }

    static void applyValue(CSSPropertyID propertyID, StyleResolver* styleResolver, CSSValue* value)
    {
        if (!is<CSSPrimitiveValue>(*value))
            return;

        CSSPrimitiveValue& primitiveValue = downcast<CSSPrimitiveValue>(*value);
        if (inheritColorFromParent && primitiveValue.getValueID() == CSSValueCurrentcolor)
            applyInheritValue(propertyID, styleResolver);
        else {
            if (styleResolver->applyPropertyToRegularStyle())
                (styleResolver->style()->*setterFunction)(styleResolver->colorFromPrimitiveValue(&primitiveValue));
            if (styleResolver->applyPropertyToVisitedLinkStyle())
                (styleResolver->style()->*visitedLinkSetterFunction)(styleResolver->colorFromPrimitiveValue(&primitiveValue, /* forVisitedLink */ true));
        }
    }

    static void applyColorValue(StyleResolver* styleResolver, const Color& color)
    {
        if (styleResolver->applyPropertyToRegularStyle())
            (styleResolver->style()->*setterFunction)(color);
        if (styleResolver->applyPropertyToVisitedLinkStyle())
            (styleResolver->style()->*visitedLinkSetterFunction)(color);
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

template <typename T>
struct FillLayerAccessorTypes {
    typedef T Setter;
    typedef T Getter;
    typedef T InitialGetter;
};

template <>
struct FillLayerAccessorTypes<StyleImage*> {
    typedef PassRefPtr<StyleImage> Setter;
    typedef StyleImage* Getter;
    typedef StyleImage* InitialGetter;
};

template<>
struct FillLayerAccessorTypes<Length>
{
    typedef Length Setter;
    typedef const Length& Getter;
    typedef Length InitialGetter;
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
          typename FillLayerAccessorTypes<T>::InitialGetter (*initialFunction)(EFillLayerType),
          void (CSSToStyleMap::*mapFillFunction)(CSSPropertyID, FillLayer*, CSSValue*)>
class ApplyPropertyFillLayer {
public:
    static void applyInheritValue(CSSPropertyID, StyleResolver* styleResolver)
    {
        // Check for no-op before copying anything.
        if (*(styleResolver->parentStyle()->*layersFunction)() == *(styleResolver->style()->*layersFunction)())
            return;

        auto* child = (styleResolver->style()->*accessLayersFunction)();
        FillLayer* previousChild = nullptr;
        for (auto* parent = (styleResolver->parentStyle()->*layersFunction)(); parent && (parent->*testFunction)(); parent = parent->next()) {
            if (!child) {
                previousChild->setNext(std::make_unique<FillLayer>(fillLayerType));
                child = previousChild->next();
            }
            (child->*setFunction)((parent->*getFunction)());
            previousChild = child;
            child = previousChild->next();
        }
        for (; child; child = child->next())
            (child->*clearFunction)();
    }

    static void applyInitialValue(CSSPropertyID, StyleResolver* styleResolver)
    {
        // Check for (single-layer) no-op before clearing anything.
        const FillLayer& layers = *(styleResolver->style()->*layersFunction)();
        if (!layers.next() && (!(layers.*testFunction)() || (layers.*getFunction)() == (*initialFunction)(fillLayerType)))
            return;

        FillLayer* child = (styleResolver->style()->*accessLayersFunction)();
        (child->*setFunction)((*initialFunction)(fillLayerType));
        for (child = child->next(); child; child = child->next())
            (child->*clearFunction)();
    }

    static void applyValue(CSSPropertyID, StyleResolver* styleResolver, CSSValue* value)
    {
        FillLayer* child = (styleResolver->style()->*accessLayersFunction)();
        FillLayer* previousChild = nullptr;
        if (is<CSSValueList>(*value)
#if ENABLE(CSS_IMAGE_SET)
        && !is<CSSImageSetValue>(*value)
#endif
        ) {
            // Walk each value and put it into a layer, creating new layers as needed.
            CSSValueList& valueList = downcast<CSSValueList>(*value);
            for (unsigned i = 0; i < valueList.length(); i++) {
                if (!child) {
                    previousChild->setNext(std::make_unique<FillLayer>(fillLayerType));
                    child = previousChild->next();
                }
                (styleResolver->styleMap()->*mapFillFunction)(propertyId, child, valueList.itemWithoutBoundsCheck(i));
                previousChild = child;
                child = child->next();
            }
        } else {
            (styleResolver->styleMap()->*mapFillFunction)(propertyId, child, value);
            child = child->next();
        }
        for (; child; child = child->next())
            (child->*clearFunction)();
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

template <typename T, T (FontDescription::*getterFunction)() const, void (FontDescription::*setterFunction)(T), T initialValue>
class ApplyPropertyFont {
public:
    static void applyInheritValue(CSSPropertyID, StyleResolver* styleResolver)
    {
        FontDescription fontDescription = styleResolver->fontDescription();
        (fontDescription.*setterFunction)((styleResolver->parentFontDescription().*getterFunction)());
        styleResolver->setFontDescription(fontDescription);
    }

    static void applyInitialValue(CSSPropertyID, StyleResolver* styleResolver)
    {
        FontDescription fontDescription = styleResolver->fontDescription();
        (fontDescription.*setterFunction)(initialValue);
        styleResolver->setFontDescription(fontDescription);
    }

    static void applyValue(CSSPropertyID, StyleResolver* styleResolver, CSSValue* value)
    {
        if (!is<CSSPrimitiveValue>(*value))
            return;
        CSSPrimitiveValue& primitiveValue = downcast<CSSPrimitiveValue>(*value);
        FontDescription fontDescription = styleResolver->fontDescription();
        (fontDescription.*setterFunction)(primitiveValue);
        styleResolver->setFontDescription(fontDescription);
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
    static void applyInheritValue(CSSPropertyID, StyleResolver* styleResolver)
    {
        float size = styleResolver->parentStyle()->fontDescription().specifiedSize();

        if (size < 0)
            return;

        FontDescription fontDescription = styleResolver->style()->fontDescription();
        fontDescription.setKeywordSize(styleResolver->parentStyle()->fontDescription().keywordSize());
        styleResolver->setFontSize(fontDescription, size);
        styleResolver->setFontDescription(fontDescription);
        return;
    }

    static void applyInitialValue(CSSPropertyID, StyleResolver* styleResolver)
    {
        FontDescription fontDescription = styleResolver->style()->fontDescription();
        float size = Style::fontSizeForKeyword(CSSValueMedium, fontDescription.useFixedDefaultSize(), styleResolver->document());

        if (size < 0)
            return;

        fontDescription.setKeywordSize(CSSValueMedium - CSSValueXxSmall + 1);
        styleResolver->setFontSize(fontDescription, size);
        styleResolver->setFontDescription(fontDescription);
        return;
    }

    static float determineRubyTextSizeMultiplier(StyleResolver* styleResolver)
    {
        if (styleResolver->style()->rubyPosition() != RubyPositionInterCharacter)
            return 0.5f;
        
        Element* element = styleResolver->state().element();
        if (element == nullptr)
            return 0.25f;
        
        // FIXME: This hack is to ensure tone marks are the same size as
        // the bopomofo. This code will go away if we make a special renderer
        // for the tone marks eventually.
        for (const Element* currElement = element->parentElement(); currElement; currElement = currElement->parentElement()) {
            if (currElement->hasTagName(rtTag))
                return 1.0f;
        }
        return 0.25f;
    }
    
    static void applyValue(CSSPropertyID, StyleResolver* styleResolver, CSSValue* value)
    {
        if (!is<CSSPrimitiveValue>(*value))
            return;

        CSSPrimitiveValue& primitiveValue = downcast<CSSPrimitiveValue>(*value);

        FontDescription fontDescription = styleResolver->style()->fontDescription();
        fontDescription.setKeywordSize(0);
        float parentSize = 0;
        bool parentIsAbsoluteSize = false;
        float size = 0;

        if (styleResolver->parentStyle()) {
            parentSize = styleResolver->parentStyle()->fontDescription().specifiedSize();
            parentIsAbsoluteSize = styleResolver->parentStyle()->fontDescription().isAbsoluteSize();
        }

        if (CSSValueID ident = primitiveValue.getValueID()) {
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
                size = Style::fontSizeForKeyword(ident, fontDescription.useFixedDefaultSize(), styleResolver->document());
                fontDescription.setKeywordSize(ident - CSSValueXxSmall + 1);
                break;
            case CSSValueLarger:
                size = largerFontSize(parentSize);
                break;
            case CSSValueSmaller:
                size = smallerFontSize(parentSize);
                break;
            case CSSValueWebkitRubyText: {
                float rubyTextSizeMultiplier = determineRubyTextSizeMultiplier(styleResolver);
                size = rubyTextSizeMultiplier * parentSize;
                break;
            } default:
                return;
            }

            fontDescription.setIsAbsoluteSize(parentIsAbsoluteSize && (ident == CSSValueLarger || ident == CSSValueSmaller || ident == CSSValueWebkitRubyText));
        } else {
            fontDescription.setIsAbsoluteSize(parentIsAbsoluteSize || !(primitiveValue.isPercentage() || primitiveValue.isFontRelativeLength()));
            if (primitiveValue.isLength()) {
                size = primitiveValue.computeLength<float>(CSSToLengthConversionData(styleResolver->parentStyle(), styleResolver->rootElementStyle(), styleResolver->document().renderView(), 1.0f, true));
                styleResolver->state().setFontSizeHasViewportUnits(primitiveValue.isViewportPercentageLength());
            } else if (primitiveValue.isPercentage())
                size = (primitiveValue.getFloatValue() * parentSize) / 100.0f;
            else if (primitiveValue.isCalculatedPercentageWithLength()) {
                Ref<CalculationValue> calculationValue { primitiveValue.cssCalcValue()->createCalculationValue(styleResolver->state().cssToLengthConversionData().copyWithAdjustedZoom(1.0f)) };
                size = calculationValue->evaluate(parentSize);
            } else
                return;
        }

        if (size < 0)
            return;

        // Overly large font sizes will cause crashes on some platforms (such as Windows).
        // Cap font size here to make sure that doesn't happen.
        size = std::min(maximumAllowedFontSize, size);

        styleResolver->setFontSize(fontDescription, size);
        styleResolver->setFontDescription(fontDescription);
        return;
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

class ApplyPropertyFontWeight {
public:
    static void applyValue(CSSPropertyID, StyleResolver* styleResolver, CSSValue* value)
    {
        if (!is<CSSPrimitiveValue>(*value))
            return;
        CSSPrimitiveValue& primitiveValue = downcast<CSSPrimitiveValue>(*value);
        FontDescription fontDescription = styleResolver->fontDescription();
        switch (primitiveValue.getValueID()) {
        case CSSValueInvalid:
            ASSERT_NOT_REACHED();
            break;
        case CSSValueBolder:
            fontDescription.setWeight(styleResolver->parentStyle()->fontDescription().weight());
            fontDescription.setWeight(fontDescription.bolderWeight());
            break;
        case CSSValueLighter:
            fontDescription.setWeight(styleResolver->parentStyle()->fontDescription().weight());
            fontDescription.setWeight(fontDescription.lighterWeight());
            break;
        default:
            fontDescription.setWeight(primitiveValue);
        }
        styleResolver->setFontDescription(fontDescription);
    }
    static PropertyHandler createHandler()
    {
        PropertyHandler handler = ApplyPropertyFont<FontWeight, &FontDescription::weight, &FontDescription::setWeight, FontWeightNormal>::createHandler();
        return PropertyHandler(handler.inheritFunction(), handler.initialFunction(), &applyValue);
    }
};

class ApplyPropertyFontVariantLigatures {
public:
    static void applyInheritValue(CSSPropertyID, StyleResolver* styleResolver)
    {
        const FontDescription& parentFontDescription = styleResolver->parentFontDescription();
        FontDescription fontDescription = styleResolver->fontDescription();

        fontDescription.setCommonLigaturesState(parentFontDescription.commonLigaturesState());
        fontDescription.setDiscretionaryLigaturesState(parentFontDescription.discretionaryLigaturesState());
        fontDescription.setHistoricalLigaturesState(parentFontDescription.historicalLigaturesState());

        styleResolver->setFontDescription(fontDescription);
    }

    static void applyInitialValue(CSSPropertyID, StyleResolver* styleResolver)
    {
        FontDescription fontDescription = styleResolver->fontDescription();

        fontDescription.setCommonLigaturesState(FontDescription::NormalLigaturesState);
        fontDescription.setDiscretionaryLigaturesState(FontDescription::NormalLigaturesState);
        fontDescription.setHistoricalLigaturesState(FontDescription::NormalLigaturesState);

        styleResolver->setFontDescription(fontDescription);
    }

    static void applyValue(CSSPropertyID, StyleResolver* styleResolver, CSSValue* value)
    {
        FontDescription::LigaturesState commonLigaturesState = FontDescription::NormalLigaturesState;
        FontDescription::LigaturesState discretionaryLigaturesState = FontDescription::NormalLigaturesState;
        FontDescription::LigaturesState historicalLigaturesState = FontDescription::NormalLigaturesState;

        if (is<CSSValueList>(*value)) {
            CSSValueList& valueList = downcast<CSSValueList>(*value);
            for (size_t i = 0; i < valueList.length(); ++i) {
                CSSValue* item = valueList.itemWithoutBoundsCheck(i);
                if (is<CSSPrimitiveValue>(*item)) {
                    switch (downcast<CSSPrimitiveValue>(*item).getValueID()) {
                    case CSSValueNoCommonLigatures:
                        commonLigaturesState = FontDescription::DisabledLigaturesState;
                        break;
                    case CSSValueCommonLigatures:
                        commonLigaturesState = FontDescription::EnabledLigaturesState;
                        break;
                    case CSSValueNoDiscretionaryLigatures:
                        discretionaryLigaturesState = FontDescription::DisabledLigaturesState;
                        break;
                    case CSSValueDiscretionaryLigatures:
                        discretionaryLigaturesState = FontDescription::EnabledLigaturesState;
                        break;
                    case CSSValueNoHistoricalLigatures:
                        historicalLigaturesState = FontDescription::DisabledLigaturesState;
                        break;
                    case CSSValueHistoricalLigatures:
                        historicalLigaturesState = FontDescription::EnabledLigaturesState;
                        break;
                    default:
                        ASSERT_NOT_REACHED();
                        break;
                    }
                }
            }
        }
#if !ASSERT_DISABLED
        else
            ASSERT(downcast<CSSPrimitiveValue>(*value).getValueID() == CSSValueNormal);
#endif

        FontDescription fontDescription = styleResolver->fontDescription();
        fontDescription.setCommonLigaturesState(commonLigaturesState);
        fontDescription.setDiscretionaryLigaturesState(discretionaryLigaturesState);
        fontDescription.setHistoricalLigaturesState(historicalLigaturesState);
        styleResolver->setFontDescription(fontDescription);
    }

    static PropertyHandler createHandler()
    {
        return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue);
    }
};

template <typename T,
          T (Animation::*getterFunction)() const,
          void (Animation::*setterFunction)(T),
          bool (Animation::*testFunction)() const,
          void (Animation::*clearFunction)(),
          T (*initialFunction)(),
          void (CSSToStyleMap::*mapFunction)(Animation*, CSSValue&),
          AnimationList* (RenderStyle::*animationGetterFunction)(),
          const AnimationList* (RenderStyle::*immutableAnimationGetterFunction)() const>
class ApplyPropertyAnimation {
public:
    static void setValue(Animation& animation, T value) { (animation.*setterFunction)(value); }
    static T value(const Animation& animation) { return (animation.*getterFunction)(); }
    static bool test(const Animation& animation) { return (animation.*testFunction)(); }
    static void clear(Animation& animation) { (animation.*clearFunction)(); }
    static T initial() { return (*initialFunction)(); }
    static void map(StyleResolver* styleResolver, Animation& animation, CSSValue& value) { (styleResolver->styleMap()->*mapFunction)(&animation, value); }
    static AnimationList* accessAnimations(RenderStyle* style) { return (style->*animationGetterFunction)(); }
    static const AnimationList* animations(RenderStyle* style) { return (style->*immutableAnimationGetterFunction)(); }

    static void applyInheritValue(CSSPropertyID, StyleResolver* styleResolver)
    {
        AnimationList* list = accessAnimations(styleResolver->style());
        const AnimationList* parentList = animations(styleResolver->parentStyle());
        size_t i = 0, parentSize = parentList ? parentList->size() : 0;
        for ( ; i < parentSize && test(parentList->animation(i)); ++i) {
            if (list->size() <= i)
                list->append(Animation::create());
            setValue(list->animation(i), value(parentList->animation(i)));
            list->animation(i).setAnimationMode(parentList->animation(i).animationMode());
        }

        /* Reset any remaining animations to not have the property set. */
        for ( ; i < list->size(); ++i)
            clear(list->animation(i));
    }

    static void applyInitialValue(CSSPropertyID propertyID, StyleResolver* styleResolver)
    {
        AnimationList* list = accessAnimations(styleResolver->style());
        if (list->isEmpty())
            list->append(Animation::create());
        setValue(list->animation(0), initial());
        if (propertyID == CSSPropertyWebkitTransitionProperty)
            list->animation(0).setAnimationMode(Animation::AnimateAll);
        for (size_t i = 1; i < list->size(); ++i)
            clear(list->animation(i));
    }

    static void applyValue(CSSPropertyID, StyleResolver* styleResolver, CSSValue* value)
    {
        AnimationList* list = accessAnimations(styleResolver->style());
        size_t childIndex = 0;
        if (is<CSSValueList>(*value)) {
            /* Walk each value and put it into an animation, creating new animations as needed. */
            for (auto& currentValue : downcast<CSSValueList>(*value)) {
                if (childIndex <= list->size())
                    list->append(Animation::create());
                map(styleResolver, list->animation(childIndex), currentValue);
                ++childIndex;
            }
        } else {
            if (list->isEmpty())
                list->append(Animation::create());
            map(styleResolver, list->animation(childIndex), *value);
            childIndex = 1;
        }
        for ( ; childIndex < list->size(); ++childIndex) {
            /* Reset all remaining animations to not have the property set. */
            clear(list->animation(childIndex));
        }
    }

    static PropertyHandler createHandler() { return PropertyHandler(&applyInheritValue, &applyInitialValue, &applyValue); }
};

const DeprecatedStyleBuilder& DeprecatedStyleBuilder::sharedStyleBuilder()
{
    static NeverDestroyed<DeprecatedStyleBuilder> styleBuilderInstance;
    return styleBuilderInstance;
}

DeprecatedStyleBuilder::DeprecatedStyleBuilder()
{
    for (int i = 0; i < numCSSProperties; ++i)
        m_propertyMap[i] = PropertyHandler();

    // Please keep CSS property list in alphabetical order.
    setPropertyHandler(CSSPropertyBackgroundAttachment, ApplyPropertyFillLayer<EFillAttachment, CSSPropertyBackgroundAttachment, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers, &FillLayer::isAttachmentSet, &FillLayer::attachment, &FillLayer::setAttachment, &FillLayer::clearAttachment, &FillLayer::initialFillAttachment, &CSSToStyleMap::mapFillAttachment>::createHandler());
    setPropertyHandler(CSSPropertyBackgroundBlendMode, ApplyPropertyFillLayer<BlendMode, CSSPropertyBackgroundBlendMode, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers, &FillLayer::isBlendModeSet, &FillLayer::blendMode, &FillLayer::setBlendMode, &FillLayer::clearBlendMode, &FillLayer::initialFillBlendMode, &CSSToStyleMap::mapFillBlendMode>::createHandler());
    setPropertyHandler(CSSPropertyBackgroundClip, ApplyPropertyFillLayer<EFillBox, CSSPropertyBackgroundClip, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers, &FillLayer::isClipSet, &FillLayer::clip, &FillLayer::setClip, &FillLayer::clearClip, &FillLayer::initialFillClip, &CSSToStyleMap::mapFillClip>::createHandler());
    setPropertyHandler(CSSPropertyBackgroundColor, ApplyPropertyColor<NoInheritFromParent, &RenderStyle::backgroundColor, &RenderStyle::setBackgroundColor, &RenderStyle::setVisitedLinkBackgroundColor, &RenderStyle::invalidColor>::createHandler());
    setPropertyHandler(CSSPropertyBackgroundImage, ApplyPropertyFillLayer<StyleImage*, CSSPropertyBackgroundImage, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers, &FillLayer::isImageSet, &FillLayer::image, &FillLayer::setImage, &FillLayer::clearImage, &FillLayer::initialFillImage, &CSSToStyleMap::mapFillImage>::createHandler());
    setPropertyHandler(CSSPropertyBackgroundOrigin, ApplyPropertyFillLayer<EFillBox, CSSPropertyBackgroundOrigin, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers, &FillLayer::isOriginSet, &FillLayer::origin, &FillLayer::setOrigin, &FillLayer::clearOrigin, &FillLayer::initialFillOrigin, &CSSToStyleMap::mapFillOrigin>::createHandler());
    setPropertyHandler(CSSPropertyBackgroundPositionX, ApplyPropertyFillLayer<Length, CSSPropertyBackgroundPositionX, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers, &FillLayer::isXPositionSet, &FillLayer::xPosition, &FillLayer::setXPosition, &FillLayer::clearXPosition, &FillLayer::initialFillXPosition, &CSSToStyleMap::mapFillXPosition>::createHandler());
    setPropertyHandler(CSSPropertyBackgroundPositionY, ApplyPropertyFillLayer<Length, CSSPropertyBackgroundPositionY, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers, &FillLayer::isYPositionSet, &FillLayer::yPosition, &FillLayer::setYPosition, &FillLayer::clearYPosition, &FillLayer::initialFillYPosition, &CSSToStyleMap::mapFillYPosition>::createHandler());
    setPropertyHandler(CSSPropertyBackgroundRepeatX, ApplyPropertyFillLayer<EFillRepeat, CSSPropertyBackgroundRepeatX, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers, &FillLayer::isRepeatXSet, &FillLayer::repeatX, &FillLayer::setRepeatX, &FillLayer::clearRepeatX, &FillLayer::initialFillRepeatX, &CSSToStyleMap::mapFillRepeatX>::createHandler());
    setPropertyHandler(CSSPropertyBackgroundRepeatY, ApplyPropertyFillLayer<EFillRepeat, CSSPropertyBackgroundRepeatY, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers, &FillLayer::isRepeatYSet, &FillLayer::repeatY, &FillLayer::setRepeatY, &FillLayer::clearRepeatY, &FillLayer::initialFillRepeatY, &CSSToStyleMap::mapFillRepeatY>::createHandler());
    setPropertyHandler(CSSPropertyBackgroundSize, ApplyPropertyFillLayer<FillSize, CSSPropertyBackgroundSize, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers, &FillLayer::isSizeSet, &FillLayer::size, &FillLayer::setSize, &FillLayer::clearSize, &FillLayer::initialFillSize, &CSSToStyleMap::mapFillSize>::createHandler());
    setPropertyHandler(CSSPropertyBorderBottomColor, ApplyPropertyColor<NoInheritFromParent, &RenderStyle::borderBottomColor, &RenderStyle::setBorderBottomColor, &RenderStyle::setVisitedLinkBorderBottomColor, &RenderStyle::color>::createHandler());
    setPropertyHandler(CSSPropertyBorderLeftColor, ApplyPropertyColor<NoInheritFromParent, &RenderStyle::borderLeftColor, &RenderStyle::setBorderLeftColor, &RenderStyle::setVisitedLinkBorderLeftColor, &RenderStyle::color>::createHandler());
    setPropertyHandler(CSSPropertyBorderRightColor, ApplyPropertyColor<NoInheritFromParent, &RenderStyle::borderRightColor, &RenderStyle::setBorderRightColor, &RenderStyle::setVisitedLinkBorderRightColor, &RenderStyle::color>::createHandler());
    setPropertyHandler(CSSPropertyBorderTopColor, ApplyPropertyColor<NoInheritFromParent, &RenderStyle::borderTopColor, &RenderStyle::setBorderTopColor, &RenderStyle::setVisitedLinkBorderTopColor, &RenderStyle::color>::createHandler());
    setPropertyHandler(CSSPropertyColor, ApplyPropertyColor<InheritFromParent, &RenderStyle::color, &RenderStyle::setColor, &RenderStyle::setVisitedLinkColor, &RenderStyle::invalidColor, RenderStyle::initialColor>::createHandler());
    setPropertyHandler(CSSPropertyFontSize, ApplyPropertyFontSize::createHandler());
    setPropertyHandler(CSSPropertyFontStyle, ApplyPropertyFont<FontItalic, &FontDescription::italic, &FontDescription::setItalic, FontItalicOff>::createHandler());
    setPropertyHandler(CSSPropertyFontVariant, ApplyPropertyFont<FontSmallCaps, &FontDescription::smallCaps, &FontDescription::setSmallCaps, FontSmallCapsOff>::createHandler());
    setPropertyHandler(CSSPropertyFontWeight, ApplyPropertyFontWeight::createHandler());
    setPropertyHandler(CSSPropertyOrphans, ApplyPropertyAuto<short, &RenderStyle::orphans, &RenderStyle::setOrphans, &RenderStyle::hasAutoOrphans, &RenderStyle::setHasAutoOrphans>::createHandler());
    setPropertyHandler(CSSPropertyOutlineColor, ApplyPropertyColor<NoInheritFromParent, &RenderStyle::outlineColor, &RenderStyle::setOutlineColor, &RenderStyle::setVisitedLinkOutlineColor, &RenderStyle::color>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTextDecorationColor, ApplyPropertyColor<NoInheritFromParent, &RenderStyle::textDecorationColor, &RenderStyle::setTextDecorationColor, &RenderStyle::setVisitedLinkTextDecorationColor, &RenderStyle::color>::createHandler());
    setPropertyHandler(CSSPropertyTextRendering, ApplyPropertyFont<TextRenderingMode, &FontDescription::textRenderingMode, &FontDescription::setTextRenderingMode, AutoTextRendering>::createHandler());

    setPropertyHandler(CSSPropertyAnimationDelay, ApplyPropertyAnimation<double, &Animation::delay, &Animation::setDelay, &Animation::isDelaySet, &Animation::clearDelay, &Animation::initialAnimationDelay, &CSSToStyleMap::mapAnimationDelay, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyAnimationDirection, ApplyPropertyAnimation<Animation::AnimationDirection, &Animation::direction, &Animation::setDirection, &Animation::isDirectionSet, &Animation::clearDirection, &Animation::initialAnimationDirection, &CSSToStyleMap::mapAnimationDirection, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyAnimationDuration, ApplyPropertyAnimation<double, &Animation::duration, &Animation::setDuration, &Animation::isDurationSet, &Animation::clearDuration, &Animation::initialAnimationDuration, &CSSToStyleMap::mapAnimationDuration, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyAnimationFillMode, ApplyPropertyAnimation<unsigned, &Animation::fillMode, &Animation::setFillMode, &Animation::isFillModeSet, &Animation::clearFillMode, &Animation::initialAnimationFillMode, &CSSToStyleMap::mapAnimationFillMode, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyAnimationIterationCount, ApplyPropertyAnimation<double, &Animation::iterationCount, &Animation::setIterationCount, &Animation::isIterationCountSet, &Animation::clearIterationCount, &Animation::initialAnimationIterationCount, &CSSToStyleMap::mapAnimationIterationCount, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyAnimationName, ApplyPropertyAnimation<const String&, &Animation::name, &Animation::setName, &Animation::isNameSet, &Animation::clearName, &Animation::initialAnimationName, &CSSToStyleMap::mapAnimationName, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyAnimationPlayState, ApplyPropertyAnimation<EAnimPlayState, &Animation::playState, &Animation::setPlayState, &Animation::isPlayStateSet, &Animation::clearPlayState, &Animation::initialAnimationPlayState, &CSSToStyleMap::mapAnimationPlayState, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyAnimationTimingFunction, ApplyPropertyAnimation<const PassRefPtr<TimingFunction>, &Animation::timingFunction, &Animation::setTimingFunction, &Animation::isTimingFunctionSet, &Animation::clearTimingFunction, &Animation::initialAnimationTimingFunction, &CSSToStyleMap::mapAnimationTimingFunction, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());

    setPropertyHandler(CSSPropertyWebkitAnimationDelay, ApplyPropertyAnimation<double, &Animation::delay, &Animation::setDelay, &Animation::isDelaySet, &Animation::clearDelay, &Animation::initialAnimationDelay, &CSSToStyleMap::mapAnimationDelay, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyWebkitAnimationDirection, ApplyPropertyAnimation<Animation::AnimationDirection, &Animation::direction, &Animation::setDirection, &Animation::isDirectionSet, &Animation::clearDirection, &Animation::initialAnimationDirection, &CSSToStyleMap::mapAnimationDirection, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyWebkitAnimationDuration, ApplyPropertyAnimation<double, &Animation::duration, &Animation::setDuration, &Animation::isDurationSet, &Animation::clearDuration, &Animation::initialAnimationDuration, &CSSToStyleMap::mapAnimationDuration, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyWebkitAnimationFillMode, ApplyPropertyAnimation<unsigned, &Animation::fillMode, &Animation::setFillMode, &Animation::isFillModeSet, &Animation::clearFillMode, &Animation::initialAnimationFillMode, &CSSToStyleMap::mapAnimationFillMode, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyWebkitAnimationIterationCount, ApplyPropertyAnimation<double, &Animation::iterationCount, &Animation::setIterationCount, &Animation::isIterationCountSet, &Animation::clearIterationCount, &Animation::initialAnimationIterationCount, &CSSToStyleMap::mapAnimationIterationCount, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyWebkitAnimationName, ApplyPropertyAnimation<const String&, &Animation::name, &Animation::setName, &Animation::isNameSet, &Animation::clearName, &Animation::initialAnimationName, &CSSToStyleMap::mapAnimationName, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyWebkitAnimationPlayState, ApplyPropertyAnimation<EAnimPlayState, &Animation::playState, &Animation::setPlayState, &Animation::isPlayStateSet, &Animation::clearPlayState, &Animation::initialAnimationPlayState, &CSSToStyleMap::mapAnimationPlayState, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyWebkitAnimationTimingFunction, ApplyPropertyAnimation<const PassRefPtr<TimingFunction>, &Animation::timingFunction, &Animation::setTimingFunction, &Animation::isTimingFunctionSet, &Animation::clearTimingFunction, &Animation::initialAnimationTimingFunction, &CSSToStyleMap::mapAnimationTimingFunction, &RenderStyle::accessAnimations, &RenderStyle::animations>::createHandler());
    setPropertyHandler(CSSPropertyWebkitBackgroundClip, CSSPropertyBackgroundClip);
    setPropertyHandler(CSSPropertyWebkitBackgroundComposite, ApplyPropertyFillLayer<CompositeOperator, CSSPropertyWebkitBackgroundComposite, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers, &FillLayer::isCompositeSet, &FillLayer::composite, &FillLayer::setComposite, &FillLayer::clearComposite, &FillLayer::initialFillComposite, &CSSToStyleMap::mapFillComposite>::createHandler());
    setPropertyHandler(CSSPropertyWebkitBackgroundOrigin, CSSPropertyBackgroundOrigin);
    setPropertyHandler(CSSPropertyWebkitBackgroundSize, CSSPropertyBackgroundSize);
    setPropertyHandler(CSSPropertyColumnCount, ApplyPropertyAuto<unsigned short, &RenderStyle::columnCount, &RenderStyle::setColumnCount, &RenderStyle::hasAutoColumnCount, &RenderStyle::setHasAutoColumnCount>::createHandler());
    setPropertyHandler(CSSPropertyColumnGap, ApplyPropertyAuto<float, &RenderStyle::columnGap, &RenderStyle::setColumnGap, &RenderStyle::hasNormalColumnGap, &RenderStyle::setHasNormalColumnGap, ComputeLength, CSSValueNormal>::createHandler());
    setPropertyHandler(CSSPropertyColumnRuleColor, ApplyPropertyColor<NoInheritFromParent, &RenderStyle::columnRuleColor, &RenderStyle::setColumnRuleColor, &RenderStyle::setVisitedLinkColumnRuleColor, &RenderStyle::color>::createHandler());
    setPropertyHandler(CSSPropertyColumnWidth, ApplyPropertyAuto<float, &RenderStyle::columnWidth, &RenderStyle::setColumnWidth, &RenderStyle::hasAutoColumnWidth, &RenderStyle::setHasAutoColumnWidth, ComputeLength>::createHandler());
    setPropertyHandler(CSSPropertyWebkitFontKerning, ApplyPropertyFont<FontDescription::Kerning, &FontDescription::kerning, &FontDescription::setKerning, FontDescription::AutoKerning>::createHandler());
    setPropertyHandler(CSSPropertyWebkitFontSmoothing, ApplyPropertyFont<FontSmoothingMode, &FontDescription::fontSmoothing, &FontDescription::setFontSmoothing, AutoSmoothing>::createHandler());
    setPropertyHandler(CSSPropertyWebkitFontVariantLigatures, ApplyPropertyFontVariantLigatures::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskClip, ApplyPropertyFillLayer<EFillBox, CSSPropertyWebkitMaskClip, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers, &FillLayer::isClipSet, &FillLayer::clip, &FillLayer::setClip, &FillLayer::clearClip, &FillLayer::initialFillClip, &CSSToStyleMap::mapFillClip>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskComposite, ApplyPropertyFillLayer<CompositeOperator, CSSPropertyWebkitMaskComposite, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers, &FillLayer::isCompositeSet, &FillLayer::composite, &FillLayer::setComposite, &FillLayer::clearComposite, &FillLayer::initialFillComposite, &CSSToStyleMap::mapFillComposite>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskOrigin, ApplyPropertyFillLayer<EFillBox, CSSPropertyWebkitMaskOrigin, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers, &FillLayer::isOriginSet, &FillLayer::origin, &FillLayer::setOrigin, &FillLayer::clearOrigin, &FillLayer::initialFillOrigin, &CSSToStyleMap::mapFillOrigin>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskPositionX, ApplyPropertyFillLayer<Length, CSSPropertyWebkitMaskPositionX, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers, &FillLayer::isXPositionSet, &FillLayer::xPosition, &FillLayer::setXPosition, &FillLayer::clearXPosition, &FillLayer::initialFillXPosition, &CSSToStyleMap::mapFillXPosition>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskPositionY, ApplyPropertyFillLayer<Length, CSSPropertyWebkitMaskPositionY, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers, &FillLayer::isYPositionSet, &FillLayer::yPosition, &FillLayer::setYPosition, &FillLayer::clearYPosition, &FillLayer::initialFillYPosition, &CSSToStyleMap::mapFillYPosition>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskRepeatX, ApplyPropertyFillLayer<EFillRepeat, CSSPropertyWebkitMaskRepeatX, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers, &FillLayer::isRepeatXSet, &FillLayer::repeatX, &FillLayer::setRepeatX, &FillLayer::clearRepeatX, &FillLayer::initialFillRepeatX, &CSSToStyleMap::mapFillRepeatX>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskRepeatY, ApplyPropertyFillLayer<EFillRepeat, CSSPropertyWebkitMaskRepeatY, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers, &FillLayer::isRepeatYSet, &FillLayer::repeatY, &FillLayer::setRepeatY, &FillLayer::clearRepeatY, &FillLayer::initialFillRepeatY, &CSSToStyleMap::mapFillRepeatY>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskSize, ApplyPropertyFillLayer<FillSize, CSSPropertyWebkitMaskSize, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers, &FillLayer::isSizeSet, &FillLayer::size, &FillLayer::setSize, &FillLayer::clearSize, &FillLayer::initialFillSize, &CSSToStyleMap::mapFillSize>::createHandler());
    setPropertyHandler(CSSPropertyWebkitMaskSourceType, ApplyPropertyFillLayer<EMaskSourceType, CSSPropertyWebkitMaskSourceType, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers, &FillLayer::isMaskSourceTypeSet, &FillLayer::maskSourceType, &FillLayer::setMaskSourceType, &FillLayer::clearMaskSourceType, &FillLayer::initialMaskSourceType, &CSSToStyleMap::mapFillMaskSourceType>::createHandler());
    setPropertyHandler(CSSPropertyWebkitPerspectiveOrigin, ApplyPropertyExpanding<SuppressValue, CSSPropertyWebkitPerspectiveOriginX, CSSPropertyWebkitPerspectiveOriginY>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTextEmphasisColor, ApplyPropertyColor<NoInheritFromParent, &RenderStyle::textEmphasisColor, &RenderStyle::setTextEmphasisColor, &RenderStyle::setVisitedLinkTextEmphasisColor, &RenderStyle::color>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTextFillColor, ApplyPropertyColor<NoInheritFromParent, &RenderStyle::textFillColor, &RenderStyle::setTextFillColor, &RenderStyle::setVisitedLinkTextFillColor, &RenderStyle::color>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTextStrokeColor, ApplyPropertyColor<NoInheritFromParent, &RenderStyle::textStrokeColor, &RenderStyle::setTextStrokeColor, &RenderStyle::setVisitedLinkTextStrokeColor, &RenderStyle::color>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTransitionDelay, ApplyPropertyAnimation<double, &Animation::delay, &Animation::setDelay, &Animation::isDelaySet, &Animation::clearDelay, &Animation::initialAnimationDelay, &CSSToStyleMap::mapAnimationDelay, &RenderStyle::accessTransitions, &RenderStyle::transitions>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTransitionDuration, ApplyPropertyAnimation<double, &Animation::duration, &Animation::setDuration, &Animation::isDurationSet, &Animation::clearDuration, &Animation::initialAnimationDuration, &CSSToStyleMap::mapAnimationDuration, &RenderStyle::accessTransitions, &RenderStyle::transitions>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTransitionProperty, ApplyPropertyAnimation<CSSPropertyID, &Animation::property, &Animation::setProperty, &Animation::isPropertySet, &Animation::clearProperty, &Animation::initialAnimationProperty, &CSSToStyleMap::mapAnimationProperty, &RenderStyle::accessTransitions, &RenderStyle::transitions>::createHandler());
    setPropertyHandler(CSSPropertyWebkitTransitionTimingFunction, ApplyPropertyAnimation<const PassRefPtr<TimingFunction>, &Animation::timingFunction, &Animation::setTimingFunction, &Animation::isTimingFunctionSet, &Animation::clearTimingFunction, &Animation::initialAnimationTimingFunction, &CSSToStyleMap::mapAnimationTimingFunction, &RenderStyle::accessTransitions, &RenderStyle::transitions>::createHandler());
    setPropertyHandler(CSSPropertyWidows, ApplyPropertyAuto<short, &RenderStyle::widows, &RenderStyle::setWidows, &RenderStyle::hasAutoWidows, &RenderStyle::setHasAutoWidows>::createHandler());

    setPropertyHandler(CSSPropertyZIndex, ApplyPropertyAuto<int, &RenderStyle::zIndex, &RenderStyle::setZIndex, &RenderStyle::hasAutoZIndex, &RenderStyle::setHasAutoZIndex>::createHandler());
}


}
