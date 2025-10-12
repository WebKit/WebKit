/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "StyleImageWrapper.h"

#include "AnimationUtilities.h"
#include "CSSValue.h"
#include "StyleCachedImage.h"
#include "StyleCrossfadeImage.h"
#include "StyleFilterImage.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

// MARK: - Conversion

Ref<CSSValue> CSSValueCreation<ImageWrapper>::operator()(CSSValuePool&, const RenderStyle& style, const ImageWrapper& value)
{
    Ref image = value.value;
    return image->computedStyleValue(style);
}

// MARK: - Serialization

void Serialize<ImageWrapper>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const ImageWrapper& value)
{
    Ref image = value.value;
    builder.append(image->computedStyleValue(style)->cssText(context));
}

// MARK: - Blending

static ImageWrapper crossfadeBlend(Ref<StyleCachedImage>&& fromStyleImage, Ref<StyleCachedImage>&& toStyleImage, const BlendingContext& context)
{
    // If progress is at one of the extremes, we want getComputedStyle to show the image,
    // not a completed cross-fade, so we hand back one of the existing images.

    if (!context.progress)
        return ImageWrapper { WTFMove(fromStyleImage) };
    if (context.progress == 1)
        return ImageWrapper { WTFMove(toStyleImage) };
    if (!fromStyleImage->cachedImage() || !toStyleImage->cachedImage())
        return ImageWrapper { WTFMove(toStyleImage) };
    return ImageWrapper { StyleCrossfadeImage::create(WTFMove(fromStyleImage), WTFMove(toStyleImage), context.progress, false) };
}

static ImageWrapper filterBlend(RefPtr<StyleImage> inputImage, const Style::Filter& from, const Style::Filter& to, const BlendingContext& context)
{
    return ImageWrapper { StyleFilterImage::create(WTFMove(inputImage), Style::blend(from, to, context)) };
}

auto Blending<ImageWrapper>::blend(const ImageWrapper& a, const ImageWrapper& b, const BlendingContext& context) -> ImageWrapper
{
    if (!context.progress)
        return a;
    if (context.progress == 1.0)
        return b;

    Ref aImage = a.value;
    Ref bImage = b.value;

    RefPtr aSelectedUnchecked = aImage->selectedImage();
    RefPtr bSelectedUnchecked = bImage->selectedImage();

    if (!aSelectedUnchecked || !bSelectedUnchecked) {
        if (aSelectedUnchecked)
            return ImageWrapper { aSelectedUnchecked.releaseNonNull() };
        if (bSelectedUnchecked)
            return ImageWrapper { bSelectedUnchecked.releaseNonNull() };
        return context.progress > 0.5 ? b : a;
    }

    Ref aSelected = aSelectedUnchecked.releaseNonNull();
    Ref bSelected = bSelectedUnchecked.releaseNonNull();

    // Interpolation between two generated images. Cross fade for all other cases.
    if (auto [aFilter, bFilter] = std::tuple { dynamicDowncast<StyleFilterImage>(aSelected), dynamicDowncast<StyleFilterImage>(bSelected) }; aFilter && bFilter) {
        // Interpolation of generated images is only possible if the input images are equal.
        // Otherwise fall back to cross fade animation.
        if (aFilter->equalInputImages(*bFilter) && is<StyleCachedImage>(aFilter->inputImage()))
            return filterBlend(aFilter->inputImage(), aFilter->filter(), bFilter->filter(), context);
    } else if (auto [aCrossfade, bCrossfade] = std::tuple { dynamicDowncast<StyleCrossfadeImage>(aSelected), dynamicDowncast<StyleCrossfadeImage>(bSelected) }; aCrossfade && bCrossfade) {
        if (aCrossfade->equalInputImages(*bCrossfade)) {
            if (RefPtr crossfadeBlend = bCrossfade->blend(*aCrossfade, context))
                return ImageWrapper { crossfadeBlend.releaseNonNull() };
        }
    } else if (auto [aFilter, bCachedImage] = std::tuple { dynamicDowncast<StyleFilterImage>(aSelected), dynamicDowncast<StyleCachedImage>(bSelected) }; aFilter && bCachedImage) {
        RefPtr aFilterInputImage = dynamicDowncast<StyleCachedImage>(aFilter->inputImage());

        if (aFilterInputImage && bCachedImage->equals(*aFilterInputImage))
            return filterBlend(WTFMove(aFilterInputImage), aFilter->filter(), Style::Filter { CSS::Keyword::None { } }, context);
    } else if (auto [aCachedImage, bFilter] = std::tuple { dynamicDowncast<StyleCachedImage>(aSelected), dynamicDowncast<StyleFilterImage>(bSelected) }; aCachedImage && bFilter) {
        RefPtr bFilterInputImage = dynamicDowncast<StyleCachedImage>(bFilter->inputImage());

        if (bFilterInputImage && aCachedImage->equals(*bFilterInputImage))
            return filterBlend(WTFMove(bFilterInputImage), Style::Filter { CSS::Keyword::None { } }, bFilter->filter(), context);
    }

    RefPtr aCachedImage = dynamicDowncast<StyleCachedImage>(aSelected);
    RefPtr bCachedImage = dynamicDowncast<StyleCachedImage>(bSelected);
    if (aCachedImage && bCachedImage)
        return crossfadeBlend(aCachedImage.releaseNonNull(), bCachedImage.releaseNonNull(), context);

    // FIXME: Add support for interpolation between two *gradient() functions.
    // https://bugs.webkit.org/show_bug.cgi?id=119956

    // FIXME: Add support cross fade between cached and generated images.
    // https://bugs.webkit.org/show_bug.cgi?id=78293

    return ImageWrapper { WTFMove(bSelected) };
}

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream& ts, const ImageWrapper& value)
{
    Ref image = value.value;

    ts << "image"_s;
    if (!image->url().resolved.isEmpty())
        ts << '(' << image->url().resolved << ')';
    return ts;
}

} // namespace Style
} // namespace WebCore
