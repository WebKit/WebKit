/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
#include "Font.h"
#include "FontSelector.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "GraphicsContext.h"
#include "HTMLAppletElement.h"
#include "HTMLEmbedElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLParamElement.h"
#include "HTMLPlugInElement.h"
#include "HitTestResult.h"
#include "LocalizedStrings.h"
#include "MIMETypeRegistry.h"
#include "MouseEvent.h"
#include "Page.h"
#include "PaintInfo.h"
#include "Path.h"
#include "PlatformMouseEvent.h"
#include "PluginViewBase.h"
#include "RenderLayer.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "Settings.h"
#include "Text.h"
#include "TextRun.h"
#include <wtf/StackStats.h>

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
#include "HTMLMediaElement.h"
#endif

namespace WebCore {

using namespace HTMLNames;

static const float replacementTextRoundedRectHeight = 22;
static const float replacementTextRoundedRectLeftTextMargin = 10;
static const float replacementTextRoundedRectRightTextMargin = 10;
static const float replacementTextRoundedRectRightTextMarginWithArrow = 5;
static const float replacementTextRoundedRectTopTextMargin = -1;
static const float replacementTextRoundedRectRadius = 11;
static const float replacementArrowLeftMargin = -4;
static const float replacementArrowPadding = 4;
static const float replacementArrowCirclePadding = 3;

static const Color& replacementTextRoundedRectPressedColor()
{
    static const Color pressed(105, 105, 105, 242);
    return pressed;
}

static const Color& replacementTextRoundedRectColor()
{
    static const Color standard(125, 125, 125, 242);
    return standard;
}

static const Color& replacementTextColor()
{
    static const Color standard(240, 240, 240, 255);
    return standard;
}

static const Color& unavailablePluginBorderColor()
{
    static const Color standard(255, 255, 255, 216);
    return standard;
}

RenderEmbeddedObject::RenderEmbeddedObject(HTMLFrameOwnerElement& element, PassRef<RenderStyle> style)
    : RenderWidget(element, std::move(style))
    , m_isPluginUnavailable(false)
    , m_isUnavailablePluginIndicatorHidden(false)
    , m_unavailablePluginIndicatorIsPressed(false)
    , m_mouseDownWasInUnavailablePluginIndicator(false)
{
    // Actual size is not known yet, report the default intrinsic size.
    view().frameView().incrementVisuallyNonEmptyPixelCount(roundedIntSize(intrinsicSize()));
}

RenderEmbeddedObject::~RenderEmbeddedObject()
{
    view().frameView().removeEmbeddedObjectToUpdate(*this);
}

RenderPtr<RenderEmbeddedObject> RenderEmbeddedObject::createForApplet(HTMLAppletElement& applet, PassRef<RenderStyle> style)
{
    auto renderer = createRenderer<RenderEmbeddedObject>(applet, std::move(style));
    renderer->setInline(true);
    return renderer;
}

bool RenderEmbeddedObject::requiresLayer() const
{
    if (RenderWidget::requiresLayer())
        return true;
    
    return allowsAcceleratedCompositing();
}

bool RenderEmbeddedObject::allowsAcceleratedCompositing() const
{
#if PLATFORM(IOS)
    // The timing of layer creation is different on the phone, since the plugin can only be manipulated from the main thread.
    return widget() && widget()->isPluginViewBase() && toPluginViewBase(widget())->willProvidePluginLayer();
#else
    return widget() && widget()->isPluginViewBase() && toPluginViewBase(widget())->platformLayer();
#endif
}

#if !PLATFORM(IOS)
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
    }

    ASSERT_NOT_REACHED();
    return String();
}
#endif

static bool shouldUnavailablePluginMessageBeButton(Document& document, RenderEmbeddedObject::PluginUnavailabilityReason pluginUnavailabilityReason)
{
    Page* page = document.page();
    return page && page->chrome().client().shouldUnavailablePluginMessageBeButton(pluginUnavailabilityReason);
}

void RenderEmbeddedObject::setPluginUnavailabilityReason(PluginUnavailabilityReason pluginUnavailabilityReason)
{
#if PLATFORM(IOS)
    UNUSED_PARAM(pluginUnavailabilityReason);
#else
    setPluginUnavailabilityReasonWithDescription(pluginUnavailabilityReason, unavailablePluginReplacementText(pluginUnavailabilityReason));
#endif
}

void RenderEmbeddedObject::setPluginUnavailabilityReasonWithDescription(PluginUnavailabilityReason pluginUnavailabilityReason, const String& description)
{
#if PLATFORM(IOS)
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

void RenderEmbeddedObject::paintSnapshotImage(PaintInfo& paintInfo, const LayoutPoint& paintOffset, Image* image)
{
    LayoutUnit cWidth = contentWidth();
    LayoutUnit cHeight = contentHeight();
    if (!cWidth || !cHeight)
        return;

    GraphicsContext* context = paintInfo.context;
    LayoutSize contentSize(cWidth, cHeight);
    LayoutPoint contentLocation = location() + paintOffset;
    contentLocation.move(borderLeft() + paddingLeft(), borderTop() + paddingTop());

    LayoutRect rect(contentLocation, contentSize);
    IntRect alignedRect = pixelSnappedIntRect(rect);
    if (alignedRect.width() <= 0 || alignedRect.height() <= 0)
        return;

    bool useLowQualityScaling = shouldPaintAtLowQuality(context, image, image, alignedRect.size());
    ImageOrientationDescription orientationDescription(shouldRespectImageOrientation());
#if ENABLE(CSS_IMAGE_ORIENTATION)
    orientationDescription.setImageOrientationEnum(style().imageOrientation());
#endif
    context->drawImage(image, style().colorSpace(), alignedRect, CompositeSourceOver, orientationDescription, useLowQualityScaling);
}

void RenderEmbeddedObject::paintContents(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!frameOwnerElement().isPluginElement())
        return;

    HTMLPlugInElement& plugInElement = toHTMLPlugInElement(frameOwnerElement());

    if (plugInElement.displayState() > HTMLPlugInElement::DisplayingSnapshot) {
        RenderWidget::paintContents(paintInfo, paintOffset);
        if (!plugInElement.isRestartedPlugin())
            return;
    }

    if (!plugInElement.isPlugInImageElement())
        return;

    Image* snapshot = toHTMLPlugInImageElement(plugInElement).snapshotImage();
    if (snapshot)
        paintSnapshotImage(paintInfo, paintOffset, snapshot);
}

void RenderEmbeddedObject::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    Page* page = frame().page();

    if (isPluginUnavailable()) {
        if (page && paintInfo.phase == PaintPhaseForeground)
            page->addRelevantUnpaintedObject(this, visualOverflowRect());
        RenderReplaced::paint(paintInfo, paintOffset);
        return;
    }

    if (page && paintInfo.phase == PaintPhaseForeground)
        page->addRelevantRepaintedObject(this, visualOverflowRect());

    RenderWidget::paint(paintInfo, paintOffset);
}

static void drawReplacementArrow(GraphicsContext* context, const FloatRect& insideRect)
{
    GraphicsContextStateSaver stateSaver(*context);

    FloatRect rect(insideRect);
    rect.inflate(-replacementArrowPadding);

    FloatPoint center(rect.center());
    FloatPoint arrowTip(rect.maxX(), center.y());

    context->setStrokeThickness(2);
    context->setLineCap(RoundCap);
    context->setLineJoin(RoundJoin);

    Path path;
    path.moveTo(FloatPoint(rect.x(), center.y()));
    path.addLineTo(arrowTip);
    path.addLineTo(FloatPoint(center.x(), rect.y()));
    path.moveTo(arrowTip);
    path.addLineTo(FloatPoint(center.x(), rect.maxY()));
    context->strokePath(path);
}

void RenderEmbeddedObject::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!showsUnavailablePluginIndicator())
        return;

    if (paintInfo.phase == PaintPhaseSelection)
        return;

    GraphicsContext* context = paintInfo.context;
    if (context->paintingDisabled())
        return;

    FloatRect contentRect;
    FloatRect indicatorRect;
    FloatRect replacementTextRect;
    FloatRect arrowRect;
    Font font;
    TextRun run("");
    float textWidth;
    if (!getReplacementTextGeometry(paintOffset, contentRect, indicatorRect, replacementTextRect, arrowRect, font, run, textWidth))
        return;

    Path background;
    background.addRoundedRect(indicatorRect, FloatSize(replacementTextRoundedRectRadius, replacementTextRoundedRectRadius));

    GraphicsContextStateSaver stateSaver(*context);
    context->clip(contentRect);
    context->setFillColor(m_unavailablePluginIndicatorIsPressed ? replacementTextRoundedRectPressedColor() : replacementTextRoundedRectColor(), style().colorSpace());
    context->fillPath(background);

    Path strokePath;
    FloatRect strokeRect(indicatorRect);
    strokeRect.inflate(1);
    strokePath.addRoundedRect(strokeRect, FloatSize(replacementTextRoundedRectRadius + 1, replacementTextRoundedRectRadius + 1));

    context->setStrokeColor(unavailablePluginBorderColor(), style().colorSpace());
    context->setStrokeThickness(2);
    context->strokePath(strokePath);

    const FontMetrics& fontMetrics = font.fontMetrics();
    float labelX = roundf(replacementTextRect.location().x() + replacementTextRoundedRectLeftTextMargin);
    float labelY = roundf(replacementTextRect.location().y() + (replacementTextRect.size().height() - fontMetrics.height()) / 2 + fontMetrics.ascent() + replacementTextRoundedRectTopTextMargin);
    context->setFillColor(replacementTextColor(), style().colorSpace());
    context->drawBidiText(font, run, FloatPoint(labelX, labelY));

    if (shouldUnavailablePluginMessageBeButton(document(), m_pluginUnavailabilityReason)) {
        arrowRect.inflate(-replacementArrowCirclePadding);

        context->beginTransparencyLayer(1.0);
        context->setFillColor(replacementTextColor(), style().colorSpace());
        context->fillEllipse(arrowRect);

        context->setCompositeOperation(CompositeClear);
        drawReplacementArrow(context, arrowRect);
        context->endTransparencyLayer();
    }
}

void RenderEmbeddedObject::setUnavailablePluginIndicatorIsHidden(bool hidden)
{
    m_isUnavailablePluginIndicatorHidden = hidden;

    repaint();
}

bool RenderEmbeddedObject::getReplacementTextGeometry(const LayoutPoint& accumulatedOffset, FloatRect& contentRect, FloatRect& indicatorRect, FloatRect& replacementTextRect, FloatRect& arrowRect, Font& font, TextRun& run, float& textWidth) const
{
    bool includesArrow = shouldUnavailablePluginMessageBeButton(document(), m_pluginUnavailabilityReason);

    contentRect = contentBoxRect();
    contentRect.moveBy(roundedIntPoint(accumulatedOffset));

    FontDescription fontDescription;
    RenderTheme::defaultTheme()->systemFont(CSSValueWebkitSmallControl, fontDescription);
    fontDescription.setWeight(FontWeightBold);
    fontDescription.setRenderingMode(frame().settings().fontRenderingMode());
    fontDescription.setComputedSize(12);
    font = Font(fontDescription, 0, 0);
    font.update(0);

    run = TextRun(m_unavailablePluginReplacementText);
    textWidth = font.width(run);

    replacementTextRect.setSize(FloatSize(textWidth + replacementTextRoundedRectLeftTextMargin + (includesArrow ? replacementTextRoundedRectRightTextMarginWithArrow : replacementTextRoundedRectRightTextMargin), replacementTextRoundedRectHeight));
    float x = (contentRect.size().width() / 2 - replacementTextRect.size().width() / 2) + contentRect.location().x();
    float y = (contentRect.size().height() / 2 - replacementTextRect.size().height() / 2) + contentRect.location().y();
    replacementTextRect.setLocation(FloatPoint(x, y));

    indicatorRect = replacementTextRect;

    // Expand the background rect to include the arrow, if it will be used.
    if (includesArrow) {
        arrowRect = indicatorRect;
        arrowRect.setX(ceilf(arrowRect.maxX() + replacementArrowLeftMargin));
        arrowRect.setWidth(arrowRect.height());
        indicatorRect.unite(arrowRect);
    }

    return true;
}

LayoutRect RenderEmbeddedObject::unavailablePluginIndicatorBounds(const LayoutPoint& accumulatedOffset) const
{
    FloatRect contentRect;
    FloatRect indicatorRect;
    FloatRect replacementTextRect;
    FloatRect arrowRect;
    Font font;
    TextRun run("", 0);
    float textWidth;
    if (getReplacementTextGeometry(accumulatedOffset, contentRect, indicatorRect, replacementTextRect, arrowRect, font, run, textWidth))
        return LayoutRect(indicatorRect);

    return LayoutRect();
}

bool RenderEmbeddedObject::isReplacementObscured() const
{
    // Return whether or not the replacement content for blocked plugins is accessible to the user.

    // Check the opacity of each layer containing the element or its ancestors.
    float opacity = 1.0;
    for (RenderLayer* layer = enclosingLayer(); layer; layer = layer->parent()) {
        opacity *= layer->renderer().style().opacity();
        if (opacity < 0.1)
            return true;
    }

    // Calculate the absolute rect for the blocked plugin replacement text.
    IntRect absoluteBoundingBox = absoluteBoundingBoxRect();
    LayoutPoint absoluteLocation(absoluteBoundingBox.location());
    LayoutRect rect = unavailablePluginIndicatorBounds(absoluteLocation);
    if (rect.isEmpty())
        return true;

    RenderView* rootRenderView = document().topDocument().renderView();
    ASSERT(rootRenderView);
    if (!rootRenderView)
        return true;

    IntRect rootViewRect = view().frameView().convertToRootView(pixelSnappedIntRect(rect));
    
    HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::IgnoreClipping | HitTestRequest::DisallowShadowContent | HitTestRequest::AllowChildFrameContent);
    HitTestResult result;
    HitTestLocation location;
    
    LayoutUnit x = rootViewRect.x();
    LayoutUnit y = rootViewRect.y();
    LayoutUnit width = rootViewRect.width();
    LayoutUnit height = rootViewRect.height();
    
    // Hit test the center and near the corners of the replacement text to ensure
    // it is visible and is not masked by other elements.
    bool hit = false;
    location = LayoutPoint(x + width / 2, y + height / 2);
    hit = rootRenderView->hitTest(request, location, result);
    if (!hit || result.innerNode() != &frameOwnerElement())
        return true;
    
    location = LayoutPoint(x, y);
    hit = rootRenderView->hitTest(request, location, result);
    if (!hit || result.innerNode() != &frameOwnerElement())
        return true;
    
    location = LayoutPoint(x + width, y);
    hit = rootRenderView->hitTest(request, location, result);
    if (!hit || result.innerNode() != &frameOwnerElement())
        return true;
    
    location = LayoutPoint(x + width, y + height);
    hit = rootRenderView->hitTest(request, location, result);
    if (!hit || result.innerNode() != &frameOwnerElement())
        return true;
    
    location = LayoutPoint(x, y + height);
    hit = rootRenderView->hitTest(request, location, result);
    if (!hit || result.innerNode() != &frameOwnerElement())
        return true;

    return false;
}

void RenderEmbeddedObject::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    LayoutSize oldSize = contentBoxRect().size();

    updateLogicalWidth();
    updateLogicalHeight();

    RenderWidget::layout();

    clearOverflow();
    addVisualEffectOverflow();

    updateLayerTransform();

    bool wasMissingWidget = false;
    if (!widget() && canHaveWidget()) {
        wasMissingWidget = true;
        view().frameView().addEmbeddedObjectToUpdate(*this);
    }

    clearNeedsLayout();

    LayoutSize newSize = contentBoxRect().size();

    if (!wasMissingWidget && newSize.width() >= oldSize.width() && newSize.height() >= oldSize.height()) {
        HTMLFrameOwnerElement& element = frameOwnerElement();
        if (element.isPluginElement() && toHTMLPlugInElement(element).isPlugInImageElement()) {
            HTMLPlugInImageElement& plugInImageElement = toHTMLPlugInImageElement(element);
            if (plugInImageElement.displayState() > HTMLPlugInElement::DisplayingSnapshot && plugInImageElement.snapshotDecision() == HTMLPlugInImageElement::MaySnapshotWhenResized) {
                plugInImageElement.setNeedsCheckForSizeChange();
                view().frameView().addEmbeddedObjectToUpdate(*this);
            }
        }
    }

    if (!canHaveChildren())
        return;

    // This code copied from RenderMedia::layout().
    RenderObject* child = firstChild();

    if (!child)
        return;

    RenderBox* childBox = toRenderBox(child);

    if (!childBox)
        return;

    if (newSize == oldSize && !childBox->needsLayout())
        return;
    
    // When calling layout() on a child node, a parent must either push a LayoutStateMaintainter, or
    // instantiate LayoutStateDisabler. Since using a LayoutStateMaintainer is slightly more efficient,
    // and this method will be called many times per second during playback, use a LayoutStateMaintainer:
    LayoutStateMaintainer statePusher(view(), *this, locationOffset(), hasTransform() || hasReflection() || style().isFlippedBlocksWritingMode());
    
    childBox->setLocation(LayoutPoint(borderLeft(), borderTop()) + LayoutSize(paddingLeft(), paddingTop()));
    childBox->style().setHeight(Length(newSize.height(), Fixed));
    childBox->style().setWidth(Length(newSize.width(), Fixed));
    childBox->setNeedsLayout(MarkOnlyThis);
    childBox->layout();
    clearChildNeedsLayout();
    
    statePusher.pop();
}

bool RenderEmbeddedObject::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (!RenderWidget::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, hitTestAction))
        return false;

    if (!widget() || !widget()->isPluginViewBase())
        return true;

    PluginViewBase* view = toPluginViewBase(widget());
    IntPoint roundedPoint = locationInContainer.roundedPoint();

    if (Scrollbar* horizontalScrollbar = view->horizontalScrollbar()) {
        if (horizontalScrollbar->shouldParticipateInHitTesting() && horizontalScrollbar->frameRect().contains(roundedPoint)) {
            result.setScrollbar(horizontalScrollbar);
            return true;
        }
    }

    if (Scrollbar* verticalScrollbar = view->verticalScrollbar()) {
        if (verticalScrollbar->shouldParticipateInHitTesting() && verticalScrollbar->frameRect().contains(roundedPoint)) {
            result.setScrollbar(verticalScrollbar);
            return true;
        }
    }

    return true;
}

bool RenderEmbeddedObject::scroll(ScrollDirection direction, ScrollGranularity granularity, float, Element**, RenderBox*, const IntPoint&)
{
    if (!widget() || !widget()->isPluginViewBase())
        return false;

    return toPluginViewBase(widget())->scroll(direction, granularity);
}

bool RenderEmbeddedObject::logicalScroll(ScrollLogicalDirection direction, ScrollGranularity granularity, float multiplier, Element** stopElement)
{
    // Plugins don't expose a writing direction, so assuming horizontal LTR.
    return scroll(logicalToPhysical(direction, true, false), granularity, multiplier, stopElement);
}


bool RenderEmbeddedObject::isInUnavailablePluginIndicator(const LayoutPoint& point) const
{
    FloatRect contentRect;
    FloatRect indicatorRect;
    FloatRect replacementTextRect;
    FloatRect arrowRect;
    Font font;
    TextRun run("");
    float textWidth;
    return getReplacementTextGeometry(IntPoint(), contentRect, indicatorRect, replacementTextRect, arrowRect, font, run, textWidth)
        && indicatorRect.contains(point);
}

bool RenderEmbeddedObject::isInUnavailablePluginIndicator(MouseEvent* event) const
{
    return isInUnavailablePluginIndicator(roundedLayoutPoint(absoluteToLocal(event->absoluteLocation(), UseTransforms)));
}

void RenderEmbeddedObject::handleUnavailablePluginIndicatorEvent(Event* event)
{
    if (!shouldUnavailablePluginMessageBeButton(document(), m_pluginUnavailabilityReason))
        return;

    if (!event->isMouseEvent())
        return;

    MouseEvent* mouseEvent = toMouseEvent(event);
    HTMLPlugInElement& element = toHTMLPlugInElement(frameOwnerElement());
    if (event->type() == eventNames().mousedownEvent && toMouseEvent(event)->button() == LeftButton) {
        m_mouseDownWasInUnavailablePluginIndicator = isInUnavailablePluginIndicator(mouseEvent);
        if (m_mouseDownWasInUnavailablePluginIndicator) {
            frame().eventHandler().setCapturingMouseEventsElement(&element);
            element.setIsCapturingMouseEvents(true);
            setUnavailablePluginIndicatorIsPressed(true);
        }
        event->setDefaultHandled();
    }
    if (event->type() == eventNames().mouseupEvent && toMouseEvent(event)->button() == LeftButton) {
        if (m_unavailablePluginIndicatorIsPressed) {
            frame().eventHandler().setCapturingMouseEventsElement(nullptr);
            element.setIsCapturingMouseEvents(false);
            setUnavailablePluginIndicatorIsPressed(false);
        }
        if (m_mouseDownWasInUnavailablePluginIndicator && isInUnavailablePluginIndicator(mouseEvent)) {
            if (Page* page = document().page())
                page->chrome().client().unavailablePluginButtonClicked(&element, m_pluginUnavailabilityReason);
        }
        m_mouseDownWasInUnavailablePluginIndicator = false;
        event->setDefaultHandled();
    }
    if (event->type() == eventNames().mousemoveEvent) {
        setUnavailablePluginIndicatorIsPressed(m_mouseDownWasInUnavailablePluginIndicator && isInUnavailablePluginIndicator(mouseEvent));
        event->setDefaultHandled();
    }
}

CursorDirective RenderEmbeddedObject::getCursor(const LayoutPoint& point, Cursor& cursor) const
{
    if (showsUnavailablePluginIndicator() && shouldUnavailablePluginMessageBeButton(document(), m_pluginUnavailabilityReason) && isInUnavailablePluginIndicator(point)) {
        cursor = handCursor();
        return SetCursor;
    }
    if (widget() && widget()->isPluginViewBase()) {
        // A plug-in is responsible for setting the cursor when the pointer is over it.
        return DoNotSetCursor;
    }
    return RenderWidget::getCursor(point, cursor);
}

bool RenderEmbeddedObject::canHaveChildren() const
{
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    if (frameOwnerElement().isMediaElement())
        return true;
#endif

    if (isSnapshottedPlugIn())
        return true;

    return false;
}

}
