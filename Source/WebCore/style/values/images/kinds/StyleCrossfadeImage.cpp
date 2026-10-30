/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "StyleCrossfadeImage.h"

#include "AnimationUtilities.h"
#include "CSSCrossfadeValue.h"
#include "CSSValuePool.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "DeprecatedCSSOMValue.h"
#include "Document.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "ImageQualityController.h"
#include "RenderElement.h"
#include "RenderObjectDocument.h"
#include "StyleCrossfadeInputSizing.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include <wtf/PointerComparison.h>
#include <wtf/ZippedRange.h>

#if ENABLE(AX_CUSTOM_COLOR_MODE)
#include <WebKitAdditions/AXCustomColorModeController.h>
#endif

namespace WebCore {
namespace Style {

CrossfadeImage::CrossfadeImage(CrossfadeFunction&& function)
    : GeneratedImage { Type::CrossfadeImage }
    , m_function { WTF::move(function) }
    , m_inputImagesAreReady { false }
{
    normalizePercentages();
}

CrossfadeImage::CrossfadeImage(WebkitCrossfadeFunction&& function)
    : GeneratedImage { Type::CrossfadeImage }
    , m_function { WTF::move(function) }
    , m_inputImagesAreReady { false }
{
    normalizePercentages();
}

Ref<CrossfadeImage> CrossfadeImage::create(CrossfadeFunction&& function)
{
    return adoptRef(*new CrossfadeImage(WTF::move(function)));
}

Ref<CrossfadeImage> CrossfadeImage::create(WebkitCrossfadeFunction&& function)
{
    return adoptRef(*new CrossfadeImage(WTF::move(function)));
}

CrossfadeImage::~CrossfadeImage()
{
    for (auto& cachedImage : m_cachedImages) {
        if (cachedImage)
            protect(cachedImage)->removeClient(*this);
    }
}

namespace {

struct CrossfadeInput {
    using Percentage = Style::Percentage<CSS::ClosedPercentageRange>;

    RefPtr<Image> image;
    std::optional<Percentage> percentage;
};

}

decltype(auto) CrossfadeImage::withInputs(NOESCAPE auto&& function) const
{
    return WTF::switchOn(m_function,
        [&](const CrossfadeFunction& crossfade) -> decltype(auto) {
            return function(crossfade.parameters.components.value | std::views::transform([](const auto& component) {
                return CrossfadeInput { .image = component.image.value.ptr(), .percentage = component.percentage };
            }));
        },
        [&](const WebkitCrossfadeFunction& crossfade) -> decltype(auto) {
            auto progress = crossfade.parameters.progress.value.value;
            return function(std::array {
                CrossfadeInput { .image = crossfade.parameters.from.tryStyleImage(), .percentage = CrossfadeInput::Percentage { 100 * (1 - progress) } },
                CrossfadeInput { .image = crossfade.parameters.to.tryStyleImage(), .percentage = CrossfadeInput::Percentage { 100 * progress } },
            });
        }
    );
}

void CrossfadeImage::normalizePercentages()
{
    m_normalizedPercentages = withInputs([](const auto& inputs) {
        return CSS::normalizedMixPercentages<CSS::ForceNormalization::No, Vector<double, 2>>(inputs);
    });
}

bool CrossfadeImage::operator==(const Image& other) const
{
    auto* otherCrossfadeImage = dynamicDowncast<CrossfadeImage>(other);
    return otherCrossfadeImage && equals(*otherCrossfadeImage);
}

bool CrossfadeImage::equals(const CrossfadeImage& other) const
{
    return m_function == other.m_function;
}

bool CrossfadeImage::equalInputImages(const CrossfadeImage& other) const
{
    if (m_function.index() != other.m_function.index())
        return false;

    return withInputs([&](const auto& inputs) {
        return other.withInputs([&](const auto& otherInputs) {
            return std::ranges::equal(inputs, otherInputs, [](const auto& input, const auto& otherInput) {
                return arePointingToEqualData(input.image, otherInput.image);
            });
        });
    });
}

RefPtr<CrossfadeImage> CrossfadeImage::blend(const CrossfadeImage& from, const BlendingContext& context) const
{
    ASSERT(equalInputImages(from));

    if (m_cachedImages.isEmpty() || m_cachedImages.containsIf([](auto& cachedImage) { return !cachedImage; }))
        return nullptr;

    return WTF::switchOn(m_function,
        [&](const CrossfadeFunction& function) -> RefPtr<CrossfadeImage> {
            auto& fromComponents = std::get<CrossfadeFunction>(from.m_function).parameters.components.value;
            auto& toComponents = function.parameters.components.value;

            CommaSeparatedVector<CrossfadeComponent> components;
            for (auto [fromComponent, toComponent] : zippedRange(fromComponents, toComponents)) {
                if (!fromComponent.percentage != !toComponent.percentage)
                    return nullptr;
                std::optional<CrossfadeComponent::Percentage> percentage;
                if (toComponent.percentage)
                    percentage = Style::blend(*fromComponent.percentage, *toComponent.percentage, context);
                components.value.append({ .image = toComponent.image, .percentage = percentage });
            }
            return CrossfadeImage::create(CrossfadeFunction { .parameters = { .components = WTF::move(components) } });
        },
        [&](const WebkitCrossfadeFunction& function) -> RefPtr<CrossfadeImage> {
            auto& fromParameters = std::get<WebkitCrossfadeFunction>(from.m_function).parameters;
            return CrossfadeImage::create(WebkitCrossfadeFunction {
                .parameters = {
                    .from = function.parameters.from,
                    .to = function.parameters.to,
                    .progress = Style::blend(fromParameters.progress, function.parameters.progress, context),
                }
            });
        }
    );
}

Ref<CSSValue> CrossfadeImage::computedStyleValue(const Style::ComputedStyle& style) const
{
    return WTF::switchOn(m_function,
        [&](const auto& function) -> Ref<CSSValue> {
            return CSSCrossfadeValue::create(toCSS(function, style));
        }
    );
}

Ref<DeprecatedCSSOMValue> CrossfadeImage::computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle& style, CSSStyleDeclaration& owner) const
{
    return computedStyleValue(style)->createDeprecatedCSSOMWrapper(owner);
}

bool CrossfadeImage::isPending() const
{
    return withInputs([](const auto& inputs) {
        return std::ranges::any_of(inputs, [](const auto& input) {
            return input.image && protect(input.image)->isPending();
        });
    });
}

void CrossfadeImage::load(CachedResourceLoader& loader, const ResourceLoaderOptions& options)
{
    auto cachedImages = withInputs([&](const auto& inputs) {
        return decltype(m_cachedImages)::map(inputs, [&](const auto& input) -> CachedResourceHandle<WebCore::CachedImage> {
            RefPtr image = input.image;
            if (!image)
                return nullptr;
            if (image->isPending())
                image->load(loader, options);
            return CachedResourceHandle<WebCore::CachedImage> { image->cachedImage() };
        });
    });

    auto count = std::max(cachedImages.size(), m_cachedImages.size());
    for (size_t i = 0; i < count; ++i) {
        auto oldCachedImage = i < m_cachedImages.size() ? m_cachedImages[i] : nullptr;
        auto newCachedImage = i < cachedImages.size() ? cachedImages[i] : nullptr;
        if (newCachedImage == oldCachedImage)
            continue;
        if (oldCachedImage)
            protect(oldCachedImage)->removeClient(*this);
        if (newCachedImage)
            protect(newCachedImage)->addClient(*this);
    }
    m_cachedImages = WTF::move(cachedImages);

    m_inputImagesAreReady = true;
}

static void drawCrossfadeInput(GraphicsContext& context, const RenderElement& renderer, const Image& input, ImageDrawingOptions inputOptions, CompositeOperator operation, float opacity, ConcreteObjectSize crossfadeSize, bool isForFirstLine)
{
    auto concreteSize = input.negotiate(renderer, CrossfadeInputSizing { crossfadeSize });
    auto imageSize = concreteSize.size();

    // A zero-sized input would produce a non-finite scale below, poisoning the CTM.
    if (imageSize.isEmpty())
        return;

    // SVGImage resets the opacity when painting, so we have to use transparency layers to accurately paint one at a given opacity.
    bool useTransparencyLayer = input.drawsSVGImage();

    GraphicsContextStateSaver stateSaver(context);

    auto options = inputOptions;
    if (useTransparencyLayer) {
        context.setCompositeOperation(operation);
        context.beginTransparencyLayer(opacity);
    } else {
        context.setAlpha(opacity);
        options = { options, operation };
    }

    if (auto targetSize = crossfadeSize.size(); targetSize != imageSize)
        context.scale(targetSize / imageSize);

    input.draw(context, renderer, concreteSize, FloatRect { { }, imageSize }, FloatRect { { }, imageSize }, options, isForFirstLine);

    if (useTransparencyLayer)
        context.endTransparencyLayer();
}

void CrossfadeImage::drawCrossfade(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, bool isForFirstLine) const
{
    // https://drafts.csswg.org/css-images-4/#cross-fade-painting

    if (concreteObjectSize.size().isEmpty())
        return;

    withInputs([&](const auto& inputs) {
        if (std::ranges::any_of(inputs, [&](const auto& input) { return !input.image || !protect(input.image)->canDraw(renderer); }))
            return;

        GraphicsContextStateSaver stateSaver(context);

        context.clip(FloatRect { { }, concreteObjectSize.size() });
        context.beginTransparencyLayer(1);

        // The final image is the weighted average of the inputs, rescaled to the concrete object size, with any
        // leftover percentage given to transparent black.
        auto operation = CompositeOperator::SourceOver;
        for (auto [input, percentage] : zippedRange(inputs, m_normalizedPercentages.percentages)) {
            Ref image = *input.image;
            ImageDrawingOptions inputOptions;
#if ENABLE(AX_CUSTOM_COLOR_MODE)
            auto invertContent = [&] {
                if (AXCustomColorModeController::shouldForceInvertBitmapImage(renderer, image))
                    return InvertContent::Forced;
                return AXCustomColorModeController::shouldInvertSVGImage(renderer, image) ? InvertContent::Yes : InvertContent::No;
            };
            inputOptions = ImageDrawingOptions { invertContent() };
#endif
            drawCrossfadeInput(context, renderer, image, inputOptions, operation, percentage / 100, concreteObjectSize, isForFirstLine);
            operation = CompositeOperator::PlusLighter;
        }

        context.endTransparencyLayer();
    });
}

ImageDrawResult CrossfadeImage::draw(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& source, ImageDrawingOptions options, bool isForFirstLine) const
{
    if (isPending() || !canDrawAtSize(renderer, flooredIntSize(destination.size())))
        return ImageDrawResult::DidNothing;

    return drawIntoDestination(context, destination, source, options, [&](GraphicsContext& context) {
        drawCrossfade(context, renderer, concreteObjectSize, isForFirstLine);
        return ImageDrawResult::DidDraw;
    });
}

ImageDrawResult CrossfadeImage::drawAsPattern(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImageDrawingOptions options, bool isForFirstLine) const
{
    if (!canDrawAtSize(renderer, concreteObjectSize.size()) || context.paintingDisabled())
        return ImageDrawResult::DidNothing;

    RefPtr imageBuffer = context.createImageBuffer(concreteObjectSize.size());
    if (!imageBuffer)
        return ImageDrawResult::DidNothing;

    drawCrossfade(imageBuffer->context(), renderer, concreteObjectSize, isForFirstLine);
    context.drawPattern(*imageBuffer, destination, tile, patternTransform, phase, spacing, options);

    return ImageDrawResult::DidDraw;
}

bool CrossfadeImage::currentFrameIsComplete(const RenderElement* renderer) const
{
    return withInputs([&](const auto& inputs) {
        return std::ranges::all_of(inputs, [&](const auto& input) {
            return !input.image || protect(input.image)->currentFrameIsComplete(renderer);
        });
    });
}

bool CrossfadeImage::knownToBeOpaque(const RenderElement& renderer) const
{
    // Any leftover percentage is transparent black.
    if (m_normalizedPercentages.leftover)
        return false;

    return withInputs([&](const auto& inputs) {
        return std::ranges::all_of(inputs, [&](const auto& input) {
            return !input.image || protect(input.image)->knownToBeOpaque(renderer);
        });
    });
}

bool CrossfadeImage::containsCurrentColor() const
{
    return withInputs([](const auto& inputs) {
        return std::ranges::any_of(inputs, [](const auto& input) {
            return input.image && protect(input.image)->containsCurrentColor();
        });
    });
}

bool CrossfadeImage::canDrawAtSize(const RenderElement& renderer, const FloatSize& size) const
{
    if (size.isEmpty())
        return false;

    return withInputs([&](const auto& inputs) {
        return std::ranges::all_of(inputs, [&](const auto& input) {
            return input.image && protect(input.image)->canDrawAtSize(renderer, size);
        });
    });
}

InterpolationQuality CrossfadeImage::interpolationQualityForImageDraw(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const void* layer, const LayoutSize& size) const
{
    return ImageQualityController::chooseInterpolationQuality(context, renderer, expandedIntSize(concreteObjectSize.size()), layer, size);
}

NaturalDimensions CrossfadeImage::naturalDimensions(const RenderElement& renderer, const ImageSizingContext& context) const
{
    // https://drafts.csswg.org/css-images-4/#cross-fade-sizing

    return withInputs([&](const auto& arguments) {
        // A -webkit-cross-fade() with an argument of none: nothing to draw, and an empty image rather than one sized by its box.
        if (std::ranges::any_of(arguments, [](const auto& argument) { return !argument.image; }))
            return NaturalDimensions::zero();

        // 1. Normalize mix percentages from the function’s arguments, and let args and leftover be the result.

        // NOTE: Done at construction, in m_normalizedPercentages.

        // 2. If leftover is 100%, return no natural dimensions.
        if (m_normalizedPercentages.leftover == 100)
            return NaturalDimensions::none();

        // 3. Let images be an empty list.
        struct Item {
            FloatSize size;
            double percentage;
        };
        Vector<Item, 2> images;

        // 4. For each <cf-image> argument of the function’s arguments:
        for (auto [argument, percentage] : zippedRange(arguments, m_normalizedPercentages.percentages)) {
            // 4.1 If argument is not an <image>, or is an <image> with no natural dimensions, continue.
            // FIXME: Add support non-image arguments.
            auto naturalDimensions = protect(argument.image)->naturalDimensions(renderer, context);
            if (naturalDimensions.isNone())
                continue;

            // 4.2 Let item be a tuple consisting of a width, a height, and a percentage.
            // 4.3 Run the object size negotiation algorithm for the <image>, as appropriate for the context in which the cross-fade() appears, and set item’s width and height to the width and height of the resulting concrete object size.
            // 4.4 Set item’s percentage to the argument’s percentage.
            images.append(Item { context.resolve(naturalDimensions).size(), percentage });
        }

        // 5. If images is empty, return no natural dimensions.
        if (images.isEmpty())
            return NaturalDimensions::none();

        // 6. Return a natural width and natural height that are weighted averages of the width and height of each item in images, according to their corresponding percentages.

        // FIXME: This device-pixel floor is a painting rule, not a property of the image -- a
        // cross-fade interpolates between its inputs and lands between pixels -- and it belongs
        // with the caller that is about to rasterize.
        auto flooredToDevicePixels = [&](FloatSize size) {
            return NaturalDimensions::fixed(floorSizeToDevicePixels(LayoutSize(size), protect(renderer.document())->deviceScaleFactor()));
        };

        // Rounding issues can cause transitions between images of equal size to return a
        // different size; avoid performing the interpolation if the sizes agree.
        if (images.size() == 2 && images[0].size == images[1].size)
            return flooredToDevicePixels(images[0].size);

        // The percentages of the items in images can sum to less than 100%, so the average is over their sum.
        double total = 0;
        auto weighted = FloatSize { };
        for (auto& image : images) {
            weighted = weighted + image.size * image.percentage;
            total += image.percentage;
        }
        if (!total)
            return NaturalDimensions::none();

        return flooredToDevicePixels(weighted / total);
    });
}

void CrossfadeImage::imageChanged(WebCore::CachedImage*, const IntRect*)
{
    if (!m_inputImagesAreReady)
        return;
    for (auto entry : clients()) {
        CheckedRef client = entry.key;
        client->imageChanged(static_cast<WrappedImagePtr>(this));
    }
}

} // namespace Style
} // namespace WebCore
