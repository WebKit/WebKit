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

#include "AnimationUtilities.h"
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
#include "GridPositionsResolver.h"
#include "IdentityTransformOperation.h"
#include "LengthPoint.h"
#include "Logging.h"
#include "Matrix3DTransformOperation.h"
#include "MatrixTransformOperation.h"
#include "RenderBox.h"
#include "RenderStyle.h"
#include "StyleCachedImage.h"
#include "StyleGeneratedImage.h"
#include "StylePropertyShorthand.h"
#include "StyleResolver.h"
#include "TabSize.h"
#include <algorithm>
#include <memory>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/PointerComparison.h>
#include <wtf/RefCounted.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

struct CSSPropertyBlendingContext : BlendingContext {
    const CSSPropertyBlendingClient* client { nullptr };

    CSSPropertyBlendingContext(double progress, bool isDiscrete, const CSSPropertyBlendingClient* client)
        : BlendingContext(progress, isDiscrete)
        , client(client)
    {
    }
};

static inline int blendFunc(int from, int to, const CSSPropertyBlendingContext& context)
{
    return blend(from, to, context);
}

static inline double blendFunc(double from, double to, const CSSPropertyBlendingContext& context)
{
    return blend(from, to, context);
}

static inline float blendFunc(float from, float to, const CSSPropertyBlendingContext& context)
{
    return narrowPrecisionToFloat(from + (to - from) * context.progress);
}

static inline Color blendFunc(const Color& from, const Color& to, const CSSPropertyBlendingContext& context)
{
    return blend(from, to, context);
}

static inline Length blendFunc(const Length& from, const Length& to, const CSSPropertyBlendingContext& context, ValueRange valueRange = ValueRange::All)
{
    return blend(from, to, context, valueRange);
}

static inline GapLength blendFunc(const GapLength& from, const GapLength& to, const CSSPropertyBlendingContext& context)
{
    if (from.isNormal() || to.isNormal())
        return context.progress < 0.5 ? from : to;
    return blend(from.length(), to.length(), context, ValueRange::NonNegative);
}

static inline TabSize blendFunc(const TabSize& from, const TabSize& to, const CSSPropertyBlendingContext& context)
{
    auto blendedValue = blend(from.value(), to.value(), context);
    return { blendedValue < 0 ? 0 : blendedValue, from.isSpaces() ? SpaceValueType : LengthValueType };
}

static inline LengthSize blendFunc(const LengthSize& from, const LengthSize& to, const CSSPropertyBlendingContext& context)
{
    return { blendFunc(from.width, to.width, context, ValueRange::NonNegative),
        blendFunc(from.height, to.height, context, ValueRange::NonNegative) };
}

static inline LengthPoint blendFunc(const LengthPoint& from, const LengthPoint& to, const CSSPropertyBlendingContext& context)
{
    return blend(from, to, context);
}

static inline ShadowStyle blendFunc(ShadowStyle from, ShadowStyle to, const CSSPropertyBlendingContext& context)
{
    if (from == to)
        return to;

    double fromVal = from == ShadowStyle::Normal ? 1 : 0;
    double toVal = to == ShadowStyle::Normal ? 1 : 0;
    double result = blendFunc(fromVal, toVal, context);
    return result > 0 ? ShadowStyle::Normal : ShadowStyle::Inset;
}

static inline std::unique_ptr<ShadowData> blendFunc(const ShadowData* from, const ShadowData* to, const CSSPropertyBlendingContext& context)
{
    ASSERT(from && to);
    ASSERT(from->style() == to->style());

    return makeUnique<ShadowData>(blend(from->location(), to->location(), context),
        std::max(0, blend(from->radius(), to->radius(), context)),
        blend(from->spread(), to->spread(), context),
        blendFunc(from->style(), to->style(), context),
        from->isWebkitBoxShadow(),
        blend(from->color(), to->color(), context));
}

static inline TransformOperations blendFunc(const TransformOperations& from, const TransformOperations& to, const CSSPropertyBlendingContext& context)
{
    if (context.client->transformFunctionListsMatch())
        return to.blendByMatchingOperations(from, context);
    return to.blendByUsingMatrixInterpolation(from, context, is<RenderBox>(context.client->renderer()) ? downcast<RenderBox>(*context.client->renderer()).borderBoxRect().size() : LayoutSize());
}

static RefPtr<ScaleTransformOperation> blendFunc(ScaleTransformOperation* from, ScaleTransformOperation* to, const CSSPropertyBlendingContext& context)
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
        return blendFunc(normalizedFrom.get(), normalizedTo.get(), context);
    }

    auto blendedOperation = to->blend(from, context);
    if (is<ScaleTransformOperation>(blendedOperation.get())) {
        auto& scale = downcast<ScaleTransformOperation>(blendedOperation.get());
        return ScaleTransformOperation::create(scale.x(), scale.y(), scale.z(), scale.type());
    }
    return nullptr;
}

static RefPtr<RotateTransformOperation> blendFunc(RotateTransformOperation* from, RotateTransformOperation* to, const CSSPropertyBlendingContext& context)
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
        return blendFunc(normalizedFrom.get(), normalizedTo.get(), context);
    }

    auto blendedOperation = to->blend(from, context);
    if (is<RotateTransformOperation>(blendedOperation.get())) {
        auto& rotate = downcast<RotateTransformOperation>(blendedOperation.get());
        return RotateTransformOperation::create(rotate.x(), rotate.y(), rotate.z(), rotate.angle(), rotate.type());
    }
    return nullptr;
}

static RefPtr<TranslateTransformOperation> blendFunc(TranslateTransformOperation* from, TranslateTransformOperation* to, const CSSPropertyBlendingContext& context)
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
        return blendFunc(normalizedFrom.get(), normalizedTo.get(), context);
    }

    Ref<TransformOperation> blendedOperation = to->blend(from, context);
    if (is<TranslateTransformOperation>(blendedOperation.get())) {
        TranslateTransformOperation& translate = downcast<TranslateTransformOperation>(blendedOperation.get());
        return TranslateTransformOperation::create(translate.x(), translate.y(), translate.z(), translate.type());
    }
    return nullptr;
}

static inline RefPtr<ClipPathOperation> blendFunc(ClipPathOperation* from, ClipPathOperation* to, const CSSPropertyBlendingContext& context)
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

    return ShapeClipPathOperation::create(toShape.blend(fromShape, context));
}

static inline RefPtr<ShapeValue> blendFunc(ShapeValue* from, ShapeValue* to, const CSSPropertyBlendingContext& context)
{
    if (!context.progress)
        return from;

    if (context.progress == 1.0)
        return to;

    ASSERT(from && to);
    const BasicShape& fromShape = *from->shape();
    const BasicShape& toShape = *to->shape();
    return ShapeValue::create(toShape.blend(fromShape, context), to->cssBox());
}

static inline RefPtr<FilterOperation> blendFunc(FilterOperation* from, FilterOperation* to, const CSSPropertyBlendingContext& context, bool blendToPassthrough = false)
{
    ASSERT(to);
    return to->blend(from, context, blendToPassthrough);
}

static inline FilterOperations blendFilterOperations(const FilterOperations& from, const FilterOperations& to, const CSSPropertyBlendingContext& context)
{
    FilterOperations result;
    size_t fromSize = from.operations().size();
    size_t toSize = to.operations().size();
    size_t size = std::max(fromSize, toSize);
    for (size_t i = 0; i < size; i++) {
        RefPtr<FilterOperation> fromOp = (i < fromSize) ? from.operations()[i].get() : 0;
        RefPtr<FilterOperation> toOp = (i < toSize) ? to.operations()[i].get() : 0;
        RefPtr<FilterOperation> blendedOp = toOp ? blendFunc(fromOp.get(), toOp.get(), context) : (fromOp ? blendFunc(0, fromOp.get(), context, true) : 0);
        if (blendedOp)
            result.operations().append(blendedOp);
        else {
            auto identityOp = PassthroughFilterOperation::create();
            if (context.progress > 0.5)
                result.operations().append(toOp ? toOp : WTFMove(identityOp));
            else
                result.operations().append(fromOp ? fromOp : WTFMove(identityOp));
        }
    }
    return result;
}

static inline FilterOperations blendFunc(const FilterOperations& from, const FilterOperations& to, const CSSPropertyBlendingContext& context, CSSPropertyID propertyID = CSSPropertyFilter)
{
    FilterOperations result;

    // If we have a filter function list, use that to do a per-function animation.
    
    bool listsMatch = false;
    switch (propertyID) {
    case CSSPropertyFilter:
        listsMatch = context.client->filterFunctionListsMatch();
        break;
#if ENABLE(FILTERS_LEVEL_2)
    case CSSPropertyWebkitBackdropFilter:
        listsMatch = context.client->backdropFilterFunctionListsMatch();
        break;
#endif
    case CSSPropertyAppleColorFilter:
        listsMatch = context.client->colorFilterFunctionListsMatch();
        break;
    default:
        break;
    }
    
    if (listsMatch)
        result = blendFilterOperations(from, to, context);
    else {
        // If the filter function lists don't match, we could try to cross-fade, but don't yet have a way to represent that in CSS.
        // For now we'll just fail to animate.
        result = to;
    }

    return result;
}

static inline RefPtr<StyleImage> blendFilter(CachedImage* image, const FilterOperations& from, const FilterOperations& to, const CSSPropertyBlendingContext& context)
{
    ASSERT(image);
    FilterOperations filterResult = blendFilterOperations(from, to, context);

    auto imageValue = CSSImageValue::create(*image);
    auto filterValue = ComputedStyleExtractor::valueForFilter(context.client->currentStyle(), filterResult, DoNotAdjustPixelValues);

    auto result = CSSFilterImageValue::create(WTFMove(imageValue), WTFMove(filterValue));
    result.get().setFilterOperations(filterResult);
    return StyleGeneratedImage::create(WTFMove(result));
}

static inline Visibility blendFunc(Visibility from, Visibility to, const CSSPropertyBlendingContext& context)
{
    if (from != Visibility::Visible && to != Visibility::Visible)
        return context.progress < 0.5 ? from : to;

    // Any non-zero result means we consider the object to be visible. Only at 0 do we consider the object to be
    // invisible. The invisible value we use (Visibility::Hidden vs. Visibility::Collapse) depends on the specified from/to values.
    double fromVal = from == Visibility::Visible ? 1. : 0.;
    double toVal = to == Visibility::Visible ? 1. : 0.;
    if (fromVal == toVal)
        return to;
    double result = blendFunc(fromVal, toVal, context);
    return result > 0. ? Visibility::Visible : (to != Visibility::Visible ? to : from);
}

static inline TextUnderlineOffset blendFunc(const TextUnderlineOffset& from, const TextUnderlineOffset& to, const CSSPropertyBlendingContext& context)
{
    if (from.isLength() && to.isLength())
        return TextUnderlineOffset::createWithLength(blendFunc(from.lengthValue(), to.lengthValue(), context));
    return TextUnderlineOffset::createWithAuto();
}

static inline TextDecorationThickness blendFunc(const TextDecorationThickness& from, const TextDecorationThickness& to, const CSSPropertyBlendingContext& context)
{
    if (from.isLength() && to.isLength())
        return TextDecorationThickness::createWithLength(blendFunc(from.lengthValue(), to.lengthValue(), context));
    return TextDecorationThickness::createWithAuto();
}

static inline LengthBox blendFunc(const LengthBox& from, const LengthBox& to, const CSSPropertyBlendingContext& context, ValueRange valueRange = ValueRange::NonNegative)
{
    LengthBox result(blendFunc(from.top(), to.top(), context, valueRange),
                     blendFunc(from.right(), to.right(), context, valueRange),
                     blendFunc(from.bottom(), to.bottom(), context, valueRange),
                     blendFunc(from.left(), to.left(), context, valueRange));
    return result;
}

static inline SVGLengthValue blendFunc(const SVGLengthValue& from, const SVGLengthValue& to, const CSSPropertyBlendingContext& context)
{
    return SVGLengthValue::blend(from, to, narrowPrecisionToFloat(context.progress));
}

static inline Vector<SVGLengthValue> blendFunc(const Vector<SVGLengthValue>& from, const Vector<SVGLengthValue>& to, const CSSPropertyBlendingContext& context)
{
    size_t fromLength = from.size();
    size_t toLength = to.size();
    if (!fromLength || !toLength)
        return context.progress < 0.5 ? from : to;
    size_t resultLength = fromLength;
    if (fromLength != toLength) {
        if (!remainder(std::max(fromLength, toLength), std::min(fromLength, toLength)))
            resultLength = std::max(fromLength, toLength);
        else
            resultLength = fromLength * toLength;
    }
    Vector<SVGLengthValue> result(resultLength);
    for (size_t i = 0; i < resultLength; ++i)
        result[i] = SVGLengthValue::blend(from[i % fromLength], to[i % toLength], narrowPrecisionToFloat(context.progress));
    return result;
}

static inline RefPtr<StyleImage> crossfadeBlend(StyleCachedImage* fromStyleImage, StyleCachedImage* toStyleImage, const CSSPropertyBlendingContext& context)
{
    // If progress is at one of the extremes, we want getComputedStyle to show the image,
    // not a completed cross-fade, so we hand back one of the existing images.
    if (!context.progress)
        return fromStyleImage;
    if (context.progress == 1)
        return toStyleImage;
    if (!fromStyleImage->cachedImage() || !toStyleImage->cachedImage())
        return toStyleImage;

    auto fromImageValue = CSSImageValue::create(*fromStyleImage->cachedImage());
    auto toImageValue = CSSImageValue::create(*toStyleImage->cachedImage());
    auto percentageValue = CSSPrimitiveValue::create(context.progress, CSSUnitType::CSS_NUMBER);

    auto crossfadeValue = CSSCrossfadeValue::create(WTFMove(fromImageValue), WTFMove(toImageValue), WTFMove(percentageValue));
    return StyleGeneratedImage::create(WTFMove(crossfadeValue));
}

static inline RefPtr<StyleImage> blendFunc(StyleImage* from, StyleImage* to, const CSSPropertyBlendingContext& context)
{
    if (!context.progress)
        return from;

    if (context.progress == 1.0)
        return to;

    ASSERT(from && to);

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
                return blendFilter(fromFilter.cachedImage(), fromFilter.filterOperations(), toFilter.filterOperations(), context);
        }

        if (is<CSSCrossfadeValue>(fromGenerated) && is<CSSCrossfadeValue>(toGenerated)) {
            CSSCrossfadeValue& fromCrossfade = downcast<CSSCrossfadeValue>(fromGenerated);
            CSSCrossfadeValue& toCrossfade = downcast<CSSCrossfadeValue>(toGenerated);
            if (fromCrossfade.equalInputImages(toCrossfade)) {
                if (auto crossfadeBlend = toCrossfade.blend(fromCrossfade, context))
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
                return blendFilter(fromFilter.cachedImage(), fromFilter.filterOperations(), FilterOperations(), context);
        }
        // FIXME: Add interpolation between cross-fade and image source.
    } else if (is<StyleCachedImage>(*from) && is<StyleGeneratedImage>(*to)) {
        CSSImageGeneratorValue& toGenerated = downcast<StyleGeneratedImage>(*to).imageValue();
        if (is<CSSFilterImageValue>(toGenerated)) {
            CSSFilterImageValue& toFilter = downcast<CSSFilterImageValue>(toGenerated);
            if (toFilter.cachedImage() && downcast<StyleCachedImage>(*from).cachedImage() == toFilter.cachedImage())
                return blendFilter(toFilter.cachedImage(), FilterOperations(), toFilter.filterOperations(), context);
        }
        // FIXME: Add interpolation between image source and cross-fade.
    }

    // FIXME: Add support cross fade between cached and generated images.
    // https://bugs.webkit.org/show_bug.cgi?id=78293
    if (is<StyleCachedImage>(*from) && is<StyleCachedImage>(*to))
        return crossfadeBlend(downcast<StyleCachedImage>(from), downcast<StyleCachedImage>(to), context);

    return to;
}

static inline NinePieceImage blendFunc(const NinePieceImage& from, const NinePieceImage& to, const CSSPropertyBlendingContext& context)
{
    if (!from.hasImage() || !to.hasImage())
        return to;

    // FIXME (74112): Support transitioning between NinePieceImages that differ by more than image content.

    if (from.imageSlices() != to.imageSlices() || from.borderSlices() != to.borderSlices() || from.outset() != to.outset() || from.fill() != to.fill() || from.horizontalRule() != to.horizontalRule() || from.verticalRule() != to.verticalRule())
        return to;

    if (auto* renderer = context.client->renderer()) {
        if (from.image()->imageSize(renderer, 1.0) != to.image()->imageSize(renderer, 1.0))
            return to;
    }

    return NinePieceImage(blendFunc(from.image(), to.image(), context),
        from.imageSlices(), from.fill(), from.borderSlices(), from.outset(), from.horizontalRule(), from.verticalRule());
}

#if ENABLE(VARIATION_FONTS)

static inline FontVariationSettings blendFunc(const FontVariationSettings& from, const FontVariationSettings& to, const CSSPropertyBlendingContext& context)
{
    if (!context.progress)
        return from;

    if (context.progress == 1.0)
        return to;

    ASSERT(from.size() == to.size());
    FontVariationSettings result;
    unsigned size = from.size();
    for (unsigned i = 0; i < size; ++i) {
        auto& fromItem = from.at(i);
        auto& toItem = to.at(i);
        ASSERT(fromItem.tag() == toItem.tag());
        float interpolated = blendFunc(fromItem.value(), toItem.value(), context);
        result.insert({ fromItem.tag(), interpolated });
    }
    return result;
}

#endif

static inline FontSelectionValue blendFunc(FontSelectionValue from, FontSelectionValue to, const CSSPropertyBlendingContext& context)
{
    return FontSelectionValue(std::max(0.0f, blendFunc(static_cast<float>(from), static_cast<float>(to), context)));
}

static inline std::optional<FontSelectionValue> blendFunc(std::optional<FontSelectionValue> from, std::optional<FontSelectionValue> to, const CSSPropertyBlendingContext& context)
{
    return blendFunc(*from, *to, context);
}

class AnimationPropertyWrapperBase {
    WTF_MAKE_NONCOPYABLE(AnimationPropertyWrapperBase);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit AnimationPropertyWrapperBase(CSSPropertyID property)
        : m_property(property)
    {
    }
    virtual ~AnimationPropertyWrapperBase() = default;

    virtual bool isShorthandWrapper() const { return false; }
    virtual bool equals(const RenderStyle&, const RenderStyle&) const = 0;
    virtual bool canInterpolate(const RenderStyle&, const RenderStyle&) const { return true; }
    virtual void blend(RenderStyle&, const RenderStyle&, const RenderStyle&, const CSSPropertyBlendingContext&) const = 0;
    
#if !LOG_DISABLED
    virtual void logBlend(const RenderStyle&, const RenderStyle&, const RenderStyle&, double) const = 0;
#endif

    CSSPropertyID property() const { return m_property; }

    virtual bool animationIsAccelerated() const { return false; }

private:
    CSSPropertyID m_property;
};

template <typename T>
class PropertyWrapperGetter : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperGetter(CSSPropertyID property, T (RenderStyle::*getter)() const)
        : AnimationPropertyWrapperBase(property)
        , m_getter(getter)
    {
    }

    T value(const RenderStyle& style) const
    {
        return (style.*m_getter)();
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const override
    {
        if (&a == &b)
            return true;
        return value(a) == value(b);
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << getPropertyName(property()) << " from " << value(from) << " to " << value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
    }
#endif

private:
    T (RenderStyle::*m_getter)() const;
};

template <typename T>
class PropertyWrapper : public PropertyWrapperGetter<T> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapper(CSSPropertyID property, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T))
        : PropertyWrapperGetter<T>(property, getter)
        , m_setter(setter)
    {
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const override
    {
        (destination.*m_setter)(blendFunc(this->value(from), this->value(to), context));
    }

protected:
    void (RenderStyle::*m_setter)(T);
};

template <typename T>
class PositivePropertyWrapper final : public PropertyWrapper<T> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PositivePropertyWrapper(CSSPropertyID property, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T))
        : PropertyWrapper<T>(property, getter, setter)
    {
    }

private:
    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        auto blendedValue = blendFunc(this->value(from), this->value(to), context);
        (destination.*this->m_setter)(blendedValue > 1 ? blendedValue : 1);
    }
};

template <typename T>
class DiscretePropertyWrapper final : public PropertyWrapperGetter<T> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DiscretePropertyWrapper(CSSPropertyID property, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T))
        : PropertyWrapperGetter<T>(property, getter)
        , m_setter(setter)
    {
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        ASSERT(!context.progress || context.progress == 1.0);
        (destination.*this->m_setter)(this->value(context.progress ? to : from));
    }

private:
    bool canInterpolate(const RenderStyle&, const RenderStyle&) const final { return false; }

    void (RenderStyle::*m_setter)(T);
};

template <GridTrackSizingDirection>
class GridTemplateTracksWrapper : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    GridTemplateTracksWrapper();

private:
    bool canInterpolate(const RenderStyle&, const RenderStyle&) const final { return false; }

    bool equals(const RenderStyle& a, const RenderStyle& b) const override
    {
        return m_gridTracksWrapper.equals(a, b) && m_autoRepeatTracksWrapper.equals(a, b) && m_gridAutoRepeatTypeTracksWrapper.equals(a, b) && m_gridAutoRepeatInsertionPointTracksWrapper.equals(a, b) && m_orderedNamedGridLinesTracksWrapper.equals(a, b) && m_autoRepeatOrderedNamedGridLinesTracksWrapper.equals(a, b);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        m_gridTracksWrapper.blend(destination, from, to, context);
        m_autoRepeatTracksWrapper.blend(destination, from, to, context);
        m_gridAutoRepeatTypeTracksWrapper.blend(destination, from, to, context);
        m_gridAutoRepeatInsertionPointTracksWrapper.blend(destination, from, to, context);
        m_orderedNamedGridLinesTracksWrapper.blend(destination, from, to, context);
        m_autoRepeatOrderedNamedGridLinesTracksWrapper.blend(destination, from, to, context);
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        m_gridTracksWrapper.logBlend(from, to, destination, progress);
        m_autoRepeatTracksWrapper.logBlend(from, to, destination, progress);
        m_gridAutoRepeatTypeTracksWrapper.logBlend(from, to, destination, progress);
        m_gridAutoRepeatInsertionPointTracksWrapper.logBlend(from, to, destination, progress);
        m_orderedNamedGridLinesTracksWrapper.logBlend(from, to, destination, progress);
        m_autoRepeatOrderedNamedGridLinesTracksWrapper.logBlend(from, to, destination, progress);
    }
#endif

    DiscretePropertyWrapper<const Vector<GridTrackSize>&> m_gridTracksWrapper;
    DiscretePropertyWrapper<const Vector<GridTrackSize>&> m_autoRepeatTracksWrapper;
    DiscretePropertyWrapper<AutoRepeatType> m_gridAutoRepeatTypeTracksWrapper;
    DiscretePropertyWrapper<unsigned> m_gridAutoRepeatInsertionPointTracksWrapper;
    DiscretePropertyWrapper<const OrderedNamedGridLinesMap&> m_orderedNamedGridLinesTracksWrapper;
    DiscretePropertyWrapper<const OrderedNamedGridLinesMap&> m_autoRepeatOrderedNamedGridLinesTracksWrapper;
};

template <>
GridTemplateTracksWrapper<ForColumns>::GridTemplateTracksWrapper()
    : AnimationPropertyWrapperBase(CSSPropertyGridTemplateColumns)
    , m_gridTracksWrapper(DiscretePropertyWrapper<const Vector<GridTrackSize>&>(CSSPropertyGridTemplateColumns, &RenderStyle::gridColumns, &RenderStyle::setGridColumns))
    , m_autoRepeatTracksWrapper(DiscretePropertyWrapper<const Vector<GridTrackSize>&>(CSSPropertyGridTemplateColumns, &RenderStyle::gridAutoRepeatColumns, &RenderStyle::setGridAutoRepeatColumns))
    , m_gridAutoRepeatTypeTracksWrapper(DiscretePropertyWrapper<AutoRepeatType>(CSSPropertyGridTemplateColumns, &RenderStyle::gridAutoRepeatColumnsType, &RenderStyle::setGridAutoRepeatColumnsType))
    , m_gridAutoRepeatInsertionPointTracksWrapper(DiscretePropertyWrapper<unsigned>(CSSPropertyGridTemplateColumns, &RenderStyle::gridAutoRepeatColumnsInsertionPoint, &RenderStyle::setGridAutoRepeatColumnsInsertionPoint))
    , m_orderedNamedGridLinesTracksWrapper(DiscretePropertyWrapper<const OrderedNamedGridLinesMap&>(CSSPropertyGridTemplateColumns, &RenderStyle::orderedNamedGridColumnLines, &RenderStyle::setOrderedNamedGridColumnLines))
    , m_autoRepeatOrderedNamedGridLinesTracksWrapper(DiscretePropertyWrapper<const OrderedNamedGridLinesMap&>(CSSPropertyGridTemplateColumns, &RenderStyle::autoRepeatOrderedNamedGridColumnLines, &RenderStyle::setAutoRepeatOrderedNamedGridColumnLines))
{
}

template <>
GridTemplateTracksWrapper<ForRows>::GridTemplateTracksWrapper()
    : AnimationPropertyWrapperBase(CSSPropertyGridTemplateRows)
    , m_gridTracksWrapper(DiscretePropertyWrapper<const Vector<GridTrackSize>&>(CSSPropertyGridTemplateRows, &RenderStyle::gridRows, &RenderStyle::setGridRows))
    , m_autoRepeatTracksWrapper(DiscretePropertyWrapper<const Vector<GridTrackSize>&>(CSSPropertyGridTemplateRows, &RenderStyle::gridAutoRepeatRows, &RenderStyle::setGridAutoRepeatRows))
    , m_gridAutoRepeatTypeTracksWrapper(DiscretePropertyWrapper<AutoRepeatType>(CSSPropertyGridTemplateRows, &RenderStyle::gridAutoRepeatRowsType, &RenderStyle::setGridAutoRepeatRowsType))
    , m_gridAutoRepeatInsertionPointTracksWrapper(DiscretePropertyWrapper<unsigned>(CSSPropertyGridTemplateRows, &RenderStyle::gridAutoRepeatRowsInsertionPoint, &RenderStyle::setGridAutoRepeatRowsInsertionPoint))
    , m_orderedNamedGridLinesTracksWrapper(DiscretePropertyWrapper<const OrderedNamedGridLinesMap&>(CSSPropertyGridTemplateRows, &RenderStyle::orderedNamedGridRowLines, &RenderStyle::setOrderedNamedGridRowLines))
    , m_autoRepeatOrderedNamedGridLinesTracksWrapper(DiscretePropertyWrapper<const OrderedNamedGridLinesMap&>(CSSPropertyGridTemplateRows, &RenderStyle::autoRepeatOrderedNamedGridRowLines, &RenderStyle::setAutoRepeatOrderedNamedGridRowLines))
{
}

class BorderImageRepeatWrapper final : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    BorderImageRepeatWrapper()
        : AnimationPropertyWrapperBase(CSSPropertyBorderImageRepeat)
        , m_horizontalWrapper(DiscretePropertyWrapper<NinePieceImageRule>(CSSPropertyBorderImageRepeat, &RenderStyle::borderImageHorizontalRule, &RenderStyle::setBorderImageHorizontalRule))
        , m_verticalWrapper(DiscretePropertyWrapper<NinePieceImageRule>(CSSPropertyBorderImageRepeat, &RenderStyle::borderImageVerticalRule, &RenderStyle::setBorderImageVerticalRule))
    {
    }

private:
    bool canInterpolate(const RenderStyle&, const RenderStyle&) const final { return false; }

    bool equals(const RenderStyle& a, const RenderStyle& b) const override
    {
        return m_horizontalWrapper.equals(a, b) && m_verticalWrapper.equals(a, b);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        m_horizontalWrapper.blend(destination, from, to, context);
        m_verticalWrapper.blend(destination, from, to, context);
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        m_horizontalWrapper.logBlend(from, to, destination, progress);
        m_verticalWrapper.logBlend(from, to, destination, progress);
    }
#endif

    DiscretePropertyWrapper<NinePieceImageRule> m_horizontalWrapper;
    DiscretePropertyWrapper<NinePieceImageRule> m_verticalWrapper;
};

template <typename T>
class RefCountedPropertyWrapper : public PropertyWrapperGetter<T*> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RefCountedPropertyWrapper(CSSPropertyID property, T* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(RefPtr<T>&&))
        : PropertyWrapperGetter<T*>(property, getter)
        , m_setter(setter)
    {
    }

private:
    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        (destination.*this->m_setter)(blendFunc(this->value(from), this->value(to), context));
    }

    void (RenderStyle::*m_setter)(RefPtr<T>&&);
};

static bool canInterpolateLengths(const Length& from, const Length& to, bool isLengthPercentage)
{
    if (from.type() == to.type())
        return true;

    // Some properties allow for <length-percentage> and <number> values. We must allow animating
    // between a <length> and a <percentage>, but exclude animating between a <number> and either
    // a <length> or <percentage>. We can use Length::isRelative() to determine whether we are
    // dealing with a <number> as opposed to a <length> or <percentage>.
    if (isLengthPercentage) {
        return (from.isFixed() || from.isPercentOrCalculated() || from.isRelative())
            && (to.isFixed() || to.isPercentOrCalculated() || to.isRelative())
            && from.isRelative() == to.isRelative();
    }

    return false;
}

class LengthPropertyWrapper : public PropertyWrapperGetter<const Length&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Flags {
        IsLengthPercentage          = 1 << 0,
        NegativeLengthsAreInvalid   = 1 << 1,
    };
    LengthPropertyWrapper(CSSPropertyID property, const Length& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(Length&&), OptionSet<Flags> flags = { })
        : PropertyWrapperGetter<const Length&>(property, getter)
        , m_setter(setter)
        , m_flags(flags)
    {
    }

protected:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to) const override
    {
        return canInterpolateLengths(value(from), value(to), m_flags.contains(Flags::IsLengthPercentage));
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const override
    {
        auto valueRange = m_flags.contains(Flags::NegativeLengthsAreInvalid) ? ValueRange::NonNegative : ValueRange::All;
        (destination.*m_setter)(blendFunc(value(from), value(to), context, valueRange));
    }

private:
    void (RenderStyle::*m_setter)(Length&&);
    OptionSet<Flags> m_flags;
};

static bool canInterpolateLengthVariants(const LengthSize& from, const LengthSize& to)
{
    bool isLengthPercentage = true;
    return canInterpolateLengths(from.width, to.width, isLengthPercentage)
        && canInterpolateLengths(from.height, to.height, isLengthPercentage);
}

static bool canInterpolateLengthVariants(const GapLength& from, const GapLength& to)
{
    if (from.isNormal() || to.isNormal())
        return false;
    bool isLengthPercentage = true;
    return canInterpolateLengths(from.length(), to.length(), isLengthPercentage);
}

class LengthPointPropertyWrapper final : public PropertyWrapperGetter<LengthPoint> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LengthPointPropertyWrapper(CSSPropertyID property, LengthPoint (RenderStyle::*getter)() const, void (RenderStyle::*setter)(LengthPoint&&))
        : PropertyWrapperGetter<LengthPoint>(property, getter)
        , m_setter(setter)
    {
    }

private:
    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        (destination.*m_setter)(blendFunc(value(from), value(to), context));
    }

    void (RenderStyle::*m_setter)(LengthPoint&&);
};

template <typename T>
class LengthVariantPropertyWrapper final : public PropertyWrapperGetter<const T&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LengthVariantPropertyWrapper(CSSPropertyID property, const T& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T&&))
        : PropertyWrapperGetter<const T&>(property, getter)
        , m_setter(setter)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to) const final
    {
        return canInterpolateLengthVariants(this->value(from), this->value(to));
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        (destination.*m_setter)(blendFunc(this->value(from), this->value(to), context));
    }

    void (RenderStyle::*m_setter)(T&&);
};

class LengthBoxPropertyWrapper : public PropertyWrapperGetter<const LengthBox&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Flags {
        IsLengthPercentage      = 1 << 0,
        UsesFillKeyword         = 1 << 1,
        AllowsNegativeValues    = 1 << 2,
    };
    LengthBoxPropertyWrapper(CSSPropertyID property, const LengthBox& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(LengthBox&&), OptionSet<Flags> flags = { })
        : PropertyWrapperGetter<const LengthBox&>(property, getter)
        , m_setter(setter)
        , m_flags(flags)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to) const override
    {
        if (m_flags.contains(Flags::UsesFillKeyword) && from.borderImage().fill() != to.borderImage().fill())
            return false;

        auto& fromLengthBox = value(from);
        auto& toLengthBox = value(to);
        bool isLengthPercentage = m_flags.contains(Flags::IsLengthPercentage);
        return canInterpolateLengths(fromLengthBox.top(), toLengthBox.top(), isLengthPercentage)
            && canInterpolateLengths(fromLengthBox.right(), toLengthBox.right(), isLengthPercentage)
            && canInterpolateLengths(fromLengthBox.bottom(), toLengthBox.bottom(), isLengthPercentage)
            && canInterpolateLengths(fromLengthBox.left(), toLengthBox.left(), isLengthPercentage);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const override
    {
        if (m_flags.contains(Flags::UsesFillKeyword))
            destination.setBorderImageSliceFill((!context.progress || !context.isDiscrete ? from : to).borderImage().fill());
        if (context.isDiscrete) {
            // It is important we have this non-interpolated shortcut because certain CSS properties
            // represented as a LengthBox, such as border-image-slice, don't know how to deal with
            // calculated Length values, see for instance valueForImageSliceSide(const Length&).
            (destination.*m_setter)(context.progress ? LengthBox(value(to)) : LengthBox(value(from)));
            return;
        }
        auto valueRange = m_flags.contains(Flags::AllowsNegativeValues) ? ValueRange::All : ValueRange::NonNegative;
        (destination.*m_setter)(blendFunc(value(from), value(to), context, valueRange));
    }

    void (RenderStyle::*m_setter)(LengthBox&&);
    OptionSet<Flags> m_flags;
};

class ClipWrapper final : public LengthBoxPropertyWrapper {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ClipWrapper()
        : LengthBoxPropertyWrapper(CSSPropertyClip, &RenderStyle::clip, &RenderStyle::setClip, { LengthBoxPropertyWrapper::Flags::AllowsNegativeValues })
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to) const final
    {
        return from.hasClip() && to.hasClip() && LengthBoxPropertyWrapper::canInterpolate(from, to);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        LengthBoxPropertyWrapper::blend(destination, from, to, context);
        destination.setHasClip(true);
    }
};

class PropertyWrapperClipPath final : public RefCountedPropertyWrapper<ClipPathOperation> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperClipPath(CSSPropertyID property, ClipPathOperation* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(RefPtr<ClipPathOperation>&&))
        : RefCountedPropertyWrapper<ClipPathOperation>(property, getter, setter)
    {
    }

private:
    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        // If the style pointers are the same, don't bother doing the test.
        if (&a == &b)
            return true;

        auto* clipPathA = value(a);
        auto* clipPathB = value(b);
        if (clipPathA == clipPathB)
            return true;
        if (!clipPathA || !clipPathB)
            return false;
        return *clipPathA == *clipPathB;
    }
};

#if ENABLE(VARIATION_FONTS)
class PropertyWrapperFontVariationSettings final : public PropertyWrapper<FontVariationSettings> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperFontVariationSettings(CSSPropertyID property, FontVariationSettings (RenderStyle::*getter)() const, void (RenderStyle::*setter)(FontVariationSettings))
        : PropertyWrapper<FontVariationSettings>(property, getter, setter)
    {
    }

private:
    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        // If the style pointers are the same, don't bother doing the test.
        if (&a == &b)
            return true;
        return value(a) == value(b);
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to) const final
    {
        auto fromVariationSettings = value(from);
        auto toVariationSettings = value(to);

        if (fromVariationSettings.size() != toVariationSettings.size())
            return false;

        auto size = fromVariationSettings.size();
        for (unsigned i = 0; i < size; ++i) {
            if (fromVariationSettings.at(i).tag() != toVariationSettings.at(i).tag())
                return false;
        }

        return true;
    }
};
#endif

class PropertyWrapperShape final : public RefCountedPropertyWrapper<ShapeValue> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperShape(CSSPropertyID property, ShapeValue* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(RefPtr<ShapeValue>&&))
        : RefCountedPropertyWrapper<ShapeValue>(property, getter, setter)
    {
    }

private:
    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        // If the style pointers are the same, don't bother doing the test.
        if (&a == &b)
            return true;

        auto* shapeA = value(a);
        auto* shapeB = value(b);
        if (shapeA == shapeB)
            return true;
        if (!shapeA || !shapeB)
            return false;
        return *shapeA == *shapeB;
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to) const final
    {
        auto* fromShape = value(from);
        auto* toShape = value(to);

        if (!fromShape || !toShape)
            return false;

        if (fromShape->type() != ShapeValue::Type::Shape || toShape->type() != ShapeValue::Type::Shape)
            return false;

        if (fromShape->cssBox() != toShape->cssBox())
            return false;

        return fromShape->shape()->canBlend(*toShape->shape());
    }
};

class StyleImagePropertyWrapper final : public RefCountedPropertyWrapper<StyleImage> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    StyleImagePropertyWrapper(CSSPropertyID property, StyleImage* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(RefPtr<StyleImage>&&))
        : RefCountedPropertyWrapper<StyleImage>(property, getter, setter)
    {
    }

private:
    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        if (&a == &b)
            return true;

        auto* imageA = value(a);
        auto* imageB = value(b);
        return arePointingToEqualData(imageA, imageB);
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to) const final
    {
        return value(from) && value(to);
    }
};

template <typename T>
class AcceleratedPropertyWrapper final : public PropertyWrapper<T> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    AcceleratedPropertyWrapper(CSSPropertyID property, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T))
        : PropertyWrapper<T>(property, getter, setter)
    {
    }

private:
    bool animationIsAccelerated() const final { return true; }
};

template <typename T>
class AcceleratedIndividualTransformPropertyWrapper final : public RefCountedPropertyWrapper<T> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    AcceleratedIndividualTransformPropertyWrapper(CSSPropertyID property, T* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(RefPtr<T>&&))
        : RefCountedPropertyWrapper<T>(property, getter, setter)
    {
    }

private:
    bool animationIsAccelerated() const final { return true; }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return arePointingToEqualData(this->value(a), this->value(b));
    }
};

class PropertyWrapperFilter final : public PropertyWrapper<const FilterOperations&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperFilter(CSSPropertyID propertyID, const FilterOperations& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const FilterOperations&))
        : PropertyWrapper<const FilterOperations&>(propertyID, getter, setter)
    {
    }

private:
    bool animationIsAccelerated() const final
    {
        return property() == CSSPropertyFilter
#if ENABLE(FILTERS_LEVEL_2)
            || property() == CSSPropertyWebkitBackdropFilter
#endif
            ;
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        (destination.*m_setter)(blendFunc(value(from), value(to), context, property()));
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

class PropertyWrapperShadow final : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperShadow(CSSPropertyID property, const ShadowData* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(std::unique_ptr<ShadowData>, bool))
        : AnimationPropertyWrapperBase(property)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

private:
    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        if (&a == &b)
            return true;

        const ShadowData* shadowA = (a.*m_getter)();
        const ShadowData* shadowB = (b.*m_getter)();

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

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to) const final
    {
        const ShadowData* fromShadow = (from.*m_getter)();
        const ShadowData* toShadow = (to.*m_getter)();

        // The only scenario where we can't interpolate is if specified items don't have the same shadow style.
        while (fromShadow && toShadow) {
            if (fromShadow->style() != toShadow->style())
                return false;
            fromShadow = fromShadow->next();
            toShadow = toShadow->next();
        }

        return true;
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        const ShadowData* fromShadow = (from.*m_getter)();
        const ShadowData* toShadow = (to.*m_getter)();

        if (context.isDiscrete) {
            ASSERT(!context.progress || context.progress == 1.0);
            auto* shadow = context.progress ? toShadow : fromShadow;
            (destination.*m_setter)(shadow ? makeUnique<ShadowData>(*shadow) : nullptr, false);
            return;
        }

        int fromLength = shadowListLength(fromShadow);
        int toLength = shadowListLength(toShadow);

        if (fromLength == toLength || (fromLength <= 1 && toLength <= 1)) {
            (destination.*m_setter)(blendSimpleOrMatchedShadowLists(fromShadow, toShadow, context), false);
            return;
        }

        (destination.*m_setter)(blendMismatchedShadowLists(fromShadow, toShadow, fromLength, toLength, context), false);
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle&, const RenderStyle&, const RenderStyle&, double progress) const final
    {
        // FIXME: better logging.
        LOG_WITH_STREAM(Animations, stream << "  blending ShadowData at " << TextStream::FormatNumberRespectingIntegers(progress));
    }
#endif

    std::unique_ptr<ShadowData> blendSimpleOrMatchedShadowLists(const ShadowData* shadowA, const ShadowData* shadowB, const CSSPropertyBlendingContext& context) const
    {
        std::unique_ptr<ShadowData> newShadowData;
        ShadowData* lastShadow = 0;

        while (shadowA || shadowB) {
            const ShadowData* srcShadow = shadowForBlending(shadowA, shadowB);
            const ShadowData* dstShadow = shadowForBlending(shadowB, shadowA);

            std::unique_ptr<ShadowData> blendedShadow = blendFunc(srcShadow, dstShadow, context);
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

    std::unique_ptr<ShadowData> blendMismatchedShadowLists(const ShadowData* shadowA, const ShadowData* shadowB, int fromLength, int toLength, const CSSPropertyBlendingContext& context) const
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

            std::unique_ptr<ShadowData> blendedShadow = blendFunc(srcShadow, dstShadow, context);
            // Insert at the start of the list to preserve the order.
            blendedShadow->setNext(WTFMove(newShadowData));
            newShadowData = WTFMove(blendedShadow);
        }

        return newShadowData;
    }

    const ShadowData* (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(std::unique_ptr<ShadowData>, bool);
};

class PropertyWrapperMaybeInvalidColor final : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperMaybeInvalidColor(CSSPropertyID property, const Color& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Color&))
        : AnimationPropertyWrapperBase(property)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

private:
    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        if (&a == &b)
            return true;

        Color fromColor = value(a);
        Color toColor = value(b);

        if (!fromColor.isValid() && !toColor.isValid())
            return true;

        if (!fromColor.isValid())
            fromColor = a.color();
        if (!toColor.isValid())
            toColor = b.color();

        return fromColor == toColor;
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        Color fromColor = value(from);
        Color toColor = value(to);

        if (!fromColor.isValid() && !toColor.isValid())
            return;

        if (!fromColor.isValid())
            fromColor = from.color();
        if (!toColor.isValid())
            toColor = to.color();
        (destination.*m_setter)(blendFunc(fromColor, toColor, context));
    }

    Color value(const RenderStyle& style) const
    {
        return (style.*m_getter)();
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        // FIXME: better logging.
        LOG_WITH_STREAM(Animations, stream << "  blending " << getPropertyName(property()) << " from " << value(from) << " to " << value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
    }
#endif

    const Color& (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(const Color&);
};


enum MaybeInvalidColorTag { MaybeInvalidColor };
class PropertyWrapperVisitedAffectedColor : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperVisitedAffectedColor(CSSPropertyID property, const Color& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Color&), const Color& (RenderStyle::*visitedGetter)() const, void (RenderStyle::*visitedSetter)(const Color&))
        : AnimationPropertyWrapperBase(property)
        , m_wrapper(makeUnique<PropertyWrapper<const Color&>>(property, getter, setter))
        , m_visitedWrapper(makeUnique<PropertyWrapper<const Color&>>(property, visitedGetter, visitedSetter))
    {
    }
    PropertyWrapperVisitedAffectedColor(CSSPropertyID property, MaybeInvalidColorTag, const Color& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Color&), const Color& (RenderStyle::*visitedGetter)() const, void (RenderStyle::*visitedSetter)(const Color&))
        : AnimationPropertyWrapperBase(property)
        , m_wrapper(makeUnique<PropertyWrapperMaybeInvalidColor>(property, getter, setter))
        , m_visitedWrapper(makeUnique<PropertyWrapperMaybeInvalidColor>(property, visitedGetter, visitedSetter))
    {
    }

protected:
    bool equals(const RenderStyle& a, const RenderStyle& b) const override
    {
        return m_wrapper->equals(a, b) && m_visitedWrapper->equals(a, b);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const override
    {
        m_wrapper->blend(destination, from, to, context);
        m_visitedWrapper->blend(destination, from, to, context);
    }

    std::unique_ptr<AnimationPropertyWrapperBase> m_wrapper;
    std::unique_ptr<AnimationPropertyWrapperBase> m_visitedWrapper;

private:
#if !LOG_DISABLED
    void logBlend(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        m_wrapper->logBlend(from, to, destination, progress);
        m_visitedWrapper->logBlend(from, to, destination, progress);
    }
#endif
};

static bool canInterpolateCaretColor(const RenderStyle& from, const RenderStyle& to, bool visited)
{
    if (visited)
        return !from.hasVisitedLinkAutoCaretColor() && !to.hasVisitedLinkAutoCaretColor();
    return !from.hasAutoCaretColor() && !to.hasAutoCaretColor();
}

class CaretColorPropertyWrapper final : public PropertyWrapperVisitedAffectedColor {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CaretColorPropertyWrapper()
        : PropertyWrapperVisitedAffectedColor(CSSPropertyCaretColor, MaybeInvalidColor, &RenderStyle::caretColor, &RenderStyle::setCaretColor, &RenderStyle::visitedLinkCaretColor, &RenderStyle::setVisitedLinkCaretColor)
    {
    }

private:
    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return a.hasAutoCaretColor() == b.hasAutoCaretColor()
            && a.hasVisitedLinkAutoCaretColor() == b.hasVisitedLinkAutoCaretColor()
            && PropertyWrapperVisitedAffectedColor::equals(a, b);
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to) const final
    {
        return canInterpolateCaretColor(from, to, false) || canInterpolateCaretColor(from, to, true);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        if (canInterpolateCaretColor(from, to, false))
            m_wrapper->blend(destination, from, to, context);
        else {
            auto& blendingRenderStyle = context.progress < 0.5 ? from : to;
            if (blendingRenderStyle.hasAutoCaretColor())
                destination.setHasAutoCaretColor();
            else
                destination.setCaretColor(blendingRenderStyle.caretColor());
        }

        if (canInterpolateCaretColor(from, to, true))
            m_visitedWrapper->blend(destination, from, to, context);
        else {
            auto& blendingRenderStyle = context.progress < 0.5 ? from : to;
            if (blendingRenderStyle.hasVisitedLinkAutoCaretColor())
                destination.setHasVisitedLinkAutoCaretColor();
            else
                destination.setVisitedLinkCaretColor(blendingRenderStyle.visitedLinkCaretColor());
        }
    }
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
    virtual void blend(FillLayer*, const FillLayer*, const FillLayer*, const CSSPropertyBlendingContext&) const = 0;
    virtual bool canInterpolate(const FillLayer*, const FillLayer*) const { return true; }

#if !LOG_DISABLED
    virtual void logBlend(const FillLayer* destination, const FillLayer*, const FillLayer*, double) const = 0;
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

protected:
    bool equals(const FillLayer* a, const FillLayer* b) const override
    {
        if (a == b)
            return true;
        if (!a || !b)
            return false;
        return value(a) == value(b);
    }

    T value(const FillLayer* layer) const
    {
        return (layer->*m_getter)();
    }

#if !LOG_DISABLED
    void logBlend(const FillLayer* destination, const FillLayer* from, const FillLayer* to, double progress) const override
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << getPropertyName(property()) << " from " << value(from) << " to " << value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
    }
#endif

private:
    T (FillLayer::*m_getter)() const;
};

template <typename T>
class FillLayerPropertyWrapper final : public FillLayerPropertyWrapperGetter<const T&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FillLayerPropertyWrapper(CSSPropertyID property, const T& (FillLayer::*getter)() const, void (FillLayer::*setter)(T))
        : FillLayerPropertyWrapperGetter<const T&>(property, getter)
        , m_setter(setter)
    {
    }

private:
    void blend(FillLayer* destination, const FillLayer* from, const FillLayer* to, const CSSPropertyBlendingContext& context) const final
    {
        (destination->*this->m_setter)(blendFunc(this->value(from), this->value(to), context));
    }

    bool canInterpolate(const FillLayer* from, const FillLayer* to) const final
    {
        return canInterpolateLengthVariants(this->value(from), this->value(to));
    }

#if !LOG_DISABLED
    void logBlend(const FillLayer* destination, const FillLayer* from, const FillLayer* to, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << getPropertyName(FillLayerPropertyWrapperGetter<const T&>::property())
            << " from " << FillLayerPropertyWrapperGetter<const T&>::value(from)
            << " to " << FillLayerPropertyWrapperGetter<const T&>::value(to)
            << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << FillLayerPropertyWrapperGetter<const T&>::value(destination));
    }
#endif

    void (FillLayer::*m_setter)(T);
};

class FillLayerPositionPropertyWrapper final : public FillLayerPropertyWrapperGetter<const Length&> {
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

private:
    bool equals(const FillLayer* a, const FillLayer* b) const final
    {
        if (a == b)
            return true;
        if (!a || !b)
            return false;

        auto fromLength = value(a);
        auto toLength = value(b);
        
        Edge fromEdge = (a->*m_originGetter)();
        Edge toEdge = (b->*m_originGetter)();
        
        return fromLength == toLength && fromEdge == toEdge;
    }

    void blend(FillLayer* destination, const FillLayer* from, const FillLayer* to, const CSSPropertyBlendingContext& context) const final
    {
        auto fromLength = value(from);
        auto toLength = value(to);
        
        Edge fromEdge = (from->*m_originGetter)();
        Edge toEdge = (to->*m_originGetter)();
        
        Edge destinationEdge = toEdge;
        if (fromEdge != toEdge) {
            // Convert the right/bottom into a calc expression,
            if (fromEdge == m_farEdge)
                fromLength = convertTo100PercentMinusLength(fromLength);
            else if (toEdge == m_farEdge) {
                toLength = convertTo100PercentMinusLength(toLength);
                destinationEdge = fromEdge; // Now we have a calc(100% - l), it's relative to the left/top edge.
            }
        }

        (destination->*m_originSetter)(destinationEdge);
        (destination->*m_lengthSetter)(blendFunc(fromLength, toLength, context));
    }

#if !LOG_DISABLED
    void logBlend(const FillLayer* destination, const FillLayer* from, const FillLayer* to, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << getPropertyName(property()) << " from " << value(from) << " to " << value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
    }
#endif

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

private:
    void blend(FillLayer* destination, const FillLayer* from, const FillLayer* to, const CSSPropertyBlendingContext& context) const final
    {
        (destination->*this->m_setter)(blendFunc(this->value(from), this->value(to), context));
    }

#if !LOG_DISABLED
    void logBlend(const FillLayer* destination, const FillLayer* from, const FillLayer* to, double progress) const override
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << getPropertyName(FillLayerPropertyWrapperGetter<T*>::property())
            << " from " << FillLayerPropertyWrapperGetter<T*>::value(from)
            << " to " << FillLayerPropertyWrapperGetter<T*>::value(to)
            << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << FillLayerPropertyWrapperGetter<T*>::value(destination));
    }
#endif

    void (FillLayer::*m_setter)(RefPtr<T>&&);
};

class FillLayerStyleImagePropertyWrapper final : public FillLayerRefCountedPropertyWrapper<StyleImage> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FillLayerStyleImagePropertyWrapper(CSSPropertyID property, StyleImage* (FillLayer::*getter)() const, void (FillLayer::*setter)(RefPtr<StyleImage>&&))
        : FillLayerRefCountedPropertyWrapper<StyleImage>(property, getter, setter)
    {
    }

private:
    bool equals(const FillLayer* a, const FillLayer* b) const final
    {
        if (a == b)
            return true;
        if (!a || !b)
            return false;
        return arePointingToEqualData(value(a), value(b));
    }

    bool canInterpolate(const FillLayer* from, const FillLayer* to) const final
    {
        return value(from) && value(to);
    }

#if !LOG_DISABLED
    void logBlend(const FillLayer* destination, const FillLayer* from, const FillLayer* to, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << getPropertyName(this->property()) << " from " << this->value(from) << " to " << this->value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
    }
#endif
};

class FillLayersPropertyWrapper final : public AnimationPropertyWrapperBase {
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

private:
    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        if (&a == &b)
            return true;

        auto* fromLayer = &(a.*m_layersGetter)();
        auto* toLayer = &(b.*m_layersGetter)();

        while (fromLayer && toLayer) {
            if (!m_fillLayerPropertyWrapper->equals(fromLayer, toLayer))
                return false;

            fromLayer = fromLayer->next();
            toLayer = toLayer->next();
        }

        return true;
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to) const final
    {
        auto* fromLayer = &(from.*m_layersGetter)();
        auto* toLayer = &(to.*m_layersGetter)();

        while (fromLayer && toLayer) {
            if (fromLayer->sizeType() != toLayer->sizeType())
                return false;

            if (!m_fillLayerPropertyWrapper->canInterpolate(fromLayer, toLayer))
                return false;

            fromLayer = fromLayer->next();
            toLayer = toLayer->next();
        }

        return true;
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        auto* fromLayer = &(from.*m_layersGetter)();
        auto* toLayer = &(to.*m_layersGetter)();
        auto* dstLayer = &(destination.*m_layersAccessor)();

        while (fromLayer && toLayer && dstLayer) {
            dstLayer->setSizeType((context.progress ? toLayer : fromLayer)->sizeType());
            m_fillLayerPropertyWrapper->blend(dstLayer, fromLayer, toLayer, context);
            fromLayer = fromLayer->next();
            toLayer = toLayer->next();
            dstLayer = dstLayer->next();
        }
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        auto* fromLayer = &(from.*m_layersGetter)();
        auto* toLayer = &(to.*m_layersGetter)();
        auto* dstLayer = &(destination.*m_layersGetter)();

        while (fromLayer && toLayer && dstLayer) {
            m_fillLayerPropertyWrapper->logBlend(dstLayer, fromLayer, toLayer, progress);
            fromLayer = fromLayer->next();
            toLayer = toLayer->next();
            dstLayer = dstLayer->next();
        }
    }
#endif

    std::unique_ptr<FillLayerAnimationPropertyWrapperBase> m_fillLayerPropertyWrapper;
    LayersGetter m_layersGetter;
    LayersAccessor m_layersAccessor;
};

class ShorthandPropertyWrapper final : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ShorthandPropertyWrapper(CSSPropertyID property, Vector<AnimationPropertyWrapperBase*> longhandWrappers)
        : AnimationPropertyWrapperBase(property)
        , m_propertyWrappers(WTFMove(longhandWrappers))
    {
    }

    bool isShorthandWrapper() const final { return true; }

    const Vector<AnimationPropertyWrapperBase*>& propertyWrappers() const { return m_propertyWrappers; }

private:
    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        if (&a == &b)
            return true;

        for (auto& wrapper : m_propertyWrappers) {
            if (!wrapper->equals(a, b))
                return false;
        }
        return true;
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        for (auto& wrapper : m_propertyWrappers)
            wrapper->blend(destination, from, to, context);
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        for (auto& wrapper : m_propertyWrappers)
            wrapper->logBlend(from, to, destination, progress);
    }
#endif

    Vector<AnimationPropertyWrapperBase*> m_propertyWrappers;
};

class PropertyWrapperFlex final : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperFlex()
        : AnimationPropertyWrapperBase(CSSPropertyFlex)
    {
    }

private:
    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        if (&a == &b)
            return true;

        return a.flexBasis() == b.flexBasis() && a.flexGrow() == b.flexGrow() && a.flexShrink() == b.flexShrink();
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to) const final
    {
        return from.flexGrow() != to.flexGrow() && from.flexShrink() != to.flexShrink() && canInterpolateLengths(from.flexBasis(), to.flexBasis(), false);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        destination.setFlexBasis(blendFunc(from.flexBasis(), to.flexBasis(), context));
        destination.setFlexGrow(blendFunc(from.flexGrow(), to.flexGrow(), context));
        destination.setFlexShrink(blendFunc(from.flexShrink(), to.flexShrink(), context));
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle&, const RenderStyle&, const RenderStyle&, double progress) const final
    {
        // FIXME: better logging.
        LOG_WITH_STREAM(Animations, stream << "  blending flex at " << TextStream::FormatNumberRespectingIntegers(progress));
    }
#endif
};

class PropertyWrapperSVGPaint final : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperSVGPaint(CSSPropertyID property, SVGPaintType (RenderStyle::*paintTypeGetter)() const, Color (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Color&))
        : AnimationPropertyWrapperBase(property)
        , m_paintTypeGetter(paintTypeGetter)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

private:
    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        if (&a == &b)
            return true;

        if ((a.*m_paintTypeGetter)() != (b.*m_paintTypeGetter)())
            return false;

        // We only support animations between SVGPaints that are pure Color values.
        // For everything else we must return true for this method, otherwise
        // we will try to animate between values forever.
        if ((a.*m_paintTypeGetter)() == SVGPaintType::RGBColor) {
            Color fromColor = (a.*m_getter)();
            Color toColor = (b.*m_getter)();

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

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        if ((from.*m_paintTypeGetter)() != SVGPaintType::RGBColor
            || (to.*m_paintTypeGetter)() != SVGPaintType::RGBColor)
            return;

        Color fromColor = (from.*m_getter)();
        Color toColor = (to.*m_getter)();

        if (!fromColor.isValid() && !toColor.isValid())
            return;

        if (!fromColor.isValid())
            fromColor = Color();
        if (!toColor.isValid())
            toColor = Color();
        (destination.*m_setter)(blendFunc(fromColor, toColor, context));
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle&, const RenderStyle&, const RenderStyle&, double progress) const final
    {
        // FIXME: better logging.
        LOG_WITH_STREAM(Animations, stream << "  blending SVGPaint at " << TextStream::FormatNumberRespectingIntegers(progress));
    }
#endif

    SVGPaintType (RenderStyle::*m_paintTypeGetter)() const;
    Color (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(const Color&);
};

class PropertyWrapperFontStyle final : public PropertyWrapper<std::optional<FontSelectionValue>> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperFontStyle()
        : PropertyWrapper<std::optional<FontSelectionValue>>(CSSPropertyFontStyle, &RenderStyle::fontItalic, &RenderStyle::setFontItalic)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to) const final
    {
        return from.fontItalic() && to.fontItalic() && from.fontDescription().fontStyleAxis() == FontStyleAxis::slnt && to.fontDescription().fontStyleAxis() == FontStyleAxis::slnt;
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        auto blendedStyleAxis = FontStyleAxis::slnt;
        if (context.isDiscrete)
            blendedStyleAxis = (context.progress < 0.5 ? from : to).fontDescription().fontStyleAxis();

        auto fromFontItalic = from.fontItalic();
        auto toFontItalic = to.fontItalic();
        auto blendedFontItalic = context.progress < 0.5 ? fromFontItalic : toFontItalic;
        if (!context.isDiscrete)
            blendedFontItalic = blendFunc(fromFontItalic, toFontItalic, context);

        auto* currentFontSelector = destination.fontCascade().fontSelector();
        auto description = destination.fontDescription();
        description.setItalic(blendedFontItalic);
        description.setFontStyleAxis(blendedStyleAxis);
        destination.setFontDescription(WTFMove(description));
        destination.fontCascade().update(currentFontSelector);
    }
};

template <typename T>
class AutoPropertyWrapper final : public PropertyWrapper<T> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    AutoPropertyWrapper(CSSPropertyID property, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T), bool (RenderStyle::*autoGetter)() const, void (RenderStyle::*autoSetter)(), std::optional<T> minValue = std::nullopt)
        : PropertyWrapper<T>(property, getter, setter)
        , m_autoGetter(autoGetter)
        , m_autoSetter(autoSetter)
        , m_minValue(minValue)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to) const final
    {
        return !(from.*m_autoGetter)() && !(to.*m_autoGetter)();
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        auto blendedValue = blendFunc(this->value(from), this->value(to), context);
        if (m_minValue)
            blendedValue = blendedValue > *m_minValue ? blendedValue : *m_minValue;
        (destination.*this->m_setter)(blendedValue);

        if (!context.isDiscrete)
            return;

        ASSERT(!context.progress || context.progress == 1.0);
        if (!context.progress) {
            if ((from.*m_autoGetter)())
                (destination.*m_autoSetter)();
        } else {
            if ((to.*m_autoGetter)())
                (destination.*m_autoSetter)();
        }
    }

    bool (RenderStyle::*m_autoGetter)() const;
    void (RenderStyle::*m_autoSetter)();
    std::optional<T> m_minValue;
};

class NonNegativeFloatPropertyWrapper : public PropertyWrapper<float> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    NonNegativeFloatPropertyWrapper(CSSPropertyID property, float (RenderStyle::*getter)() const, void (RenderStyle::*setter)(float))
        : PropertyWrapper<float>(property, getter, setter)
    {
    }

protected:
    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const override
    {
        auto blendedValue = blendFunc(value(from), value(to), context);
        (destination.*m_setter)(blendedValue > 0 ? blendedValue : 0);
    }
};

class VerticalAlignWrapper final : public LengthPropertyWrapper {
    WTF_MAKE_FAST_ALLOCATED;
public:
    VerticalAlignWrapper()
        : LengthPropertyWrapper(CSSPropertyVerticalAlign, &RenderStyle::verticalAlignLength, &RenderStyle::setVerticalAlignLength, LengthPropertyWrapper::Flags::IsLengthPercentage)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to) const final
    {
        return from.verticalAlign() == VerticalAlign::Length && to.verticalAlign() == VerticalAlign::Length && LengthPropertyWrapper::canInterpolate(from, to);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        LengthPropertyWrapper::blend(destination, from, to, context);
        auto& blendingStyle = context.isDiscrete && context.progress ? to : from;
        destination.setVerticalAlign(blendingStyle.verticalAlign());
    }
};

class TextIndentWrapper final : public LengthPropertyWrapper {
    WTF_MAKE_FAST_ALLOCATED;
public:
    TextIndentWrapper()
        : LengthPropertyWrapper(CSSPropertyTextIndent, &RenderStyle::textIndent, &RenderStyle::setTextIndent, LengthPropertyWrapper::Flags::IsLengthPercentage)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to) const final
    {
        if (from.textIndentLine() != to.textIndentLine())
            return false;
        if (from.textIndentType() != to.textIndentType())
            return false;
        return LengthPropertyWrapper::canInterpolate(from, to);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        auto& blendingStyle = context.isDiscrete && context.progress ? to : from;
        destination.setTextIndentLine(blendingStyle.textIndentLine());
        destination.setTextIndentType(blendingStyle.textIndentType());
        LengthPropertyWrapper::blend(destination, from, to, context);
    }
};

class PerspectiveWrapper final : public NonNegativeFloatPropertyWrapper {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PerspectiveWrapper()
        : NonNegativeFloatPropertyWrapper(CSSPropertyPerspective, &RenderStyle::perspective, &RenderStyle::setPerspective)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to) const final
    {
        if (!from.hasPerspective() || !to.hasPerspective())
            return false;
        return NonNegativeFloatPropertyWrapper::canInterpolate(from, to);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        if (context.isDiscrete)
            (destination.*m_setter)(context.progress ? value(to) : value(from));
        else
            NonNegativeFloatPropertyWrapper::blend(destination, from, to, context);
    }
};

class TabSizePropertyWrapper final : public PropertyWrapper<const TabSize&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    TabSizePropertyWrapper()
        : PropertyWrapper<const TabSize&>(CSSPropertyTabSize, &RenderStyle::tabSize, &RenderStyle::setTabSize)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to) const final
    {
        return value(from).isSpaces() == value(to).isSpaces();
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        if (context.isDiscrete)
            (destination.*m_setter)(context.progress ? value(to) : value(from));
        else
            PropertyWrapper::blend(destination, from, to, context);
    }
};

class PropertyWrapperAspectRatio final : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperAspectRatio()
        : AnimationPropertyWrapperBase(CSSPropertyAspectRatio)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        if (&a == &b)
            return true;

        return a.aspectRatioType() == b.aspectRatioType() && a.aspectRatioWidth() == b.aspectRatioWidth() && a.aspectRatioHeight() == b.aspectRatioHeight();
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to) const final
    {
        return (from.aspectRatioType() == AspectRatioType::Ratio && to.aspectRatioType() == AspectRatioType::Ratio) || (from.aspectRatioType() == AspectRatioType::AutoAndRatio && to.aspectRatioType() == AspectRatioType::AutoAndRatio);
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << getPropertyName(property()) << " from " << from.logicalAspectRatio() << " to " << to.logicalAspectRatio() << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << destination.logicalAspectRatio());
    }
#endif

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        destination.setAspectRatioType(context.progress < 0.5 ? from.aspectRatioType() : to.aspectRatioType());
        if (!context.isDiscrete) {
            auto aspectRatioDst = WebCore::blend(log(from.logicalAspectRatio()), log(to.logicalAspectRatio()), context);
            destination.setAspectRatio(exp(aspectRatioDst), 1);
            return;
        }
        // For auto/auto-zero aspect-ratio we use discrete values, we can't use general
        // logic since logicalAspectRatio asserts on aspect-ratio type.
        ASSERT(!context.progress || context.progress == 1);
        auto& applicableStyle = context.progress ? to : from;
        destination.setAspectRatio(applicableStyle.aspectRatioWidth(), applicableStyle.aspectRatioHeight());
    }
};

class CSSPropertyAnimationWrapperMap final {
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
        new LengthPropertyWrapper(CSSPropertyLeft, &RenderStyle::left, &RenderStyle::setLeft, { LengthPropertyWrapper::Flags::IsLengthPercentage }),
        new LengthPropertyWrapper(CSSPropertyRight, &RenderStyle::right, &RenderStyle::setRight, { LengthPropertyWrapper::Flags::IsLengthPercentage }),
        new LengthPropertyWrapper(CSSPropertyTop, &RenderStyle::top, &RenderStyle::setTop, { LengthPropertyWrapper::Flags::IsLengthPercentage }),
        new LengthPropertyWrapper(CSSPropertyBottom, &RenderStyle::bottom, &RenderStyle::setBottom, { LengthPropertyWrapper::Flags::IsLengthPercentage }),

        new LengthPropertyWrapper(CSSPropertyWidth, &RenderStyle::width, &RenderStyle::setWidth, { LengthPropertyWrapper::Flags::IsLengthPercentage, LengthPropertyWrapper::Flags::NegativeLengthsAreInvalid }),
        new LengthPropertyWrapper(CSSPropertyMinWidth, &RenderStyle::minWidth, &RenderStyle::setMinWidth, { LengthPropertyWrapper::Flags::IsLengthPercentage, LengthPropertyWrapper::Flags::NegativeLengthsAreInvalid }),
        new LengthPropertyWrapper(CSSPropertyMaxWidth, &RenderStyle::maxWidth, &RenderStyle::setMaxWidth, { LengthPropertyWrapper::Flags::IsLengthPercentage, LengthPropertyWrapper::Flags::NegativeLengthsAreInvalid }),

        new LengthPropertyWrapper(CSSPropertyHeight, &RenderStyle::height, &RenderStyle::setHeight, { LengthPropertyWrapper::Flags::IsLengthPercentage, LengthPropertyWrapper::Flags::NegativeLengthsAreInvalid }),
        new LengthPropertyWrapper(CSSPropertyMinHeight, &RenderStyle::minHeight, &RenderStyle::setMinHeight, { LengthPropertyWrapper::Flags::IsLengthPercentage, LengthPropertyWrapper::Flags::NegativeLengthsAreInvalid }),
        new LengthPropertyWrapper(CSSPropertyMaxHeight, &RenderStyle::maxHeight, &RenderStyle::setMaxHeight, { LengthPropertyWrapper::Flags::IsLengthPercentage, LengthPropertyWrapper::Flags::NegativeLengthsAreInvalid }),

        new PropertyWrapperFlex(),

        new NonNegativeFloatPropertyWrapper(CSSPropertyBorderLeftWidth, &RenderStyle::borderLeftWidth, &RenderStyle::setBorderLeftWidth),
        new NonNegativeFloatPropertyWrapper(CSSPropertyBorderRightWidth, &RenderStyle::borderRightWidth, &RenderStyle::setBorderRightWidth),
        new NonNegativeFloatPropertyWrapper(CSSPropertyBorderTopWidth, &RenderStyle::borderTopWidth, &RenderStyle::setBorderTopWidth),
        new NonNegativeFloatPropertyWrapper(CSSPropertyBorderBottomWidth, &RenderStyle::borderBottomWidth, &RenderStyle::setBorderBottomWidth),
        new LengthPropertyWrapper(CSSPropertyMarginLeft, &RenderStyle::marginLeft, &RenderStyle::setMarginLeft),
        new LengthPropertyWrapper(CSSPropertyMarginRight, &RenderStyle::marginRight, &RenderStyle::setMarginRight),
        new LengthPropertyWrapper(CSSPropertyMarginTop, &RenderStyle::marginTop, &RenderStyle::setMarginTop),
        new LengthPropertyWrapper(CSSPropertyMarginBottom, &RenderStyle::marginBottom, &RenderStyle::setMarginBottom),
        new LengthPropertyWrapper(CSSPropertyPaddingLeft, &RenderStyle::paddingLeft, &RenderStyle::setPaddingLeft, { LengthPropertyWrapper::Flags::NegativeLengthsAreInvalid }),
        new LengthPropertyWrapper(CSSPropertyPaddingRight, &RenderStyle::paddingRight, &RenderStyle::setPaddingRight, { LengthPropertyWrapper::Flags::NegativeLengthsAreInvalid }),
        new LengthPropertyWrapper(CSSPropertyPaddingTop, &RenderStyle::paddingTop, &RenderStyle::setPaddingTop, { LengthPropertyWrapper::Flags::NegativeLengthsAreInvalid }),
        new LengthPropertyWrapper(CSSPropertyPaddingBottom, &RenderStyle::paddingBottom, &RenderStyle::setPaddingBottom, { LengthPropertyWrapper::Flags::NegativeLengthsAreInvalid }),

        new CaretColorPropertyWrapper,

        new PropertyWrapperVisitedAffectedColor(CSSPropertyColor, &RenderStyle::color, &RenderStyle::setColor, &RenderStyle::visitedLinkColor, &RenderStyle::setVisitedLinkColor),

        new PropertyWrapperVisitedAffectedColor(CSSPropertyBackgroundColor, MaybeInvalidColor, &RenderStyle::backgroundColor, &RenderStyle::setBackgroundColor, &RenderStyle::visitedLinkBackgroundColor, &RenderStyle::setVisitedLinkBackgroundColor),

        new FillLayersPropertyWrapper(CSSPropertyBackgroundImage, &RenderStyle::backgroundLayers, &RenderStyle::ensureBackgroundLayers),
        new StyleImagePropertyWrapper(CSSPropertyListStyleImage, &RenderStyle::listStyleImage, &RenderStyle::setListStyleImage),
        new StyleImagePropertyWrapper(CSSPropertyWebkitMaskImage, &RenderStyle::maskImage, &RenderStyle::setMaskImage),

        new StyleImagePropertyWrapper(CSSPropertyBorderImageSource, &RenderStyle::borderImageSource, &RenderStyle::setBorderImageSource),
        new LengthBoxPropertyWrapper(CSSPropertyBorderImageSlice, &RenderStyle::borderImageSlices, &RenderStyle::setBorderImageSlices, { LengthBoxPropertyWrapper::Flags::UsesFillKeyword }),
        new LengthBoxPropertyWrapper(CSSPropertyBorderImageWidth, &RenderStyle::borderImageWidth, &RenderStyle::setBorderImageWidth, { LengthBoxPropertyWrapper::Flags::IsLengthPercentage }),
        new LengthBoxPropertyWrapper(CSSPropertyBorderImageOutset, &RenderStyle::borderImageOutset, &RenderStyle::setBorderImageOutset),

        new StyleImagePropertyWrapper(CSSPropertyWebkitMaskBoxImageSource, &RenderStyle::maskBoxImageSource, &RenderStyle::setMaskBoxImageSource),
        new PropertyWrapper<const NinePieceImage&>(CSSPropertyWebkitMaskBoxImage, &RenderStyle::maskBoxImage, &RenderStyle::setMaskBoxImage),

        new FillLayersPropertyWrapper(CSSPropertyBackgroundPositionX, &RenderStyle::backgroundLayers, &RenderStyle::ensureBackgroundLayers),
        new FillLayersPropertyWrapper(CSSPropertyBackgroundPositionY, &RenderStyle::backgroundLayers, &RenderStyle::ensureBackgroundLayers),
        new FillLayersPropertyWrapper(CSSPropertyBackgroundSize, &RenderStyle::backgroundLayers, &RenderStyle::ensureBackgroundLayers),
        new FillLayersPropertyWrapper(CSSPropertyWebkitBackgroundSize, &RenderStyle::backgroundLayers, &RenderStyle::ensureBackgroundLayers),

        new FillLayersPropertyWrapper(CSSPropertyWebkitMaskPositionX, &RenderStyle::maskLayers, &RenderStyle::ensureMaskLayers),
        new FillLayersPropertyWrapper(CSSPropertyWebkitMaskPositionY, &RenderStyle::maskLayers, &RenderStyle::ensureMaskLayers),
        new FillLayersPropertyWrapper(CSSPropertyWebkitMaskSize, &RenderStyle::maskLayers, &RenderStyle::ensureMaskLayers),

        new LengthPointPropertyWrapper(CSSPropertyObjectPosition, &RenderStyle::objectPosition, &RenderStyle::setObjectPosition),

        new PropertyWrapper<float>(CSSPropertyFontSize, &RenderStyle::computedFontSize, &RenderStyle::setFontSize),
        new PropertyWrapper<unsigned short>(CSSPropertyColumnRuleWidth, &RenderStyle::columnRuleWidth, &RenderStyle::setColumnRuleWidth),
        new LengthVariantPropertyWrapper<GapLength>(CSSPropertyColumnGap, &RenderStyle::columnGap, &RenderStyle::setColumnGap),
        new LengthVariantPropertyWrapper<GapLength>(CSSPropertyRowGap, &RenderStyle::rowGap, &RenderStyle::setRowGap),
        new AutoPropertyWrapper<unsigned short>(CSSPropertyColumnCount, &RenderStyle::columnCount, &RenderStyle::setColumnCount, &RenderStyle::hasAutoColumnCount, &RenderStyle::setHasAutoColumnCount, 1),
        new AutoPropertyWrapper<float>(CSSPropertyColumnWidth, &RenderStyle::columnWidth, &RenderStyle::setColumnWidth, &RenderStyle::hasAutoColumnWidth, &RenderStyle::setHasAutoColumnWidth, 0),
        new NonNegativeFloatPropertyWrapper(CSSPropertyWebkitBorderHorizontalSpacing, &RenderStyle::horizontalBorderSpacing, &RenderStyle::setHorizontalBorderSpacing),
        new NonNegativeFloatPropertyWrapper(CSSPropertyWebkitBorderVerticalSpacing, &RenderStyle::verticalBorderSpacing, &RenderStyle::setVerticalBorderSpacing),
        new AutoPropertyWrapper<int>(CSSPropertyZIndex, &RenderStyle::specifiedZIndex, &RenderStyle::setSpecifiedZIndex, &RenderStyle::hasAutoSpecifiedZIndex, &RenderStyle::setHasAutoSpecifiedZIndex),
        new PositivePropertyWrapper<unsigned short>(CSSPropertyOrphans, &RenderStyle::orphans, &RenderStyle::setOrphans),
        new PositivePropertyWrapper<unsigned short>(CSSPropertyWidows, &RenderStyle::widows, &RenderStyle::setWidows),
        new LengthPropertyWrapper(CSSPropertyLineHeight, &RenderStyle::specifiedLineHeight, &RenderStyle::setLineHeight),
        new PropertyWrapper<float>(CSSPropertyOutlineOffset, &RenderStyle::outlineOffset, &RenderStyle::setOutlineOffset),
        new NonNegativeFloatPropertyWrapper(CSSPropertyOutlineWidth, &RenderStyle::outlineWidth, &RenderStyle::setOutlineWidth),
        new PropertyWrapper<float>(CSSPropertyLetterSpacing, &RenderStyle::letterSpacing, &RenderStyle::setLetterSpacing),
        new LengthPropertyWrapper(CSSPropertyWordSpacing, &RenderStyle::wordSpacing, &RenderStyle::setWordSpacing),
        new TextIndentWrapper,
        new VerticalAlignWrapper,

        new PerspectiveWrapper,
        new LengthPropertyWrapper(CSSPropertyPerspectiveOriginX, &RenderStyle::perspectiveOriginX, &RenderStyle::setPerspectiveOriginX, LengthPropertyWrapper::Flags::IsLengthPercentage),
        new LengthPropertyWrapper(CSSPropertyPerspectiveOriginY, &RenderStyle::perspectiveOriginY, &RenderStyle::setPerspectiveOriginY, LengthPropertyWrapper::Flags::IsLengthPercentage),
        new LengthPropertyWrapper(CSSPropertyTransformOriginX, &RenderStyle::transformOriginX, &RenderStyle::setTransformOriginX, LengthPropertyWrapper::Flags::IsLengthPercentage),
        new LengthPropertyWrapper(CSSPropertyTransformOriginY, &RenderStyle::transformOriginY, &RenderStyle::setTransformOriginY, LengthPropertyWrapper::Flags::IsLengthPercentage),
        new PropertyWrapper<float>(CSSPropertyTransformOriginZ, &RenderStyle::transformOriginZ, &RenderStyle::setTransformOriginZ),
        new LengthVariantPropertyWrapper<LengthSize>(CSSPropertyBorderTopLeftRadius, &RenderStyle::borderTopLeftRadius, &RenderStyle::setBorderTopLeftRadius),
        new LengthVariantPropertyWrapper<LengthSize>(CSSPropertyBorderTopRightRadius, &RenderStyle::borderTopRightRadius, &RenderStyle::setBorderTopRightRadius),
        new LengthVariantPropertyWrapper<LengthSize>(CSSPropertyBorderBottomLeftRadius, &RenderStyle::borderBottomLeftRadius, &RenderStyle::setBorderBottomLeftRadius),
        new LengthVariantPropertyWrapper<LengthSize>(CSSPropertyBorderBottomRightRadius, &RenderStyle::borderBottomRightRadius, &RenderStyle::setBorderBottomRightRadius),
        new PropertyWrapper<Visibility>(CSSPropertyVisibility, &RenderStyle::visibility, &RenderStyle::setVisibility),
        new PropertyWrapper<float>(CSSPropertyZoom, &RenderStyle::zoom, &RenderStyle::setZoomWithoutReturnValue),

        new ClipWrapper,

        new AcceleratedPropertyWrapper<float>(CSSPropertyOpacity, &RenderStyle::opacity, &RenderStyle::setOpacity),
        new AcceleratedPropertyWrapper<const TransformOperations&>(CSSPropertyTransform, &RenderStyle::transform, &RenderStyle::setTransform),

        new AcceleratedIndividualTransformPropertyWrapper<ScaleTransformOperation>(CSSPropertyScale, &RenderStyle::scale, &RenderStyle::setScale),
        new AcceleratedIndividualTransformPropertyWrapper<RotateTransformOperation>(CSSPropertyRotate, &RenderStyle::rotate, &RenderStyle::setRotate),
        new AcceleratedIndividualTransformPropertyWrapper<TranslateTransformOperation>(CSSPropertyTranslate, &RenderStyle::translate, &RenderStyle::setTranslate),

        new PropertyWrapperFilter(CSSPropertyFilter, &RenderStyle::filter, &RenderStyle::setFilter),
#if ENABLE(FILTERS_LEVEL_2)
        new PropertyWrapperFilter(CSSPropertyWebkitBackdropFilter, &RenderStyle::backdropFilter, &RenderStyle::setBackdropFilter),
#endif
        new PropertyWrapperFilter(CSSPropertyAppleColorFilter, &RenderStyle::appleColorFilter, &RenderStyle::setAppleColorFilter),

        new PropertyWrapperClipPath(CSSPropertyClipPath, &RenderStyle::clipPath, &RenderStyle::setClipPath),

        new PropertyWrapperShape(CSSPropertyShapeOutside, &RenderStyle::shapeOutside, &RenderStyle::setShapeOutside),
        new LengthPropertyWrapper(CSSPropertyShapeMargin, &RenderStyle::shapeMargin, &RenderStyle::setShapeMargin, { LengthPropertyWrapper::Flags::NegativeLengthsAreInvalid }),
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

        new LengthPropertyWrapper(CSSPropertyFlexBasis, &RenderStyle::flexBasis, &RenderStyle::setFlexBasis, { LengthPropertyWrapper::Flags::NegativeLengthsAreInvalid }),
        new NonNegativeFloatPropertyWrapper(CSSPropertyFlexGrow, &RenderStyle::flexGrow, &RenderStyle::setFlexGrow),
        new NonNegativeFloatPropertyWrapper(CSSPropertyFlexShrink, &RenderStyle::flexShrink, &RenderStyle::setFlexShrink),
        new PropertyWrapper<int>(CSSPropertyOrder, &RenderStyle::order, &RenderStyle::setOrder),

        new TabSizePropertyWrapper,

        // FIXME: The following properties are currently not animatable but should be:
        // background-blend-mode, clip-rule, color-interpolation,
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
        new DiscretePropertyWrapper<BackfaceVisibility>(CSSPropertyBackfaceVisibility, &RenderStyle::backfaceVisibility, &RenderStyle::setBackfaceVisibility),
        new DiscretePropertyWrapper<FillAttachment>(CSSPropertyBackgroundAttachment, &RenderStyle::backgroundAttachment, &RenderStyle::setBackgroundAttachment),
        new DiscretePropertyWrapper<FillBox>(CSSPropertyBackgroundClip, &RenderStyle::backgroundClip, &RenderStyle::setBackgroundClip),
        new DiscretePropertyWrapper<FillBox>(CSSPropertyBackgroundOrigin, &RenderStyle::backgroundOrigin, &RenderStyle::setBackgroundOrigin),
        new DiscretePropertyWrapper<FillRepeat>(CSSPropertyBackgroundRepeatX, &RenderStyle::backgroundRepeatX, &RenderStyle::setBackgroundRepeatX),
        new DiscretePropertyWrapper<FillRepeat>(CSSPropertyBackgroundRepeatY, &RenderStyle::backgroundRepeatY, &RenderStyle::setBackgroundRepeatY),
        new DiscretePropertyWrapper<BorderStyle>(CSSPropertyBorderBottomStyle, &RenderStyle::borderBottomStyle, &RenderStyle::setBorderBottomStyle),
        new DiscretePropertyWrapper<BorderCollapse>(CSSPropertyBorderCollapse, &RenderStyle::borderCollapse, &RenderStyle::setBorderCollapse),
        new BorderImageRepeatWrapper,
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
        new GridTemplateTracksWrapper<ForColumns>,
        new GridTemplateTracksWrapper<ForRows>,
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
        new DiscretePropertyWrapper<BreakBetween>(CSSPropertyBreakAfter, &RenderStyle::breakAfter, &RenderStyle::setBreakAfter),
        new DiscretePropertyWrapper<BreakBetween>(CSSPropertyBreakBefore, &RenderStyle::breakBefore, &RenderStyle::setBreakBefore),
        new DiscretePropertyWrapper<BreakInside>(CSSPropertyBreakInside, &RenderStyle::breakInside, &RenderStyle::setBreakInside),
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
        new PropertyWrapperAspectRatio,
    };
    const unsigned animatableLonghandPropertiesCount = WTF_ARRAY_LENGTH(animatableLonghandPropertyWrappers);

    static const CSSPropertyID animatableShorthandProperties[] = {
        CSSPropertyBackground, // for background-color, background-position, background-image
        CSSPropertyBackgroundPosition,
        CSSPropertyBackgroundRepeat,
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
        CSSPropertyPageBreakAfter,
        CSSPropertyPageBreakBefore,
        CSSPropertyPageBreakInside,
        CSSPropertyWebkitTextStroke,
        CSSPropertyColumnRule,
        CSSPropertyWebkitBorderRadius,
        CSSPropertyTransformOrigin,
        CSSPropertyPerspectiveOrigin
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

void CSSPropertyAnimation::blendProperties(const CSSPropertyBlendingClient* client, CSSPropertyID property, RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, double progress)
{
    ASSERT(property != CSSPropertyInvalid);

    AnimationPropertyWrapperBase* wrapper = CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(property);
    if (wrapper) {
        // https://drafts.csswg.org/web-animations-1/#discrete
        // The property's values cannot be meaningfully combined, thus it is not additive and
        // interpolation swaps from Va to Vb at 50% (p=0.5).
        auto isDiscrete = !wrapper->canInterpolate(from, to);
        if (isDiscrete)
            progress = progress < 0.5 ? 0 : 1;
        wrapper->blend(destination, from, to, { progress, isDiscrete, client });
#if !LOG_DISABLED
        wrapper->logBlend(from, to, destination, progress);
#endif
    }
}

bool CSSPropertyAnimation::isPropertyAnimatable(CSSPropertyID property)
{
    return CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(property);
}

bool CSSPropertyAnimation::animationOfPropertyIsAccelerated(CSSPropertyID property)
{
    AnimationPropertyWrapperBase* wrapper = CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(property);
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

bool CSSPropertyAnimation::propertiesEqual(CSSPropertyID property, const RenderStyle& a, const RenderStyle& b)
{
    AnimationPropertyWrapperBase* wrapper = CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(property);
    if (wrapper)
        return wrapper->equals(a, b);
    return true;
}

bool CSSPropertyAnimation::canPropertyBeInterpolated(CSSPropertyID property, const RenderStyle& from, const RenderStyle& to)
{
    AnimationPropertyWrapperBase* wrapper = CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(property);
    if (wrapper)
        return wrapper->canInterpolate(from, to);
    return false;
}

CSSPropertyID CSSPropertyAnimation::getPropertyAtIndex(int i, std::optional<bool>& isShorthand)
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
