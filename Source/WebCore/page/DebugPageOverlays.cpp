/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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
#include "DebugPageOverlays.h"

#include "ElementIterator.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "MainFrame.h"
#include "Page.h"
#include "PageOverlay.h"
#include "PageOverlayController.h"
#include "Region.h"
#include "ScrollingCoordinator.h"
#include "Settings.h"

namespace WebCore {

DebugPageOverlays* DebugPageOverlays::sharedDebugOverlays;

class RegionOverlay : public RefCounted<RegionOverlay>, public PageOverlay::Client {
public:
    static Ref<RegionOverlay> create(MainFrame&, DebugPageOverlays::RegionType);
    virtual ~RegionOverlay();

    void recomputeRegion();
    PageOverlay& overlay() { return *m_overlay; }

protected:
    RegionOverlay(MainFrame&, Color);

private:
    void willMoveToPage(PageOverlay&, Page*) final;
    void didMoveToPage(PageOverlay&, Page*) final;
    void drawRect(PageOverlay&, GraphicsContext&, const IntRect& dirtyRect) final;
    bool mouseEvent(PageOverlay&, const PlatformMouseEvent&) final;
    void didScrollFrame(PageOverlay&, Frame&) final;

protected:
    // Returns true if the region changed.
    virtual bool updateRegion() = 0;
    
    MainFrame& m_frame;
    RefPtr<PageOverlay> m_overlay;
    std::unique_ptr<Region> m_region;
    Color m_color;
};

class MouseWheelRegionOverlay final : public RegionOverlay {
public:
    static Ref<MouseWheelRegionOverlay> create(MainFrame& frame)
    {
        return adoptRef(*new MouseWheelRegionOverlay(frame));
    }

private:
    explicit MouseWheelRegionOverlay(MainFrame& frame)
        : RegionOverlay(frame, Color(0.5f, 0.0f, 0.0f, 0.4f))
    {
    }

    bool updateRegion() override;
};

bool MouseWheelRegionOverlay::updateRegion()
{
    auto region = std::make_unique<Region>();
    
    for (const Frame* frame = &m_frame; frame; frame = frame->tree().traverseNext()) {
        if (!frame->view() || !frame->document())
            continue;

        auto frameRegion = frame->document()->absoluteRegionForEventTargets(frame->document()->wheelEventTargets());
        frameRegion.first.translate(toIntSize(frame->view()->contentsToRootView(IntPoint())));
        region->unite(frameRegion.first);
    }
    
    region->translate(m_overlay->viewToOverlayOffset());

    bool regionChanged = !m_region || !(*m_region == *region);
    m_region = WTFMove(region);
    return regionChanged;
}

class NonFastScrollableRegionOverlay final : public RegionOverlay {
public:
    static Ref<NonFastScrollableRegionOverlay> create(MainFrame& frame)
    {
        return adoptRef(*new NonFastScrollableRegionOverlay(frame));
    }

private:
    explicit NonFastScrollableRegionOverlay(MainFrame& frame)
        : RegionOverlay(frame, Color(1.0f, 0.5f, 0.0f, 0.4f))
    {
    }

    bool updateRegion() override;
};

bool NonFastScrollableRegionOverlay::updateRegion()
{
    auto region = std::make_unique<Region>();

    if (Page* page = m_frame.page()) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator()) {
            EventTrackingRegions eventTrackingRegions = scrollingCoordinator->absoluteEventTrackingRegions();
            for (const auto& synchronousEventRegion : eventTrackingRegions.eventSpecificSynchronousDispatchRegions)
                region->unite(synchronousEventRegion.value);
        }
    }

    bool regionChanged = !m_region || !(*m_region == *region);
    m_region = WTFMove(region);
    return regionChanged;
}

Ref<RegionOverlay> RegionOverlay::create(MainFrame& frame, DebugPageOverlays::RegionType regionType)
{
    switch (regionType) {
    case DebugPageOverlays::RegionType::WheelEventHandlers:
        return MouseWheelRegionOverlay::create(frame);
    case DebugPageOverlays::RegionType::NonFastScrollableRegion:
        return NonFastScrollableRegionOverlay::create(frame);
    }
    ASSERT_NOT_REACHED();
    return MouseWheelRegionOverlay::create(frame);
}

RegionOverlay::RegionOverlay(MainFrame& frame, Color regionColor)
    : m_frame(frame)
    , m_overlay(PageOverlay::create(*this, PageOverlay::OverlayType::Document))
    , m_color(regionColor)
{
}

RegionOverlay::~RegionOverlay()
{
    if (m_overlay)
        m_frame.pageOverlayController().uninstallPageOverlay(*m_overlay, PageOverlay::FadeMode::DoNotFade);
}

void RegionOverlay::willMoveToPage(PageOverlay&, Page* page)
{
    if (!page)
        m_overlay = nullptr;
}

void RegionOverlay::didMoveToPage(PageOverlay&, Page* page)
{
    if (page)
        recomputeRegion();
}

void RegionOverlay::drawRect(PageOverlay&, GraphicsContext& context, const IntRect& dirtyRect)
{
    context.clearRect(dirtyRect);

    if (!m_region)
        return;

    GraphicsContextStateSaver saver(context);
    context.setFillColor(m_color);
    for (auto rect : m_region->rects()) {
        if (rect.intersects(dirtyRect))
            context.fillRect(rect);
    }
}

bool RegionOverlay::mouseEvent(PageOverlay&, const PlatformMouseEvent&)
{
    return false;
}

void RegionOverlay::didScrollFrame(PageOverlay&, Frame&)
{
}

void RegionOverlay::recomputeRegion()
{
    if (updateRegion())
        m_overlay->setNeedsDisplay();
}

DebugPageOverlays& DebugPageOverlays::singleton()
{
    if (!sharedDebugOverlays)
        sharedDebugOverlays = new DebugPageOverlays;

    return *sharedDebugOverlays;
}

static inline size_t indexOf(DebugPageOverlays::RegionType regionType)
{
    return static_cast<size_t>(regionType);
}

RegionOverlay& DebugPageOverlays::ensureRegionOverlayForFrame(MainFrame& frame, RegionType regionType)
{
    auto it = m_frameRegionOverlays.find(&frame);
    if (it != m_frameRegionOverlays.end()) {
        auto& visualizer = it->value[indexOf(regionType)];
        if (!visualizer)
            visualizer = RegionOverlay::create(frame, regionType);
        return *visualizer;
    }

    Vector<RefPtr<RegionOverlay>> visualizers(NumberOfRegionTypes);
    auto visualizer = RegionOverlay::create(frame, regionType);
    visualizers[indexOf(regionType)] = visualizer.copyRef();
    m_frameRegionOverlays.add(&frame, WTFMove(visualizers));
    return visualizer;
}

void DebugPageOverlays::showRegionOverlay(MainFrame& frame, RegionType regionType)
{
    auto& visualizer = ensureRegionOverlayForFrame(frame, regionType);
    frame.pageOverlayController().installPageOverlay(visualizer.overlay(), PageOverlay::FadeMode::DoNotFade);
}

void DebugPageOverlays::hideRegionOverlay(MainFrame& frame, RegionType regionType)
{
    auto it = m_frameRegionOverlays.find(&frame);
    if (it == m_frameRegionOverlays.end())
        return;
    auto& visualizer = it->value[indexOf(regionType)];
    if (!visualizer)
        return;
    frame.pageOverlayController().uninstallPageOverlay(visualizer->overlay(), PageOverlay::FadeMode::DoNotFade);
    visualizer = nullptr;
}

void DebugPageOverlays::regionChanged(Frame& frame, RegionType regionType)
{
    if (auto* visualizer = regionOverlayForFrame(frame.mainFrame(), regionType))
        visualizer->recomputeRegion();
}

RegionOverlay* DebugPageOverlays::regionOverlayForFrame(MainFrame& frame, RegionType regionType) const
{
    auto it = m_frameRegionOverlays.find(&frame);
    if (it == m_frameRegionOverlays.end())
        return nullptr;
    return it->value.at(indexOf(regionType)).get();
}

void DebugPageOverlays::updateOverlayRegionVisibility(MainFrame& frame, DebugOverlayRegions visibleRegions)
{
    if (visibleRegions & NonFastScrollableRegion)
        showRegionOverlay(frame, RegionType::NonFastScrollableRegion);
    else
        hideRegionOverlay(frame, RegionType::NonFastScrollableRegion);

    if (visibleRegions & WheelEventHandlerRegion)
        showRegionOverlay(frame, RegionType::WheelEventHandlers);
    else
        hideRegionOverlay(frame, RegionType::WheelEventHandlers);
}

void DebugPageOverlays::settingsChanged(MainFrame& frame)
{
    DebugOverlayRegions activeOverlayRegions = frame.settings().visibleDebugOverlayRegions();
    if (!activeOverlayRegions && !hasOverlays(frame))
        return;

    DebugPageOverlays::singleton().updateOverlayRegionVisibility(frame, activeOverlayRegions);
}

}
