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
#include "BitmapTexturePool.h"
#include "CoordinatedPlatformLayer.h"
#include "CoordinatedPlatformLayerBufferRGB.h"
#include "GLContext.h"
#include "GLFence.h"
#include "GraphicsContextSkia.h"
#include "PlatformDisplay.h"
#include "ScrollerImpAdwaita.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkSurface.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
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
        Locker layerLocker { hostLayer->lock() };
        hostLayer->setContentsBuffer(nullptr);
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

    if (rect.isEmpty())
        return;

    if (m_orientation == ScrollbarOrientation::Horizontal)
        rect = rect.transposedRect();

    const int minimumThumbLength = AdwaitaScrollbarPainter::minimumThumbSize;
    if (values.visibleSize <= minimumThumbLength)
        state.thumbLength = 0;
    else
        state.thumbLength = std::max<int>(values.visibleSize * values.proportion, minimumThumbLength);
    state.thumbPosition = (values.visibleSize - state.thumbLength) * values.value;
    state.frameRect = rect;

    auto& display = PlatformDisplay::sharedDisplay();
    auto* glContext = display.skiaGLContext();
    if (!glContext)
        return;

    auto* grContext = display.skiaGrContext();
    RELEASE_ASSERT(grContext);
    GLContext::ScopedGLContextCurrent scopedCurrent(*glContext);

    Ref texture = BitmapTexturePool::singleton().acquireTexture(state.frameRect.size(), { BitmapTexture::Flags::SupportsAlpha });
    auto surface = texture->createSkiaSurface(grContext);
    if (!surface)
        return;

    auto* canvas = surface->getCanvas();
    if (!canvas)
        return;

    canvas->clear(SK_ColorTRANSPARENT);

    GraphicsContextSkia context(*canvas, RenderingMode::Accelerated, RenderingPurpose::DOM);
    scrollerImp->paint(context, state.frameRect, state);

    grContext->flushAndSubmit(surface.get(), GLFence::isSupported(display.glDisplay()) ? GrSyncCpu::kNo : GrSyncCpu::kYes);
    auto buffer = CoordinatedPlatformLayerBufferRGB::create(WTF::move(texture), { TextureMapperFlags::ShouldBlend }, GLFence::create(display.glDisplay()));
    if (!buffer)
        return;

    Locker layerLocker { hostLayer->lock() };
    hostLayer->setContentsRect(state.frameRect);
    hostLayer->setContentsClippingRect(FloatRoundedRect(state.frameRect));
    hostLayer->setContentsBuffer(WTF::move(buffer));
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

void ScrollerCoordinated::setScrollbarColor(const std::optional<ScrollbarColor>& scrollbarColor)
{
    Locker locker { m_lock };
    m_state.scrollbarColor = scrollbarColor;
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
