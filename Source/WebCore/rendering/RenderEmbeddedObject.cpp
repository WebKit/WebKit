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
#include "DOMTokenList.h"
#include "EventHandler.h"
#include "Font.h"
#include "FontSelector.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "GraphicsContext.h"
#include "HTMLDivElement.h"
#include "HTMLEmbedElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLParamElement.h"
#include "HTMLPlugInElement.h"
#include "HTMLSpanElement.h"
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
#include "RenderWidgetProtector.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "Text.h"
#include "TextRun.h"
#include <wtf/StackStats.h>

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
#include "HTMLMediaElement.h"
#endif

namespace WebCore {

using namespace HTMLNames;

RenderEmbeddedObject::RenderEmbeddedObject(Element* element)
    : RenderPart(element)
    , m_hasFallbackContent(false)
    , m_showsUnavailablePluginIndicator(false)
{
    // Actual size is not known yet, report the default intrinsic size.
    view()->frameView()->incrementVisuallyNonEmptyPixelCount(roundedIntSize(intrinsicSize()));
}

RenderEmbeddedObject::~RenderEmbeddedObject()
{
    if (frameView())
        frameView()->removeWidgetToUpdate(this);
}

#if USE(ACCELERATED_COMPOSITING)
bool RenderEmbeddedObject::requiresLayer() const
{
    if (RenderPart::requiresLayer())
        return true;
    
    return allowsAcceleratedCompositing();
}

bool RenderEmbeddedObject::allowsAcceleratedCompositing() const
{
    return widget() && widget()->isPluginViewBase() && toPluginViewBase(widget())->platformLayer();
}
#endif

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

static bool shouldUnavailablePluginMessageIncludeButton(Document* document, RenderEmbeddedObject::PluginUnavailabilityReason pluginUnavailabilityReason)
{
    Page* page = document->page();
    return page && page->chrome().client()->shouldUnavailablePluginMessageIncludeButton(pluginUnavailabilityReason);
}

void RenderEmbeddedObject::setPluginUnavailabilityReasonDescription(const String& description)
{
    m_unavailabilityDescription = description;

    if (m_showsUnavailablePluginIndicator) {
        String unavailabilityReasonText = description;
        if (unavailabilityReasonText.isEmpty())
            unavailabilityReasonText = unavailablePluginReplacementText(m_pluginUnavailabilityReason);

        m_indicatorLabel->setTextContent(unavailabilityReasonText, ASSERT_NO_EXCEPTION);
    }
}

void RenderEmbeddedObject::setPluginUnavailabilityReason(PluginUnavailabilityReason pluginUnavailabilityReason)
{
    ASSERT(!m_showsUnavailablePluginIndicator);
    m_showsUnavailablePluginIndicator = true;
    m_pluginUnavailabilityReason = pluginUnavailabilityReason;

    Element* element = toElement(node());
    ShadowRoot* shadowRoot = element->userAgentShadowRoot();
    if (!shadowRoot) {
        // This can cause the RenderEmbeddedObject to be destroyed via a reattach.
        // Callers should be wary that calling this can destroy the object they call it on.
        element->ensureUserAgentShadowRoot();
        return;
    }

    DEFINE_STATIC_LOCAL(AtomicString, unavailablePluginContentPseudoAttribute, ("-webkit-unavailable-plugin-content", AtomicString::ConstructFromLiteral));
    RefPtr<HTMLDivElement> container = HTMLDivElement::create(document());
    container->setPseudo(unavailablePluginContentPseudoAttribute);

    RefPtr<HTMLDivElement> indicator = HTMLDivElement::create(document());
    bool indicatorShouldIncludeButton = shouldUnavailablePluginMessageIncludeButton(document(), m_pluginUnavailabilityReason);
    if (indicatorShouldIncludeButton)
        indicator->addEventListener(eventNames().clickEvent, RenderEmbeddedObjectEventListener::create(this), false);

    m_indicatorLabel = HTMLSpanElement::create(HTMLNames::spanTag, document());
    m_indicatorLabel->setAttribute(roleAttr, "button");
    indicator->appendChild(m_indicatorLabel.get());

    setPluginUnavailabilityReasonDescription(m_unavailabilityDescription);

    if (indicatorShouldIncludeButton) {
        RefPtr<HTMLImageElement> arrow = HTMLImageElement::create(document());
        indicator->appendChild(arrow.release());
    }

    container->appendChild(indicator);
    shadowRoot->appendChild(container);

    m_indicator = container;
}

void RenderEmbeddedObject::handleUnavailablePluginButtonClickEvent(Event*)
{
    if (Page* page = document()->page())
        page->chrome().client()->unavailablePluginButtonClicked(toElement(node()), m_pluginUnavailabilityReason);
}

bool RenderEmbeddedObject::showsUnavailablePluginIndicator() const
{
    return m_showsUnavailablePluginIndicator;
}

void RenderEmbeddedObject::willBeDestroyed()
{
    Element* element = toElement(node());
    if (ShadowRoot* shadowRoot = element->userAgentShadowRoot())
        shadowRoot->removeChildren();

    RenderPart::willBeDestroyed();
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
    context->drawImage(image, style()->colorSpace(), alignedRect, CompositeSourceOver, shouldRespectImageOrientation(), useLowQualityScaling);
}

void RenderEmbeddedObject::paintContents(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    Element* element = toElement(node());
    if (!element || !element->isPluginElement())
        return;

    HTMLPlugInElement* plugInElement = toHTMLPlugInElement(element);

    if (plugInElement->displayState() > HTMLPlugInElement::DisplayingSnapshot) {
        RenderPart::paintContents(paintInfo, paintOffset);
        if (!plugInElement->isRestartedPlugin())
            return;
    }

    if (!plugInElement->isPlugInImageElement())
        return;

    Image* snapshot = toHTMLPlugInImageElement(plugInElement)->snapshotImage();
    if (snapshot)
        paintSnapshotImage(paintInfo, paintOffset, snapshot);
}

void RenderEmbeddedObject::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (Frame* frame = this->frame()) {
        if (Page* page = frame->page()) {
            if (paintInfo.phase == PaintPhaseForeground)
                page->addRelevantRepaintedObject(this, visualOverflowRect());
        }
    }

    // RenderEmbeddedObject doesn't normally paint its children, but if we have a shadow root we need to paint that.
    Element* element = toElement(node());
    if (ShadowRoot* shadowRoot = element->userAgentShadowRoot()) {
        RenderObject* shadowRootRenderer = shadowRoot->renderer();
        if (shadowRootRenderer && shadowRootRenderer->isBox()) {
            RenderBox* shadowRootBox = toRenderBox(shadowRootRenderer);
            PaintPhase newPhase = (paintInfo.phase == PaintPhaseChildOutlines) ? PaintPhaseOutline : paintInfo.phase;
            newPhase = (newPhase == PaintPhaseChildBlockBackgrounds) ? PaintPhaseChildBlockBackground : newPhase;

            PaintInfo paintInfoForChild(paintInfo);
            paintInfoForChild.phase = newPhase;
            paintInfoForChild.updateSubtreePaintRootForChildren(this);

            for (RenderBox* child = shadowRootBox->firstChildBox(); child; child = child->nextSiblingBox()) {
                LayoutPoint childPoint = flipForWritingModeForChild(child, paintOffset);
                if (!child->hasSelfPaintingLayer() && !child->isFloating())
                    child->paint(paintInfoForChild, childPoint);
            }
        }
    }

    RenderPart::paint(paintInfo, paintOffset);
}

bool RenderEmbeddedObject::isReplacementObscured() const
{
    // Return whether or not the replacement content for blocked plugins is accessible to the user.
    
    // Check the opacity of each layer containing the element or its ancestors.
    float opacity = 1.0;
    for (RenderLayer* layer = enclosingLayer(); layer; layer = layer->parent()) {
        RenderLayerModelObject* renderer = layer->renderer();
        RenderStyle* style = renderer->style();
        opacity *= style->opacity();
        if (opacity < 0.1)
            return true;
    }

    // Calculate the absolute rect for the blocked plugin replacement text.
    IntRect absoluteBoundingBox = absoluteBoundingBoxRect();
    LayoutPoint absoluteLocation(absoluteBoundingBox.location());
    LayoutRect rect;
    if (m_indicator && m_indicator->renderer())
        rect = m_indicator->renderer()->absoluteBoundingBoxRect();

    if (rect.isEmpty())
        return true;

    RenderView* docRenderer = document()->renderView();
    ASSERT(docRenderer);
    if (!docRenderer)
        return true;
    
    HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::IgnoreClipping | HitTestRequest::DisallowShadowContent);
    HitTestResult result;
    HitTestLocation location;
    
    LayoutUnit x = rect.x();
    LayoutUnit y = rect.y();
    LayoutUnit width = rect.width();
    LayoutUnit height = rect.height();
    
    // Hit test the center and near the corners of the replacement text to ensure
    // it is visible and is not masked by other elements.
    bool hit = false;
    location = LayoutPoint(x + width / 2, y + height / 2);
    hit = docRenderer->hitTest(request, location, result);
    if (!hit || result.innerNode() != node())
        return true;
    
    location = LayoutPoint(x, y);
    hit = docRenderer->hitTest(request, location, result);
    if (!hit || result.innerNode() != node())
        return true;
    
    location = LayoutPoint(x + width, y);
    hit = docRenderer->hitTest(request, location, result);
    if (!hit || result.innerNode() != node())
        return true;
    
    location = LayoutPoint(x + width, y + height);
    hit = docRenderer->hitTest(request, location, result);
    if (!hit || result.innerNode() != node())
        return true;
    
    location = LayoutPoint(x, y + height);
    hit = docRenderer->hitTest(request, location, result);
    if (!hit || result.innerNode() != node())
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

    RenderPart::layout();

    m_overflow.clear();
    addVisualEffectOverflow();

    updateLayerTransform();

    bool wasMissingWidget = false;
    if (!widget() && frameView() && canHaveWidget()) {
        wasMissingWidget = true;
        frameView()->addWidgetToUpdate(this);
    }

    setNeedsLayout(false);

    LayoutSize newSize = contentBoxRect().size();

    if (!wasMissingWidget && newSize.width() >= oldSize.width() && newSize.height() >= oldSize.height()) {
        Element* element = toElement(node());
        if (element && element->isPluginElement() && toHTMLPlugInElement(element)->isPlugInImageElement()) {
            HTMLPlugInImageElement* plugInImageElement = toHTMLPlugInImageElement(element);
            if (plugInImageElement->displayState() > HTMLPlugInElement::DisplayingSnapshot && plugInImageElement->snapshotDecision() == HTMLPlugInImageElement::MaySnapshotWhenResized && document()->view()) {
                plugInImageElement->setNeedsCheckForSizeChange();
                document()->view()->addWidgetToUpdate(this);
            }
        }
    }

    if (!canHaveChildren())
        return;

    Element* element = toElement(node());
    ShadowRoot* shadowRoot = element->userAgentShadowRoot();
    if (!shadowRoot)
        return;

    Node* shadowRootChild = shadowRoot->firstChild();
    if (!shadowRootChild)
        return;

    RenderObject* shadowRootRenderer = shadowRootChild->renderer();
    if (!shadowRootRenderer || !shadowRootRenderer->isBox())
        return;

    RenderBox* childBox = toRenderBox(shadowRootRenderer);

    if (newSize == oldSize && !childBox->needsLayout())
        return;
    
    // When calling layout() on a child node, a parent must either push a LayoutStateMaintainter, or
    // instantiate LayoutStateDisabler. Since using a LayoutStateMaintainer is slightly more efficient,
    // and this method will be called many times per second during playback, use a LayoutStateMaintainer:
    LayoutStateMaintainer statePusher(view(), this, locationOffset(), hasTransform() || hasReflection() || style()->isFlippedBlocksWritingMode());
    
    childBox->setLocation(LayoutPoint(borderLeft(), borderTop()) + LayoutSize(paddingLeft(), paddingTop()));
    childBox->style()->setHeight(Length(newSize.height(), Fixed));
    childBox->style()->setWidth(Length(newSize.width(), Fixed));
    childBox->setNeedsLayout(true, MarkOnlyThis);
    childBox->layout();
    setChildNeedsLayout(false);
    
    statePusher.pop();
}

void RenderEmbeddedObject::viewCleared()
{
    // This is required for <object> elements whose contents are rendered by WebCore (e.g. src="foo.html").
    if (node() && widget() && widget()->isFrameView()) {
        FrameView* view = toFrameView(widget());
        int marginWidth = -1;
        int marginHeight = -1;
        if (node()->hasTagName(iframeTag)) {
            HTMLIFrameElement* frame = toHTMLIFrameElement(node());
            marginWidth = frame->marginWidth();
            marginHeight = frame->marginHeight();
        }
        if (marginWidth != -1)
            view->setMarginWidth(marginWidth);
        if (marginHeight != -1)
            view->setMarginHeight(marginHeight);
    }
}

bool RenderEmbeddedObject::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (!RenderPart::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, hitTestAction))
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

bool RenderEmbeddedObject::scroll(ScrollDirection direction, ScrollGranularity granularity, float, Node**)
{
    if (!widget() || !widget()->isPluginViewBase())
        return false;

    return toPluginViewBase(widget())->scroll(direction, granularity);
}

bool RenderEmbeddedObject::logicalScroll(ScrollLogicalDirection direction, ScrollGranularity granularity, float multiplier, Node** stopNode)
{
    // Plugins don't expose a writing direction, so assuming horizontal LTR.
    return scroll(logicalToPhysical(direction, true, false), granularity, multiplier, stopNode);
}

bool RenderEmbeddedObject::canHaveChildren() const
{
    if (isSnapshottedPlugIn())
        return true;

    if (!node())
        return false;

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    if (toElement(node())->isMediaElement())
        return true;
#endif

    return toElement(node())->userAgentShadowRoot();
}

void RenderEmbeddedObjectEventListener::handleEvent(ScriptExecutionContext*, Event* event)
{
    if (event->type() == eventNames().clickEvent)
        m_renderEmbeddedObject->handleUnavailablePluginButtonClickEvent(event);
}

bool RenderEmbeddedObjectEventListener::operator==(const EventListener& listener)
{
    if (const RenderEmbeddedObjectEventListener* renderEmbeddedObjectEventListener = RenderEmbeddedObjectEventListener::cast(&listener))
        return m_renderEmbeddedObject == renderEmbeddedObjectEventListener->m_renderEmbeddedObject;
    return false;
}

}
