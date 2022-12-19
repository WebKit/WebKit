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
#include "DisplayListItemBuffer.h"

#include "DisplayListItemBufferIdentifier.h"
#include "DisplayListItems.h"
#include "Filter.h"
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
    case ItemType::Save:
        get<Save>().apply(context);
        return;
    case ItemType::Restore:
        get<Restore>().apply(context);
        return;
    case ItemType::Translate:
        get<Translate>().apply(context);
        return;
    case ItemType::Rotate:
        get<Rotate>().apply(context);
        return;
    case ItemType::Scale:
        get<Scale>().apply(context);
        return;
    case ItemType::ConcatenateCTM:
        get<ConcatenateCTM>().apply(context);
        return;
    case ItemType::SetCTM:
        get<SetCTM>().apply(context);
        return;
    case ItemType::SetInlineFillColor:
        get<SetInlineFillColor>().apply(context);
        return;
    case ItemType::SetInlineStrokeColor:
        get<SetInlineStrokeColor>().apply(context);
        return;
    case ItemType::SetStrokeThickness:
        get<SetStrokeThickness>().apply(context);
        return;
    case ItemType::SetState:
        ASSERT_NOT_REACHED();
        return;
    case ItemType::SetLineCap:
        get<SetLineCap>().apply(context);
        return;
    case ItemType::SetLineDash:
        get<SetLineDash>().apply(context);
        return;
    case ItemType::SetLineJoin:
        get<SetLineJoin>().apply(context);
        return;
    case ItemType::SetMiterLimit:
        get<SetMiterLimit>().apply(context);
        return;
    case ItemType::ClearShadow:
        get<ClearShadow>().apply(context);
        return;
    case ItemType::Clip:
        get<Clip>().apply(context);
        return;
    case ItemType::ClipOut:
        get<ClipOut>().apply(context);
        return;
    case ItemType::ClipToImageBuffer:
        ASSERT_NOT_REACHED();
        return;
    case ItemType::ClipOutToPath:
        get<ClipOutToPath>().apply(context);
        return;
    case ItemType::ClipPath:
        get<ClipPath>().apply(context);
        return;
    case ItemType::DrawFilteredImageBuffer:
        ASSERT_NOT_REACHED();
        return;
    case ItemType::DrawGlyphs:
        ASSERT_NOT_REACHED();
        return;
    case ItemType::DrawDecomposedGlyphs:
        ASSERT_NOT_REACHED();
        return;
    case ItemType::DrawImageBuffer:
        ASSERT_NOT_REACHED();
        return;
    case ItemType::DrawNativeImage:
        ASSERT_NOT_REACHED();
        return;
    case ItemType::DrawSystemImage:
        get<DrawSystemImage>().apply(context);
        return;
    case ItemType::DrawPattern:
        ASSERT_NOT_REACHED();
        return;
    case ItemType::DrawRect:
        get<DrawRect>().apply(context);
        return;
    case ItemType::DrawLine:
        get<DrawLine>().apply(context);
        return;
    case ItemType::DrawLinesForText:
        get<DrawLinesForText>().apply(context);
        return;
    case ItemType::DrawDotsForDocumentMarker:
        get<DrawDotsForDocumentMarker>().apply(context);
        return;
    case ItemType::DrawEllipse:
        get<DrawEllipse>().apply(context);
        return;
    case ItemType::DrawPath:
        get<DrawPath>().apply(context);
        return;
    case ItemType::DrawFocusRingPath:
        get<DrawFocusRingPath>().apply(context);
        return;
    case ItemType::DrawFocusRingRects:
        get<DrawFocusRingRects>().apply(context);
        return;
    case ItemType::FillRect:
        get<FillRect>().apply(context);
        return;
    case ItemType::FillRectWithColor:
        get<FillRectWithColor>().apply(context);
        return;
    case ItemType::FillRectWithGradient:
        get<FillRectWithGradient>().apply(context);
        return;
    case ItemType::FillCompositedRect:
        get<FillCompositedRect>().apply(context);
        return;
    case ItemType::FillRoundedRect:
        get<FillRoundedRect>().apply(context);
        return;
    case ItemType::FillRectWithRoundedHole:
        get<FillRectWithRoundedHole>().apply(context);
        return;
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::FillLine:
        get<FillLine>().apply(context);
        return;
    case ItemType::FillArc:
        get<FillArc>().apply(context);
        return;
    case ItemType::FillQuadCurve:
        get<FillQuadCurve>().apply(context);
        return;
    case ItemType::FillBezierCurve:
        get<FillBezierCurve>().apply(context);
        return;
#endif
    case ItemType::FillPath:
        get<FillPath>().apply(context);
        return;
    case ItemType::FillEllipse:
        get<FillEllipse>().apply(context);
        return;
#if ENABLE(VIDEO)
    case ItemType::PaintFrameForMedia:
        ASSERT_NOT_REACHED();
        return;
#endif
    case ItemType::StrokeRect:
        get<StrokeRect>().apply(context);
        return;
    case ItemType::StrokeLine:
        get<StrokeLine>().apply(context);
        return;
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::StrokeArc:
        get<StrokeArc>().apply(context);
        return;
    case ItemType::StrokeQuadCurve:
        get<StrokeQuadCurve>().apply(context);
        return;
    case ItemType::StrokeBezierCurve:
        get<StrokeBezierCurve>().apply(context);
        return;
#endif
    case ItemType::StrokePath:
        get<StrokePath>().apply(context);
        return;
    case ItemType::StrokeEllipse:
        get<StrokeEllipse>().apply(context);
        return;
    case ItemType::ClearRect:
        get<ClearRect>().apply(context);
        return;
    case ItemType::DrawControlPart:
        get<DrawControlPart>().apply(context);
        return;
    case ItemType::BeginTransparencyLayer:
        get<BeginTransparencyLayer>().apply(context);
        return;
    case ItemType::EndTransparencyLayer:
        get<EndTransparencyLayer>().apply(context);
        return;
#if USE(CG)
    case ItemType::ApplyStrokePattern:
        get<ApplyStrokePattern>().apply(context);
        return;
    case ItemType::ApplyFillPattern:
        get<ApplyFillPattern>().apply(context);
        return;
#endif
    case ItemType::ApplyDeviceScaleFactor:
        get<ApplyDeviceScaleFactor>().apply(context);
        return;
    }
}

void ItemHandle::destroy()
{
    switch (type()) {
    case ItemType::ClipOutToPath:
        get<ClipOutToPath>().~ClipOutToPath();
        return;
    case ItemType::ClipPath:
        get<ClipPath>().~ClipPath();
        return;
    case ItemType::DrawControlPart:
        get<DrawControlPart>().~DrawControlPart();
        return;
    case ItemType::DrawFilteredImageBuffer:
        get<DrawFilteredImageBuffer>().~DrawFilteredImageBuffer();
        return;
    case ItemType::DrawFocusRingPath:
        get<DrawFocusRingPath>().~DrawFocusRingPath();
        return;
    case ItemType::DrawFocusRingRects:
        get<DrawFocusRingRects>().~DrawFocusRingRects();
        return;
    case ItemType::DrawGlyphs:
        get<DrawGlyphs>().~DrawGlyphs();
        return;
    case ItemType::DrawDecomposedGlyphs:
        get<DrawDecomposedGlyphs>().~DrawDecomposedGlyphs();
        break;
    case ItemType::DrawLinesForText:
        get<DrawLinesForText>().~DrawLinesForText();
        return;
    case ItemType::DrawPath:
        get<DrawPath>().~DrawPath();
        return;
    case ItemType::FillCompositedRect:
        get<FillCompositedRect>().~FillCompositedRect();
        return;
    case ItemType::FillPath:
        get<FillPath>().~FillPath();
        return;
    case ItemType::FillRectWithColor:
        get<FillRectWithColor>().~FillRectWithColor();
        return;
    case ItemType::FillRectWithGradient:
        get<FillRectWithGradient>().~FillRectWithGradient();
        return;
    case ItemType::FillRectWithRoundedHole:
        get<FillRectWithRoundedHole>().~FillRectWithRoundedHole();
        return;
    case ItemType::FillRoundedRect:
        get<FillRoundedRect>().~FillRoundedRect();
        return;
    case ItemType::SetLineDash:
        get<SetLineDash>().~SetLineDash();
        return;
    case ItemType::SetState:
        get<SetState>().~SetState();
        return;
    case ItemType::StrokePath:
        get<StrokePath>().~StrokePath();
        return;
    case ItemType::ApplyDeviceScaleFactor:
        static_assert(std::is_trivially_destructible<ApplyDeviceScaleFactor>::value);
        return;
#if USE(CG)
    case ItemType::ApplyFillPattern:
        static_assert(std::is_trivially_destructible<ApplyFillPattern>::value);
        return;
    case ItemType::ApplyStrokePattern:
        static_assert(std::is_trivially_destructible<ApplyStrokePattern>::value);
        return;
#endif
    case ItemType::BeginTransparencyLayer:
        static_assert(std::is_trivially_destructible<BeginTransparencyLayer>::value);
        return;
    case ItemType::ClearRect:
        static_assert(std::is_trivially_destructible<ClearRect>::value);
        return;
    case ItemType::ClearShadow:
        static_assert(std::is_trivially_destructible<ClearShadow>::value);
        return;
    case ItemType::Clip:
        static_assert(std::is_trivially_destructible<Clip>::value);
        return;
    case ItemType::ClipOut:
        static_assert(std::is_trivially_destructible<ClipOut>::value);
        return;
    case ItemType::ClipToImageBuffer:
        static_assert(std::is_trivially_destructible<ClipToImageBuffer>::value);
        return;
    case ItemType::ConcatenateCTM:
        static_assert(std::is_trivially_destructible<ConcatenateCTM>::value);
        return;
    case ItemType::DrawDotsForDocumentMarker:
        static_assert(std::is_trivially_destructible<DrawDotsForDocumentMarker>::value);
        return;
    case ItemType::DrawEllipse:
        static_assert(std::is_trivially_destructible<DrawEllipse>::value);
        return;
    case ItemType::DrawImageBuffer:
        static_assert(std::is_trivially_destructible<DrawImageBuffer>::value);
        return;
    case ItemType::DrawNativeImage:
        static_assert(std::is_trivially_destructible<DrawNativeImage>::value);
        return;
    case ItemType::DrawSystemImage:
        get<DrawSystemImage>().~DrawSystemImage();
        return;
    case ItemType::DrawPattern:
        static_assert(std::is_trivially_destructible<DrawPattern>::value);
        return;
    case ItemType::DrawLine:
        static_assert(std::is_trivially_destructible<DrawLine>::value);
        return;
    case ItemType::DrawRect:
        static_assert(std::is_trivially_destructible<DrawRect>::value);
        return;
    case ItemType::EndTransparencyLayer:
        static_assert(std::is_trivially_destructible<EndTransparencyLayer>::value);
        return;
    case ItemType::FillEllipse:
        static_assert(std::is_trivially_destructible<FillEllipse>::value);
        return;
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::FillLine:
        static_assert(std::is_trivially_destructible<FillLine>::value);
        return;
    case ItemType::FillArc:
        static_assert(std::is_trivially_destructible<FillArc>::value);
        return;
    case ItemType::FillQuadCurve:
        static_assert(std::is_trivially_destructible<FillQuadCurve>::value);
        return;
    case ItemType::FillBezierCurve:
        static_assert(std::is_trivially_destructible<FillBezierCurve>::value);
        return;
#endif
    case ItemType::FillRect:
        static_assert(std::is_trivially_destructible<FillRect>::value);
        return;
#if ENABLE(VIDEO)
    case ItemType::PaintFrameForMedia:
        static_assert(std::is_trivially_destructible<PaintFrameForMedia>::value);
        return;
#endif
    case ItemType::Restore:
        static_assert(std::is_trivially_destructible<Restore>::value);
        return;
    case ItemType::Rotate:
        static_assert(std::is_trivially_destructible<Rotate>::value);
        return;
    case ItemType::Save:
        static_assert(std::is_trivially_destructible<Save>::value);
        return;
    case ItemType::Scale:
        static_assert(std::is_trivially_destructible<Scale>::value);
        return;
    case ItemType::SetCTM:
        static_assert(std::is_trivially_destructible<SetCTM>::value);
        return;
    case ItemType::SetInlineFillColor:
        static_assert(std::is_trivially_destructible<SetInlineFillColor>::value);
        return;
    case ItemType::SetInlineStrokeColor:
        static_assert(std::is_trivially_destructible<SetInlineStrokeColor>::value);
        return;
    case ItemType::SetLineCap:
        static_assert(std::is_trivially_destructible<SetLineCap>::value);
        return;
    case ItemType::SetLineJoin:
        static_assert(std::is_trivially_destructible<SetLineJoin>::value);
        return;
    case ItemType::SetMiterLimit:
        static_assert(std::is_trivially_destructible<SetMiterLimit>::value);
        return;
    case ItemType::SetStrokeThickness:
        static_assert(std::is_trivially_destructible<SetStrokeThickness>::value);
        return;
    case ItemType::StrokeEllipse:
        static_assert(std::is_trivially_destructible<StrokeEllipse>::value);
        return;
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::StrokeArc:
        static_assert(std::is_trivially_destructible<StrokeArc>::value);
        return;
    case ItemType::StrokeQuadCurve:
        static_assert(std::is_trivially_destructible<StrokeQuadCurve>::value);
        return;
    case ItemType::StrokeBezierCurve:
        static_assert(std::is_trivially_destructible<StrokeBezierCurve>::value);
        return;
#endif
    case ItemType::StrokeRect:
        static_assert(std::is_trivially_destructible<StrokeRect>::value);
        return;
    case ItemType::StrokeLine:
        static_assert(std::is_trivially_destructible<StrokeLine>::value);
        return;
    case ItemType::Translate:
        static_assert(std::is_trivially_destructible<Translate>::value);
        return;
    }
}

template<typename, typename = void> inline constexpr bool HasIsValid = false;
template<typename T> inline constexpr bool HasIsValid<T, std::void_t<decltype(std::declval<T>().isValid())>> = true;

template<typename Item>
static inline bool isValid(const Item& item)
{
    if constexpr (HasIsValid<Item>)
        return item.isValid();
    else {
        UNUSED_PARAM(item);
        return true;
    }
}

template<typename Item>
static inline bool copyInto(uint8_t* destinationWithOffset, const Item& item)
{
    auto* newItem = new (destinationWithOffset) Item(item);
    return isValid(*newItem);
}

template<typename Item>
static inline bool copyInto(uint8_t* destinationWithOffset, const ItemHandle& itemHandle)
{
    return copyInto(destinationWithOffset, itemHandle.get<Item>());
}

bool ItemHandle::safeCopy(ItemType itemType, ItemHandle destination) const
{
    ASSERT(itemType == type());
    destination.data[0] = static_cast<uint8_t>(itemType);
    auto itemOffset = destination.data + sizeof(uint64_t);
    switch (itemType) {
    case ItemType::ClipOutToPath:
        return copyInto<ClipOutToPath>(itemOffset, *this);
    case ItemType::ClipPath:
        return copyInto<ClipPath>(itemOffset, *this);
    case ItemType::DrawControlPart:
        return copyInto<DrawControlPart>(itemOffset, *this);
    case ItemType::DrawFilteredImageBuffer:
        return copyInto<DrawFilteredImageBuffer>(itemOffset, *this);
    case ItemType::DrawFocusRingPath:
        return copyInto<DrawFocusRingPath>(itemOffset, *this);
    case ItemType::DrawFocusRingRects:
        return copyInto<DrawFocusRingRects>(itemOffset, *this);
    case ItemType::DrawGlyphs:
        return copyInto<DrawGlyphs>(itemOffset, *this);
    case ItemType::DrawDecomposedGlyphs:
        return copyInto<DrawDecomposedGlyphs>(itemOffset, *this);
    case ItemType::DrawImageBuffer:
        return copyInto<DrawImageBuffer>(itemOffset, *this);
    case ItemType::DrawLinesForText:
        return copyInto<DrawLinesForText>(itemOffset, *this);
    case ItemType::DrawNativeImage:
        return copyInto<DrawNativeImage>(itemOffset, *this);
    case ItemType::DrawSystemImage:
        return copyInto<DrawSystemImage>(itemOffset, *this);
    case ItemType::DrawPattern:
        return copyInto<DrawPattern>(itemOffset, *this);
    case ItemType::DrawPath:
        return copyInto<DrawPath>(itemOffset, *this);
    case ItemType::FillCompositedRect:
        return copyInto<FillCompositedRect>(itemOffset, *this);
    case ItemType::FillPath:
        return copyInto<FillPath>(itemOffset, *this);
    case ItemType::FillRectWithColor:
        return copyInto<FillRectWithColor>(itemOffset, *this);
    case ItemType::FillRectWithGradient:
        return copyInto<FillRectWithGradient>(itemOffset, *this);
    case ItemType::FillRectWithRoundedHole:
        return copyInto<FillRectWithRoundedHole>(itemOffset, *this);
    case ItemType::FillRoundedRect:
        return copyInto<FillRoundedRect>(itemOffset, *this);
    case ItemType::SetLineDash:
        return copyInto<SetLineDash>(itemOffset, *this);
    case ItemType::SetState:
        return copyInto<SetState>(itemOffset, *this);
    case ItemType::StrokePath:
        return copyInto<StrokePath>(itemOffset, *this);
    case ItemType::ApplyDeviceScaleFactor:
        return copyInto<ApplyDeviceScaleFactor>(itemOffset, *this);
#if USE(CG)
    case ItemType::ApplyFillPattern:
        return copyInto<ApplyFillPattern>(itemOffset, *this);
    case ItemType::ApplyStrokePattern:
        return copyInto<ApplyStrokePattern>(itemOffset, *this);
#endif
    case ItemType::BeginTransparencyLayer:
        return copyInto<BeginTransparencyLayer>(itemOffset, *this);
    case ItemType::ClearRect:
        return copyInto<ClearRect>(itemOffset, *this);
    case ItemType::ClearShadow:
        return copyInto<ClearShadow>(itemOffset, *this);
    case ItemType::Clip:
        return copyInto<Clip>(itemOffset, *this);
    case ItemType::ClipOut:
        return copyInto<ClipOut>(itemOffset, *this);
    case ItemType::ClipToImageBuffer:
        return copyInto<ClipToImageBuffer>(itemOffset, *this);
    case ItemType::ConcatenateCTM:
        return copyInto<ConcatenateCTM>(itemOffset, *this);
    case ItemType::DrawDotsForDocumentMarker:
        return copyInto<DrawDotsForDocumentMarker>(itemOffset, *this);
    case ItemType::DrawEllipse:
        return copyInto<DrawEllipse>(itemOffset, *this);
    case ItemType::DrawLine:
        return copyInto<DrawLine>(itemOffset, *this);
    case ItemType::DrawRect:
        return copyInto<DrawRect>(itemOffset, *this);
    case ItemType::EndTransparencyLayer:
        return copyInto<EndTransparencyLayer>(itemOffset, *this);
    case ItemType::FillEllipse:
        return copyInto<FillEllipse>(itemOffset, *this);
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::FillLine:
        return copyInto<FillLine>(itemOffset, *this);
    case ItemType::FillArc:
        return copyInto<FillArc>(itemOffset, *this);
    case ItemType::FillQuadCurve:
        return copyInto<FillQuadCurve>(itemOffset, *this);
    case ItemType::FillBezierCurve:
        return copyInto<FillBezierCurve>(itemOffset, *this);
#endif
    case ItemType::FillRect:
        return copyInto<FillRect>(itemOffset, *this);
#if ENABLE(VIDEO)
    case ItemType::PaintFrameForMedia:
        return copyInto<PaintFrameForMedia>(itemOffset, *this);
#endif
    case ItemType::Restore:
        return copyInto<Restore>(itemOffset, *this);
    case ItemType::Rotate:
        return copyInto<Rotate>(itemOffset, *this);
    case ItemType::Save:
        return copyInto<Save>(itemOffset, *this);
    case ItemType::Scale:
        return copyInto<Scale>(itemOffset, *this);
    case ItemType::SetCTM:
        return copyInto<SetCTM>(itemOffset, *this);
    case ItemType::SetInlineFillColor:
        return copyInto<SetInlineFillColor>(itemOffset, *this);
    case ItemType::SetInlineStrokeColor:
        return copyInto<SetInlineStrokeColor>(itemOffset, *this);
    case ItemType::SetLineCap:
        return copyInto<SetLineCap>(itemOffset, *this);
    case ItemType::SetLineJoin:
        return copyInto<SetLineJoin>(itemOffset, *this);
    case ItemType::SetMiterLimit:
        return copyInto<SetMiterLimit>(itemOffset, *this);
    case ItemType::SetStrokeThickness:
        return copyInto<SetStrokeThickness>(itemOffset, *this);
    case ItemType::StrokeEllipse:
        return copyInto<StrokeEllipse>(itemOffset, *this);
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::StrokeArc:
        return copyInto<StrokeArc>(itemOffset, *this);
    case ItemType::StrokeQuadCurve:
        return copyInto<StrokeQuadCurve>(itemOffset, *this);
    case ItemType::StrokeBezierCurve:
        return copyInto<StrokeBezierCurve>(itemOffset, *this);
#endif
    case ItemType::StrokeRect:
        return copyInto<StrokeRect>(itemOffset, *this);
    case ItemType::StrokeLine:
        return copyInto<StrokeLine>(itemOffset, *this);
    case ItemType::Translate:
        return copyInto<Translate>(itemOffset, *this);
    }
    return false;
}

bool safeCopy(ItemHandle destination, const DisplayListItem& source)
{
    return std::visit([&](const auto& source) {
        using DisplayListItemType = std::remove_cvref_t<decltype(source)>;
        constexpr auto itemType = DisplayListItemType::itemType;
        destination.data[0] = static_cast<uint8_t>(itemType);
        auto itemOffset = destination.data + sizeof(uint64_t);
        return copyInto<DisplayListItemType>(itemOffset, source);
    }, source);
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
    auto* buffer = static_cast<uint8_t*>(fastMalloc(newBufferCapacity));
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
    for (auto* buffer : std::exchange(m_allocatedBuffers, { }))
        fastFree(buffer);

    m_readOnlyBuffers.clear();
    m_writableBuffer = { };
    m_writtenNumberOfBytes = 0;
}

void ItemBuffer::shrinkToFit()
{
    m_allocatedBuffers.shrinkToFit();
}

DidChangeItemBuffer ItemBuffer::swapWritableBufferIfNeeded(size_t numberOfBytes)
{
    if (m_writtenNumberOfBytes + numberOfBytes <= m_writableBuffer.capacity)
        return DidChangeItemBuffer::No;

    auto nextBuffer = createItemBuffer(numberOfBytes);
    bool hadPreviousBuffer = m_writableBuffer && m_writableBuffer.identifier != nextBuffer.identifier;
    if (hadPreviousBuffer) {
        m_writableBuffer.capacity = m_writtenNumberOfBytes;
        m_readOnlyBuffers.append(m_writableBuffer);
    }
    m_writtenNumberOfBytes = 0;
    m_writableBuffer = WTFMove(nextBuffer);
    return hadPreviousBuffer ? DidChangeItemBuffer::Yes : DidChangeItemBuffer::No;
}

void ItemBuffer::append(const DisplayListItem& temporaryItem)
{
    auto requiredSizeForItem = m_writingClient->requiredSizeForItem(temporaryItem);
    RefPtr<FragmentedSharedBuffer> outOfLineItem;
    if (!requiredSizeForItem) {
        outOfLineItem = m_writingClient->encodeItemOutOfLine(temporaryItem);
        if (!outOfLineItem)
            return;
    }

    auto dataLength = valueOrCompute(requiredSizeForItem, [&] {
        ASSERT(outOfLineItem);
        return outOfLineItem->size();
    });
    auto additionalCapacityForEncodedItem = 2 * sizeof(uint64_t) + roundUpToMultipleOf(alignof(uint64_t), dataLength);

    auto bufferChanged = swapWritableBufferIfNeeded(additionalCapacityForEncodedItem);

    m_writableBuffer.data[m_writtenNumberOfBytes] = static_cast<uint8_t>(displayListItemType(temporaryItem));
    reinterpret_cast<uint64_t*>(m_writableBuffer.data + m_writtenNumberOfBytes)[1] = dataLength;
    auto* location = m_writableBuffer.data + m_writtenNumberOfBytes + 2 * sizeof(uint64_t);

    if (requiredSizeForItem)
        m_writingClient->encodeItemInline(temporaryItem, location);
    else
        outOfLineItem->copyTo(location, dataLength);

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
