/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
#include "DisplayListItemType.h"

#include "DisplayListItems.h"

namespace WebCore {
namespace DisplayList {

static size_t sizeOfItemInBytes(ItemType type)
{
    switch (type) {
    case ItemType::Save:
        return sizeof(Save);
    case ItemType::Restore:
        return sizeof(Restore);
    case ItemType::Translate:
        return sizeof(Translate);
    case ItemType::Rotate:
        return sizeof(Rotate);
    case ItemType::Scale:
        return sizeof(Scale);
    case ItemType::ConcatenateCTM:
        return sizeof(ConcatenateCTM);
    case ItemType::SetCTM:
        return sizeof(SetCTM);
    case ItemType::SetInlineFillColor:
        return sizeof(SetInlineFillColor);
    case ItemType::SetInlineStrokeColor:
        return sizeof(SetInlineStrokeColor);
    case ItemType::SetStrokeThickness:
        return sizeof(SetStrokeThickness);
    case ItemType::SetState:
        return sizeof(SetState);
    case ItemType::SetLineCap:
        return sizeof(SetLineCap);
    case ItemType::SetLineDash:
        return sizeof(SetLineDash);
    case ItemType::SetLineJoin:
        return sizeof(SetLineJoin);
    case ItemType::SetMiterLimit:
        return sizeof(SetMiterLimit);
    case ItemType::ClearShadow:
        return sizeof(ClearShadow);
    case ItemType::Clip:
        return sizeof(Clip);
    case ItemType::ClipOut:
        return sizeof(ClipOut);
    case ItemType::ClipToImageBuffer:
        return sizeof(ClipToImageBuffer);
    case ItemType::ClipOutToPath:
        return sizeof(ClipOutToPath);
    case ItemType::ClipPath:
        return sizeof(ClipPath);
    case ItemType::DrawFilteredImageBuffer:
        return sizeof(DrawFilteredImageBuffer);
    case ItemType::DrawGlyphs:
        return sizeof(DrawGlyphs);
    case ItemType::DrawDecomposedGlyphs:
        return sizeof(DrawDecomposedGlyphs);
    case ItemType::DrawImageBuffer:
        return sizeof(DrawImageBuffer);
    case ItemType::DrawNativeImage:
        return sizeof(DrawNativeImage);
    case ItemType::DrawSystemImage:
        return sizeof(DrawSystemImage);
    case ItemType::DrawPattern:
        return sizeof(DrawPattern);
    case ItemType::DrawRect:
        return sizeof(DrawRect);
    case ItemType::DrawLine:
        return sizeof(DrawLine);
    case ItemType::DrawLinesForText:
        return sizeof(DrawLinesForText);
    case ItemType::DrawDotsForDocumentMarker:
        return sizeof(DrawDotsForDocumentMarker);
    case ItemType::DrawEllipse:
        return sizeof(DrawEllipse);
    case ItemType::DrawPath:
        return sizeof(DrawPath);
    case ItemType::DrawFocusRingPath:
        return sizeof(DrawFocusRingPath);
    case ItemType::DrawFocusRingRects:
        return sizeof(DrawFocusRingRects);
    case ItemType::FillRect:
        return sizeof(FillRect);
    case ItemType::FillRectWithColor:
        return sizeof(FillRectWithColor);
    case ItemType::FillRectWithGradient:
        return sizeof(FillRectWithGradient);
    case ItemType::FillCompositedRect:
        return sizeof(FillCompositedRect);
    case ItemType::FillRoundedRect:
        return sizeof(FillRoundedRect);
    case ItemType::FillRectWithRoundedHole:
        return sizeof(FillRectWithRoundedHole);
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::FillLine:
        return sizeof(FillLine);
    case ItemType::FillArc:
        return sizeof(FillArc);
    case ItemType::FillQuadCurve:
        return sizeof(FillQuadCurve);
    case ItemType::FillBezierCurve:
        return sizeof(FillBezierCurve);
#endif
    case ItemType::FillPath:
        return sizeof(FillPath);
    case ItemType::FillEllipse:
        return sizeof(FillEllipse);
#if ENABLE(VIDEO)
    case ItemType::PaintFrameForMedia:
        return sizeof(PaintFrameForMedia);
#endif
    case ItemType::StrokeRect:
        return sizeof(StrokeRect);
    case ItemType::StrokeLine:
        return sizeof(StrokeLine);
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::StrokeArc:
        return sizeof(StrokeArc);
    case ItemType::StrokeQuadCurve:
        return sizeof(StrokeQuadCurve);
    case ItemType::StrokeBezierCurve:
        return sizeof(StrokeBezierCurve);
#endif
    case ItemType::StrokePath:
        return sizeof(StrokePath);
    case ItemType::StrokeEllipse:
        return sizeof(StrokeEllipse);
    case ItemType::ClearRect:
        return sizeof(ClearRect);
    case ItemType::DrawControlPart:
        return sizeof(DrawControlPart);
    case ItemType::BeginTransparencyLayer:
        return sizeof(BeginTransparencyLayer);
    case ItemType::EndTransparencyLayer:
        return sizeof(EndTransparencyLayer);
#if USE(CG)
    case ItemType::ApplyStrokePattern:
        return sizeof(ApplyStrokePattern);
    case ItemType::ApplyFillPattern:
        return sizeof(ApplyFillPattern);
#endif
    case ItemType::ApplyDeviceScaleFactor:
        return sizeof(ApplyDeviceScaleFactor);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

bool isDrawingItem(ItemType type)
{
    /* See the comment at the top of DisplayListItems.h for what this means.
     * This needs to match all the "static constexpr bool isDrawingItem"s inside the individual item classes. */

    switch (type) {
    case ItemType::ApplyDeviceScaleFactor:
#if USE(CG)
    case ItemType::ApplyFillPattern:
    case ItemType::ApplyStrokePattern:
#endif
    case ItemType::ClearShadow:
    case ItemType::Clip:
    case ItemType::ClipOut:
    case ItemType::ClipToImageBuffer:
    case ItemType::ClipOutToPath:
    case ItemType::ClipPath:
    case ItemType::ConcatenateCTM:
    case ItemType::Restore:
    case ItemType::Rotate:
    case ItemType::Save:
    case ItemType::Scale:
    case ItemType::SetCTM:
    case ItemType::SetInlineFillColor:
    case ItemType::SetInlineStrokeColor:
    case ItemType::SetLineCap:
    case ItemType::SetLineDash:
    case ItemType::SetLineJoin:
    case ItemType::SetMiterLimit:
    case ItemType::SetState:
    case ItemType::SetStrokeThickness:
    case ItemType::Translate:
        return false;
    case ItemType::BeginTransparencyLayer:
    case ItemType::ClearRect:
    case ItemType::DrawControlPart:
    case ItemType::DrawDotsForDocumentMarker:
    case ItemType::DrawEllipse:
    case ItemType::DrawFilteredImageBuffer:
    case ItemType::DrawFocusRingPath:
    case ItemType::DrawFocusRingRects:
    case ItemType::DrawGlyphs:
    case ItemType::DrawDecomposedGlyphs:
    case ItemType::DrawImageBuffer:
    case ItemType::DrawLine:
    case ItemType::DrawLinesForText:
    case ItemType::DrawNativeImage:
    case ItemType::DrawSystemImage:
    case ItemType::DrawPattern:
    case ItemType::DrawPath:
    case ItemType::DrawRect:
    case ItemType::EndTransparencyLayer:
    case ItemType::FillCompositedRect:
    case ItemType::FillEllipse:
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::FillLine:
    case ItemType::FillArc:
    case ItemType::FillQuadCurve:
    case ItemType::FillBezierCurve:
#endif
    case ItemType::FillPath:
    case ItemType::FillRect:
    case ItemType::FillRectWithColor:
    case ItemType::FillRectWithGradient:
    case ItemType::FillRectWithRoundedHole:
    case ItemType::FillRoundedRect:
#if ENABLE(VIDEO)
    case ItemType::PaintFrameForMedia:
#endif
    case ItemType::StrokeEllipse:
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::StrokeArc:
    case ItemType::StrokeQuadCurve:
    case ItemType::StrokeBezierCurve:
#endif
    case ItemType::StrokePath:
    case ItemType::StrokeRect:
    case ItemType::StrokeLine:
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

size_t paddedSizeOfTypeAndItemInBytes(ItemType type)
{
    return sizeof(uint64_t) + roundUpToMultipleOf(alignof(uint64_t), sizeOfItemInBytes(type));
}

size_t paddedSizeOfTypeAndItemInBytes(const DisplayListItem& displayListItem)
{
    auto itemSize = std::visit([](const auto& item) {
        return sizeof(item);
    }, displayListItem);
    return sizeof(uint64_t) + roundUpToMultipleOf(alignof(uint64_t), itemSize);
}

ItemType displayListItemType(const DisplayListItem& displayListItem)
{
    return std::visit([](const auto& item) {
        return item.itemType;
    }, displayListItem);
}

bool isInlineItem(ItemType type)
{
    /* See the comment at the top of DisplayListItems.h for what this means.
     * This needs to match (1) RemoteImageBufferProxy::encodeItem(), (2) RemoteRenderingBackend::decodeItem(),
     * and (3) all the "static constexpr bool isInlineItem"s inside the individual item classes. */

    switch (type) {
    case ItemType::ClipOutToPath:
    case ItemType::ClipPath:
    case ItemType::DrawControlPart:
    case ItemType::DrawFocusRingPath:
    case ItemType::DrawFocusRingRects:
    case ItemType::DrawGlyphs:
    case ItemType::DrawLinesForText:
    case ItemType::DrawPath:
    case ItemType::FillCompositedRect:
    case ItemType::FillPath:
    case ItemType::FillRectWithColor:
    case ItemType::FillRectWithGradient:
    case ItemType::FillRectWithRoundedHole:
    case ItemType::FillRoundedRect:
    case ItemType::SetLineDash:
    case ItemType::SetState:
    case ItemType::StrokePath:
    case ItemType::DrawSystemImage:
        return false;
    case ItemType::ApplyDeviceScaleFactor:
#if USE(CG)
    case ItemType::ApplyFillPattern:
    case ItemType::ApplyStrokePattern:
#endif
    case ItemType::BeginTransparencyLayer:
    case ItemType::ClearRect:
    case ItemType::ClearShadow:
    case ItemType::Clip:
    case ItemType::ClipOut:
    case ItemType::ClipToImageBuffer:
    case ItemType::ConcatenateCTM:
    case ItemType::DrawDotsForDocumentMarker:
    case ItemType::DrawEllipse:
    case ItemType::DrawFilteredImageBuffer:
    case ItemType::DrawDecomposedGlyphs:
    case ItemType::DrawImageBuffer:
    case ItemType::DrawNativeImage:
    case ItemType::DrawPattern:
    case ItemType::DrawLine:
    case ItemType::DrawRect:
    case ItemType::EndTransparencyLayer:
    case ItemType::FillEllipse:
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::FillLine:
    case ItemType::FillArc:
    case ItemType::FillQuadCurve:
    case ItemType::FillBezierCurve:
#endif
    case ItemType::FillRect:
#if ENABLE(VIDEO)
    case ItemType::PaintFrameForMedia:
#endif
    case ItemType::Restore:
    case ItemType::Rotate:
    case ItemType::Save:
    case ItemType::Scale:
    case ItemType::SetCTM:
    case ItemType::SetInlineFillColor:
    case ItemType::SetInlineStrokeColor:
    case ItemType::SetLineCap:
    case ItemType::SetLineJoin:
    case ItemType::SetMiterLimit:
    case ItemType::SetStrokeThickness:
    case ItemType::StrokeEllipse:
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::StrokeArc:
    case ItemType::StrokeQuadCurve:
    case ItemType::StrokeBezierCurve:
#endif
    case ItemType::StrokeRect:
    case ItemType::StrokeLine:
    case ItemType::Translate:
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

} // namespace DisplayList
} // namespace WebCore
