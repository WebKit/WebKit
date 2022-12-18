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
#include "CSSCustomPropertyValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyBlendingClient.h"
#include "CSSPropertyNames.h"
#include "CSSRegisteredCustomProperty.h"
#include "CachedImage.h"
#include "CalculationValue.h"
#include "ColorBlending.h"
#include "ComputedStyleExtractor.h"
#include "ContentData.h"
#include "CounterDirectives.h"
#include "Document.h"
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
#include "OffsetRotation.h"
#include "PathOperation.h"
#include "QuotesData.h"
#include "RenderBox.h"
#include "RenderStyle.h"
#include "StyleCachedImage.h"
#include "StyleCrossfadeImage.h"
#include "StyleFilterImage.h"
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

#if !LOG_DISABLED

static TextStream& operator<<(TextStream& stream, CSSPropertyID property)
{
    return stream << nameLiteral(property);
}

#endif

struct CSSPropertyBlendingContext : BlendingContext {
    const CSSPropertyBlendingClient& client;
    AnimatableProperty property;

    CSSPropertyBlendingContext(double progress, bool isDiscrete, CompositeOperation compositeOperation, const CSSPropertyBlendingClient& client, AnimatableProperty property, IterationCompositeOperation iterationCompositeOperation = IterationCompositeOperation::Replace, double currentIteration = 0)
        : BlendingContext(progress, isDiscrete, compositeOperation, iterationCompositeOperation, currentIteration)
        , client(client)
        , property(property)
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
    if (context.iterationCompositeOperation == IterationCompositeOperation::Accumulate && context.currentIteration) {
        auto iterationIncrement = context.currentIteration * to;
        from += iterationIncrement;
        to += iterationIncrement;
    }

    if (context.compositeOperation == CompositeOperation::Replace)
        return narrowPrecisionToFloat(from + (to - from) * context.progress);
    return narrowPrecisionToFloat(from + from + (to - from) * context.progress);
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
    return blend(from, to, context, ValueRange::NonNegative);
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
        blend(from->radius(), to->radius(), context, ValueRange::NonNegative),
        blend(from->spread(), to->spread(), context),
        blendFunc(from->style(), to->style(), context),
        from->isWebkitBoxShadow(),
        blend(from->color(), to->color(), context));
}

static inline TransformOperations blendFunc(const TransformOperations& from, const TransformOperations& to, const CSSPropertyBlendingContext& context)
{
    if (context.compositeOperation == CompositeOperation::Add) {
        ASSERT(context.progress == 1.0);
        TransformOperations resultOperations;
        resultOperations.operations().appendVector(from.operations());
        resultOperations.operations().appendVector(to.operations());
        return resultOperations;
    }

    auto prefix = [&]() -> std::optional<unsigned> {
        // We cannot use the pre-computed prefix when dealing with accumulation
        // since the values used to accumulate may be different than those held
        // in the initial keyframe list. We must do the same with any property
        // other than "transform" since we only pre-compute the prefix for that
        // property.
        if (context.compositeOperation == CompositeOperation::Accumulate || std::holds_alternative<AtomString>(context.property) || std::get<CSSPropertyID>(context.property) != CSSPropertyTransform)
            return std::nullopt;
        return context.client.transformFunctionListPrefix();
    };

    auto boxSize = is<RenderBox>(context.client.renderer()) ? downcast<RenderBox>(*context.client.renderer()).borderBoxRect().size() : LayoutSize();
    return to.blend(from, context, boxSize, prefix());
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
            normalizedFrom = ScaleTransformOperation::create(from->x(), from->y(), from->z(), TransformOperation::Type::Scale3D);
            normalizedTo = ScaleTransformOperation::create(to->x(), to->y(), to->z(), TransformOperation::Type::Scale3D);
        } else {
            normalizedFrom = ScaleTransformOperation::create(from->x(), from->y(), TransformOperation::Type::Scale);
            normalizedTo = ScaleTransformOperation::create(to->x(), to->y(), TransformOperation::Type::Scale);
        }
        return blendFunc(normalizedFrom.get(), normalizedTo.get(), context);
    }

    auto blendedOperation = to->blend(from, context);
    if (is<ScaleTransformOperation>(blendedOperation)) {
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
            normalizedFrom = RotateTransformOperation::create(from->x(), from->y(), from->z(), from->angle(), TransformOperation::Type::Rotate3D);
            normalizedTo = RotateTransformOperation::create(to->x(), to->y(), to->z(), to->angle(), TransformOperation::Type::Rotate3D);
        } else {
            normalizedFrom = RotateTransformOperation::create(from->angle(), TransformOperation::Type::Rotate);
            normalizedTo = RotateTransformOperation::create(to->angle(), TransformOperation::Type::Rotate);
        }
        return blendFunc(normalizedFrom.get(), normalizedTo.get(), context);
    }

    auto blendedOperation = to->blend(from, context);
    if (is<RotateTransformOperation>(blendedOperation)) {
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
            normalizedFrom = TranslateTransformOperation::create(from->x(), from->y(), from->z(), TransformOperation::Type::Translate3D);
            normalizedTo = TranslateTransformOperation::create(to->x(), to->y(), to->z(), TransformOperation::Type::Translate3D);
        } else {
            normalizedFrom = TranslateTransformOperation::create(from->x(), from->y(), TransformOperation::Type::Translate);
            normalizedTo = TranslateTransformOperation::create(to->x(), to->y(), TransformOperation::Type::Translate);
        }
        return blendFunc(normalizedFrom.get(), normalizedTo.get(), context);
    }

    Ref<TransformOperation> blendedOperation = to->blend(from, context);
    if (is<TranslateTransformOperation>(blendedOperation)) {
        TranslateTransformOperation& translate = downcast<TranslateTransformOperation>(blendedOperation.get());
        return TranslateTransformOperation::create(translate.x(), translate.y(), translate.z(), translate.type());
    }
    return nullptr;
}

static RefPtr<TransformOperation> blendFunc(TransformOperation* from, TransformOperation* to, const CSSPropertyBlendingContext& context)
{
    if (from && to)
        return to->blend(from, context);
    return nullptr;
}

static inline RefPtr<PathOperation> blendFunc(PathOperation* from, PathOperation* to, const CSSPropertyBlendingContext& context)
{
    if (is<ShapePathOperation>(from) && is<ShapePathOperation>(to)) {
        auto& fromShape = downcast<ShapePathOperation>(*from).basicShape();
        auto& toShape = downcast<ShapePathOperation>(*to).basicShape();

        if (fromShape.canBlend(toShape))
            return ShapePathOperation::create(toShape.blend(fromShape, context));
    }

    if (is<RayPathOperation>(from) && is<RayPathOperation>(to)) {
        auto& fromRay = downcast<RayPathOperation>(*from);
        auto& toRay = downcast<RayPathOperation>(*to);

        if (fromRay.canBlend(toRay))
            return fromRay.blend(toRay, context);
    }

    // fall back to discrete animation.
    return context.progress < 0.5 ? from : to;
}

static inline RefPtr<ShapeValue> blendFunc(ShapeValue* from, ShapeValue* to, const CSSPropertyBlendingContext& context)
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? to : from;
    }

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
    if (context.compositeOperation == CompositeOperation::Add) {
        ASSERT(context.progress == 1.0);
        FilterOperations resultOperations;
        resultOperations.operations().appendVector(from.operations());
        resultOperations.operations().appendVector(to.operations());
        return resultOperations;
    }

    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? to : from;
    }

    FilterOperations result;
    size_t fromSize = from.operations().size();
    size_t toSize = to.operations().size();
    size_t size = std::max(fromSize, toSize);
    for (size_t i = 0; i < size; i++) {
        RefPtr<FilterOperation> fromOp = (i < fromSize) ? from.operations()[i].get() : nullptr;
        RefPtr<FilterOperation> toOp = (i < toSize) ? to.operations()[i].get() : nullptr;
        RefPtr<FilterOperation> blendedOp = toOp ? blendFunc(fromOp.get(), toOp.get(), context) : (fromOp ? blendFunc(0, fromOp.get(), context, true) : nullptr);
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

static inline FilterOperations blendFunc(const FilterOperations& from, const FilterOperations& to, const CSSPropertyBlendingContext& context)
{
    return blendFilterOperations(from, to, context);
}

static inline RefPtr<StyleImage> blendFilter(RefPtr<StyleImage> inputImage, const FilterOperations& from, const FilterOperations& to, const CSSPropertyBlendingContext& context)
{
    auto filterResult = blendFilterOperations(from, to, context);
    return StyleFilterImage::create(WTFMove(inputImage), WTFMove(filterResult));
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
    // The composite operation here is irrelevant.
    double result = blendFunc(fromVal, toVal, { context.progress, false, CompositeOperation::Replace, context.client, context.property });
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

static inline RefPtr<StyleImage> crossfadeBlend(StyleCachedImage& fromStyleImage, StyleCachedImage& toStyleImage, const CSSPropertyBlendingContext& context)
{
    // If progress is at one of the extremes, we want getComputedStyle to show the image,
    // not a completed cross-fade, so we hand back one of the existing images.
    if (!context.progress)
        return &fromStyleImage;
    if (context.progress == 1)
        return &toStyleImage;
    if (!fromStyleImage.cachedImage() || !toStyleImage.cachedImage())
        return &toStyleImage;
    return StyleCrossfadeImage::create(&fromStyleImage, &toStyleImage, context.progress, false);
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
    if (is<StyleFilterImage>(*from) && is<StyleFilterImage>(*to)) {
        // Animation of generated images just possible if input images are equal.
        // Otherwise fall back to cross fade animation.
        auto& fromFilter = downcast<StyleFilterImage>(*from);
        auto& toFilter = downcast<StyleFilterImage>(*to);

        if (fromFilter.equalInputImages(toFilter) && is<StyleCachedImage>(fromFilter.inputImage()))
            return blendFilter(fromFilter.inputImage(), fromFilter.filterOperations(), toFilter.filterOperations(), context);
    } else if (is<StyleCrossfadeImage>(*from) && is<StyleCrossfadeImage>(*to)) {
        auto& fromCrossfade = downcast<StyleCrossfadeImage>(*from);
        auto& toCrossfade = downcast<StyleCrossfadeImage>(*to);

        if (fromCrossfade.equalInputImages(toCrossfade)) {
            if (auto crossfadeBlend = toCrossfade.blend(fromCrossfade, context))
                return crossfadeBlend;
        }
    } else if (is<StyleFilterImage>(*from) && is<StyleCachedImage>(*to)) {
        auto& fromFilter = downcast<StyleFilterImage>(*from);
        auto fromFilterInputImage = fromFilter.inputImage();

        if (is<StyleCachedImage>(fromFilterInputImage) && downcast<StyleCachedImage>(*to).equals(downcast<StyleCachedImage>(*fromFilterInputImage)))
            return blendFilter(WTFMove(fromFilterInputImage), fromFilter.filterOperations(), FilterOperations(), context);
    } else if (is<StyleCachedImage>(*from) && is<StyleFilterImage>(*to)) {
        auto& toFilter = downcast<StyleFilterImage>(*to);
        auto toFilterInputImage = toFilter.inputImage();

        if (is<StyleCachedImage>(toFilterInputImage) && downcast<StyleCachedImage>(*from).equals(downcast<StyleCachedImage>(*toFilterInputImage)))
            return blendFilter(WTFMove(toFilterInputImage), FilterOperations(), toFilter.filterOperations(), context);
    }

    if (is<StyleCachedImage>(*from) && is<StyleCachedImage>(*to))
        return crossfadeBlend(downcast<StyleCachedImage>(*from), downcast<StyleCachedImage>(*to), context);

    // FIXME: Add support for animation between two *gradient() functions.
    // https://bugs.webkit.org/show_bug.cgi?id=119956

    // FIXME: Add support cross fade between cached and generated images.
    // https://bugs.webkit.org/show_bug.cgi?id=78293

    return to;
}

static inline NinePieceImage blendFunc(const NinePieceImage& from, const NinePieceImage& to, const CSSPropertyBlendingContext& context)
{
    if (!from.hasImage() || !to.hasImage())
        return to;

    // FIXME (74112): Support transitioning between NinePieceImages that differ by more than image content.

    if (from.imageSlices() != to.imageSlices() || from.borderSlices() != to.borderSlices() || from.outset() != to.outset() || from.fill() != to.fill() || from.overridesBorderWidths() != to.overridesBorderWidths() || from.horizontalRule() != to.horizontalRule() || from.verticalRule() != to.verticalRule())
        return to;

    if (auto* renderer = context.client.renderer()) {
        if (from.image()->imageSize(renderer, 1.0) != to.image()->imageSize(renderer, 1.0))
            return to;
    }

    return NinePieceImage(blendFunc(from.image(), to.image(), context),
        from.imageSlices(), from.fill(), from.borderSlices(), from.overridesBorderWidths(), from.outset(), from.horizontalRule(), from.verticalRule());
}

#if ENABLE(VARIATION_FONTS)

static inline FontVariationSettings blendFunc(const FontVariationSettings& from, const FontVariationSettings& to, const CSSPropertyBlendingContext& context)
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? to : from;
    }

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

static inline OffsetRotation blendFunc(const OffsetRotation& from, const OffsetRotation& to, const CSSPropertyBlendingContext& context)
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? to : from;
    }

    ASSERT(from.hasAuto() == to.hasAuto());

    return OffsetRotation(from.hasAuto(), clampTo<float>(blend(from.angle(), to.angle(), context)));
}

static inline bool canInterpolate(const GridTrackList& from, const GridTrackList& to)
{
    if (from.size() != to.size())
        return false;

    size_t i = 0;
    auto visitor = WTF::makeVisitor([&](const GridTrackSize&) {
        return std::holds_alternative<GridTrackSize>(to[i]);
    }, [&](const Vector<String>&) {
        return std::holds_alternative<Vector<String>>(to[i]);
    }, [&](const GridTrackEntryRepeat& repeat) {
        if (!std::holds_alternative<GridTrackEntryRepeat>(to[i]))
            return false;
        const auto& toEntry = std::get<GridTrackEntryRepeat>(to[i]);
        return repeat.repeats == toEntry.repeats && repeat.list.size() == toEntry.list.size();
    }, [&](const GridTrackEntryAutoRepeat&) {
        return false;
    }, [&](const GridTrackEntrySubgrid&) {
        return false;
    }, [&](const GridTrackEntryMasonry&) {
        return false;
    });

    for (i = 0; i < from.size(); i++) {
        if (!std::visit(visitor, from[i]))
            return false;
    }

    return true;
}

static inline GridLength blendFunc(const GridLength& from, const GridLength& to, const CSSPropertyBlendingContext& context)
{
    if (from.isFlex() != to.isFlex())
        return context.progress < 0.5 ? from : to;

    if (from.isFlex())
        return GridLength(blend(from.flex(), to.flex(), context));

    return GridLength(blendFunc(from.length(), to.length(), context));
}

static inline GridTrackSize blendFunc(const GridTrackSize& from, const GridTrackSize& to, const CSSPropertyBlendingContext& context)
{
    if (from.type() != to.type())
        return context.progress < 0.5 ? from : to;

    if (from.type() == LengthTrackSizing) {
        auto length = blendFunc(from.minTrackBreadth(), to.minTrackBreadth(), context);
        return GridTrackSize(length, LengthTrackSizing);
    }
    if (from.type() == MinMaxTrackSizing) {
        auto minTrackBreadth = blendFunc(from.minTrackBreadth(), to.minTrackBreadth(), context);
        auto maxTrackBreadth = blendFunc(from.maxTrackBreadth(), to.maxTrackBreadth(), context);
        return GridTrackSize(minTrackBreadth, maxTrackBreadth);
    }

    auto fitContentBreadth = blendFunc(from.fitContentTrackBreadth(), to.fitContentTrackBreadth(), context);
    return GridTrackSize(fitContentBreadth, FitContentTrackSizing);
}

static inline RepeatTrackList blendFunc(const RepeatTrackList& from, const RepeatTrackList& to, const CSSPropertyBlendingContext& context)
{
    RepeatTrackList result;
    size_t i = 0;

    auto visitor = WTF::makeVisitor([&](const GridTrackSize& size) {
        result.append(blendFunc(size, std::get<GridTrackSize>(to[i]), context));
    }, [&](const Vector<String>& names) {
        if (context.progress < 0.5)
            result.append(names);
        else {
            const Vector<String>& toNames = std::get<Vector<String>>(to[i]);
            result.append(toNames);
        }
    });

    for (i = 0; i < from.size(); i++)
        std::visit(visitor, from[i]);

    return result;
}

static inline GridTrackList blendFunc(const GridTrackList& from, const GridTrackList& to, const CSSPropertyBlendingContext& context)
{
    if (!canInterpolate(from, to))
        return context.progress < 0.5 ? from : to;

    GridTrackList result;
    size_t i = 0;

    auto visitor = WTF::makeVisitor([&](const GridTrackSize& size) {
        result.append(blendFunc(size, std::get<GridTrackSize>(to[i]), context));
    }, [&](const Vector<String>& names) {
        if (context.progress < 0.5)
            result.append(names);
        else {
            const Vector<String>& toNames = std::get<Vector<String>>(to[i]);
            result.append(toNames);
        }
    }, [&](const GridTrackEntryRepeat& repeatFrom) {
        const auto& repeatTo = std::get<GridTrackEntryRepeat>(to[i]);
        GridTrackEntryRepeat repeatResult;
        repeatResult.repeats = repeatFrom.repeats;
        repeatResult.list = blendFunc(repeatFrom.list, repeatTo.list, context);
        result.append(WTFMove(repeatResult));
    }, [&](const GridTrackEntryAutoRepeat& repeatFrom) {
        const auto& repeatTo = std::get<GridTrackEntryAutoRepeat>(to[i]);
        GridTrackEntryAutoRepeat repeatResult;
        repeatResult.type = repeatFrom.type;
        repeatResult.list = blendFunc(repeatFrom.list, repeatTo.list, context);
        result.append(WTFMove(repeatResult));
    }, [&](const GridTrackEntrySubgrid&) {
    }, [&](const GridTrackEntryMasonry&) {
    });


    for (i = 0; i < from.size(); i++)
        std::visit(visitor, from[i]);

    return result;
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
    virtual bool isAdditiveOrCumulative() const { return true; }
    virtual bool requiresBlendingForAccumulativeIteration(const RenderStyle&, const RenderStyle&) const { return false; }
    virtual bool equals(const RenderStyle&, const RenderStyle&) const = 0;
    virtual bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const { return true; }
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
        LOG_WITH_STREAM(Animations, stream << "  blending " << property() << " from " << value(from) << " to " << value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
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

class OffsetRotatePropertyWrapper final : public PropertyWrapperGetter<OffsetRotation> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    OffsetRotatePropertyWrapper(CSSPropertyID property, OffsetRotation (RenderStyle::*getter)() const, void (RenderStyle::*setter)(OffsetRotation&&))
        : PropertyWrapperGetter(property, getter)
        , m_setter(setter)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return value(from).hasAuto() == value(to).hasAuto();
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        (destination.*m_setter)(blendFunc(value(from), value(to), context));
    }

private:
    void (RenderStyle::*m_setter)(OffsetRotation&&);
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
class DiscretePropertyWrapper : public PropertyWrapperGetter<T> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DiscretePropertyWrapper(CSSPropertyID property, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T))
        : PropertyWrapperGetter<T>(property, getter)
        , m_setter(setter)
    {
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const override
    {
        ASSERT(!context.progress || context.progress == 1.0);
        (destination.*this->m_setter)(this->value(context.progress ? to : from));
    }

private:
    bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const final { return false; }

    void (RenderStyle::*m_setter)(T);
};

class GridTemplatePropertyWrapper final : public PropertyWrapper<const GridTrackList&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    GridTemplatePropertyWrapper(CSSPropertyID property, const GridTrackList& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const GridTrackList&))
        : PropertyWrapper(property, getter, setter)
    {
    }

private:
    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        (destination.*m_setter)(blendFunc(this->value(from), this->value(to), context));
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return WebCore::canInterpolate(this->value(from), this->value(to));
    }
};

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
    bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const final { return false; }

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

    if (from.isCalculated())
        return to.isFixed() || to.isPercentOrCalculated();
    if (to.isCalculated())
        return from.isFixed() || from.isPercentOrCalculated();

    return false;
}

static bool lengthsRequireBlendingForAccumulativeIteration(const Length& from, const Length& to)
{
    // If blending the values can yield a calc() value, we must go through the blending code for iterationComposite.
    return from.isCalculated() || to.isCalculated() || from.type() != to.type();
}

class LengthPropertyWrapper : public PropertyWrapperGetter<const Length&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Flags {
        IsLengthPercentage          = 1 << 0,
        NegativeLengthsAreInvalid   = 1 << 1,
    };
    LengthPropertyWrapper(CSSPropertyID property, const Length& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(Length&&), OptionSet<Flags> flags = { })
        : PropertyWrapperGetter(property, getter)
        , m_setter(setter)
        , m_flags(flags)
    {
    }

protected:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const override
    {
        return canInterpolateLengths(value(from), value(to), m_flags.contains(Flags::IsLengthPercentage));
    }

    bool requiresBlendingForAccumulativeIteration(const RenderStyle& from, const RenderStyle& to) const final
    {
        return lengthsRequireBlendingForAccumulativeIteration(value(from), value(to));
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

class LengthPointPropertyWrapper : public PropertyWrapperGetter<LengthPoint> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LengthPointPropertyWrapper(CSSPropertyID property, LengthPoint (RenderStyle::*getter)() const, void (RenderStyle::*setter)(LengthPoint&&))
        : PropertyWrapperGetter(property, getter)
        , m_setter(setter)
    {
    }

private:
    bool requiresBlendingForAccumulativeIteration(const RenderStyle& from, const RenderStyle& to) const final
    {
        auto fromLengthPoint = value(from);
        auto toLengthPoint = value(to);
        return lengthsRequireBlendingForAccumulativeIteration(fromLengthPoint.x(), toLengthPoint.x())
            || lengthsRequireBlendingForAccumulativeIteration(fromLengthPoint.y(), toLengthPoint.y());
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        (destination.*m_setter)(blendFunc(value(from), value(to), context));
    }

    void (RenderStyle::*m_setter)(LengthPoint&&);
};

// This class extends LengthPointPropertyWrapper to accommodate `auto` value expressed as
// LengthPoint(Length(LengthType::Auto), Length(LengthType::Auto)). This is used for 
// offset-anchor and offset-position, which allows `auto`, and is expressed like so.
class LengthPointOrAutoPropertyWrapper final : public LengthPointPropertyWrapper {
public:
    LengthPointOrAutoPropertyWrapper(CSSPropertyID property, LengthPoint (RenderStyle::*getter)() const, void (RenderStyle::*setter)(LengthPoint&&))
        : LengthPointPropertyWrapper(property, getter, setter)
    {
    }

private:
    // Check if it's possible to interpolate between the from and to values. In particular,
    // it's only possible if they're both not auto.
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        bool fromIsAuto = value(from).x().isAuto() && value(from).y().isAuto();
        bool toIsAuto = value(to).x().isAuto() && value(to).y().isAuto();

        return (!fromIsAuto && !toIsAuto); 
    }
};

static bool lengthVariantRequiresBlendingForAccumulativeIteration(const LengthSize& from, const LengthSize& to)
{
    return lengthsRequireBlendingForAccumulativeIteration(from.width, to.width)
        || lengthsRequireBlendingForAccumulativeIteration(from.height, to.height);
}

static bool lengthVariantRequiresBlendingForAccumulativeIteration(const GapLength& from, const GapLength& to)
{
    return from.isNormal() || to.isNormal() || lengthsRequireBlendingForAccumulativeIteration(from.length(), to.length());
}

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
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return canInterpolateLengthVariants(this->value(from), this->value(to));
    }

    bool requiresBlendingForAccumulativeIteration(const RenderStyle& from, const RenderStyle& to) const final
    {
        return lengthVariantRequiresBlendingForAccumulativeIteration(this->value(from), this->value(to));
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        (destination.*m_setter)(blendFunc(this->value(from), this->value(to), context));
    }

    void (RenderStyle::*m_setter)(T&&);
};

class OptionalLengthPropertyWrapper : public PropertyWrapperGetter<std::optional<Length>> {
    WTF_MAKE_FAST_ALLOCATED;

public:
    enum class Flags {
        IsLengthPercentage = 1 << 0,
        NegativeLengthsAreInvalid = 1 << 1,
    };
    OptionalLengthPropertyWrapper(CSSPropertyID property, std::optional<Length> (RenderStyle::*getter)() const, void (RenderStyle::*setter)(std::optional<Length>), OptionSet<Flags> flags = { })
        : PropertyWrapperGetter<std::optional<Length>>(property, getter)
        , m_setter(setter)
        , m_flags(flags)
    {
    }

protected:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const override
    {
        if (!this->value(from) || !this->value(to))
            return false;

        bool isLengthPercentage = m_flags.contains(Flags::IsLengthPercentage);
        return canInterpolateLengths(*this->value(from), *this->value(to), isLengthPercentage);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const override
    {
        if (context.isDiscrete) {
            ASSERT(!context.progress || context.progress == 1);
            (destination.*m_setter)(context.progress ? this->value(to) : this->value(from));
            return;
        }

        auto valueRange = m_flags.contains(Flags::NegativeLengthsAreInvalid) ? ValueRange::NonNegative : ValueRange::All;
        (destination.*m_setter)(blendFunc(*this->value(from), *this->value(to), context, valueRange));
    }

private:
    void (RenderStyle::*m_setter)(std::optional<Length>);
    OptionSet<Flags> m_flags;
};


class ContainIntrinsiclLengthPropertyWrapper final : public OptionalLengthPropertyWrapper {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ContainIntrinsiclLengthPropertyWrapper(CSSPropertyID property, std::optional<Length> (RenderStyle::*getter)() const, void (RenderStyle::*setter)(std::optional<Length>), ContainIntrinsicSizeType (RenderStyle::*typeGetter)() const, void (RenderStyle::*typeSetter)(ContainIntrinsicSizeType))
        : OptionalLengthPropertyWrapper(property, getter, setter, { Flags::NegativeLengthsAreInvalid })
        , m_containIntrinsicSizeTypeGetter(typeGetter)
        , m_containIntrinsicSizeTypeSetter(typeSetter)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation operation) const final
    {
        if ((from.*m_containIntrinsicSizeTypeGetter)() != (to.*m_containIntrinsicSizeTypeGetter)())
            return false;
        return OptionalLengthPropertyWrapper::canInterpolate(from, to, operation);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {

        auto type = context.progress < 0.5 ? (from.*m_containIntrinsicSizeTypeGetter)() : (to.*m_containIntrinsicSizeTypeGetter)();
        (destination.*m_containIntrinsicSizeTypeSetter)(type);

        OptionalLengthPropertyWrapper::blend(destination, from, to, context);
    }

    ContainIntrinsicSizeType (RenderStyle::*m_containIntrinsicSizeTypeGetter)() const;
    void (RenderStyle::*m_containIntrinsicSizeTypeSetter)(ContainIntrinsicSizeType);
};

class LengthBoxPropertyWrapper : public PropertyWrapperGetter<const LengthBox&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Flags {
        IsLengthPercentage      = 1 << 0,
        UsesFillKeyword         = 1 << 1,
        AllowsNegativeValues    = 1 << 2,
        MayOverrideBorderWidths = 1 << 3,
    };
    LengthBoxPropertyWrapper(CSSPropertyID property, const LengthBox& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(LengthBox&&), OptionSet<Flags> flags = { })
        : PropertyWrapperGetter(property, getter)
        , m_setter(setter)
        , m_flags(flags)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const override
    {
        if (m_flags.contains(Flags::UsesFillKeyword) && from.borderImage().fill() != to.borderImage().fill())
            return false;

        bool isLengthPercentage = m_flags.contains(Flags::IsLengthPercentage);

        if (m_flags.contains(Flags::MayOverrideBorderWidths)) {
            bool overridesBorderWidths = from.borderImage().overridesBorderWidths();
            if (overridesBorderWidths != to.borderImage().overridesBorderWidths())
                return false;
            // Even if this property accepts <length-percentage>, border widths can only be a <length>.
            if (overridesBorderWidths)
                isLengthPercentage = false;
        }

        auto& fromLengthBox = value(from);
        auto& toLengthBox = value(to);
        return canInterpolateLengths(fromLengthBox.top(), toLengthBox.top(), isLengthPercentage)
            && canInterpolateLengths(fromLengthBox.right(), toLengthBox.right(), isLengthPercentage)
            && canInterpolateLengths(fromLengthBox.bottom(), toLengthBox.bottom(), isLengthPercentage)
            && canInterpolateLengths(fromLengthBox.left(), toLengthBox.left(), isLengthPercentage);
    }

    bool requiresBlendingForAccumulativeIteration(const RenderStyle& from, const RenderStyle& to) const final
    {
        auto& fromLengthBox = value(from);
        auto& toLengthBox = value(to);
        return lengthsRequireBlendingForAccumulativeIteration(fromLengthBox.top(), toLengthBox.top())
            && lengthsRequireBlendingForAccumulativeIteration(fromLengthBox.right(), toLengthBox.right())
            && lengthsRequireBlendingForAccumulativeIteration(fromLengthBox.bottom(), toLengthBox.bottom())
            && lengthsRequireBlendingForAccumulativeIteration(fromLengthBox.left(), toLengthBox.left());
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const override
    {
        if (m_flags.contains(Flags::UsesFillKeyword))
            destination.setBorderImageSliceFill((!context.progress || !context.isDiscrete ? from : to).borderImage().fill());
        if (m_flags.contains(Flags::MayOverrideBorderWidths))
            destination.setBorderImageWidthOverridesBorderWidths((!context.progress || !context.isDiscrete ? from : to).borderImage().overridesBorderWidths());
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
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation compositeOperation) const final
    {
        return from.hasClip() && to.hasClip() && LengthBoxPropertyWrapper::canInterpolate(from, to, compositeOperation);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        LengthBoxPropertyWrapper::blend(destination, from, to, context);
        destination.setHasClip(true);
    }
};

class PathOperationPropertyWrapper final : public RefCountedPropertyWrapper<PathOperation> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PathOperationPropertyWrapper(CSSPropertyID property, PathOperation* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(RefPtr<PathOperation>&&))
        : RefCountedPropertyWrapper(property, getter, setter)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const override
    {
        auto fromPath = value(from);
        auto toPath = value(to);

        if (is<ShapePathOperation>(fromPath) && is<ShapePathOperation>(toPath)) {
            auto& fromShape = downcast<ShapePathOperation>(*fromPath).basicShape();
            auto& toShape = downcast<ShapePathOperation>(*toPath).basicShape();

            return fromShape.canBlend(toShape);
        }

        if (is<RayPathOperation>(fromPath) && is<RayPathOperation>(toPath)) {
            auto& fromRay = downcast<RayPathOperation>(*fromPath);
            auto& toRay = downcast<RayPathOperation>(*toPath);

            return fromRay.canBlend(toRay);
        }

        return false;
    }

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
        : PropertyWrapper(property, getter, setter)
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

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
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
        : RefCountedPropertyWrapper(property, getter, setter)
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

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
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
        : RefCountedPropertyWrapper(property, getter, setter)
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

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
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
    bool requiresBlendingForAccumulativeIteration(const RenderStyle&, const RenderStyle&) const final { return this->property() == CSSPropertyTransform; }
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
        : PropertyWrapper(propertyID, getter, setter)
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

    bool requiresBlendingForAccumulativeIteration(const RenderStyle&, const RenderStyle&) const final { return true; }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation compositeOperation) const final
    {
        auto& fromFilterOperations = value(from);
        auto& toFilterOperations = value(to);

        // https://drafts.fxtf.org/filter-effects/#interpolation-of-filters

        auto listContainsReference = [](auto& filterOperations) {
            return filterOperations.operations().findIf([](auto& filterOperation) {
                return filterOperation->type() == FilterOperation::OperationType::REFERENCE;
            }) != notFound;
        };

        if (listContainsReference(fromFilterOperations) || listContainsReference(toFilterOperations))
            return false;

        // If one filter is none and the other is a <filter-value-list> without <url>
        auto oneListIsEmpty = [&]() {
            return fromFilterOperations.isEmpty() != toFilterOperations.isEmpty();
        };

        // If both filters have a <filter-value-list> of same length without <url> and for each <filter-function>
        // for which there is a corresponding item in each list
        // If both filters have a <filter-value-list> of different length without <url> and for each
        // <filter-function> for which there is a corresponding item in each list
        auto listsMatch = [&]() {
            auto numItems = [&]() {
                if (fromFilterOperations.size() == toFilterOperations.size())
                    return fromFilterOperations.size();
                return std::min(fromFilterOperations.size(), toFilterOperations.size());
            }();

            for (size_t i = 0; i < numItems; ++i) {
                auto* fromOperation = fromFilterOperations.at(i);
                auto* toOperation = toFilterOperations.at(i);
                if (!!fromOperation != !!toOperation)
                    return false;
                if (fromOperation && toOperation && fromOperation->type() != toOperation->type())
                    return false;
            }

            return true;
        };

        return compositeOperation != CompositeOperation::Replace || oneListIsEmpty() || listsMatch();
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        (destination.*m_setter)(blendFunc(value(from), value(to), context));
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
    static NeverDestroyed<ShadowData> defaultShadowData(LengthPoint(Length(LengthType::Fixed), Length(LengthType::Fixed)), Length(LengthType::Fixed), Length(LengthType::Fixed), ShadowStyle::Normal, false, Color::transparentBlack);
    static NeverDestroyed<ShadowData> defaultInsetShadowData(LengthPoint(Length(LengthType::Fixed), Length(LengthType::Fixed)), Length(LengthType::Fixed), Length(LengthType::Fixed), ShadowStyle::Inset, false, Color::transparentBlack);
    static NeverDestroyed<ShadowData> defaultWebKitBoxShadowData(LengthPoint(Length(LengthType::Fixed), Length(LengthType::Fixed)), Length(LengthType::Fixed), Length(LengthType::Fixed), ShadowStyle::Normal, true, Color::transparentBlack);
    static NeverDestroyed<ShadowData> defaultInsetWebKitBoxShadowData(LengthPoint(Length(LengthType::Fixed), Length(LengthType::Fixed)), Length(LengthType::Fixed), Length(LengthType::Fixed), ShadowStyle::Inset, true, Color::transparentBlack);

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
    bool requiresBlendingForAccumulativeIteration(const RenderStyle&, const RenderStyle&) const final { return true; }

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

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation compositeOperation) const final
    {
        if (compositeOperation != CompositeOperation::Replace)
            return true;

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

    std::unique_ptr<ShadowData> addShadowLists(const ShadowData* shadowA, const ShadowData* shadowB) const
    {
        std::unique_ptr<ShadowData> newShadowData;
        ShadowData* lastShadow = nullptr;
        auto addShadows = [&](const ShadowData* shadow) {
            while (shadow) {
                auto blendedShadow = makeUnique<ShadowData>(*shadow);
                auto* blendedShadowPtr = blendedShadow.get();
                if (!lastShadow)
                    newShadowData = WTFMove(blendedShadow);
                else
                    lastShadow->setNext(WTFMove(blendedShadow));

                lastShadow = blendedShadowPtr;
                shadow = shadow ? shadow->next() : nullptr;
            }
        };
        addShadows(shadowB);
        addShadows(shadowA);
        return newShadowData;
    }

    std::unique_ptr<ShadowData> blendSimpleOrMatchedShadowLists(const ShadowData* shadowA, const ShadowData* shadowB, const CSSPropertyBlendingContext& context) const
    {
        // from or to might be null in which case we don't want to do additivity, but do replace instead.
        if (shadowA && shadowB && context.compositeOperation == CompositeOperation::Add)
            return addShadowLists(shadowA, shadowB);

        std::unique_ptr<ShadowData> newShadowData;
        ShadowData* lastShadow = nullptr;

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
        if (shadowA && shadowB && context.compositeOperation != CompositeOperation::Replace)
            return addShadowLists(shadowA, shadowB);

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

template <typename T = StyleColor> class PropertyWrapperColor : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperColor(CSSPropertyID property, const T& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const T&))
        : AnimationPropertyWrapperBase(property)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

protected:
    bool equals(const RenderStyle& a, const RenderStyle& b) const override
    {
        if (&a == &b)
            return true;
        
        auto fromStyleColor = unresolvedValue(a);
        auto toStyleColor = unresolvedValue(b);
        
        if (RenderStyle::isCurrentColor(fromStyleColor) && RenderStyle::isCurrentColor(toStyleColor))
            return true;
        
        return value(a) == value(b);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const override
    {
        auto fromStyleColor = unresolvedValue(from);
        auto toStyleColor = unresolvedValue(to);

        // We don't animate on currentcolor-only transition.
        // https://github.com/WebKit/WebKit/blob/main/LayoutTests/imported/w3c/web-platform-tests/css/css-transitions/currentcolor-animation-001.html#L27
        if (RenderStyle::isCurrentColor(fromStyleColor) && RenderStyle::isCurrentColor(toStyleColor))
            return;

        auto fromColor = value(from);
        auto toColor = value(to);
        auto result = blendFunc(fromColor, toColor, context);
        (destination.*m_setter)(result);
    }

private:
    Color value(const RenderStyle& style) const
    {
        auto color = (style.*m_getter)();
        return style.colorResolvingCurrentColor(color);
    }

    StyleColor unresolvedValue(const RenderStyle& style) const
    {
        return (style.*m_getter)();
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        // FIXME: better logging.
        LOG_WITH_STREAM(Animations, stream << "  blending " << property() << " from " << value(from) << " to " << value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
    }
#endif

    const T& (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(const T&);
};


template <typename T = StyleColor> class PropertyWrapperVisitedAffectedColor : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperVisitedAffectedColor(CSSPropertyID property, const T& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const T&), const T& (RenderStyle::*visitedGetter)() const, void (RenderStyle::*visitedSetter)(const T&))
        : AnimationPropertyWrapperBase(property)
        , m_wrapper(makeUnique<PropertyWrapperColor<T>>(property, getter, setter))
        , m_visitedWrapper(makeUnique<PropertyWrapperColor<T>>(property, visitedGetter, visitedSetter))
    {
    }

protected:
    bool requiresBlendingForAccumulativeIteration(const RenderStyle&, const RenderStyle&) const final { return true; }

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

class AccentColorPropertyWrapper final : public PropertyWrapperColor<> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    AccentColorPropertyWrapper()
        : PropertyWrapperColor(CSSPropertyAccentColor, &RenderStyle::accentColor, &RenderStyle::setAccentColor)
    {
    }

private:
    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return a.hasAutoAccentColor() == b.hasAutoAccentColor()
            && PropertyWrapperColor::equals(a, b);
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return !from.hasAutoAccentColor() && !to.hasAutoAccentColor();
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        if (canInterpolate(from, to, context.compositeOperation)) {
            PropertyWrapperColor::blend(destination, from, to, context);
            return;
        }

        ASSERT(!context.progress || context.progress == 1.0);
        auto& blendingRenderStyle = context.progress ? to : from;
        if (blendingRenderStyle.hasAutoAccentColor())
            destination.setHasAutoAccentColor();
        else
            destination.setAccentColor(blendingRenderStyle.accentColor());
    }
};

static bool canInterpolateCaretColor(const RenderStyle& from, const RenderStyle& to, bool visited)
{
    if (visited)
        return !from.hasVisitedLinkAutoCaretColor() && !to.hasVisitedLinkAutoCaretColor();
    return !from.hasAutoCaretColor() && !to.hasAutoCaretColor();
}

class CaretColorPropertyWrapper final : public PropertyWrapperVisitedAffectedColor<> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CaretColorPropertyWrapper()
        : PropertyWrapperVisitedAffectedColor(CSSPropertyCaretColor, &RenderStyle::caretColor, &RenderStyle::setCaretColor, &RenderStyle::visitedLinkCaretColor, &RenderStyle::setVisitedLinkCaretColor)
    {
    }

private:
    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return a.hasAutoCaretColor() == b.hasAutoCaretColor()
            && a.hasVisitedLinkAutoCaretColor() == b.hasVisitedLinkAutoCaretColor()
            && PropertyWrapperVisitedAffectedColor::equals(a, b);
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
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
        LOG_WITH_STREAM(Animations, stream << "  blending " << property() << " from " << value(from) << " to " << value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
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
        LOG_WITH_STREAM(Animations, stream << "  blending " << FillLayerPropertyWrapperGetter<const T&>::property()
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
        : FillLayerPropertyWrapperGetter(property, lengthGetter)
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
        LOG_WITH_STREAM(Animations, stream << "  blending " << property() << " from " << value(from) << " to " << value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
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
        LOG_WITH_STREAM(Animations, stream << "  blending " << FillLayerPropertyWrapperGetter<T*>::property()
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
        : FillLayerRefCountedPropertyWrapper(property, getter, setter)
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
        LOG_WITH_STREAM(Animations, stream << "  blending " << property() << " from " << this->value(from) << " to " << this->value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
    }
#endif
};

template <typename T>
class DiscreteFillLayerPropertyWrapper final : public FillLayerAnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DiscreteFillLayerPropertyWrapper(CSSPropertyID property, T (FillLayer::*getter)() const, void (FillLayer::*setter)(T))
        : FillLayerAnimationPropertyWrapperBase(property)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

private:
    bool equals(const FillLayer* a, const FillLayer* b) const final
    {
        return (a->*m_getter)() == (b->*m_getter)();
    }

    bool canInterpolate(const FillLayer*, const FillLayer*) const final { return false; }

#if !LOG_DISABLED
    void logBlend(const FillLayer* destination, const FillLayer* from, const FillLayer* to, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << property() << " from " << (from->*m_getter)() << " to " << (to->*m_getter)() << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << (destination->*m_getter)());
    }
#endif

    void blend(FillLayer* destination, const FillLayer* from, const FillLayer* to, const CSSPropertyBlendingContext& context) const final
    {
        ASSERT(!context.progress || context.progress == 1.0);
        (destination->*m_setter)(((context.progress ? to : from)->*m_getter)());
    }

    T (FillLayer::*m_getter)() const;
    void (FillLayer::*m_setter)(T);
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
        case CSSPropertyMaskSize:
            m_fillLayerPropertyWrapper = makeUnique<FillLayerPropertyWrapper<LengthSize>>(property, &FillLayer::sizeLength, &FillLayer::setSizeLength);
            break;
        case CSSPropertyBackgroundImage:
        case CSSPropertyMaskImage:
            m_fillLayerPropertyWrapper = makeUnique<FillLayerStyleImagePropertyWrapper>(property, &FillLayer::image, &FillLayer::setImage);
            break;
        case CSSPropertyMaskClip:
            m_fillLayerPropertyWrapper = makeUnique<DiscreteFillLayerPropertyWrapper<FillBox>>(property, &FillLayer::clip, &FillLayer::setClip);
            break;
        case CSSPropertyMaskOrigin:
            m_fillLayerPropertyWrapper = makeUnique<DiscreteFillLayerPropertyWrapper<FillBox>>(property, &FillLayer::origin, &FillLayer::setOrigin);
            break;
        case CSSPropertyMaskComposite:
            m_fillLayerPropertyWrapper = makeUnique<DiscreteFillLayerPropertyWrapper<CompositeOperator>>(property, &FillLayer::composite, &FillLayer::setComposite);
            break;
        case CSSPropertyMaskMode:
            m_fillLayerPropertyWrapper = makeUnique<DiscreteFillLayerPropertyWrapper<MaskMode>>(property, &FillLayer::maskMode, &FillLayer::setMaskMode);
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

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
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

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
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
    PropertyWrapperSVGPaint(CSSPropertyID property, SVGPaintType (RenderStyle::*paintTypeGetter)() const, StyleColor (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const StyleColor&))
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
            auto fromStyleColor = (a.*m_getter)();
            auto toStyleColor = (b.*m_getter)();
            
            // We don't animate when both are currentcolor
            auto fromColor = a.colorResolvingCurrentColor(fromStyleColor);
            auto toColor = b.colorResolvingCurrentColor(toStyleColor);

            return (fromStyleColor.isCurrentColor() && toStyleColor.isCurrentColor()) || fromColor == toColor;
        }
        return true;
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        auto isValidPaintType = [](SVGPaintType paintType) {
            return paintType == SVGPaintType::RGBColor || paintType == SVGPaintType::CurrentColor;
        };

        if (!isValidPaintType((from.*m_paintTypeGetter)()) || !isValidPaintType((to.*m_paintTypeGetter)()))
            return;

        auto fromStyleColor = (from.*m_getter)();
        auto toStyleColor = (to.*m_getter)();

        // We don't animate when both are currentcolor
        if (RenderStyle::isCurrentColor(fromStyleColor) && RenderStyle::isCurrentColor(toStyleColor))
            return;

        auto fromColor = from.colorResolvingCurrentColor(fromStyleColor);
        auto toColor = to.colorResolvingCurrentColor(toStyleColor);

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
    StyleColor (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(const StyleColor&);
};

class PropertyWrapperFontWeight final : public PropertyWrapper<FontSelectionValue> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperFontWeight()
        : PropertyWrapper(CSSPropertyFontWeight, &RenderStyle::fontWeight, &RenderStyle::setFontWeight)
    {
    }

private:

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        (destination.*m_setter)(FontSelectionValue(std::clamp(blendFunc(static_cast<float>(this->value(from)), static_cast<float>(this->value(to)), context), 1.0f, 1000.0f)));
    }
};

class PropertyWrapperFontStyle final : public PropertyWrapper<std::optional<FontSelectionValue>> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperFontStyle()
        : PropertyWrapper(CSSPropertyFontStyle, &RenderStyle::fontItalic, &RenderStyle::setFontItalic)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
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

class PropertyWrapperFontSizeAdjust final : public PropertyWrapperGetter<std::optional<float>> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperFontSizeAdjust()
        : PropertyWrapperGetter(CSSPropertyFontSizeAdjust, &RenderStyle::fontSizeAdjust)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return from.fontSizeAdjust().has_value() && to.fontSizeAdjust().has_value();
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        auto blendedFontSizeAdjust = [&]() -> std::optional<float> {
            if (context.isDiscrete)
                return (!context.progress ? from : to).fontSizeAdjust();
            ASSERT(from.fontSizeAdjust().has_value() && to.fontSizeAdjust().has_value());
            auto blendedAdjust = blendFunc(from.fontSizeAdjust().value(), to.fontSizeAdjust().value(), context);
            return std::max(blendedAdjust, 0.0f);
        };

        destination.setFontSizeAdjust(blendedFontSizeAdjust());
    }
};

class PropertyWrapperBaselineShift final : public PropertyWrapper<SVGLengthValue> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperBaselineShift()
        : PropertyWrapper(CSSPropertyBaselineShift, &RenderStyle::baselineShiftValue, &RenderStyle::setBaselineShiftValue)
    {
    }

private:
    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return a.svgStyle().baselineShift() == b.svgStyle().baselineShift() && PropertyWrapper::equals(a, b);
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation compositeOperation) const final
    {
        return from.svgStyle().baselineShift() == to.svgStyle().baselineShift() && PropertyWrapper::canInterpolate(from, to, compositeOperation);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        auto& srcSVGStyle = !context.progress ? from.svgStyle() : to.svgStyle();
        destination.accessSVGStyle().setBaselineShift(srcSVGStyle.baselineShift());
        PropertyWrapper::blend(destination, from, to, context);
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
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
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


class FloatPropertyWrapper : public PropertyWrapper<float> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class ValueRange : uint8_t {
        All,
        NonNegative,
        Positive
    };
    FloatPropertyWrapper(CSSPropertyID property, float (RenderStyle::*getter)() const, void (RenderStyle::*setter)(float), ValueRange valueRange = ValueRange::All)
        : PropertyWrapper(property, getter, setter)
        , m_valueRange(valueRange)
    {
    }

protected:
    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const override
    {
        auto blendedValue = blendFunc(value(from), value(to), context);
        if (m_valueRange == ValueRange::NonNegative && blendedValue <= 0)
            blendedValue = 0;
        else if (m_valueRange == ValueRange::Positive && blendedValue < 0)
            blendedValue = std::numeric_limits<float>::epsilon();
        (destination.*m_setter)(blendedValue);
    }

private:
    ValueRange m_valueRange;
};

class VerticalAlignWrapper final : public LengthPropertyWrapper {
    WTF_MAKE_FAST_ALLOCATED;
public:
    VerticalAlignWrapper()
        : LengthPropertyWrapper(CSSPropertyVerticalAlign, &RenderStyle::verticalAlignLength, &RenderStyle::setVerticalAlignLength, LengthPropertyWrapper::Flags::IsLengthPercentage)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation compositeOperation) const final
    {
        return from.verticalAlign() == VerticalAlign::Length && to.verticalAlign() == VerticalAlign::Length && LengthPropertyWrapper::canInterpolate(from, to, compositeOperation);
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
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation compositeOperation) const final
    {
        if (from.textIndentLine() != to.textIndentLine())
            return false;
        if (from.textIndentType() != to.textIndentType())
            return false;
        return LengthPropertyWrapper::canInterpolate(from, to, compositeOperation);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        auto& blendingStyle = context.isDiscrete && context.progress ? to : from;
        destination.setTextIndentLine(blendingStyle.textIndentLine());
        destination.setTextIndentType(blendingStyle.textIndentType());
        LengthPropertyWrapper::blend(destination, from, to, context);
    }
};

class PerspectiveWrapper final : public FloatPropertyWrapper {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PerspectiveWrapper()
        : FloatPropertyWrapper(CSSPropertyPerspective, &RenderStyle::perspective, &RenderStyle::setPerspective, FloatPropertyWrapper::ValueRange::NonNegative)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation compositeOperation) const final
    {
        if (!from.hasPerspective() || !to.hasPerspective())
            return false;
        return FloatPropertyWrapper::canInterpolate(from, to, compositeOperation);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        if (context.isDiscrete)
            (destination.*m_setter)(context.progress ? value(to) : value(from));
        else
            FloatPropertyWrapper::blend(destination, from, to, context);
    }
};

class TabSizePropertyWrapper final : public PropertyWrapper<const TabSize&> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    TabSizePropertyWrapper()
        : PropertyWrapper(CSSPropertyTabSize, &RenderStyle::tabSize, &RenderStyle::setTabSize)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
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

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return (from.aspectRatioType() == AspectRatioType::Ratio && to.aspectRatioType() == AspectRatioType::Ratio) || (from.aspectRatioType() == AspectRatioType::AutoAndRatio && to.aspectRatioType() == AspectRatioType::AutoAndRatio);
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << "  blending " << property() << " from " << from.logicalAspectRatio() << " to " << to.logicalAspectRatio() << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << destination.logicalAspectRatio());
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

class StrokeDasharrayPropertyWrapper final : public PropertyWrapper<Vector<SVGLengthValue>> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    StrokeDasharrayPropertyWrapper()
        : PropertyWrapper(CSSPropertyStrokeDasharray, &RenderStyle::strokeDashArray, &RenderStyle::setStrokeDashArray)
    {
    }

private:
    bool isAdditiveOrCumulative() const final
    {
        return false;
    }
};

class PropertyWrapperContent final : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyWrapperContent()
        : AnimationPropertyWrapperBase(CSSPropertyContent)
    {
    }

    bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const final { return false; }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        if (!a.hasContent() && !b.hasContent())
            return true;
        if (a.hasContent() && b.hasContent())
            return *a.contentData() == *b.contentData();
        return false;
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle&, const RenderStyle&, const RenderStyle&, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << " blending content at " << TextStream::FormatNumberRespectingIntegers(progress) << ".");
    }
#endif

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        ASSERT(context.isDiscrete);
        ASSERT(!context.progress || context.progress == 1);

        auto& style = context.progress ? to : from;
        if (auto* content = style.contentData())
            destination.setContent(content->clone(), false);
        else
            destination.clearContent();
    }
};

class TextEmphasisStyleWrapper final : public DiscretePropertyWrapper<TextEmphasisMark> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    TextEmphasisStyleWrapper()
        : DiscretePropertyWrapper(CSSPropertyTextEmphasisStyle, &RenderStyle::textEmphasisMark, &RenderStyle::setTextEmphasisMark)
    {
    }

private:
    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        destination.setTextEmphasisFill((context.progress > 0.5 ? to : from).textEmphasisFill());
        DiscretePropertyWrapper::blend(destination, from, to, context);
    }
};

class DiscreteFontDescriptionWrapper : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DiscreteFontDescriptionWrapper(CSSPropertyID property)
        : AnimationPropertyWrapperBase(property)
    {
    }

protected:
    virtual bool propertiesInFontDescriptionAreEqual(const FontCascadeDescription&, const FontCascadeDescription&) const { return false; }
    virtual void setPropertiesInFontDescription(const FontCascadeDescription&, FontCascadeDescription&) const { }

private:
    bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const override { return false; }

    bool equals(const RenderStyle& a, const RenderStyle& b) const override
    {
        return propertiesInFontDescriptionAreEqual(a.fontDescription(), b.fontDescription());
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const override
    {
        ASSERT(!context.progress || context.progress == 1.0);
        FontSelector* currentFontSelector = destination.fontCascade().fontSelector();
        auto destinationDescription = destination.fontDescription();
        auto& sourceDescription = (context.progress ? to : from).fontDescription();
        setPropertiesInFontDescription(sourceDescription, destinationDescription);
        destination.setFontDescription(WTFMove(destinationDescription));
        destination.fontCascade().update(currentFontSelector);
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle&, const RenderStyle&, const RenderStyle&, double) const override
    {
    }
#endif
};

template <typename T>
class DiscreteFontDescriptionTypedWrapper : public DiscreteFontDescriptionWrapper {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DiscreteFontDescriptionTypedWrapper(CSSPropertyID property, T (FontCascadeDescription::*getter)() const, void (FontCascadeDescription::*setter)(T))
        : DiscreteFontDescriptionWrapper(property)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

private:
    bool propertiesInFontDescriptionAreEqual(const FontCascadeDescription& a, const FontCascadeDescription& b) const override
    {
        return this->value(a) == this->value(b);
    }

    void setPropertiesInFontDescription(const FontCascadeDescription& source, FontCascadeDescription& destination) const override
    {
        (destination.*this->m_setter)(this->value(source));
    }

    T value(const FontCascadeDescription& description) const
    {
        return (description.*this->m_getter)();
    }

    T (FontCascadeDescription::*m_getter)() const;
    void (FontCascadeDescription::*m_setter)(T);
};

class FontFamilyWrapper final : public DiscreteFontDescriptionWrapper {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FontFamilyWrapper()
        : DiscreteFontDescriptionWrapper(CSSPropertyFontFamily)
    {
    }

private:
    bool propertiesInFontDescriptionAreEqual(const FontCascadeDescription& a, const FontCascadeDescription& b) const override
    {
        return a.families() == b.families();
    }

    void setPropertiesInFontDescription(const FontCascadeDescription& source, FontCascadeDescription& destination) const override
    {
        destination.setFamilies(source.families());
    }
};

class CounterWrapper final : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CounterWrapper(CSSPropertyID property)
        : AnimationPropertyWrapperBase(property)
    {
        ASSERT(property == CSSPropertyCounterIncrement || property == CSSPropertyCounterReset);
    }

    bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const override { return false; }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        auto* aCounterDirectives = a.counterDirectives();
        auto* bCounterDirectives = b.counterDirectives();

        if (!aCounterDirectives && !bCounterDirectives)
            return true;
        if (aCounterDirectives && bCounterDirectives) {
            if (aCounterDirectives->size() != bCounterDirectives->size())
                return false;
            for (auto& [key, aDirective] : *aCounterDirectives) {
                auto it = bCounterDirectives->find(key);
                if (it == bCounterDirectives->end())
                    return false;
                auto& bDirective = it->value;
                if ((property() == CSSPropertyCounterIncrement && aDirective.incrementValue != bDirective.incrementValue)
                    || (property() == CSSPropertyCounterReset && aDirective.resetValue != bDirective.resetValue))
                    return false;
            }
            return true;
        }
        return false;
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle&, const RenderStyle&, const RenderStyle&, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << " blending " << property() << " at " << TextStream::FormatNumberRespectingIntegers(progress) << ".");
    }
#endif

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        ASSERT(context.isDiscrete);
        ASSERT(!context.progress || context.progress == 1);

        // Clear all existing values in the existing set of directives.
        if (destination.counterDirectives()) {
            for (auto& [key, directive] : destination.accessCounterDirectives()) {
                if (property() == CSSPropertyCounterIncrement)
                    directive.incrementValue = std::nullopt;
                else
                    directive.resetValue = std::nullopt;
            }
        }

        auto& style = context.progress ? to : from;
        if (!style.counterDirectives())
            return;

        auto& targetDirectives = destination.accessCounterDirectives();
        for (auto& [key, directive] : *style.counterDirectives()) {
            auto updateDirective = [&](CounterDirectives& target, const CounterDirectives& source) {
                if (property() == CSSPropertyCounterIncrement)
                    target.incrementValue = source.incrementValue;
                else
                    target.resetValue = source.resetValue;
            };
            auto it = targetDirectives.find(key);
            if (it == targetDirectives.end())
                updateDirective(targetDirectives.add(key, CounterDirectives { }).iterator->value, directive);
            else
                updateDirective(it->value, directive);
        }
    }
};

class FontFeatureSettingsWrapper final : public DiscreteFontDescriptionWrapper {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FontFeatureSettingsWrapper()
        : DiscreteFontDescriptionWrapper(CSSPropertyFontFeatureSettings)
    {
    }

private:
    bool propertiesInFontDescriptionAreEqual(const FontCascadeDescription& a, const FontCascadeDescription& b) const override
    {
        return a.featureSettings() == b.featureSettings();
    }

    void setPropertiesInFontDescription(const FontCascadeDescription& source, FontCascadeDescription& destination) const override
    {
        destination.setFeatureSettings(FontFeatureSettings(source.featureSettings()));
    }
};

class FontVariantEastAsianWrapper final : public DiscreteFontDescriptionWrapper {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FontVariantEastAsianWrapper()
        : DiscreteFontDescriptionWrapper(CSSPropertyFontVariantEastAsian)
    {
    }

private:
    bool propertiesInFontDescriptionAreEqual(const FontCascadeDescription& a, const FontCascadeDescription& b) const override
    {
        return a.variantEastAsianVariant() == b.variantEastAsianVariant()
            && a.variantEastAsianWidth() == b.variantEastAsianWidth()
            && a.variantEastAsianRuby() == b.variantEastAsianRuby();
    }

    void setPropertiesInFontDescription(const FontCascadeDescription& source, FontCascadeDescription& destination) const override
    {
        destination.setVariantEastAsianVariant(source.variantEastAsianVariant());
        destination.setVariantEastAsianWidth(source.variantEastAsianWidth());
        destination.setVariantEastAsianRuby(source.variantEastAsianRuby());
    }
};

class FontVariantLigaturesWrapper final : public DiscreteFontDescriptionWrapper {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FontVariantLigaturesWrapper()
        : DiscreteFontDescriptionWrapper(CSSPropertyFontVariantLigatures)
    {
    }

private:
    bool propertiesInFontDescriptionAreEqual(const FontCascadeDescription& a, const FontCascadeDescription& b) const override
    {
        return a.variantCommonLigatures() == b.variantCommonLigatures()
            && a.variantDiscretionaryLigatures() == b.variantDiscretionaryLigatures()
            && a.variantHistoricalLigatures() == b.variantHistoricalLigatures()
            && a.variantContextualAlternates() == b.variantContextualAlternates();
    }

    void setPropertiesInFontDescription(const FontCascadeDescription& source, FontCascadeDescription& destination) const override
    {
        destination.setVariantCommonLigatures(source.variantCommonLigatures());
        destination.setVariantDiscretionaryLigatures(source.variantDiscretionaryLigatures());
        destination.setVariantHistoricalLigatures(source.variantHistoricalLigatures());
        destination.setVariantContextualAlternates(source.variantContextualAlternates());
    }
};

class GridTemplateAreasWrapper final : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    GridTemplateAreasWrapper()
        : AnimationPropertyWrapperBase(CSSPropertyGridTemplateAreas)
    {
    }

    bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const override { return false; }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return a.implicitNamedGridColumnLines() == b.implicitNamedGridColumnLines()
            && a.implicitNamedGridRowLines() == b.implicitNamedGridRowLines()
            && a.namedGridArea() == b.namedGridArea()
            && a.namedGridAreaRowCount() == b.namedGridAreaRowCount()
            && a.namedGridAreaColumnCount() == b.namedGridAreaColumnCount();
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle&, const RenderStyle&, const RenderStyle&, double progress) const final
    {
        LOG_WITH_STREAM(Animations, stream << " blending " << property() << " at " << TextStream::FormatNumberRespectingIntegers(progress) << ".");
    }
#endif

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        ASSERT(context.isDiscrete);
        ASSERT(!context.progress || context.progress == 1);

        auto& source = context.progress ? to : from;
        destination.setImplicitNamedGridColumnLines(source.implicitNamedGridColumnLines());
        destination.setImplicitNamedGridRowLines(source.implicitNamedGridRowLines());
        destination.setNamedGridArea(source.namedGridArea());
        destination.setNamedGridAreaRowCount(source.namedGridAreaRowCount());
        destination.setNamedGridAreaColumnCount(source.namedGridAreaColumnCount());
    }
};

class FontVariantNumericWrapper final : public DiscreteFontDescriptionWrapper {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FontVariantNumericWrapper()
        : DiscreteFontDescriptionWrapper(CSSPropertyFontVariantNumeric)
    {
    }

private:
    bool propertiesInFontDescriptionAreEqual(const FontCascadeDescription& a, const FontCascadeDescription& b) const override
    {
        return a.variantNumericFigure() == b.variantNumericFigure()
            && a.variantNumericSpacing() == b.variantNumericSpacing()
            && a.variantNumericFraction() == b.variantNumericFraction()
            && a.variantNumericOrdinal() == b.variantNumericOrdinal()
            && a.variantNumericSlashedZero() == b.variantNumericSlashedZero();
    }

    void setPropertiesInFontDescription(const FontCascadeDescription& source, FontCascadeDescription& destination) const override
    {
        destination.setVariantNumericFigure(source.variantNumericFigure());
        destination.setVariantNumericSpacing(source.variantNumericSpacing());
        destination.setVariantNumericFraction(source.variantNumericFraction());
        destination.setVariantNumericOrdinal(source.variantNumericOrdinal());
        destination.setVariantNumericSlashedZero(source.variantNumericSlashedZero());
    }
};

class QuotesWrapper final : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    QuotesWrapper()
        : AnimationPropertyWrapperBase(CSSPropertyQuotes)
    {
    }

private:
    bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const override { return false; }

    bool equals(const RenderStyle& a, const RenderStyle& b) const override
    {
        return a.quotes() == b.quotes();
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const override
    {
        ASSERT(!context.progress || context.progress == 1.0);
        destination.setQuotes((context.progress ? to : from).quotes());
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle&, const RenderStyle&, const RenderStyle&, double) const override
    {
    }
#endif
};

template <typename T>
class DiscreteSVGPropertyWrapper final : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DiscreteSVGPropertyWrapper(CSSPropertyID property, T (SVGRenderStyle::*getter)() const, void (SVGRenderStyle::*setter)(T))
        : AnimationPropertyWrapperBase(property)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

private:
    bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const final { return false; }

    bool equals(const RenderStyle& a, const RenderStyle& b) const override
    {
        return this->value(a) == this->value(b);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const override
    {
        ASSERT(!context.progress || context.progress == 1.0);
        (destination.accessSVGStyle().*this->m_setter)(this->value(context.progress ? to : from));
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle&, const RenderStyle&, const RenderStyle&, double) const override
    {
    }
#endif

    T value(const RenderStyle& style) const
    {
        return (style.svgStyle().*this->m_getter)();
    }

    T (SVGRenderStyle::*m_getter)() const;
    void (SVGRenderStyle::*m_setter)(T);
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
        if (propertyID < firstCSSProperty || propertyID - firstCSSProperty >= numCSSProperties)
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

    unsigned short& indexFromPropertyID(CSSPropertyID propertyID)
    {
        return m_propertyToIdMap[propertyID - firstCSSProperty];
    }

    Vector<std::unique_ptr<AnimationPropertyWrapperBase>> m_propertyWrappers;
    unsigned short m_propertyToIdMap[numCSSProperties];

    static const unsigned short cInvalidPropertyWrapperIndex = std::numeric_limits<unsigned short>::max();

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

        new PropertyWrapperFlex,

        new FloatPropertyWrapper(CSSPropertyBorderLeftWidth, &RenderStyle::borderLeftWidth, &RenderStyle::setBorderLeftWidth, FloatPropertyWrapper::ValueRange::NonNegative),
        new FloatPropertyWrapper(CSSPropertyBorderRightWidth, &RenderStyle::borderRightWidth, &RenderStyle::setBorderRightWidth, FloatPropertyWrapper::ValueRange::NonNegative),
        new FloatPropertyWrapper(CSSPropertyBorderTopWidth, &RenderStyle::borderTopWidth, &RenderStyle::setBorderTopWidth, FloatPropertyWrapper::ValueRange::NonNegative),
        new FloatPropertyWrapper(CSSPropertyBorderBottomWidth, &RenderStyle::borderBottomWidth, &RenderStyle::setBorderBottomWidth, FloatPropertyWrapper::ValueRange::NonNegative),
        new LengthPropertyWrapper(CSSPropertyMarginLeft, &RenderStyle::marginLeft, &RenderStyle::setMarginLeft, { LengthPropertyWrapper::Flags::IsLengthPercentage }),
        new LengthPropertyWrapper(CSSPropertyMarginRight, &RenderStyle::marginRight, &RenderStyle::setMarginRight, { LengthPropertyWrapper::Flags::IsLengthPercentage }),
        new LengthPropertyWrapper(CSSPropertyMarginTop, &RenderStyle::marginTop, &RenderStyle::setMarginTop, { LengthPropertyWrapper::Flags::IsLengthPercentage }),
        new LengthPropertyWrapper(CSSPropertyMarginBottom, &RenderStyle::marginBottom, &RenderStyle::setMarginBottom, { LengthPropertyWrapper::Flags::IsLengthPercentage }),
        new DiscretePropertyWrapper<OptionSet<MarginTrimType>>(CSSPropertyMarginTrim, &RenderStyle::marginTrim, &RenderStyle::setMarginTrim),
        new LengthPropertyWrapper(CSSPropertyPaddingLeft, &RenderStyle::paddingLeft, &RenderStyle::setPaddingLeft, { LengthPropertyWrapper::Flags::IsLengthPercentage, LengthPropertyWrapper::Flags::NegativeLengthsAreInvalid }),
        new LengthPropertyWrapper(CSSPropertyPaddingRight, &RenderStyle::paddingRight, &RenderStyle::setPaddingRight, { LengthPropertyWrapper::Flags::IsLengthPercentage, LengthPropertyWrapper::Flags::NegativeLengthsAreInvalid }),
        new LengthPropertyWrapper(CSSPropertyPaddingTop, &RenderStyle::paddingTop, &RenderStyle::setPaddingTop, { LengthPropertyWrapper::Flags::IsLengthPercentage, LengthPropertyWrapper::Flags::NegativeLengthsAreInvalid }),
        new LengthPropertyWrapper(CSSPropertyPaddingBottom, &RenderStyle::paddingBottom, &RenderStyle::setPaddingBottom, { LengthPropertyWrapper::Flags::IsLengthPercentage, LengthPropertyWrapper::Flags::NegativeLengthsAreInvalid }),

        new AccentColorPropertyWrapper,

        new CaretColorPropertyWrapper,

        new PropertyWrapperVisitedAffectedColor(CSSPropertyColor, &RenderStyle::color, &RenderStyle::setColor, &RenderStyle::visitedLinkColor, &RenderStyle::setVisitedLinkColor),

        new PropertyWrapperVisitedAffectedColor(CSSPropertyBackgroundColor, &RenderStyle::backgroundColor, &RenderStyle::setBackgroundColor, &RenderStyle::visitedLinkBackgroundColor, &RenderStyle::setVisitedLinkBackgroundColor),

        new FillLayersPropertyWrapper(CSSPropertyBackgroundImage, &RenderStyle::backgroundLayers, &RenderStyle::ensureBackgroundLayers),
        new StyleImagePropertyWrapper(CSSPropertyListStyleImage, &RenderStyle::listStyleImage, &RenderStyle::setListStyleImage),
        new FillLayersPropertyWrapper(CSSPropertyMaskImage, &RenderStyle::maskLayers, &RenderStyle::ensureMaskLayers),

        new StyleImagePropertyWrapper(CSSPropertyBorderImageSource, &RenderStyle::borderImageSource, &RenderStyle::setBorderImageSource),
        new LengthBoxPropertyWrapper(CSSPropertyBorderImageSlice, &RenderStyle::borderImageSlices, &RenderStyle::setBorderImageSlices, { LengthBoxPropertyWrapper::Flags::UsesFillKeyword }),
        new LengthBoxPropertyWrapper(CSSPropertyBorderImageWidth, &RenderStyle::borderImageWidth, &RenderStyle::setBorderImageWidth, { LengthBoxPropertyWrapper::Flags::IsLengthPercentage, LengthBoxPropertyWrapper::Flags::MayOverrideBorderWidths }),
        new LengthBoxPropertyWrapper(CSSPropertyBorderImageOutset, &RenderStyle::borderImageOutset, &RenderStyle::setBorderImageOutset),

        new StyleImagePropertyWrapper(CSSPropertyWebkitMaskBoxImageSource, &RenderStyle::maskBoxImageSource, &RenderStyle::setMaskBoxImageSource),
        new PropertyWrapper<const NinePieceImage&>(CSSPropertyWebkitMaskBoxImage, &RenderStyle::maskBoxImage, &RenderStyle::setMaskBoxImage),

        new FillLayersPropertyWrapper(CSSPropertyBackgroundPositionX, &RenderStyle::backgroundLayers, &RenderStyle::ensureBackgroundLayers),
        new FillLayersPropertyWrapper(CSSPropertyBackgroundPositionY, &RenderStyle::backgroundLayers, &RenderStyle::ensureBackgroundLayers),
        new FillLayersPropertyWrapper(CSSPropertyBackgroundSize, &RenderStyle::backgroundLayers, &RenderStyle::ensureBackgroundLayers),
        new FillLayersPropertyWrapper(CSSPropertyWebkitBackgroundSize, &RenderStyle::backgroundLayers, &RenderStyle::ensureBackgroundLayers),

        new FillLayersPropertyWrapper(CSSPropertyMaskClip, &RenderStyle::maskLayers, &RenderStyle::ensureMaskLayers),
        new FillLayersPropertyWrapper(CSSPropertyMaskComposite, &RenderStyle::maskLayers, &RenderStyle::ensureMaskLayers),
        new FillLayersPropertyWrapper(CSSPropertyMaskMode, &RenderStyle::maskLayers, &RenderStyle::ensureMaskLayers),
        new FillLayersPropertyWrapper(CSSPropertyMaskOrigin, &RenderStyle::maskLayers, &RenderStyle::ensureMaskLayers),
        new FillLayersPropertyWrapper(CSSPropertyWebkitMaskPositionX, &RenderStyle::maskLayers, &RenderStyle::ensureMaskLayers),
        new FillLayersPropertyWrapper(CSSPropertyWebkitMaskPositionY, &RenderStyle::maskLayers, &RenderStyle::ensureMaskLayers),
        new FillLayersPropertyWrapper(CSSPropertyMaskSize, &RenderStyle::maskLayers, &RenderStyle::ensureMaskLayers),

        new DiscretePropertyWrapper<FillRepeatXY>(CSSPropertyMaskRepeat, &RenderStyle::maskRepeat, &RenderStyle::setMaskRepeat),

        new LengthPointPropertyWrapper(CSSPropertyObjectPosition, &RenderStyle::objectPosition, &RenderStyle::setObjectPosition),

        new PropertyWrapper<float>(CSSPropertyFontSize, &RenderStyle::computedFontSize, &RenderStyle::setFontSize),
        new PropertyWrapper<unsigned short>(CSSPropertyColumnRuleWidth, &RenderStyle::columnRuleWidth, &RenderStyle::setColumnRuleWidth),
        new LengthVariantPropertyWrapper<GapLength>(CSSPropertyColumnGap, &RenderStyle::columnGap, &RenderStyle::setColumnGap),
        new LengthVariantPropertyWrapper<GapLength>(CSSPropertyRowGap, &RenderStyle::rowGap, &RenderStyle::setRowGap),
        new AutoPropertyWrapper<unsigned short>(CSSPropertyColumnCount, &RenderStyle::columnCount, &RenderStyle::setColumnCount, &RenderStyle::hasAutoColumnCount, &RenderStyle::setHasAutoColumnCount, 1),
        new AutoPropertyWrapper<float>(CSSPropertyColumnWidth, &RenderStyle::columnWidth, &RenderStyle::setColumnWidth, &RenderStyle::hasAutoColumnWidth, &RenderStyle::setHasAutoColumnWidth, 0),
        new FloatPropertyWrapper(CSSPropertyWebkitBorderHorizontalSpacing, &RenderStyle::horizontalBorderSpacing, &RenderStyle::setHorizontalBorderSpacing, FloatPropertyWrapper::ValueRange::NonNegative),
        new FloatPropertyWrapper(CSSPropertyWebkitBorderVerticalSpacing, &RenderStyle::verticalBorderSpacing, &RenderStyle::setVerticalBorderSpacing, FloatPropertyWrapper::ValueRange::NonNegative),
        new AutoPropertyWrapper<int>(CSSPropertyZIndex, &RenderStyle::specifiedZIndex, &RenderStyle::setSpecifiedZIndex, &RenderStyle::hasAutoSpecifiedZIndex, &RenderStyle::setHasAutoSpecifiedZIndex),
        new PositivePropertyWrapper<unsigned short>(CSSPropertyOrphans, &RenderStyle::orphans, &RenderStyle::setOrphans),
        new PositivePropertyWrapper<unsigned short>(CSSPropertyWidows, &RenderStyle::widows, &RenderStyle::setWidows),
        new LengthPropertyWrapper(CSSPropertyLineHeight, &RenderStyle::specifiedLineHeight, &RenderStyle::setLineHeight),
        new PropertyWrapper<float>(CSSPropertyOutlineOffset, &RenderStyle::outlineOffset, &RenderStyle::setOutlineOffset),
        new FloatPropertyWrapper(CSSPropertyOutlineWidth, &RenderStyle::outlineWidth, &RenderStyle::setOutlineWidth, FloatPropertyWrapper::ValueRange::NonNegative),
        new PropertyWrapper<float>(CSSPropertyLetterSpacing, &RenderStyle::letterSpacing, &RenderStyle::setLetterSpacing),
        new LengthPropertyWrapper(CSSPropertyWordSpacing, &RenderStyle::wordSpacing, &RenderStyle::setWordSpacing, LengthPropertyWrapper::Flags::IsLengthPercentage),
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
        new FloatPropertyWrapper(CSSPropertyZoom, &RenderStyle::zoom, &RenderStyle::setZoomWithoutReturnValue, FloatPropertyWrapper::ValueRange::Positive),

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

        new PathOperationPropertyWrapper(CSSPropertyClipPath, &RenderStyle::clipPath, &RenderStyle::setClipPath),

        new PropertyWrapperShape(CSSPropertyShapeOutside, &RenderStyle::shapeOutside, &RenderStyle::setShapeOutside),
        new LengthPropertyWrapper(CSSPropertyShapeMargin, &RenderStyle::shapeMargin, &RenderStyle::setShapeMargin, { LengthPropertyWrapper::Flags::IsLengthPercentage, LengthPropertyWrapper::Flags::NegativeLengthsAreInvalid }),
        new PropertyWrapper<float>(CSSPropertyShapeImageThreshold, &RenderStyle::shapeImageThreshold, &RenderStyle::setShapeImageThreshold),

        new PropertyWrapperVisitedAffectedColor(CSSPropertyColumnRuleColor, &RenderStyle::columnRuleColor, &RenderStyle::setColumnRuleColor, &RenderStyle::visitedLinkColumnRuleColor, &RenderStyle::setVisitedLinkColumnRuleColor),
        new PropertyWrapperVisitedAffectedColor(CSSPropertyWebkitTextStrokeColor, &RenderStyle::textStrokeColor, &RenderStyle::setTextStrokeColor, &RenderStyle::visitedLinkTextStrokeColor, &RenderStyle::setVisitedLinkTextStrokeColor),
        new PropertyWrapperVisitedAffectedColor(CSSPropertyWebkitTextFillColor, &RenderStyle::textFillColor, &RenderStyle::setTextFillColor, &RenderStyle::visitedLinkTextFillColor, &RenderStyle::setVisitedLinkTextFillColor),
        new PropertyWrapperVisitedAffectedColor(CSSPropertyBorderLeftColor, &RenderStyle::borderLeftColor, &RenderStyle::setBorderLeftColor, &RenderStyle::visitedLinkBorderLeftColor, &RenderStyle::setVisitedLinkBorderLeftColor),
        new PropertyWrapperVisitedAffectedColor(CSSPropertyBorderRightColor, &RenderStyle::borderRightColor, &RenderStyle::setBorderRightColor, &RenderStyle::visitedLinkBorderRightColor, &RenderStyle::setVisitedLinkBorderRightColor),
        new PropertyWrapperVisitedAffectedColor(CSSPropertyBorderTopColor, &RenderStyle::borderTopColor, &RenderStyle::setBorderTopColor, &RenderStyle::visitedLinkBorderTopColor, &RenderStyle::setVisitedLinkBorderTopColor),
        new PropertyWrapperVisitedAffectedColor(CSSPropertyBorderBottomColor, &RenderStyle::borderBottomColor, &RenderStyle::setBorderBottomColor, &RenderStyle::visitedLinkBorderBottomColor, &RenderStyle::setVisitedLinkBorderBottomColor),
        new PropertyWrapperVisitedAffectedColor(CSSPropertyOutlineColor, &RenderStyle::outlineColor, &RenderStyle::setOutlineColor, &RenderStyle::visitedLinkOutlineColor, &RenderStyle::setVisitedLinkOutlineColor),

        new PropertyWrapperShadow(CSSPropertyBoxShadow, &RenderStyle::boxShadow, &RenderStyle::setBoxShadow),
        new PropertyWrapperShadow(CSSPropertyWebkitBoxShadow, &RenderStyle::boxShadow, &RenderStyle::setBoxShadow),
        new PropertyWrapperShadow(CSSPropertyTextShadow, &RenderStyle::textShadow, &RenderStyle::setTextShadow),

        new PropertyWrapperSVGPaint(CSSPropertyFill, &RenderStyle::fillPaintType, &RenderStyle::fillPaintColor, &RenderStyle::setFillPaintColor),
        new PropertyWrapper<float>(CSSPropertyFillOpacity, &RenderStyle::fillOpacity, &RenderStyle::setFillOpacity),

        new PropertyWrapperSVGPaint(CSSPropertyStroke, &RenderStyle::strokePaintType, &RenderStyle::strokePaintColor, &RenderStyle::setStrokePaintColor),
        new PropertyWrapper<float>(CSSPropertyStrokeOpacity, &RenderStyle::strokeOpacity, &RenderStyle::setStrokeOpacity),
        new StrokeDasharrayPropertyWrapper,
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
        new PropertyWrapperColor(CSSPropertyFloodColor, &RenderStyle::floodColor, &RenderStyle::setFloodColor),

        new PropertyWrapper<float>(CSSPropertyStopOpacity, &RenderStyle::stopOpacity, &RenderStyle::setStopOpacity),
        new PropertyWrapperColor(CSSPropertyStopColor, &RenderStyle::stopColor, &RenderStyle::setStopColor),

        new PropertyWrapperColor(CSSPropertyLightingColor, &RenderStyle::lightingColor, &RenderStyle::setLightingColor),

        new PropertyWrapperBaselineShift,
        new PropertyWrapper<SVGLengthValue>(CSSPropertyKerning, &RenderStyle::kerning, &RenderStyle::setKerning),
#if ENABLE(VARIATION_FONTS)
        new PropertyWrapperFontVariationSettings(CSSPropertyFontVariationSettings, &RenderStyle::fontVariationSettings, &RenderStyle::setFontVariationSettings),
#endif
        new PropertyWrapperFontSizeAdjust,
        new PropertyWrapperFontWeight,
        new PropertyWrapper<FontSelectionValue>(CSSPropertyFontStretch, &RenderStyle::fontStretch, &RenderStyle::setFontStretch),
        new PropertyWrapperFontStyle,
        new PropertyWrapper<TextDecorationThickness>(CSSPropertyTextDecorationThickness, &RenderStyle::textDecorationThickness, &RenderStyle::setTextDecorationThickness),
        new PropertyWrapper<TextUnderlineOffset>(CSSPropertyTextUnderlineOffset, &RenderStyle::textUnderlineOffset, &RenderStyle::setTextUnderlineOffset),
        new PropertyWrapperVisitedAffectedColor(CSSPropertyTextDecorationColor, &RenderStyle::textDecorationColor, &RenderStyle::setTextDecorationColor, &RenderStyle::visitedLinkTextDecorationColor, &RenderStyle::setVisitedLinkTextDecorationColor),

        new LengthPropertyWrapper(CSSPropertyFlexBasis, &RenderStyle::flexBasis, &RenderStyle::setFlexBasis, { LengthPropertyWrapper::Flags::IsLengthPercentage, LengthPropertyWrapper::Flags::NegativeLengthsAreInvalid }),
        new FloatPropertyWrapper(CSSPropertyFlexGrow, &RenderStyle::flexGrow, &RenderStyle::setFlexGrow, FloatPropertyWrapper::ValueRange::NonNegative),
        new FloatPropertyWrapper(CSSPropertyFlexShrink, &RenderStyle::flexShrink, &RenderStyle::setFlexShrink, FloatPropertyWrapper::ValueRange::NonNegative),
        new PropertyWrapper<int>(CSSPropertyOrder, &RenderStyle::order, &RenderStyle::setOrder),

        new TabSizePropertyWrapper,

        new ContainIntrinsiclLengthPropertyWrapper(CSSPropertyContainIntrinsicWidth, &RenderStyle::containIntrinsicWidth, &RenderStyle::setContainIntrinsicWidth, &RenderStyle::containIntrinsicWidthType, &RenderStyle::setContainIntrinsicWidthType),
        new ContainIntrinsiclLengthPropertyWrapper(CSSPropertyContainIntrinsicHeight, &RenderStyle::containIntrinsicHeight, &RenderStyle::setContainIntrinsicHeight, &RenderStyle::containIntrinsicHeightType, &RenderStyle::setContainIntrinsicHeightType),

        // FIXME: The following properties are currently not animatable but should be:
        // background-blend-mode, clip-rule, color-interpolation,
        // color-interpolation-filters, counter-increment, counter-reset, dominant-baseline,
        // fill-rule, font-family, font-feature-settings, font-kerning, font-language-override,
        // font-synthesis, font-variant-alternates, font-variant-caps, font-variant-east-asian,
        // font-variant-ligatures, font-variant-numeric, font-variant-position, grid-template-areas,
        // ime-mode, marker-end, marker-mid, marker-start, mask, mask-clip, mask-composite, mask-image,
        // mask-mode, mask-origin, mask-repeat, mask-type, offset-distance, perspective-origin, quotes,
        // ruby-align, scroll-behavior, shape-rendering, stroke-linecap, stroke-linejoin,
        // text-anchor, text-emphasis-style, text-rendering, vector-effect
        new DiscretePropertyWrapper<const StyleContentAlignmentData&>(CSSPropertyAlignContent, &RenderStyle::alignContent, &RenderStyle::setAlignContent),
        new DiscretePropertyWrapper<const StyleSelfAlignmentData&>(CSSPropertyAlignItems, &RenderStyle::alignItems, &RenderStyle::setAlignItems),
        new DiscretePropertyWrapper<const StyleSelfAlignmentData&>(CSSPropertyAlignSelf, &RenderStyle::alignSelf, &RenderStyle::setAlignSelf),
        new DiscretePropertyWrapper<BackfaceVisibility>(CSSPropertyBackfaceVisibility, &RenderStyle::backfaceVisibility, &RenderStyle::setBackfaceVisibility),
        new DiscretePropertyWrapper<FillAttachment>(CSSPropertyBackgroundAttachment, &RenderStyle::backgroundAttachment, &RenderStyle::setBackgroundAttachment),
        new DiscretePropertyWrapper<FillBox>(CSSPropertyBackgroundClip, &RenderStyle::backgroundClip, &RenderStyle::setBackgroundClip),
        new DiscretePropertyWrapper<FillBox>(CSSPropertyBackgroundOrigin, &RenderStyle::backgroundOrigin, &RenderStyle::setBackgroundOrigin),
        new DiscretePropertyWrapper<FillRepeatXY>(CSSPropertyBackgroundRepeat, &RenderStyle::backgroundRepeat, &RenderStyle::setBackgroundRepeat),
        new DiscretePropertyWrapper<BorderStyle>(CSSPropertyBorderBottomStyle, &RenderStyle::borderBottomStyle, &RenderStyle::setBorderBottomStyle),
        new DiscretePropertyWrapper<BorderCollapse>(CSSPropertyBorderCollapse, &RenderStyle::borderCollapse, &RenderStyle::setBorderCollapse),
        new BorderImageRepeatWrapper,
        new DiscretePropertyWrapper<BorderStyle>(CSSPropertyBorderLeftStyle, &RenderStyle::borderLeftStyle, &RenderStyle::setBorderLeftStyle),
        new DiscretePropertyWrapper<BorderStyle>(CSSPropertyBorderRightStyle, &RenderStyle::borderRightStyle, &RenderStyle::setBorderRightStyle),
        new DiscretePropertyWrapper<BorderStyle>(CSSPropertyBorderTopStyle, &RenderStyle::borderTopStyle, &RenderStyle::setBorderTopStyle),
        new DiscretePropertyWrapper<BoxSizing>(CSSPropertyBoxSizing, &RenderStyle::boxSizing, &RenderStyle::setBoxSizing),
        new DiscretePropertyWrapper<CaptionSide>(CSSPropertyCaptionSide, &RenderStyle::captionSide, &RenderStyle::setCaptionSide),
        new DiscretePropertyWrapper<Clear>(CSSPropertyClear, &RenderStyle::clear, &RenderStyle::setClear),
        new DiscretePropertyWrapper<LeadingTrim>(CSSPropertyLeadingTrim, &RenderStyle::leadingTrim, &RenderStyle::setLeadingTrim),
        new DiscretePropertyWrapper<PrintColorAdjust>(CSSPropertyPrintColorAdjust, &RenderStyle::printColorAdjust, &RenderStyle::setPrintColorAdjust),
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
        new GridTemplatePropertyWrapper(CSSPropertyGridTemplateRows, &RenderStyle::gridRowList, &RenderStyle::setGridRowList),
        new GridTemplatePropertyWrapper(CSSPropertyGridTemplateColumns, &RenderStyle::gridColumnList, &RenderStyle::setGridColumnList),
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
        new DiscretePropertyWrapper<LineBreak>(CSSPropertyLineBreak, &RenderStyle::lineBreak, &RenderStyle::setLineBreak),
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
        new DiscretePropertyWrapper<TextAlignLast>(CSSPropertyTextAlignLast, &RenderStyle::textAlignLast, &RenderStyle::setTextAlignLast),
        new DiscretePropertyWrapper<OptionSet<TextDecorationLine>>(CSSPropertyTextDecorationLine, &RenderStyle::textDecorationLine, &RenderStyle::setTextDecorationLine),
        new DiscretePropertyWrapper<TextDecorationStyle>(CSSPropertyTextDecorationStyle, &RenderStyle::textDecorationStyle, &RenderStyle::setTextDecorationStyle),
        new PropertyWrapperVisitedAffectedColor(CSSPropertyTextEmphasisColor, &RenderStyle::textEmphasisColor, &RenderStyle::setTextEmphasisColor, &RenderStyle::visitedLinkTextEmphasisColor, &RenderStyle::setVisitedLinkTextEmphasisColor),
        new DiscretePropertyWrapper<OptionSet<TextEmphasisPosition>>(CSSPropertyTextEmphasisPosition, &RenderStyle::textEmphasisPosition, &RenderStyle::setTextEmphasisPosition),
        new TextEmphasisStyleWrapper,
        new DiscretePropertyWrapper<TextJustify>(CSSPropertyTextJustify, &RenderStyle::textJustify, &RenderStyle::setTextJustify),
        new DiscretePropertyWrapper<TextOverflow>(CSSPropertyTextOverflow, &RenderStyle::textOverflow, &RenderStyle::setTextOverflow),
        new DiscretePropertyWrapper<OptionSet<TouchAction>>(CSSPropertyTouchAction, &RenderStyle::touchActions, &RenderStyle::setTouchActions),
        new DiscretePropertyWrapper<TextTransform>(CSSPropertyTextTransform, &RenderStyle::textTransform, &RenderStyle::setTextTransform),
        new DiscretePropertyWrapper<TransformBox>(CSSPropertyTransformBox, &RenderStyle::transformBox, &RenderStyle::setTransformBox),
        new DiscretePropertyWrapper<TransformStyle3D>(CSSPropertyTransformStyle, &RenderStyle::transformStyle3D, &RenderStyle::setTransformStyle3D),
        new DiscretePropertyWrapper<WhiteSpace>(CSSPropertyWhiteSpace, &RenderStyle::whiteSpace, &RenderStyle::setWhiteSpace),
        new DiscretePropertyWrapper<WordBreak>(CSSPropertyWordBreak, &RenderStyle::wordBreak, &RenderStyle::setWordBreak),
        new DiscretePropertyWrapper<OverflowAnchor>(CSSPropertyOverflowAnchor, &RenderStyle::overflowAnchor, &RenderStyle::setOverflowAnchor),

#if ENABLE(CSS_BOX_DECORATION_BREAK)
        new DiscretePropertyWrapper<BoxDecorationBreak>(CSSPropertyWebkitBoxDecorationBreak, &RenderStyle::boxDecorationBreak, &RenderStyle::setBoxDecorationBreak),
#endif
#if ENABLE(CSS_COMPOSITING)
        new DiscretePropertyWrapper<Isolation>(CSSPropertyIsolation, &RenderStyle::isolation, &RenderStyle::setIsolation),
        new DiscretePropertyWrapper<BlendMode>(CSSPropertyMixBlendMode, &RenderStyle::blendMode, &RenderStyle::setBlendMode),
        new DiscretePropertyWrapper<BlendMode>(CSSPropertyBackgroundBlendMode, &RenderStyle::backgroundBlendMode, &RenderStyle::setBackgroundBlendMode),
#endif
        new PropertyWrapperAspectRatio,
        new DiscretePropertyWrapper<FontPalette>(CSSPropertyFontPalette, &RenderStyle::fontPalette, &RenderStyle::setFontPalette),

        new PathOperationPropertyWrapper(CSSPropertyOffsetPath, &RenderStyle::offsetPath, &RenderStyle::setOffsetPath),
        new LengthPropertyWrapper(CSSPropertyOffsetDistance, &RenderStyle::offsetDistance, &RenderStyle::setOffsetDistance, LengthPropertyWrapper::Flags::IsLengthPercentage),
        new LengthPointOrAutoPropertyWrapper(CSSPropertyOffsetPosition, &RenderStyle::offsetPosition, &RenderStyle::setOffsetPosition),
        new LengthPointOrAutoPropertyWrapper(CSSPropertyOffsetAnchor, &RenderStyle::offsetAnchor, &RenderStyle::setOffsetAnchor),
        new PropertyWrapperContent,
        new OffsetRotatePropertyWrapper(CSSPropertyOffsetRotate, &RenderStyle::offsetRotate, &RenderStyle::setOffsetRotate),
        new DiscretePropertyWrapper<TextDecorationSkipInk>(CSSPropertyTextDecorationSkipInk, &RenderStyle::textDecorationSkipInk, &RenderStyle::setTextDecorationSkipInk),
        new DiscreteSVGPropertyWrapper<ColorInterpolation>(CSSPropertyColorInterpolation, &SVGRenderStyle::colorInterpolation, &SVGRenderStyle::setColorInterpolation),
        new DiscreteFontDescriptionTypedWrapper<Kerning>(CSSPropertyFontKerning, &FontCascadeDescription::kerning, &FontCascadeDescription::setKerning),
        new FontFeatureSettingsWrapper,
        new FontFamilyWrapper,
        new DiscreteSVGPropertyWrapper<WindRule>(CSSPropertyClipRule, &SVGRenderStyle::clipRule, &SVGRenderStyle::setClipRule),
        new DiscreteSVGPropertyWrapper<ColorInterpolation>(CSSPropertyColorInterpolationFilters, &SVGRenderStyle::colorInterpolationFilters, &SVGRenderStyle::setColorInterpolationFilters),
        new DiscreteSVGPropertyWrapper<DominantBaseline>(CSSPropertyDominantBaseline, &SVGRenderStyle::dominantBaseline, &SVGRenderStyle::setDominantBaseline),
        new CounterWrapper(CSSPropertyCounterIncrement),
        new CounterWrapper(CSSPropertyCounterReset),
        new DiscreteSVGPropertyWrapper<WindRule>(CSSPropertyFillRule, &SVGRenderStyle::fillRule, &SVGRenderStyle::setFillRule),
        new DiscreteFontDescriptionTypedWrapper<FontSynthesisLonghandValue>(CSSPropertyFontSynthesisWeight, &FontCascadeDescription::fontSynthesisWeight, &FontCascadeDescription::setFontSynthesisWeight),
        new DiscreteFontDescriptionTypedWrapper<FontSynthesisLonghandValue>(CSSPropertyFontSynthesisStyle, &FontCascadeDescription::fontSynthesisStyle, &FontCascadeDescription::setFontSynthesisStyle),
        new DiscreteFontDescriptionTypedWrapper<FontSynthesisLonghandValue>(CSSPropertyFontSynthesisSmallCaps, &FontCascadeDescription::fontSynthesisSmallCaps, &FontCascadeDescription::setFontSynthesisSmallCaps),
        new DiscreteFontDescriptionTypedWrapper<FontVariantAlternates>(CSSPropertyFontVariantAlternates, &FontCascadeDescription::variantAlternates, &FontCascadeDescription::setVariantAlternates),
        new FontVariantEastAsianWrapper,
        new FontVariantLigaturesWrapper,
        new FontVariantNumericWrapper,
        new DiscreteFontDescriptionTypedWrapper<FontVariantPosition>(CSSPropertyFontVariantPosition, &FontCascadeDescription::variantPosition, &FontCascadeDescription::setVariantPosition),
        new DiscreteFontDescriptionTypedWrapper<FontVariantCaps>(CSSPropertyFontVariantCaps, &FontCascadeDescription::variantCaps, &FontCascadeDescription::setVariantCaps),
        new GridTemplateAreasWrapper,
        new QuotesWrapper,
        new DiscretePropertyWrapper<bool>(CSSPropertyScrollBehavior, &RenderStyle::useSmoothScrolling, &RenderStyle::setUseSmoothScrolling),
        new DiscreteFontDescriptionTypedWrapper<TextRenderingMode>(CSSPropertyTextRendering, &FontCascadeDescription::textRenderingMode, &FontCascadeDescription::setTextRenderingMode),
        new DiscreteSVGPropertyWrapper<MaskType>(CSSPropertyMaskType, &SVGRenderStyle::maskType, &SVGRenderStyle::setMaskType),
        new DiscretePropertyWrapper<LineCap>(CSSPropertyStrokeLinecap, &RenderStyle::capStyle, &RenderStyle::setCapStyle),
        new DiscretePropertyWrapper<LineJoin>(CSSPropertyStrokeLinejoin, &RenderStyle::joinStyle, &RenderStyle::setJoinStyle),
        new DiscreteSVGPropertyWrapper<TextAnchor>(CSSPropertyTextAnchor, &SVGRenderStyle::textAnchor, &SVGRenderStyle::setTextAnchor),
        new DiscreteSVGPropertyWrapper<VectorEffect>(CSSPropertyVectorEffect, &SVGRenderStyle::vectorEffect, &SVGRenderStyle::setVectorEffect),
        new DiscreteSVGPropertyWrapper<ShapeRendering>(CSSPropertyShapeRendering, &SVGRenderStyle::shapeRendering, &SVGRenderStyle::setShapeRendering),
        new DiscreteSVGPropertyWrapper<const String&>(CSSPropertyMarkerEnd, &SVGRenderStyle::markerEndResource, &SVGRenderStyle::setMarkerEndResource),
        new DiscreteSVGPropertyWrapper<const String&>(CSSPropertyMarkerMid, &SVGRenderStyle::markerMidResource, &SVGRenderStyle::setMarkerMidResource),
        new DiscreteSVGPropertyWrapper<const String&>(CSSPropertyMarkerStart, &SVGRenderStyle::markerStartResource, &SVGRenderStyle::setMarkerStartResource)
    };
    const unsigned animatableLonghandPropertiesCount = WTF_ARRAY_LENGTH(animatableLonghandPropertyWrappers);

    static const CSSPropertyID animatableShorthandProperties[] = {
        CSSPropertyAll,
        CSSPropertyBackground, // for background-color, background-position, background-image
        CSSPropertyBackgroundPosition,
        CSSPropertyFont, // for font-size, font-weight
        CSSPropertyMask, // for mask-position
        CSSPropertyWebkitMask, // for mask-position
        CSSPropertyMaskPosition,
        CSSPropertyWebkitMaskPosition,
        CSSPropertyBorderTop, CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft,
        CSSPropertyBorderBlockStart, CSSPropertyBorderBlockEnd, CSSPropertyBorderInlineStart, CSSPropertyBorderInlineEnd,
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
        CSSPropertyPerspectiveOrigin,
        CSSPropertyOffset,
        CSSPropertyTextEmphasis,
        CSSPropertyFontVariant,
        CSSPropertyFontSynthesis,
        CSSPropertyContainIntrinsicSize
    };
    const unsigned animatableShorthandPropertiesCount = WTF_ARRAY_LENGTH(animatableShorthandProperties);

    // Make sure unused slots have a value
    for (int i = 0; i < numCSSProperties; ++i)
        m_propertyToIdMap[i] = cInvalidPropertyWrapperIndex;

    static_assert(animatableLonghandPropertiesCount + animatableShorthandPropertiesCount < std::numeric_limits<unsigned short>::max(), "number of AnimatableProperties must be less than UShrtMax");
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
        longhandWrappers.shrinkToFit();

        m_propertyWrappers.uncheckedAppend(makeUnique<ShorthandPropertyWrapper>(propertyID, WTFMove(longhandWrappers)));
        indexFromPropertyID(propertyID) = animatableLonghandPropertiesCount + i;
    }

#ifndef NDEBUG
    for (auto property : allCSSProperties()) {
        switch (property) {
        // If a property is not animatable per spec, add it to this list of cases.
        // When adding a new property, you should make sure it belongs in this list
        // or provide a wrapper for it above. If you are adding to this list but the
        // property should be animatable, make sure to file a bug.
#if ENABLE(DARK_MODE_CSS)
        case CSSPropertyColorScheme:
#endif
        case CSSPropertyDirection:
        case CSSPropertyDisplay:
#if ENABLE(VARIATION_FONTS)
        case CSSPropertyFontOpticalSizing:
#endif
        case CSSPropertyTextOrientation:
        case CSSPropertyWritingMode:
        case CSSPropertyWebkitFontSmoothing:
        case CSSPropertyWebkitLocale:
        case CSSPropertyWebkitTextOrientation:
#if ENABLE(TEXT_AUTOSIZING)
        case CSSPropertyWebkitTextSizeAdjust:
        case CSSPropertyInternalTextAutosizingStatus:
#endif
        case CSSPropertyWebkitTextZoom:
        case CSSPropertyAdditiveSymbols:
        case CSSPropertyAlignmentBaseline:
        case CSSPropertyAlt:
        case CSSPropertyAnimation:
        case CSSPropertyAnimationComposition:
        case CSSPropertyAnimationDelay:
        case CSSPropertyAnimationDirection:
        case CSSPropertyAnimationDuration:
        case CSSPropertyAnimationFillMode:
        case CSSPropertyAnimationIterationCount:
        case CSSPropertyAnimationName:
        case CSSPropertyAnimationPlayState:
        case CSSPropertyAnimationTimingFunction:
        case CSSPropertyAppearance:
        case CSSPropertyBasePalette:
        case CSSPropertyBorderBlock: // logical shorthand
        case CSSPropertyBorderBlockColor: // logical shorthand
        case CSSPropertyBorderBlockStyle: // logical shorthand
        case CSSPropertyBorderBlockWidth: // logical shorthand
        case CSSPropertyBorderInline: // logical shorthand
        case CSSPropertyBorderInlineColor: // logical shorthand
        case CSSPropertyBorderInlineStyle: // logical shorthand
        case CSSPropertyBorderInlineWidth: // logical shorthand
        case CSSPropertyBorderStyle:
        case CSSPropertyBufferedRendering:
        case CSSPropertyColumnSpan:
        case CSSPropertyColumns:
        case CSSPropertyContain:
        case CSSPropertyContainer:
        case CSSPropertyContainerName:
        case CSSPropertyContainerType:
        case CSSPropertyContentVisibility:
        case CSSPropertyFallback:
        case CSSPropertyFlex:
        case CSSPropertyFlexFlow:
        case CSSPropertyFontDisplay:
        case CSSPropertyGap:
        case CSSPropertyGlyphOrientationHorizontal:
        case CSSPropertyGlyphOrientationVertical:
        case CSSPropertyGrid:
        case CSSPropertyGridArea:
        case CSSPropertyGridColumn:
        case CSSPropertyGridRow:
        case CSSPropertyGridTemplate:
        case CSSPropertyHangingPunctuation:
        case CSSPropertyImageRendering:
        case CSSPropertyInlineSize:
        case CSSPropertyInputSecurity:
        case CSSPropertyInset:
        case CSSPropertyInsetBlock:
        case CSSPropertyInsetBlockEnd:
        case CSSPropertyInsetBlockStart:
        case CSSPropertyInsetInline:
        case CSSPropertyInsetInlineEnd:
        case CSSPropertyInsetInlineStart:
        case CSSPropertyListStyle:
        case CSSPropertyMarginBlock: // logical shorthand
        case CSSPropertyMarginInline: // logical shorthand
        case CSSPropertyMarker:
        case CSSPropertyMasonryAutoFlow:
        case CSSPropertyMathStyle:
        case CSSPropertyNegative:
        case CSSPropertyOverflow:
        case CSSPropertyOverrideColors:
        case CSSPropertyOverscrollBehavior:
        case CSSPropertyOverscrollBehaviorBlock:
        case CSSPropertyOverscrollBehaviorInline:
        case CSSPropertyOverscrollBehaviorX:
        case CSSPropertyOverscrollBehaviorY:
        case CSSPropertyPad:
        case CSSPropertyPaddingBlock: // logical shorthand
        case CSSPropertyPaddingInline: // logical shorthand
        case CSSPropertyPage:
        case CSSPropertyPlaceContent:
        case CSSPropertyPlaceItems:
        case CSSPropertyPlaceSelf:
        case CSSPropertyPrefix:
        case CSSPropertyRange:
        case CSSPropertyScrollMargin:
        case CSSPropertyScrollMarginBlock:
        case CSSPropertyScrollMarginBlockEnd:
        case CSSPropertyScrollMarginBlockStart:
        case CSSPropertyScrollMarginBottom:
        case CSSPropertyScrollMarginInline:
        case CSSPropertyScrollMarginInlineEnd:
        case CSSPropertyScrollMarginInlineStart:
        case CSSPropertyScrollMarginLeft:
        case CSSPropertyScrollMarginRight:
        case CSSPropertyScrollMarginTop:
        case CSSPropertyScrollPadding:
        case CSSPropertyScrollPaddingBlock:
        case CSSPropertyScrollPaddingBlockEnd:
        case CSSPropertyScrollPaddingBlockStart:
        case CSSPropertyScrollPaddingBottom:
        case CSSPropertyScrollPaddingInline:
        case CSSPropertyScrollPaddingInlineEnd:
        case CSSPropertyScrollPaddingInlineStart:
        case CSSPropertyScrollPaddingLeft:
        case CSSPropertyScrollPaddingRight:
        case CSSPropertyScrollPaddingTop:
        case CSSPropertyScrollSnapAlign:
        case CSSPropertyScrollSnapStop:
        case CSSPropertyScrollSnapType:
        case CSSPropertyTextEdge:
        case CSSPropertySize:
        case CSSPropertySpeakAs:
        case CSSPropertySrc:
        case CSSPropertyStrokeColor:
        case CSSPropertySuffix:
        case CSSPropertySymbols:
        case CSSPropertySystem:
        case CSSPropertyTextCombineUpright:
        case CSSPropertyTextDecoration:
        case CSSPropertyTextDecorationSkip:
        case CSSPropertyTextUnderlinePosition:
        case CSSPropertyTransition:
        case CSSPropertyTransitionDelay:
        case CSSPropertyTransitionDuration:
        case CSSPropertyTransitionProperty:
        case CSSPropertyTransitionTimingFunction:
        case CSSPropertyUnicodeBidi:
        case CSSPropertyUnicodeRange:
        case CSSPropertyWillChange:
#if ENABLE(APPLE_PAY)
        case CSSPropertyApplePayButtonStyle:
        case CSSPropertyApplePayButtonType:
#endif
        case CSSPropertyWebkitBackgroundClip:
        case CSSPropertyWebkitBackgroundOrigin:
        case CSSPropertyWebkitBorderImage:
        case CSSPropertyWebkitBoxAlign:
        case CSSPropertyWebkitBoxDirection:
        case CSSPropertyWebkitBoxFlex:
        case CSSPropertyWebkitBoxFlexGroup:
        case CSSPropertyWebkitBoxLines:
        case CSSPropertyWebkitBoxOrdinalGroup:
        case CSSPropertyWebkitBoxOrient:
        case CSSPropertyWebkitBoxPack:
        case CSSPropertyWebkitBoxReflect:
        case CSSPropertyWebkitColumnAxis:
        case CSSPropertyWebkitColumnBreakAfter:
        case CSSPropertyWebkitColumnBreakBefore:
        case CSSPropertyWebkitColumnBreakInside:
        case CSSPropertyWebkitColumnProgression:
#if ENABLE(CURSOR_VISIBILITY)
        case CSSPropertyWebkitCursorVisibility:
#endif
        case CSSPropertyWebkitFontSizeDelta:
        case CSSPropertyWebkitHyphenateCharacter:
        case CSSPropertyWebkitHyphenateLimitAfter:
        case CSSPropertyWebkitHyphenateLimitBefore:
        case CSSPropertyWebkitHyphenateLimitLines:
        case CSSPropertyWebkitLineAlign:
        case CSSPropertyWebkitLineBoxContain:
        case CSSPropertyWebkitLineClamp:
        case CSSPropertyWebkitLineGrid:
        case CSSPropertyWebkitLineSnap:
        case CSSPropertyWebkitMarqueeDirection:
        case CSSPropertyWebkitMarqueeIncrement:
        case CSSPropertyWebkitMarqueeRepetition:
        case CSSPropertyWebkitMarqueeSpeed:
        case CSSPropertyWebkitMarqueeStyle:
        case CSSPropertyWebkitMaskBoxImage:
        case CSSPropertyWebkitMaskBoxImageOutset:
        case CSSPropertyWebkitMaskBoxImageRepeat:
        case CSSPropertyWebkitMaskBoxImageSlice:
        case CSSPropertyWebkitMaskBoxImageSource:
        case CSSPropertyWebkitMaskBoxImageWidth:
        case CSSPropertyWebkitMaskClip:
        case CSSPropertyWebkitMaskComposite:
        case CSSPropertyWebkitMaskSourceType:
        case CSSPropertyWebkitNbspMode:
        case CSSPropertyWebkitPerspective:
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
        case CSSPropertyWebkitOverflowScrolling:
#endif
        case CSSPropertyWebkitRtlOrdering:
#if ENABLE(TOUCH_EVENTS)
        case CSSPropertyWebkitTapHighlightColor:
#endif
        case CSSPropertyWebkitTextCombine:
        case CSSPropertyWebkitTextDecoration:
        case CSSPropertyWebkitTextDecorationsInEffect:
        case CSSPropertyWebkitTextSecurity:
        case CSSPropertyWebkitTextStroke:
        case CSSPropertyWebkitTextStrokeWidth:
#if PLATFORM(IOS_FAMILY)
        case CSSPropertyWebkitTouchCallout:
#endif
        case CSSPropertyWebkitUserDrag:
        case CSSPropertyWebkitUserModify:
        case CSSPropertyWebkitUserSelect:
            continue;
        default:
            auto resolvedProperty = CSSProperty::resolveDirectionAwareProperty(property, RenderStyle::initialDirection(), RenderStyle::initialWritingMode());
            ASSERT_UNUSED(resolvedProperty, wrapperForProperty(resolvedProperty));
            break;
        }
    }
#endif
}

static void blendStandardProperty(const CSSPropertyBlendingClient& client, CSSPropertyID property, RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, double progress, CompositeOperation compositeOperation, IterationCompositeOperation iterationCompositeOperation, double currentIteration)
{
    ASSERT(property != CSSPropertyInvalid && property != CSSPropertyCustom);

    AnimationPropertyWrapperBase* wrapper = CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(property);
    if (wrapper) {
        // https://drafts.csswg.org/web-animations-1/#discrete
        // The property's values cannot be meaningfully combined, thus it is not additive and
        // interpolation swaps from Va to Vb at 50% (p=0.5).
        auto isDiscrete = !wrapper->canInterpolate(from, to, compositeOperation);
        if (isDiscrete) {
            // If we want additive, we should specify progress at 0 actually and return from.
            progress = progress < 0.5 ? 0 : 1;
            compositeOperation = CompositeOperation::Replace;
        }
        wrapper->blend(destination, from, to, { progress, isDiscrete, compositeOperation, client, property, iterationCompositeOperation, currentIteration });
#if !LOG_DISABLED
        wrapper->logBlend(from, to, destination, progress);
#endif
    }
}

static CSSCustomPropertyValue::NumericSyntaxValue blendFunc(const CSSCustomPropertyValue::NumericSyntaxValue& from, const CSSCustomPropertyValue::NumericSyntaxValue& to, const CSSPropertyBlendingContext& blendingContext)
{
    ASSERT(from.unitType == to.unitType);
    return { blendFunc(from.value, to.value, blendingContext), from.unitType };
}

static std::optional<CSSCustomPropertyValue::SyntaxValue> blendSyntaxValues(const RenderStyle& fromStyle, const RenderStyle& toStyle, const CSSCustomPropertyValue::SyntaxValue& from, const CSSCustomPropertyValue::SyntaxValue& to, const CSSPropertyBlendingContext& blendingContext)
{
    if (std::holds_alternative<Length>(from) && std::holds_alternative<Length>(to))
        return blendFunc(std::get<Length>(from), std::get<Length>(to), blendingContext);

    if (std::holds_alternative<StyleColor>(from) && std::holds_alternative<StyleColor>(to)) {
        auto& fromStyleColor = std::get<StyleColor>(from);
        auto& toStyleColor = std::get<StyleColor>(to);
        if (!RenderStyle::isCurrentColor(fromStyleColor) || !RenderStyle::isCurrentColor(toStyleColor))
            return blendFunc(fromStyle.colorResolvingCurrentColor(fromStyleColor), toStyle.colorResolvingCurrentColor(toStyleColor), blendingContext);
    }

    if (std::holds_alternative<CSSCustomPropertyValue::NumericSyntaxValue>(from) && std::holds_alternative<CSSCustomPropertyValue::NumericSyntaxValue>(to)) {
        auto& fromNumeric = std::get<CSSCustomPropertyValue::NumericSyntaxValue>(from);
        auto& toNumeric = std::get<CSSCustomPropertyValue::NumericSyntaxValue>(to);
        if (fromNumeric.unitType == toNumeric.unitType)
            return blendFunc(fromNumeric, toNumeric, blendingContext);
    }

    if (std::holds_alternative<RefPtr<TransformOperation>>(from) && std::holds_alternative<RefPtr<TransformOperation>>(to)) {
        auto& fromTransformOperation = std::get<RefPtr<TransformOperation>>(from);
        auto& toTransformOperation = std::get<RefPtr<TransformOperation>>(to);
        return blendFunc(fromTransformOperation.get(), toTransformOperation.get(), blendingContext);
    }

    return std::nullopt;
}

static std::optional<CSSCustomPropertyValue::SyntaxValueList> blendSyntaxValueLists(const RenderStyle& fromStyle, const RenderStyle& toStyle, const CSSCustomPropertyValue::SyntaxValueList& from, const CSSCustomPropertyValue::SyntaxValueList& to, const CSSPropertyBlendingContext& blendingContext)
{
    // https://drafts.css-houdini.org/css-properties-values-api-1/#animation-behavior-of-custom-properties

    auto firstValue = [&]() -> std::optional<CSSCustomPropertyValue::SyntaxValue> {
        if (!from.values.isEmpty())
            return from.values[0];
        if (!to.values.isEmpty())
            return to.values[0];
        return std::nullopt;
    }();

    if (!firstValue)
        return std::nullopt;

    // <transform-function> lists are special in that they don't require matching numbers of items.
    if (std::holds_alternative<RefPtr<TransformOperation>>(*firstValue)) {
        auto transformOperationsFromSyntaxValueList = [](const CSSCustomPropertyValue::SyntaxValueList& src) {
            TransformOperations transformOperations;
            for (auto& syntaxValue : src.values) {
                ASSERT(std::holds_alternative<RefPtr<TransformOperation>>(syntaxValue));
                transformOperations.operations().append(std::get<RefPtr<TransformOperation>>(syntaxValue));
            }
            return transformOperations;
        };

        auto fromTransformOperations = transformOperationsFromSyntaxValueList(from);
        auto toTransformOperations = transformOperationsFromSyntaxValueList(to);
        auto blendedTransformOperations = blendFunc(fromTransformOperations, toTransformOperations, blendingContext);
        auto blendedSyntaxValues = blendedTransformOperations.operations().map([](auto& transformOperation) -> CSSCustomPropertyValue::SyntaxValue {
            return transformOperation;
        });
        return CSSCustomPropertyValue::SyntaxValueList { blendedSyntaxValues, from.separator };
    }

    // Other lists must have matching sizes.
    if (from.values.size() != to.values.size())
        return std::nullopt;

    Vector<CSSCustomPropertyValue::SyntaxValue> blendedSyntaxValues;
    for (size_t i = 0; i < from.values.size(); ++i) {
        auto blendedSyntaxValue = blendSyntaxValues(fromStyle, toStyle, from.values[i], to.values[i], blendingContext);
        if (!blendedSyntaxValue)
            return std::nullopt;
        blendedSyntaxValues.append(*blendedSyntaxValue);
    }

    return CSSCustomPropertyValue::SyntaxValueList { blendedSyntaxValues, from.separator };
}

static Ref<CSSCustomPropertyValue> blendedCSSCustomPropertyValue(const RenderStyle& fromStyle, const RenderStyle& toStyle, const CSSCustomPropertyValue& from, const CSSCustomPropertyValue& to, const CSSPropertyBlendingContext& blendingContext)
{
    if (std::holds_alternative<CSSCustomPropertyValue::SyntaxValue>(from.value()) && std::holds_alternative<CSSCustomPropertyValue::SyntaxValue>(to.value())) {
        auto& fromSyntaxValue = std::get<CSSCustomPropertyValue::SyntaxValue>(from.value());
        auto& toSyntaxValue = std::get<CSSCustomPropertyValue::SyntaxValue>(to.value());
        if (auto blendedSyntaxValue = blendSyntaxValues(fromStyle, toStyle, fromSyntaxValue, toSyntaxValue, blendingContext))
            return CSSCustomPropertyValue::createForSyntaxValue(from.name(), WTFMove(*blendedSyntaxValue));
    }

    if (std::holds_alternative<CSSCustomPropertyValue::SyntaxValueList>(from.value()) && std::holds_alternative<CSSCustomPropertyValue::SyntaxValueList>(to.value())) {
        auto& fromSyntaxValueList = std::get<CSSCustomPropertyValue::SyntaxValueList>(from.value());
        auto& toSyntaxValueList = std::get<CSSCustomPropertyValue::SyntaxValueList>(to.value());
        if (auto blendedSyntaxValueList = blendSyntaxValueLists(fromStyle, toStyle, fromSyntaxValueList, toSyntaxValueList, blendingContext))
            return CSSCustomPropertyValue::createForSyntaxValueList(from.name(), WTFMove(*blendedSyntaxValueList));
    }

    // Use a discrete interpolation for all other cases.
    return CSSCustomPropertyValue::create(blendingContext.progress < 0.5 ? from : to);
}

static std::pair<const CSSCustomPropertyValue*, const CSSCustomPropertyValue*> customPropertyValuesForBlending(const CSSPropertyBlendingClient& client, const AtomString& customProperty, const CSSCustomPropertyValue* fromValue, const CSSCustomPropertyValue* toValue)
{
    // FIXME: it would be convenient if RenderStyle stored the initialValue
    // for a custom property that was not explicitly set.
    auto initialValue = [&]() -> const CSSCustomPropertyValue* {
        if (auto* document = client.document()) {
            if (auto registered = document->registeredCSSCustomProperties().get(customProperty))
                return registered->initialValue();
        }
        return nullptr;
    };

    if (!!fromValue == !!toValue)
        return { fromValue, toValue };
    if (!fromValue)
        return { initialValue(), toValue };
    return { fromValue, initialValue() };
}

static void blendCustomProperty(const CSSPropertyBlendingClient& client, const AtomString& customProperty, RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, double progress, CompositeOperation compositeOperation, IterationCompositeOperation iterationCompositeOperation, double currentIteration)
{
    CSSPropertyBlendingContext blendingContext { progress, false, compositeOperation, client, customProperty, iterationCompositeOperation, currentIteration };

    {
        auto [fromValue, toValue] = customPropertyValuesForBlending(client, customProperty, from.nonInheritedCustomProperties().get(customProperty), to.nonInheritedCustomProperties().get(customProperty));
        if (fromValue && toValue)
            destination.setNonInheritedCustomPropertyValue(customProperty, blendedCSSCustomPropertyValue(from, to, *fromValue, *toValue, blendingContext));
    }

    {
        auto [fromValue, toValue] = customPropertyValuesForBlending(client, customProperty, from.inheritedCustomProperties().get(customProperty), to.inheritedCustomProperties().get(customProperty));
        if (fromValue && toValue)
            destination.setInheritedCustomPropertyValue(customProperty, blendedCSSCustomPropertyValue(from, to, *fromValue, *toValue, blendingContext));
    }
}

void CSSPropertyAnimation::blendProperty(const CSSPropertyBlendingClient& client, AnimatableProperty property, RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, double progress, CompositeOperation compositeOperation, IterationCompositeOperation iterationCompositeOperation, double currentIteration)
{
    WTF::switchOn(property,
        [&] (CSSPropertyID propertyId) {
            blendStandardProperty(client, propertyId, destination, from, to, progress, compositeOperation, iterationCompositeOperation, currentIteration);
        }, [&] (const AtomString& customProperty) {
            blendCustomProperty(client, customProperty, destination, from, to, progress, compositeOperation, iterationCompositeOperation, currentIteration);
        }
    );
}

bool CSSPropertyAnimation::isPropertyAnimatable(CSSPropertyID property)
{
    return property == CSSPropertyCustom || CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(property);
}

bool CSSPropertyAnimation::isPropertyAdditiveOrCumulative(AnimatableProperty property)
{
    return WTF::switchOn(property,
        [] (CSSPropertyID propertyId) {
            if (auto* wrapper = CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(propertyId))
                return wrapper->isAdditiveOrCumulative();
            return false;
        }, [] (const AtomString&) { return true; }
    );
}

bool CSSPropertyAnimation::propertyRequiresBlendingForAccumulativeIteration(const CSSPropertyBlendingClient& client, AnimatableProperty property, const RenderStyle& a, const RenderStyle& b)
{
    return WTF::switchOn(property,
        [&] (CSSPropertyID propertyId) {
            if (auto* wrapper = CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(propertyId))
                return wrapper->requiresBlendingForAccumulativeIteration(a, b);
            return false;
        }, [&] (const AtomString& customProperty) {
            auto [from, to] = customPropertyValuesForBlending(client, customProperty, a.getCustomProperty(customProperty), b.getCustomProperty(customProperty));
            if (!from || !to)
                return false;

            if (!std::holds_alternative<CSSCustomPropertyValue::SyntaxValue>(from->value()) || !std::holds_alternative<CSSCustomPropertyValue::SyntaxValue>(to->value()))
                return false;

            auto& fromSyntaxValue = std::get<CSSCustomPropertyValue::SyntaxValue>(from->value());
            auto& toSyntaxValue = std::get<CSSCustomPropertyValue::SyntaxValue>(to->value());

            // FIXME: we need to also ensure we blend for iterationComposite for <transform-list>, <filter-value-list> and <shadow>.
            return WTF::switchOn(fromSyntaxValue, [toSyntaxValue](const Length& fromLength) {
                ASSERT(std::holds_alternative<Length>(toSyntaxValue));
                return lengthsRequireBlendingForAccumulativeIteration(fromLength, std::get<Length>(toSyntaxValue) );
            }, [] (const StyleColor&) {
                return true;
            }, [] (auto&) {
                return false;
            });
        }
    );
}

bool CSSPropertyAnimation::animationOfPropertyIsAccelerated(CSSPropertyID property)
{
    AnimationPropertyWrapperBase* wrapper = CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(property);
    return wrapper ? wrapper->animationIsAccelerated() : false;
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
        return wrapper->canInterpolate(from, to, CompositeOperation::Replace);
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
