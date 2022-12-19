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

#pragma once

namespace WebCore {
namespace DisplayList {

enum class AsTextFlag : uint8_t {
    IncludePlatformOperations      = 1 << 0,
    IncludeResourceIdentifiers     = 1 << 1,
};

enum class ItemType : uint8_t {
    Save,
    Restore,
    Translate,
    Rotate,
    Scale,
    ConcatenateCTM,
    SetCTM,
    SetInlineFillColor,
    SetInlineStrokeColor,
    SetStrokeThickness,
    SetState,
    SetLineCap,
    SetLineDash,
    SetLineJoin,
    SetMiterLimit,
    ClearShadow,
    Clip,
    ClipOut,
    ClipToImageBuffer,
    ClipOutToPath,
    ClipPath,
    DrawControlPart,
    DrawFilteredImageBuffer,
    DrawGlyphs,
    DrawDecomposedGlyphs,
    DrawImageBuffer,
    DrawNativeImage,
    DrawSystemImage,
    DrawPattern,
    DrawRect,
    DrawLine,
    DrawLinesForText,
    DrawDotsForDocumentMarker,
    DrawEllipse,
    DrawPath,
    DrawFocusRingPath,
    DrawFocusRingRects,
    FillRect,
    FillRectWithColor,
    FillRectWithGradient,
    FillCompositedRect,
    FillRoundedRect,
    FillRectWithRoundedHole,
#if ENABLE(INLINE_PATH_DATA)
    FillLine,
    FillArc,
    FillQuadCurve,
    FillBezierCurve,
#endif
    FillPath,
    FillEllipse,
#if ENABLE(VIDEO)
    PaintFrameForMedia,
#endif
    StrokeRect,
    StrokeLine,
#if ENABLE(INLINE_PATH_DATA)
    StrokeArc,
    StrokeQuadCurve,
    StrokeBezierCurve,
#endif
    StrokePath,
    StrokeEllipse,
    ClearRect,
    BeginTransparencyLayer,
    EndTransparencyLayer,
#if USE(CG)
    ApplyStrokePattern, // FIXME: should not be a recorded item.
    ApplyFillPattern, // FIXME: should not be a recorded item.
#endif
    ApplyDeviceScaleFactor,
};

WEBCORE_EXPORT size_t paddedSizeOfTypeAndItemInBytes(ItemType);
WEBCORE_EXPORT bool isInlineItem(ItemType);
WEBCORE_EXPORT bool isDrawingItem(ItemType);

} // namespace DisplayList
} // namespace WebCore
