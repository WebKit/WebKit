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

#include "ColorHash.h"
#include "ElementIterator.h"
#include "FrameView.h"
#include "GraphicsContext.h"
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
    static Ref<RegionOverlay> create(Page&, DebugPageOverlays::RegionType);
    virtual ~RegionOverlay();

    void recomputeRegion();
    PageOverlay& overlay() { return *m_overlay; }

    void setRegionChanged() { m_regionChanged = true; }

protected:
    RegionOverlay(Page&, Color);

private:
    void willMoveToPage(PageOverlay&, Page*) final;
    void didMoveToPage(PageOverlay&, Page*) final;
    void drawRect(PageOverlay&, GraphicsContext&, const IntRect& dirtyRect) override;
    bool mouseEvent(PageOverlay&, const PlatformMouseEvent&) final;
    void didScrollFrame(PageOverlay&, Frame&) final;

protected:
    // Returns true if the region changed.
    virtual bool updateRegion() = 0;
    void drawRegion(GraphicsContext&, const Region&, const Color&, const IntRect& dirtyRect);
    
    Page& m_page;
    RefPtr<PageOverlay> m_overlay;
    std::unique_ptr<Region> m_region;
    Color m_color;
    bool m_regionChanged { true };
};

class MouseWheelRegionOverlay final : public RegionOverlay {
public:
    static Ref<MouseWheelRegionOverlay> create(Page& page)
    {
        return adoptRef(*new MouseWheelRegionOverlay(page));
    }

private:
    explicit MouseWheelRegionOverlay(Page& page)
        : RegionOverlay(page, makeSimpleColorFromFloats(0.5f, 0.0f, 0.0f, 0.4f))
    {
    }

    bool updateRegion() override;
};

bool MouseWheelRegionOverlay::updateRegion()
{
    auto region = makeUnique<Region>();
    
    for (const Frame* frame = &m_page.mainFrame(); frame; frame = frame->tree().traverseNext()) {
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
    static Ref<NonFastScrollableRegionOverlay> create(Page& page)
    {
        return adoptRef(*new NonFastScrollableRegionOverlay(page));
    }

private:
    explicit NonFastScrollableRegionOverlay(Page& page)
        : RegionOverlay(page, makeSimpleColorFromFloats(1.0f, 0.5f, 0.0f, 0.4f))
    {
    }

    bool updateRegion() override;
    void drawRect(PageOverlay&, GraphicsContext&, const IntRect& dirtyRect) final;
    
    EventTrackingRegions m_eventTrackingRegions;
};

bool NonFastScrollableRegionOverlay::updateRegion()
{
    bool regionChanged = false;

    if (ScrollingCoordinator* scrollingCoordinator = m_page.scrollingCoordinator()) {
        EventTrackingRegions eventTrackingRegions = scrollingCoordinator->absoluteEventTrackingRegions();

        if (eventTrackingRegions != m_eventTrackingRegions) {
            m_eventTrackingRegions = eventTrackingRegions;
            regionChanged = true;
        }
    }

    return regionChanged;
}

static const HashMap<String, SimpleColor>& touchEventRegionColors()
{
    static const auto regionColors = makeNeverDestroyed([] {
        struct MapEntry {
            ASCIILiteral name;
            int r;
            int g;
            int b;
        };
        static const MapEntry entries[] = {
            { "touchstart"_s, 191, 191, 63 },
            { "touchmove"_s, 80, 204, 245 },
            { "touchend"_s, 191, 63, 127 },
            { "touchforcechange"_s, 63, 63, 191 },
            { "wheel"_s, 255, 128, 0 },
            { "mousedown"_s, 80, 245, 80 },
            { "mousemove"_s, 245, 245, 80 },
            { "mouseup"_s, 80, 245, 176 },
        };
        HashMap<String, SimpleColor> map;
        for (auto& entry : entries)
            map.add(entry.name, makeSimpleColor(entry.r, entry.g, entry.b, 50));
        return map;
    }());
    return regionColors;
}

static void drawRightAlignedText(const String& text, GraphicsContext& context, const FontCascade& font, const FloatPoint& boxLocation)
{
    float textGap = 10;
    float textBaselineFromTop = 14;

    TextRun textRun = TextRun(text);
    context.setFillColor(Color::transparent);
    float textWidth = context.drawText(font, textRun, { });
    context.setFillColor(Color::black);
    context.drawText(font, textRun, boxLocation + FloatSize(-(textWidth + textGap), textBaselineFromTop));
}

void NonFastScrollableRegionOverlay::drawRect(PageOverlay& pageOverlay, GraphicsContext& context, const IntRect&)
{
    IntRect bounds = pageOverlay.bounds();
    
    context.clearRect(bounds);
    
    FloatRect legendRect = { bounds.maxX() - 30.0f, 10, 20, 20 };
    
    FontCascadeDescription fontDescription;
    fontDescription.setOneFamily("Helvetica");
    fontDescription.setSpecifiedSize(12);
    fontDescription.setComputedSize(12);
    fontDescription.setWeight(FontSelectionValue(500));
    FontCascade font(WTFMove(fontDescription), 0, 0);
    font.update(nullptr);

#if ENABLE(TOUCH_EVENTS)
    context.setFillColor(touchEventRegionColors().get("touchstart"));
    context.fillRect(legendRect);
    drawRightAlignedText("touchstart", context, font, legendRect.location());

    legendRect.move(0, 30);
    context.setFillColor(touchEventRegionColors().get("touchmove"));
    context.fillRect(legendRect);
    drawRightAlignedText("touchmove", context, font, legendRect.location());

    legendRect.move(0, 30);
    context.setFillColor(touchEventRegionColors().get("touchend"));
    context.fillRect(legendRect);
    drawRightAlignedText("touchend", context, font, legendRect.location());

    legendRect.move(0, 30);
    context.setFillColor(touchEventRegionColors().get("touchforcechange"));
    context.fillRect(legendRect);
    drawRightAlignedText("touchforcechange", context, font, legendRect.location());

    legendRect.move(0, 30);
    context.setFillColor(m_color);
    context.fillRect(legendRect);
    drawRightAlignedText("passive listeners", context, font, legendRect.location());

    legendRect.move(0, 30);
    context.setFillColor(touchEventRegionColors().get("mousedown"));
    context.fillRect(legendRect);
    drawRightAlignedText("mousedown", context, font, legendRect.location());

    legendRect.move(0, 30);
    context.setFillColor(touchEventRegionColors().get("mousemove"));
    context.fillRect(legendRect);
    drawRightAlignedText("mousemove", context, font, legendRect.location());

    legendRect.move(0, 30);
    context.setFillColor(touchEventRegionColors().get("mouseup"));
    context.fillRect(legendRect);
    drawRightAlignedText("mouseup", context, font, legendRect.location());
#else
    // On desktop platforms, the "wheel" region includes the non-fast scrollable region.
    context.setFillColor(touchEventRegionColors().get("wheel"));
    context.fillRect(legendRect);
    drawRightAlignedText("non-fast region", context, font, legendRect.location());
#endif

    for (const auto& synchronousEventRegion : m_eventTrackingRegions.eventSpecificSynchronousDispatchRegions) {
        auto regionColor = makeSimpleColor(0, 0, 0, 64);
        auto it = touchEventRegionColors().find(synchronousEventRegion.key);
        if (it != touchEventRegionColors().end())
            regionColor = it->value;
        drawRegion(context, synchronousEventRegion.value, regionColor, bounds);
    }

    drawRegion(context, m_eventTrackingRegions.asynchronousDispatchRegion, m_color, bounds);
}

Ref<RegionOverlay> RegionOverlay::create(Page& page, DebugPageOverlays::RegionType regionType)
{
    switch (regionType) {
    case DebugPageOverlays::RegionType::WheelEventHandlers:
        return MouseWheelRegionOverlay::create(page);
    case DebugPageOverlays::RegionType::NonFastScrollableRegion:
        return NonFastScrollableRegionOverlay::create(page);
    }
    ASSERT_NOT_REACHED();
    return MouseWheelRegionOverlay::create(page);
}

RegionOverlay::RegionOverlay(Page& page, Color regionColor)
    : m_page(page)
    , m_overlay(PageOverlay::create(*this, PageOverlay::OverlayType::Document))
    , m_color(regionColor)
{
}

RegionOverlay::~RegionOverlay()
{
    if (m_overlay)
        m_page.pageOverlayController().uninstallPageOverlay(*m_overlay, PageOverlay::FadeMode::DoNotFade);
}

void RegionOverlay::willMoveToPage(PageOverlay&, Page* page)
{
    if (!page)
        m_overlay = nullptr;
}

void RegionOverlay::didMoveToPage(PageOverlay&, Page* page)
{
    if (page)
        setRegionChanged();
}

void RegionOverlay::drawRect(PageOverlay&, GraphicsContext& context, const IntRect& dirtyRect)
{
    context.clearRect(dirtyRect);

    if (!m_region)
        return;

    drawRegion(context, *m_region, m_color, dirtyRect);
}

void RegionOverlay::drawRegion(GraphicsContext& context, const Region& region, const Color& color, const IntRect& dirtyRect)
{
    GraphicsContextStateSaver saver(context);
    context.setFillColor(color);
    for (auto rect : region.rects()) {
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
    if (!m_regionChanged)
        return;

    if (updateRegion())
        m_overlay->setNeedsDisplay();

    m_regionChanged = false;
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

RegionOverlay& DebugPageOverlays::ensureRegionOverlayForPage(Page& page, RegionType regionType)
{
    auto it = m_pageRegionOverlays.find(&page);
    if (it != m_pageRegionOverlays.end()) {
        auto& visualizer = it->value[indexOf(regionType)];
        if (!visualizer)
            visualizer = RegionOverlay::create(page, regionType);
        return *visualizer;
    }

    Vector<RefPtr<RegionOverlay>> visualizers(NumberOfRegionTypes);
    auto visualizer = RegionOverlay::create(page, regionType);
    visualizers[indexOf(regionType)] = visualizer.copyRef();
    m_pageRegionOverlays.add(&page, WTFMove(visualizers));
    return visualizer;
}

void DebugPageOverlays::showRegionOverlay(Page& page, RegionType regionType)
{
    auto& visualizer = ensureRegionOverlayForPage(page, regionType);
    page.pageOverlayController().installPageOverlay(visualizer.overlay(), PageOverlay::FadeMode::DoNotFade);
}

void DebugPageOverlays::hideRegionOverlay(Page& page, RegionType regionType)
{
    auto it = m_pageRegionOverlays.find(&page);
    if (it == m_pageRegionOverlays.end())
        return;
    auto& visualizer = it->value[indexOf(regionType)];
    if (!visualizer)
        return;
    page.pageOverlayController().uninstallPageOverlay(visualizer->overlay(), PageOverlay::FadeMode::DoNotFade);
    visualizer = nullptr;
}

void DebugPageOverlays::regionChanged(Frame& frame, RegionType regionType)
{
    auto* page = frame.page();
    if (!page)
        return;

    if (auto* visualizer = regionOverlayForPage(*page, regionType))
        visualizer->setRegionChanged();
}

void DebugPageOverlays::updateRegionIfNecessary(Page& page, RegionType regionType)
{
    if (auto* visualizer = regionOverlayForPage(page, regionType))
        visualizer->recomputeRegion();
}

RegionOverlay* DebugPageOverlays::regionOverlayForPage(Page& page, RegionType regionType) const
{
    auto it = m_pageRegionOverlays.find(&page);
    if (it == m_pageRegionOverlays.end())
        return nullptr;
    return it->value.at(indexOf(regionType)).get();
}

void DebugPageOverlays::updateOverlayRegionVisibility(Page& page, DebugOverlayRegions visibleRegions)
{
    if (visibleRegions & NonFastScrollableRegion)
        showRegionOverlay(page, RegionType::NonFastScrollableRegion);
    else
        hideRegionOverlay(page, RegionType::NonFastScrollableRegion);

    if (visibleRegions & WheelEventHandlerRegion)
        showRegionOverlay(page, RegionType::WheelEventHandlers);
    else
        hideRegionOverlay(page, RegionType::WheelEventHandlers);
}

void DebugPageOverlays::settingsChanged(Page& page)
{
    DebugOverlayRegions activeOverlayRegions = page.settings().visibleDebugOverlayRegions();
    if (!activeOverlayRegions && !hasOverlays(page))
        return;

    DebugPageOverlays::singleton().updateOverlayRegionVisibility(page, activeOverlayRegions);
}

}
