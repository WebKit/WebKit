/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorOverlay.h"

#include "DocumentLoader.h"
#include "Element.h"
#include "EmptyClients.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "InspectorClient.h"
#include "InspectorOverlayPage.h"
#include "MainFrame.h"
#include "Node.h"
#include "Page.h"
#include "PageConfiguration.h"
#include "PolygonShape.h"
#include "PseudoElement.h"
#include "RectangleShape.h"
#include "RenderBoxModelObject.h"
#include "RenderElement.h"
#include "RenderFlowThread.h"
#include "RenderInline.h"
#include "RenderNamedFlowFragment.h"
#include "RenderNamedFlowThread.h"
#include "RenderRegion.h"
#include "RenderView.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "Settings.h"
#include "StyledElement.h"
#include <bindings/ScriptValue.h>
#include <inspector/InspectorProtocolObjects.h>
#include <inspector/InspectorValues.h>
#include <wtf/text/StringBuilder.h>

using namespace Inspector;

namespace WebCore {

static void contentsQuadToCoordinateSystem(const FrameView* mainView, const FrameView* view, FloatQuad& quad, InspectorOverlay::CoordinateSystem coordinateSystem)
{
    quad.setP1(view->contentsToRootView(roundedIntPoint(quad.p1())));
    quad.setP2(view->contentsToRootView(roundedIntPoint(quad.p2())));
    quad.setP3(view->contentsToRootView(roundedIntPoint(quad.p3())));
    quad.setP4(view->contentsToRootView(roundedIntPoint(quad.p4())));

    if (coordinateSystem == InspectorOverlay::CoordinateSystem::View)
        quad += toIntSize(mainView->scrollPosition());
}

static void contentsQuadToPage(const FrameView* mainView, const FrameView* view, FloatQuad& quad)
{
    contentsQuadToCoordinateSystem(mainView, view, quad, InspectorOverlay::CoordinateSystem::View);
}

static void buildRendererHighlight(RenderObject* renderer, RenderRegion* region, const HighlightConfig& highlightConfig, Highlight& highlight, InspectorOverlay::CoordinateSystem coordinateSystem)
{
    Frame* containingFrame = renderer->document().frame();
    if (!containingFrame)
        return;

    highlight.setDataFromConfig(highlightConfig);
    FrameView* containingView = containingFrame->view();
    FrameView* mainView = containingFrame->page()->mainFrame().view();

    // RenderSVGRoot should be highlighted through the isBox() code path, all other SVG elements should just dump their absoluteQuads().
    bool isSVGRenderer = renderer->node() && renderer->node()->isSVGElement() && !renderer->isSVGRoot();

    if (isSVGRenderer) {
        highlight.type = HighlightType::Rects;
        renderer->absoluteQuads(highlight.quads);
        for (auto& quad : highlight.quads)
            contentsQuadToCoordinateSystem(mainView, containingView, quad, coordinateSystem);
    } else if (is<RenderBox>(*renderer) || is<RenderInline>(*renderer)) {
        LayoutRect contentBox;
        LayoutRect paddingBox;
        LayoutRect borderBox;
        LayoutRect marginBox;

        if (is<RenderBox>(*renderer)) {
            auto& renderBox = downcast<RenderBox>(*renderer);

            LayoutBoxExtent margins(renderBox.marginTop(), renderBox.marginRight(), renderBox.marginBottom(), renderBox.marginLeft());

            if (!renderBox.isOutOfFlowPositioned() && region) {
                RenderBox::LogicalExtentComputedValues computedValues;
                renderBox.computeLogicalWidthInRegion(computedValues, region);
                margins.start(renderBox.style().writingMode()) = computedValues.m_margins.m_start;
                margins.end(renderBox.style().writingMode()) = computedValues.m_margins.m_end;
            }

            paddingBox = renderBox.clientBoxRectInRegion(region);
            contentBox = LayoutRect(paddingBox.x() + renderBox.paddingLeft(), paddingBox.y() + renderBox.paddingTop(),
                paddingBox.width() - renderBox.paddingLeft() - renderBox.paddingRight(), paddingBox.height() - renderBox.paddingTop() - renderBox.paddingBottom());
            borderBox = LayoutRect(paddingBox.x() - renderBox.borderLeft(), paddingBox.y() - renderBox.borderTop(),
                paddingBox.width() + renderBox.borderLeft() + renderBox.borderRight(), paddingBox.height() + renderBox.borderTop() + renderBox.borderBottom());
            marginBox = LayoutRect(borderBox.x() - margins.left(), borderBox.y() - margins.top(),
                borderBox.width() + margins.left() + margins.right(), borderBox.height() + margins.top() + margins.bottom());
        } else {
            auto& renderInline = downcast<RenderInline>(*renderer);

            // RenderInline's bounding box includes paddings and borders, excludes margins.
            borderBox = renderInline.linesBoundingBox();
            paddingBox = LayoutRect(borderBox.x() + renderInline.borderLeft(), borderBox.y() + renderInline.borderTop(),
                borderBox.width() - renderInline.borderLeft() - renderInline.borderRight(), borderBox.height() - renderInline.borderTop() - renderInline.borderBottom());
            contentBox = LayoutRect(paddingBox.x() + renderInline.paddingLeft(), paddingBox.y() + renderInline.paddingTop(),
                paddingBox.width() - renderInline.paddingLeft() - renderInline.paddingRight(), paddingBox.height() - renderInline.paddingTop() - renderInline.paddingBottom());
            // Ignore marginTop and marginBottom for inlines.
            marginBox = LayoutRect(borderBox.x() - renderInline.marginLeft(), borderBox.y(),
                borderBox.width() + renderInline.horizontalMarginExtent(), borderBox.height());
        }

        FloatQuad absContentQuad;
        FloatQuad absPaddingQuad;
        FloatQuad absBorderQuad;
        FloatQuad absMarginQuad;

        if (region) {
            RenderFlowThread* flowThread = region->flowThread();

            // Figure out the quads in the space of the RenderFlowThread.
            absContentQuad = renderer->localToContainerQuad(FloatRect(contentBox), flowThread);
            absPaddingQuad = renderer->localToContainerQuad(FloatRect(paddingBox), flowThread);
            absBorderQuad = renderer->localToContainerQuad(FloatRect(borderBox), flowThread);
            absMarginQuad = renderer->localToContainerQuad(FloatRect(marginBox), flowThread);

            // Move the quad relative to the space of the current region.
            LayoutRect flippedRegionRect(region->flowThreadPortionRect());
            flowThread->flipForWritingMode(flippedRegionRect);

            FloatSize delta = region->contentBoxRect().location() - flippedRegionRect.location();
            absContentQuad.move(delta);
            absPaddingQuad.move(delta);
            absBorderQuad.move(delta);
            absMarginQuad.move(delta);

            // Resolve the absolute quads starting from the current region.
            absContentQuad = region->localToAbsoluteQuad(absContentQuad);
            absPaddingQuad = region->localToAbsoluteQuad(absPaddingQuad);
            absBorderQuad = region->localToAbsoluteQuad(absBorderQuad);
            absMarginQuad = region->localToAbsoluteQuad(absMarginQuad);
        } else {
            absContentQuad = renderer->localToAbsoluteQuad(FloatRect(contentBox));
            absPaddingQuad = renderer->localToAbsoluteQuad(FloatRect(paddingBox));
            absBorderQuad = renderer->localToAbsoluteQuad(FloatRect(borderBox));
            absMarginQuad = renderer->localToAbsoluteQuad(FloatRect(marginBox));
        }

        contentsQuadToCoordinateSystem(mainView, containingView, absContentQuad, coordinateSystem);
        contentsQuadToCoordinateSystem(mainView, containingView, absPaddingQuad, coordinateSystem);
        contentsQuadToCoordinateSystem(mainView, containingView, absBorderQuad, coordinateSystem);
        contentsQuadToCoordinateSystem(mainView, containingView, absMarginQuad, coordinateSystem);

        highlight.type = HighlightType::Node;
        highlight.quads.append(absMarginQuad);
        highlight.quads.append(absBorderQuad);
        highlight.quads.append(absPaddingQuad);
        highlight.quads.append(absContentQuad);
    }
}

static void buildNodeHighlight(Node& node, RenderRegion* region, const HighlightConfig& highlightConfig, Highlight& highlight, InspectorOverlay::CoordinateSystem coordinateSystem)
{
    RenderObject* renderer = node.renderer();
    if (!renderer)
        return;

    buildRendererHighlight(renderer, region, highlightConfig, highlight, coordinateSystem);
}

static void buildQuadHighlight(const FloatQuad& quad, const HighlightConfig& highlightConfig, Highlight& highlight)
{
    highlight.setDataFromConfig(highlightConfig);
    highlight.type = HighlightType::Rects;
    highlight.quads.append(quad);
}

InspectorOverlay::InspectorOverlay(Page& page, InspectorClient* client)
    : m_page(page)
    , m_client(client)
    , m_paintRectUpdateTimer(*this, &InspectorOverlay::updatePaintRectsTimerFired)
{
}

InspectorOverlay::~InspectorOverlay()
{
}

void InspectorOverlay::paint(GraphicsContext& context)
{
    if (!shouldShowOverlay())
        return;

    GraphicsContextStateSaver stateSaver(context);
    FrameView* view = overlayPage()->mainFrame().view();
    view->updateLayoutAndStyleIfNeededRecursive();
    view->paint(context, IntRect(0, 0, view->width(), view->height()));
}

void InspectorOverlay::getHighlight(Highlight& highlight, InspectorOverlay::CoordinateSystem coordinateSystem) const
{
    if (!m_highlightNode && !m_highlightQuad && !m_highlightNodeList)
        return;

    highlight.type = HighlightType::Rects;
    if (m_highlightNode)
        buildNodeHighlight(*m_highlightNode, nullptr, m_nodeHighlightConfig, highlight, coordinateSystem);
    else if (m_highlightNodeList) {
        highlight.setDataFromConfig(m_nodeHighlightConfig);
        for (unsigned i = 0; i < m_highlightNodeList->length(); ++i) {
            Highlight nodeHighlight;
            buildNodeHighlight(*(m_highlightNodeList->item(i)), nullptr, m_nodeHighlightConfig, nodeHighlight, coordinateSystem);
            if (nodeHighlight.type == HighlightType::Node)
                highlight.quads.appendVector(nodeHighlight.quads);
        }
        highlight.type = HighlightType::NodeList;
    } else
        buildQuadHighlight(*m_highlightQuad, m_quadHighlightConfig, highlight);
}

void InspectorOverlay::setPausedInDebuggerMessage(const String* message)
{
    m_pausedInDebuggerMessage = message ? *message : String();
    update();
}

void InspectorOverlay::hideHighlight()
{
    m_highlightNode = nullptr;
    m_highlightNodeList = nullptr;
    m_highlightQuad = nullptr;
    update();
}

void InspectorOverlay::highlightNodeList(RefPtr<NodeList>&& nodes, const HighlightConfig& highlightConfig)
{
    m_nodeHighlightConfig = highlightConfig;
    m_highlightNodeList = WTFMove(nodes);
    m_highlightNode = nullptr;
    update();
}

void InspectorOverlay::highlightNode(Node* node, const HighlightConfig& highlightConfig)
{
    m_nodeHighlightConfig = highlightConfig;
    m_highlightNode = node;
    m_highlightNodeList = nullptr;
    update();
}

void InspectorOverlay::highlightQuad(std::unique_ptr<FloatQuad> quad, const HighlightConfig& highlightConfig)
{
    if (highlightConfig.usePageCoordinates)
        *quad -= toIntSize(m_page.mainFrame().view()->scrollPosition());

    m_quadHighlightConfig = highlightConfig;
    m_highlightQuad = WTFMove(quad);
    update();
}

Node* InspectorOverlay::highlightedNode() const
{
    return m_highlightNode.get();
}

void InspectorOverlay::didSetSearchingForNode(bool enabled)
{
    m_client->didSetSearchingForNode(enabled);
}

void InspectorOverlay::setIndicating(bool indicating)
{
    m_indicating = indicating;

    if (m_indicating)
        evaluateInOverlay(ASCIILiteral("showPageIndication"));
    else
        evaluateInOverlay(ASCIILiteral("hidePageIndication"));

    update();
}

bool InspectorOverlay::shouldShowOverlay() const
{
    return m_highlightNode || m_highlightNodeList || m_highlightQuad || m_indicating || m_showingPaintRects || !m_pausedInDebuggerMessage.isNull();
}

void InspectorOverlay::update()
{
    if (!shouldShowOverlay()) {
        m_client->hideHighlight();
        return;
    }

    FrameView* view = m_page.mainFrame().view();
    if (!view)
        return;

    FrameView* overlayView = overlayPage()->mainFrame().view();
    IntSize viewportSize = view->unscaledVisibleContentSizeIncludingObscuredArea();
    IntSize frameViewFullSize = view->unscaledVisibleContentSizeIncludingObscuredArea(ScrollableArea::IncludeScrollbars);
    overlayView->resize(frameViewFullSize);

    // Clear canvas and paint things.
    // FIXME: Remove extra parameter?
    reset(viewportSize, IntSize());

    // Include scrollbars to avoid masking them by the gutter.
    drawGutter();
    drawNodeHighlight();
    drawQuadHighlight();
    drawPausedInDebuggerMessage();
    drawPaintRects();

    // Position DOM elements.
    overlayPage()->mainFrame().document()->recalcStyle(Style::Force);
    if (overlayView->needsLayout())
        overlayView->layout();

    forcePaint();
}

static Ref<Inspector::Protocol::OverlayTypes::Point> buildObjectForPoint(const FloatPoint& point)
{
    return Inspector::Protocol::OverlayTypes::Point::create()
        .setX(point.x())
        .setY(point.y())
        .release();
}

static Ref<Inspector::Protocol::OverlayTypes::Rect> buildObjectForRect(const FloatRect& rect)
{
    return Inspector::Protocol::OverlayTypes::Rect::create()
        .setX(rect.x())
        .setY(rect.y())
        .setWidth(rect.width())
        .setHeight(rect.height())
        .release();
}

static Ref<Inspector::Protocol::OverlayTypes::Quad> buildArrayForQuad(const FloatQuad& quad)
{
    auto array = Inspector::Protocol::OverlayTypes::Quad::create();
    array->addItem(buildObjectForPoint(quad.p1()));
    array->addItem(buildObjectForPoint(quad.p2()));
    array->addItem(buildObjectForPoint(quad.p3()));
    array->addItem(buildObjectForPoint(quad.p4()));
    return array;
}

static Ref<Inspector::Protocol::OverlayTypes::FragmentHighlightData> buildObjectForHighlight(const Highlight& highlight)
{
    auto arrayOfQuads = Inspector::Protocol::Array<Inspector::Protocol::OverlayTypes::Quad>::create();
    for (auto& quad : highlight.quads)
        arrayOfQuads->addItem(buildArrayForQuad(quad));

    return Inspector::Protocol::OverlayTypes::FragmentHighlightData::create()
        .setQuads(WTFMove(arrayOfQuads))
        .setContentColor(highlight.contentColor.serialized())
        .setContentOutlineColor(highlight.contentOutlineColor.serialized())
        .setPaddingColor(highlight.paddingColor.serialized())
        .setBorderColor(highlight.borderColor.serialized())
        .setMarginColor(highlight.marginColor.serialized())
        .release();
}

static RefPtr<Inspector::Protocol::OverlayTypes::Region> buildObjectForRegion(FrameView* mainView, RenderRegion* region)
{
    FrameView* containingView = region->frame().view();
    if (!containingView)
        return nullptr;

    RenderBlockFlow& regionContainer = downcast<RenderBlockFlow>(*region->parent());
    LayoutRect borderBox = regionContainer.borderBoxRect();
    borderBox.setWidth(borderBox.width() + regionContainer.verticalScrollbarWidth());
    borderBox.setHeight(borderBox.height() + regionContainer.horizontalScrollbarHeight());

    // Create incoming and outgoing boxes that we use to chain the regions toghether.
    const LayoutSize linkBoxSize(10, 10);
    const LayoutSize linkBoxMidpoint(linkBoxSize.width() / 2, linkBoxSize.height() / 2);

    LayoutRect incomingRectBox = LayoutRect(borderBox.location() - linkBoxMidpoint, linkBoxSize);
    LayoutRect outgoingRectBox = LayoutRect(borderBox.location() - linkBoxMidpoint + borderBox.size(), linkBoxSize);

    // Move the link boxes slightly inside the region border box.
    LayoutUnit maxUsableHeight = std::max(LayoutUnit(), borderBox.height() - linkBoxMidpoint.height());
    LayoutUnit linkBoxVerticalOffset = std::min(LayoutUnit::fromPixel(15), maxUsableHeight);
    incomingRectBox.move(0, linkBoxVerticalOffset);
    outgoingRectBox.move(0, -linkBoxVerticalOffset);

    FloatQuad borderRectQuad = regionContainer.localToAbsoluteQuad(FloatRect(borderBox));
    FloatQuad incomingRectQuad = regionContainer.localToAbsoluteQuad(FloatRect(incomingRectBox));
    FloatQuad outgoingRectQuad = regionContainer.localToAbsoluteQuad(FloatRect(outgoingRectBox));

    contentsQuadToPage(mainView, containingView, borderRectQuad);
    contentsQuadToPage(mainView, containingView, incomingRectQuad);
    contentsQuadToPage(mainView, containingView, outgoingRectQuad);

    return Inspector::Protocol::OverlayTypes::Region::create()
        .setBorderQuad(buildArrayForQuad(borderRectQuad))
        .setIncomingQuad(buildArrayForQuad(incomingRectQuad))
        .setOutgoingQuad(buildArrayForQuad(outgoingRectQuad))
        .release();
}

static Ref<Inspector::Protocol::Array<Inspector::Protocol::OverlayTypes::Region>> buildObjectForFlowRegions(RenderRegion* region, RenderFlowThread* flowThread)
{
    FrameView* mainFrameView = region->document().page()->mainFrame().view();

    auto arrayOfRegions = Inspector::Protocol::Array<Inspector::Protocol::OverlayTypes::Region>::create();

    const RenderRegionList& regionList = flowThread->renderRegionList();
    for (auto& iterRegion : regionList) {
        if (!iterRegion->isValid())
            continue;
        RefPtr<Inspector::Protocol::OverlayTypes::Region> regionObject = buildObjectForRegion(mainFrameView, iterRegion);
        if (!regionObject)
            continue;
        if (region == iterRegion) {
            // Let the script know that this is the currently highlighted node.
            regionObject->setIsHighlighted(true);
        }
        arrayOfRegions->addItem(WTFMove(regionObject));
    }

    return arrayOfRegions;
}

static Ref<Inspector::Protocol::OverlayTypes::Size> buildObjectForSize(const IntSize& size)
{
    return Inspector::Protocol::OverlayTypes::Size::create()
        .setWidth(size.width())
        .setHeight(size.height())
        .release();
}

static RefPtr<Inspector::Protocol::OverlayTypes::Quad> buildQuadObjectForCSSRegionContentClip(RenderRegion* region)
{
    Frame* containingFrame = region->document().frame();
    if (!containingFrame)
        return nullptr;

    FrameView* containingView = containingFrame->view();
    FrameView* mainView = containingFrame->page()->mainFrame().view();
    RenderFlowThread* flowThread = region->flowThread();

    // Get the clip box of the current region and covert it into an absolute quad.
    LayoutRect flippedRegionRect(region->flowThreadPortionOverflowRect());
    flowThread->flipForWritingMode(flippedRegionRect);

    // Apply any border or padding of the region.
    flippedRegionRect.setLocation(region->contentBoxRect().location());
    
    FloatQuad clipQuad = region->localToAbsoluteQuad(FloatRect(flippedRegionRect));
    contentsQuadToPage(mainView, containingView, clipQuad);

    return buildArrayForQuad(clipQuad);
}

void InspectorOverlay::setShowingPaintRects(bool showingPaintRects)
{
    if (m_showingPaintRects == showingPaintRects)
        return;

    m_showingPaintRects = showingPaintRects;
    if (!m_showingPaintRects) {
        m_paintRects.clear();
        m_paintRectUpdateTimer.stop();
        drawPaintRects();
        forcePaint();
    }
}

void InspectorOverlay::showPaintRect(const FloatRect& rect)
{
    if (!m_showingPaintRects)
        return;

    IntRect rootRect = m_page.mainFrame().view()->contentsToRootView(enclosingIntRect(rect));

    const std::chrono::milliseconds removeDelay = std::chrono::milliseconds(250);

    std::chrono::steady_clock::time_point removeTime = std::chrono::steady_clock::now() + removeDelay;
    m_paintRects.append(TimeRectPair(removeTime, rootRect));

    if (!m_paintRectUpdateTimer.isActive()) {
        const double paintRectsUpdateIntervalSeconds = 0.032;
        m_paintRectUpdateTimer.startRepeating(paintRectsUpdateIntervalSeconds);
    }

    drawPaintRects();
    forcePaint();
}

void InspectorOverlay::updatePaintRectsTimerFired()
{
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    bool rectsChanged = false;
    while (!m_paintRects.isEmpty() && m_paintRects.first().first < now) {
        m_paintRects.removeFirst();
        rectsChanged = true;
    }

    if (m_paintRects.isEmpty())
        m_paintRectUpdateTimer.stop();

    if (rectsChanged) {
        drawPaintRects();
        forcePaint();
    }
}

void InspectorOverlay::drawPaintRects()
{
    auto arrayOfRects = Inspector::Protocol::Array<Inspector::Protocol::OverlayTypes::Rect>::create();
    for (const auto& pair : m_paintRects)
        arrayOfRects->addItem(buildObjectForRect(pair.second));

    evaluateInOverlay(ASCIILiteral("updatePaintRects"), WTFMove(arrayOfRects));
}

void InspectorOverlay::drawGutter()
{
    evaluateInOverlay(ASCIILiteral("drawGutter"));
}

static RefPtr<Inspector::Protocol::Array<Inspector::Protocol::OverlayTypes::FragmentHighlightData>> buildArrayForRendererFragments(RenderObject* renderer, const HighlightConfig& config)
{
    auto arrayOfFragments = Inspector::Protocol::Array<Inspector::Protocol::OverlayTypes::FragmentHighlightData>::create();

    RenderFlowThread* containingFlowThread = renderer->flowThreadContainingBlock();
    if (!containingFlowThread) {
        Highlight highlight;
        buildRendererHighlight(renderer, nullptr, config, highlight, InspectorOverlay::CoordinateSystem::View);
        arrayOfFragments->addItem(buildObjectForHighlight(highlight));
    } else {
        RenderRegion* startRegion = nullptr;
        RenderRegion* endRegion = nullptr;
        if (!containingFlowThread->getRegionRangeForBox(&renderer->enclosingBox(), startRegion, endRegion)) {
            // The flow has no visible regions. The renderer is not visible on screen.
            return nullptr;
        }

        const RenderRegionList& regionList = containingFlowThread->renderRegionList();
        for (RenderRegionList::const_iterator iter = regionList.find(startRegion); iter != regionList.end(); ++iter) {
            RenderRegion* region = *iter;
            if (region->isValid()) {
                // Compute the highlight of the fragment inside the current region.
                Highlight highlight;
                buildRendererHighlight(renderer, region, config, highlight, InspectorOverlay::CoordinateSystem::View);
                Ref<Inspector::Protocol::OverlayTypes::FragmentHighlightData> fragmentHighlight = buildObjectForHighlight(highlight);

                // Compute the clipping area of the region.
                fragmentHighlight->setRegionClippingArea(buildQuadObjectForCSSRegionContentClip(region));
                arrayOfFragments->addItem(WTFMove(fragmentHighlight));
            }
            if (region == endRegion)
                break;
        }
    }

    return WTFMove(arrayOfFragments);
}

#if ENABLE(CSS_SHAPES)
static FloatPoint localPointToRoot(RenderObject* renderer, const FrameView* mainView, const FrameView* view, const FloatPoint& point)
{
    FloatPoint result = renderer->localToAbsolute(point);
    result = view->contentsToRootView(roundedIntPoint(result));
    result += toIntSize(mainView->scrollPosition());
    return result;
}

struct PathApplyInfo {
    FrameView* rootView;
    FrameView* view;
    Inspector::Protocol::OverlayTypes::DisplayPath* pathArray;
    RenderObject* renderer;
    const ShapeOutsideInfo* shapeOutsideInfo;
};

static void appendPathCommandAndPoints(PathApplyInfo& info, const String& command, const FloatPoint points[], unsigned length)
{
    FloatPoint point;
    info.pathArray->addItem(command);
    for (unsigned i = 0; i < length; i++) {
        point = info.shapeOutsideInfo->shapeToRendererPoint(points[i]);
        point = localPointToRoot(info.renderer, info.rootView, info.view, point);
        info.pathArray->addItem(point.x());
        info.pathArray->addItem(point.y());
    }
}

static void appendPathSegment(PathApplyInfo& pathApplyInfo, const PathElement& pathElement)
{
    FloatPoint point;
    switch (pathElement.type) {
    // The points member will contain 1 value.
    case PathElementMoveToPoint:
        appendPathCommandAndPoints(pathApplyInfo, ASCIILiteral("M"), pathElement.points, 1);
        break;
    // The points member will contain 1 value.
    case PathElementAddLineToPoint:
        appendPathCommandAndPoints(pathApplyInfo, ASCIILiteral("L"), pathElement.points, 1);
        break;
    // The points member will contain 3 values.
    case PathElementAddCurveToPoint:
        appendPathCommandAndPoints(pathApplyInfo, ASCIILiteral("C"), pathElement.points, 3);
        break;
    // The points member will contain 2 values.
    case PathElementAddQuadCurveToPoint:
        appendPathCommandAndPoints(pathApplyInfo, ASCIILiteral("Q"), pathElement.points, 2);
        break;
    // The points member will contain no values.
    case PathElementCloseSubpath:
        appendPathCommandAndPoints(pathApplyInfo, ASCIILiteral("Z"), nullptr, 0);
        break;
    }
}

static RefPtr<Inspector::Protocol::OverlayTypes::ShapeOutsideData> buildObjectForShapeOutside(Frame* containingFrame, RenderBox* renderer)
{
    const ShapeOutsideInfo* shapeOutsideInfo = renderer->shapeOutsideInfo();
    if (!shapeOutsideInfo)
        return nullptr;

    LayoutRect shapeBounds = shapeOutsideInfo->computedShapePhysicalBoundingBox();
    FloatQuad shapeQuad = renderer->localToAbsoluteQuad(FloatRect(shapeBounds));
    contentsQuadToPage(containingFrame->page()->mainFrame().view(), containingFrame->view(), shapeQuad);

    auto shapeObject = Inspector::Protocol::OverlayTypes::ShapeOutsideData::create()
        .setBounds(buildArrayForQuad(shapeQuad))
        .release();

    Shape::DisplayPaths paths;
    shapeOutsideInfo->computedShape().buildDisplayPaths(paths);

    if (paths.shape.length()) {
        auto shapePath = Inspector::Protocol::OverlayTypes::DisplayPath::create();
        PathApplyInfo info;
        info.rootView = containingFrame->page()->mainFrame().view();
        info.view = containingFrame->view();
        info.pathArray = &shapePath.get();
        info.renderer = renderer;
        info.shapeOutsideInfo = shapeOutsideInfo;

        paths.shape.apply([&info](const PathElement& pathElement) {
            appendPathSegment(info, pathElement);
        });

        shapeObject->setShape(shapePath.copyRef());

        if (paths.marginShape.length()) {
            auto marginShapePath = Inspector::Protocol::OverlayTypes::DisplayPath::create();
            info.pathArray = &marginShapePath.get();

            paths.marginShape.apply([&info](const PathElement& pathElement) {
                appendPathSegment(info, pathElement);
            });

            shapeObject->setMarginShape(marginShapePath.copyRef());
        }
    }

    return WTFMove(shapeObject);
}
#endif

static RefPtr<Inspector::Protocol::OverlayTypes::ElementData> buildObjectForElementData(Node* node, HighlightType type)
{
    if (!is<Element>(node) || !node->document().frame())
        return nullptr;

    Element* effectiveElement = downcast<Element>(node);
    if (node->isPseudoElement()) {
        Element* hostElement = downcast<PseudoElement>(*node).hostElement();
        if (!hostElement)
            return nullptr;
        effectiveElement = hostElement;
    }

    Element& element = *effectiveElement;
    bool isXHTML = element.document().isXHTMLDocument();
    auto elementData = Inspector::Protocol::OverlayTypes::ElementData::create()
        .setTagName(isXHTML ? element.nodeName() : element.nodeName().convertToASCIILowercase())
        .setIdValue(element.getIdAttribute())
        .release();

    StringBuilder classNames;
    if (element.hasClass() && is<StyledElement>(element)) {
        HashSet<AtomicString> usedClassNames;
        const SpaceSplitString& classNamesString = downcast<StyledElement>(element).classNames();
        size_t classNameCount = classNamesString.size();
        for (size_t i = 0; i < classNameCount; ++i) {
            const AtomicString& className = classNamesString[i];
            if (usedClassNames.contains(className))
                continue;
            usedClassNames.add(className);
            classNames.append('.');
            classNames.append(className);
        }
    }
    if (node->isPseudoElement()) {
        if (node->pseudoId() == BEFORE)
            classNames.appendLiteral("::before");
        else if (node->pseudoId() == AFTER)
            classNames.appendLiteral("::after");
    }
    if (!classNames.isEmpty())
        elementData->setClassName(classNames.toString());

    RenderElement* renderer = element.renderer();
    if (!renderer)
        return nullptr;

    Frame* containingFrame = node->document().frame();
    FrameView* containingView = containingFrame->view();
    IntRect boundingBox = snappedIntRect(containingView->contentsToRootView(renderer->absoluteBoundingBoxRect()));
    RenderBoxModelObject* modelObject = is<RenderBoxModelObject>(*renderer) ? downcast<RenderBoxModelObject>(renderer) : nullptr;
    auto sizeObject = Inspector::Protocol::OverlayTypes::Size::create()
        .setWidth(modelObject ? adjustForAbsoluteZoom(roundToInt(modelObject->offsetWidth()), *modelObject) : boundingBox.width())
        .setHeight(modelObject ? adjustForAbsoluteZoom(roundToInt(modelObject->offsetHeight()), *modelObject) : boundingBox.height())
        .release();
    elementData->setSize(WTFMove(sizeObject));

    if (type != HighlightType::NodeList && renderer->isRenderNamedFlowFragmentContainer()) {
        RenderNamedFlowFragment& region = *downcast<RenderBlockFlow>(*renderer).renderNamedFlowFragment();
        if (region.isValid()) {
            RenderFlowThread* flowThread = region.flowThread();
            auto regionFlowData = Inspector::Protocol::OverlayTypes::RegionFlowData::create()
                .setName(downcast<RenderNamedFlowThread>(*flowThread).flowThreadName())
                .setRegions(buildObjectForFlowRegions(&region, flowThread))
                .release();
            elementData->setRegionFlowData(WTFMove(regionFlowData));
        }
    }

    RenderFlowThread* containingFlowThread = renderer->flowThreadContainingBlock();
    if (is<RenderNamedFlowThread>(containingFlowThread)) {
        auto contentFlowData = Inspector::Protocol::OverlayTypes::ContentFlowData::create()
            .setName(downcast<RenderNamedFlowThread>(*containingFlowThread).flowThreadName())
            .release();

        elementData->setContentFlowData(WTFMove(contentFlowData));
    }

#if ENABLE(CSS_SHAPES)
    if (is<RenderBox>(*renderer)) {
        auto& renderBox = downcast<RenderBox>(*renderer);
        if (RefPtr<Inspector::Protocol::OverlayTypes::ShapeOutsideData> shapeObject = buildObjectForShapeOutside(containingFrame, &renderBox))
            elementData->setShapeOutsideData(WTFMove(shapeObject));
    }
#endif

    // Need to enable AX to get the computed role.
    if (!WebCore::AXObjectCache::accessibilityEnabled())
        WebCore::AXObjectCache::enableAccessibility();

    if (AXObjectCache* axObjectCache = node->document().axObjectCache()) {
        if (AccessibilityObject* axObject = axObjectCache->getOrCreate(node))
            elementData->setRole(axObject->computedRoleString());
    }

    return WTFMove(elementData);
}

RefPtr<Inspector::Protocol::OverlayTypes::NodeHighlightData> InspectorOverlay::buildHighlightObjectForNode(Node* node, HighlightType type) const
{
    if (!node)
        return nullptr;

    RenderObject* renderer = node->renderer();
    if (!renderer)
        return nullptr;

    RefPtr<Inspector::Protocol::Array<Inspector::Protocol::OverlayTypes::FragmentHighlightData>> arrayOfFragmentHighlights = buildArrayForRendererFragments(renderer, m_nodeHighlightConfig);
    if (!arrayOfFragmentHighlights)
        return nullptr;

    // The main view's scroll offset is shared across all quads.
    FrameView* mainView = m_page.mainFrame().view();

    auto nodeHighlightObject = Inspector::Protocol::OverlayTypes::NodeHighlightData::create()
        .setScrollOffset(buildObjectForPoint(!mainView->delegatesScrolling() ? mainView->visibleContentRect().location() : FloatPoint()))
        .setFragments(WTFMove(arrayOfFragmentHighlights))
        .release();

    if (m_nodeHighlightConfig.showInfo) {
        if (RefPtr<Inspector::Protocol::OverlayTypes::ElementData> elementData = buildObjectForElementData(node, type))
            nodeHighlightObject->setElementData(WTFMove(elementData));
    }

    return WTFMove(nodeHighlightObject);
}

Ref<Inspector::Protocol::Array<Inspector::Protocol::OverlayTypes::NodeHighlightData>> InspectorOverlay::buildObjectForHighlightedNodes() const
{
    auto highlights = Inspector::Protocol::Array<Inspector::Protocol::OverlayTypes::NodeHighlightData>::create();

    if (m_highlightNode) {
        if (RefPtr<Inspector::Protocol::OverlayTypes::NodeHighlightData> nodeHighlightData = buildHighlightObjectForNode(m_highlightNode.get(), HighlightType::Node))
            highlights->addItem(WTFMove(nodeHighlightData));
    } else if (m_highlightNodeList) {
        for (unsigned i = 0; i < m_highlightNodeList->length(); ++i) {
            if (RefPtr<Inspector::Protocol::OverlayTypes::NodeHighlightData> nodeHighlightData = buildHighlightObjectForNode(m_highlightNodeList->item(i), HighlightType::NodeList))
                highlights->addItem(WTFMove(nodeHighlightData));
        }
    }

    return highlights;
}

void InspectorOverlay::drawNodeHighlight()
{
    if (m_highlightNode || m_highlightNodeList)
        evaluateInOverlay("drawNodeHighlight", buildObjectForHighlightedNodes());
}

void InspectorOverlay::drawQuadHighlight()
{
    if (!m_highlightQuad)
        return;

    Highlight highlight;
    buildQuadHighlight(*m_highlightQuad, m_quadHighlightConfig, highlight);
    evaluateInOverlay("drawQuadHighlight", buildObjectForHighlight(highlight));
}

void InspectorOverlay::drawPausedInDebuggerMessage()
{
    if (!m_pausedInDebuggerMessage.isNull())
        evaluateInOverlay("drawPausedInDebuggerMessage", m_pausedInDebuggerMessage);
}

Page* InspectorOverlay::overlayPage()
{
    if (m_overlayPage)
        return m_overlayPage.get();

    PageConfiguration pageConfiguration;
    fillWithEmptyClients(pageConfiguration);
    m_overlayPage = std::make_unique<Page>(pageConfiguration);

    Settings& settings = m_page.settings();
    Settings& overlaySettings = m_overlayPage->settings();

    overlaySettings.setStandardFontFamily(settings.standardFontFamily());
    overlaySettings.setSerifFontFamily(settings.serifFontFamily());
    overlaySettings.setSansSerifFontFamily(settings.sansSerifFontFamily());
    overlaySettings.setCursiveFontFamily(settings.cursiveFontFamily());
    overlaySettings.setFantasyFontFamily(settings.fantasyFontFamily());
    overlaySettings.setPictographFontFamily(settings.pictographFontFamily());
    overlaySettings.setMinimumFontSize(settings.minimumFontSize());
    overlaySettings.setMinimumLogicalFontSize(settings.minimumLogicalFontSize());
    overlaySettings.setMediaEnabled(false);
    overlaySettings.setScriptEnabled(true);
    overlaySettings.setPluginsEnabled(false);

    Frame& frame = m_overlayPage->mainFrame();
    frame.setView(FrameView::create(frame));
    frame.init();
    FrameLoader& loader = frame.loader();
    frame.view()->setCanHaveScrollbars(false);
    frame.view()->setTransparent(true);
    ASSERT(loader.activeDocumentLoader());
    loader.activeDocumentLoader()->writer().setMIMEType("text/html");
    loader.activeDocumentLoader()->writer().begin();
    loader.activeDocumentLoader()->writer().addData(reinterpret_cast<const char*>(InspectorOverlayPage_html), sizeof(InspectorOverlayPage_html));
    loader.activeDocumentLoader()->writer().end();

#if OS(WINDOWS)
    evaluateInOverlay("setPlatform", "windows");
#elif OS(MAC_OS_X)
    evaluateInOverlay("setPlatform", "mac");
#elif OS(UNIX)
    evaluateInOverlay("setPlatform", "linux");
#endif

    return m_overlayPage.get();
}

void InspectorOverlay::forcePaint()
{
    // This overlay page is very weird and doesn't automatically paint. We have to force paints manually.
    m_client->highlight();
}

void InspectorOverlay::reset(const IntSize& viewportSize, const IntSize& frameViewFullSize)
{
    auto configObject = Inspector::Protocol::OverlayTypes::OverlayConfiguration::create()
        .setDeviceScaleFactor(m_page.deviceScaleFactor())
        .setViewportSize(buildObjectForSize(viewportSize))
        .setFrameViewFullSize(buildObjectForSize(frameViewFullSize))
        .release();
    evaluateInOverlay("reset", WTFMove(configObject));
}

void InspectorOverlay::evaluateInOverlay(const String& method)
{
    Ref<InspectorArray> command = InspectorArray::create();
    command->pushString(method);
    overlayPage()->mainFrame().script().evaluate(ScriptSourceCode(makeString("dispatch(", command->toJSONString(), ')')));
}

void InspectorOverlay::evaluateInOverlay(const String& method, const String& argument)
{
    Ref<InspectorArray> command = InspectorArray::create();
    command->pushString(method);
    command->pushString(argument);
    overlayPage()->mainFrame().script().evaluate(ScriptSourceCode(makeString("dispatch(", command->toJSONString(), ')')));
}

void InspectorOverlay::evaluateInOverlay(const String& method, RefPtr<InspectorValue>&& argument)
{
    Ref<InspectorArray> command = InspectorArray::create();
    command->pushString(method);
    command->pushValue(WTFMove(argument));
    overlayPage()->mainFrame().script().evaluate(ScriptSourceCode(makeString("dispatch(", command->toJSONString(), ')')));
}

void InspectorOverlay::freePage()
{
    m_overlayPage = nullptr;
}

} // namespace WebCore
