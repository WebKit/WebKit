/*
 * Copyright (C) 2026 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AdwaitaScrollbarPainter.h"

#if USE(THEME_ADWAITA)

#include "GraphicsContext.h"

namespace WebCore::AdwaitaScrollbarPainter {

void paint(GraphicsContext& graphicsContext, const IntRect& damageRect, const State& scrollbar)
{
    if (graphicsContext.paintingDisabled())
        return;

    if (!scrollbar.enabled && scrollbar.usesOverlayScrollbars)
        return;

    IntRect rect = scrollbar.frameRect;
    if (!rect.intersects(damageRect))
        return;

    double opacity;
    if (scrollbar.usesOverlayScrollbars)
        opacity = scrollbar.opacity;
    else
        opacity = 1;
    if (!opacity)
        return;

    SRGBA<uint8_t> scrollbarBackgroundColor;
    SRGBA<uint8_t> scrollbarBorderColor;
    SRGBA<uint8_t> overlayThumbBorderColor;
    SRGBA<uint8_t> overlayTroughBorderColor;
    SRGBA<uint8_t> overlayTroughColor;
    SRGBA<uint8_t> thumbHoveredColor;
    SRGBA<uint8_t> thumbPressedColor;
    SRGBA<uint8_t> thumbColor;

    if (scrollbar.useDarkAppearanceForScrollbars) {
        scrollbarBackgroundColor = scrollbarBackgroundColorDark;
        scrollbarBorderColor = scrollbarBorderColorDark;
        overlayThumbBorderColor = overlayThumbBorderColorDark;
        overlayTroughBorderColor = overlayThumbBorderColorDark;
        overlayTroughColor = overlayTroughColorDark;
        thumbHoveredColor = thumbHoveredColorDark;
        thumbPressedColor = thumbPressedColorDark;
        thumbColor = thumbColorDark;
    } else {
        scrollbarBackgroundColor = scrollbarBackgroundColorLight;
        scrollbarBorderColor = scrollbarBorderColorLight;
        overlayThumbBorderColor = overlayThumbBorderColorLight;
        overlayTroughBorderColor = overlayThumbBorderColorLight;
        overlayTroughColor = overlayTroughColorLight;
        thumbHoveredColor = thumbHoveredColorLight;
        thumbPressedColor = thumbPressedColorLight;
        thumbColor = thumbColorLight;
    }

    GraphicsContextStateSaver stateSaver(graphicsContext);
    if (opacity != 1) {
        graphicsContext.clip(damageRect);
        graphicsContext.beginTransparencyLayer(opacity);
    }

    int thumbSize = scrollbarSize - scrollbarBorderSize - horizThumbMargin * 2;

    if (!scrollbar.usesOverlayScrollbars) {
        graphicsContext.fillRect(rect, scrollbarBackgroundColor);

        IntRect frame = rect;
        if (scrollbar.orientation == ScrollbarOrientation::Vertical) {
            if (scrollbar.shouldPlaceVerticalScrollbarOnLeft)
                frame.move(frame.width() - scrollbarBorderSize, 0);
            frame.setWidth(scrollbarBorderSize);
        } else
            frame.setHeight(scrollbarBorderSize);
        graphicsContext.fillRect(frame, scrollbarBorderColor);
    } else if (scrollbar.hoveredPart != NoPart) {
        int thumbCornerSize = thumbSize / 2;
        FloatSize corner(thumbCornerSize, thumbCornerSize);
        FloatSize borderCorner(thumbCornerSize + thumbBorderSize, thumbCornerSize + thumbBorderSize);

        IntRect trough = rect;
        if (scrollbar.orientation == ScrollbarOrientation::Vertical) {
            if (scrollbar.shouldPlaceVerticalScrollbarOnLeft)
                trough.move(scrollbarSize - (scrollbarSize / 2 + thumbSize / 2) - scrollbarBorderSize, vertThumbMargin);
            else
                trough.move(scrollbarSize - (scrollbarSize / 2 + thumbSize / 2), vertThumbMargin);
            trough.setWidth(thumbSize);
            trough.setHeight(rect.height() - vertThumbMargin * 2);
        } else {
            trough.move(vertThumbMargin, scrollbarSize - (scrollbarSize / 2 + thumbSize / 2));
            trough.setWidth(rect.width() - vertThumbMargin * 2);
            trough.setHeight(thumbSize);
        }

        IntRect troughBorder(trough);
        troughBorder.inflate(thumbBorderSize);

        Path path;
        path.addRoundedRect(trough, corner);
        graphicsContext.setFillRule(WindRule::NonZero);
        graphicsContext.setFillColor(overlayTroughColor);
        graphicsContext.fillPath(path);
        path.clear();

        path.addRoundedRect(trough, corner);
        path.addRoundedRect(troughBorder, borderCorner);
        graphicsContext.setFillRule(WindRule::EvenOdd);
        graphicsContext.setFillColor(overlayTroughBorderColor);
        graphicsContext.fillPath(path);
    }

    int thumbCornerSize;
    int thumbPos = scrollbar.thumbPosition;
    int thumbLen = scrollbar.thumbLength;
    IntRect thumb = rect;
    if (scrollbar.hoveredPart == NoPart && scrollbar.usesOverlayScrollbars) {
        thumbCornerSize = overlayThumbSize / 2;

        if (scrollbar.orientation == ScrollbarOrientation::Vertical) {
            if (scrollbar.shouldPlaceVerticalScrollbarOnLeft)
                thumb.move(horizOverlayThumbMargin, thumbPos + vertThumbMargin);
            else
                thumb.move(scrollbarSize - overlayThumbSize - horizOverlayThumbMargin, thumbPos + vertThumbMargin);
            thumb.setWidth(overlayThumbSize);
            thumb.setHeight(thumbLen - vertThumbMargin * 2);
        } else {
            thumb.move(thumbPos + vertThumbMargin, scrollbarSize - overlayThumbSize - horizOverlayThumbMargin);
            thumb.setWidth(thumbLen - vertThumbMargin * 2);
            thumb.setHeight(overlayThumbSize);
        }
    } else {
        thumbCornerSize = thumbSize / 2;

        if (scrollbar.orientation == ScrollbarOrientation::Vertical) {
            if (scrollbar.shouldPlaceVerticalScrollbarOnLeft)
                thumb.move(scrollbarSize - (scrollbarSize / 2 + thumbSize / 2) - scrollbarBorderSize, thumbPos + vertThumbMargin);
            else
                thumb.move(scrollbarSize - (scrollbarSize / 2 + thumbSize / 2), thumbPos + vertThumbMargin);
            thumb.setWidth(thumbSize);
            thumb.setHeight(thumbLen - vertThumbMargin * 2);
        } else {
            thumb.move(thumbPos + vertThumbMargin, scrollbarSize - (scrollbarSize / 2 + thumbSize / 2));
            thumb.setWidth(thumbLen - vertThumbMargin * 2);
            thumb.setHeight(thumbSize);
        }
    }

    FloatSize corner(thumbCornerSize, thumbCornerSize);
    FloatSize borderCorner(thumbCornerSize + thumbBorderSize, thumbCornerSize + thumbBorderSize);

    Path path;

    path.addRoundedRect(thumb, corner);
    graphicsContext.setFillRule(WindRule::NonZero);
    if (scrollbar.pressedPart == ThumbPart)
        graphicsContext.setFillColor(thumbPressedColor);
    else if (scrollbar.hoveredPart == ThumbPart)
        graphicsContext.setFillColor(thumbHoveredColor);
    else
        graphicsContext.setFillColor(thumbColor);
    graphicsContext.fillPath(path);
    path.clear();

    if (scrollbar.usesOverlayScrollbars) {
        IntRect thumbBorder(thumb);
        thumbBorder.inflate(thumbBorderSize);

        path.addRoundedRect(thumb, corner);
        path.addRoundedRect(thumbBorder, borderCorner);
        graphicsContext.setFillRule(WindRule::EvenOdd);
        graphicsContext.setFillColor(overlayThumbBorderColor);
        graphicsContext.fillPath(path);
    }

    if (opacity != 1)
        graphicsContext.endTransparencyLayer();
}

} // namespace WebCore::AdwaitaScrollbarPainter

#endif // USE(THEME_ADWAITA)
