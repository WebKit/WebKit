/*
 * Copyright (C) 2007, 2008, 2009, 2013, 2016 Apple Inc. All rights reserved.
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

#include "CSSComputedStyleDeclaration.h"
#include "CSSCrossfadeValue.h"
#include "CSSFilterImageValue.h"
#include "CSSImageGeneratorValue.h"
#include "CSSImageValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyBlendingClient.h"
#include "CSSPropertyNames.h"
#include "CachedImage.h"
#include "CalculationValue.h"
#include "ClipPathOperation.h"
#include "ColorBlending.h"
#include "FloatConversion.h"
#include "FontCascade.h"
#include "FontSelectionAlgorithm.h"
#include "FontTaggedSettings.h"
#include "GapLength.h"
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
#include <algorithm>
#include <memory>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/PointerComparison.h>
#include <wtf/RefCounted.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

static inline int blendFunc(const CSSPropertyBlendingClient*, int from, int to, double progress)
{
    return blend(from, to, progress);
}

static inline double blendFunc(const CSSPropertyBlendingClient*, double from, double to, double progress)
{
    return blend(from, to, progress);
}

static inline float blendFunc(const CSSPropertyBlendingClient*, float from, float to, double progress)
{
    return narrowPrecisionToFloat(from + (to - from) * progress);
}

static inline Color blendFunc(const CSSPropertyBlendingClient*, const Color& from, const Color& to, double progress)
{
    return blend(from, to, progress);
}

static inline Length blendFunc(const CSSPropertyBlendingClient*, const Length& from, const Length& to, double progress)
{
    return blend(from, to, progress);
}

static inline GapLength blendFunc(const CSSPropertyBlendingClient*, const GapLength& from, const GapLength& to, double progress)
{
    return (from.isNormal() || to.isNormal()) ? to : blend(from.length(), to.length(), progress);
}

static inline LengthSize blendFunc(const CSSPropertyBlendingClient* anim, const LengthSize& from, const LengthSize& to, double progress)
{
    return { blendFunc(anim, from.width, to.width, progress), blendFunc(anim, from.height, to.height, progress) };
}

static inline ShadowStyle blendFunc(const CSSPropertyBlendingClient* anim, ShadowStyle from, ShadowStyle to, double progress)
{
    if (from == to)
        return to;

    double fromVal = from == ShadowStyle::Normal ? 1 : 0;
    double toVal = to == ShadowStyle::Normal ? 1 : 0;
    double result = blendFunc(anim, fromVal, toVal, progress);
    return result > 0 ? ShadowStyle::Normal : ShadowStyle::Inset;
}

static inline std::unique_ptr<ShadowData> blendFunc(const CSSPropertyBlendingClient* anim, const ShadowData* from, const ShadowData* to, double progress)
{
    ASSERT(from && to);
    if (from->style() != to->style())
        return makeUnique<ShadowData>(*to);

    return makeUnique<ShadowData>(blend(from->location(), to->location(), progress),
        blend(from->radius(), to->radius(), progress),
        blend(from->spread(), to->spread(), progress),
        blendFunc(anim, from->style(), to->style(), progress),
        from->isWebkitBoxShadow(),
        blend(from->color(), to->color(), progress));
}

static inline TransformOperations blendFunc(const CSSPropertyBlendingClient* animation, const TransformOperations& from, const TransformOperations& to, double progress)
{
    if (animation->transformFunctionListsMatch())
        return to.blendByMatchingOperations(from, progress);
    return to.blendByUsingMatrixInterpolation(from, progress, is<RenderBox>(animation->renderer()) ? downcast<RenderBox>(*animation->renderer()).borderBoxRect().size() : LayoutSize());
}

static RefPtr<ScaleTransformOperation> blendFunc(const CSSPropertyBlendingClient* client, ScaleTransformOperation* from, ScaleTransformOperation* to, double progress)
{
    if (!from && !to)
        return nullptr;

    RefPtr<ScaleTransformOperation> identity;
    if (!from) {
        identity = ScaleTransformOperation::create(1, 1, 1, to->type());
        from = identity.get();
    } else if (!to) {
        identity = ScaleTransformOperation::create(1, 1, 1, from->type());
        to = identity.get();
    }

    // Ensure the two transforms have the same type.
    if (!from->isSameType(*to)) {
        RefPtr<ScaleTransformOperation> normalizedFrom;
        RefPtr<ScaleTransformOperation> normalizedTo;
        if (from->is3DOperation() || to->is3DOperation()) {
            normalizedFrom = ScaleTransformOperation::create(from->x(), from->y(), from->z(), TransformOperation::SCALE_3D);
            normalizedTo = ScaleTransformOperation::create(to->x(), to->y(), to->z(), TransformOperation::SCALE_3D);
        } else {
            normalizedFrom = ScaleTransformOperation::create(from->x(), from->y(), TransformOperation::SCALE);
            normalizedTo = ScaleTransformOperation::create(to->x(), to->y(), TransformOperation::SCALE);
        }
        return blendFunc(client, normalizedFrom.get(), normalizedTo.get(), progress);
    }

    auto blendedOperation = to->blend(from, progress);
    if (is<ScaleTransformOperation>(blendedOperation.get())) {
        auto& scale = downcast<ScaleTransformOperation>(blendedOperation.get());
        return ScaleTransformOperation::create(scale.x(), scale.y(), scale.z(), scale.type());
    }
    return nullptr;
}

static RefPtr<RotateTransformOperation> blendFunc(const CSSPropertyBlendingClient* client, RotateTransformOperation* from, RotateTransformOperation* to, double progress)
{
    if (!from && !to)
        return nullptr;

    RefPtr<RotateTransformOperation> identity;
    if (!from) {
        identity = RotateTransformOperation::create(0, to->type());
        from = identity.get();
    } else if (!to) {
        identity = RotateTransformOperation::create(0, from->type());
        to = identity.get();
    }

    // Ensure the two transforms have the same type.
    if (!from->isSameType(*to)) {
        RefPtr<RotateTransformOperation> normalizedFrom;
        RefPtr<RotateTransformOperation> normalizedTo;
        if (from->is3DOperation() || to->is3DOperation()) {
            normalizedFrom = RotateTransformOperation::create(from->x(), from->y(), from->z(), from->angle(), TransformOperation::ROTATE_3D);
            normalizedTo = RotateTransformOperation::create(to->x(), to->y(), to->z(), to->angle(), TransformOperation::ROTATE_3D);
        } else {
            normalizedFrom = RotateTransformOperation::create(from->angle(), TransformOperation::ROTATE);
            normalizedTo = RotateTransformOperation::create(to->angle(), TransformOperation::ROTATE);
        }
        return blendFunc(client, normalizedFrom.get(), normalizedTo.get(), progress);
    }

    auto blendedOperation = to->blend(from, progress);
    if (is<RotateTransformOperation>(blendedOperation.get())) {
        auto& rotate = downcast<RotateTransformOperation>(blendedOperation.get());
        return RotateTransformOperation::create(rotate.x(), rotate.y(), rotate.z(), rotate.angle(), rotate.type());
    }
    return nullptr;
}

static RefPtr<TranslateTransformOperation> blendFunc(const CSSPropertyBlendingClient* client, TranslateTransformOperation* from, TranslateTransformOperation* to, double progress)
{
    if (!from && !to)
        return nullptr;

    RefPtr<TranslateTransformOperation> identity;
    if (!from) {
        identity = TranslateTransformOperation::create(Length(0, LengthType::Fixed), Length(0, LengthType::Fixed), Length(0, LengthType::Fixed), to->type());
        from = identity.get();
    } else if (!to) {
        identity = TranslateTransformOperation::create(Length(0, LengthType::Fixed), Length(0, LengthType::Fixed), Length(0, LengthType::Fixed), from->type());
        to = identity.get();
    }

    // Ensure the two transforms have the same type.
    if (!from->isSameType(*to)) {
        RefPtr<TranslateTransformOperation> normalizedFrom;
        RefPtr<TranslateTransformOperation> normalizedTo;
        if (from->is3DOperation() || to->is3DOperation()) {
            normalizedFrom = TranslateTransformOperation::create(from->x(), from->y(), from->z(), TransformOperation::TRANSLATE_3D);
            normalizedTo = TranslateTransformOperation::create(to->x(), to->y(), to->z(), TransformOperation::TRANSLATE_3D);
        } else {
            normalizedFrom = TranslateTransformOperation::create(from->x(), from->y(), TransformOperation::TRANSLATE);
            normalizedTo = TranslateTransformOperation::create(to->x(), to->y(), TransformOperation::TRANSLATE);
        }
        return blendFunc(client, normalizedFrom.get(), normalizedTo.get(), progress);
    }

    Ref<TransformOperation> blendedOperation = to->blend(from, progress);
    if (is<TranslateTransformOperation>(blendedOperation.get())) {
        TranslateTransformOperation& translate = downcast<TranslateTransformOperation>(blendedOperation.get());
        return TranslateTransformOperation::create(translate.x(), translate.y(), translate.z(), translate.type());
    }
    return nullptr;
}

static inline RefPtr<ClipPathOperation> blendFunc(const CSSPropertyBlendingClient*, ClipPathOperation* from, ClipPathOperation* to, double progress)
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

static inline RefPtr<ShapeValue> blendFunc(const CSSPropertyBlendingClient*, ShapeValue* from, ShapeValue* to, double progress)
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

    return ShapeValue::create(toShape.blend(fromShape, progress), to->cssBox());
}

static inline RefPtr<FilterOperation> blendFunc(const CSSPropertyBlendingClient*, FilterOperation* fromOp, FilterOperation* toOp, double progress, bool blendToPassthrough = false)
{
    ASSERT(toOp);
    return toOp->blend(fromOp, progress, blendToPassthrough);
}

static inline FilterOperations blendFilterOperations(const CSSPropertyBlendingClient* anim,  const FilterOperations& from, const FilterOperations& to, double progress)
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
            auto identityOp = PassthroughFilterOperation::create();
            if (progress > 0.5)
                result.operations().append(toOp ? toOp : WTFMove(identityOp));
            else
                result.operations().append(fromOp ? fromOp : WTFMove(identityOp));
        }
    }
    return result;
}

static inline FilterOperations blendFunc(const CSSPropertyBlendingClient* anim, const FilterOperations& from, const FilterOperations& to, double progress, CSSPropertyID propertyID = CSSPropertyFilter)
{
    FilterOperations result;

    // If we have a filter function list, use that to do a per-function animation.
    
    bool listsMatch = false;
    switch (propertyID) {
    case CSSPropertyFilter:
        listsMatch = anim->filterFunctionListsMatch();
        break;
#if ENABLE(FILTERS_LEVEL_2)
    case CSSPropertyWebkitBackdropFilter:
        listsMatch = anim->backdropFilterFunctionListsMatch();
        break;
#endif
    case CSSPropertyAppleColorFilter:
        listsMatch = anim->colorFilterFunctionListsMatch();
        break;
    default:
        break;
    }
    
    if (listsMatch)
        result = blendFilterOperations(anim, from, to, progress);
    else {
        // If the filter function lists don't match, we could try to cross-fade, but don't yet have a way to represent that in CSS.
        // For now we'll just fail to animate.
        result = to;
    }

    return result;
}

static inline RefPtr<StyleImage> blendFilter(const CSSPropertyBlendingClient* anim, CachedImage* image, const FilterOperations& from, const FilterOperations& to, double progress)
{
    ASSERT(image);
    FilterOperations filterResult = blendFilterOperations(anim, from, to, progress);

    auto imageValue = CSSImageValue::create(*image);
    auto filterValue = ComputedStyleExtractor::valueForFilter(anim->currentStyle(), filterResult, DoNotAdjustPixelValues);

    auto result = CSSFilterImageValue::create(WTFMove(imageValue), WTFMove(filterValue));
    result.get().setFilterOperations(filterResult);
    return StyleGeneratedImage::create(WTFMove(result));
}

static inline Visibility blendFunc(const CSSPropertyBlendingClient* anim, Visibility from, Visibility to, double progress)
{
    // Any non-zero result means we consider the object to be visible. Only at 0 do we consider the object to be
    // invisible. The invisible value we use (Visibility::Hidden vs. Visibility::Collapse) depends on the specified from/to values.
    double fromVal = from == Visibility::Visible ? 1. : 0.;
    double toVal = to == Visibility::Visible ? 1. : 0.;
    if (fromVal == toVal)
        return to;
    double result = blendFunc(anim, fromVal, toVal, progress);
    return result > 0. ? Visibility::Visible : (to != Visibility::Visible ? to : from);
}

static inline TextUnderlineOffset blendFunc(const CSSPropertyBlendingClient* anim, const TextUnderlineOffset& from, const TextUnderlineOffset& to, double progress)
{
    if (from.isLength() && to.isLength())
        return TextUnderlineOffset::createWithLength(blendFunc(anim, from.lengthValue(), to.lengthValue(), progress));
    return TextUnderlineOffset::createWithAuto();
}

static inline TextDecorationThickness blendFunc(const CSSPropertyBlendingClient* anim, const TextDecorationThickness& from, const TextDecorationThickness& to, double progress)
{
    if (from.isLength() && to.isLength())
        return TextDecorationThickness::createWithLength(blendFunc(anim, from.lengthValue(), to.lengthValue(), progress));
    return TextDecorationThickness::createWithAuto();
}

static inline LengthBox blendFunc(const CSSPropertyBlendingClient* anim, const LengthBox& from, const LengthBox& to, double progress)
{
    LengthBox result(blendFunc(anim, from.top(), to.top(), progress),
                     blendFunc(anim, from.right(), to.right(), progress),
                     blendFunc(anim, from.bottom(), to.bottom(), progress),
                     blendFunc(anim, from.left(), to.left(), progress));
    return result;
}

static inline SVGLengthValue blendFunc(const CSSPropertyBlendingClient*, const SVGLengthValue& from, const SVGLengthValue& to, double progress)
{
    return SVGLengthValue::blend(from, to, narrowPrecisionToFloat(progress));
}

static inline Vector<SVGLengthValue> blendFunc(const CSSPropertyBlendingClient*, const Vector<SVGLengthValue>& from, const Vector<SVGLengthValue>& to, double progress)
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
    Vector<SVGLengthValue> result(resultLength);
    for (size_t i = 0; i < resultLength; ++i)
        result[i] = SVGLengthValue::blend(from[i % fromLength], to[i % toLength], narrowPrecisionToFloat(progress));
    return result;
}

static inline RefPtr<StyleImage> crossfadeBlend(const CSSPropertyBlendingClient*, StyleCachedImage* fromStyleImage, StyleCachedImage* toStyleImage, double progress)
{
    // If progress is at one of the extremes, we want getComputedStyle to show the image,
    // not a completed cross-fade, so we hand back one of the existing images.
    if (!progress)
        return fromStyleImage;
    if (progress == 1)
        return toStyleImage;
    if (!fromStyleImage->cachedImage() || !toStyleImage->cachedImage())
        return toStyleImage;

    auto fromImageValue = CSSImageValue::create(*fromStyleImage->cachedImage());
    auto toImageValue = CSSImageValue::create(*toStyleImage->cachedImage());
    auto percentageValue = CSSPrimitiveValue::create(progress, CSSUnitType::CSS_NUMBER);

    auto crossfadeValue = CSSCrossfadeValue::create(WTFMove(fromImageValue), WTFMove(toImageValue), WTFMove(percentageValue));
    return StyleGeneratedImage::create(WTFMove(crossfadeValue));
}

static inline RefPtr<StyleImage> blendFunc(const CSSPropertyBlendingClient* anim, StyleImage* from, StyleImage* to, double progress)
{
    if (!from || !to)
        return to;

    from = from->selectedImage();
    to = to->selectedImage();

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

static inline NinePieceImage blendFunc(const CSSPropertyBlendingClient* anim, const NinePieceImage& from, const NinePieceImage& to, double progress)
{
    if (!from.hasImage() || !to.hasImage())
        return to;

    // FIXME (74112): Support transitioning between NinePieceImages that differ by more than image content.

    if (from.imageSlices() != to.imageSlices() || from.borderSlices() != to.borderSlices() || from.outset() != to.outset() || from.fill() != to.fill() || from.horizontalRule() != to.horizontalRule() || from.verticalRule() != to.verticalRule())
        return to;

    if (auto* renderer = anim->renderer()) {
        if (from.image()->imageSize(renderer, 1.0) != to.image()->imageSize(renderer, 1.0))
            return to;
    }

    return NinePieceImage(blendFunc(anim, from.image(), to.image(), progress),
        from.imageSlices(), from.fill(), from.borderSlices(), from.outset(), from.horizontalRule(), from.verticalRule());
}

#if ENABLE(VARIATION_FONTS)

static inline FontVariationSettings blendFunc(const CSSPropertyBlendingClient* anim, const FontVariationSettings& from, const FontVariationSettings& to, double progress)
{
    if (from.size() != to.size())
        return FontVariationSettings();
    FontVariationSettings result;
    unsigned size = from.size();
    for (unsigned i = 0; i < size; ++i) {
        auto& fromItem = from.at(i);
        auto& toItem = to.at(i);
        if (fromItem.tag() != toItem.tag())
            return FontVariationSettings();
        float interpolated = blendFunc(anim, fromItem.value(), toItem.value(), progress);
        result.insert({ fromItem.tag(), interpolated });
    }
    return result;
}

#endif

static inline FontSelectionValue blendFunc(const CSSPropertyBlendingClient* anim, FontSelectionValue from, FontSelectionValue to, double progress)
{
    return FontSelectionValue(blendFunc(anim, static_cast<float>(from), static_cast<float>(to), progress));
}

static inline Optional<FontSelectionValue> blendFunc(const CSSPropertyBlendingClient* anim, Optional<FontSelectionValue> from, Optional<FontSelectionValue> to, double progress)
{
    return FontSelectionValue(blendFunc(anim, static_cast<float>(from.value()), static_cast<float>(to.value()), progress));
}

class AnimationPropertyWrapperBase {
    WTF_MAKE_NONCOPYABLE(AnimationPropertyWrapperBase);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit AnimationPropertyWrapperBase(CSSPropertyID prop)
        : m_prop(prop)
    {
    }
    virtual ~AnimationPropertyWrapperBase() = default;

    virtual bool isShorthandWrapper() const { return false; }
    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const = 0;
    virtual bool canInterpolate(const RenderStyle*, const RenderStyle*) const { return true; }
    virtual void blend(const CSSPropertyBlendingClient*, RenderStyle*, const RenderStyle*, const RenderStyle*, double) const = 0;
    
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

    void blend(const CSSPropertyBlendingClient* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        (dst->*m_setter)(blendFunc(anim, (a->*PropertyWrapperGetter<T>::m_getter)(), (b->*PropertyWrapperGetter<T>::m_getter)(), progress));
    }

protected:
    void (RenderStyle::*m_setter)(T);
};

template <typename T>
class DiscretePropertyWrapper : public PropertyWrapperGetter<T> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DiscretePropertyWrapper(CSSPropertyID prop, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T))
        : PropertyWrapperGetter<T>(prop, getter)
        , m_setter(setter)
    {
    }

    bool canInterpolate(const RenderStyle*, const RenderStyle*) const final { return false; }

    void blend(const CSSPropertyBlendingClient*, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        // https://drafts.csswg.org/web-animations-1/#discrete
        // The property’s values cannot be meaningfully combined, thus it is not additive and
        // interpolation swaps from Va to Vb at 50% (p=0.5).
        if (progress < 0.5)
            (dst->*m_setter)((a->*PropertyWrapperGetter<T>::m_getter)());
        else
            (dst->*m_setter)((b->*PropertyWrapperGetter<T>::m_getter)());
    }

protected:
    void (RenderStyle::*m_setter)(T);
};

template <typename T>
class RefCountedPropertyWrapper : public PropertyWrapperGetter<T*> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RefCountedPropertyWrapper(CSSPropertyID prop, T* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(RefPtr<T>&&))
        : PropertyWrapperGetter<T*>(prop, getter)
        , m_setter(setter)
    {
    }

    void blend(const CSSPropertyBlendingClient* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        (dst->*m_setter)(blendFunc(anim, (a->*PropertyWrapperGetter<T*>::m_getter)(), (b->*PropertyWrapperGetter<T*>::m_getter)(), progress));
    }

protected:
    void (RenderStyle::*m_setter)(RefPtr<T>&&);
};

class LengthPropertyWrapper : public PropertyWrapperGetter<const Length&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LengthPropertyWrapper(CSSPropertyID prop, const Length& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(Length&&))
        : PropertyWrapperGetter<const Length&>(prop, getter)
        , m_setter(setter)
    {
    }

    bool canInterpolate(const RenderStyle* a, const RenderStyle* b) const override
    {
        return !(a->*PropertyWrapperGetter<const Length&>::m_getter)().isAuto() && !(b->*PropertyWrapperGetter<const Length&>::m_getter)().isAuto();
    }

    void blend(const CSSPropertyBlendingClient* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        (dst->*m_setter)(blendFunc(anim, (a->*PropertyWrapperGetter<const Length&>::m_getter)(), (b->*PropertyWrapperGetter<const Length&>::m_getter)(), progress));
    }

protected:
    void (RenderStyle::*m_setter)(Length&&);
};

class NonNegativeLengthPropertyWrapper : public PropertyWrapperGetter<const Length&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    NonNegativeLengthPropertyWrapper(CSSPropertyID prop, const Length& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(Length&&))
        : PropertyWrapperGetter<const Length&>(prop, getter)
        , m_setter(setter)
    {
    }

    bool canInterpolate(const RenderStyle* a, const RenderStyle* b) const override
    {
        return !(a->*PropertyWrapperGetter<const Length&>::m_getter)().isAuto() && !(b->*PropertyWrapperGetter<const Length&>::m_getter)().isAuto();
    }

    void blend(const CSSPropertyBlendingClient* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        auto blended = blendFunc(anim, (a->*PropertyWrapperGetter<const Length&>::m_getter)(), (b->*PropertyWrapperGetter<const Length&>::m_getter)(), progress);
        if (blended.isNegative())
            (dst->*m_setter)(Length(0, LengthType::Fixed));
        else
            (dst->*m_setter)(WTFMove(blended));
    }

protected:
    void (RenderStyle::*m_setter)(Length&&);
};

template <typename T>
class LengthVariantPropertyWrapper : public PropertyWrapperGetter<const T&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LengthVariantPropertyWrapper(CSSPropertyID prop, const T& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T&&))
        : PropertyWrapperGetter<const T&>(prop, getter)
        , m_setter(setter)
    {
    }

    void blend(const CSSPropertyBlendingClient* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        (dst->*m_setter)(blendFunc(anim, (a->*PropertyWrapperGetter<const T&>::m_getter)(), (b->*PropertyWrapperGetter<const T&>::m_getter)(), progress));
    }

protected:
    void (RenderStyle::*m_setter)(T&&);
};

class PropertyWrapperClipPath : public RefCountedPropertyWrapper<ClipPathOperation> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperClipPath(CSSPropertyID prop, ClipPathOperation* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(RefPtr<ClipPathOperation>&&))
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

#if ENABLE(VARIATION_FONTS)
class PropertyWrapperFontVariationSettings : public PropertyWrapper<FontVariationSettings> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperFontVariationSettings(CSSPropertyID prop, FontVariationSettings (RenderStyle::*getter)() const, void (RenderStyle::*setter)(FontVariationSettings))
        : PropertyWrapper<FontVariationSettings>(prop, getter, setter)
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

        const FontVariationSettings& variationSettingsA = (a->*m_getter)();
        const FontVariationSettings& variationSettingsB = (b->*m_getter)();
        return variationSettingsA == variationSettingsB;
    }
};
#endif

class PropertyWrapperShape : public RefCountedPropertyWrapper<ShapeValue> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperShape(CSSPropertyID prop, ShapeValue* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(RefPtr<ShapeValue>&&))
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

class StyleImagePropertyWrapper : public RefCountedPropertyWrapper<StyleImage> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    StyleImagePropertyWrapper(CSSPropertyID prop, StyleImage* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(RefPtr<StyleImage>&&))
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

class PropertyWrapperColor : public PropertyWrapperGetter<const Color&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperColor(CSSPropertyID prop, const Color& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Color&))
        : PropertyWrapperGetter<const Color&>(prop, getter)
        , m_setter(setter)
    {
    }

    void blend(const CSSPropertyBlendingClient* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        (dst->*m_setter)(blendFunc(anim, (a->*PropertyWrapperGetter<const Color&>::m_getter)(), (b->*PropertyWrapperGetter<const Color&>::m_getter)(), progress));
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

    void blend(const CSSPropertyBlendingClient* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        dst->setOpacity(blendFunc(anim, a->opacity(), b->opacity(), progress));
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

    void blend(const CSSPropertyBlendingClient* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        dst->setTransform(blendFunc(anim, a->transform(), b->transform(), progress));
    }
};

class PropertyWrapperScale final : public RefCountedPropertyWrapper<ScaleTransformOperation> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperScale()
        : RefCountedPropertyWrapper<ScaleTransformOperation>(CSSPropertyScale, &RenderStyle::scale, &RenderStyle::setScale)
    {
    }

private:
    bool animationIsAccelerated() const final { return true; }

    bool equals(const RenderStyle* a, const RenderStyle* b) const final
    {
        return arePointingToEqualData((a->*m_getter)(), (b->*m_getter)());
    }
};

class PropertyWrapperRotate final : public RefCountedPropertyWrapper<RotateTransformOperation> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperRotate()
        : RefCountedPropertyWrapper<RotateTransformOperation>(CSSPropertyRotate, &RenderStyle::rotate, &RenderStyle::setRotate)
    {
    }

private:
    bool animationIsAccelerated() const final { return true; }

    bool equals(const RenderStyle* a, const RenderStyle* b) const final
    {
        return arePointingToEqualData((a->*m_getter)(), (b->*m_getter)());
    }
};

class PropertyWrapperTranslate final : public RefCountedPropertyWrapper<TranslateTransformOperation> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperTranslate()
        : RefCountedPropertyWrapper<TranslateTransformOperation>(CSSPropertyTranslate, &RenderStyle::translate, &RenderStyle::setTranslate)
    {
    }

private:
    bool animationIsAccelerated() const final { return true; }

    bool equals(const RenderStyle* a, const RenderStyle* b) const final
    {
        return arePointingToEqualData((a->*m_getter)(), (b->*m_getter)());
    }
};

class PropertyWrapperFilter : public PropertyWrapper<const FilterOperations&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperFilter(CSSPropertyID propertyID, const FilterOperations& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const FilterOperations&))
        : PropertyWrapper<const FilterOperations&>(propertyID, getter, setter)
    {
    }

    bool animationIsAccelerated() const override
    {
        return property() == CSSPropertyFilter
#if ENABLE(FILTERS_LEVEL_2)
            || property() == CSSPropertyWebkitBackdropFilter
#endif
            ;
    }

    void blend(const CSSPropertyBlendingClient* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        (dst->*m_setter)(blendFunc(anim, (a->*PropertyWrapperGetter<const FilterOperations&>::m_getter)(), (b->*PropertyWrapperGetter<const FilterOperations&>::m_getter)(), progress, property()));
    }
};

static inline size_t shadowListLength(const ShadowData* shadow)
{
    size_t count;
    for (count = 0; shadow; shadow = shadow->next())
        ++count;
    return count;
}

static inline const ShadowData* shadowForBlending(const ShadowData* srcShadow, const ShadowData* otherShadow)
{
    static NeverDestroyed<ShadowData> defaultShadowData(LayoutPoint(), 0, 0, ShadowStyle::Normal, false, Color::transparentBlack);
    static NeverDestroyed<ShadowData> defaultInsetShadowData(LayoutPoint(), 0, 0, ShadowStyle::Inset, false, Color::transparentBlack);
    static NeverDestroyed<ShadowData> defaultWebKitBoxShadowData(LayoutPoint(), 0, 0, ShadowStyle::Normal, true, Color::transparentBlack);
    static NeverDestroyed<ShadowData> defaultInsetWebKitBoxShadowData(LayoutPoint(), 0, 0, ShadowStyle::Inset, true, Color::transparentBlack);

    if (srcShadow)
        return srcShadow;

    if (otherShadow->style() == ShadowStyle::Inset)
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

    void blend(const CSSPropertyBlendingClient* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
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
    std::unique_ptr<ShadowData> blendSimpleOrMatchedShadowLists(const CSSPropertyBlendingClient* anim, double progress, const ShadowData* shadowA, const ShadowData* shadowB) const
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

    std::unique_ptr<ShadowData> blendMismatchedShadowLists(const CSSPropertyBlendingClient* anim, double progress, const ShadowData* shadowA, const ShadowData* shadowB, int fromLength, int toLength) const
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
    PropertyWrapperMaybeInvalidColor(CSSPropertyID prop, const Color& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Color&))
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

    void blend(const CSSPropertyBlendingClient* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
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
    const Color& (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(const Color&);
};


enum MaybeInvalidColorTag { MaybeInvalidColor };
class PropertyWrapperVisitedAffectedColor : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperVisitedAffectedColor(CSSPropertyID prop, const Color& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Color&), const Color& (RenderStyle::*visitedGetter)() const, void (RenderStyle::*visitedSetter)(const Color&))
        : AnimationPropertyWrapperBase(prop)
        , m_wrapper(makeUnique<PropertyWrapperColor>(prop, getter, setter))
        , m_visitedWrapper(makeUnique<PropertyWrapperColor>(prop, visitedGetter, visitedSetter))
    {
    }
    PropertyWrapperVisitedAffectedColor(CSSPropertyID prop, MaybeInvalidColorTag, const Color& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Color&), const Color& (RenderStyle::*visitedGetter)() const, void (RenderStyle::*visitedSetter)(const Color&))
        : AnimationPropertyWrapperBase(prop)
        , m_wrapper(makeUnique<PropertyWrapperMaybeInvalidColor>(prop, getter, setter))
        , m_visitedWrapper(makeUnique<PropertyWrapperMaybeInvalidColor>(prop, visitedGetter, visitedSetter))
    {
    }
    bool equals(const RenderStyle* a, const RenderStyle* b) const override
    {
        return m_wrapper->equals(a, b) && m_visitedWrapper->equals(a, b);
    }
    void blend(const CSSPropertyBlendingClient* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
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
    FillLayerAnimationPropertyWrapperBase(CSSPropertyID property)
        : m_property(property)
    {
    }
    virtual ~FillLayerAnimationPropertyWrapperBase() = default;

    CSSPropertyID property() const { return m_property; }

    virtual bool equals(const FillLayer*, const FillLayer*) const = 0;
    virtual void blend(const CSSPropertyBlendingClient*, FillLayer*, const FillLayer*, const FillLayer*, double) const = 0;

#if !LOG_DISABLED
    virtual void logBlend(const FillLayer* result, const FillLayer*, const FillLayer*, double) const = 0;
#endif
private:
    CSSPropertyID m_property;
};

template <typename T>
class FillLayerPropertyWrapperGetter : public FillLayerAnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(FillLayerPropertyWrapperGetter);
public:
    FillLayerPropertyWrapperGetter(CSSPropertyID property, T (FillLayer::*getter)() const)
        : FillLayerAnimationPropertyWrapperBase(property)
        , m_getter(getter)
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

    T value(const FillLayer* layer) const
    {
        return (layer->*m_getter)();
    }

#if !LOG_DISABLED
    void logBlend(const FillLayer* result, const FillLayer* a, const FillLayer* b, double progress) const override
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << getPropertyName(property()) << " from " << value(a) << " to " << value(b) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(result));
    }
#endif

protected:
    T (FillLayer::*m_getter)() const;
};

template <typename T>
class FillLayerPropertyWrapper : public FillLayerPropertyWrapperGetter<const T&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FillLayerPropertyWrapper(CSSPropertyID property, const T& (FillLayer::*getter)() const, void (FillLayer::*setter)(T))
        : FillLayerPropertyWrapperGetter<const T&>(property, getter)
        , m_setter(setter)
    {
    }

    void blend(const CSSPropertyBlendingClient* anim, FillLayer* dst, const FillLayer* a, const FillLayer* b, double progress) const override
    {
        (dst->*m_setter)(blendFunc(anim, (a->*FillLayerPropertyWrapperGetter<const T&>::m_getter)(), (b->*FillLayerPropertyWrapperGetter<const T&>::m_getter)(), progress));
    }

#if !LOG_DISABLED
    void logBlend(const FillLayer* result, const FillLayer* a, const FillLayer* b, double progress) const override
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << getPropertyName(FillLayerPropertyWrapperGetter<const T&>::property())
            << " from " << FillLayerPropertyWrapperGetter<const T&>::value(a)
            << " to " << FillLayerPropertyWrapperGetter<const T&>::value(b)
            << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << FillLayerPropertyWrapperGetter<const T&>::value(result));
    }
#endif

protected:
    void (FillLayer::*m_setter)(T);
};

class FillLayerPositionPropertyWrapper : public FillLayerPropertyWrapperGetter<const Length&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FillLayerPositionPropertyWrapper(CSSPropertyID property, const Length& (FillLayer::*lengthGetter)() const, void (FillLayer::*lengthSetter)(Length), Edge (FillLayer::*originGetter)() const, void (FillLayer::*originSetter)(Edge), Edge farEdge)
        : FillLayerPropertyWrapperGetter<const Length&>(property, lengthGetter)
        , m_lengthSetter(lengthSetter)
        , m_originGetter(originGetter)
        , m_originSetter(originSetter)
        , m_farEdge(farEdge)
    {
    }

    bool equals(const FillLayer* a, const FillLayer* b) const override
    {
        if (a == b)
            return true;
        if (!a || !b)
            return false;

        Length fromLength = (a->*FillLayerPropertyWrapperGetter<const Length&>::m_getter)();
        Length toLength = (b->*FillLayerPropertyWrapperGetter<const Length&>::m_getter)();
        
        Edge fromEdge = (a->*m_originGetter)();
        Edge toEdge = (b->*m_originGetter)();
        
        return fromLength == toLength && fromEdge == toEdge;
    }

    void blend(const CSSPropertyBlendingClient* anim, FillLayer* dst, const FillLayer* a, const FillLayer* b, double progress) const override
    {
        Length fromLength = (a->*FillLayerPropertyWrapperGetter<const Length&>::m_getter)();
        Length toLength = (b->*FillLayerPropertyWrapperGetter<const Length&>::m_getter)();
        
        Edge fromEdge = (a->*m_originGetter)();
        Edge toEdge = (b->*m_originGetter)();
        
        if (fromEdge != toEdge) {
            // Convert the right/bottom into a calc expression,
            if (fromEdge == m_farEdge)
                fromLength = convertTo100PercentMinusLength(fromLength);
            else if (toEdge == m_farEdge) {
                toLength = convertTo100PercentMinusLength(toLength);
                (dst->*m_originSetter)(fromEdge); // Now we have a calc(100% - l), it's relative to the left/top edge.
            }
        }

        (dst->*m_lengthSetter)(blendFunc(anim, fromLength, toLength, progress));
    }

#if !LOG_DISABLED
    void logBlend(const FillLayer* result, const FillLayer* a, const FillLayer* b, double progress) const override
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << getPropertyName(property()) << " from " << value(a) << " to " << value(b) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(result));
    }
#endif

protected:
    void (FillLayer::*m_lengthSetter)(Length);
    Edge (FillLayer::*m_originGetter)() const;
    void (FillLayer::*m_originSetter)(Edge);
    Edge m_farEdge;
};

template <typename T>
class FillLayerRefCountedPropertyWrapper : public FillLayerPropertyWrapperGetter<T*> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FillLayerRefCountedPropertyWrapper(CSSPropertyID property, T* (FillLayer::*getter)() const, void (FillLayer::*setter)(RefPtr<T>&&))
        : FillLayerPropertyWrapperGetter<T*>(property, getter)
        , m_setter(setter)
    {
    }

    void blend(const CSSPropertyBlendingClient* anim, FillLayer* dst, const FillLayer* a, const FillLayer* b, double progress) const override
    {
        (dst->*m_setter)(blendFunc(anim, (a->*FillLayerPropertyWrapperGetter<T*>::m_getter)(), (b->*FillLayerPropertyWrapperGetter<T*>::m_getter)(), progress));
    }

#if !LOG_DISABLED
    void logBlend(const FillLayer* result, const FillLayer* a, const FillLayer* b, double progress) const override
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << getPropertyName(FillLayerPropertyWrapperGetter<T*>::property())
            << " from " << FillLayerPropertyWrapperGetter<T*>::value(a)
            << " to " << FillLayerPropertyWrapperGetter<T*>::value(b)
            << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << FillLayerPropertyWrapperGetter<T*>::value(result));
    }
#endif

protected:
    void (FillLayer::*m_setter)(RefPtr<T>&&);
};

class FillLayerStyleImagePropertyWrapper : public FillLayerRefCountedPropertyWrapper<StyleImage> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FillLayerStyleImagePropertyWrapper(CSSPropertyID property, StyleImage* (FillLayer::*getter)() const, void (FillLayer::*setter)(RefPtr<StyleImage>&&))
        : FillLayerRefCountedPropertyWrapper<StyleImage>(property, getter, setter)
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

#if !LOG_DISABLED
    void logBlend(const FillLayer* result, const FillLayer* a, const FillLayer* b, double progress) const override
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << getPropertyName(property()) << " from " << value(a) << " to " << value(b) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(result));
    }
#endif
};

class FillLayersPropertyWrapper : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    typedef const FillLayer& (RenderStyle::*LayersGetter)() const;
    typedef FillLayer& (RenderStyle::*LayersAccessor)();

    FillLayersPropertyWrapper(CSSPropertyID property, LayersGetter getter, LayersAccessor accessor)
        : AnimationPropertyWrapperBase(property)
        , m_layersGetter(getter)
        , m_layersAccessor(accessor)
    {
        switch (property) {
        case CSSPropertyBackgroundPositionX:
        case CSSPropertyWebkitMaskPositionX:
            m_fillLayerPropertyWrapper = makeUnique<FillLayerPositionPropertyWrapper>(property, &FillLayer::xPosition, &FillLayer::setXPosition, &FillLayer::backgroundXOrigin, &FillLayer::setBackgroundXOrigin, Edge::Right);
            break;
        case CSSPropertyBackgroundPositionY:
        case CSSPropertyWebkitMaskPositionY:
            m_fillLayerPropertyWrapper = makeUnique<FillLayerPositionPropertyWrapper>(property, &FillLayer::yPosition, &FillLayer::setYPosition, &FillLayer::backgroundYOrigin, &FillLayer::setBackgroundYOrigin, Edge::Bottom);
            break;
        case CSSPropertyBackgroundSize:
        case CSSPropertyWebkitBackgroundSize:
        case CSSPropertyWebkitMaskSize:
            m_fillLayerPropertyWrapper = makeUnique<FillLayerPropertyWrapper<LengthSize>>(property, &FillLayer::sizeLength, &FillLayer::setSizeLength);
            break;
        case CSSPropertyBackgroundImage:
            m_fillLayerPropertyWrapper = makeUnique<FillLayerStyleImagePropertyWrapper>(property, &FillLayer::image, &FillLayer::setImage);
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

        auto* fromLayer = &(a->*m_layersGetter)();
        auto* toLayer = &(b->*m_layersGetter)();

        while (fromLayer && toLayer) {
            if (!m_fillLayerPropertyWrapper->equals(fromLayer, toLayer))
                return false;

            fromLayer = fromLayer->next();
            toLayer = toLayer->next();
        }

        return true;
    }

    void blend(const CSSPropertyBlendingClient* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        auto* aLayer = &(a->*m_layersGetter)();
        auto* bLayer = &(b->*m_layersGetter)();
        auto* dstLayer = &(dst->*m_layersAccessor)();

        while (aLayer && bLayer && dstLayer) {
            m_fillLayerPropertyWrapper->blend(anim, dstLayer, aLayer, bLayer, progress);
            aLayer = aLayer->next();
            bLayer = bLayer->next();
            dstLayer = dstLayer->next();
        }
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle* from, const RenderStyle* to, const RenderStyle* result, double progress) const final
    {
        auto* aLayer = &(from->*m_layersGetter)();
        auto* bLayer = &(to->*m_layersGetter)();
        auto* dstLayer = &(result->*m_layersGetter)();

        while (aLayer && bLayer && dstLayer) {
            m_fillLayerPropertyWrapper->logBlend(dstLayer, aLayer, bLayer, progress);
            aLayer = aLayer->next();
            bLayer = bLayer->next();
            dstLayer = dstLayer->next();
        }
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

    void blend(const CSSPropertyBlendingClient* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        for (auto& wrapper : m_propertyWrappers)
            wrapper->blend(anim, dst, a, b, progress);
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle* a, const RenderStyle* b, const RenderStyle* dst, double progress) const final
    {
        for (auto& wrapper : m_propertyWrappers)
            wrapper->logBlend(a, b, dst, progress);
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

    void blend(const CSSPropertyBlendingClient* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
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
    PropertyWrapperSVGPaint(CSSPropertyID prop, SVGPaintType (RenderStyle::*paintTypeGetter)() const, Color (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Color&))
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
        if ((a->*m_paintTypeGetter)() == SVGPaintType::RGBColor) {
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

    void blend(const CSSPropertyBlendingClient* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const override
    {
        if ((a->*m_paintTypeGetter)() != SVGPaintType::RGBColor
            || (b->*m_paintTypeGetter)() != SVGPaintType::RGBColor)
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
    SVGPaintType (RenderStyle::*m_paintTypeGetter)() const;
    Color (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(const Color&);
};

class PropertyWrapperFontStyle : public PropertyWrapper<Optional<FontSelectionValue>> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperFontStyle()
        : PropertyWrapper<Optional<FontSelectionValue>>(CSSPropertyFontStyle, &RenderStyle::fontItalic, &RenderStyle::setFontItalic)
    {
    }

    bool canInterpolate(const RenderStyle* from, const RenderStyle* to) const override
    {
        return from->fontItalic() && to->fontItalic() && from->fontDescription().fontStyleAxis() == FontStyleAxis::slnt && to->fontDescription().fontStyleAxis() == FontStyleAxis::slnt;
    }

    void blend(const CSSPropertyBlendingClient* anim, RenderStyle* dst, const RenderStyle* from, const RenderStyle* to, double progress) const override
    {
        auto discrete = !canInterpolate(from, to);

        auto blendedStyleAxis = FontStyleAxis::slnt;
        if (discrete)
            blendedStyleAxis = (progress < 0.5 ? from : to)->fontDescription().fontStyleAxis();

        auto fromFontItalic = from->fontItalic();
        auto toFontItalic = to->fontItalic();
        auto blendedFontItalic = progress < 0.5 ? fromFontItalic : toFontItalic;
        if (!discrete)
            blendedFontItalic = blendFunc(anim, fromFontItalic, toFontItalic, progress);

        FontSelector* currentFontSelector = dst->fontCascade().fontSelector();
        auto description = dst->fontDescription();
        description.setItalic(blendedFontItalic);
        description.setFontStyleAxis(blendedStyleAxis);
        dst->setFontDescription(WTFMove(description));
        dst->fontCascade().update(currentFontSelector);
    }
};

class CSSPropertyAnimationWrapperMap {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static CSSPropertyAnimationWrapperMap& singleton()
    {
        // FIXME: This data is never destroyed. Maybe we should ref count it and toss it when the last CSSAnimationController is destroyed?
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
        new LengthPropertyWrapper(CSSPropertyLeft, &RenderStyle::left, &RenderStyle::setLeft),
        new LengthPropertyWrapper(CSSPropertyRight, &RenderStyle::right, &RenderStyle::setRight),
        new LengthPropertyWrapper(CSSPropertyTop, &RenderStyle::top, &RenderStyle::setTop),
        new LengthPropertyWrapper(CSSPropertyBottom, &RenderStyle::bottom, &RenderStyle::setBottom),

        new NonNegativeLengthPropertyWrapper(CSSPropertyWidth, &RenderStyle::width, &RenderStyle::setWidth),
        new NonNegativeLengthPropertyWrapper(CSSPropertyMinWidth, &RenderStyle::minWidth, &RenderStyle::setMinWidth),
        new LengthPropertyWrapper(CSSPropertyMaxWidth, &RenderStyle::maxWidth, &RenderStyle::setMaxWidth),

        new NonNegativeLengthPropertyWrapper(CSSPropertyHeight, &RenderStyle::height, &RenderStyle::setHeight),
        new NonNegativeLengthPropertyWrapper(CSSPropertyMinHeight, &RenderStyle::minHeight, &RenderStyle::setMinHeight),
        new LengthPropertyWrapper(CSSPropertyMaxHeight, &RenderStyle::maxHeight, &RenderStyle::setMaxHeight),

        new PropertyWrapperFlex(),

        new PropertyWrapper<float>(CSSPropertyBorderLeftWidth, &RenderStyle::borderLeftWidth, &RenderStyle::setBorderLeftWidth),
        new PropertyWrapper<float>(CSSPropertyBorderRightWidth, &RenderStyle::borderRightWidth, &RenderStyle::setBorderRightWidth),
        new PropertyWrapper<float>(CSSPropertyBorderTopWidth, &RenderStyle::borderTopWidth, &RenderStyle::setBorderTopWidth),
        new PropertyWrapper<float>(CSSPropertyBorderBottomWidth, &RenderStyle::borderBottomWidth, &RenderStyle::setBorderBottomWidth),
        new LengthPropertyWrapper(CSSPropertyMarginLeft, &RenderStyle::marginLeft, &RenderStyle::setMarginLeft),
        new LengthPropertyWrapper(CSSPropertyMarginRight, &RenderStyle::marginRight, &RenderStyle::setMarginRight),
        new LengthPropertyWrapper(CSSPropertyMarginTop, &RenderStyle::marginTop, &RenderStyle::setMarginTop),
        new LengthPropertyWrapper(CSSPropertyMarginBottom, &RenderStyle::marginBottom, &RenderStyle::setMarginBottom),
        new NonNegativeLengthPropertyWrapper(CSSPropertyPaddingLeft, &RenderStyle::paddingLeft, &RenderStyle::setPaddingLeft),
        new NonNegativeLengthPropertyWrapper(CSSPropertyPaddingRight, &RenderStyle::paddingRight, &RenderStyle::setPaddingRight),
        new NonNegativeLengthPropertyWrapper(CSSPropertyPaddingTop, &RenderStyle::paddingTop, &RenderStyle::setPaddingTop),
        new NonNegativeLengthPropertyWrapper(CSSPropertyPaddingBottom, &RenderStyle::paddingBottom, &RenderStyle::setPaddingBottom),

        new PropertyWrapperVisitedAffectedColor(CSSPropertyCaretColor, &RenderStyle::caretColor, &RenderStyle::setCaretColor, &RenderStyle::visitedLinkCaretColor, &RenderStyle::setVisitedLinkCaretColor),

        new PropertyWrapperVisitedAffectedColor(CSSPropertyColor, &RenderStyle::color, &RenderStyle::setColor, &RenderStyle::visitedLinkColor, &RenderStyle::setVisitedLinkColor),

        new PropertyWrapperVisitedAffectedColor(CSSPropertyBackgroundColor, MaybeInvalidColor, &RenderStyle::backgroundColor, &RenderStyle::setBackgroundColor, &RenderStyle::visitedLinkBackgroundColor, &RenderStyle::setVisitedLinkBackgroundColor),

        new FillLayersPropertyWrapper(CSSPropertyBackgroundImage, &RenderStyle::backgroundLayers, &RenderStyle::ensureBackgroundLayers),
        new StyleImagePropertyWrapper(CSSPropertyListStyleImage, &RenderStyle::listStyleImage, &RenderStyle::setListStyleImage),
        new StyleImagePropertyWrapper(CSSPropertyWebkitMaskImage, &RenderStyle::maskImage, &RenderStyle::setMaskImage),

        new StyleImagePropertyWrapper(CSSPropertyBorderImageSource, &RenderStyle::borderImageSource, &RenderStyle::setBorderImageSource),
        new LengthVariantPropertyWrapper<LengthBox>(CSSPropertyBorderImageSlice, &RenderStyle::borderImageSlices, &RenderStyle::setBorderImageSlices),
        new LengthVariantPropertyWrapper<LengthBox>(CSSPropertyBorderImageWidth, &RenderStyle::borderImageWidth, &RenderStyle::setBorderImageWidth),
        new LengthVariantPropertyWrapper<LengthBox>(CSSPropertyBorderImageOutset, &RenderStyle::borderImageOutset, &RenderStyle::setBorderImageOutset),

        new StyleImagePropertyWrapper(CSSPropertyWebkitMaskBoxImageSource, &RenderStyle::maskBoxImageSource, &RenderStyle::setMaskBoxImageSource),
        new PropertyWrapper<const NinePieceImage&>(CSSPropertyWebkitMaskBoxImage, &RenderStyle::maskBoxImage, &RenderStyle::setMaskBoxImage),

        new FillLayersPropertyWrapper(CSSPropertyBackgroundPositionX, &RenderStyle::backgroundLayers, &RenderStyle::ensureBackgroundLayers),
        new FillLayersPropertyWrapper(CSSPropertyBackgroundPositionY, &RenderStyle::backgroundLayers, &RenderStyle::ensureBackgroundLayers),
        new FillLayersPropertyWrapper(CSSPropertyBackgroundSize, &RenderStyle::backgroundLayers, &RenderStyle::ensureBackgroundLayers),
        new FillLayersPropertyWrapper(CSSPropertyWebkitBackgroundSize, &RenderStyle::backgroundLayers, &RenderStyle::ensureBackgroundLayers),

        new FillLayersPropertyWrapper(CSSPropertyWebkitMaskPositionX, &RenderStyle::maskLayers, &RenderStyle::ensureMaskLayers),
        new FillLayersPropertyWrapper(CSSPropertyWebkitMaskPositionY, &RenderStyle::maskLayers, &RenderStyle::ensureMaskLayers),
        new FillLayersPropertyWrapper(CSSPropertyWebkitMaskSize, &RenderStyle::maskLayers, &RenderStyle::ensureMaskLayers),

        new PropertyWrapper<float>(CSSPropertyFontSize, &RenderStyle::computedFontSize, &RenderStyle::setFontSize),
        new PropertyWrapper<unsigned short>(CSSPropertyColumnRuleWidth, &RenderStyle::columnRuleWidth, &RenderStyle::setColumnRuleWidth),
        new LengthVariantPropertyWrapper<GapLength>(CSSPropertyColumnGap, &RenderStyle::columnGap, &RenderStyle::setColumnGap),
        new LengthVariantPropertyWrapper<GapLength>(CSSPropertyRowGap, &RenderStyle::rowGap, &RenderStyle::setRowGap),
        new PropertyWrapper<unsigned short>(CSSPropertyColumnCount, &RenderStyle::columnCount, &RenderStyle::setColumnCount),
        new PropertyWrapper<float>(CSSPropertyColumnWidth, &RenderStyle::columnWidth, &RenderStyle::setColumnWidth),
        new PropertyWrapper<float>(CSSPropertyWebkitBorderHorizontalSpacing, &RenderStyle::horizontalBorderSpacing, &RenderStyle::setHorizontalBorderSpacing),
        new PropertyWrapper<float>(CSSPropertyWebkitBorderVerticalSpacing, &RenderStyle::verticalBorderSpacing, &RenderStyle::setVerticalBorderSpacing),
        new PropertyWrapper<int>(CSSPropertyZIndex, &RenderStyle::specifiedZIndex, &RenderStyle::setSpecifiedZIndex),
        new PropertyWrapper<short>(CSSPropertyOrphans, &RenderStyle::orphans, &RenderStyle::setOrphans),
        new PropertyWrapper<short>(CSSPropertyWidows, &RenderStyle::widows, &RenderStyle::setWidows),
        new LengthPropertyWrapper(CSSPropertyLineHeight, &RenderStyle::specifiedLineHeight, &RenderStyle::setLineHeight),
        new PropertyWrapper<float>(CSSPropertyOutlineOffset, &RenderStyle::outlineOffset, &RenderStyle::setOutlineOffset),
        new PropertyWrapper<float>(CSSPropertyOutlineWidth, &RenderStyle::outlineWidth, &RenderStyle::setOutlineWidth),
        new PropertyWrapper<float>(CSSPropertyLetterSpacing, &RenderStyle::letterSpacing, &RenderStyle::setLetterSpacing),
        new LengthPropertyWrapper(CSSPropertyWordSpacing, &RenderStyle::wordSpacing, &RenderStyle::setWordSpacing),
        new LengthPropertyWrapper(CSSPropertyTextIndent, &RenderStyle::textIndent, &RenderStyle::setTextIndent),

        new PropertyWrapper<float>(CSSPropertyPerspective, &RenderStyle::perspective, &RenderStyle::setPerspective),
        new LengthPropertyWrapper(CSSPropertyPerspectiveOriginX, &RenderStyle::perspectiveOriginX, &RenderStyle::setPerspectiveOriginX),
        new LengthPropertyWrapper(CSSPropertyPerspectiveOriginY, &RenderStyle::perspectiveOriginY, &RenderStyle::setPerspectiveOriginY),
        new LengthPropertyWrapper(CSSPropertyTransformOriginX, &RenderStyle::transformOriginX, &RenderStyle::setTransformOriginX),
        new LengthPropertyWrapper(CSSPropertyTransformOriginY, &RenderStyle::transformOriginY, &RenderStyle::setTransformOriginY),
        new PropertyWrapper<float>(CSSPropertyTransformOriginZ, &RenderStyle::transformOriginZ, &RenderStyle::setTransformOriginZ),
        new LengthVariantPropertyWrapper<LengthSize>(CSSPropertyBorderTopLeftRadius, &RenderStyle::borderTopLeftRadius, &RenderStyle::setBorderTopLeftRadius),
        new LengthVariantPropertyWrapper<LengthSize>(CSSPropertyBorderTopRightRadius, &RenderStyle::borderTopRightRadius, &RenderStyle::setBorderTopRightRadius),
        new LengthVariantPropertyWrapper<LengthSize>(CSSPropertyBorderBottomLeftRadius, &RenderStyle::borderBottomLeftRadius, &RenderStyle::setBorderBottomLeftRadius),
        new LengthVariantPropertyWrapper<LengthSize>(CSSPropertyBorderBottomRightRadius, &RenderStyle::borderBottomRightRadius, &RenderStyle::setBorderBottomRightRadius),
        new PropertyWrapper<Visibility>(CSSPropertyVisibility, &RenderStyle::visibility, &RenderStyle::setVisibility),
        new PropertyWrapper<float>(CSSPropertyZoom, &RenderStyle::zoom, &RenderStyle::setZoomWithoutReturnValue),

        new LengthVariantPropertyWrapper<LengthBox>(CSSPropertyClip, &RenderStyle::clip, &RenderStyle::setClip),

        new PropertyWrapperAcceleratedOpacity(),
        new PropertyWrapperAcceleratedTransform(),

        new PropertyWrapperScale,
        new PropertyWrapperRotate,
        new PropertyWrapperTranslate,

        new PropertyWrapperFilter(CSSPropertyFilter, &RenderStyle::filter, &RenderStyle::setFilter),
#if ENABLE(FILTERS_LEVEL_2)
        new PropertyWrapperFilter(CSSPropertyWebkitBackdropFilter, &RenderStyle::backdropFilter, &RenderStyle::setBackdropFilter),
#endif
        new PropertyWrapperFilter(CSSPropertyAppleColorFilter, &RenderStyle::appleColorFilter, &RenderStyle::setAppleColorFilter),

        new PropertyWrapperClipPath(CSSPropertyClipPath, &RenderStyle::clipPath, &RenderStyle::setClipPath),

        new PropertyWrapperShape(CSSPropertyShapeOutside, &RenderStyle::shapeOutside, &RenderStyle::setShapeOutside),
        new NonNegativeLengthPropertyWrapper(CSSPropertyShapeMargin, &RenderStyle::shapeMargin, &RenderStyle::setShapeMargin),
        new PropertyWrapper<float>(CSSPropertyShapeImageThreshold, &RenderStyle::shapeImageThreshold, &RenderStyle::setShapeImageThreshold),

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
        new PropertyWrapper<Vector<SVGLengthValue>>(CSSPropertyStrokeDasharray, &RenderStyle::strokeDashArray, &RenderStyle::setStrokeDashArray),
        new PropertyWrapper<float>(CSSPropertyStrokeMiterlimit, &RenderStyle::strokeMiterLimit, &RenderStyle::setStrokeMiterLimit),

        new LengthPropertyWrapper(CSSPropertyCx, &RenderStyle::cx, &RenderStyle::setCx),
        new LengthPropertyWrapper(CSSPropertyCy, &RenderStyle::cy, &RenderStyle::setCy),
        new LengthPropertyWrapper(CSSPropertyR, &RenderStyle::r, &RenderStyle::setR),
        new LengthPropertyWrapper(CSSPropertyRx, &RenderStyle::rx, &RenderStyle::setRx),
        new LengthPropertyWrapper(CSSPropertyRy, &RenderStyle::ry, &RenderStyle::setRy),
        new LengthPropertyWrapper(CSSPropertyStrokeDashoffset, &RenderStyle::strokeDashOffset, &RenderStyle::setStrokeDashOffset),
        new LengthPropertyWrapper(CSSPropertyStrokeWidth, &RenderStyle::strokeWidth, &RenderStyle::setStrokeWidth),
        new LengthPropertyWrapper(CSSPropertyX, &RenderStyle::x, &RenderStyle::setX),
        new LengthPropertyWrapper(CSSPropertyY, &RenderStyle::y, &RenderStyle::setY),

        new PropertyWrapper<float>(CSSPropertyFloodOpacity, &RenderStyle::floodOpacity, &RenderStyle::setFloodOpacity),
        new PropertyWrapperMaybeInvalidColor(CSSPropertyFloodColor, &RenderStyle::floodColor, &RenderStyle::setFloodColor),

        new PropertyWrapper<float>(CSSPropertyStopOpacity, &RenderStyle::stopOpacity, &RenderStyle::setStopOpacity),
        new PropertyWrapperMaybeInvalidColor(CSSPropertyStopColor, &RenderStyle::stopColor, &RenderStyle::setStopColor),

        new PropertyWrapperMaybeInvalidColor(CSSPropertyLightingColor, &RenderStyle::lightingColor, &RenderStyle::setLightingColor),

        new PropertyWrapper<SVGLengthValue>(CSSPropertyBaselineShift, &RenderStyle::baselineShiftValue, &RenderStyle::setBaselineShiftValue),
        new PropertyWrapper<SVGLengthValue>(CSSPropertyKerning, &RenderStyle::kerning, &RenderStyle::setKerning),
#if ENABLE(VARIATION_FONTS)
        new PropertyWrapperFontVariationSettings(CSSPropertyFontVariationSettings, &RenderStyle::fontVariationSettings, &RenderStyle::setFontVariationSettings),
#endif
        new PropertyWrapper<FontSelectionValue>(CSSPropertyFontWeight, &RenderStyle::fontWeight, &RenderStyle::setFontWeight),
        new PropertyWrapper<FontSelectionValue>(CSSPropertyFontStretch, &RenderStyle::fontStretch, &RenderStyle::setFontStretch),
        new PropertyWrapperFontStyle(),
        new PropertyWrapper<TextDecorationThickness>(CSSPropertyTextDecorationThickness, &RenderStyle::textDecorationThickness, &RenderStyle::setTextDecorationThickness),
        new PropertyWrapper<TextUnderlineOffset>(CSSPropertyTextUnderlineOffset, &RenderStyle::textUnderlineOffset, &RenderStyle::setTextUnderlineOffset),
        new PropertyWrapperVisitedAffectedColor(CSSPropertyTextDecorationColor, &RenderStyle::textDecorationColor, &RenderStyle::setTextDecorationColor, &RenderStyle::visitedLinkTextDecorationColor, &RenderStyle::setVisitedLinkTextDecorationColor),

        new LengthPropertyWrapper(CSSPropertyFlexBasis, &RenderStyle::flexBasis, &RenderStyle::setFlexBasis),
        new PropertyWrapper<float>(CSSPropertyFlexGrow, &RenderStyle::flexGrow, &RenderStyle::setFlexGrow),
        new PropertyWrapper<float>(CSSPropertyFlexShrink, &RenderStyle::flexShrink, &RenderStyle::setFlexShrink),
        new PropertyWrapper<int>(CSSPropertyOrder, &RenderStyle::order, &RenderStyle::setOrder),

        // FIXME: The following properties are currently not animatable but should be:
        // background-attachment, background-blend-mode,background-clip, background-origin,
        // background-repeat, border-image-repeat, clip-rule, color-interpolation,
        // color-interpolation-filters, counter-increment, counter-reset, dominant-baseline,
        // fill-rule, font-family, font-feature-settings, font-kerning, font-language-override,
        // font-synthesis, font-variant-alternates, font-variant-caps, font-variant-east-asian,
        // font-variant-ligatures, font-variant-numeric, font-variant-position, grid-template-areas,
        // ime-mode, marker-end, marker-mid, marker-start, mask, mask-clip, mask-composite, mask-image,
        // mask-mode, mask-origin, mask-repeat, mask-type, offset-distance, perspective-origin, quotes,
        // ruby-align, scroll-behavior, shape-rendering, stroke-linecap, stroke-linejoin, text-align-last,
        // text-anchor, text-emphasis-style, text-rendering, vector-effect
        new DiscretePropertyWrapper<const StyleContentAlignmentData&>(CSSPropertyAlignContent, &RenderStyle::alignContent, &RenderStyle::setAlignContent),
        new DiscretePropertyWrapper<const StyleSelfAlignmentData&>(CSSPropertyAlignItems, &RenderStyle::alignItems, &RenderStyle::setAlignItems),
        new DiscretePropertyWrapper<const StyleSelfAlignmentData&>(CSSPropertyAlignSelf, &RenderStyle::alignSelf, &RenderStyle::setAlignSelf),
        new DiscretePropertyWrapper<BackfaceVisibility>(CSSPropertyWebkitBackfaceVisibility, &RenderStyle::backfaceVisibility, &RenderStyle::setBackfaceVisibility),
        new DiscretePropertyWrapper<BorderStyle>(CSSPropertyBorderBottomStyle, &RenderStyle::borderBottomStyle, &RenderStyle::setBorderBottomStyle),
        new DiscretePropertyWrapper<BorderCollapse>(CSSPropertyBorderCollapse, &RenderStyle::borderCollapse, &RenderStyle::setBorderCollapse),
        new DiscretePropertyWrapper<BorderStyle>(CSSPropertyBorderLeftStyle, &RenderStyle::borderLeftStyle, &RenderStyle::setBorderLeftStyle),
        new DiscretePropertyWrapper<BorderStyle>(CSSPropertyBorderRightStyle, &RenderStyle::borderRightStyle, &RenderStyle::setBorderRightStyle),
        new DiscretePropertyWrapper<BorderStyle>(CSSPropertyBorderTopStyle, &RenderStyle::borderTopStyle, &RenderStyle::setBorderTopStyle),
        new DiscretePropertyWrapper<BoxSizing>(CSSPropertyBoxSizing, &RenderStyle::boxSizing, &RenderStyle::setBoxSizing),
        new DiscretePropertyWrapper<CaptionSide>(CSSPropertyCaptionSide, &RenderStyle::captionSide, &RenderStyle::setCaptionSide),
        new DiscretePropertyWrapper<Clear>(CSSPropertyClear, &RenderStyle::clear, &RenderStyle::setClear),
        new DiscretePropertyWrapper<PrintColorAdjust>(CSSPropertyWebkitPrintColorAdjust, &RenderStyle::printColorAdjust, &RenderStyle::setPrintColorAdjust),
        new DiscretePropertyWrapper<ColumnFill>(CSSPropertyColumnFill, &RenderStyle::columnFill, &RenderStyle::setColumnFill),
        new DiscretePropertyWrapper<BorderStyle>(CSSPropertyColumnRuleStyle, &RenderStyle::columnRuleStyle, &RenderStyle::setColumnRuleStyle),
        new DiscretePropertyWrapper<BorderStyle>(CSSPropertyColumnRuleStyle, &RenderStyle::columnRuleStyle, &RenderStyle::setColumnRuleStyle),
        new DiscretePropertyWrapper<CursorType>(CSSPropertyCursor, &RenderStyle::cursor, &RenderStyle::setCursor),
        new DiscretePropertyWrapper<EmptyCell>(CSSPropertyEmptyCells, &RenderStyle::emptyCells, &RenderStyle::setEmptyCells),
        new DiscretePropertyWrapper<FlexDirection>(CSSPropertyFlexDirection, &RenderStyle::flexDirection, &RenderStyle::setFlexDirection),
        new DiscretePropertyWrapper<FlexWrap>(CSSPropertyFlexWrap, &RenderStyle::flexWrap, &RenderStyle::setFlexWrap),
        new DiscretePropertyWrapper<Float>(CSSPropertyFloat, &RenderStyle::floating, &RenderStyle::setFloating),
        new DiscretePropertyWrapper<const Vector<GridTrackSize>&>(CSSPropertyGridAutoColumns, &RenderStyle::gridAutoColumns, &RenderStyle::setGridAutoColumns),
        new DiscretePropertyWrapper<GridAutoFlow>(CSSPropertyGridAutoFlow, &RenderStyle::gridAutoFlow, &RenderStyle::setGridAutoFlow),
        new DiscretePropertyWrapper<const Vector<GridTrackSize>&>(CSSPropertyGridAutoRows, &RenderStyle::gridAutoRows, &RenderStyle::setGridAutoRows),
        new DiscretePropertyWrapper<const GridPosition&>(CSSPropertyGridColumnEnd, &RenderStyle::gridItemColumnEnd, &RenderStyle::setGridItemColumnEnd),
        new DiscretePropertyWrapper<const GridPosition&>(CSSPropertyGridColumnStart, &RenderStyle::gridItemColumnStart, &RenderStyle::setGridItemColumnStart),
        new DiscretePropertyWrapper<const GridPosition&>(CSSPropertyGridRowEnd, &RenderStyle::gridItemRowEnd, &RenderStyle::setGridItemRowEnd),
        new DiscretePropertyWrapper<const GridPosition&>(CSSPropertyGridRowStart, &RenderStyle::gridItemRowStart, &RenderStyle::setGridItemRowStart),
        new DiscretePropertyWrapper<Hyphens>(CSSPropertyWebkitHyphens, &RenderStyle::hyphens, &RenderStyle::setHyphens),
        new DiscretePropertyWrapper<ImageOrientation>(CSSPropertyImageOrientation, &RenderStyle::imageOrientation, &RenderStyle::setImageOrientation),
        new DiscretePropertyWrapper<const IntSize&>(CSSPropertyWebkitInitialLetter, &RenderStyle::initialLetter, &RenderStyle::setInitialLetter),
        new DiscretePropertyWrapper<const StyleContentAlignmentData&>(CSSPropertyJustifyContent, &RenderStyle::justifyContent, &RenderStyle::setJustifyContent),
        new DiscretePropertyWrapper<const StyleSelfAlignmentData&>(CSSPropertyJustifyItems, &RenderStyle::justifyItems, &RenderStyle::setJustifyItems),
        new DiscretePropertyWrapper<const StyleSelfAlignmentData&>(CSSPropertyJustifySelf, &RenderStyle::justifySelf, &RenderStyle::setJustifySelf),
        new DiscretePropertyWrapper<ListStylePosition>(CSSPropertyListStylePosition, &RenderStyle::listStylePosition, &RenderStyle::setListStylePosition),
        new DiscretePropertyWrapper<ListStyleType>(CSSPropertyListStyleType, &RenderStyle::listStyleType, &RenderStyle::setListStyleType),
        new DiscretePropertyWrapper<ObjectFit>(CSSPropertyObjectFit, &RenderStyle::objectFit, &RenderStyle::setObjectFit),
        new DiscretePropertyWrapper<BorderStyle>(CSSPropertyOutlineStyle, &RenderStyle::outlineStyle, &RenderStyle::setOutlineStyle),
        new DiscretePropertyWrapper<OverflowWrap>(CSSPropertyOverflowWrap, &RenderStyle::overflowWrap, &RenderStyle::setOverflowWrap),
        new DiscretePropertyWrapper<Overflow>(CSSPropertyOverflowX, &RenderStyle::overflowX, &RenderStyle::setOverflowX),
        new DiscretePropertyWrapper<Overflow>(CSSPropertyOverflowY, &RenderStyle::overflowY, &RenderStyle::setOverflowY),
        new DiscretePropertyWrapper<BreakBetween>(CSSPropertyPageBreakAfter, &RenderStyle::breakAfter, &RenderStyle::setBreakAfter),
        new DiscretePropertyWrapper<BreakBetween>(CSSPropertyPageBreakBefore, &RenderStyle::breakBefore, &RenderStyle::setBreakBefore),
        new DiscretePropertyWrapper<BreakInside>(CSSPropertyPageBreakInside, &RenderStyle::breakInside, &RenderStyle::setBreakInside),
        new DiscretePropertyWrapper<PaintOrder>(CSSPropertyPaintOrder, &RenderStyle::paintOrder, &RenderStyle::setPaintOrder),
        new DiscretePropertyWrapper<PointerEvents>(CSSPropertyPointerEvents, &RenderStyle::pointerEvents, &RenderStyle::setPointerEvents),
        new DiscretePropertyWrapper<PositionType>(CSSPropertyPosition, &RenderStyle::position, &RenderStyle::setPosition),
        new DiscretePropertyWrapper<Resize>(CSSPropertyResize, &RenderStyle::resize, &RenderStyle::setResize),
        new DiscretePropertyWrapper<RubyPosition>(CSSPropertyWebkitRubyPosition, &RenderStyle::rubyPosition, &RenderStyle::setRubyPosition),
        new DiscretePropertyWrapper<TableLayoutType>(CSSPropertyTableLayout, &RenderStyle::tableLayout, &RenderStyle::setTableLayout),
        new DiscretePropertyWrapper<TextAlignMode>(CSSPropertyTextAlign, &RenderStyle::textAlign, &RenderStyle::setTextAlign),
        new DiscretePropertyWrapper<OptionSet<TextDecoration>>(CSSPropertyTextDecorationLine, &RenderStyle::textDecoration, &RenderStyle::setTextDecoration),
        new DiscretePropertyWrapper<TextDecorationStyle>(CSSPropertyTextDecorationStyle, &RenderStyle::textDecorationStyle, &RenderStyle::setTextDecorationStyle),
        new DiscretePropertyWrapper<const Color&>(CSSPropertyWebkitTextEmphasisColor, &RenderStyle::textEmphasisColor, &RenderStyle::setTextEmphasisColor),
        new DiscretePropertyWrapper<OptionSet<TextEmphasisPosition>>(CSSPropertyWebkitTextEmphasisPosition, &RenderStyle::textEmphasisPosition, &RenderStyle::setTextEmphasisPosition),
        new DiscretePropertyWrapper<TextOverflow>(CSSPropertyTextOverflow, &RenderStyle::textOverflow, &RenderStyle::setTextOverflow),
        new DiscretePropertyWrapper<OptionSet<TouchAction>>(CSSPropertyTouchAction, &RenderStyle::touchActions, &RenderStyle::setTouchActions),
        new DiscretePropertyWrapper<TextTransform>(CSSPropertyTextTransform, &RenderStyle::textTransform, &RenderStyle::setTextTransform),
        new DiscretePropertyWrapper<TransformBox>(CSSPropertyTransformBox, &RenderStyle::transformBox, &RenderStyle::setTransformBox),
        new DiscretePropertyWrapper<TransformStyle3D>(CSSPropertyTransformStyle, &RenderStyle::transformStyle3D, &RenderStyle::setTransformStyle3D),
        new DiscretePropertyWrapper<WhiteSpace>(CSSPropertyWhiteSpace, &RenderStyle::whiteSpace, &RenderStyle::setWhiteSpace),
        new DiscretePropertyWrapper<WordBreak>(CSSPropertyWordBreak, &RenderStyle::wordBreak, &RenderStyle::setWordBreak),

#if ENABLE(CSS_BOX_DECORATION_BREAK)
        new DiscretePropertyWrapper<BoxDecorationBreak>(CSSPropertyWebkitBoxDecorationBreak, &RenderStyle::boxDecorationBreak, &RenderStyle::setBoxDecorationBreak),
#endif
#if ENABLE(CSS_COMPOSITING)
        new DiscretePropertyWrapper<Isolation>(CSSPropertyIsolation, &RenderStyle::isolation, &RenderStyle::setIsolation),
        new DiscretePropertyWrapper<BlendMode>(CSSPropertyMixBlendMode, &RenderStyle::blendMode, &RenderStyle::setBlendMode),
#endif
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
        auto shorthand = shorthandForProperty(propertyID);
        if (!shorthand.length())
            continue;

        Vector<AnimationPropertyWrapperBase*> longhandWrappers;
        longhandWrappers.reserveInitialCapacity(shorthand.length());
        for (auto longhand : shorthand) {
            unsigned wrapperIndex = indexFromPropertyID(longhand);
            if (wrapperIndex == cInvalidPropertyWrapperIndex)
                continue;
            ASSERT(m_propertyWrappers[wrapperIndex]);
            longhandWrappers.uncheckedAppend(m_propertyWrappers[wrapperIndex].get());
        }

        m_propertyWrappers.uncheckedAppend(makeUnique<ShorthandPropertyWrapper>(propertyID, WTFMove(longhandWrappers)));
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
void CSSPropertyAnimation::blendProperties(const CSSPropertyBlendingClient* anim, CSSPropertyID prop, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress)
{
    ASSERT(prop != CSSPropertyInvalid);

    AnimationPropertyWrapperBase* wrapper = CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(prop);
    if (wrapper) {
        wrapper->blend(anim, dst, a, b, progress);
#if !LOG_DISABLED
        wrapper->logBlend(a, b, dst, progress);
#endif
    }
}

bool CSSPropertyAnimation::isPropertyAnimatable(CSSPropertyID prop)
{
    return CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(prop);
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

bool CSSPropertyAnimation::canPropertyBeInterpolated(CSSPropertyID prop, const RenderStyle* a, const RenderStyle* b)
{
    AnimationPropertyWrapperBase* wrapper = CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(prop);
    if (wrapper)
        return wrapper->canInterpolate(a, b);
    return false;
}

CSSPropertyID CSSPropertyAnimation::getPropertyAtIndex(int i, Optional<bool>& isShorthand)
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
