/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollerCoordinated.h"

#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
#include "CoordinatedPlatformLayer.h"
#include "ImageBuffer.h"
#include "NativeImage.h"
#include "ScrollerImpAdwaita.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollerPairCoordinated);

ScrollerCoordinated::ScrollerCoordinated(ScrollerPairCoordinated& pair, ScrollbarOrientation orientation)
    : m_pair(pair)
    , m_orientation(orientation)
{
    m_state.orientation = m_orientation;
    m_state.pressedPart = NoPart;
}

ScrollerCoordinated::~ScrollerCoordinated() = default;

void ScrollerCoordinated::setScrollerImp(ScrollerImpAdwaita* scrollerImp)
{
    Locker locker { m_lock };
    m_scrollerImp = scrollerImp;
    m_needsUpdate = true;
}

void ScrollerCoordinated::setHostLayer(CoordinatedPlatformLayer* layer)
{
    Locker locker { m_lock };
    m_hostLayer = layer;
    m_needsUpdate = true;
}

void ScrollerCoordinated::updateValues()
{
    RefPtr<CoordinatedPlatformLayer> hostLayer;
    RefPtr<ScrollerImpAdwaita> scrollerImp;
    {
        Locker locker { m_lock };
        scrollerImp = m_scrollerImp;
        hostLayer = m_hostLayer;
    }

    if (!hostLayer)
        return;

    if (!scrollerImp) {
        // Custom scrollbars are painted by RenderScrollbar
        hostLayer->setContentsScrollbarImageForScrolling(nullptr);
        return;
    }

    RefPtr pair = m_pair.get();
    auto values = pair->valuesForOrientation(m_orientation);
    AdwaitaScrollbarPainter::State state;

    {
        Locker locker { m_lock };
        if (m_currentValue != values)
            m_needsUpdate = true;
        m_currentValue = values;
        if (!m_needsUpdate)
            return;
        m_needsUpdate = false;
        state = m_state;
    }

    int width = AdwaitaScrollbarPainter::scrollbarSize;
    IntRect rect { 0, 0, width, int(values.visibleSize) };

    if (m_orientation == ScrollbarOrientation::Horizontal)
        rect = rect.transposedRect();

    const int minimumThumbLength = AdwaitaScrollbarPainter::minimumThumbSize;
    if (values.visibleSize <= minimumThumbLength)
        state.thumbLength = 0;
    else
        state.thumbLength = std::max<int>(values.visibleSize * values.proportion, minimumThumbLength);
    state.thumbPosition = (values.visibleSize - state.thumbLength) * values.value;
    state.frameRect = rect;

    RefPtr imageBuffer = ImageBuffer::create(state.frameRect.size(), RenderingMode::Accelerated, RenderingPurpose::DOM, 1, DestinationColorSpace::SRGB(), PixelFormat::RGBA8);
    if (!imageBuffer)
        return;
    scrollerImp->paint(imageBuffer->context(), state.frameRect, state);
    RefPtr nativeImage = ImageBuffer::sinkIntoNativeImage(WTF::move(imageBuffer));
    if (!nativeImage)
        return;
    hostLayer->setContentsScrollbarImageForScrolling(WTF::move(nativeImage));
}

void ScrollerCoordinated::setHoveredAndPressedParts(ScrollbarPart hoveredPart, ScrollbarPart pressedPart)
{
    Locker locker { m_lock };
    m_state.hoveredPart = hoveredPart;
    m_state.pressedPart = pressedPart;
    m_needsUpdate = true;
}

void ScrollerCoordinated::setEnabled(bool enabled)
{
    Locker locker { m_lock };
    m_state.enabled = enabled;
    m_needsUpdate = true;
}

void ScrollerCoordinated::setOverlayScrollbarEnabled(bool enabled)
{
    Locker locker { m_lock };
    m_state.usesOverlayScrollbars = enabled;
    m_needsUpdate = true;
}

void ScrollerCoordinated::setUseDarkAppearance(bool isDark)
{
    Locker locker { m_lock };
    m_state.useDarkAppearanceForScrollbars = isDark;
    m_needsUpdate = true;
}

void ScrollerCoordinated::setOpacity(float opacity)
{
    Locker locker { m_lock };
    m_state.opacity = opacity;
    m_needsUpdate = true;
}

void ScrollerCoordinated::setScrollbarLayoutDirection(UserInterfaceLayoutDirection direction)
{
    Locker locker { m_lock };
    m_state.shouldPlaceVerticalScrollbarOnLeft = direction == UserInterfaceLayoutDirection::RTL;
    m_needsUpdate = true;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
