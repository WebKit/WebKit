/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderEmbeddedObject.h"

#include "CSSValueKeywords.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Cursor.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FontCascade.h"
#include "FontSelector.h"
#include "GraphicsContext.h"
#include "HTMLEmbedElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLParamElement.h"
#include "HTMLPlugInElement.h"
#include "HitTestResult.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "Page.h"
#include "PaintInfo.h"
#include "Path.h"
#include "PlatformMouseEvent.h"
#include "PluginViewBase.h"
#include "RenderBoxInlines.h"
#include "RenderLayoutState.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "Settings.h"
#include "SystemFontDatabase.h"
#include "Text.h"
#include "TextRun.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderEmbeddedObject);

static const float replacementTextRoundedRectHeight = 22;
static const float replacementTextRoundedRectLeftTextMargin = 10;
static const float replacementTextRoundedRectRightTextMargin = 10;
static const float replacementTextRoundedRectRightTextMarginWithArrow = 5;
static const float replacementTextRoundedRectTopTextMargin = -1;
static const float replacementTextRoundedRectRadius = 11;
static const float replacementArrowLeftMargin = -4;
static const float replacementArrowPadding = 4;
static const float replacementArrowCirclePadding = 3;

static constexpr auto replacementTextRoundedRectPressedColor = SRGBA<uint8_t> { 105, 105, 105, 242 };
static constexpr auto replacementTextRoundedRectColor = SRGBA<uint8_t> { 125, 125, 125, 242 };
static constexpr auto replacementTextColor = SRGBA<uint8_t> { 240, 240, 240 };
static constexpr auto unavailablePluginBorderColor = Color::white.colorWithAlphaByte(216);

RenderEmbeddedObject::RenderEmbeddedObject(HTMLFrameOwnerElement& element, RenderStyle&& style)
    : RenderWidget(Type::EmbeddedObject, element, WTFMove(style))
    , m_isPluginUnavailable(false)
    , m_unavailablePluginIndicatorIsPressed(false)
    , m_mouseDownWasInUnavailablePluginIndicator(false)
{
    ASSERT(isRenderEmbeddedObject());
}

RenderEmbeddedObject::~RenderEmbeddedObject()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
}

void RenderEmbeddedObject::willBeDestroyed()
{
    view().frameView().removeEmbeddedObjectToUpdate(*this);
    RenderWidget::willBeDestroyed();
}

bool RenderEmbeddedObject::requiresAcceleratedCompositing() const
{
    if (RenderWidget::requiresAcceleratedCompositing())
        return true;

    auto* pluginViewBase = dynamicDowncast<PluginViewBase>(widget());
    if (!pluginViewBase)
        return false;
    return pluginViewBase->layerHostingStrategy() != PluginLayerHostingStrategy::None;
}

bool RenderEmbeddedObject::usesAsyncScrolling() const
{
    auto* pluginViewBase = dynamicDowncast<PluginViewBase>(widget());
    if (!pluginViewBase)
        return false;
    return pluginViewBase->usesAsyncScrolling();
}

ScrollingNodeID RenderEmbeddedObject::scrollingNodeID() const
{
    auto* pluginViewBase = dynamicDowncast<PluginViewBase>(widget());
    if (!pluginViewBase)
        return false;
    return pluginViewBase->scrollingNodeID();
}

#if !PLATFORM(IOS_FAMILY)
static String unavailablePluginReplacementText(RenderEmbeddedObject::PluginUnavailabilityReason pluginUnavailabilityReason)
{
    switch (pluginUnavailabilityReason) {
    case RenderEmbeddedObject::PluginMissing:
        return missingPluginText();
    case RenderEmbeddedObject::PluginCrashed:
        return crashedPluginText();
    case RenderEmbeddedObject::PluginBlockedByContentSecurityPolicy:
        return blockedPluginByContentSecurityPolicyText();
    case RenderEmbeddedObject::InsecurePluginVersion:
        return insecurePluginVersionText();
    case RenderEmbeddedObject::UnsupportedPlugin:
        return unsupportedPluginText();
    case RenderEmbeddedObject::PluginTooSmall:
        return pluginTooSmallText();
    }

    ASSERT_NOT_REACHED();
    return String();
}
#endif

static bool shouldUnavailablePluginMessageBeButton(Page& page, RenderEmbeddedObject::PluginUnavailabilityReason pluginUnavailabilityReason)
{
    return page.chrome().client().shouldUnavailablePluginMessageBeButton(pluginUnavailabilityReason);
}

void RenderEmbeddedObject::setPluginUnavailabilityReason(PluginUnavailabilityReason pluginUnavailabilityReason)
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(pluginUnavailabilityReason);
#else
    setPluginUnavailabilityReasonWithDescription(pluginUnavailabilityReason, unavailablePluginReplacementText(pluginUnavailabilityReason));
#endif
}

void RenderEmbeddedObject::setPluginUnavailabilityReasonWithDescription(PluginUnavailabilityReason pluginUnavailabilityReason, const String& description)
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(pluginUnavailabilityReason);
    UNUSED_PARAM(description);
#else
    ASSERT(!m_isPluginUnavailable);
    m_isPluginUnavailable = true;
    m_pluginUnavailabilityReason = pluginUnavailabilityReason;

    if (description.isEmpty())
        m_unavailablePluginReplacementText = unavailablePluginReplacementText(pluginUnavailabilityReason);
    else
        m_unavailablePluginReplacementText = description;
#endif
}

void RenderEmbeddedObject::setUnavailablePluginIndicatorIsPressed(bool pressed)
{
    if (m_unavailablePluginIndicatorIsPressed == pressed)
        return;
    m_unavailablePluginIndicatorIsPressed = pressed;
    repaint();
}

void RenderEmbeddedObject::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // The relevant repainted object heuristic is not tuned for plugin documents.
    bool countsTowardsRelevantObjects = !document().isPluginDocument() && paintInfo.phase == PaintPhase::Foreground;

    if (isPluginUnavailable()) {
        if (countsTowardsRelevantObjects)
            page().addRelevantUnpaintedObject(*this, visualOverflowRect());
        RenderReplaced::paint(paintInfo, paintOffset);
        return;
    }

    if (countsTowardsRelevantObjects)
        page().addRelevantRepaintedObject(*this, visualOverflowRect());

    RenderWidget::paint(paintInfo, paintOffset);
}

static void drawReplacementArrow(GraphicsContext& context, const FloatRect& insideRect)
{
    GraphicsContextStateSaver stateSaver(context);

    FloatRect rect(insideRect);
    rect.inflate(-replacementArrowPadding);

    FloatPoint center(rect.center());
    FloatPoint arrowTip(rect.maxX(), center.y());

    context.setStrokeThickness(2);
    context.setLineCap(LineCap::Round);
    context.setLineJoin(LineJoin::Round);

    Path path;
    path.moveTo(FloatPoint(rect.x(), center.y()));
    path.addLineTo(arrowTip);
    path.addLineTo(FloatPoint(center.x(), rect.y()));
    path.moveTo(arrowTip);
    path.addLineTo(FloatPoint(center.x(), rect.maxY()));
    context.strokePath(path);
}

void RenderEmbeddedObject::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!showsUnavailablePluginIndicator())
        return;

    if (paintInfo.phase == PaintPhase::Selection)
        return;

    GraphicsContext& context = paintInfo.context();
    if (context.paintingDisabled())
        return;

    FloatRect contentRect;
    FloatRect indicatorRect;
    FloatRect replacementTextRect;
    FloatRect arrowRect;
    FontCascade font;
    TextRun run(emptyString());
    float textWidth;
    getReplacementTextGeometry(paintOffset, contentRect, indicatorRect, replacementTextRect, arrowRect, font, run, textWidth);

    Path background;
    background.addRoundedRect(indicatorRect, FloatSize(replacementTextRoundedRectRadius, replacementTextRoundedRectRadius));

    GraphicsContextStateSaver stateSaver(context);
    context.clip(contentRect);
    context.setFillColor(m_unavailablePluginIndicatorIsPressed ? replacementTextRoundedRectPressedColor : replacementTextRoundedRectColor);
    context.fillPath(background);

    Path strokePath;
    FloatRect strokeRect(indicatorRect);
    strokeRect.inflate(1);
    strokePath.addRoundedRect(strokeRect, FloatSize(replacementTextRoundedRectRadius + 1, replacementTextRoundedRectRadius + 1));

    context.setStrokeColor(unavailablePluginBorderColor);
    context.setStrokeThickness(2);
    context.strokePath(strokePath);

    const FontMetrics& fontMetrics = font.metricsOfPrimaryFont();
    float labelX = roundf(replacementTextRect.location().x() + replacementTextRoundedRectLeftTextMargin);
    float labelY = roundf(replacementTextRect.location().y() + (replacementTextRect.size().height() - fontMetrics.height()) / 2 + fontMetrics.ascent() + replacementTextRoundedRectTopTextMargin);
    context.setFillColor(replacementTextColor);
    context.drawBidiText(font, run, FloatPoint(labelX, labelY));

    if (shouldUnavailablePluginMessageBeButton(page(), m_pluginUnavailabilityReason)) {
        arrowRect.inflate(-replacementArrowCirclePadding);

        context.beginTransparencyLayer(1.0);
        context.setFillColor(replacementTextColor);
        context.fillEllipse(arrowRect);

        context.setCompositeOperation(CompositeOperator::Clear);
        drawReplacementArrow(context, arrowRect);
        context.endTransparencyLayer();
    }
}

void RenderEmbeddedObject::setUnavailablePluginIndicatorIsHidden(bool hidden)
{
    auto newState = hidden ? UnavailablePluginIndicatorState::Hidden : UnavailablePluginIndicatorState::Visible;
    if (m_isUnavailablePluginIndicatorState == newState)
        return;
    m_isUnavailablePluginIndicatorState = newState;
    repaint();
}

LayoutRect RenderEmbeddedObject::getReplacementTextGeometry(const LayoutPoint& accumulatedOffset) const
{
    FloatRect contentRect;
    FloatRect indicatorRect;
    FloatRect replacementTextRect;
    FloatRect arrowRect;
    FontCascade font;
    TextRun run(emptyString());
    float textWidth;
    getReplacementTextGeometry(accumulatedOffset, contentRect, indicatorRect, replacementTextRect, arrowRect, font, run, textWidth);
    return LayoutRect(indicatorRect);
}

void RenderEmbeddedObject::getReplacementTextGeometry(const LayoutPoint& accumulatedOffset, FloatRect& contentRect, FloatRect& indicatorRect, FloatRect& replacementTextRect, FloatRect& arrowRect, FontCascade& font, TextRun& run, float& textWidth) const
{
    bool includesArrow = shouldUnavailablePluginMessageBeButton(page(), m_pluginUnavailabilityReason);

    contentRect = contentBoxRect();
    contentRect.moveBy(roundedIntPoint(accumulatedOffset));

    FontCascadeDescription fontDescription;
    fontDescription.setOneFamily(SystemFontDatabase::singleton().systemFontShorthandFamily(SystemFontDatabase::FontShorthand::WebkitSmallControl));
    fontDescription.setWeight(boldWeightValue());
    fontDescription.setComputedSize(12);
    font = FontCascade(WTFMove(fontDescription));
    font.update(nullptr);

    run = TextRun(m_unavailablePluginReplacementText);
    textWidth = font.width(run);

    replacementTextRect.setSize(FloatSize(textWidth + replacementTextRoundedRectLeftTextMargin + (includesArrow ? replacementTextRoundedRectRightTextMarginWithArrow : replacementTextRoundedRectRightTextMargin), replacementTextRoundedRectHeight));
    replacementTextRect.setLocation(contentRect.location() + (contentRect.size() / 2 - replacementTextRect.size() / 2));

    indicatorRect = replacementTextRect;

    // Expand the background rect to include the arrow, if it will be used.
    if (includesArrow) {
        arrowRect = indicatorRect;
        arrowRect.setX(ceilf(arrowRect.maxX() + replacementArrowLeftMargin));
        arrowRect.setWidth(arrowRect.height());
        indicatorRect.unite(arrowRect);
    }
}

LayoutRect RenderEmbeddedObject::unavailablePluginIndicatorBounds(const LayoutPoint& accumulatedOffset) const
{
    return getReplacementTextGeometry(accumulatedOffset);
}

void RenderEmbeddedObject::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    updateLogicalWidth();
    updateLogicalHeight();

    RenderWidget::layout();

    clearOverflow();
    addVisualEffectOverflow();

    updateLayerTransform();

    if (!widget())
        view().frameView().addEmbeddedObjectToUpdate(*this);

    clearNeedsLayout();
}

bool RenderEmbeddedObject::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (!RenderWidget::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, hitTestAction))
        return false;

    if (!is<PluginViewBase>(widget()))
        return true;

    PluginViewBase& view = downcast<PluginViewBase>(*widget());
    IntPoint roundedPoint = locationInContainer.roundedPoint();

    if (Scrollbar* horizontalScrollbar = view.horizontalScrollbar()) {
        if (horizontalScrollbar->shouldParticipateInHitTesting() && horizontalScrollbar->frameRect().contains(roundedPoint)) {
            result.setScrollbar(horizontalScrollbar);
            return true;
        }
    }

    if (Scrollbar* verticalScrollbar = view.verticalScrollbar()) {
        if (verticalScrollbar->shouldParticipateInHitTesting() && verticalScrollbar->frameRect().contains(roundedPoint)) {
            result.setScrollbar(verticalScrollbar);
            return true;
        }
    }

    return true;
}

bool RenderEmbeddedObject::scroll(ScrollDirection direction, ScrollGranularity granularity, unsigned, Element**, RenderBox*, const IntPoint&)
{
    RefPtr pluginView = dynamicDowncast<PluginViewBase>(widget());
    return pluginView && pluginView->scroll(direction, granularity);
}

bool RenderEmbeddedObject::logicalScroll(ScrollLogicalDirection direction, ScrollGranularity granularity, unsigned stepCount, Element** stopElement)
{
    // Plugins don't expose a writing direction, so assuming horizontal LTR.
    return scroll(logicalToPhysical(direction, true, false), granularity, stepCount, stopElement);
}

bool RenderEmbeddedObject::isInUnavailablePluginIndicator(const FloatPoint& point) const
{
    return getReplacementTextGeometry(LayoutPoint()).contains(LayoutPoint(point));
}

bool RenderEmbeddedObject::isInUnavailablePluginIndicator(const MouseEvent& event) const
{
    return isInUnavailablePluginIndicator(absoluteToLocal(event.absoluteLocation(), UseTransforms));
}

void RenderEmbeddedObject::handleUnavailablePluginIndicatorEvent(Event* event)
{
    if (!shouldUnavailablePluginMessageBeButton(page(), m_pluginUnavailabilityReason))
        return;

    RefPtr mouseEvent = dynamicDowncast<MouseEvent>(*event);
    if (!mouseEvent)
        return;

    Ref element = downcast<HTMLPlugInElement>(frameOwnerElement());
    if (mouseEvent->type() == eventNames().mousedownEvent && mouseEvent->button() == MouseButton::Left) {
        m_mouseDownWasInUnavailablePluginIndicator = isInUnavailablePluginIndicator(*mouseEvent);
        if (m_mouseDownWasInUnavailablePluginIndicator) {
            frame().eventHandler().setCapturingMouseEventsElement(element.copyRef());
            element->setIsCapturingMouseEvents(true);
            setUnavailablePluginIndicatorIsPressed(true);
        }
        mouseEvent->setDefaultHandled();
    }
    if (mouseEvent->type() == eventNames().mouseupEvent && mouseEvent->button() == MouseButton::Left) {
        if (m_unavailablePluginIndicatorIsPressed) {
            frame().eventHandler().setCapturingMouseEventsElement(nullptr);
            element->setIsCapturingMouseEvents(false);
            setUnavailablePluginIndicatorIsPressed(false);
        }
        if (m_mouseDownWasInUnavailablePluginIndicator && isInUnavailablePluginIndicator(*mouseEvent)) {
            page().chrome().client().unavailablePluginButtonClicked(element.get(), m_pluginUnavailabilityReason);
        }
        m_mouseDownWasInUnavailablePluginIndicator = false;
        event->setDefaultHandled();
    }
    if (mouseEvent->type() == eventNames().mousemoveEvent) {
        setUnavailablePluginIndicatorIsPressed(m_mouseDownWasInUnavailablePluginIndicator && isInUnavailablePluginIndicator(*mouseEvent));
        mouseEvent->setDefaultHandled();
    }
}

CursorDirective RenderEmbeddedObject::getCursor(const LayoutPoint& point, Cursor& cursor) const
{
    if (showsUnavailablePluginIndicator() && shouldUnavailablePluginMessageBeButton(page(), m_pluginUnavailabilityReason) && isInUnavailablePluginIndicator(point)) {
        cursor = handCursor();
        return SetCursor;
    }
    if (widget() && widget()->isPluginViewBase()) {
        // A plug-in is responsible for setting the cursor when the pointer is over it.
        return DoNotSetCursor;
    }
    return RenderWidget::getCursor(point, cursor);
}

}
