/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleCursor.h"

#include "CSSCursorImageValue.h"
#include "CSSValueList.h"
#include "Cursor.h"
#include "DocumentView.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "RenderElement.h"
#include "RenderView.h"
#include "StyleBuilderChecking.h"
#include "StyleCursorImage.h"
#include "StyleCursorSizing.h"
#include "StyleInvalidImage.h"
#include "StyleKeyword+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StylePrimitiveNumericTypes+Logging.h"

#if ENABLE(AX_CUSTOM_COLOR_MODE)
#include <WebKitAdditions/AXCustomColorModeController.h>
#endif

namespace WebCore {
namespace Style {

std::optional<WebCore::Cursor> Cursor::selectImageCursor(const RenderObject* renderer, const LocalFrame& frame, float deviceScaleFactor, IntPoint pointInMainFrame) const
{
    if (!images)
        return std::nullopt;

    CheckedPtr renderElement = dynamicDowncast<RenderElement>(renderer);
    if (!renderElement && renderer && renderer->parent())
        renderElement = renderer->parent();
    if (!renderElement)
        renderElement = frame.contentRenderer();
    if (!renderElement)
        return std::nullopt;

    RefPtr frameView = frame.view();
    if (!frameView)
        return std::nullopt;

    IntRect visibleContentRect = frameView->visibleContentRect();

    for (auto& styleCursorImage : *images) {
        Ref styleImage = styleCursorImage.image;
        if (styleImage->errorOccurred() || !styleImage->canDraw(*renderElement))
            continue;

        auto concreteCursorSize = styleImage->negotiate(*renderElement, CursorSizing { styleImage->imageScaleFactor() });
        auto cursorSize = concreteCursorSize.size();
        if (cursorSize.isEmpty())
            continue;

        // Limit the size of cursors (in UI pixels) so that they cannot be used to cover UI elements in chrome.
        if (cursorSize.width() > CursorSizing::maximumCursorSize.width() || cursorSize.height() > CursorSizing::maximumCursorSize.height())
            continue;

        auto cursorRect = IntRect { pointInMainFrame, expandedIntSize(cursorSize) };
        auto hotSpot = styleCursorImage.hotSpot ? Style::evaluate<IntPoint>(*styleCursorImage.hotSpot) : IntPoint { -1, -1 };
        cursorRect.moveBy(-hotSpot);

        if (!visibleContentRect.contains(cursorRect))
            continue;

        ImageDrawingOptions options {
#if ENABLE(AX_CUSTOM_COLOR_MODE)
            AXCustomColorModeController::shouldInvertSVGImage(*renderElement, styleImage.get()) ? InvertContent::Yes : InvertContent::No,
#endif
        };

        RefPtr nativeImage = styleImage->nativeImage(*renderElement, concreteCursorSize, deviceScaleFactor, options);
        if (!nativeImage || nativeImage->size().isEmpty())
            continue;

        // Image pixels per UI pixel.
        float scale = nativeImage->size().width() / cursorSize.width();

        std::optional<IntPoint> specifiedHotSpot;
        if (styleCursorImage.hotSpot)
            specifiedHotSpot = roundedIntPoint(FloatPoint { hotSpot }.scaled(scale));
        auto effectiveHotSpot = determineHotSpot(nativeImage->size(), specifiedHotSpot, styleImage->hotSpot());

#if ENABLE(MOUSE_CURSOR_SCALE)
        // It's pretty unlikely that a scale of less than one would ever be used. But all we really
        // need to ensure here is that the scale isn't so small that integer overflow can occur when
        // dividing cursor sizes (limited above) by the scale.
        constexpr double minimumCursorScale = 0.001;
        if (scale < minimumCursorScale)
            continue;
        return WebCore::Cursor(WTF::move(nativeImage), effectiveHotSpot, scale);
#else
        return WebCore::Cursor(WTF::move(nativeImage), effectiveHotSpot);
#endif // ENABLE(MOUSE_CURSOR_SCALE)
    }

    return std::nullopt;
}

// MARK: - Conversion

auto CSSValueConversion<Cursor>::operator()(BuilderState& state, const CSSValue& value) -> Cursor
{
    if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value))
        return toStyleFromCSSValue<CursorType>(state, *keywordValue);

    auto list = requiredListDowncast<CSSValueList, CSSValue, 2>(state, value);
    if (!list)
        return CSS::Keyword::Auto { };

    auto images = CursorImageList::createWithSizeFromGenerator(list->size() - 1, [&](auto index) {
        Ref item = list->item(index);
        RefPtr image = requiredDowncast<CSSCursorImageValue>(state, item);
        if (!image)
            return CursorImageAndHotSpot { InvalidImage::create(), std::nullopt };

        auto styleImage = image->createStyleImage(state);
        if (!styleImage) {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CursorImageAndHotSpot { InvalidImage::create(), std::nullopt };
        }

        auto hotSpot = styleImage->specifiedHotSpot();
        return CursorImageAndHotSpot { styleImage.releaseNonNull(), WTF::move(hotSpot) };
    });

    return { WTF::move(images), toStyleFromCSSValue<CursorType>(state, protect(list->item(list->size() - 1))) };
}

Ref<CSSValue> CSSValueCreation<CursorImageAndHotSpot>::operator()(CSSValuePool&, const Style::ComputedStyle& style, const CursorImageAndHotSpot& value)
{
    Ref image = value.image;
    return image->computedStyleValue(style);
}

// MARK: - Serialization

void Serialize<CursorImageAndHotSpot>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const Style::ComputedStyle& style, const CursorImageAndHotSpot& value)
{
    Ref image = value.image;
    Ref computedValue = image->computedStyleValue(style);
    builder.append(computedValue->cssText(context));
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const CursorImageAndHotSpot& value)
{
    return ts << "cursor image with hotspot " << value.hotSpot;
}

} // namespace Style
} // namespace WebCore
