/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "DisplayListItemBuffer.h"

#include "DisplayListItems.h"
#include <wtf/FastMalloc.h>

namespace WebCore {
namespace DisplayList {

#if !defined(NDEBUG) || !LOG_DISABLED
CString ItemHandle::description() const
{
    TextStream ts;
    ts << *this;
    return ts.release().utf8();
}
#endif

void ItemHandle::apply(GraphicsContext& context)
{
    switch (type()) {
    case ItemType::Save: {
        get<Save>().apply(context);
        return;
    }
    case ItemType::Restore: {
        get<Restore>().apply(context);
        return;
    }
    case ItemType::Translate: {
        get<Translate>().apply(context);
        return;
    }
    case ItemType::Rotate: {
        get<Rotate>().apply(context);
        return;
    }
    case ItemType::Scale: {
        get<Scale>().apply(context);
        return;
    }
    case ItemType::ConcatenateCTM: {
        get<ConcatenateCTM>().apply(context);
        return;
    }
    case ItemType::SetCTM: {
        get<SetCTM>().apply(context);
        return;
    }
    case ItemType::SetInlineFillGradient: {
        get<SetInlineFillGradient>().apply(context);
        return;
    }
    case ItemType::SetInlineFillColor: {
        get<SetInlineFillColor>().apply(context);
        return;
    }
    case ItemType::SetInlineStrokeColor: {
        get<SetInlineStrokeColor>().apply(context);
        return;
    }
    case ItemType::SetStrokeThickness: {
        get<SetStrokeThickness>().apply(context);
        return;
    }
    case ItemType::SetState: {
        get<SetState>().apply(context);
        return;
    }
    case ItemType::SetLineCap: {
        get<SetLineCap>().apply(context);
        return;
    }
    case ItemType::SetLineDash: {
        get<SetLineDash>().apply(context);
        return;
    }
    case ItemType::SetLineJoin: {
        get<SetLineJoin>().apply(context);
        return;
    }
    case ItemType::SetMiterLimit: {
        get<SetMiterLimit>().apply(context);
        return;
    }
    case ItemType::ClearShadow: {
        get<ClearShadow>().apply(context);
        return;
    }
    case ItemType::Clip: {
        get<Clip>().apply(context);
        return;
    }
    case ItemType::ClipOut: {
        get<ClipOut>().apply(context);
        return;
    }
    case ItemType::ClipOutToPath: {
        get<ClipOutToPath>().apply(context);
        return;
    }
    case ItemType::ClipPath: {
        get<ClipPath>().apply(context);
        return;
    }
    case ItemType::ClipToDrawingCommands: {
        get<ClipToDrawingCommands>().apply(context);
        return;
    }
    case ItemType::DrawGlyphs: {
        get<DrawGlyphs>().apply(context);
        return;
    }
    case ItemType::DrawImage: {
        get<DrawImage>().apply(context);
        return;
    }
    case ItemType::DrawTiledImage: {
        get<DrawTiledImage>().apply(context);
        return;
    }
    case ItemType::DrawTiledScaledImage: {
        get<DrawTiledScaledImage>().apply(context);
        return;
    }
    case ItemType::DrawImageBuffer: {
        get<DrawImageBuffer>().apply(context);
        return;
    }
    case ItemType::DrawNativeImage: {
        get<DrawNativeImage>().apply(context);
        return;
    }
    case ItemType::DrawPattern: {
        get<DrawPattern>().apply(context);
        return;
    }
    case ItemType::DrawRect: {
        get<DrawRect>().apply(context);
        return;
    }
    case ItemType::DrawLine: {
        get<DrawLine>().apply(context);
        return;
    }
    case ItemType::DrawLinesForText: {
        get<DrawLinesForText>().apply(context);
        return;
    }
    case ItemType::DrawDotsForDocumentMarker: {
        get<DrawDotsForDocumentMarker>().apply(context);
        return;
    }
    case ItemType::DrawEllipse: {
        get<DrawEllipse>().apply(context);
        return;
    }
    case ItemType::DrawPath: {
        get<DrawPath>().apply(context);
        return;
    }
    case ItemType::DrawFocusRingPath: {
        get<DrawFocusRingPath>().apply(context);
        return;
    }
    case ItemType::DrawFocusRingRects: {
        get<DrawFocusRingRects>().apply(context);
        return;
    }
    case ItemType::FillRect: {
        get<FillRect>().apply(context);
        return;
    }
    case ItemType::FillRectWithColor: {
        get<FillRectWithColor>().apply(context);
        return;
    }
    case ItemType::FillRectWithGradient: {
        get<FillRectWithGradient>().apply(context);
        return;
    }
    case ItemType::FillCompositedRect: {
        get<FillCompositedRect>().apply(context);
        return;
    }
    case ItemType::FillRoundedRect: {
        get<FillRoundedRect>().apply(context);
        return;
    }
    case ItemType::FillRectWithRoundedHole: {
        get<FillRectWithRoundedHole>().apply(context);
        return;
    }
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::FillInlinePath: {
        get<FillInlinePath>().apply(context);
        return;
    }
#endif
    case ItemType::FillPath: {
        get<FillPath>().apply(context);
        return;
    }
    case ItemType::FillEllipse: {
        get<FillEllipse>().apply(context);
        return;
    }
    case ItemType::PutImageData: {
        get<PutImageData>().apply(context);
        return;
    }
    case ItemType::PaintFrameForMedia: {
        get<PaintFrameForMedia>().apply(context);
        return;
    }
    case ItemType::StrokeRect: {
        get<StrokeRect>().apply(context);
        return;
    }
    case ItemType::StrokeLine: {
        get<StrokeLine>().apply(context);
        return;
    }
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::StrokeInlinePath: {
        get<StrokeInlinePath>().apply(context);
        return;
    }
#endif
    case ItemType::StrokePath: {
        get<StrokePath>().apply(context);
        return;
    }
    case ItemType::StrokeEllipse: {
        get<StrokeEllipse>().apply(context);
        return;
    }
    case ItemType::ClearRect: {
        get<ClearRect>().apply(context);
        return;
    }
    case ItemType::BeginTransparencyLayer: {
        get<BeginTransparencyLayer>().apply(context);
        return;
    }
    case ItemType::EndTransparencyLayer: {
        get<EndTransparencyLayer>().apply(context);
        return;
    }
#if USE(CG)
    case ItemType::ApplyStrokePattern: {
        get<ApplyStrokePattern>().apply(context);
        return;
    }
    case ItemType::ApplyFillPattern: {
        get<ApplyFillPattern>().apply(context);
        return;
    }
#endif
    case ItemType::ApplyDeviceScaleFactor: {
        get<ApplyDeviceScaleFactor>().apply(context);
        return;
    }
    }
}

void ItemHandle::destroy()
{
    switch (type()) {
    case ItemType::ClipOutToPath: {
        get<ClipOutToPath>().~ClipOutToPath();
        return;
    }
    case ItemType::ClipPath: {
        get<ClipPath>().~ClipPath();
        return;
    }
    case ItemType::ClipToDrawingCommands: {
        get<ClipToDrawingCommands>().~ClipToDrawingCommands();
        return;
    }
    case ItemType::DrawFocusRingPath: {
        get<DrawFocusRingPath>().~DrawFocusRingPath();
        return;
    }
    case ItemType::DrawFocusRingRects: {
        get<DrawFocusRingRects>().~DrawFocusRingRects();
        return;
    }
    case ItemType::DrawGlyphs: {
        get<DrawGlyphs>().~DrawGlyphs();
        return;
    }
    case ItemType::DrawImage: {
        get<DrawImage>().~DrawImage();
        return;
    }
    case ItemType::DrawLinesForText: {
        get<DrawLinesForText>().~DrawLinesForText();
        return;
    }
    case ItemType::DrawNativeImage: {
        get<DrawNativeImage>().~DrawNativeImage();
        return;
    }
    case ItemType::DrawPath: {
        get<DrawPath>().~DrawPath();
        return;
    }
    case ItemType::DrawPattern: {
        get<DrawPattern>().~DrawPattern();
        return;
    }
    case ItemType::DrawTiledImage: {
        get<DrawTiledImage>().~DrawTiledImage();
        return;
    }
    case ItemType::DrawTiledScaledImage: {
        get<DrawTiledScaledImage>().~DrawTiledScaledImage();
        return;
    }
    case ItemType::FillCompositedRect: {
        get<FillCompositedRect>().~FillCompositedRect();
        return;
    }
    case ItemType::FillPath: {
        get<FillPath>().~FillPath();
        return;
    }
    case ItemType::FillRectWithColor: {
        get<FillRectWithColor>().~FillRectWithColor();
        return;
    }
    case ItemType::FillRectWithGradient: {
        get<FillRectWithGradient>().~FillRectWithGradient();
        return;
    }
    case ItemType::FillRectWithRoundedHole: {
        get<FillRectWithRoundedHole>().~FillRectWithRoundedHole();
        return;
    }
    case ItemType::FillRoundedRect: {
        get<FillRoundedRect>().~FillRoundedRect();
        return;
    }
    case ItemType::PutImageData: {
        get<PutImageData>().~PutImageData();
        return;
    }
    case ItemType::SetLineDash: {
        get<SetLineDash>().~SetLineDash();
        return;
    }
    case ItemType::SetState: {
        get<SetState>().~SetState();
        return;
    }
    case ItemType::StrokePath: {
        get<StrokePath>().~StrokePath();
        return;
    }
    case ItemType::ApplyDeviceScaleFactor: {
        static_assert(std::is_trivially_destructible<ApplyDeviceScaleFactor>::value);
        return;
    }
#if USE(CG)
    case ItemType::ApplyFillPattern: {
        static_assert(std::is_trivially_destructible<ApplyFillPattern>::value);
        return;
    }
    case ItemType::ApplyStrokePattern: {
        static_assert(std::is_trivially_destructible<ApplyStrokePattern>::value);
        return;
    }
#endif
    case ItemType::BeginTransparencyLayer: {
        static_assert(std::is_trivially_destructible<BeginTransparencyLayer>::value);
        return;
    }
    case ItemType::ClearRect: {
        static_assert(std::is_trivially_destructible<ClearRect>::value);
        return;
    }
    case ItemType::ClearShadow: {
        static_assert(std::is_trivially_destructible<ClearShadow>::value);
        return;
    }
    case ItemType::Clip: {
        static_assert(std::is_trivially_destructible<Clip>::value);
        return;
    }
    case ItemType::ClipOut: {
        static_assert(std::is_trivially_destructible<ClipOut>::value);
        return;
    }
    case ItemType::ConcatenateCTM: {
        static_assert(std::is_trivially_destructible<ConcatenateCTM>::value);
        return;
    }
    case ItemType::DrawDotsForDocumentMarker: {
        static_assert(std::is_trivially_destructible<DrawDotsForDocumentMarker>::value);
        return;
    }
    case ItemType::DrawEllipse: {
        static_assert(std::is_trivially_destructible<DrawEllipse>::value);
        return;
    }
    case ItemType::DrawImageBuffer: {
        static_assert(std::is_trivially_destructible<DrawImageBuffer>::value);
        return;
    }
    case ItemType::DrawLine: {
        static_assert(std::is_trivially_destructible<DrawLine>::value);
        return;
    }
    case ItemType::DrawRect: {
        static_assert(std::is_trivially_destructible<DrawRect>::value);
        return;
    }
    case ItemType::EndTransparencyLayer: {
        static_assert(std::is_trivially_destructible<EndTransparencyLayer>::value);
        return;
    }
    case ItemType::FillEllipse: {
        static_assert(std::is_trivially_destructible<FillEllipse>::value);
        return;
    }
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::FillInlinePath: {
        static_assert(std::is_trivially_destructible<FillInlinePath>::value);
        return;
    }
#endif
    case ItemType::FillRect: {
        static_assert(std::is_trivially_destructible<FillRect>::value);
        return;
    }
    case ItemType::PaintFrameForMedia: {
        static_assert(std::is_trivially_destructible<PaintFrameForMedia>::value);
        return;
    }
    case ItemType::Restore: {
        static_assert(std::is_trivially_destructible<Restore>::value);
        return;
    }
    case ItemType::Rotate: {
        static_assert(std::is_trivially_destructible<Rotate>::value);
        return;
    }
    case ItemType::Save: {
        static_assert(std::is_trivially_destructible<Save>::value);
        return;
    }
    case ItemType::Scale: {
        static_assert(std::is_trivially_destructible<Scale>::value);
        return;
    }
    case ItemType::SetCTM: {
        static_assert(std::is_trivially_destructible<SetCTM>::value);
        return;
    }
    case ItemType::SetInlineFillColor: {
        static_assert(std::is_trivially_destructible<SetInlineFillColor>::value);
        return;
    }
    case ItemType::SetInlineFillGradient: {
        static_assert(std::is_trivially_destructible<SetInlineFillGradient>::value);
        return;
    }
    case ItemType::SetInlineStrokeColor: {
        static_assert(std::is_trivially_destructible<SetInlineStrokeColor>::value);
        return;
    }
    case ItemType::SetLineCap: {
        static_assert(std::is_trivially_destructible<SetLineCap>::value);
        return;
    }
    case ItemType::SetLineJoin: {
        static_assert(std::is_trivially_destructible<SetLineJoin>::value);
        return;
    }
    case ItemType::SetMiterLimit: {
        static_assert(std::is_trivially_destructible<SetMiterLimit>::value);
        return;
    }
    case ItemType::SetStrokeThickness: {
        static_assert(std::is_trivially_destructible<SetStrokeThickness>::value);
        return;
    }
    case ItemType::StrokeEllipse: {
        static_assert(std::is_trivially_destructible<StrokeEllipse>::value);
        return;
    }
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::StrokeInlinePath: {
        static_assert(std::is_trivially_destructible<StrokeInlinePath>::value);
        return;
    }
#endif
    case ItemType::StrokeRect: {
        static_assert(std::is_trivially_destructible<StrokeRect>::value);
        return;
    }
    case ItemType::StrokeLine: {
        static_assert(std::is_trivially_destructible<StrokeLine>::value);
        return;
    }
    case ItemType::Translate: {
        static_assert(std::is_trivially_destructible<Translate>::value);
        return;
    }
    }
}

void ItemHandle::copyTo(ItemHandle destination) const
{
    auto itemType = type();
    destination.data[0] = static_cast<uint8_t>(itemType);
    switch (itemType) {
    case ItemType::ClipOutToPath: {
        new (destination.data + sizeof(ItemType)) ClipOutToPath(get<ClipOutToPath>());
        return;
    }
    case ItemType::ClipPath: {
        new (destination.data + sizeof(ItemType)) ClipPath(get<ClipPath>());
        return;
    }
    case ItemType::ClipToDrawingCommands: {
        new (destination.data + sizeof(ItemType)) ClipToDrawingCommands(get<ClipToDrawingCommands>());
        return;
    }
    case ItemType::DrawFocusRingPath: {
        new (destination.data + sizeof(ItemType)) DrawFocusRingPath(get<DrawFocusRingPath>());
        return;
    }
    case ItemType::DrawFocusRingRects: {
        new (destination.data + sizeof(ItemType)) DrawFocusRingRects(get<DrawFocusRingRects>());
        return;
    }
    case ItemType::DrawGlyphs: {
        new (destination.data + sizeof(ItemType)) DrawGlyphs(get<DrawGlyphs>());
        return;
    }
    case ItemType::DrawImage: {
        new (destination.data + sizeof(ItemType)) DrawImage(get<DrawImage>());
        return;
    }
    case ItemType::DrawImageBuffer: {
        new (destination.data + sizeof(ItemType)) DrawImageBuffer(get<DrawImageBuffer>());
        return;
    }
    case ItemType::DrawLinesForText: {
        new (destination.data + sizeof(ItemType)) DrawLinesForText(get<DrawLinesForText>());
        return;
    }
    case ItemType::DrawNativeImage: {
        new (destination.data + sizeof(ItemType)) DrawNativeImage(get<DrawNativeImage>());
        return;
    }
    case ItemType::DrawPath: {
        new (destination.data + sizeof(ItemType)) DrawPath(get<DrawPath>());
        return;
    }
    case ItemType::DrawPattern: {
        new (destination.data + sizeof(ItemType)) DrawPattern(get<DrawPattern>());
        return;
    }
    case ItemType::DrawTiledImage: {
        new (destination.data + sizeof(ItemType)) DrawTiledImage(get<DrawTiledImage>());
        return;
    }
    case ItemType::DrawTiledScaledImage: {
        new (destination.data + sizeof(ItemType)) DrawTiledScaledImage(get<DrawTiledScaledImage>());
        return;
    }
    case ItemType::FillCompositedRect: {
        new (destination.data + sizeof(ItemType)) FillCompositedRect(get<FillCompositedRect>());
        return;
    }
    case ItemType::FillPath: {
        new (destination.data + sizeof(ItemType)) FillPath(get<FillPath>());
        return;
    }
    case ItemType::FillRectWithColor: {
        new (destination.data + sizeof(ItemType)) FillRectWithColor(get<FillRectWithColor>());
        return;
    }
    case ItemType::FillRectWithGradient: {
        new (destination.data + sizeof(ItemType)) FillRectWithGradient(get<FillRectWithGradient>());
        return;
    }
    case ItemType::FillRectWithRoundedHole: {
        new (destination.data + sizeof(ItemType)) FillRectWithRoundedHole(get<FillRectWithRoundedHole>());
        return;
    }
    case ItemType::FillRoundedRect: {
        new (destination.data + sizeof(ItemType)) FillRoundedRect(get<FillRoundedRect>());
        return;
    }
    case ItemType::PutImageData: {
        new (destination.data + sizeof(ItemType)) PutImageData(get<PutImageData>());
        return;
    }
    case ItemType::SetLineDash: {
        new (destination.data + sizeof(ItemType)) SetLineDash(get<SetLineDash>());
        return;
    }
    case ItemType::SetState: {
        new (destination.data + sizeof(ItemType)) SetState(get<SetState>());
        return;
    }
    case ItemType::StrokePath: {
        new (destination.data + sizeof(ItemType)) StrokePath(get<StrokePath>());
        return;
    }
    case ItemType::ApplyDeviceScaleFactor: {
        new (destination.data + sizeof(ItemType)) ApplyDeviceScaleFactor(get<ApplyDeviceScaleFactor>());
        return;
    }
#if USE(CG)
    case ItemType::ApplyFillPattern: {
        new (destination.data + sizeof(ItemType)) ApplyFillPattern(get<ApplyFillPattern>());
        return;
    }
    case ItemType::ApplyStrokePattern: {
        new (destination.data + sizeof(ItemType)) ApplyStrokePattern(get<ApplyStrokePattern>());
        return;
    }
#endif
    case ItemType::BeginTransparencyLayer: {
        new (destination.data + sizeof(ItemType)) BeginTransparencyLayer(get<BeginTransparencyLayer>());
        return;
    }
    case ItemType::ClearRect: {
        new (destination.data + sizeof(ItemType)) ClearRect(get<ClearRect>());
        return;
    }
    case ItemType::ClearShadow: {
        new (destination.data + sizeof(ItemType)) ClearShadow(get<ClearShadow>());
        return;
    }
    case ItemType::Clip: {
        new (destination.data + sizeof(ItemType)) Clip(get<Clip>());
        return;
    }
    case ItemType::ClipOut: {
        new (destination.data + sizeof(ItemType)) ClipOut(get<ClipOut>());
        return;
    }
    case ItemType::ConcatenateCTM: {
        new (destination.data + sizeof(ItemType)) ConcatenateCTM(get<ConcatenateCTM>());
        return;
    }
    case ItemType::DrawDotsForDocumentMarker: {
        new (destination.data + sizeof(ItemType)) DrawDotsForDocumentMarker(get<DrawDotsForDocumentMarker>());
        return;
    }
    case ItemType::DrawEllipse: {
        new (destination.data + sizeof(ItemType)) DrawEllipse(get<DrawEllipse>());
        return;
    }
    case ItemType::DrawLine: {
        new (destination.data + sizeof(ItemType)) DrawLine(get<DrawLine>());
        return;
    }
    case ItemType::DrawRect: {
        new (destination.data + sizeof(ItemType)) DrawRect(get<DrawRect>());
        return;
    }
    case ItemType::EndTransparencyLayer: {
        new (destination.data + sizeof(ItemType)) EndTransparencyLayer(get<EndTransparencyLayer>());
        return;
    }
    case ItemType::FillEllipse: {
        new (destination.data + sizeof(ItemType)) FillEllipse(get<FillEllipse>());
        return;
    }
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::FillInlinePath: {
        new (destination.data + sizeof(ItemType)) FillInlinePath(get<FillInlinePath>());
        return;
    }
#endif
    case ItemType::FillRect: {
        new (destination.data + sizeof(ItemType)) FillRect(get<FillRect>());
        return;
    }
    case ItemType::PaintFrameForMedia: {
        new (destination.data + sizeof(ItemType)) PaintFrameForMedia(get<PaintFrameForMedia>());
        return;
    }
    case ItemType::Restore: {
        new (destination.data + sizeof(ItemType)) Restore(get<Restore>());
        return;
    }
    case ItemType::Rotate: {
        new (destination.data + sizeof(ItemType)) Rotate(get<Rotate>());
        return;
    }
    case ItemType::Save: {
        new (destination.data + sizeof(ItemType)) Save(get<Save>());
        return;
    }
    case ItemType::Scale: {
        new (destination.data + sizeof(ItemType)) Scale(get<Scale>());
        return;
    }
    case ItemType::SetCTM: {
        new (destination.data + sizeof(ItemType)) SetCTM(get<SetCTM>());
        return;
    }
    case ItemType::SetInlineFillColor: {
        new (destination.data + sizeof(ItemType)) SetInlineFillColor(get<SetInlineFillColor>());
        return;
    }
    case ItemType::SetInlineFillGradient: {
        new (destination.data + sizeof(ItemType)) SetInlineFillGradient(get<SetInlineFillGradient>());
        return;
    }
    case ItemType::SetInlineStrokeColor: {
        new (destination.data + sizeof(ItemType)) SetInlineStrokeColor(get<SetInlineStrokeColor>());
        return;
    }
    case ItemType::SetLineCap: {
        new (destination.data + sizeof(ItemType)) SetLineCap(get<SetLineCap>());
        return;
    }
    case ItemType::SetLineJoin: {
        new (destination.data + sizeof(ItemType)) SetLineJoin(get<SetLineJoin>());
        return;
    }
    case ItemType::SetMiterLimit: {
        new (destination.data + sizeof(ItemType)) SetMiterLimit(get<SetMiterLimit>());
        return;
    }
    case ItemType::SetStrokeThickness: {
        new (destination.data + sizeof(ItemType)) SetStrokeThickness(get<SetStrokeThickness>());
        return;
    }
    case ItemType::StrokeEllipse: {
        new (destination.data + sizeof(ItemType)) StrokeEllipse(get<StrokeEllipse>());
        return;
    }
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::StrokeInlinePath: {
        new (destination.data + sizeof(ItemType)) StrokeInlinePath(get<StrokeInlinePath>());
        return;
    }
#endif
    case ItemType::StrokeRect: {
        new (destination.data + sizeof(ItemType)) StrokeRect(get<StrokeRect>());
        return;
    }
    case ItemType::StrokeLine: {
        new (destination.data + sizeof(ItemType)) StrokeLine(get<StrokeLine>());
        return;
    }
    case ItemType::Translate: {
        new (destination.data + sizeof(ItemType)) Translate(get<Translate>());
        return;
    }
    }
}

ItemBuffer::ItemBuffer(ItemBufferHandles&& handles)
    : m_readOnlyBuffers(WTFMove(handles))
{
}

ItemBuffer::ItemBuffer() = default;

ItemBuffer::~ItemBuffer()
{
    clear();
}

ItemBuffer::ItemBuffer(ItemBuffer&& other)
    : m_readingClient(std::exchange(other.m_readingClient, nullptr))
    , m_writingClient(std::exchange(other.m_writingClient, nullptr))
    , m_itemsToDestroyInAllocatedBuffers(std::exchange(other.m_itemsToDestroyInAllocatedBuffers, { }))
    , m_allocatedBuffers(std::exchange(other.m_allocatedBuffers, { }))
    , m_readOnlyBuffers(std::exchange(other.m_readOnlyBuffers, { }))
    , m_writableBuffer(std::exchange(other.m_writableBuffer, { }))
    , m_writtenNumberOfBytes(std::exchange(other.m_writtenNumberOfBytes, 0))
{
}

ItemBuffer& ItemBuffer::operator=(ItemBuffer&& other)
{
    m_readingClient = std::exchange(other.m_readingClient, nullptr);
    m_writingClient = std::exchange(other.m_writingClient, nullptr);
    m_itemsToDestroyInAllocatedBuffers = std::exchange(other.m_itemsToDestroyInAllocatedBuffers, { });
    m_allocatedBuffers = std::exchange(other.m_allocatedBuffers, { });
    m_readOnlyBuffers = std::exchange(other.m_readOnlyBuffers, { });
    m_writableBuffer = std::exchange(other.m_writableBuffer, { });
    m_writtenNumberOfBytes = std::exchange(other.m_writtenNumberOfBytes, 0);
    return *this;
}

ItemBufferHandle ItemBuffer::createItemBuffer(size_t capacity)
{
    if (m_writingClient) {
        if (auto handle = m_writingClient->createItemBuffer(capacity))
            return handle;
    }

    constexpr size_t defaultItemBufferCapacity = 1 << 10;

    auto newBufferCapacity = std::max(capacity, defaultItemBufferCapacity);
    auto* buffer = reinterpret_cast<uint8_t*>(fastMalloc(newBufferCapacity));
    m_allocatedBuffers.append(buffer);
    return { ItemBufferIdentifier::generate(), buffer, newBufferCapacity };
}

void ItemBuffer::forEachItemBuffer(Function<void(const ItemBufferHandle&)>&& mapFunction) const
{
    for (auto& handle : m_readOnlyBuffers)
        mapFunction(handle);

    if (m_writableBuffer && m_writtenNumberOfBytes)
        mapFunction({ m_writableBuffer.identifier, m_writableBuffer.data, m_writtenNumberOfBytes });
}

void ItemBuffer::clear()
{
    for (auto item : std::exchange(m_itemsToDestroyInAllocatedBuffers, { }))
        item.destroy();

    for (auto* buffer : std::exchange(m_allocatedBuffers, { }))
        fastFree(buffer);

    m_readOnlyBuffers.clear();
    m_writableBuffer = { };
    m_writtenNumberOfBytes = 0;
}

void ItemBuffer::swapWritableBufferIfNeeded(size_t numberOfBytes)
{
    if (m_writtenNumberOfBytes + numberOfBytes <= m_writableBuffer.capacity)
        return;

    auto nextBuffer = createItemBuffer(numberOfBytes);
    bool hadPreviousBuffer = m_writableBuffer;
    auto previousBuffer = std::exchange(m_writableBuffer, { });
    previousBuffer.capacity = std::exchange(m_writtenNumberOfBytes, 0);
    if (hadPreviousBuffer)
        m_readOnlyBuffers.append(WTFMove(previousBuffer));

    m_writableBuffer = WTFMove(nextBuffer);
}

void ItemBuffer::appendDataAndLength(Ref<SharedBuffer>&& data, uint8_t* location)
{
    reinterpret_cast<size_t*>(location)[0] = data->size();

    auto* startOfPadding = location + sizeof(size_t);
    size_t padding = (alignof(uint64_t) - reinterpret_cast<uintptr_t>(startOfPadding) % alignof(uint64_t)) % alignof(uint64_t);
    auto* startOfData = startOfPadding + padding;
    memcpy(startOfData, data->dataAsUInt8Ptr(), data->size());

    m_writtenNumberOfBytes += sizeof(ItemType) + sizeof(size_t) + padding + data->size();
}

} // namespace DisplayList
} // namespace WebCore
