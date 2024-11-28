/*
 * Copyright (C) 2007-2023 Apple Inc. All rights reserved.
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

#include "AnimationMalloc.h"
#include "AnimationUtilities.h"
#include "BlockEllipsis.h"
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
#include "CustomPropertyRegistry.h"
#include "Document.h"
#include "FloatConversion.h"
#include "FontCascade.h"
#include "FontSelectionAlgorithm.h"
#include "FontSelectionValueInlines.h"
#include "FontTaggedSettings.h"
#include "GridPositionsResolver.h"
#include "IdentityTransformOperation.h"
#include "LengthPoint.h"
#include "Logging.h"
#include "Matrix3DTransformOperation.h"
#include "MatrixTransformOperation.h"
#include "QuotesData.h"
#include "RenderBox.h"
#include "RenderStyleSetters.h"
#include "SVGRenderStyle.h"
#include "ScopedName.h"
#include "ScrollbarGutter.h"
#include "Settings.h"
#include "StyleCachedImage.h"
#include "StyleCrossfadeImage.h"
#include "StyleFilterImage.h"
#include "StylePropertyShorthand.h"
#include "StyleResolver.h"
#include "StyleTextEdge.h"
#include <algorithm>
#include <memory>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/PointerComparison.h>
#include <wtf/text/TextStream.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {

#if !LOG_DISABLED

static TextStream& operator<<(TextStream& stream, CSSPropertyID property)
{
    return stream << nameLiteral(property);
}

#endif

struct CSSPropertyBlendingContext : BlendingContext {
    const CSSPropertyBlendingClient& client;
    AnimatableCSSProperty property;

    CSSPropertyBlendingContext(double progress, bool isDiscrete, CompositeOperation compositeOperation, const CSSPropertyBlendingClient& client, const AnimatableCSSProperty& property, IterationCompositeOperation iterationCompositeOperation = IterationCompositeOperation::Replace, double currentIteration = 0)
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

static inline std::unique_ptr<ShadowData> blendFunc(const ShadowData* from, const ShadowData* to, const RenderStyle& fromStyle, const RenderStyle& toStyle, const CSSPropertyBlendingContext& context)
{
    ASSERT(from && to);
    ASSERT(from->style() == to->style());

    return makeUnique<ShadowData>(blend(from->location(), to->location(), context),
        blend(from->radius(), to->radius(), context, ValueRange::NonNegative),
        blend(from->spread(), to->spread(), context),
        blendFunc(from->style(), to->style(), context),
        from->isWebkitBoxShadow(),
        blend(fromStyle.colorResolvingCurrentColor(from->color()), toStyle.colorResolvingCurrentColor(to->color()), context));
}

static inline TransformOperations blendFunc(const TransformOperations& from, const TransformOperations& to, const CSSPropertyBlendingContext& context)
{
    if (context.compositeOperation == CompositeOperation::Add) {
        ASSERT(context.progress == 1.0);

        Vector<Ref<TransformOperation>> operations;
        operations.reserveInitialCapacity(from.size() + to.size());

        operations.appendRange(from.begin(), from.end());
        operations.appendRange(to.begin(), to.end());

        return TransformOperations { WTFMove(operations) };
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

    auto* renderBox = dynamicDowncast<RenderBox>(context.client.renderer());
    auto boxSize = renderBox ? renderBox->borderBoxRect().size() : LayoutSize();
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
    if (auto* scale = dynamicDowncast<ScaleTransformOperation>(blendedOperation.get()))
        return ScaleTransformOperation::create(scale->x(), scale->y(), scale->z(), scale->type());
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
    if (auto* rotate = dynamicDowncast<RotateTransformOperation>(blendedOperation.get()))
        return RotateTransformOperation::create(rotate->x(), rotate->y(), rotate->z(), rotate->angle(), rotate->type());
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
    if (auto* translate = dynamicDowncast<TranslateTransformOperation>(blendedOperation.get()))
        return TranslateTransformOperation::create(translate->x(), translate->y(), translate->z(), translate->type());
    return nullptr;
}

static Ref<TransformOperation> blendFunc(TransformOperation& from, TransformOperation& to, const CSSPropertyBlendingContext& context)
{
    return to.blend(&from, context);
}

static inline RefPtr<PathOperation> blendFunc(PathOperation* from, PathOperation* to, const CSSPropertyBlendingContext& context)
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? to : from;
    }

    ASSERT(from && to);
    return from->blend(to, context);
}

static inline RefPtr<ShapeValue> blendFunc(ShapeValue* from, ShapeValue* to, const CSSPropertyBlendingContext& context)
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? to : from;
    }

    ASSERT(from && to);
    return from->blend(*to, context);
}

static inline FilterOperations blendFunc(const FilterOperations& from, const FilterOperations& to, const CSSPropertyBlendingContext& context)
{
    return from.blend(to, context);
}

static inline RefPtr<StyleImage> blendFilter(RefPtr<StyleImage> inputImage, const FilterOperations& from, const FilterOperations& to, const CSSPropertyBlendingContext& context)
{
    auto filterResult = from.blend(to, context);
    return StyleFilterImage::create(WTFMove(inputImage), WTFMove(filterResult));
}

static inline ContentVisibility blendFunc(ContentVisibility from, ContentVisibility to, const CSSPropertyBlendingContext& context)
{
    // https://drafts.csswg.org/css-contain-3/#content-visibility-animation
    // In general, the content-visibility property's animation type is discrete. However, similar to interpolation of
    // visibility, during interpolation between hidden and any other content-visibility value, p values between 0 and 1
    // map to the non-hidden value.
    if (from != ContentVisibility::Hidden && to != ContentVisibility::Hidden)
        return context.progress < 0.5 ? from : to;
    if (context.progress <= 0)
        return from;
    if (context.progress >= 1)
        return to;
    return from == ContentVisibility::Hidden ? to : from;
}

static inline Visibility blendFunc(Visibility from, Visibility to, const CSSPropertyBlendingContext& context)
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? to : from;
    }

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

static inline DisplayType blendFunc(DisplayType from, DisplayType to, const CSSPropertyBlendingContext& context)
{
    // https://drafts.csswg.org/css-display-4/#display-animation
    // In general, the display property's animation type is discrete. However, similar to interpolation of
    // visibility, during interpolation between none and any other display value, p values between 0 and 1
    // map to the non-none value. Additionally, the element is inert as long as its display value would
    // compute to none when ignoring the Transitions and Animations cascade origins.
    if (from != DisplayType::None && to != DisplayType::None)
        return context.progress < 0.5 ? from : to;
    if (context.progress <= 0)
        return from;
    if (context.progress >= 1)
        return to;
    return from == DisplayType::None ? to : from;
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
    if (auto [fromFilter, toFilter] = std::tuple { dynamicDowncast<StyleFilterImage>(*from), dynamicDowncast<StyleFilterImage>(*to) }; fromFilter && toFilter) {
        // Animation of generated images just possible if input images are equal.
        // Otherwise fall back to cross fade animation.
        if (fromFilter->equalInputImages(*toFilter) && is<StyleCachedImage>(fromFilter->inputImage()))
            return blendFilter(fromFilter->inputImage(), fromFilter->filterOperations(), toFilter->filterOperations(), context);
    } else if (auto [fromCrossfade, toCrossfade] = std::tuple { dynamicDowncast<StyleCrossfadeImage>(*from), dynamicDowncast<StyleCrossfadeImage>(*to) }; fromCrossfade && toCrossfade) {
        if (fromCrossfade->equalInputImages(*toCrossfade)) {
            if (auto crossfadeBlend = toCrossfade->blend(*fromCrossfade, context))
                return crossfadeBlend;
        }
    } else if (auto [fromFilter, toCachedImage] = std::tuple { dynamicDowncast<StyleFilterImage>(*from), dynamicDowncast<StyleCachedImage>(*to) }; fromFilter && toCachedImage) {
        RefPtr fromFilterInputImage = dynamicDowncast<StyleCachedImage>(fromFilter->inputImage());

        if (fromFilterInputImage && toCachedImage->equals(*fromFilterInputImage))
            return blendFilter(WTFMove(fromFilterInputImage), fromFilter->filterOperations(), FilterOperations(), context);
    } else if (auto [fromCachedImage, toFilter] = std::tuple { dynamicDowncast<StyleCachedImage>(*from), dynamicDowncast<StyleFilterImage>(*to) }; fromCachedImage && toFilter) {
        RefPtr toFilterInputImage = dynamicDowncast<StyleCachedImage>(toFilter->inputImage());

        if (toFilterInputImage && fromCachedImage->equals(*toFilterInputImage))
            return blendFilter(WTFMove(toFilterInputImage), FilterOperations(), toFilter->filterOperations(), context);
    }

    auto* fromCachedImage = dynamicDowncast<StyleCachedImage>(*from);
    auto* toCachedImage = dynamicDowncast<StyleCachedImage>(*to);
    if (fromCachedImage && toCachedImage)
        return crossfadeBlend(*fromCachedImage, *toCachedImage, context);

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
    if (!from && !to)
        return std::nullopt;

    auto valueOrDefault = [](std::optional<FontSelectionValue> fontSelectionValue) {
        if (!fontSelectionValue)
            return 0.0f;
        return static_cast<float>(fontSelectionValue.value());
    };

    return normalizedFontItalicValue(blendFunc(valueOrDefault(from), valueOrDefault(to), context));
}

static inline bool canInterpolate(const GridTrackList& from, const GridTrackList& to)
{
    if (from.list.size() != to.list.size())
        return false;

    size_t i = 0;
    auto visitor = WTF::makeVisitor([&](const GridTrackSize&) {
        return std::holds_alternative<GridTrackSize>(to.list[i]);
    }, [&](const Vector<String>&) {
        return std::holds_alternative<Vector<String>>(to.list[i]);
    }, [&](const GridTrackEntryRepeat& repeat) {
        if (!std::holds_alternative<GridTrackEntryRepeat>(to.list[i]))
            return false;
        const auto& toEntry = std::get<GridTrackEntryRepeat>(to.list[i]);
        return repeat.repeats == toEntry.repeats && repeat.list.size() == toEntry.list.size();
    }, [](const GridTrackEntryAutoRepeat&) {
        return false;
    }, [](const GridTrackEntrySubgrid&) {
        return false;
    }, [](const GridTrackEntryMasonry&) {
        return false;
    });

    for (i = 0; i < from.list.size(); i++) {
        if (!std::visit(visitor, from.list[i]))
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
        result.list.append(blendFunc(size, std::get<GridTrackSize>(to.list[i]), context));
    }, [&](const Vector<String>& names) {
        if (context.progress < 0.5)
            result.list.append(names);
        else {
            const Vector<String>& toNames = std::get<Vector<String>>(to.list[i]);
            result.list.append(toNames);
        }
    }, [&](const GridTrackEntryRepeat& repeatFrom) {
        auto& repeatTo = std::get<GridTrackEntryRepeat>(to.list[i]);
        GridTrackEntryRepeat repeatResult;
        repeatResult.repeats = repeatFrom.repeats;
        repeatResult.list = blendFunc(repeatFrom.list, repeatTo.list, context);
        result.list.append(WTFMove(repeatResult));
    }, [&](const GridTrackEntryAutoRepeat& repeatFrom) {
        auto& repeatTo = std::get<GridTrackEntryAutoRepeat>(to.list[i]);
        GridTrackEntryAutoRepeat repeatResult;
        repeatResult.type = repeatFrom.type;
        repeatResult.list = blendFunc(repeatFrom.list, repeatTo.list, context);
        result.list.append(WTFMove(repeatResult));
    }, [](const GridTrackEntrySubgrid&) {
    }, [](const GridTrackEntryMasonry&) {
    });


    for (i = 0; i < from.list.size(); i++)
        std::visit(visitor, from.list[i]);

    return result;
}

static inline RefPtr<StylePathData> blendFunc(StylePathData* from, StylePathData* to, const CSSPropertyBlendingContext& context)
{
    if (context.isDiscrete)
        return context.progress < 0.5 ? from : to;
    ASSERT(from && to);
    return from->blend(*to, context);
}

class AnimationPropertyWrapperBase {
    WTF_MAKE_NONCOPYABLE(AnimationPropertyWrapperBase);
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    virtual bool normalizesProgressForDiscreteInterpolation() const { return true; }
    virtual void blend(RenderStyle&, const RenderStyle&, const RenderStyle&, const CSSPropertyBlendingContext&) const = 0;

#if !LOG_DISABLED
    virtual void logBlend(const RenderStyle&, const RenderStyle&, const RenderStyle&, double) const = 0;
#endif

    CSSPropertyID property() const { return m_property; }

    virtual bool animationIsAccelerated(const Settings&) const { return false; }

private:
    CSSPropertyID m_property;
};

template <typename T>
class PropertyWrapperGetter : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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


class OffsetRotateWrapper final : public PropertyWrapperGetter<OffsetRotation> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    OffsetRotateWrapper()
        : PropertyWrapperGetter(CSSPropertyOffsetRotate, &RenderStyle::offsetRotate)
    {
    }

private:
    bool animationIsAccelerated(const Settings& settings) const final
    {
#if ENABLE(THREADED_ANIMATION_RESOLUTION)
        return settings.threadedAnimationResolutionEnabled();
#else
        UNUSED_PARAM(settings);
        return false;
#endif
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return value(from).canBlend(value(to));
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        destination.setOffsetRotate(value(from).blend(value(to), context));
    }
};

template <typename T>
class PositivePropertyWrapper final : public PropertyWrapper<T> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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

class NinePieceImageRepeatWrapper final : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    NinePieceImageRepeatWrapper(CSSPropertyID property, NinePieceImageRule (RenderStyle::*horizontalGetter)() const, void (RenderStyle::*horizontalSetter)(NinePieceImageRule), NinePieceImageRule (RenderStyle::*verticalGetter)() const, void (RenderStyle::*verticalSetter)(NinePieceImageRule))
        : AnimationPropertyWrapperBase(property)
        , m_horizontalWrapper(DiscretePropertyWrapper<NinePieceImageRule>(property, horizontalGetter, horizontalSetter))
        , m_verticalWrapper(DiscretePropertyWrapper<NinePieceImageRule>(property, verticalGetter, verticalSetter))
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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


class LengthPointPropertyWrapper : public PropertyWrapperGetter<const LengthPoint&> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    LengthPointPropertyWrapper(CSSPropertyID property, const LengthPoint& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(LengthPoint))
        : PropertyWrapperGetter(property, getter)
        , m_setter(setter)
    {
    }

private:
    bool requiresBlendingForAccumulativeIteration(const RenderStyle& from, const RenderStyle& to) const final
    {
        auto fromLengthPoint = value(from);
        auto toLengthPoint = value(to);
        return lengthsRequireBlendingForAccumulativeIteration(fromLengthPoint.x, toLengthPoint.x)
            || lengthsRequireBlendingForAccumulativeIteration(fromLengthPoint.y, toLengthPoint.y);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        (destination.*m_setter)(blendFunc(value(from), value(to), context));
    }

    void (RenderStyle::*m_setter)(LengthPoint);
};


// This class extends LengthPointPropertyWrapper to accommodate `auto` or `normal` values expressed as
// LengthPoint(Length(LengthType::Auto/Normal), Length(LengthType::Auto/Normal)). This is used for
// offset-anchor and offset-position, which allows `auto` and `normal`, and is expressed like so.
class LengthPointOrAutoPropertyWrapper : public LengthPointPropertyWrapper {
public:
    LengthPointOrAutoPropertyWrapper(CSSPropertyID property, const LengthPoint& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(LengthPoint))
        : LengthPointPropertyWrapper(property, getter, setter)
    {
    }

private:
    // Check if it's possible to interpolate between the from and to values. In particular,
    // it's only possible if they're both not auto or normal.
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        auto valueFrom = value(from);
        auto valueTo = value(to);

        return !valueFrom.x.isAuto() && !valueTo.x.isAuto() && !valueFrom.x.isNormal() && !valueTo.x.isNormal();
    }
};

class OffsetLengthPointWrapper final : public LengthPointOrAutoPropertyWrapper {
public:
    OffsetLengthPointWrapper(CSSPropertyID property, const LengthPoint& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(LengthPoint))
        : LengthPointOrAutoPropertyWrapper(property, getter, setter)
    {
    }

private:
    bool animationIsAccelerated(const Settings& settings) const final
    {
#if ENABLE(THREADED_ANIMATION_RESOLUTION)
        return settings.threadedAnimationResolutionEnabled();
#else
        UNUSED_PARAM(settings);
        return false;
#endif
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);

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




class ContainIntrinsicLengthPropertyWrapper final : public OptionalLengthPropertyWrapper {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    ContainIntrinsicLengthPropertyWrapper(CSSPropertyID property, std::optional<Length> (RenderStyle::*getter)() const, void (RenderStyle::*setter)(std::optional<Length>), ContainIntrinsicSizeType (RenderStyle::*typeGetter)() const, void (RenderStyle::*typeSetter)(ContainIntrinsicSizeType))
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
        if (m_flags.contains(Flags::UsesFillKeyword)) {
            if (property() == CSSPropertyBorderImageSlice && from.borderImage().fill() != to.borderImage().fill())
                return false;
            if (property() == CSSPropertyMaskBorderSlice && from.maskBorder().fill() != to.maskBorder().fill())
                return false;
        }

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
        if (m_flags.contains(Flags::UsesFillKeyword)) {
            if (property() == CSSPropertyBorderImageSlice)
                destination.setBorderImageSliceFill((!context.progress || !context.isDiscrete ? from : to).borderImage().fill());
            else if (property() == CSSPropertyMaskBorderSlice)
                destination.setMaskBorderSliceFill((!context.progress || !context.isDiscrete ? from : to).maskBorder().fill());
        }
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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



class PathOperationPropertyWrapper : public RefCountedPropertyWrapper<PathOperation> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    PathOperationPropertyWrapper(CSSPropertyID property, PathOperation* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(RefPtr<PathOperation>&&))
        : RefCountedPropertyWrapper(property, getter, setter)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const override
    {
        auto* fromPath = value(from);
        auto* toPath = value(to);
        return fromPath && toPath && fromPath->canBlend(*toPath);
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



class OffsetPathWrapper final : public PathOperationPropertyWrapper {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    OffsetPathWrapper()
        : PathOperationPropertyWrapper(CSSPropertyOffsetPath, &RenderStyle::offsetPath, &RenderStyle::setOffsetPath)
    {
    }

private:
    bool animationIsAccelerated(const Settings& settings) const final
    {
#if ENABLE(THREADED_ANIMATION_RESOLUTION)
        return settings.threadedAnimationResolutionEnabled();
#else
        UNUSED_PARAM(settings);
        return false;
#endif
    }
};


#if ENABLE(VARIATION_FONTS)


class PropertyWrapperFontVariationSettings final : public PropertyWrapper<FontVariationSettings> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    PropertyWrapperFontVariationSettings()
        : PropertyWrapper(CSSPropertyFontVariationSettings, &RenderStyle::fontVariationSettings, &RenderStyle::setFontVariationSettings)
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
        return fromShape && toShape && fromShape->canBlend(*toShape);
    }
};



class StyleImagePropertyWrapper final : public RefCountedPropertyWrapper<StyleImage> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    AcceleratedPropertyWrapper(CSSPropertyID property, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T))
        : PropertyWrapper<T>(property, getter, setter)
    {
    }

private:
    bool animationIsAccelerated(const Settings&) const final { return true; }
    bool requiresBlendingForAccumulativeIteration(const RenderStyle&, const RenderStyle&) const final { return this->property() == CSSPropertyTransform; }
};

class AcceleratedTransformOperationsPropertyWrapper final : public PropertyWrapperGetter<const TransformOperations&> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    AcceleratedTransformOperationsPropertyWrapper()
        : PropertyWrapperGetter<const TransformOperations&>(CSSPropertyTransform, &RenderStyle::transform)
    {
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation compositeOperation) const override
    {
        if (compositeOperation == CompositeOperation::Replace)
            return !this->value(to).shouldFallBackToDiscreteAnimation(this->value(from), { });
        return true;
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const override
    {
        destination.setTransform(blendFunc(this->value(from), this->value(to), context));
    }

private:
    bool animationIsAccelerated(const Settings&) const final { return true; }
    bool requiresBlendingForAccumulativeIteration(const RenderStyle&, const RenderStyle&) const final { return true; }
};

template <typename T>
class AcceleratedIndividualTransformPropertyWrapper final : public RefCountedPropertyWrapper<T> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    AcceleratedIndividualTransformPropertyWrapper(CSSPropertyID property, T* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(RefPtr<T>&&))
        : RefCountedPropertyWrapper<T>(property, getter, setter)
    {
    }

private:
    bool animationIsAccelerated(const Settings&) const final { return true; }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return arePointingToEqualData(this->value(a), this->value(b));
    }
};


class PropertyWrapperFilter final : public PropertyWrapperGetter<const FilterOperations&> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    PropertyWrapperFilter(CSSPropertyID property, const FilterOperations& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(FilterOperations&&))
        : PropertyWrapperGetter<const FilterOperations&>(property, getter)
        , m_setter(setter)
    {
    }

private:
    bool animationIsAccelerated(const Settings&) const final
    {
        return property() == CSSPropertyFilter
            || property() == CSSPropertyBackdropFilter
            || property() == CSSPropertyWebkitBackdropFilter;
    }

    bool requiresBlendingForAccumulativeIteration(const RenderStyle&, const RenderStyle&) const final { return true; }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation compositeOperation) const final
    {
        return value(from).canInterpolate(value(to), compositeOperation);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        (destination.*m_setter)(blendFunc(value(from), value(to), context));
    }

    void (RenderStyle::*m_setter)(FilterOperations&&);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
            (destination.*m_setter)(blendSimpleOrMatchedShadowLists(fromShadow, toShadow, from, to, context), false);
            return;
        }

        (destination.*m_setter)(blendMismatchedShadowLists(fromShadow, toShadow, fromLength, toLength, from, to, context), false);
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

    std::unique_ptr<ShadowData> blendSimpleOrMatchedShadowLists(const ShadowData* shadowA, const ShadowData* shadowB, const RenderStyle& styleA, const RenderStyle& styleB, const CSSPropertyBlendingContext& context) const
    {
        // from or to might be null in which case we don't want to do additivity, but do replace instead.
        if (shadowA && shadowB && context.compositeOperation == CompositeOperation::Add)
            return addShadowLists(shadowA, shadowB);

        std::unique_ptr<ShadowData> newShadowData;
        ShadowData* lastShadow = nullptr;

        while (shadowA || shadowB) {
            const ShadowData* srcShadow = shadowForBlending(shadowA, shadowB);
            const ShadowData* dstShadow = shadowForBlending(shadowB, shadowA);

            std::unique_ptr<ShadowData> blendedShadow = blendFunc(srcShadow, dstShadow, styleA, styleB, context);
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

    std::unique_ptr<ShadowData> blendMismatchedShadowLists(const ShadowData* shadowA, const ShadowData* shadowB, int fromLength, int toLength, const RenderStyle& styleA, const RenderStyle& styleB, const CSSPropertyBlendingContext& context) const
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

            std::unique_ptr<ShadowData> blendedShadow = blendFunc(srcShadow, dstShadow, styleA, styleB, context);
            // Insert at the start of the list to preserve the order.
            blendedShadow->setNext(WTFMove(newShadowData));
            newShadowData = WTFMove(blendedShadow);
        }

        return newShadowData;
    }

    const ShadowData* (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(std::unique_ptr<ShadowData>, bool);
};



class PropertyWrapperStyleColor : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    PropertyWrapperStyleColor(CSSPropertyID property, const StyleColor& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const StyleColor&))
        : AnimationPropertyWrapperBase(property)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const override
    {
        if (&a == &b)
            return true;
        
        auto& fromStyleColor = value(a);
        auto& toStyleColor = value(b);
        
        if (fromStyleColor.isCurrentColor() && toStyleColor.isCurrentColor())
            return true;
        
        if (fromStyleColor.isAbsoluteColor() && toStyleColor.isAbsoluteColor())
            return fromStyleColor.absoluteColor() == toStyleColor.absoluteColor();

        return a.colorResolvingCurrentColor(fromStyleColor) == b.colorResolvingCurrentColor(toStyleColor);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const override
    {
        auto& fromStyleColor = value(from);
        auto& toStyleColor = value(to);

        // We don't animate on currentcolor-only transition.
        // https://github.com/WebKit/WebKit/blob/main/LayoutTests/imported/w3c/web-platform-tests/css/css-transitions/currentcolor-animation-001.html#L27
        if (fromStyleColor.isCurrentColor() && toStyleColor.isCurrentColor())
            return;

        auto fromColor = from.colorResolvingCurrentColor(fromStyleColor);
        auto toColor = to.colorResolvingCurrentColor(toStyleColor);

        auto result = blendFunc(fromColor, toColor, context);
        (destination.*m_setter)(WTFMove(result));
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        // FIXME: better logging.
        LOG_WITH_STREAM(Animations, stream << "  blending " << property() << " from " << value(from) << " to " << value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
    }
#endif
private:
    const StyleColor& value(const RenderStyle& style) const
    {
        return (style.*m_getter)();
    }

    const StyleColor& (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(const StyleColor&);
};



class PropertyWrapperColor : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    PropertyWrapperColor(CSSPropertyID property, const Color& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Color&))
        : AnimationPropertyWrapperBase(property)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

    bool equals(const RenderStyle& a, const RenderStyle& b) const override
    {
        if (&a == &b)
            return true;

        return value(a) == value(b);
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const override
    {
        auto result = blendFunc(value(from), value(to), context);
        (destination.*m_setter)(WTFMove(result));
    }

#if !LOG_DISABLED
    void logBlend(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        // FIXME: better logging.
        LOG_WITH_STREAM(Animations, stream << "  blending " << property() << " from " << value(from) << " to " << value(to) << " at " << TextStream::FormatNumberRespectingIntegers(progress) << " -> " << value(destination));
    }
#endif

private:
    const Color& value(const RenderStyle& style) const
    {
        return (style.*m_getter)();
    }

    const Color& (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(const Color&);
};



class ScrollbarColorPropertyWrapper final : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    ScrollbarColorPropertyWrapper()
        : AnimationPropertyWrapperBase(CSSPropertyScrollbarColor)
        , m_thumbWrapper(makeUnique<PropertyWrapperStyleColor>(CSSPropertyScrollbarColor, &RenderStyle::scrollbarThumbColor, &RenderStyle::setScrollbarThumbColor))
        , m_trackWrapper(makeUnique<PropertyWrapperStyleColor>(CSSPropertyScrollbarColor, &RenderStyle::scrollbarTrackColor, &RenderStyle::setScrollbarTrackColor))
    {
    }

private:
    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        bool aAuto = !a.scrollbarColor().has_value();
        bool bAuto = !b.scrollbarColor().has_value();

        if (aAuto || bAuto)
            return aAuto == bAuto;

        return m_thumbWrapper->equals(a, b) && m_trackWrapper->equals(a, b);
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return from.scrollbarColor().has_value() && to.scrollbarColor().has_value();
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        if (canInterpolate(from, to, context.compositeOperation)) {
            destination.setScrollbarColor(from.scrollbarColor().value());
            m_thumbWrapper->blend(destination, from, to, context);
            m_trackWrapper->blend(destination, from, to, context);
            return;
        }

        ASSERT(!context.progress || context.progress == 1.0);
        auto& blendingRenderStyle = context.progress ? to : from;
        destination.setScrollbarColor(blendingRenderStyle.scrollbarColor());
    }

    std::unique_ptr<PropertyWrapperStyleColor> m_thumbWrapper;
    std::unique_ptr<PropertyWrapperStyleColor> m_trackWrapper;

#if !LOG_DISABLED
    void logBlend(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        m_thumbWrapper->logBlend(from, to, destination, progress);
        m_trackWrapper->logBlend(from, to, destination, progress);
    }
#endif
};




class PropertyWrapperVisitedAffectedStyleColor : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    PropertyWrapperVisitedAffectedStyleColor(CSSPropertyID property, const StyleColor& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const StyleColor&), const StyleColor& (RenderStyle::*visitedGetter)() const, void (RenderStyle::*visitedSetter)(const StyleColor&))
        : AnimationPropertyWrapperBase(property)
        , m_wrapper(makeUnique<PropertyWrapperStyleColor>(property, getter, setter))
        , m_visitedWrapper(makeUnique<PropertyWrapperStyleColor>(property, visitedGetter, visitedSetter))
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

    std::unique_ptr<PropertyWrapperStyleColor> m_wrapper;
    std::unique_ptr<PropertyWrapperStyleColor> m_visitedWrapper;

private:
#if !LOG_DISABLED
    void logBlend(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        m_wrapper->logBlend(from, to, destination, progress);
        m_visitedWrapper->logBlend(from, to, destination, progress);
    }
#endif
};



class PropertyWrapperVisitedAffectedColor : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    PropertyWrapperVisitedAffectedColor(CSSPropertyID property, const Color& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Color&), const Color& (RenderStyle::*visitedGetter)() const, void (RenderStyle::*visitedSetter)(const Color&))
        : AnimationPropertyWrapperBase(property)
        , m_wrapper(makeUnique<PropertyWrapperColor>(property, getter, setter))
        , m_visitedWrapper(makeUnique<PropertyWrapperColor>(property, visitedGetter, visitedSetter))
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

    std::unique_ptr<PropertyWrapperColor> m_wrapper;
    std::unique_ptr<PropertyWrapperColor> m_visitedWrapper;

private:
#if !LOG_DISABLED
    void logBlend(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        m_wrapper->logBlend(from, to, destination, progress);
        m_visitedWrapper->logBlend(from, to, destination, progress);
    }
#endif
};



class AccentColorPropertyWrapper final : public PropertyWrapperStyleColor {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    AccentColorPropertyWrapper()
        : PropertyWrapperStyleColor(CSSPropertyAccentColor, &RenderStyle::accentColor, &RenderStyle::setAccentColor)
    {
    }

private:
    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return a.hasAutoAccentColor() == b.hasAutoAccentColor()
            && PropertyWrapperStyleColor::equals(a, b);
    }

    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return !from.hasAutoAccentColor() && !to.hasAutoAccentColor();
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        if (canInterpolate(from, to, context.compositeOperation)) {
            PropertyWrapperStyleColor::blend(destination, from, to, context);
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


class CaretColorPropertyWrapper final : public PropertyWrapperVisitedAffectedStyleColor {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    CaretColorPropertyWrapper()
        : PropertyWrapperVisitedAffectedStyleColor(CSSPropertyCaretColor, &RenderStyle::caretColor, &RenderStyle::setCaretColor, &RenderStyle::visitedLinkCaretColor, &RenderStyle::setVisitedLinkCaretColor)
    {
    }

private:
    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return a.hasAutoCaretColor() == b.hasAutoCaretColor()
            && a.hasVisitedLinkAutoCaretColor() == b.hasVisitedLinkAutoCaretColor()
            && PropertyWrapperVisitedAffectedStyleColor::equals(a, b);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
        if (property() == CSSPropertyMaskImage)
            return false;
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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

        if (context.isDiscrete) {
            ASSERT(!context.progress || context.progress == 1.0);
            auto* layer = context.progress ? toLayer : fromLayer;
            fromLayer = layer;
            toLayer = layer;
        }

        size_t layerCount = 0;
        Vector<FillLayer*> previousDstLayers;
        FillLayer* previousDstLayer = nullptr;
        while (fromLayer && toLayer) {
            if (dstLayer)
                previousDstLayers.append(dstLayer);
            else {
                ASSERT(!previousDstLayers.isEmpty());
                auto* layerToCopy = previousDstLayers[layerCount % previousDstLayers.size()];
                previousDstLayer->setNext(layerToCopy->copy());
                dstLayer = previousDstLayer->next();
            }

            dstLayer->setSizeType((context.progress ? toLayer : fromLayer)->sizeType());
            m_fillLayerPropertyWrapper->blend(dstLayer, fromLayer, toLayer, context);
            fromLayer = fromLayer->next();
            toLayer = toLayer->next();

            previousDstLayer = dstLayer;
            dstLayer = dstLayer->next();
            layerCount++;
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    PropertyWrapperSVGPaint(CSSPropertyID property, SVGPaintType (RenderStyle::*paintTypeGetter)() const, const StyleColor& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const StyleColor&))
        : AnimationPropertyWrapperBase(property)
        , m_paintTypeGetter(paintTypeGetter)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

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
        if (fromStyleColor.isCurrentColor() && toStyleColor.isCurrentColor())
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

private:
    SVGPaintType (RenderStyle::*m_paintTypeGetter)() const;
    const StyleColor& (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(const StyleColor&);
};



class PropertyWrapperVisitedAffectedSVGPaint : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    PropertyWrapperVisitedAffectedSVGPaint(CSSPropertyID property, SVGPaintType (RenderStyle::*paintTypeGetter)() const, const StyleColor& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const StyleColor&), SVGPaintType (RenderStyle::*visitedPaintTypeGetter)() const, const StyleColor& (RenderStyle::*visitedGetter)() const, void (RenderStyle::*visitedSetter)(const StyleColor&))
        : AnimationPropertyWrapperBase(property)
        , m_wrapper(makeUnique<PropertyWrapperSVGPaint>(property, paintTypeGetter, getter, setter))
        , m_visitedWrapper(makeUnique<PropertyWrapperSVGPaint>(property, visitedPaintTypeGetter, visitedGetter, visitedSetter))
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

    std::unique_ptr<PropertyWrapperSVGPaint> m_wrapper;
    std::unique_ptr<PropertyWrapperSVGPaint> m_visitedWrapper;

private:
#if !LOG_DISABLED
    void logBlend(const RenderStyle& from, const RenderStyle& to, const RenderStyle& destination, double progress) const final
    {
        m_wrapper->logBlend(from, to, destination, progress);
        m_visitedWrapper->logBlend(from, to, destination, progress);
    }
#endif
};



class PropertyWrapperFontWeight final : public PropertyWrapper<FontSelectionValue> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    PropertyWrapperFontStyle()
        : PropertyWrapper(CSSPropertyFontStyle, &RenderStyle::fontItalic, &RenderStyle::setFontItalic)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        return from.fontDescription().fontStyleAxis() == FontStyleAxis::slnt && to.fontDescription().fontStyleAxis() == FontStyleAxis::slnt;
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



class PropertyWrapperFontSizeAdjust final : public PropertyWrapperGetter<FontSizeAdjust> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    PropertyWrapperFontSizeAdjust()
        : PropertyWrapperGetter(CSSPropertyFontSizeAdjust, &RenderStyle::fontSizeAdjust)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        auto fromFontSizeAdjust = from.fontSizeAdjust();
        auto toFontSizeAdjust = to.fontSizeAdjust();
        return fromFontSizeAdjust.metric == toFontSizeAdjust.metric
            && fromFontSizeAdjust.value && toFontSizeAdjust.value;
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        auto blendedFontSizeAdjust = [&]() -> FontSizeAdjust {
            if (context.isDiscrete)
                return (!context.progress ? from : to).fontSizeAdjust();

            ASSERT(from.fontSizeAdjust().value && to.fontSizeAdjust().value);
            auto blendedAdjust = blendFunc(*from.fontSizeAdjust().value, *to.fontSizeAdjust().value, context);

            ASSERT(from.fontSizeAdjust().metric == to.fontSizeAdjust().metric);
            return { to.fontSizeAdjust().metric, FontSizeAdjust::ValueType::Number, std::max(blendedAdjust, 0.0f) };
        };

        destination.setFontSizeAdjust(blendedFontSizeAdjust());
    }
};



class PropertyWrapperBaselineShift final : public PropertyWrapper<SVGLengthValue> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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


class PropertyWrapperTextUnderlineOffset final : public PropertyWrapperGetter<TextUnderlineOffset> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    PropertyWrapperTextUnderlineOffset()
        : PropertyWrapperGetter(CSSPropertyTextUnderlineOffset, &RenderStyle::textUnderlineOffset)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        auto fromTextUnderlineOffset = from.textUnderlineOffset();
        auto toTextUnderlineOffset = to.textUnderlineOffset();
        if (fromTextUnderlineOffset.isAuto() || toTextUnderlineOffset.isAuto())
            return false;

        auto fromValue = fromTextUnderlineOffset.resolve(from.computedFontSize());
        auto toValue = toTextUnderlineOffset.resolve(to.computedFontSize());
        return fromValue != toValue;
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        auto blendedTextUnderlineOffset = [&]() -> TextUnderlineOffset {
            if (context.isDiscrete)
                return (!context.progress ? from : to).textUnderlineOffset();

            auto fromTextUnderlineOffset = from.textUnderlineOffset();
            auto toTextUnderlineOffset = to.textUnderlineOffset();

            auto fromValue = fromTextUnderlineOffset.resolve(from.computedFontSize());
            auto toValue = toTextUnderlineOffset.resolve(to.computedFontSize());

            auto blendedValue = blendFunc(fromValue, toValue, context);
            return TextUnderlineOffset::createWithLength(Length(clampTo<float>(blendedValue, minValueForCssLength, maxValueForCssLength), LengthType::Fixed));
        };

        destination.setTextUnderlineOffset(blendedTextUnderlineOffset());
    }
};


class PropertyWrapperTextDecorationThickness final : public PropertyWrapperGetter<TextDecorationThickness> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    PropertyWrapperTextDecorationThickness()
        : PropertyWrapperGetter(CSSPropertyTextDecorationThickness, &RenderStyle::textDecorationThickness)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        auto fromTextDecorationThickness = from.textDecorationThickness();
        auto toTextDecorationThickness = to.textDecorationThickness();
        if (fromTextDecorationThickness.isAuto() || toTextDecorationThickness.isAuto())
            return false;

        auto fromValue = fromTextDecorationThickness.resolve(from.computedFontSize(), from.metricsOfPrimaryFont());
        auto toValue = toTextDecorationThickness.resolve(to.computedFontSize(), to.metricsOfPrimaryFont());
        return fromValue != toValue;
    }

    void blend(RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, const CSSPropertyBlendingContext& context) const final
    {
        auto blendedTextDecorationThickness = [&]() -> TextDecorationThickness {
            if (context.isDiscrete)
                return (!context.progress ? from : to).textDecorationThickness();

            auto fromTextDecorationThickness = from.textDecorationThickness();
            auto toTextDecorationThickness = to.textDecorationThickness();

            auto fromValue = fromTextDecorationThickness.resolve(from.computedFontSize(), from.metricsOfPrimaryFont());
            auto toValue = toTextDecorationThickness.resolve(to.computedFontSize(), to.metricsOfPrimaryFont());

            auto blendedValue = blendFunc(fromValue, toValue, context);
            return TextDecorationThickness::createWithLength(Length(clampTo<float>(blendedValue, minValueForCssLength, maxValueForCssLength), LengthType::Fixed));
        };

        destination.setTextDecorationThickness(blendedTextDecorationThickness());
    }
};


template <typename T>
class AutoPropertyWrapper final : public PropertyWrapper<T> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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



class LineHeightWrapper final : public LengthPropertyWrapper {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    LineHeightWrapper()
        : LengthPropertyWrapper(CSSPropertyLineHeight, &RenderStyle::specifiedLineHeight, &RenderStyle::setLineHeight)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation compositeOperation) const final
    {
        // We must account for how BuilderConverter::convertLineHeight() deals with line-height values:
        // - "normal" is converted to LengthType::Percent with a -100 value
        // - <number> values are converted to LengthType::Percent
        // - <length-percentage> values are converted to LengthType::Fixed
        // This means that animating between "normal" and a "<number>" would work with LengthPropertyWrapper::canInterpolate()
        // since it would see two LengthType::Percent values. So if either value is "normal" we cannot interpolate since those
        // values are either equal or of incompatible types.
        auto normalLineHeight = RenderStyle::initialLineHeight();
        if (value(from) == normalLineHeight || value(to) == normalLineHeight)
            return false;

        // The default logic will now apply since <number> and <length-percentage> values
        // are converted to different LengthType values.
        return LengthPropertyWrapper::canInterpolate(from, to, compositeOperation);
    }
};



class VerticalAlignWrapper final : public LengthPropertyWrapper {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    TextIndentWrapper()
        : LengthPropertyWrapper(CSSPropertyTextIndent, &RenderStyle::textIndent, &RenderStyle::setTextIndent, LengthPropertyWrapper::Flags::IsLengthPercentage)
    {
    }

private:
    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        if (a.textIndentLine() != b.textIndentLine())
            return false;
        if (a.textIndentType() != b.textIndentType())
            return false;
        return LengthPropertyWrapper::equals(a, b);
    }

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



class OffsetDistanceWrapper final : public LengthPropertyWrapper {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    OffsetDistanceWrapper()
        : LengthPropertyWrapper(CSSPropertyOffsetDistance, &RenderStyle::offsetDistance, &RenderStyle::setOffsetDistance, LengthPropertyWrapper::Flags::IsLengthPercentage)
    {
    }

private:
    bool animationIsAccelerated(const Settings& settings) const final
    {
#if ENABLE(THREADED_ANIMATION_RESOLUTION)
        return settings.threadedAnimationResolutionEnabled();
#else
        UNUSED_PARAM(settings);
        return false;
#endif
    }
};



class PerspectiveWrapper final : public FloatPropertyWrapper {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    CounterWrapper(CSSPropertyID property)
        : AnimationPropertyWrapperBase(property)
    {
        ASSERT(property == CSSPropertyCounterIncrement || property == CSSPropertyCounterReset || property == CSSPropertyCounterSet);
    }

    bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const override { return false; }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        auto& mapA = a.counterDirectives().map;
        auto& mapB = b.counterDirectives().map;
        if (mapA.size() != mapB.size())
            return false;
        for (auto& [key, aDirective] : mapA) {
            auto it = mapB.find(key);
            if (it == mapB.end())
                return false;
            auto& bDirective = it->value;
            if ((property() == CSSPropertyCounterIncrement && aDirective.incrementValue != bDirective.incrementValue)
                || (property() == CSSPropertyCounterReset && aDirective.resetValue != bDirective.resetValue)
                || (property() == CSSPropertyCounterSet && aDirective.setValue != bDirective.setValue))
                return false;
        }
        return true;
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
        for (auto& [key, directive] : destination.accessCounterDirectives().map) {
            if (property() == CSSPropertyCounterIncrement)
                directive.incrementValue = std::nullopt;
            else if (property() == CSSPropertyCounterReset)
                directive.resetValue = std::nullopt;
            else
                directive.setValue = std::nullopt;
        }

        auto& style = context.progress ? to : from;
        auto& targetDirectives = destination.accessCounterDirectives().map;
        for (auto& [key, directive] : style.counterDirectives().map) {
            auto updateDirective = [&](CounterDirectives& target, const CounterDirectives& source) {
                if (property() == CSSPropertyCounterIncrement)
                    target.incrementValue = source.incrementValue;
                else if (property() == CSSPropertyCounterReset)
                    target.resetValue = source.resetValue;
                else
                    target.setValue = source.setValue;
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    GridTemplateAreasWrapper()
        : AnimationPropertyWrapperBase(CSSPropertyGridTemplateAreas)
    {
    }

    bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const override { return false; }

    bool equals(const RenderStyle& a, const RenderStyle& b) const final
    {
        return a.implicitNamedGridColumnLines().map == b.implicitNamedGridColumnLines().map
            && a.implicitNamedGridRowLines().map == b.implicitNamedGridRowLines().map
            && a.namedGridArea().map == b.namedGridArea().map
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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



class VisibilityWrapper final : public PropertyWrapper<Visibility> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    VisibilityWrapper()
        : PropertyWrapper(CSSPropertyVisibility, &RenderStyle::visibility, &RenderStyle::setVisibility)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        // https://drafts.csswg.org/web-animations-1/#animating-visibility
        // If neither value is visible, then discrete animation is used.
        return value(from) == Visibility::Visible || value(to) == Visibility::Visible;
    }
};


template <typename T>
class DiscreteSVGPropertyWrapper final : public AnimationPropertyWrapperBase {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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


class DWrapper final : public RefCountedPropertyWrapper<StylePathData> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    DWrapper()
        : RefCountedPropertyWrapper(CSSPropertyD, &RenderStyle::d, &RenderStyle::setD)
    {
    }

private:
    bool canInterpolate(const RenderStyle& from, const RenderStyle& to, CompositeOperation) const final
    {
        auto* fromValue = value(from);
        auto* toValue = value(to);
        return fromValue && toValue && fromValue->canBlend(*toValue);
    }
};



class CSSPropertyAnimationWrapperMap final {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
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



template <typename T>
class NonNormalizedDiscretePropertyWrapper final : public PropertyWrapper<T> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Animation);
public:
    NonNormalizedDiscretePropertyWrapper(CSSPropertyID property, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T))
        : PropertyWrapper<T>(property, getter, setter)
    {
    }

private:
    bool canInterpolate(const RenderStyle&, const RenderStyle&, CompositeOperation) const final { return false; }
    bool normalizesProgressForDiscreteInterpolation() const final { return false; }
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

        new ScrollbarColorPropertyWrapper,

        new PropertyWrapperVisitedAffectedColor(CSSPropertyColor, &RenderStyle::color, &RenderStyle::setColor, &RenderStyle::visitedLinkColor, &RenderStyle::setVisitedLinkColor),

        new PropertyWrapperVisitedAffectedStyleColor(CSSPropertyBackgroundColor, &RenderStyle::backgroundColor, &RenderStyle::setBackgroundColor, &RenderStyle::visitedLinkBackgroundColor, &RenderStyle::setVisitedLinkBackgroundColor),

        new FillLayersPropertyWrapper(CSSPropertyBackgroundImage, &RenderStyle::backgroundLayers, &RenderStyle::ensureBackgroundLayers),
        new StyleImagePropertyWrapper(CSSPropertyListStyleImage, &RenderStyle::listStyleImage, &RenderStyle::setListStyleImage),
        new FillLayersPropertyWrapper(CSSPropertyMaskImage, &RenderStyle::maskLayers, &RenderStyle::ensureMaskLayers),

        new StyleImagePropertyWrapper(CSSPropertyBorderImageSource, &RenderStyle::borderImageSource, &RenderStyle::setBorderImageSource),
        new LengthBoxPropertyWrapper(CSSPropertyBorderImageSlice, &RenderStyle::borderImageSlices, &RenderStyle::setBorderImageSlices, { LengthBoxPropertyWrapper::Flags::UsesFillKeyword }),
        new LengthBoxPropertyWrapper(CSSPropertyBorderImageWidth, &RenderStyle::borderImageWidth, &RenderStyle::setBorderImageWidth, { LengthBoxPropertyWrapper::Flags::IsLengthPercentage, LengthBoxPropertyWrapper::Flags::MayOverrideBorderWidths }),
        new LengthBoxPropertyWrapper(CSSPropertyBorderImageOutset, &RenderStyle::borderImageOutset, &RenderStyle::setBorderImageOutset),
        new NinePieceImageRepeatWrapper(CSSPropertyBorderImageRepeat, &RenderStyle::borderImageHorizontalRule, &RenderStyle::setBorderImageHorizontalRule, &RenderStyle::borderImageVerticalRule, &RenderStyle::setBorderImageVerticalRule),

        new StyleImagePropertyWrapper(CSSPropertyMaskBorderSource, &RenderStyle::maskBorderSource, &RenderStyle::setMaskBorderSource),
        new LengthBoxPropertyWrapper(CSSPropertyMaskBorderSlice, &RenderStyle::maskBorderSlices, &RenderStyle::setMaskBorderSlices, { LengthBoxPropertyWrapper::Flags::UsesFillKeyword }),
        new LengthBoxPropertyWrapper(CSSPropertyMaskBorderWidth, &RenderStyle::maskBorderWidth, &RenderStyle::setMaskBorderWidth, { LengthBoxPropertyWrapper::Flags::IsLengthPercentage }),
        new LengthBoxPropertyWrapper(CSSPropertyMaskBorderOutset, &RenderStyle::maskBorderOutset, &RenderStyle::setMaskBorderOutset),
        new NinePieceImageRepeatWrapper(CSSPropertyMaskBorderRepeat, &RenderStyle::maskBorderHorizontalRule, &RenderStyle::setMaskBorderHorizontalRule, &RenderStyle::maskBorderVerticalRule, &RenderStyle::setMaskBorderVerticalRule),
        new PropertyWrapper<const NinePieceImage&>(CSSPropertyWebkitMaskBoxImage, &RenderStyle::maskBorder, &RenderStyle::setMaskBorder),

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
        new LineHeightWrapper,
        new PropertyWrapper<float>(CSSPropertyOutlineOffset, &RenderStyle::outlineOffset, &RenderStyle::setOutlineOffset),
        new FloatPropertyWrapper(CSSPropertyOutlineWidth, &RenderStyle::outlineWidth, &RenderStyle::setOutlineWidth, FloatPropertyWrapper::ValueRange::NonNegative),
        new LengthPropertyWrapper(CSSPropertyLetterSpacing, &RenderStyle::computedLetterSpacing, &RenderStyle::setLetterSpacing, LengthPropertyWrapper::Flags::IsLengthPercentage),
        new LengthPropertyWrapper(CSSPropertyWordSpacing, &RenderStyle::computedWordSpacing, &RenderStyle::setWordSpacing, LengthPropertyWrapper::Flags::IsLengthPercentage),
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
        new VisibilityWrapper,
        new NonNormalizedDiscretePropertyWrapper<DisplayType>(CSSPropertyDisplay, &RenderStyle::display, &RenderStyle::setDisplay),

        new ClipWrapper,

        new AcceleratedPropertyWrapper<float>(CSSPropertyOpacity, &RenderStyle::opacity, &RenderStyle::setOpacity),
        new AcceleratedTransformOperationsPropertyWrapper,
        new AcceleratedIndividualTransformPropertyWrapper<ScaleTransformOperation>(CSSPropertyScale, &RenderStyle::scale, &RenderStyle::setScale),
        new AcceleratedIndividualTransformPropertyWrapper<RotateTransformOperation>(CSSPropertyRotate, &RenderStyle::rotate, &RenderStyle::setRotate),
        new AcceleratedIndividualTransformPropertyWrapper<TranslateTransformOperation>(CSSPropertyTranslate, &RenderStyle::translate, &RenderStyle::setTranslate),

        new PropertyWrapperFilter(CSSPropertyFilter, &RenderStyle::filter, &RenderStyle::setFilter),
        new PropertyWrapperFilter(CSSPropertyBackdropFilter, &RenderStyle::backdropFilter, &RenderStyle::setBackdropFilter),
        new PropertyWrapperFilter(CSSPropertyWebkitBackdropFilter, &RenderStyle::backdropFilter, &RenderStyle::setBackdropFilter),
        new PropertyWrapperFilter(CSSPropertyAppleColorFilter, &RenderStyle::appleColorFilter, &RenderStyle::setAppleColorFilter),

        new PathOperationPropertyWrapper(CSSPropertyClipPath, &RenderStyle::clipPath, &RenderStyle::setClipPath),

        new PropertyWrapperShape(CSSPropertyShapeOutside, &RenderStyle::shapeOutside, &RenderStyle::setShapeOutside),
        new LengthPropertyWrapper(CSSPropertyShapeMargin, &RenderStyle::shapeMargin, &RenderStyle::setShapeMargin, { LengthPropertyWrapper::Flags::IsLengthPercentage, LengthPropertyWrapper::Flags::NegativeLengthsAreInvalid }),
        new PropertyWrapper<float>(CSSPropertyShapeImageThreshold, &RenderStyle::shapeImageThreshold, &RenderStyle::setShapeImageThreshold),

        new PropertyWrapperVisitedAffectedStyleColor(CSSPropertyColumnRuleColor, &RenderStyle::columnRuleColor, &RenderStyle::setColumnRuleColor, &RenderStyle::visitedLinkColumnRuleColor, &RenderStyle::setVisitedLinkColumnRuleColor),
        new PropertyWrapperVisitedAffectedStyleColor(CSSPropertyWebkitTextStrokeColor, &RenderStyle::textStrokeColor, &RenderStyle::setTextStrokeColor, &RenderStyle::visitedLinkTextStrokeColor, &RenderStyle::setVisitedLinkTextStrokeColor),
        new PropertyWrapperVisitedAffectedStyleColor(CSSPropertyWebkitTextFillColor, &RenderStyle::textFillColor, &RenderStyle::setTextFillColor, &RenderStyle::visitedLinkTextFillColor, &RenderStyle::setVisitedLinkTextFillColor),
        new PropertyWrapperVisitedAffectedStyleColor(CSSPropertyBorderLeftColor, &RenderStyle::borderLeftColor, &RenderStyle::setBorderLeftColor, &RenderStyle::visitedLinkBorderLeftColor, &RenderStyle::setVisitedLinkBorderLeftColor),
        new PropertyWrapperVisitedAffectedStyleColor(CSSPropertyBorderRightColor, &RenderStyle::borderRightColor, &RenderStyle::setBorderRightColor, &RenderStyle::visitedLinkBorderRightColor, &RenderStyle::setVisitedLinkBorderRightColor),
        new PropertyWrapperVisitedAffectedStyleColor(CSSPropertyBorderTopColor, &RenderStyle::borderTopColor, &RenderStyle::setBorderTopColor, &RenderStyle::visitedLinkBorderTopColor, &RenderStyle::setVisitedLinkBorderTopColor),
        new PropertyWrapperVisitedAffectedStyleColor(CSSPropertyBorderBottomColor, &RenderStyle::borderBottomColor, &RenderStyle::setBorderBottomColor, &RenderStyle::visitedLinkBorderBottomColor, &RenderStyle::setVisitedLinkBorderBottomColor),
        new PropertyWrapperVisitedAffectedStyleColor(CSSPropertyOutlineColor, &RenderStyle::outlineColor, &RenderStyle::setOutlineColor, &RenderStyle::visitedLinkOutlineColor, &RenderStyle::setVisitedLinkOutlineColor),

        new PropertyWrapperShadow(CSSPropertyBoxShadow, &RenderStyle::boxShadow, &RenderStyle::setBoxShadow),
        new PropertyWrapperShadow(CSSPropertyWebkitBoxShadow, &RenderStyle::boxShadow, &RenderStyle::setBoxShadow),
        new PropertyWrapperShadow(CSSPropertyTextShadow, &RenderStyle::textShadow, &RenderStyle::setTextShadow),

        new PropertyWrapperVisitedAffectedSVGPaint(CSSPropertyFill, &RenderStyle::fillPaintType, &RenderStyle::fillPaintColor, &RenderStyle::setFillPaintColor, &RenderStyle::visitedFillPaintType, &RenderStyle::visitedFillPaintColor, &RenderStyle::setVisitedFillPaintColor),
        new PropertyWrapper<float>(CSSPropertyFillOpacity, &RenderStyle::fillOpacity, &RenderStyle::setFillOpacity),

        new PropertyWrapperVisitedAffectedSVGPaint(CSSPropertyStroke, &RenderStyle::strokePaintType, &RenderStyle::strokePaintColor, &RenderStyle::setStrokePaintColor, &RenderStyle::visitedStrokePaintType, &RenderStyle::visitedStrokePaintColor, &RenderStyle::setVisitedStrokePaintColor),
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

        new DWrapper,

        new PropertyWrapper<float>(CSSPropertyFloodOpacity, &RenderStyle::floodOpacity, &RenderStyle::setFloodOpacity),
        new PropertyWrapperStyleColor(CSSPropertyFloodColor, &RenderStyle::floodColor, &RenderStyle::setFloodColor),

        new PropertyWrapper<float>(CSSPropertyStopOpacity, &RenderStyle::stopOpacity, &RenderStyle::setStopOpacity),
        new PropertyWrapperStyleColor(CSSPropertyStopColor, &RenderStyle::stopColor, &RenderStyle::setStopColor),

        new PropertyWrapperStyleColor(CSSPropertyLightingColor, &RenderStyle::lightingColor, &RenderStyle::setLightingColor),
        new PropertyWrapperStyleColor(CSSPropertyStrokeColor, &RenderStyle::strokeColor, &RenderStyle::setStrokeColor),

        new PropertyWrapperBaselineShift,
#if ENABLE(VARIATION_FONTS)
        new DiscretePropertyWrapper<FontOpticalSizing>(CSSPropertyFontOpticalSizing, &RenderStyle::fontOpticalSizing, &RenderStyle::setFontOpticalSizing),
        new PropertyWrapperFontVariationSettings,
#endif
        new PropertyWrapperFontSizeAdjust,
        new PropertyWrapperFontWeight,
        new PropertyWrapper<FontSelectionValue>(CSSPropertyFontStretch, &RenderStyle::fontStretch, &RenderStyle::setFontStretch),
        new PropertyWrapperFontStyle,
        new PropertyWrapperTextDecorationThickness,
        new PropertyWrapperTextUnderlineOffset,
        new PropertyWrapperVisitedAffectedStyleColor(CSSPropertyTextDecorationColor, &RenderStyle::textDecorationColor, &RenderStyle::setTextDecorationColor, &RenderStyle::visitedLinkTextDecorationColor, &RenderStyle::setVisitedLinkTextDecorationColor),

        new LengthPropertyWrapper(CSSPropertyFlexBasis, &RenderStyle::flexBasis, &RenderStyle::setFlexBasis, { LengthPropertyWrapper::Flags::IsLengthPercentage, LengthPropertyWrapper::Flags::NegativeLengthsAreInvalid }),
        new FloatPropertyWrapper(CSSPropertyFlexGrow, &RenderStyle::flexGrow, &RenderStyle::setFlexGrow, FloatPropertyWrapper::ValueRange::NonNegative),
        new FloatPropertyWrapper(CSSPropertyFlexShrink, &RenderStyle::flexShrink, &RenderStyle::setFlexShrink, FloatPropertyWrapper::ValueRange::NonNegative),
        new PropertyWrapper<int>(CSSPropertyOrder, &RenderStyle::order, &RenderStyle::setOrder),

        new TabSizePropertyWrapper,

        new DiscretePropertyWrapper<BlockStepInsert>(CSSPropertyBlockStepInsert, &RenderStyle::blockStepInsert, &RenderStyle::setBlockStepInsert),
        new OptionalLengthPropertyWrapper(CSSPropertyBlockStepSize, &RenderStyle::blockStepSize, &RenderStyle::setBlockStepSize, { OptionalLengthPropertyWrapper::Flags::NegativeLengthsAreInvalid }),

        new ContainIntrinsicLengthPropertyWrapper(CSSPropertyContainIntrinsicWidth, &RenderStyle::containIntrinsicWidth, &RenderStyle::setContainIntrinsicWidth, &RenderStyle::containIntrinsicWidthType, &RenderStyle::setContainIntrinsicWidthType),
        new ContainIntrinsicLengthPropertyWrapper(CSSPropertyContainIntrinsicHeight, &RenderStyle::containIntrinsicHeight, &RenderStyle::setContainIntrinsicHeight, &RenderStyle::containIntrinsicHeightType, &RenderStyle::setContainIntrinsicHeightType),

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
        new DiscretePropertyWrapper<BorderStyle>(CSSPropertyBorderLeftStyle, &RenderStyle::borderLeftStyle, &RenderStyle::setBorderLeftStyle),
        new DiscretePropertyWrapper<BorderStyle>(CSSPropertyBorderRightStyle, &RenderStyle::borderRightStyle, &RenderStyle::setBorderRightStyle),
        new DiscretePropertyWrapper<BorderStyle>(CSSPropertyBorderTopStyle, &RenderStyle::borderTopStyle, &RenderStyle::setBorderTopStyle),
        new DiscretePropertyWrapper<BoxSizing>(CSSPropertyBoxSizing, &RenderStyle::boxSizing, &RenderStyle::setBoxSizing),
        new DiscretePropertyWrapper<CaptionSide>(CSSPropertyCaptionSide, &RenderStyle::captionSide, &RenderStyle::setCaptionSide),
        new DiscretePropertyWrapper<Clear>(CSSPropertyClear, &RenderStyle::clear, &RenderStyle::setClear),
        new DiscretePropertyWrapper<TextEdge>(CSSPropertyTextBoxEdge, &RenderStyle::textBoxEdge, &RenderStyle::setTextBoxEdge),
        new DiscretePropertyWrapper<TextEdge>(CSSPropertyLineFitEdge, &RenderStyle::lineFitEdge, &RenderStyle::setLineFitEdge),
        new DiscretePropertyWrapper<TextBoxTrim>(CSSPropertyTextBoxTrim, &RenderStyle::textBoxTrim, &RenderStyle::setTextBoxTrim),
        new DiscretePropertyWrapper<PrintColorAdjust>(CSSPropertyPrintColorAdjust, &RenderStyle::printColorAdjust, &RenderStyle::setPrintColorAdjust),
        new DiscretePropertyWrapper<ColumnFill>(CSSPropertyColumnFill, &RenderStyle::columnFill, &RenderStyle::setColumnFill),
        new DiscretePropertyWrapper<ColumnSpan>(CSSPropertyColumnSpan, &RenderStyle::columnSpan, &RenderStyle::setColumnSpan),
        new DiscretePropertyWrapper<BorderStyle>(CSSPropertyColumnRuleStyle, &RenderStyle::columnRuleStyle, &RenderStyle::setColumnRuleStyle),
        new NonNormalizedDiscretePropertyWrapper<ContentVisibility>(CSSPropertyContentVisibility, &RenderStyle::contentVisibility, &RenderStyle::setContentVisibility),
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
        new DiscretePropertyWrapper<OptionSet<HangingPunctuation>>(CSSPropertyHangingPunctuation, &RenderStyle::hangingPunctuation, &RenderStyle::setHangingPunctuation),
        new DiscretePropertyWrapper<Hyphens>(CSSPropertyHyphens, &RenderStyle::hyphens, &RenderStyle::setHyphens),
        new DiscretePropertyWrapper<const AtomString&>(CSSPropertyHyphenateCharacter, &RenderStyle::hyphenationString, &RenderStyle::setHyphenationString),
        new DiscretePropertyWrapper<ImageOrientation>(CSSPropertyImageOrientation, &RenderStyle::imageOrientation, &RenderStyle::setImageOrientation),
        new DiscretePropertyWrapper<ImageRendering>(CSSPropertyImageRendering, &RenderStyle::imageRendering, &RenderStyle::setImageRendering),
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
        new DiscretePropertyWrapper<RubyPosition>(CSSPropertyRubyPosition, &RenderStyle::rubyPosition, &RenderStyle::setRubyPosition),
        new DiscretePropertyWrapper<RubyAlign>(CSSPropertyRubyAlign, &RenderStyle::rubyAlign, &RenderStyle::setRubyAlign),
        new DiscretePropertyWrapper<RubyOverhang>(CSSPropertyRubyOverhang, &RenderStyle::rubyOverhang, &RenderStyle::setRubyOverhang),
        new DiscretePropertyWrapper<TableLayoutType>(CSSPropertyTableLayout, &RenderStyle::tableLayout, &RenderStyle::setTableLayout),
        new DiscretePropertyWrapper<TextAlignMode>(CSSPropertyTextAlign, &RenderStyle::textAlign, &RenderStyle::setTextAlign),
        new DiscretePropertyWrapper<TextAlignLast>(CSSPropertyTextAlignLast, &RenderStyle::textAlignLast, &RenderStyle::setTextAlignLast),
        new DiscretePropertyWrapper<OptionSet<TextDecorationLine>>(CSSPropertyTextDecorationLine, &RenderStyle::textDecorationLine, &RenderStyle::setTextDecorationLine),
        new DiscretePropertyWrapper<TextDecorationStyle>(CSSPropertyTextDecorationStyle, &RenderStyle::textDecorationStyle, &RenderStyle::setTextDecorationStyle),
        new PropertyWrapperVisitedAffectedStyleColor(CSSPropertyTextEmphasisColor, &RenderStyle::textEmphasisColor, &RenderStyle::setTextEmphasisColor, &RenderStyle::visitedLinkTextEmphasisColor, &RenderStyle::setVisitedLinkTextEmphasisColor),
        new DiscretePropertyWrapper<OptionSet<TextEmphasisPosition>>(CSSPropertyTextEmphasisPosition, &RenderStyle::textEmphasisPosition, &RenderStyle::setTextEmphasisPosition),
        new TextEmphasisStyleWrapper,
        new DiscretePropertyWrapper<TextGroupAlign>(CSSPropertyTextGroupAlign, &RenderStyle::textGroupAlign, &RenderStyle::setTextGroupAlign),
        new DiscretePropertyWrapper<TextJustify>(CSSPropertyTextJustify, &RenderStyle::textJustify, &RenderStyle::setTextJustify),
        new DiscretePropertyWrapper<TextOverflow>(CSSPropertyTextOverflow, &RenderStyle::textOverflow, &RenderStyle::setTextOverflow),
        new DiscretePropertyWrapper<OptionSet<TouchAction>>(CSSPropertyTouchAction, &RenderStyle::touchActions, &RenderStyle::setTouchActions),
        new DiscretePropertyWrapper<OptionSet<TextTransform>>(CSSPropertyTextTransform, &RenderStyle::textTransform, &RenderStyle::setTextTransform),
        new DiscretePropertyWrapper<WhiteSpaceCollapse>(CSSPropertyWhiteSpaceCollapse, &RenderStyle::whiteSpaceCollapse, &RenderStyle::setWhiteSpaceCollapse),
        new DiscretePropertyWrapper<TextWrapMode>(CSSPropertyTextWrapMode, &RenderStyle::textWrapMode, &RenderStyle::setTextWrapMode),
        new DiscretePropertyWrapper<TextWrapStyle>(CSSPropertyTextWrapStyle, &RenderStyle::textWrapStyle, &RenderStyle::setTextWrapStyle),
        new DiscretePropertyWrapper<TransformBox>(CSSPropertyTransformBox, &RenderStyle::transformBox, &RenderStyle::setTransformBox),
        new DiscretePropertyWrapper<TransformStyle3D>(CSSPropertyTransformStyle, &RenderStyle::transformStyle3D, &RenderStyle::setTransformStyle3D),
        new DiscretePropertyWrapper<WordBreak>(CSSPropertyWordBreak, &RenderStyle::wordBreak, &RenderStyle::setWordBreak),
        new DiscretePropertyWrapper<OverflowAnchor>(CSSPropertyOverflowAnchor, &RenderStyle::overflowAnchor, &RenderStyle::setOverflowAnchor),
        new DiscretePropertyWrapper<TextSpacingTrim>(CSSPropertyTextSpacingTrim, &RenderStyle::textSpacingTrim, &RenderStyle::setTextSpacingTrim),
        new DiscretePropertyWrapper<TextAutospace>(CSSPropertyTextAutospace, &RenderStyle::textAutospace, &RenderStyle::setTextAutospace),
        new DiscretePropertyWrapper<OptionSet<TextUnderlinePosition>>(CSSPropertyTextUnderlinePosition, &RenderStyle::textUnderlinePosition, &RenderStyle::setTextUnderlinePosition),

        new DiscretePropertyWrapper<BoxDecorationBreak>(CSSPropertyWebkitBoxDecorationBreak, &RenderStyle::boxDecorationBreak, &RenderStyle::setBoxDecorationBreak),
        new DiscretePropertyWrapper<Isolation>(CSSPropertyIsolation, &RenderStyle::isolation, &RenderStyle::setIsolation),
        new DiscretePropertyWrapper<BlendMode>(CSSPropertyMixBlendMode, &RenderStyle::blendMode, &RenderStyle::setBlendMode),
        new DiscretePropertyWrapper<BlendMode>(CSSPropertyBackgroundBlendMode, &RenderStyle::backgroundBlendMode, &RenderStyle::setBackgroundBlendMode),
        new DiscretePropertyWrapper<StyleAppearance>(CSSPropertyAppearance, &RenderStyle::appearance, &RenderStyle::setAppearance),
#if ENABLE(DARK_MODE_CSS)
        new DiscretePropertyWrapper<Style::ColorScheme>(CSSPropertyColorScheme, &RenderStyle::colorScheme, &RenderStyle::setColorScheme),
#endif
        new PropertyWrapperAspectRatio,
        new DiscretePropertyWrapper<const FontPalette&>(CSSPropertyFontPalette, &RenderStyle::fontPalette, &RenderStyle::setFontPalette),

        new OffsetPathWrapper,
        new OffsetDistanceWrapper,
        new OffsetLengthPointWrapper(CSSPropertyOffsetPosition, &RenderStyle::offsetPosition, &RenderStyle::setOffsetPosition),
        new OffsetLengthPointWrapper(CSSPropertyOffsetAnchor, &RenderStyle::offsetAnchor, &RenderStyle::setOffsetAnchor),
        new OffsetRotateWrapper,

        new PropertyWrapperContent,
        new DiscretePropertyWrapper<TextDecorationSkipInk>(CSSPropertyTextDecorationSkipInk, &RenderStyle::textDecorationSkipInk, &RenderStyle::setTextDecorationSkipInk),
        new DiscreteSVGPropertyWrapper<ColorInterpolation>(CSSPropertyColorInterpolation, &SVGRenderStyle::colorInterpolation, &SVGRenderStyle::setColorInterpolation),
        new DiscreteFontDescriptionTypedWrapper<Kerning>(CSSPropertyFontKerning, &FontCascadeDescription::kerning, &FontCascadeDescription::setKerning),
        new FontFeatureSettingsWrapper,
        new FontFamilyWrapper,
        new DiscreteSVGPropertyWrapper<AlignmentBaseline>(CSSPropertyAlignmentBaseline, &SVGRenderStyle::alignmentBaseline, &SVGRenderStyle::setAlignmentBaseline),
        new DiscreteSVGPropertyWrapper<BufferedRendering>(CSSPropertyBufferedRendering, &SVGRenderStyle::bufferedRendering, &SVGRenderStyle::setBufferedRendering),
        new DiscreteSVGPropertyWrapper<WindRule>(CSSPropertyClipRule, &SVGRenderStyle::clipRule, &SVGRenderStyle::setClipRule),
        new DiscreteSVGPropertyWrapper<ColorInterpolation>(CSSPropertyColorInterpolationFilters, &SVGRenderStyle::colorInterpolationFilters, &SVGRenderStyle::setColorInterpolationFilters),
        new DiscreteSVGPropertyWrapper<DominantBaseline>(CSSPropertyDominantBaseline, &SVGRenderStyle::dominantBaseline, &SVGRenderStyle::setDominantBaseline),
        new CounterWrapper(CSSPropertyCounterIncrement),
        new CounterWrapper(CSSPropertyCounterReset),
        new CounterWrapper(CSSPropertyCounterSet),
        new DiscreteSVGPropertyWrapper<WindRule>(CSSPropertyFillRule, &SVGRenderStyle::fillRule, &SVGRenderStyle::setFillRule),
        new DiscreteFontDescriptionTypedWrapper<FontSynthesisLonghandValue>(CSSPropertyFontSynthesisWeight, &FontCascadeDescription::fontSynthesisWeight, &FontCascadeDescription::setFontSynthesisWeight),
        new DiscreteFontDescriptionTypedWrapper<FontSynthesisLonghandValue>(CSSPropertyFontSynthesisStyle, &FontCascadeDescription::fontSynthesisStyle, &FontCascadeDescription::setFontSynthesisStyle),
        new DiscreteFontDescriptionTypedWrapper<FontSynthesisLonghandValue>(CSSPropertyFontSynthesisSmallCaps, &FontCascadeDescription::fontSynthesisSmallCaps, &FontCascadeDescription::setFontSynthesisSmallCaps),
        new DiscreteFontDescriptionTypedWrapper<const FontVariantAlternates&>(CSSPropertyFontVariantAlternates, &FontCascadeDescription::variantAlternates, &FontCascadeDescription::setVariantAlternates),
        new FontVariantEastAsianWrapper,
        new FontVariantLigaturesWrapper,
        new FontVariantNumericWrapper,
        new DiscreteFontDescriptionTypedWrapper<FontVariantPosition>(CSSPropertyFontVariantPosition, &FontCascadeDescription::variantPosition, &FontCascadeDescription::setVariantPosition),
        new DiscreteFontDescriptionTypedWrapper<FontVariantCaps>(CSSPropertyFontVariantCaps, &FontCascadeDescription::variantCaps, &FontCascadeDescription::setVariantCaps),
        new DiscreteFontDescriptionTypedWrapper<FontVariantEmoji>(CSSPropertyFontVariantEmoji, &FontCascadeDescription::variantEmoji, &FontCascadeDescription::setVariantEmoji),
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
        new DiscreteSVGPropertyWrapper<const String&>(CSSPropertyMarkerStart, &SVGRenderStyle::markerStartResource, &SVGRenderStyle::setMarkerStartResource),
        new DiscretePropertyWrapper<ScrollbarGutter>(CSSPropertyScrollbarGutter, &RenderStyle::scrollbarGutter, &RenderStyle::setScrollbarGutter),
        new DiscretePropertyWrapper<ScrollbarWidth>(CSSPropertyScrollbarWidth, &RenderStyle::scrollbarWidth, &RenderStyle::setScrollbarWidth),
        new DiscretePropertyWrapper<const ScrollSnapAlign&>(CSSPropertyScrollSnapAlign, &RenderStyle::scrollSnapAlign, &RenderStyle::setScrollSnapAlign),
        new DiscretePropertyWrapper<ScrollSnapStop>(CSSPropertyScrollSnapStop, &RenderStyle::scrollSnapStop, &RenderStyle::setScrollSnapStop),
        new DiscretePropertyWrapper<ScrollSnapType>(CSSPropertyScrollSnapType, &RenderStyle::scrollSnapType, &RenderStyle::setScrollSnapType),
        new DiscretePropertyWrapper<const Vector<Style::ScopedName>&>(CSSPropertyViewTransitionClass, &RenderStyle::viewTransitionClasses, &RenderStyle::setViewTransitionClasses),
        new DiscretePropertyWrapper<Style::ViewTransitionName>(CSSPropertyViewTransitionName, &RenderStyle::viewTransitionName, &RenderStyle::setViewTransitionName),
        new DiscretePropertyWrapper<FieldSizing>(CSSPropertyFieldSizing, &RenderStyle::fieldSizing, &RenderStyle::setFieldSizing),
        new DiscretePropertyWrapper<const Vector<Style::ScopedName>&>(CSSPropertyAnchorName, &RenderStyle::anchorNames, &RenderStyle::setAnchorNames),
        new DiscretePropertyWrapper<const std::optional<Style::ScopedName>&>(CSSPropertyPositionAnchor, &RenderStyle::positionAnchor, &RenderStyle::setPositionAnchor),
        new DiscretePropertyWrapper<Style::PositionTryOrder>(CSSPropertyPositionTryOrder, &RenderStyle::positionTryOrder, &RenderStyle::setPositionTryOrder),
        new DiscretePropertyWrapper<const BlockEllipsis&>(CSSPropertyBlockEllipsis, &RenderStyle::blockEllipsis, &RenderStyle::setBlockEllipsis),
        new DiscretePropertyWrapper<size_t>(CSSPropertyMaxLines, &RenderStyle::maxLines, &RenderStyle::setMaxLines),
        new DiscretePropertyWrapper<OverflowContinue>(CSSPropertyContinue, &RenderStyle::overflowContinue, &RenderStyle::setOverflowContinue)
    };
    const unsigned animatableLonghandPropertiesCount = std::size(animatableLonghandPropertyWrappers);

    static const CSSPropertyID animatableShorthandProperties[] = {
        CSSPropertyAll,
        CSSPropertyBackground, // for background-color, background-position, background-image
        CSSPropertyBackgroundPosition,
        CSSPropertyFont, // for font-size, font-weight
        CSSPropertyMask, // for mask-position
        CSSPropertyWebkitMask, // for mask-position
        CSSPropertyMaskPosition,
        CSSPropertyWebkitMaskPosition,
        CSSPropertyBorderBlock,
        CSSPropertyBorderBlockColor,
        CSSPropertyBorderBlockStyle,
        CSSPropertyBorderBlockWidth,
        CSSPropertyBorderInline,
        CSSPropertyBorderInlineColor,
        CSSPropertyBorderInlineStyle,
        CSSPropertyBorderInlineWidth,
        CSSPropertyBorderTop, CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft,
        CSSPropertyBorderBlockStart, CSSPropertyBorderBlockEnd, CSSPropertyBorderInlineStart, CSSPropertyBorderInlineEnd,
        CSSPropertyBorderColor,
        CSSPropertyBorderRadius,
        CSSPropertyBorderWidth,
        CSSPropertyBorder,
        CSSPropertyBorderImage,
        CSSPropertyBorderSpacing,
        CSSPropertyColumns,
        CSSPropertyFlex,
        CSSPropertyFlexFlow,
        CSSPropertyGap,
        CSSPropertyGrid,
        CSSPropertyGridArea,
        CSSPropertyGridColumn,
        CSSPropertyGridRow,
        CSSPropertyGridTemplate,
        CSSPropertyInsetBlock, // logical shorthand
        CSSPropertyLineClamp,
        CSSPropertyListStyle, // for list-style-image
        CSSPropertyMargin,
        CSSPropertyMarginBlock, // logical shorthand
        CSSPropertyMarginInline, // logical shorthand
        CSSPropertyMarker,
        CSSPropertyMaskBorder,
        CSSPropertyOutline,
        CSSPropertyPadding,
        CSSPropertyPaddingBlock,
        CSSPropertyPaddingInline,
        CSSPropertyPageBreakAfter,
        CSSPropertyPageBreakBefore,
        CSSPropertyPageBreakInside,
        CSSPropertyPlaceContent,
        CSSPropertyPlaceItems,
        CSSPropertyPlaceSelf,
        CSSPropertyWebkitTextStroke,
        CSSPropertyColumnRule,
        CSSPropertyWebkitBorderRadius,
        CSSPropertyTextDecoration,
        CSSPropertyTextDecorationSkip,
        CSSPropertyTransformOrigin,
        CSSPropertyPerspectiveOrigin,
        CSSPropertyOffset,
        CSSPropertyOverflow,
        CSSPropertyTextEmphasis,
        CSSPropertyFontVariant,
        CSSPropertyFontSynthesis,
        CSSPropertyContainIntrinsicSize,
        CSSPropertyTextBox,
        CSSPropertyTextWrap,
        CSSPropertyWhiteSpace
    };
    const unsigned animatableShorthandPropertiesCount = std::size(animatableShorthandProperties);

    // Make sure unused slots have a value
    for (int i = 0; i < numCSSProperties; ++i)
        m_propertyToIdMap[i] = cInvalidPropertyWrapperIndex;

    static_assert(animatableLonghandPropertiesCount + animatableShorthandPropertiesCount < std::numeric_limits<unsigned short>::max(), "number of AnimatableProperties must be less than UShrtMax");
    m_propertyWrappers.reserveInitialCapacity(animatableLonghandPropertiesCount + animatableShorthandPropertiesCount);

    // First we put the non-shorthand property wrappers into the map, so the shorthand-building
    // code can find them.
    unsigned index = 0;
    m_propertyWrappers.appendContainerWithMapping(animatableLonghandPropertyWrappers, [&](auto* wrapper) {
        indexFromPropertyID(wrapper->property()) = index++;
        return std::unique_ptr<AnimationPropertyWrapperBase>(wrapper);
    });

    for (size_t i = 0; i < animatableShorthandPropertiesCount; ++i) {
        CSSPropertyID propertyID = animatableShorthandProperties[i];
        auto shorthand = shorthandForProperty(propertyID);
        if (!shorthand.length())
            continue;

        auto longhandWrappers = WTF::compactMap(shorthand, [&](auto longhand) -> std::optional<AnimationPropertyWrapperBase*> {
            unsigned wrapperIndex = indexFromPropertyID(longhand);
            if (wrapperIndex == cInvalidPropertyWrapperIndex)
                return std::nullopt;
            ASSERT(m_propertyWrappers[wrapperIndex]);
            return m_propertyWrappers[wrapperIndex].get();
        });

        m_propertyWrappers.append(makeUnique<ShorthandPropertyWrapper>(propertyID, WTFMove(longhandWrappers)));
        indexFromPropertyID(propertyID) = animatableLonghandPropertiesCount + i;
    }

#ifndef NDEBUG
    for (auto property : allCSSProperties()) {
        switch (property) {
        // If a property is not animatable per spec, add it to this list of cases.
        // When adding a new property, you should make sure it belongs in this list
        // or provide a wrapper for it above. If you are adding to this list but the
        // property should be animatable, make sure to file a bug.

        // To be fixed / untriaged:
        case CSSPropertyBorderStyle:
        case CSSPropertyInlineSize:
        case CSSPropertyInputSecurity:
        case CSSPropertyInset:
        case CSSPropertyInsetBlockEnd:
        case CSSPropertyInsetBlockStart:
        case CSSPropertyInsetInline:
        case CSSPropertyInsetInlineEnd:
        case CSSPropertyInsetInlineStart:
        case CSSPropertyMasonryAutoFlow:
        case CSSPropertyOverscrollBehavior:
        case CSSPropertyOverscrollBehaviorBlock:
        case CSSPropertyOverscrollBehaviorInline:
        case CSSPropertyOverscrollBehaviorX:
        case CSSPropertyOverscrollBehaviorY:
        case CSSPropertyPage:
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
#if ENABLE(TEXT_AUTOSIZING)
        case CSSPropertyWebkitTextSizeAdjust:
#endif
        case CSSPropertyViewTimeline:
        case CSSPropertyViewTimelineInset: // FIXME: view-timeline-inset should be animatable (bug 265690)
        case CSSPropertyWebkitUserSelect:

        // Not animatable per-spec:
        case CSSPropertyAnimation:
        case CSSPropertyAnimationComposition:
        case CSSPropertyAnimationDelay:
        case CSSPropertyAnimationDirection:
        case CSSPropertyAnimationDuration:
        case CSSPropertyAnimationFillMode:
        case CSSPropertyAnimationIterationCount:
        case CSSPropertyAnimationName:
        case CSSPropertyAnimationPlayState:
        case CSSPropertyAnimationRange:
        case CSSPropertyAnimationRangeStart:
        case CSSPropertyAnimationRangeEnd:
        case CSSPropertyAnimationTimeline:
        case CSSPropertyAnimationTimingFunction:
        case CSSPropertyContain:
        case CSSPropertyContainer:
        case CSSPropertyContainerName:
        case CSSPropertyContainerType:
        case CSSPropertyDirection:
        case CSSPropertyGlyphOrientationHorizontal:
        case CSSPropertyGlyphOrientationVertical:
        case CSSPropertyMathStyle:
        case CSSPropertyScrollTimeline:
        case CSSPropertyScrollTimelineAxis:
        case CSSPropertyScrollTimelineName:
        case CSSPropertySpeakAs:
        case CSSPropertyTextCombineUpright:
        case CSSPropertyTextOrientation:
        case CSSPropertyTimelineScope:
        case CSSPropertyTransition:
        case CSSPropertyTransitionBehavior:
        case CSSPropertyTransitionDelay:
        case CSSPropertyTransitionDuration:
        case CSSPropertyTransitionProperty:
        case CSSPropertyTransitionTimingFunction:
        case CSSPropertyUnicodeBidi:
        case CSSPropertyViewTimelineAxis:
        case CSSPropertyViewTimelineName:
        case CSSPropertyWillChange:
        case CSSPropertyWritingMode:
        case CSSPropertyZoom:

        // FIXME: This is a descriptor, not a CSS property:
        case CSSPropertySize:

        // Legacy -webkit- properties.
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
        case CSSPropertyWebkitFontSmoothing:
        case CSSPropertyWebkitHyphenateLimitAfter:
        case CSSPropertyWebkitHyphenateLimitBefore:
        case CSSPropertyWebkitHyphenateLimitLines:
        case CSSPropertyWebkitLineAlign:
        case CSSPropertyWebkitLineBoxContain:
        case CSSPropertyWebkitLineClamp:
        case CSSPropertyWebkitLineGrid:
        case CSSPropertyWebkitLineSnap:
        case CSSPropertyWebkitLocale:
        case CSSPropertyWebkitMarqueeDirection:
        case CSSPropertyWebkitMarqueeIncrement:
        case CSSPropertyWebkitMarqueeRepetition:
        case CSSPropertyWebkitMarqueeSpeed:
        case CSSPropertyWebkitMarqueeStyle:
        case CSSPropertyWebkitMaskClip:
        case CSSPropertyWebkitMaskComposite:
        case CSSPropertyWebkitMaskSourceType:
        case CSSPropertyWebkitNbspMode:
        case CSSPropertyWebkitPerspective:
        case CSSPropertyWebkitRubyPosition:
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
        case CSSPropertyWebkitTextOrientation:
        case CSSPropertyWebkitTextSecurity:
#if ENABLE(TEXT_AUTOSIZING)
        case CSSPropertyInternalTextAutosizingStatus:
#endif
        case CSSPropertyWebkitTextStroke:
        case CSSPropertyWebkitTextStrokeWidth:
        case CSSPropertyWebkitTextZoom:
#if PLATFORM(IOS_FAMILY)
        case CSSPropertyWebkitTouchCallout:
#endif
        case CSSPropertyWebkitUserDrag:
        case CSSPropertyWebkitUserModify:
            continue;
        default:
            if (CSSProperty::isDescriptorOnly(property))
                continue;

            auto resolvedProperty = CSSProperty::resolveDirectionAwareProperty(property, WritingMode());
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
        auto isDiscrete = !wrapper->canInterpolate(from, to, compositeOperation);
        CSSPropertyBlendingContext context { progress, isDiscrete, compositeOperation, client, property, iterationCompositeOperation, currentIteration };
        if (wrapper->normalizesProgressForDiscreteInterpolation())
            context.normalizeProgress();
        wrapper->blend(destination, from, to, context);
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
        if (!fromStyleColor.isCurrentColor() || !toStyleColor.isCurrentColor())
            return blendFunc(fromStyle.colorResolvingCurrentColor(fromStyleColor), toStyle.colorResolvingCurrentColor(toStyleColor), blendingContext);
    }

    if (std::holds_alternative<CSSCustomPropertyValue::NumericSyntaxValue>(from) && std::holds_alternative<CSSCustomPropertyValue::NumericSyntaxValue>(to)) {
        auto& fromNumeric = std::get<CSSCustomPropertyValue::NumericSyntaxValue>(from);
        auto& toNumeric = std::get<CSSCustomPropertyValue::NumericSyntaxValue>(to);
        if (fromNumeric.unitType == toNumeric.unitType)
            return blendFunc(fromNumeric, toNumeric, blendingContext);
    }

    if (std::holds_alternative<CSSCustomPropertyValue::TransformSyntaxValue>(from) && std::holds_alternative<CSSCustomPropertyValue::TransformSyntaxValue>(to)) {
        auto& fromTransformOperation = std::get<CSSCustomPropertyValue::TransformSyntaxValue>(from).transform;
        auto& toTransformOperation = std::get<CSSCustomPropertyValue::TransformSyntaxValue>(to).transform;
        return CSSCustomPropertyValue::TransformSyntaxValue { blendFunc(fromTransformOperation, toTransformOperation, blendingContext) };
    }

    return std::nullopt;
}

static std::optional<CSSCustomPropertyValue::SyntaxValue> firstValueInSyntaxValueLists(const CSSCustomPropertyValue::SyntaxValueList& a, const CSSCustomPropertyValue::SyntaxValueList& b)
{
    if (!a.values.isEmpty())
        return a.values[0];
    if (!b.values.isEmpty())
        return b.values[0];
    return std::nullopt;
}

static std::optional<CSSCustomPropertyValue::SyntaxValueList> blendSyntaxValueLists(const RenderStyle& fromStyle, const RenderStyle& toStyle, const CSSCustomPropertyValue::SyntaxValueList& from, const CSSCustomPropertyValue::SyntaxValueList& to, const CSSPropertyBlendingContext& blendingContext)
{
    // We should only attempt to blend lists containing the same types. Since we know all items in a
    // list are of the same type, it is sufficient to check the first value from each list.
    if (from.values.size() && to.values.size() && from.values.first().index() != to.values.first().index())
        return std::nullopt;

    // https://drafts.css-houdini.org/css-properties-values-api-1/#animation-behavior-of-custom-properties
    auto firstValue = firstValueInSyntaxValueLists(from, to);

    if (!firstValue)
        return std::nullopt;

    // <transform-function> lists are special in that they don't require matching numbers of items.
    if (std::holds_alternative<CSSCustomPropertyValue::TransformSyntaxValue>(*firstValue)) {
        auto transformOperationsFromSyntaxValueList = [](const CSSCustomPropertyValue::SyntaxValueList& list) {
            return TransformOperations {
                list.values.map([](auto& syntaxValue) {
                    ASSERT(std::holds_alternative<CSSCustomPropertyValue::TransformSyntaxValue>(syntaxValue));
                    return std::get<CSSCustomPropertyValue::TransformSyntaxValue>(syntaxValue).transform.copyRef();
                })
            };
        };

        auto fromTransformOperations = transformOperationsFromSyntaxValueList(from);
        auto toTransformOperations = transformOperationsFromSyntaxValueList(to);
        auto blendedTransformOperations = blendFunc(fromTransformOperations, toTransformOperations, blendingContext);

        auto blendedSyntaxValues = WTF::map(blendedTransformOperations, [](auto& transformOperation) -> CSSCustomPropertyValue::SyntaxValue {
            return CSSCustomPropertyValue::TransformSyntaxValue { transformOperation.copyRef() };
        });

        return CSSCustomPropertyValue::SyntaxValueList { WTFMove(blendedSyntaxValues), from.separator };
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

static Ref<const CSSCustomPropertyValue> blendedCSSCustomPropertyValue(const RenderStyle& fromStyle, const RenderStyle& toStyle, const CSSCustomPropertyValue& from, const CSSCustomPropertyValue& to, const CSSPropertyBlendingContext& blendingContext)
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
    return blendingContext.progress < 0.5 ? from : to;
}

static std::pair<const CSSCustomPropertyValue*, const CSSCustomPropertyValue*> customPropertyValuesForBlending(const AtomString& customProperty, const RenderStyle& fromStyle, const RenderStyle& toStyle)
{
    return {
        fromStyle.customPropertyValue(customProperty),
        toStyle.customPropertyValue(customProperty)
    };
}

static void blendCustomProperty(const CSSPropertyBlendingClient& client, const AtomString& customProperty, RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, double progress, CompositeOperation compositeOperation, IterationCompositeOperation iterationCompositeOperation, double currentIteration)
{
    CSSPropertyBlendingContext blendingContext { progress, false, compositeOperation, client, customProperty, iterationCompositeOperation, currentIteration };

    auto [fromValue, toValue] = customPropertyValuesForBlending(customProperty, from, to);
    if (!fromValue || !toValue)
        return;

    bool isInherited = client.document()->customPropertyRegistry().isInherited(customProperty);
    destination.setCustomPropertyValue(blendedCSSCustomPropertyValue(from, to, *fromValue, *toValue, blendingContext), isInherited);
}

void CSSPropertyAnimation::blendProperty(const CSSPropertyBlendingClient& client, const AnimatableCSSProperty& property, RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, double progress, CompositeOperation compositeOperation, IterationCompositeOperation iterationCompositeOperation, double currentIteration)
{
    WTF::switchOn(property,
        [&] (CSSPropertyID propertyId) {
            blendStandardProperty(client, propertyId, destination, from, to, progress, compositeOperation, iterationCompositeOperation, currentIteration);
        }, [&] (const AtomString& customProperty) {
            blendCustomProperty(client, customProperty, destination, from, to, progress, compositeOperation, iterationCompositeOperation, currentIteration);
        }
    );
}

bool CSSPropertyAnimation::isPropertyAnimatable(const AnimatableCSSProperty& property)
{
    return WTF::switchOn(property,
        [] (CSSPropertyID propertyId) {
            return propertyId == CSSPropertyCustom || !!CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(propertyId);
        },
        [] (const AtomString&) {
            // FIXME: this should only be true for property that are registered custom properties.
            return true;
        }
    );
}

bool CSSPropertyAnimation::isPropertyAdditiveOrCumulative(const AnimatableCSSProperty& property)
{
    return WTF::switchOn(property,
        [] (CSSPropertyID propertyId) {
            if (auto* wrapper = CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(propertyId))
                return wrapper->isAdditiveOrCumulative();
            return false;
        }, [] (const AtomString&) { return true; }
    );
}

static bool syntaxValuesRequireBlendingForAccumulativeIteration(const CSSCustomPropertyValue::SyntaxValue& a, const CSSCustomPropertyValue::SyntaxValue& b, bool isList)
{
    return WTF::switchOn(a, [b, isList](const Length& aLength) {
        ASSERT(std::holds_alternative<Length>(b));
        return !isList && lengthsRequireBlendingForAccumulativeIteration(aLength, std::get<Length>(b));
    }, [] (const RefPtr<TransformOperation>&) {
        return true;
    }, [] (const StyleColor&) {
        return true;
    }, [] (auto&) {
        return false;
    });
}

bool CSSPropertyAnimation::propertyRequiresBlendingForAccumulativeIteration(const CSSPropertyBlendingClient&, const AnimatableCSSProperty& property, const RenderStyle& a, const RenderStyle& b)
{
    return WTF::switchOn(property,
        [&] (CSSPropertyID propertyId) {
            if (auto* wrapper = CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(propertyId))
                return wrapper->requiresBlendingForAccumulativeIteration(a, b);
            return false;
        }, [&] (const AtomString& customProperty) {
            auto [from, to] = customPropertyValuesForBlending(customProperty, a, b);
            if (!from || !to)
                return false;

            if (std::holds_alternative<CSSCustomPropertyValue::SyntaxValueList>(from->value()) && std::holds_alternative<CSSCustomPropertyValue::SyntaxValueList>(to->value())) {
                auto& fromSyntaxValues = std::get<CSSCustomPropertyValue::SyntaxValueList>(from->value()).values;
                auto& toSyntaxValues = std::get<CSSCustomPropertyValue::SyntaxValueList>(to->value()).values;
                if (fromSyntaxValues.size() == toSyntaxValues.size()) {
                    for (size_t i = 0; i < fromSyntaxValues.size(); ++i) {
                        if (!syntaxValuesRequireBlendingForAccumulativeIteration(fromSyntaxValues[i], toSyntaxValues[i], true))
                            return false;
                    }
                    return true;
                }
            }

            if (std::holds_alternative<CSSCustomPropertyValue::SyntaxValue>(from->value()) && std::holds_alternative<CSSCustomPropertyValue::SyntaxValue>(to->value())) {
                auto& fromSyntaxValue = std::get<CSSCustomPropertyValue::SyntaxValue>(from->value());
                auto& toSyntaxValue = std::get<CSSCustomPropertyValue::SyntaxValue>(to->value());
                return syntaxValuesRequireBlendingForAccumulativeIteration(fromSyntaxValue, toSyntaxValue, false);
            }

            return false;
        }
    );
}

bool CSSPropertyAnimation::animationOfPropertyIsAccelerated(const AnimatableCSSProperty& property, const Settings& settings)
{
    return WTF::switchOn(property,
        [&] (CSSPropertyID cssProperty) {
            if (auto* wrapper = CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(cssProperty))
                return wrapper->animationIsAccelerated(settings);
            return false;
        }, [] (const AtomString&) { return false; }
    );
}

bool CSSPropertyAnimation::propertiesEqual(const AnimatableCSSProperty& property, const RenderStyle& a, const RenderStyle& b, const Document&)
{
    return WTF::switchOn(property,
        [&] (CSSPropertyID propertyId) {
            if (auto* wrapper = CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(propertyId))
                return wrapper->equals(a, b);
            return true;
        }, [&] (const AtomString& customProperty) {
            auto [aCustomPropertyValue, bCustomPropertyValue] = customPropertyValuesForBlending(customProperty, a, b);
            if (aCustomPropertyValue && bCustomPropertyValue)
                return aCustomPropertyValue->equals(*bCustomPropertyValue);
            return !aCustomPropertyValue && !bCustomPropertyValue;
        }
    );
}

static bool typeOfSyntaxValueCanBeInterpolated(const CSSCustomPropertyValue::SyntaxValue& syntaxValue)
{
    return WTF::switchOn(syntaxValue,
        [] (const Length&) {
            return true;
        },
        [] (const StyleColor&) {
            return true;
        },
        [] (CSSCustomPropertyValue::NumericSyntaxValue) {
            return true;
        },
        [] (const CSSCustomPropertyValue::TransformSyntaxValue&) {
            return true;
        },
        [] (RefPtr<StyleImage>) {
            return false;
        },
        [] (auto&) {
            return false;
        }
    );
}

bool CSSPropertyAnimation::canPropertyBeInterpolated(const AnimatableCSSProperty& property, const RenderStyle& a, const RenderStyle& b, const Document&)
{
    return WTF::switchOn(property,
        [&] (CSSPropertyID propertyId) {
            if (auto* wrapper = CSSPropertyAnimationWrapperMap::singleton().wrapperForProperty(propertyId))
                return wrapper->canInterpolate(a, b, CompositeOperation::Replace);
            return true;
        }, [&] (const AtomString& customProperty) {
            auto [aCustomPropertyValue, bCustomPropertyValue] = customPropertyValuesForBlending(customProperty, a, b);
            if (!aCustomPropertyValue || !bCustomPropertyValue || aCustomPropertyValue == bCustomPropertyValue)
                return false;
            auto& aVariantValue = aCustomPropertyValue->value();
            auto& bVariantValue = bCustomPropertyValue->value();
            if (aVariantValue.index() != bVariantValue.index())
                return false;
            return WTF::switchOn(aVariantValue,
                [bVariantValue] (const CSSCustomPropertyValue::SyntaxValueList& aValueList) {
                    auto bValueList = std::get<CSSCustomPropertyValue::SyntaxValueList>(bVariantValue);
                    if (aValueList == bValueList)
                        return false;
                    if (auto firstValue = firstValueInSyntaxValueLists(aValueList, bValueList)) {
                        // List sizes must match except for transform lists.
                        if (!std::holds_alternative<CSSCustomPropertyValue::TransformSyntaxValue>(*firstValue)
                            && aValueList.values.size() != bValueList.values.size()) {
                            return false;
                        }
                        return typeOfSyntaxValueCanBeInterpolated(*firstValue);
                    }
                    return false;
                },
                [bVariantValue] (const CSSCustomPropertyValue::SyntaxValue& aSyntaxValue) {
                    auto bSyntaxValue = std::get<CSSCustomPropertyValue::SyntaxValue>(bVariantValue);
                    return aSyntaxValue != bSyntaxValue && typeOfSyntaxValueCanBeInterpolated(aSyntaxValue);
                },
                [] (auto&) {
                    return false;
                }
            );
        }
    );
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

std::optional<CSSPropertyID> CSSPropertyAnimation::getAcceleratedPropertyAtIndex(int i, const Settings& settings)
{
    // FIXME: We really ought to expose an iterator to go over all animatable properties.
    // https://bugs.webkit.org/show_bug.cgi?id=252807
    auto& map = CSSPropertyAnimationWrapperMap::singleton();

    if (i < 0 || static_cast<unsigned>(i) >= map.size())
        return std::nullopt;

    auto* wrapper = map.wrapperForIndex(i);
    if (wrapper->isShorthandWrapper() || !wrapper->animationIsAccelerated(settings))
        return std::nullopt;

    return wrapper->property();
}

int CSSPropertyAnimation::getNumProperties()
{
    return CSSPropertyAnimationWrapperMap::singleton().size();
}

}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
