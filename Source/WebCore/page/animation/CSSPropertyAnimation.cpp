/*
 * Copyright (C) 2007, 2008, 2009, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2012, 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSPropertyAnimation.h"

#include "AnimationBase.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSCrossfadeValue.h"
#include "CSSFilterImageValue.h"
#include "CSSImageGeneratorValue.h"
#include "CSSImageValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "CachedImage.h"
#include "ClipPathOperation.h"
#include "FloatConversion.h"
#include "IdentityTransformOperation.h"
#include "Logging.h"
#include "Matrix3DTransformOperation.h"
#include "MatrixTransformOperation.h"
#include "RenderBox.h"
#include "RenderStyle.h"
#include "StyleCachedImage.h"
#include "StyleGeneratedImage.h"
#include "StylePropertyShorthand.h"
#include "StyleResolver.h"
#include "TextStream.h"
#include <algorithm>
#include <memory>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/PointerComparison.h>
#include <wtf/RefCounted.h>

namespace WebCore {

static inline int blendFunc(const AnimationBase*, int from, int to, double progress)
{
    return blend(from, to, progress);
}

static inline double blendFunc(const AnimationBase*, double from, double to, double progress)
{
    return blend(from, to, progress);
}

static inline float blendFunc(const AnimationBase*, float from, float to, double progress)
{
    return narrowPrecisionToFloat(from + (to - from) * progress);
}

static inline Color blendFunc(const AnimationBase*, const Color& from, const Color& to, double progress)
{
    return blend(from, to, progress);
}

static inline Length blendFunc(const AnimationBase*, const Length& from, const Length& to, double progress)
{
    return blend(from, to, progress);
}

static inline LengthSize blendFunc(const AnimationBase* anim, const LengthSize& from, const LengthSize& to, double progress)
{
    return LengthSize(blendFunc(anim, from.width(), to.width(), progress),
                      blendFunc(anim, from.height(), to.height(), progress));
}

static inline ShadowStyle blendFunc(const AnimationBase* anim, ShadowStyle from, ShadowStyle to, double progress)
{
    if (from == to)
        return to;

    double fromVal = from == Normal ? 1 : 0;
    double toVal = to == Normal ? 1 : 0;
    double result = blendFunc(anim, fromVal, toVal, progress);
    return result > 0 ? Normal : Inset;
}

static inline std::unique_ptr<ShadowData> blendFunc(const AnimationBase* anim, const ShadowData* from, const ShadowData* to, double progress)
{
    ASSERT(from && to);
    if (from->style() != to->style())
        return std::make_unique<ShadowData>(*to);

    return std::make_unique<ShadowData>(blend(from->location(), to->location(), progress),
        blend(from->radius(), to->radius(), progress),
        blend(from->spread(), to->spread(), progress),
        blendFunc(anim, from->style(), to->style(), progress),
        from->isWebkitBoxShadow(),
        blend(from->color(), to->color(), progress));
}

static inline TransformOperations blendFunc(const AnimationBase* animation, const TransformOperations& from, const TransformOperations& to, double progress)
{
    if (animation->transformFunctionListsMatch())
        return to.blendByMatchingOperations(from, progress);
    return to.blendByUsingMatrixInterpolation(from, progress, is<RenderBox>(*animation->renderer()) ? downcast<RenderBox>(*animation->renderer()).borderBoxRect().size() : LayoutSize());
}

static inline PassRefPtr<ClipPathOperation> blendFunc(const AnimationBase*, ClipPathOperation* from, ClipPathOperation* to, double progress)
{
    if (!from || !to)
        return to;

    // Other clip-path operations than BasicShapes can not be animated.
    if (from->type() != ClipPathOperation::Shape || to->type() != ClipPathOperation::Shape)
        return to;

    const BasicShape& fromShape = downcast<ShapeClipPathOperation>(*from).basicShape();
    const BasicShape& toShape = downcast<ShapeClipPathOperation>(*to).basicShape();

    if (!fromShape.canBlend(toShape))
        return to;

    return ShapeClipPathOperation::create(toShape.blend(fromShape, progress));
}

#if ENABLE(CSS_SHAPES)
static inline PassRefPtr<ShapeValue> blendFunc(const AnimationBase*, ShapeValue* from, ShapeValue* to, double progress)
{
    if (!from || !to)
        return to;

    if (from->type() != ShapeValue::Type::Shape || to->type() != ShapeValue::Type::Shape)
        return to;

    if (from->cssBox() != to->cssBox())
        return to;

    const BasicShape& fromShape = *from->shape();
    const BasicShape& toShape = *to->shape();

    if (!fromShape.canBlend(toShape))
        return to;

    return ShapeValue::createShapeValue(toShape.blend(fromShape, progress), to->cssBox());
}
#endif

static inline PassRefPtr<FilterOperation> blendFunc(const AnimationBase* animation, FilterOperation* fromOp, FilterOperation* toOp, double progress, bool blendToPassthrough = false)
{
    ASSERT(toOp);
    if (toOp->blendingNeedsRendererSize()) {
        LayoutSize size = is<RenderBox>(*animation->renderer()) ? downcast<RenderBox>(*animation->renderer()).borderBoxRect().size() : LayoutSize();
        return toOp->blend(fromOp, progress, size, blendToPassthrough);
    }
    return toOp->blend(fromOp, progress, blendToPassthrough);
}

static inline FilterOperations blendFilterOperations(const AnimationBase* anim,  const FilterOperations& from, const FilterOperations& to, double progress)
{
    FilterOperations result;
    size_t fromSize = from.operations().size();
    size_t toSize = to.operations().size();
    size_t size = std::max(fromSize, toSize);
    for (size_t i = 0; i < size; i++) {
        RefPtr<FilterOperation> fromOp = (i < fromSize) ? from.operations()[i].get() : 0;
        RefPtr<FilterOperation> toOp = (i < toSize) ? to.operations()[i].get() : 0;
        RefPtr<FilterOperation> blendedOp = toOp ? blendFunc(anim, fromOp.get(), toOp.get(), progress) : (fromOp ? blendFunc(anim, 0, fromOp.get(), progress, true) : 0);
        if (blendedOp)
            result.operations().append(blendedOp);
        else {
            RefPtr<FilterOperation> identityOp = PassthroughFilterOperation::create();
            if (progress > 0.5)
                result.operations().append(toOp ? toOp : identityOp);
            else
                result.operations().append(fromOp ? fromOp : identityOp);
        }
    }
    return result;
}

static inline FilterOperations blendFunc(const AnimationBase* anim, const FilterOperations& from, const FilterOperations& to, double progress, bool animatingBackdropFilter = false)
{
    FilterOperations result;

    // If we have a filter function list, use that to do a per-function animation.
#if ENABLE(FILTERS_LEVEL_2)
    if ((!animatingBackdropFilter && anim->filterFunctionListsMatch()) || (animatingBackdropFilter && anim->backdropFilterFunctionListsMatch()))
#else
    UNUSED_PARAM(animatingBackdropFilter);
    if (anim->filterFunctionListsMatch())
#endif

        result = blendFilterOperations(anim, from, to, progress);
    else {
        // If the filter function lists don't match, we could try to cross-fade, but don't yet have a way to represent that in CSS.
        // For now we'll just fail to animate.
        result = to;
    }

    return result;
}

static inline PassRefPtr<StyleImage> blendFilter(const AnimationBase* anim, CachedImage* image, const FilterOperations& from, const FilterOperations& to, double progress)
{
    ASSERT(image);
    FilterOperations filterResult = blendFilterOperations(anim, from, to, progress);

    auto imageValue = CSSImageValue::create(*image);
    auto filterValue = ComputedStyleExtractor::valueForFilter(anim->renderer()->style(), filterResult, DoNotAdjustPixelValues);

    auto result = CSSFilterImageValue::create(WTFMove(imageValue), WTFMove(filterValue));
    result.get().setFilterOperations(filterResult);
    return StyleGeneratedImage::create(WTFMove(result));
}

static inline EVisibility blendFunc(const AnimationBase* anim, EVisibility from, EVisibility to, double progress)
{
    // Any non-zero result means we consider the object to be visible. Only at 0 do we consider the object to be
    // invisible. The invisible value we use (HIDDEN vs. COLLAPSE) depends on the specified from/to values.
    double fromVal = from == VISIBLE ? 1. : 0.;
    double toVal = to == VISIBLE ? 1. : 0.;
    if (fromVal == toVal)
        return to;
    double result = blendFunc(anim, fromVal, toVal, progress);
    return result > 0. ? VISIBLE : (to != VISIBLE ? to : from);
}

static inline LengthBox blendFunc(const AnimationBase* anim, const LengthBox& from, const LengthBox& to, double progress)
{
    LengthBox result(blendFunc(anim, from.top(), to.top(), progress),
                     blendFunc(anim, from.right(), to.right(), progress),
                     blendFunc(anim, from.bottom(), to.bottom(), progress),
                     blendFunc(anim, from.left(), to.left(), progress));
    return result;
}

static inline SVGLength blendFunc(const AnimationBase*, const SVGLength& from, const SVGLength& to, double progress)
{
    return to.blend(from, narrowPrecisionToFloat(progress));
}

static inline Vector<SVGLength> blendFunc(const AnimationBase*, const Vector<SVGLength>& from, const Vector<SVGLength>& to, double progress)
{
    size_t fromLength = from.size();
    size_t toLength = to.size();
    if (!fromLength)
        return !progress ? from : to;
    if (!toLength)
        return progress == 1 ? from : to;
    size_t resultLength = fromLength;
    if (fromLength != toLength) {
        if (!remainder(std::max(fromLength, toLength), std::min(fromLength, toLength)))
            resultLength = std::max(fromLength, toLength);
        else
            resultLength = fromLength * toLength;
    }
    Vector<SVGLength> result(resultLength);
    for (size_t i = 0; i < resultLength; ++i)
        result[i] = to[i % toLength].blend(from[i % fromLength], narrowPrecisionToFloat(progress));
    return result;
}

static inline PassRefPtr<StyleImage> crossfadeBlend(const AnimationBase*, StyleCachedImage* fromStyleImage, StyleCachedImage* toStyleImage, double progress)
{
    // If progress is at one of the extremes, we want getComputedStyle to show the image,
    // not a completed cross-fade, so we hand back one of the existing images.
    if (!progress)
        return fromStyleImage;
    if (progress == 1)
        return toStyleImage;

    auto fromImageValue = CSSImageValue::create(*fromStyleImage->cachedImage());
    auto toImageValue = CSSImageValue::create(*toStyleImage->cachedImage());
    auto percentageValue = CSSPrimitiveValue::create(progress, CSSPrimitiveValue::CSS_NUMBER);

    auto crossfadeValue = CSSCrossfadeValue::create(WTFMove(fromImageValue), WTFMove(toImageValue), WTFMove(percentageValue));
    return StyleGeneratedImage::create(WTFMove(crossfadeValue));
}

static inline PassRefPtr<StyleImage> blendFunc(const AnimationBase* anim, StyleImage* from, StyleImage* to, double progress)
{
    if (!from || !to)
        return to;

    // Animation between two generated images. Cross fade for all other cases.
    if (is<StyleGeneratedImage>(*from) && is<StyleGeneratedImage>(*to)) {
        CSSImageGeneratorValue& fromGenerated = downcast<StyleGeneratedImage>(*from).imageValue();
        CSSImageGeneratorValue& toGenerated = downcast<StyleGeneratedImage>(*to).imageValue();

        if (is<CSSFilterImageValue>(fromGenerated) && is<CSSFilterImageValue>(toGenerated)) {
            // Animation of generated images just possible if input images are equal.
            // Otherwise fall back to cross fade animation.
            CSSFilterImageValue& fromFilter = downcast<CSSFilterImageValue>(fromGenerated);
            CSSFilterImageValue& toFilter = downcast<CSSFilterImageValue>(toGenerated);
            if (fromFilter.equalInputImages(toFilter) && fromFilter.cachedImage())
                return blendFilter(anim, fromFilter.cachedImage(), fromFilter.filterOperations(), toFilter.filterOperations(), progress);
        }

        if (is<CSSCrossfadeValue>(fromGenerated) && is<CSSCrossfadeValue>(toGenerated)) {
            CSSCrossfadeValue& fromCrossfade = downcast<CSSCrossfadeValue>(fromGenerated);
            CSSCrossfadeValue& toCrossfade = downcast<CSSCrossfadeValue>(toGenerated);
            if (fromCrossfade.equalInputImages(toCrossfade)) {
                if (auto crossfadeBlend = toCrossfade.blend(fromCrossfade, progress))
                    return StyleGeneratedImage::create(*crossfadeBlend);
            }
        }

        // FIXME: Add support for animation between two *gradient() functions.
        // https://bugs.webkit.org/show_bug.cgi?id=119956
    } else if (is<StyleGeneratedImage>(*from) && is<StyleCachedImage>(*to)) {
        CSSImageGeneratorValue& fromGenerated = downcast<StyleGeneratedImage>(*from).imageValue();
        if (is<CSSFilterImageValue>(fromGenerated)) {
            CSSFilterImageValue& fromFilter = downcast<CSSFilterImageValue>(fromGenerated);
            if (fromFilter.cachedImage() && downcast<StyleCachedImage>(*to).cachedImage() == fromFilter.cachedImage())
                return blendFilter(anim, fromFilter.cachedImage(), fromFilter.filterOperations(), FilterOperations(), progress);
        }
        // FIXME: Add interpolation between cross-fade and image source.
    } else if (is<StyleCachedImage>(*from) && is<StyleGeneratedImage>(*to)) {
        CSSImageGeneratorValue& toGenerated = downcast<StyleGeneratedImage>(*to).imageValue();
        if (is<CSSFilterImageValue>(toGenerated)) {
            CSSFilterImageValue& toFilter = downcast<CSSFilterImageValue>(toGenerated);
            if (toFilter.cachedImage() && downcast<StyleCachedImage>(*from).cachedImage() == toFilter.cachedImage())
                return blendFilter(anim, toFilter.cachedImage(), FilterOperations(), toFilter.filterOperations(), progress);
        }
        // FIXME: Add interpolation between image source and cross-fade.
    }

    // FIXME: Add support cross fade between cached and generated images.
    // https://bugs.webkit.org/show_bug.cgi?id=78293
    if (is<StyleCachedImage>(*from) && is<StyleCachedImage>(*to))
        return crossfadeBlend(anim, downcast<StyleCachedImage>(from), downcast<StyleCachedImage>(to), progress);

    return to;
}

static inline NinePieceImage blendFunc(const AnimationBase* anim, const NinePieceImage& from, const NinePieceImage& to, double progress)
{
    if (!from.hasImage() || !to.hasImage())
        return to;

    // FIXME (74112): Support transitioning between NinePieceImages that differ by more than image content.

    if (from.imageSlices() != to.imageSlices() || from.borderSlices() != to.borderSlices() || from.outset() != to.outset() || from.fill() != to.fill() || from.horizontalRule() != to.horizontalRule() || from.verticalRule() != to.verticalRule())
        return to;

    if (from.image()->imageSize(anim->renderer(), 1.0) != to.image()->imageSize(anim->renderer(), 1.0))
        return to;

    RefPtr<StyleImage> newContentImage = blendFunc(anim, from.image(), to.image(), progress);

    return NinePieceImage(newContentImage, from.imageSlices(), from.fill(), from.borderSlices(), from.outset(), from.horizontalRule(), from.verticalRule());
}

class AnimationPropertyWrapperBase {
    WTF_MAKE_NONCOPYABLE(AnimationPropertyWrapperBase);
    WTF_MAKE_FAST_ALLOCATED;
public:
    AnimationPropertyWrapperBase(CSSPropertyID prop)
        : m_prop(prop)
    {
    }

    virtual ~AnimationPropertyWrapperBase() { }

    virtual bool isShorthandWrapper() const { return false; }
    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const = 0;
    virtual void blend(const AnimationBase*, RenderStyle*, const RenderStyle*, const RenderStyle*, double) const = 0;
    
#if !LOG_DISABLED
    virtual void logBlend(const RenderStyle* a, const RenderStyle* b, const RenderStyle* result, double) const = 0;
#endif

    CSSPropertyID property() const { return m_prop; }

    virtual bool animationIsAccelerated() const { return false; }

private:
    CSSPropertyID m_prop;
};

template <typename T>
class PropertyWrapperGetter : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperGetter(CSSPropertyID prop, T (RenderStyle::*getter)() const)
        : AnimationPropertyWrapperBase(prop)
        , m_getter(getter)
    {
    }

    bool equals(const RenderStyle* a, const RenderStyle* b) const override
    {
        if (a == b)
            return true;
        if (!a || !b)
            return false;
        return (a->*m_getter)() == (b->*m_getter)();
    }

    T value(const RenderStyle* a) const
    {
        return (a->*m_getter)();
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle* a, const RenderStyle* b, const RenderStyle* result, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << getPropertyName(property()) << " from " << value(a) << " to " << value(b) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(result));
    }
#endif

protected:
    T (RenderStyle::*m_getter)() const;
};

template <typename T>
class PropertyWrapper : public PropertyWrapperGetter<T> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapper(CSSPropertyID prop, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T))
        : PropertyWrapperGetter<T>(prop, getter)
        , m_setter(setter)
    {
    }

    virtual void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const
    {
        (dst->*m_setter)(blendFunc(anim, (a->*PropertyWrapperGetter<T>::m_getter)(), (b->*PropertyWrapperGetter<T>::m_getter)(), progress));
    }

protected:
    void (RenderStyle::*m_setter)(T);
};

template <typename T>
class RefCountedPropertyWrapper : public PropertyWrapperGetter<T*> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RefCountedPropertyWrapper(CSSPropertyID prop, T* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(PassRefPtr<T>))
        : PropertyWrapperGetter<T*>(prop, getter)
        , m_setter(setter)
    {
    }

    virtual void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const
    {
        (dst->*m_setter)(blendFunc(anim, (a->*PropertyWrapperGetter<T*>::m_getter)(), (b->*PropertyWrapperGetter<T*>::m_getter)(), progress));
    }

protected:
    void (RenderStyle::*m_setter)(PassRefPtr<T>);
};

template <typename T>
class LengthPropertyWrapper : public PropertyWrapperGetter<const T&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LengthPropertyWrapper(CSSPropertyID prop, const T& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T))
        : PropertyWrapperGetter<const T&>(prop, getter)
        , m_setter(setter)
    {
    }

    virtual void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const
    {
        (dst->*m_setter)(blendFunc(anim, (a->*PropertyWrapperGetter<const T&>::m_getter)(), (b->*PropertyWrapperGetter<const T&>::m_getter)(), progress));
    }

protected:
    void (RenderStyle::*m_setter)(T);
};

class PropertyWrapperClipPath : public RefCountedPropertyWrapper<ClipPathOperation> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperClipPath(CSSPropertyID prop, ClipPathOperation* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(PassRefPtr<ClipPathOperation>))
        : RefCountedPropertyWrapper<ClipPathOperation>(prop, getter, setter)
    {
    }

    bool equals(const RenderStyle* a, const RenderStyle* b) const override
    {
        // If the style pointers are the same, don't bother doing the test.
        // If either is null, return false. If both are null, return true.
        if (a == b)
            return true;
        if (!a || !b)
            return false;

        ClipPathOperation* clipPathA = (a->*m_getter)();
        ClipPathOperation* clipPathB = (b->*m_getter)();
        if (clipPathA == clipPathB)
            return true;
        if (!clipPathA || !clipPathB)
            return false;
        return *clipPathA == *clipPathB;
    }
};

#if ENABLE(CSS_SHAPES)
class PropertyWrapperShape : public RefCountedPropertyWrapper<ShapeValue> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperShape(CSSPropertyID prop, ShapeValue* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(PassRefPtr<ShapeValue>))
        : RefCountedPropertyWrapper<ShapeValue>(prop, getter, setter)
    {
    }

    bool equals(const RenderStyle* a, const RenderStyle* b) const override
    {
        // If the style pointers are the same, don't bother doing the test.
        // If either is null, return false. If both are null, return true.
        if (a == b)
            return true;
        if (!a || !b)
            return false;

        ShapeValue* shapeA = (a->*m_getter)();
        ShapeValue* shapeB = (b->*m_getter)();
        if (shapeA == shapeB)
            return true;
        if (!shapeA || !shapeB)
            return false;
        return *shapeA == *shapeB;
    }
};
#endif

class StyleImagePropertyWrapper : public RefCountedPropertyWrapper<StyleImage> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    StyleImagePropertyWrapper(CSSPropertyID prop, StyleImage* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(PassRefPtr<StyleImage>))
        : RefCountedPropertyWrapper<StyleImage>(prop, getter, setter)
    {
    }

    bool equals(const RenderStyle* a, const RenderStyle* b) const override
    {
       if (a == b)
           return true;
       if (!a || !b)
            return false;

        StyleImage* imageA = (a->*m_getter)();
        StyleImage* imageB = (b->*m_getter)();
        return arePointingToEqualData(imageA, imageB);
    }
};

class PropertyWrapperColor : public PropertyWrapperGetter<Color> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperColor(CSSPropertyID prop, Color (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Color&))
        : PropertyWrapperGetter<Color>(prop, getter)
        , m_setter(setter)
    {
    }

    void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        (dst->*m_setter)(blendFunc(anim, (a->*PropertyWrapperGetter<Color>::m_getter)(), (b->*PropertyWrapperGetter<Color>::m_getter)(), progress));
    }

protected:
    void (RenderStyle::*m_setter)(const Color&);
};


class PropertyWrapperAcceleratedOpacity : public PropertyWrapper<float> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperAcceleratedOpacity()
        : PropertyWrapper<float>(CSSPropertyOpacity, &RenderStyle::opacity, &RenderStyle::setOpacity)
    {
    }

    bool animationIsAccelerated() const override { return true; }

    void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        float fromOpacity = a->opacity();

        // This makes sure we put the object being animated into a RenderLayer during the animation
        dst->setOpacity(blendFunc(anim, (fromOpacity == 1) ? 0.999999f : fromOpacity, b->opacity(), progress));
    }
};

class PropertyWrapperAcceleratedTransform : public PropertyWrapper<const TransformOperations&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperAcceleratedTransform()
        : PropertyWrapper<const TransformOperations&>(CSSPropertyTransform, &RenderStyle::transform, &RenderStyle::setTransform)
    {
    }

    bool animationIsAccelerated() const override { return true; }

    void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        dst->setTransform(blendFunc(anim, a->transform(), b->transform(), progress));
    }
};

class PropertyWrapperAcceleratedFilter : public PropertyWrapper<const FilterOperations&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperAcceleratedFilter()
        : PropertyWrapper<const FilterOperations&>(CSSPropertyFilter, &RenderStyle::filter, &RenderStyle::setFilter)
    {
    }

    bool animationIsAccelerated() const override { return true; }

    void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        dst->setFilter(blendFunc(anim, a->filter(), b->filter(), progress));
    }
};

#if ENABLE(FILTERS_LEVEL_2)
class PropertyWrapperAcceleratedBackdropFilter : public PropertyWrapper<const FilterOperations&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperAcceleratedBackdropFilter()
        : PropertyWrapper<const FilterOperations&>(CSSPropertyWebkitBackdropFilter, &RenderStyle::backdropFilter, &RenderStyle::setBackdropFilter)
    {
    }

    virtual bool animationIsAccelerated() const { return true; }

    virtual void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const
    {
        dst->setBackdropFilter(blendFunc(anim, a->backdropFilter(), b->backdropFilter(), progress, true));
    }
};
#endif

static inline size_t shadowListLength(const ShadowData* shadow)
{
    size_t count;
    for (count = 0; shadow; shadow = shadow->next())
        ++count;
    return count;
}

static inline const ShadowData* shadowForBlending(const ShadowData* srcShadow, const ShadowData* otherShadow)
{
    static NeverDestroyed<ShadowData> defaultShadowData(IntPoint(), 0, 0, Normal, false, Color::transparent);
    static NeverDestroyed<ShadowData> defaultInsetShadowData(IntPoint(), 0, 0, Inset, false, Color::transparent);
    static NeverDestroyed<ShadowData> defaultWebKitBoxShadowData(IntPoint(), 0, 0, Normal, true, Color::transparent);
    static NeverDestroyed<ShadowData> defaultInsetWebKitBoxShadowData(IntPoint(), 0, 0, Inset, true, Color::transparent);

    if (srcShadow)
        return srcShadow;

    if (otherShadow->style() == Inset)
        return otherShadow->isWebkitBoxShadow() ? &defaultInsetWebKitBoxShadowData.get() : &defaultInsetShadowData.get();

    return otherShadow->isWebkitBoxShadow() ? &defaultWebKitBoxShadowData.get() : &defaultShadowData.get();
}

class PropertyWrapperShadow : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperShadow(CSSPropertyID prop, const ShadowData* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(std::unique_ptr<ShadowData>, bool))
        : AnimationPropertyWrapperBase(prop)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

    bool equals(const RenderStyle* a, const RenderStyle* b) const override
    {
        if (a == b)
            return true;
        if (!a || !b)
            return false;

        const ShadowData* shadowA = (a->*m_getter)();
        const ShadowData* shadowB = (b->*m_getter)();

        while (true) {
            // end of both lists
            if (!shadowA && !shadowB)
                return true;

            // end of just one of the lists
            if (!shadowA || !shadowB)
                return false;

            if (*shadowA != *shadowB)
                return false;

            shadowA = shadowA->next();
            shadowB = shadowB->next();
        }

        return true;
    }

    void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        const ShadowData* shadowA = (a->*m_getter)();
        const ShadowData* shadowB = (b->*m_getter)();

        int fromLength = shadowListLength(shadowA);
        int toLength = shadowListLength(shadowB);

        if (fromLength == toLength || (fromLength <= 1 && toLength <= 1)) {
            (dst->*m_setter)(blendSimpleOrMatchedShadowLists(anim, progress, shadowA, shadowB), false);
            return;
        }

        (dst->*m_setter)(blendMismatchedShadowLists(anim, progress, shadowA, shadowB, fromLength, toLength), false);
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle*, const RenderStyle*, const RenderStyle*, double progress) const final
    {
        // FIXME: better logging.
        LOG_WITH_STREAM(Animations, stream << "  blending ShadowData at " << TextStream::FormatNumberRespectingIntegers(progress));
    }
#endif

private:
    std::unique_ptr<ShadowData> blendSimpleOrMatchedShadowLists(const AnimationBase* anim, double progress, const ShadowData* shadowA, const ShadowData* shadowB) const
    {
        std::unique_ptr<ShadowData> newShadowData;
        ShadowData* lastShadow = 0;

        while (shadowA || shadowB) {
            const ShadowData* srcShadow = shadowForBlending(shadowA, shadowB);
            const ShadowData* dstShadow = shadowForBlending(shadowB, shadowA);

            std::unique_ptr<ShadowData> blendedShadow = blendFunc(anim, srcShadow, dstShadow, progress);
            ShadowData* blendedShadowPtr = blendedShadow.get();

            if (!lastShadow)
                newShadowData = WTFMove(blendedShadow);
            else
                lastShadow->setNext(WTFMove(blendedShadow));

            lastShadow = blendedShadowPtr;

            shadowA = shadowA ? shadowA->next() : 0;
            shadowB = shadowB ? shadowB->next() : 0;
        }

        return newShadowData;
    }

    std::unique_ptr<ShadowData> blendMismatchedShadowLists(const AnimationBase* anim, double progress, const ShadowData* shadowA, const ShadowData* shadowB, int fromLength, int toLength) const
    {
        // The shadows in ShadowData are stored in reverse order, so when animating mismatched lists,
        // reverse them and match from the end.
        Vector<const ShadowData*, 4> fromShadows(fromLength);
        for (int i = fromLength - 1; i >= 0; --i) {
            fromShadows[i] = shadowA;
            shadowA = shadowA->next();
        }

        Vector<const ShadowData*, 4> toShadows(toLength);
        for (int i = toLength - 1; i >= 0; --i) {
            toShadows[i] = shadowB;
            shadowB = shadowB->next();
        }

        std::unique_ptr<ShadowData> newShadowData;

        int maxLength = std::max(fromLength, toLength);
        for (int i = 0; i < maxLength; ++i) {
            const ShadowData* fromShadow = i < fromLength ? fromShadows[i] : 0;
            const ShadowData* toShadow = i < toLength ? toShadows[i] : 0;

            const ShadowData* srcShadow = shadowForBlending(fromShadow, toShadow);
            const ShadowData* dstShadow = shadowForBlending(toShadow, fromShadow);

            std::unique_ptr<ShadowData> blendedShadow = blendFunc(anim, srcShadow, dstShadow, progress);
            // Insert at the start of the list to preserve the order.
            blendedShadow->setNext(WTFMove(newShadowData));
            newShadowData = WTFMove(blendedShadow);
        }

        return newShadowData;
    }

    const ShadowData* (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(std::unique_ptr<ShadowData>, bool);
};

class PropertyWrapperMaybeInvalidColor : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperMaybeInvalidColor(CSSPropertyID prop, Color (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Color&))
        : AnimationPropertyWrapperBase(prop)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

    bool equals(const RenderStyle* a, const RenderStyle* b) const override
    {
        if (a == b)
            return true;
        if (!a || !b)
            return false;

        Color fromColor = value(a);
        Color toColor = value(b);

        if (!fromColor.isValid() && !toColor.isValid())
            return true;

        if (!fromColor.isValid())
            fromColor = a->color();
        if (!toColor.isValid())
            toColor = b->color();

        return fromColor == toColor;
    }

    void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        Color fromColor = value(a);
        Color toColor = value(b);

        if (!fromColor.isValid() && !toColor.isValid())
            return;

        if (!fromColor.isValid())
            fromColor = a->color();
        if (!toColor.isValid())
            toColor = b->color();
        (dst->*m_setter)(blendFunc(anim, fromColor, toColor, progress));
    }

    Color value(const RenderStyle* a) const
    {
        return (a->*m_getter)();
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle* a, const RenderStyle* b, const RenderStyle* result, double progress) const final
    {
        // FIXME: better logging.
        LOG_WITH_STREAM(Animations, stream << "  blending " << getPropertyName(property()) << " from " << value(a) << " to " << value(b) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(result));
    }
#endif

private:
    Color (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(const Color&);
};


enum MaybeInvalidColorTag { MaybeInvalidColor };
class PropertyWrapperVisitedAffectedColor : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperVisitedAffectedColor(CSSPropertyID prop, Color (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Color&),
                                        Color (RenderStyle::*visitedGetter)() const, void (RenderStyle::*visitedSetter)(const Color&))
        : AnimationPropertyWrapperBase(prop)
        , m_wrapper(std::make_unique<PropertyWrapperColor>(prop, getter, setter))
        , m_visitedWrapper(std::make_unique<PropertyWrapperColor>(prop, visitedGetter, visitedSetter))
    {
    }
    PropertyWrapperVisitedAffectedColor(CSSPropertyID prop, MaybeInvalidColorTag, Color (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Color&),
                                        Color (RenderStyle::*visitedGetter)() const, void (RenderStyle::*visitedSetter)(const Color&))
        : AnimationPropertyWrapperBase(prop)
        , m_wrapper(std::make_unique<PropertyWrapperMaybeInvalidColor>(prop, getter, setter))
        , m_visitedWrapper(std::make_unique<PropertyWrapperMaybeInvalidColor>(prop, visitedGetter, visitedSetter))
    {
    }
    bool equals(const RenderStyle* a, const RenderStyle* b) const override
    {
        return m_wrapper->equals(a, b) && m_visitedWrapper->equals(a, b);
    }
    void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        m_wrapper->blend(anim, dst, a, b, progress);
        m_visitedWrapper->blend(anim, dst, a, b, progress);
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle* a, const RenderStyle* b, const RenderStyle* result, double progress) const final
    {
        m_wrapper->logBlend(a, b, result, progress);
        m_visitedWrapper->logBlend(a, b, result, progress);
    }
#endif

private:
    std::unique_ptr<AnimationPropertyWrapperBase> m_wrapper;
    std::unique_ptr<AnimationPropertyWrapperBase> m_visitedWrapper;
};

// Wrapper base class for an animatable property in a FillLayer
class FillLayerAnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FillLayerAnimationPropertyWrapperBase()
    {
    }

    virtual ~FillLayerAnimationPropertyWrapperBase() { }

    virtual bool equals(const FillLayer*, const FillLayer*) const = 0;
    virtual void blend(const AnimationBase*, FillLayer*, const FillLayer*, const FillLayer*, double) const = 0;
};

template <typename T>
class FillLayerPropertyWrapperGetter : public FillLayerAnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(FillLayerPropertyWrapperGetter);
public:
    FillLayerPropertyWrapperGetter(T (FillLayer::*getter)() const)
        : m_getter(getter)
    {
    }

    bool equals(const FillLayer* a, const FillLayer* b) const override
    {
        if (a == b)
            return true;
        if (!a || !b)
            return false;
        return (a->*m_getter)() == (b->*m_getter)();
    }

protected:
    T (FillLayer::*m_getter)() const;
};

template <typename T>
class FillLayerPropertyWrapper : public FillLayerPropertyWrapperGetter<const T&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FillLayerPropertyWrapper(const T& (FillLayer::*getter)() const, void (FillLayer::*setter)(T))
        : FillLayerPropertyWrapperGetter<const T&>(getter)
        , m_setter(setter)
    {
    }

    virtual void blend(const AnimationBase* anim, FillLayer* dst, const FillLayer* a, const FillLayer* b, double progress) const
    {
        (dst->*m_setter)(blendFunc(anim, (a->*FillLayerPropertyWrapperGetter<const T&>::m_getter)(), (b->*FillLayerPropertyWrapperGetter<const T&>::m_getter)(), progress));
    }

protected:
    void (FillLayer::*m_setter)(T);
};

template <typename T>
class FillLayerRefCountedPropertyWrapper : public FillLayerPropertyWrapperGetter<T*> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FillLayerRefCountedPropertyWrapper(T* (FillLayer::*getter)() const, void (FillLayer::*setter)(PassRefPtr<T>))
        : FillLayerPropertyWrapperGetter<T*>(getter)
        , m_setter(setter)
    {
    }

    virtual void blend(const AnimationBase* anim, FillLayer* dst, const FillLayer* a, const FillLayer* b, double progress) const
    {
        (dst->*m_setter)(blendFunc(anim, (a->*FillLayerPropertyWrapperGetter<T*>::m_getter)(), (b->*FillLayerPropertyWrapperGetter<T*>::m_getter)(), progress));
    }

protected:
    void (FillLayer::*m_setter)(PassRefPtr<T>);
};

class FillLayerStyleImagePropertyWrapper : public FillLayerRefCountedPropertyWrapper<StyleImage> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FillLayerStyleImagePropertyWrapper(StyleImage* (FillLayer::*getter)() const, void (FillLayer::*setter)(PassRefPtr<StyleImage>))
        : FillLayerRefCountedPropertyWrapper<StyleImage>(getter, setter)
    {
    }

    bool equals(const FillLayer* a, const FillLayer* b) const override
    {
       if (a == b)
           return true;
       if (!a || !b)
            return false;

        StyleImage* imageA = (a->*m_getter)();
        StyleImage* imageB = (b->*m_getter)();
        return arePointingToEqualData(imageA, imageB);
    }
};

class FillLayersPropertyWrapper : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    typedef const FillLayer* (RenderStyle::*LayersGetter)() const;
    typedef FillLayer& (RenderStyle::*LayersAccessor)();

    FillLayersPropertyWrapper(CSSPropertyID prop, LayersGetter getter, LayersAccessor accessor)
        : AnimationPropertyWrapperBase(prop)
        , m_layersGetter(getter)
        , m_layersAccessor(accessor)
    {
        switch (prop) {
        case CSSPropertyBackgroundPositionX:
        case CSSPropertyWebkitMaskPositionX:
            m_fillLayerPropertyWrapper = std::make_unique<FillLayerPropertyWrapper<Length>>(&FillLayer::xPosition, &FillLayer::setXPosition);
            break;
        case CSSPropertyBackgroundPositionY:
        case CSSPropertyWebkitMaskPositionY:
            m_fillLayerPropertyWrapper = std::make_unique<FillLayerPropertyWrapper<Length>>(&FillLayer::yPosition, &FillLayer::setYPosition);
            break;
        case CSSPropertyBackgroundSize:
        case CSSPropertyWebkitBackgroundSize:
        case CSSPropertyWebkitMaskSize:
            m_fillLayerPropertyWrapper = std::make_unique<FillLayerPropertyWrapper<LengthSize>>(&FillLayer::sizeLength, &FillLayer::setSizeLength);
            break;
        case CSSPropertyBackgroundImage:
            m_fillLayerPropertyWrapper = std::make_unique<FillLayerStyleImagePropertyWrapper>(&FillLayer::image, &FillLayer::setImage);
            break;
        default:
            break;
        }
    }

    bool equals(const RenderStyle* a, const RenderStyle* b) const override
    {
        if (a == b)
            return true;
        if (!a || !b)
            return false;

        const FillLayer* fromLayer = (a->*m_layersGetter)();
        const FillLayer* toLayer = (b->*m_layersGetter)();

        while (fromLayer && toLayer) {
            if (!m_fillLayerPropertyWrapper->equals(fromLayer, toLayer))
                return false;

            fromLayer = fromLayer->next();
            toLayer = toLayer->next();
        }

        return true;
    }

    void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        const FillLayer* aLayer = (a->*m_layersGetter)();
        const FillLayer* bLayer = (b->*m_layersGetter)();
        FillLayer* dstLayer = &(dst->*m_layersAccessor)();

        while (aLayer && bLayer && dstLayer) {
            m_fillLayerPropertyWrapper->blend(anim, dstLayer, aLayer, bLayer, progress);
            aLayer = aLayer->next();
            bLayer = bLayer->next();
            dstLayer = dstLayer->next();
        }
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle*, const RenderStyle*, const RenderStyle*, double progress) const final
    {
        // FIXME: better logging.
        LOG_WITH_STREAM(Animations, stream << "  blending FillLayers at " << TextStream::FormatNumberRespectingIntegers(progress));
    }
#endif

private:
    std::unique_ptr<FillLayerAnimationPropertyWrapperBase> m_fillLayerPropertyWrapper;

    LayersGetter m_layersGetter;
    LayersAccessor m_layersAccessor;
};

class ShorthandPropertyWrapper : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ShorthandPropertyWrapper(CSSPropertyID property, Vector<AnimationPropertyWrapperBase*> longhandWrappers)
        : AnimationPropertyWrapperBase(property)
        , m_propertyWrappers(WTFMove(longhandWrappers))
    {
    }

    bool isShorthandWrapper() const override { return true; }

    bool equals(const RenderStyle* a, const RenderStyle* b) const override
    {
        if (a == b)
            return true;
        if (!a || !b)
            return false;

        for (auto& wrapper : m_propertyWrappers) {
            if (!wrapper->equals(a, b))
                return false;
        }
        return true;
    }

    void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        for (auto& wrapper : m_propertyWrappers)
            wrapper->blend(anim, dst, a, b, progress);
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle*, const RenderStyle*, const RenderStyle*, double progress) const final
    {
        // FIXME: better logging.
        LOG_WITH_STREAM(Animations, stream << "  blending shorthand property " << getPropertyName(property()) << " at " << TextStream::FormatNumberRespectingIntegers(progress));
    }
#endif

    const Vector<AnimationPropertyWrapperBase*>& propertyWrappers() const { return m_propertyWrappers; }

private:
    Vector<AnimationPropertyWrapperBase*> m_propertyWrappers;
};

class PropertyWrapperFlex : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperFlex()
        : AnimationPropertyWrapperBase(CSSPropertyFlex)
    {
    }

    bool equals(const RenderStyle* a, const RenderStyle* b) const override
    {
        if (a == b)
            return true;
        if (!a || !b)
            return false;

        return a->flexBasis() == b->flexBasis() && a->flexGrow() == b->flexGrow() && a->flexShrink() == b->flexShrink();
    }

    void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        dst->setFlexBasis(blendFunc(anim, a->flexBasis(), b->flexBasis(), progress));
        dst->setFlexGrow(blendFunc(anim, a->flexGrow(), b->flexGrow(), progress));
        dst->setFlexShrink(blendFunc(anim, a->flexShrink(), b->flexShrink(), progress));
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle*, const RenderStyle*, const RenderStyle*, double progress) const final
    {
        // FIXME: better logging.
        LOG_WITH_STREAM(Animations, stream << "  blending flex at " << TextStream::FormatNumberRespectingIntegers(progress));
    }
#endif
};

class PropertyWrapperSVGPaint : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperSVGPaint(CSSPropertyID prop, const SVGPaint::SVGPaintType& (RenderStyle::*paintTypeGetter)() const, Color (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Color&))
        : AnimationPropertyWrapperBase(prop)
        , m_paintTypeGetter(paintTypeGetter)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

    bool equals(const RenderStyle* a, const RenderStyle* b) const override
    {
        if (a == b)
            return true;
        if (!a || !b)
            return false;

        if ((a->*m_paintTypeGetter)() != (b->*m_paintTypeGetter)())
            return false;

        // We only support animations between SVGPaints that are pure Color values.
        // For everything else we must return true for this method, otherwise
        // we will try to animate between values forever.
        if ((a->*m_paintTypeGetter)() == SVGPaint::SVG_PAINTTYPE_RGBCOLOR) {
            Color fromColor = (a->*m_getter)();
            Color toColor = (b->*m_getter)();

            if (!fromColor.isValid() && !toColor.isValid())
                return true;

            if (!fromColor.isValid())
                fromColor = Color();
            if (!toColor.isValid())
                toColor = Color();

            return fromColor == toColor;
        }
        return true;
    }

    void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        if ((a->*m_paintTypeGetter)() != SVGPaint::SVG_PAINTTYPE_RGBCOLOR
            || (b->*m_paintTypeGetter)() != SVGPaint::SVG_PAINTTYPE_RGBCOLOR)
            return;

        Color fromColor = (a->*m_getter)();
        Color toColor = (b->*m_getter)();

        if (!fromColor.isValid() && !toColor.isValid())
            return;

        if (!fromColor.isValid())
            fromColor = Color();
        if (!toColor.isValid())
            toColor = Color();
        (dst->*m_setter)(blendFunc(anim, fromColor, toColor, progress));
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle*, const RenderStyle*, const RenderStyle*, double progress) const final
    {
        // FIXME: better logging.
        LOG_WITH_STREAM(Animations, stream << "  blending SVGPaint at " << TextStream::FormatNumberRespectingIntegers(progress));
    }
#endif

private:
    const SVGPaint::SVGPaintType& (RenderStyle::*m_paintTypeGetter)() const;
    Color (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(const Color&);
};

class CSSPropertyAnimationWrapperMap {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static CSSPropertyAnimationWrapperMap& singleton()
    {
        // FIXME: This data is never destroyed. Maybe we should ref count it and toss it when the last AnimationController is destroyed?
        static NeverDestroyed<CSSPropertyAnimationWrapperMap> map;
        return map;
    }

    AnimationPropertyWrapperBase* wrapperForProperty(CSSPropertyID propertyID)
    {
        if (propertyID < firstCSSProperty || propertyID > lastCSSProperty)
            return nullptr;

        unsigned wrapperIndex = indexFromPropertyID(propertyID);
        if (wrapperIndex == cInvalidPropertyWrapperIndex)
            return nullptr;

        return m_propertyWrappers[wrapperIndex].get();
    }

    AnimationPropertyWrapperBase* wrapperForIndex(unsigned index)
    {
        ASSERT(index < m_propertyWrappers.size());
        return m_propertyWrappers[index].get();
    }

    unsigned size()
    {
        return m_propertyWrappers.size();
    }

private:
    CSSPropertyAnimationWrapperMap();
    ~CSSPropertyAnimationWrapperMap() = delete;

    unsigned char& indexFromPropertyID(CSSPropertyID propertyID)
    {
        return m_propertyToIdMap[propertyID - firstCSSProperty];
    }

    Vector<std::unique_ptr<AnimationPropertyWrapperBase>> m_propertyWrappers;
    unsigned char m_propertyToIdMap[numCSSProperties];

    static const unsigned char cInvalidPropertyWrapperIndex = UCHAR_MAX;

    friend class WTF::NeverDestroyed<CSSPropertyAnimationWrapperMap>;
};

CSSPropertyAnimationWrapperMap::CSSPropertyAnimationWrapperMap()
{
    // build the list of property wrappers to do the comparisons and blends
    AnimationPropertyWrapperBase* animatableLonghandPropertyWrappers[] = {
        new LengthPropertyWrapper<Length>(CSSPropertyLeft, &RenderStyle::left, &RenderStyle::setLeft),
        new LengthPropertyWrapper<Length>(CSSPropertyRight, &RenderStyle::right, &RenderStyle::setRight),
        new LengthPropertyWrapper<Length>(CSSPropertyTop, &RenderStyle::top, &RenderStyle::setTop),
        new LengthPropertyWrapper<Length>(CSSPropertyBottom, &RenderStyle::bottom, &RenderStyle::setBottom),

        new LengthPropertyWrapper<Length>(CSSPropertyWidth, &RenderStyle::width, &RenderStyle::setWidth),
        new LengthPropertyWrapper<Length>(CSSPropertyMinWidth, &RenderStyle::minWidth, &RenderStyle::setMinWidth),
        new LengthPropertyWrapper<Length>(CSSPropertyMaxWidth, &RenderStyle::maxWidth, &RenderStyle::setMaxWidth),

        new LengthPropertyWrapper<Length>(CSSPropertyHeight, &RenderStyle::height, &RenderStyle::setHeight),
        new LengthPropertyWrapper<Length>(CSSPropertyMinHeight, &RenderStyle::minHeight, &RenderStyle::setMinHeight),
        new LengthPropertyWrapper<Length>(CSSPropertyMaxHeight, &RenderStyle::maxHeight, &RenderStyle::setMaxHeight),

        new PropertyWrapperFlex(),

        new PropertyWrapper<float>(CSSPropertyBorderLeftWidth, &RenderStyle::borderLeftWidth, &RenderStyle::setBorderLeftWidth),
        new PropertyWrapper<float>(CSSPropertyBorderRightWidth, &RenderStyle::borderRightWidth, &RenderStyle::setBorderRightWidth),
        new PropertyWrapper<float>(CSSPropertyBorderTopWidth, &RenderStyle::borderTopWidth, &RenderStyle::setBorderTopWidth),
        new PropertyWrapper<float>(CSSPropertyBorderBottomWidth, &RenderStyle::borderBottomWidth, &RenderStyle::setBorderBottomWidth),
        new LengthPropertyWrapper<Length>(CSSPropertyMarginLeft, &RenderStyle::marginLeft, &RenderStyle::setMarginLeft),
        new LengthPropertyWrapper<Length>(CSSPropertyMarginRight, &RenderStyle::marginRight, &RenderStyle::setMarginRight),
        new LengthPropertyWrapper<Length>(CSSPropertyMarginTop, &RenderStyle::marginTop, &RenderStyle::setMarginTop),
        new LengthPropertyWrapper<Length>(CSSPropertyMarginBottom, &RenderStyle::marginBottom, &RenderStyle::setMarginBottom),
        new LengthPropertyWrapper<Length>(CSSPropertyPaddingLeft, &RenderStyle::paddingLeft, &RenderStyle::setPaddingLeft),
        new LengthPropertyWrapper<Length>(CSSPropertyPaddingRight, &RenderStyle::paddingRight, &RenderStyle::setPaddingRight),
        new LengthPropertyWrapper<Length>(CSSPropertyPaddingTop, &RenderStyle::paddingTop, &RenderStyle::setPaddingTop),
        new LengthPropertyWrapper<Length>(CSSPropertyPaddingBottom, &RenderStyle::paddingBottom, &RenderStyle::setPaddingBottom),
        new PropertyWrapperVisitedAffectedColor(CSSPropertyColor, &RenderStyle::color, &RenderStyle::setColor, &RenderStyle::visitedLinkColor, &RenderStyle::setVisitedLinkColor),

        new PropertyWrapperVisitedAffectedColor(CSSPropertyBackgroundColor, &RenderStyle::backgroundColor, &RenderStyle::setBackgroundColor, &RenderStyle::visitedLinkBackgroundColor, &RenderStyle::setVisitedLinkBackgroundColor),

        new FillLayersPropertyWrapper(CSSPropertyBackgroundImage, &RenderStyle::backgroundLayers, &RenderStyle::ensureBackgroundLayers),
        new StyleImagePropertyWrapper(CSSPropertyListStyleImage, &RenderStyle::listStyleImage, &RenderStyle::setListStyleImage),
        new StyleImagePropertyWrapper(CSSPropertyWebkitMaskImage, &RenderStyle::maskImage, &RenderStyle::setMaskImage),

        new StyleImagePropertyWrapper(CSSPropertyBorderImageSource, &RenderStyle::borderImageSource, &RenderStyle::setBorderImageSource),
        new LengthPropertyWrapper<LengthBox>(CSSPropertyBorderImageSlice, &RenderStyle::borderImageSlices, &RenderStyle::setBorderImageSlices),
        new LengthPropertyWrapper<LengthBox>(CSSPropertyBorderImageWidth, &RenderStyle::borderImageWidth, &RenderStyle::setBorderImageWidth),
        new LengthPropertyWrapper<LengthBox>(CSSPropertyBorderImageOutset, &RenderStyle::borderImageOutset, &RenderStyle::setBorderImageOutset),

        new StyleImagePropertyWrapper(CSSPropertyWebkitMaskBoxImageSource, &RenderStyle::maskBoxImageSource, &RenderStyle::setMaskBoxImageSource),
        new PropertyWrapper<const NinePieceImage&>(CSSPropertyWebkitMaskBoxImage, &RenderStyle::maskBoxImage, &RenderStyle::setMaskBoxImage),

        new FillLayersPropertyWrapper(CSSPropertyBackgroundPositionX, &RenderStyle::backgroundLayers, &RenderStyle::ensureBackgroundLayers),
        new FillLayersPropertyWrapper(CSSPropertyBackgroundPositionY, &RenderStyle::backgroundLayers, &RenderStyle::ensureBackgroundLayers),
        new FillLayersPropertyWrapper(CSSPropertyBackgroundSize, &RenderStyle::backgroundLayers, &RenderStyle::ensureBackgroundLayers),
        new FillLayersPropertyWrapper(CSSPropertyWebkitBackgroundSize, &RenderStyle::backgroundLayers, &RenderStyle::ensureBackgroundLayers),

        new FillLayersPropertyWrapper(CSSPropertyWebkitMaskPositionX, &RenderStyle::maskLayers, &RenderStyle::ensureMaskLayers),
        new FillLayersPropertyWrapper(CSSPropertyWebkitMaskPositionY, &RenderStyle::maskLayers, &RenderStyle::ensureMaskLayers),
        new FillLayersPropertyWrapper(CSSPropertyWebkitMaskSize, &RenderStyle::maskLayers, &RenderStyle::ensureMaskLayers),

        new PropertyWrapper<float>(CSSPropertyFontSize,
            // Must pass a specified size to setFontSize if Text Autosizing is enabled, but a computed size
            // if text zoom is enabled (if neither is enabled it's irrelevant as they're probably the same).
            // FIXME: Find some way to assert that text zoom isn't activated when Text Autosizing is compiled in.
#if ENABLE(TEXT_AUTOSIZING)
            &RenderStyle::specifiedFontSize,
#else
            &RenderStyle::computedFontSize,
#endif
            &RenderStyle::setFontSize),
        new PropertyWrapper<unsigned short>(CSSPropertyColumnRuleWidth, &RenderStyle::columnRuleWidth, &RenderStyle::setColumnRuleWidth),
        new PropertyWrapper<float>(CSSPropertyColumnGap, &RenderStyle::columnGap, &RenderStyle::setColumnGap),
        new PropertyWrapper<unsigned short>(CSSPropertyColumnCount, &RenderStyle::columnCount, &RenderStyle::setColumnCount),
        new PropertyWrapper<float>(CSSPropertyColumnWidth, &RenderStyle::columnWidth, &RenderStyle::setColumnWidth),
        new PropertyWrapper<float>(CSSPropertyWebkitBorderHorizontalSpacing, &RenderStyle::horizontalBorderSpacing, &RenderStyle::setHorizontalBorderSpacing),
        new PropertyWrapper<float>(CSSPropertyWebkitBorderVerticalSpacing, &RenderStyle::verticalBorderSpacing, &RenderStyle::setVerticalBorderSpacing),
        new PropertyWrapper<int>(CSSPropertyZIndex, &RenderStyle::zIndex, &RenderStyle::setZIndex),
        new PropertyWrapper<short>(CSSPropertyOrphans, &RenderStyle::orphans, &RenderStyle::setOrphans),
        new PropertyWrapper<short>(CSSPropertyWidows, &RenderStyle::widows, &RenderStyle::setWidows),
        new LengthPropertyWrapper<Length>(CSSPropertyLineHeight, &RenderStyle::specifiedLineHeight, &RenderStyle::setLineHeight),
        new PropertyWrapper<float>(CSSPropertyOutlineOffset, &RenderStyle::outlineOffset, &RenderStyle::setOutlineOffset),
        new PropertyWrapper<float>(CSSPropertyOutlineWidth, &RenderStyle::outlineWidth, &RenderStyle::setOutlineWidth),
        new PropertyWrapper<float>(CSSPropertyLetterSpacing, &RenderStyle::letterSpacing, &RenderStyle::setLetterSpacing),
        new LengthPropertyWrapper<Length>(CSSPropertyWordSpacing, &RenderStyle::wordSpacing, &RenderStyle::setWordSpacing),
        new LengthPropertyWrapper<Length>(CSSPropertyTextIndent, &RenderStyle::textIndent, &RenderStyle::setTextIndent),

        new PropertyWrapper<float>(CSSPropertyPerspective, &RenderStyle::perspective, &RenderStyle::setPerspective),
        new LengthPropertyWrapper<Length>(CSSPropertyPerspectiveOriginX, &RenderStyle::perspectiveOriginX, &RenderStyle::setPerspectiveOriginX),
        new LengthPropertyWrapper<Length>(CSSPropertyPerspectiveOriginY, &RenderStyle::perspectiveOriginY, &RenderStyle::setPerspectiveOriginY),
        new LengthPropertyWrapper<Length>(CSSPropertyTransformOriginX, &RenderStyle::transformOriginX, &RenderStyle::setTransformOriginX),
        new LengthPropertyWrapper<Length>(CSSPropertyTransformOriginY, &RenderStyle::transformOriginY, &RenderStyle::setTransformOriginY),
        new PropertyWrapper<float>(CSSPropertyTransformOriginZ, &RenderStyle::transformOriginZ, &RenderStyle::setTransformOriginZ),
        new LengthPropertyWrapper<LengthSize>(CSSPropertyBorderTopLeftRadius, &RenderStyle::borderTopLeftRadius, &RenderStyle::setBorderTopLeftRadius),
        new LengthPropertyWrapper<LengthSize>(CSSPropertyBorderTopRightRadius, &RenderStyle::borderTopRightRadius, &RenderStyle::setBorderTopRightRadius),
        new LengthPropertyWrapper<LengthSize>(CSSPropertyBorderBottomLeftRadius, &RenderStyle::borderBottomLeftRadius, &RenderStyle::setBorderBottomLeftRadius),
        new LengthPropertyWrapper<LengthSize>(CSSPropertyBorderBottomRightRadius, &RenderStyle::borderBottomRightRadius, &RenderStyle::setBorderBottomRightRadius),
        new PropertyWrapper<EVisibility>(CSSPropertyVisibility, &RenderStyle::visibility, &RenderStyle::setVisibility),
        new PropertyWrapper<float>(CSSPropertyZoom, &RenderStyle::zoom, &RenderStyle::setZoomWithoutReturnValue),

        new LengthPropertyWrapper<LengthBox>(CSSPropertyClip, &RenderStyle::clip, &RenderStyle::setClip),

        new PropertyWrapperAcceleratedOpacity(),
        new PropertyWrapperAcceleratedTransform(),
        new PropertyWrapperAcceleratedFilter(),
#if ENABLE(FILTERS_LEVEL_2)
        new PropertyWrapperAcceleratedBackdropFilter(),
#endif
        new PropertyWrapperClipPath(CSSPropertyWebkitClipPath, &RenderStyle::clipPath, &RenderStyle::setClipPath),

#if ENABLE(CSS_SHAPES)
        new PropertyWrapperShape(CSSPropertyWebkitShapeOutside, &RenderStyle::shapeOutside, &RenderStyle::setShapeOutside),
        new LengthPropertyWrapper<Length>(CSSPropertyWebkitShapeMargin, &RenderStyle::shapeMargin, &RenderStyle::setShapeMargin),
        new PropertyWrapper<float>(CSSPropertyWebkitShapeImageThreshold, &RenderStyle::shapeImageThreshold, &RenderStyle::setShapeImageThreshold),
#endif

        new PropertyWrapperVisitedAffectedColor(CSSPropertyColumnRuleColor, MaybeInvalidColor, &RenderStyle::columnRuleColor, &RenderStyle::setColumnRuleColor, &RenderStyle::visitedLinkColumnRuleColor, &RenderStyle::setVisitedLinkColumnRuleColor),
        new PropertyWrapperVisitedAffectedColor(CSSPropertyWebkitTextStrokeColor, MaybeInvalidColor, &RenderStyle::textStrokeColor, &RenderStyle::setTextStrokeColor, &RenderStyle::visitedLinkTextStrokeColor, &RenderStyle::setVisitedLinkTextStrokeColor),
        new PropertyWrapperVisitedAffectedColor(CSSPropertyWebkitTextFillColor, MaybeInvalidColor, &RenderStyle::textFillColor, &RenderStyle::setTextFillColor, &RenderStyle::visitedLinkTextFillColor, &RenderStyle::setVisitedLinkTextFillColor),
        new PropertyWrapperVisitedAffectedColor(CSSPropertyBorderLeftColor, MaybeInvalidColor, &RenderStyle::borderLeftColor, &RenderStyle::setBorderLeftColor, &RenderStyle::visitedLinkBorderLeftColor, &RenderStyle::setVisitedLinkBorderLeftColor),
        new PropertyWrapperVisitedAffectedColor(CSSPropertyBorderRightColor, MaybeInvalidColor, &RenderStyle::borderRightColor, &RenderStyle::setBorderRightColor, &RenderStyle::visitedLinkBorderRightColor, &RenderStyle::setVisitedLinkBorderRightColor),
        new PropertyWrapperVisitedAffectedColor(CSSPropertyBorderTopColor, MaybeInvalidColor, &RenderStyle::borderTopColor, &RenderStyle::setBorderTopColor, &RenderStyle::visitedLinkBorderTopColor, &RenderStyle::setVisitedLinkBorderTopColor),
        new PropertyWrapperVisitedAffectedColor(CSSPropertyBorderBottomColor, MaybeInvalidColor, &RenderStyle::borderBottomColor, &RenderStyle::setBorderBottomColor, &RenderStyle::visitedLinkBorderBottomColor, &RenderStyle::setVisitedLinkBorderBottomColor),
        new PropertyWrapperVisitedAffectedColor(CSSPropertyOutlineColor, MaybeInvalidColor, &RenderStyle::outlineColor, &RenderStyle::setOutlineColor, &RenderStyle::visitedLinkOutlineColor, &RenderStyle::setVisitedLinkOutlineColor),

        new PropertyWrapperShadow(CSSPropertyBoxShadow, &RenderStyle::boxShadow, &RenderStyle::setBoxShadow),
        new PropertyWrapperShadow(CSSPropertyWebkitBoxShadow, &RenderStyle::boxShadow, &RenderStyle::setBoxShadow),
        new PropertyWrapperShadow(CSSPropertyTextShadow, &RenderStyle::textShadow, &RenderStyle::setTextShadow),

        new PropertyWrapperSVGPaint(CSSPropertyFill, &RenderStyle::fillPaintType, &RenderStyle::fillPaintColor, &RenderStyle::setFillPaintColor),
        new PropertyWrapper<float>(CSSPropertyFillOpacity, &RenderStyle::fillOpacity, &RenderStyle::setFillOpacity),

        new PropertyWrapperSVGPaint(CSSPropertyStroke, &RenderStyle::strokePaintType, &RenderStyle::strokePaintColor, &RenderStyle::setStrokePaintColor),
        new PropertyWrapper<float>(CSSPropertyStrokeOpacity, &RenderStyle::strokeOpacity, &RenderStyle::setStrokeOpacity),
        new PropertyWrapper< Vector<SVGLength>>(CSSPropertyStrokeDasharray, &RenderStyle::strokeDashArray, &RenderStyle::setStrokeDashArray),
        new PropertyWrapper<float>(CSSPropertyStrokeMiterlimit, &RenderStyle::strokeMiterLimit, &RenderStyle::setStrokeMiterLimit),

        new LengthPropertyWrapper<Length>(CSSPropertyCx, &RenderStyle::cx, &RenderStyle::setCx),
        new LengthPropertyWrapper<Length>(CSSPropertyCy, &RenderStyle::cy, &RenderStyle::setCy),
        new LengthPropertyWrapper<Length>(CSSPropertyR, &RenderStyle::r, &RenderStyle::setR),
        new LengthPropertyWrapper<Length>(CSSPropertyRx, &RenderStyle::rx, &RenderStyle::setRx),
        new LengthPropertyWrapper<Length>(CSSPropertyRy, &RenderStyle::ry, &RenderStyle::setRy),
        new LengthPropertyWrapper<Length>(CSSPropertyStrokeDashoffset, &RenderStyle::strokeDashOffset, &RenderStyle::setStrokeDashOffset),
        new LengthPropertyWrapper<Length>(CSSPropertyStrokeWidth, &RenderStyle::strokeWidth, &RenderStyle::setStrokeWidth),
        new LengthPropertyWrapper<Length>(CSSPropertyX, &RenderStyle::x, &RenderStyle::setX),
        new LengthPropertyWrapper<Length>(CSSPropertyY, &RenderStyle::y, &RenderStyle::setY),

        new PropertyWrapper<float>(CSSPropertyFloodOpacity, &RenderStyle::floodOpacity, &RenderStyle::setFloodOpacity),
        new PropertyWrapperMaybeInvalidColor(CSSPropertyFloodColor, &RenderStyle::floodColor, &RenderStyle::setFloodColor),

        new PropertyWrapper<float>(CSSPropertyStopOpacity, &RenderStyle::stopOpacity, &RenderStyle::setStopOpacity),
        new PropertyWrapperMaybeInvalidColor(CSSPropertyStopColor, &RenderStyle::stopColor, &RenderStyle::setStopColor),

        new PropertyWrapperMaybeInvalidColor(CSSPropertyLightingColor, &RenderStyle::lightingColor, &RenderStyle::setLightingColor),

        new PropertyWrapper<SVGLength>(CSSPropertyBaselineShift, &RenderStyle::baselineShiftValue, &RenderStyle::setBaselineShiftValue),
        new PropertyWrapper<SVGLength>(CSSPropertyKerning, &RenderStyle::kerning, &RenderStyle::setKerning),
    };
    const unsigned animatableLonghandPropertiesCount = WTF_ARRAY_LENGTH(animatableLonghandPropertyWrappers);

    static const CSSPropertyID animatableShorthandProperties[] = {
        CSSPropertyBackground, // for background-color, background-position, background-image
        CSSPropertyBackgroundPosition,
        CSSPropertyFont, // for font-size, font-weight
        CSSPropertyWebkitMask, // for mask-position
        CSSPropertyWebkitMaskPosition,
        CSSPropertyBorderTop, CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft,
        CSSPropertyBorderColor,
        CSSPropertyBorderRadius,
        CSSPropertyBorderWidth,
        CSSPropertyBorder,
        CSSPropertyBorderImage,
        CSSPropertyBorderSpacing,
        CSSPropertyListStyle, // for list-style-image
        CSSPropertyMargin,
        CSSPropertyOutline,
        CSSPropertyPadding,
        CSSPropertyWebkitTextStroke,
        CSSPropertyColumnRule,
        CSSPropertyWebkitBorderRadius,
        CSSPropertyTransformOrigin
    };
    const unsigned animatableShorthandPropertiesCount = WTF_ARRAY_LENGTH(animatableShorthandProperties);

    // TODO:
    //
    //  CSSPropertyVerticalAlign
    //
    // Compound properties that have components that should be animatable:
    //
    //  CSSPropertyColumns
    //  CSSPropertyWebkitBoxReflect

    // Make sure unused slots have a value
    for (int i = 0; i < numCSSProperties; ++i)
        m_propertyToIdMap[i] = cInvalidPropertyWrapperIndex;

    COMPILE_ASSERT(animatableLonghandPropertiesCount + animatableShorthandPropertiesCount < UCHAR_MAX, numberOfAnimatablePropertiesMustBeLessThanUCharMax);
    m_propertyWrappers.reserveInitialCapacity(animatableLonghandPropertiesCount + animatableShorthandPropertiesCount);

    // First we put the non-shorthand property wrappers into the map, so the shorthand-building
    // code can find them.

    for (unsigned i = 0; i < animatableLonghandPropertiesCount; ++i) {
        AnimationPropertyWrapperBase* wrapper = animatableLonghandPropertyWrappers[i];
        m_propertyWrappers.uncheckedAppend(std::unique_ptr<AnimationPropertyWrapperBase>(wrapper));
        indexFromPropertyID(wrapper->property()) = i;
    }

    for (size_t i = 0; i < animatableShorthandPropertiesCount; ++i) {
        CSSPropertyID propertyID = animatableShorthandProperties[i];
        StylePropertyShorthand shorthand = shorthandForProperty(propertyID);
        if (!shorthand.length())
            continue;

        Vector<AnimationPropertyWrapperBase*> longhandWrappers;
        longhandWrappers.reserveInitialCapacity(shorthand.length());
        const CSSPropertyID* properties = shorthand.properties();
        for (unsigned j = 0; j < shorthand.length(); ++j) {
            unsigned wrapperIndex = indexFromPropertyID(properties[j]);
            if (wrapperIndex == cInvalidPropertyWrapperIndex)
                continue;
            ASSERT(m_propertyWrappers[wrapperIndex]);
            longhandWrappers.uncheckedAppend(m_propertyWrappers[wrapperIndex].get());
        }

        m_propertyWrappers.uncheckedAppend(std::make_unique<ShorthandPropertyWrapper>(propertyID, WTFMove(longhandWrappers)));
        indexFromPropertyID(propertyID) = animatableLonghandPropertiesCount + i;
    }
}

static bool gatherEnclosingShorthandProperties(CSSPropertyID property, AnimationPropertyWrapperBase* wrapper, HashSet<CSSPropertyID>& propertySet)
{
    if (!wrapper->isShorthandWrapper())
        return false;

    ShorthandPropertyWrapper* shorthandWrapper = static_cast<ShorthandPropertyWrapper*>(wrapper);
    bool contained = false;
    for (auto& currWrapper : shorthandWrapper->propertyWrappers()) {
        if (gatherEnclosingShorthandProperties(property, currWrapper, propertySet) || currWrapper->property() == property)
            contained = true;
    }

    if (contained)
        propertySet.add(wrapper->property());

    return contained;
}

// Returns true if we need to start animation timers
bool CSSPropertyAnimation::blendProperties(const AnimationBase* anim, CSSPropertyID prop, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress)
{
    ASSERT(prop != CSSPropertyInvalid);

    AnimationPropertyWrapperBase* wrapper = CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(prop);
    if (wrapper) {
        wrapper->blend(anim, dst, a, b, progress);
#if !LOG_DISABLED
        wrapper->logBlend(a, b, dst, progress);
#endif
        return !wrapper->animationIsAccelerated() || !anim->isAccelerated();
    }
    return false;
}

bool CSSPropertyAnimation::animationOfPropertyIsAccelerated(CSSPropertyID prop)
{
    AnimationPropertyWrapperBase* wrapper = CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(prop);
    return wrapper ? wrapper->animationIsAccelerated() : false;
}

// Note: this is inefficient. It's only called from pauseTransitionAtTime().
HashSet<CSSPropertyID> CSSPropertyAnimation::animatableShorthandsAffectingProperty(CSSPropertyID property)
{
    CSSPropertyAnimationWrapperMap& map = CSSPropertyAnimationWrapperMap::singleton();

    HashSet<CSSPropertyID> foundProperties;
    for (unsigned i = 0; i < map.size(); ++i)
        gatherEnclosingShorthandProperties(property, map.wrapperForIndex(i), foundProperties);

    return foundProperties;
}

bool CSSPropertyAnimation::propertiesEqual(CSSPropertyID prop, const RenderStyle* a, const RenderStyle* b)
{
    AnimationPropertyWrapperBase* wrapper = CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(prop);
    if (wrapper)
        return wrapper->equals(a, b);
    return true;
}

CSSPropertyID CSSPropertyAnimation::getPropertyAtIndex(int i, bool& isShorthand)
{
    CSSPropertyAnimationWrapperMap& map = CSSPropertyAnimationWrapperMap::singleton();

    if (i < 0 || static_cast<unsigned>(i) >= map.size())
        return CSSPropertyInvalid;

    AnimationPropertyWrapperBase* wrapper = map.wrapperForIndex(i);
    isShorthand = wrapper->isShorthandWrapper();
    return wrapper->property();
}

int CSSPropertyAnimation::getNumProperties()
{
    return CSSPropertyAnimationWrapperMap::singleton().size();
}

}
