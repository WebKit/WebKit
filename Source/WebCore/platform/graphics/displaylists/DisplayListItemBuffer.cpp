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
        ASSERT_NOT_REACHED();
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
    case ItemType::ClipToImageBuffer: {
        get<ClipToImageBuffer>().apply(context);
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
    case ItemType::BeginClipToDrawingCommands: {
        ASSERT_NOT_REACHED();
        return;
    }
    case ItemType::EndClipToDrawingCommands: {
        ASSERT_NOT_REACHED();
        return;
    }
    case ItemType::DrawGlyphs:
        ASSERT_NOT_REACHED();
        return;
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
    case ItemType::FlushContext: {
        get<FlushContext>().apply(context);
        return;
    }
    case ItemType::MetaCommandChangeDestinationImageBuffer:
    case ItemType::MetaCommandChangeItemBuffer:
        return;
    case ItemType::PutImageData: {
        get<PutImageData>().apply(context);
        return;
    }
#if ENABLE(VIDEO)
    case ItemType::PaintFrameForMedia: {
        get<PaintFrameForMedia>().apply(context);
        return;
    }
#endif
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
    case ItemType::DrawLinesForText: {
        get<DrawLinesForText>().~DrawLinesForText();
        return;
    }
    case ItemType::DrawPath: {
        get<DrawPath>().~DrawPath();
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
    case ItemType::BeginClipToDrawingCommands: {
        static_assert(std::is_trivially_destructible<BeginClipToDrawingCommands>::value);
        return;
    }
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
    case ItemType::ClipToImageBuffer: {
        static_assert(std::is_trivially_destructible<ClipToImageBuffer>::value);
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
    case ItemType::DrawNativeImage: {
        static_assert(std::is_trivially_destructible<DrawNativeImage>::value);
        return;
    }
    case ItemType::DrawPattern: {
        static_assert(std::is_trivially_destructible<DrawPattern>::value);
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
    case ItemType::EndClipToDrawingCommands: {
        static_assert(std::is_trivially_destructible<EndClipToDrawingCommands>::value);
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
    case ItemType::FlushContext: {
        static_assert(std::is_trivially_destructible<FlushContext>::value);
        return;
    }
    case ItemType::MetaCommandChangeDestinationImageBuffer: {
        static_assert(std::is_trivially_destructible<MetaCommandChangeDestinationImageBuffer>::value);
        return;
    }
    case ItemType::MetaCommandChangeItemBuffer: {
        static_assert(std::is_trivially_destructible<MetaCommandChangeItemBuffer>::value);
        return;
    }
#if ENABLE(VIDEO)
    case ItemType::PaintFrameForMedia: {
        static_assert(std::is_trivially_destructible<PaintFrameForMedia>::value);
        return;
    }
#endif
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

template<typename, typename = void> inline constexpr bool HasIsValid = false;
template<typename T> inline constexpr bool HasIsValid<T, std::void_t<decltype(std::declval<T>().isValid())>> = true;

template<typename Item>
typename std::enable_if_t<!HasIsValid<Item>, bool> copyInto(const ItemHandle& itemHandle, uint8_t* destinationWithOffset)
{
    new (destinationWithOffset) Item(itemHandle.get<Item>());
    return true;
}

template<typename Item>
typename std::enable_if_t<HasIsValid<Item>, bool> copyInto(const ItemHandle& itemHandle, uint8_t* destinationWithOffset)
{
    auto* newItem = new (destinationWithOffset) Item(itemHandle.get<Item>());
    return newItem->isValid();
}

bool ItemHandle::safeCopy(ItemHandle destination) const
{
    auto itemType = type();
    destination.data[0] = static_cast<uint8_t>(itemType);
    auto itemOffset = destination.data + sizeof(uint64_t);
    switch (itemType) {
    case ItemType::ClipOutToPath:
        return copyInto<ClipOutToPath>(*this, itemOffset);
    case ItemType::ClipPath:
        return copyInto<ClipPath>(*this, itemOffset);
    case ItemType::DrawFocusRingPath:
        return copyInto<DrawFocusRingPath>(*this, itemOffset);
    case ItemType::DrawFocusRingRects:
        return copyInto<DrawFocusRingRects>(*this, itemOffset);
    case ItemType::DrawGlyphs:
        return copyInto<DrawGlyphs>(*this, itemOffset);
    case ItemType::DrawImageBuffer:
        return copyInto<DrawImageBuffer>(*this, itemOffset);
    case ItemType::DrawLinesForText:
        return copyInto<DrawLinesForText>(*this, itemOffset);
    case ItemType::DrawNativeImage:
        return copyInto<DrawNativeImage>(*this, itemOffset);
    case ItemType::DrawPattern:
        return copyInto<DrawPattern>(*this, itemOffset);
    case ItemType::DrawPath:
        return copyInto<DrawPath>(*this, itemOffset);
    case ItemType::FillCompositedRect:
        return copyInto<FillCompositedRect>(*this, itemOffset);
    case ItemType::FillPath:
        return copyInto<FillPath>(*this, itemOffset);
    case ItemType::FillRectWithColor:
        return copyInto<FillRectWithColor>(*this, itemOffset);
    case ItemType::FillRectWithGradient:
        return copyInto<FillRectWithGradient>(*this, itemOffset);
    case ItemType::FillRectWithRoundedHole:
        return copyInto<FillRectWithRoundedHole>(*this, itemOffset);
    case ItemType::FillRoundedRect:
        return copyInto<FillRoundedRect>(*this, itemOffset);
    case ItemType::PutImageData:
        return copyInto<PutImageData>(*this, itemOffset);
    case ItemType::SetLineDash:
        return copyInto<SetLineDash>(*this, itemOffset);
    case ItemType::SetState:
        return copyInto<SetState>(*this, itemOffset);
    case ItemType::StrokePath:
        return copyInto<StrokePath>(*this, itemOffset);
    case ItemType::ApplyDeviceScaleFactor:
        return copyInto<ApplyDeviceScaleFactor>(*this, itemOffset);
#if USE(CG)
    case ItemType::ApplyFillPattern:
        return copyInto<ApplyFillPattern>(*this, itemOffset);
    case ItemType::ApplyStrokePattern:
        return copyInto<ApplyStrokePattern>(*this, itemOffset);
#endif
    case ItemType::BeginClipToDrawingCommands:
        return copyInto<BeginClipToDrawingCommands>(*this, itemOffset);
    case ItemType::BeginTransparencyLayer:
        return copyInto<BeginTransparencyLayer>(*this, itemOffset);
    case ItemType::ClearRect:
        return copyInto<ClearRect>(*this, itemOffset);
    case ItemType::ClearShadow:
        return copyInto<ClearShadow>(*this, itemOffset);
    case ItemType::Clip:
        return copyInto<Clip>(*this, itemOffset);
    case ItemType::ClipOut:
        return copyInto<ClipOut>(*this, itemOffset);
    case ItemType::ClipToImageBuffer:
        return copyInto<ClipToImageBuffer>(*this, itemOffset);
    case ItemType::ConcatenateCTM:
        return copyInto<ConcatenateCTM>(*this, itemOffset);
    case ItemType::DrawDotsForDocumentMarker:
        return copyInto<DrawDotsForDocumentMarker>(*this, itemOffset);
    case ItemType::DrawEllipse:
        return copyInto<DrawEllipse>(*this, itemOffset);
    case ItemType::DrawLine:
        return copyInto<DrawLine>(*this, itemOffset);
    case ItemType::DrawRect:
        return copyInto<DrawRect>(*this, itemOffset);
    case ItemType::EndClipToDrawingCommands:
        return copyInto<EndClipToDrawingCommands>(*this, itemOffset);
    case ItemType::EndTransparencyLayer:
        return copyInto<EndTransparencyLayer>(*this, itemOffset);
    case ItemType::FillEllipse:
        return copyInto<FillEllipse>(*this, itemOffset);
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::FillInlinePath:
        return copyInto<FillInlinePath>(*this, itemOffset);
#endif
    case ItemType::FillRect:
        return copyInto<FillRect>(*this, itemOffset);
    case ItemType::FlushContext:
        return copyInto<FlushContext>(*this, itemOffset);
    case ItemType::MetaCommandChangeDestinationImageBuffer:
        return copyInto<MetaCommandChangeDestinationImageBuffer>(*this, itemOffset);
    case ItemType::MetaCommandChangeItemBuffer:
        return copyInto<MetaCommandChangeItemBuffer>(*this, itemOffset);
#if ENABLE(VIDEO)
    case ItemType::PaintFrameForMedia:
        return copyInto<PaintFrameForMedia>(*this, itemOffset);
#endif
    case ItemType::Restore:
        return copyInto<Restore>(*this, itemOffset);
    case ItemType::Rotate:
        return copyInto<Rotate>(*this, itemOffset);
    case ItemType::Save:
        return copyInto<Save>(*this, itemOffset);
    case ItemType::Scale:
        return copyInto<Scale>(*this, itemOffset);
    case ItemType::SetCTM:
        return copyInto<SetCTM>(*this, itemOffset);
    case ItemType::SetInlineFillColor:
        return copyInto<SetInlineFillColor>(*this, itemOffset);
    case ItemType::SetInlineFillGradient:
        return copyInto<SetInlineFillGradient>(*this, itemOffset);
    case ItemType::SetInlineStrokeColor:
        return copyInto<SetInlineStrokeColor>(*this, itemOffset);
    case ItemType::SetLineCap:
        return copyInto<SetLineCap>(*this, itemOffset);
    case ItemType::SetLineJoin:
        return copyInto<SetLineJoin>(*this, itemOffset);
    case ItemType::SetMiterLimit:
        return copyInto<SetMiterLimit>(*this, itemOffset);
    case ItemType::SetStrokeThickness:
        return copyInto<SetStrokeThickness>(*this, itemOffset);
    case ItemType::StrokeEllipse:
        return copyInto<StrokeEllipse>(*this, itemOffset);
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::StrokeInlinePath:
        return copyInto<StrokeInlinePath>(*this, itemOffset);
#endif
    case ItemType::StrokeRect:
        return copyInto<StrokeRect>(*this, itemOffset);
    case ItemType::StrokeLine:
        return copyInto<StrokeLine>(*this, itemOffset);
    case ItemType::Translate:
        return copyInto<Translate>(*this, itemOffset);
    }
    return false;
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

DidChangeItemBuffer ItemBuffer::swapWritableBufferIfNeeded(size_t numberOfBytes)
{
    auto sizeForBufferSwitchItem = paddedSizeOfTypeAndItemInBytes(ItemType::MetaCommandChangeItemBuffer);
    if (m_writtenNumberOfBytes + numberOfBytes + sizeForBufferSwitchItem <= m_writableBuffer.capacity)
        return DidChangeItemBuffer::No;

    auto nextBuffer = createItemBuffer(numberOfBytes + sizeForBufferSwitchItem);
    bool hadPreviousBuffer = m_writableBuffer && m_writableBuffer.identifier != nextBuffer.identifier;
    if (hadPreviousBuffer) {
        uncheckedAppend<MetaCommandChangeItemBuffer>(DidChangeItemBuffer::No, nextBuffer.identifier);
        m_writableBuffer.capacity = m_writtenNumberOfBytes;
        m_readOnlyBuffers.append(m_writableBuffer);
    }
    m_writtenNumberOfBytes = 0;
    m_writableBuffer = WTFMove(nextBuffer);
    return hadPreviousBuffer ? DidChangeItemBuffer::Yes : DidChangeItemBuffer::No;
}

void ItemBuffer::append(ItemHandle temporaryItem)
{
    auto data = m_writingClient->encodeItem(temporaryItem);
    if (!data)
        return;

    auto dataLength = data->size();
    auto additionalCapacityForEncodedItem = 2 * sizeof(uint64_t) + roundUpToMultipleOf(alignof(uint64_t), dataLength);

    auto bufferChanged = swapWritableBufferIfNeeded(additionalCapacityForEncodedItem);

    m_writableBuffer.data[m_writtenNumberOfBytes] = static_cast<uint8_t>(temporaryItem.type());
    reinterpret_cast<uint64_t*>(m_writableBuffer.data + m_writtenNumberOfBytes)[1] = dataLength;
    memcpy(m_writableBuffer.data + m_writtenNumberOfBytes + 2 * sizeof(uint64_t), data->dataAsUInt8Ptr(), dataLength);

    didAppendData(additionalCapacityForEncodedItem, bufferChanged);
}

void ItemBuffer::didAppendData(size_t numberOfBytes, DidChangeItemBuffer didChangeItemBuffer)
{
    m_writtenNumberOfBytes += numberOfBytes;
    if (m_writingClient)
        m_writingClient->didAppendData(m_writableBuffer, numberOfBytes, didChangeItemBuffer);
}

void ItemBuffer::prepareToAppend(ItemBufferHandle&& handle)
{
    m_writtenNumberOfBytes = 0;
    m_readOnlyBuffers.append(std::exchange(m_writableBuffer, WTFMove(handle)));
}

} // namespace DisplayList
} // namespace WebCore
