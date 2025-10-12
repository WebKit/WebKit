/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/OptionSet.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class ControlFactory;
class GraphicsContext;

namespace DisplayList {

class ApplyDeviceScaleFactor;
class BeginTransparencyLayer;
class BeginTransparencyLayerWithCompositeMode;
class ClearRect;
class Clip;
class ClipRoundedRect;
class ClipOut;
class ClipOutRoundedRect;
class ClipOutToPath;
class ClipPath;
class ClipToImageBuffer;
class ConcatenateCTM;
class DrawControlPart;
class DrawDotsForDocumentMarker;
class DrawEllipse;
class DrawFilteredImageBuffer;
class DrawFocusRingPath;
class DrawFocusRingRects;
class DrawGlyphs;
class DrawDisplayList;
class DrawPlaceholder;
class DrawImageBuffer;
class DrawLine;
class DrawLinesForText;
class DrawNativeImage;
class DrawPath;
class DrawPatternNativeImage;
class DrawPatternImageBuffer;
class DrawRect;
class DrawSystemImage;
class EndTransparencyLayer;
class FillCompositedRect;
class FillEllipse;
class FillPath;
class FillRect;
class FillRectWithColor;
class FillRectWithGradient;
class FillRectWithGradientAndSpaceTransform;
class FillRectWithRoundedHole;
class FillRoundedRect;
class ResetClip;
class Restore;
class Rotate;
class Save;
class Scale;
class SetCTM;
class SetInlineFillColor;
class SetInlineStroke;
class SetLineCap;
class SetLineDash;
class SetLineJoin;
class SetMiterLimit;
class SetState;
class StrokeEllipse;
class StrokePath;
class StrokeRect;
class Translate;
#if USE(CG)
class ApplyFillPattern;
class ApplyStrokePattern;
#endif
class BeginPage;
class EndPage;
class SetURLForRect;
#if USE(SKIA)
class DrawTextBlob;
#endif

using Item = Variant
    < ApplyDeviceScaleFactor
    , BeginTransparencyLayer
    , BeginTransparencyLayerWithCompositeMode
    , ClearRect
    , Clip
    , ClipRoundedRect
    , ClipOut
    , ClipOutRoundedRect
    , ClipOutToPath
    , ClipPath
    , ClipToImageBuffer
    , ConcatenateCTM
    , DrawControlPart
    , DrawDotsForDocumentMarker
    , DrawEllipse
    , DrawFilteredImageBuffer
    , DrawFocusRingPath
    , DrawFocusRingRects
    , DrawGlyphs
    , DrawDisplayList
    , DrawPlaceholder
    , DrawImageBuffer
    , DrawLine
    , DrawLinesForText
    , DrawNativeImage
    , DrawPath
    , DrawPatternNativeImage
    , DrawPatternImageBuffer
    , DrawRect
    , DrawSystemImage
    , EndTransparencyLayer
    , FillCompositedRect
    , FillEllipse
    , FillPath
    , FillRect
    , FillRectWithColor
    , FillRectWithGradient
    , FillRectWithGradientAndSpaceTransform
    , FillRectWithRoundedHole
    , FillRoundedRect
    , ResetClip
    , Restore
    , Rotate
    , Save
    , Scale
    , SetCTM
    , SetInlineFillColor
    , SetInlineStroke
    , SetLineCap
    , SetLineDash
    , SetLineJoin
    , SetMiterLimit
    , SetState
    , StrokeEllipse
    , StrokePath
    , StrokeRect
    , Translate
#if USE(CG)
    , ApplyFillPattern
    , ApplyStrokePattern
#endif
    , BeginPage
    , EndPage
    , SetURLForRect
#if USE(SKIA)
    , DrawTextBlob
#endif
>;

enum class AsTextFlag : uint8_t {
    IncludePlatformOperations      = 1 << 0,
    IncludeResourceIdentifiers     = 1 << 1,
};

void applyItem(GraphicsContext&, ControlFactory&, const Item&);

bool shouldDumpItem(const Item&, OptionSet<AsTextFlag>);

WEBCORE_EXPORT void dumpItem(TextStream&, const Item&, OptionSet<AsTextFlag>);

WEBCORE_EXPORT TextStream& operator<<(TextStream&, const Item&);

} // namespace DisplayList
} // namespace WebCore
