/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "DisplayList.h"

#include "DisplayListItemBuffer.h"
#include "DisplayListItems.h"
#include "DisplayListIterator.h"
#include "Logging.h"
#include <wtf/FastMalloc.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace DisplayList {

#if !defined(NDEBUG) || !LOG_DISABLED
WTF::CString DisplayList::description() const
{
    TextStream ts;
    ts << *this;
    return ts.release().utf8();
}

void DisplayList::dump() const
{
    fprintf(stderr, "%s", description().data());
}
#endif

DisplayList::DisplayList(DisplayList&& other)
    : m_imageBuffers(std::exchange(other.m_imageBuffers, { }))
    , m_nativeImages(std::exchange(other.m_nativeImages, { }))
    , m_items(std::exchange(other.m_items, nullptr))
    , m_drawingItemExtents(std::exchange(other.m_drawingItemExtents, { }))
{
}

DisplayList::DisplayList(ItemBufferHandles&& handles)
    : m_items(makeUnique<ItemBuffer>(WTFMove(handles)))
{
}

DisplayList::DisplayList() = default;
DisplayList::~DisplayList() = default;

DisplayList& DisplayList::operator=(DisplayList&& other)
{
    m_imageBuffers = std::exchange(other.m_imageBuffers, { });
    m_nativeImages = std::exchange(other.m_nativeImages, { });
    m_items = std::exchange(other.m_items, nullptr);
    m_drawingItemExtents = std::exchange(other.m_drawingItemExtents, { });
    return *this;
}

void DisplayList::clear()
{
    if (m_items)
        m_items->clear();
    m_drawingItemExtents.clear();
    m_imageBuffers.clear();
    m_nativeImages.clear();
}

bool DisplayList::shouldDumpForFlags(AsTextFlags flags, ItemHandle item)
{
    switch (item.type()) {
    case ItemType::SetState:
        if (!(flags & AsTextFlag::IncludesPlatformOperations)) {
            const auto& stateItem = item.get<SetState>();
            // FIXME: for now, only drop the item if the only state-change flags are platform-specific.
            if (stateItem.stateChange().m_changeFlags == GraphicsContextState::ShouldSubpixelQuantizeFontsChange)
                return false;

            if (stateItem.stateChange().m_changeFlags == GraphicsContextState::ShouldSubpixelQuantizeFontsChange)
                return false;
        }
        break;
#if USE(CG)
    case ItemType::ApplyFillPattern:
    case ItemType::ApplyStrokePattern:
        if (!(flags & AsTextFlag::IncludesPlatformOperations))
            return false;
        break;
#endif
    default:
        break;
    }
    return true;
}

String DisplayList::asText(AsTextFlags flags) const
{
    TextStream stream(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);
    for (auto displayListItem : *this) {
        auto [item, extent, itemSizeInBuffer] = displayListItem.value();
        if (!shouldDumpForFlags(flags, item))
            continue;

        TextStream::GroupScope group(stream);
        stream << item;
        if (item.isDrawingItem())
            stream << " extent " << extent;
    }
    return stream.release();
}

void DisplayList::dump(TextStream& ts) const
{
    TextStream::GroupScope group(ts);
    ts << "display list";

    for (auto displayListItem : *this) {
        auto [item, extent, itemSizeInBuffer] = displayListItem.value();
        TextStream::GroupScope group(ts);
        ts << item;
        if (item.isDrawingItem())
            ts << " extent " << extent;
    }
    ts.startGroup();
    ts << "size in bytes: " << sizeInBytes();
    ts.endGroup();
}

size_t DisplayList::sizeInBytes() const
{
    return m_items ? m_items->sizeInBytes() : 0;
}

bool DisplayList::isEmpty() const
{
    return !m_items || m_items->isEmpty();
}

ItemBuffer& DisplayList::itemBuffer()
{
    if (!m_items)
        m_items = makeUnique<ItemBuffer>();
    return *m_items;
}

void DisplayList::shrinkToFit()
{
    if (auto* itemBuffer = itemBufferIfExists())
        itemBuffer->shrinkToFit();

    m_drawingItemExtents.shrinkToFit();
}

void DisplayList::setItemBufferReadingClient(ItemBufferReadingClient* client)
{
    itemBuffer().setClient(client);
}

void DisplayList::setItemBufferWritingClient(ItemBufferWritingClient* client)
{
    itemBuffer().setClient(client);
}

void DisplayList::prepareToAppend(ItemBufferHandle&& handle)
{
    itemBuffer().prepareToAppend(WTFMove(handle));
}

void DisplayList::forEachItemBuffer(Function<void(const ItemBufferHandle&)>&& mapFunction) const
{
    if (m_items)
        m_items->forEachItemBuffer(WTFMove(mapFunction));
}

void DisplayList::setTracksDrawingItemExtents(bool value)
{
    RELEASE_ASSERT(isEmpty());
    m_tracksDrawingItemExtents = value;
}

void DisplayList::append(ItemHandle item)
{
    switch (item.type()) {
    case ItemType::Save:
        return append<Save>(item.get<Save>());
    case ItemType::Restore:
        return append<Restore>(item.get<Restore>());
    case ItemType::Translate:
        return append<Translate>(item.get<Translate>());
    case ItemType::Rotate:
        return append<Rotate>(item.get<Rotate>());
    case ItemType::Scale:
        return append<Scale>(item.get<Scale>());
    case ItemType::ConcatenateCTM:
        return append<ConcatenateCTM>(item.get<ConcatenateCTM>());
    case ItemType::SetCTM:
        return append<SetCTM>(item.get<SetCTM>());
    case ItemType::SetInlineFillGradient:
        return append<SetInlineFillGradient>(item.get<SetInlineFillGradient>());
    case ItemType::SetInlineFillColor:
        return append<SetInlineFillColor>(item.get<SetInlineFillColor>());
    case ItemType::SetInlineStrokeColor:
        return append<SetInlineStrokeColor>(item.get<SetInlineStrokeColor>());
    case ItemType::SetStrokeThickness:
        return append<SetStrokeThickness>(item.get<SetStrokeThickness>());
    case ItemType::SetState:
        return append<SetState>(item.get<SetState>());
    case ItemType::SetLineCap:
        return append<SetLineCap>(item.get<SetLineCap>());
    case ItemType::SetLineDash:
        return append<SetLineDash>(item.get<SetLineDash>());
    case ItemType::SetLineJoin:
        return append<SetLineJoin>(item.get<SetLineJoin>());
    case ItemType::SetMiterLimit:
        return append<SetMiterLimit>(item.get<SetMiterLimit>());
    case ItemType::ClearShadow:
        return append<ClearShadow>(item.get<ClearShadow>());
    case ItemType::Clip:
        return append<Clip>(item.get<Clip>());
    case ItemType::ClipOut:
        return append<ClipOut>(item.get<ClipOut>());
    case ItemType::ClipToImageBuffer:
        return append<ClipToImageBuffer>(item.get<ClipToImageBuffer>());
    case ItemType::ClipOutToPath:
        return append<ClipOutToPath>(item.get<ClipOutToPath>());
    case ItemType::ClipPath:
        return append<ClipPath>(item.get<ClipPath>());
    case ItemType::BeginClipToDrawingCommands:
        return append<BeginClipToDrawingCommands>(item.get<BeginClipToDrawingCommands>());
    case ItemType::EndClipToDrawingCommands:
        return append<EndClipToDrawingCommands>(item.get<EndClipToDrawingCommands>());
    case ItemType::DrawGlyphs:
        return append<DrawGlyphs>(item.get<DrawGlyphs>());
    case ItemType::DrawImageBuffer:
        return append<DrawImageBuffer>(item.get<DrawImageBuffer>());
    case ItemType::DrawNativeImage:
        return append<DrawNativeImage>(item.get<DrawNativeImage>());
    case ItemType::DrawPattern:
        return append<DrawPattern>(item.get<DrawPattern>());
    case ItemType::DrawRect:
        return append<DrawRect>(item.get<DrawRect>());
    case ItemType::DrawLine:
        return append<DrawLine>(item.get<DrawLine>());
    case ItemType::DrawLinesForText:
        return append<DrawLinesForText>(item.get<DrawLinesForText>());
    case ItemType::DrawDotsForDocumentMarker:
        return append<DrawDotsForDocumentMarker>(item.get<DrawDotsForDocumentMarker>());
    case ItemType::DrawEllipse:
        return append<DrawEllipse>(item.get<DrawEllipse>());
    case ItemType::DrawPath:
        return append<DrawPath>(item.get<DrawPath>());
    case ItemType::DrawFocusRingPath:
        return append<DrawFocusRingPath>(item.get<DrawFocusRingPath>());
    case ItemType::DrawFocusRingRects:
        return append<DrawFocusRingRects>(item.get<DrawFocusRingRects>());
    case ItemType::FillRect:
        return append<FillRect>(item.get<FillRect>());
    case ItemType::FillRectWithColor:
        return append<FillRectWithColor>(item.get<FillRectWithColor>());
    case ItemType::FillRectWithGradient:
        return append<FillRectWithGradient>(item.get<FillRectWithGradient>());
    case ItemType::FillCompositedRect:
        return append<FillCompositedRect>(item.get<FillCompositedRect>());
    case ItemType::FillRoundedRect:
        return append<FillRoundedRect>(item.get<FillRoundedRect>());
    case ItemType::FillRectWithRoundedHole:
        return append<FillRectWithRoundedHole>(item.get<FillRectWithRoundedHole>());
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::FillLine:
        return append<FillLine>(item.get<FillLine>());
    case ItemType::FillArc:
        return append<FillArc>(item.get<FillArc>());
    case ItemType::FillQuadCurve:
        return append<FillQuadCurve>(item.get<FillQuadCurve>());
    case ItemType::FillBezierCurve:
        return append<FillBezierCurve>(item.get<FillBezierCurve>());
#endif
    case ItemType::FillPath:
        return append<FillPath>(item.get<FillPath>());
    case ItemType::FillEllipse:
        return append<FillEllipse>(item.get<FillEllipse>());
    case ItemType::FlushContext:
        return append<FlushContext>(item.get<FlushContext>());
    case ItemType::MetaCommandChangeDestinationImageBuffer:
        return append<MetaCommandChangeDestinationImageBuffer>(item.get<MetaCommandChangeDestinationImageBuffer>());
    case ItemType::MetaCommandChangeItemBuffer:
        return append<MetaCommandChangeItemBuffer>(item.get<MetaCommandChangeItemBuffer>());
    case ItemType::GetPixelBuffer:
        return append<GetPixelBuffer>(item.get<GetPixelBuffer>());
    case ItemType::PutPixelBuffer:
        return append<PutPixelBuffer>(item.get<PutPixelBuffer>());
#if ENABLE(VIDEO)
    case ItemType::PaintFrameForMedia:
        return append<PaintFrameForMedia>(item.get<PaintFrameForMedia>());
#endif
    case ItemType::StrokeRect:
        return append<StrokeRect>(item.get<StrokeRect>());
    case ItemType::StrokeLine:
        return append<StrokeLine>(item.get<StrokeLine>());
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::StrokeArc:
        return append<StrokeArc>(item.get<StrokeArc>());
    case ItemType::StrokeQuadCurve:
        return append<StrokeQuadCurve>(item.get<StrokeQuadCurve>());
    case ItemType::StrokeBezierCurve:
        return append<StrokeBezierCurve>(item.get<StrokeBezierCurve>());
#endif
    case ItemType::StrokePath:
        return append<StrokePath>(item.get<StrokePath>());
    case ItemType::StrokeEllipse:
        return append<StrokeEllipse>(item.get<StrokeEllipse>());
    case ItemType::ClearRect:
        return append<ClearRect>(item.get<ClearRect>());
    case ItemType::BeginTransparencyLayer:
        return append<BeginTransparencyLayer>(item.get<BeginTransparencyLayer>());
    case ItemType::EndTransparencyLayer:
        return append<EndTransparencyLayer>(item.get<EndTransparencyLayer>());
#if USE(CG)
    case ItemType::ApplyStrokePattern:
        return append<ApplyStrokePattern>(item.get<ApplyStrokePattern>());
    case ItemType::ApplyFillPattern:
        return append<ApplyFillPattern>(item.get<ApplyFillPattern>());
#endif
    case ItemType::ApplyDeviceScaleFactor:
        return append<ApplyDeviceScaleFactor>(item.get<ApplyDeviceScaleFactor>());
    }
}

auto DisplayList::begin() const -> Iterator
{
    return { *this };
}

auto DisplayList::end() const -> Iterator
{
    return { *this, Iterator::ImmediatelyMoveToEnd::Yes };
}

TextStream& operator<<(TextStream& ts, const DisplayList& displayList)
{
    displayList.dump(ts);
    return ts;
}

} // namespace DisplayList

} // namespace WebCore
