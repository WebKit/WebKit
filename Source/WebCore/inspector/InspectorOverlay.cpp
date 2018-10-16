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

#include "CacheStorageProvider.h"
#include "DocumentLoader.h"
#include "EditorClient.h"
#include "Element.h"
#include "EmptyClients.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "InspectorClient.h"
#include "InspectorOverlayPage.h"
#include "LibWebRTCProvider.h"
#include "Node.h"
#include "Page.h"
#include "PageConfiguration.h"
#include "PolygonShape.h"
#include "PseudoElement.h"
#include "RTCController.h"
#include "RectangleShape.h"
#include "RenderBoxModelObject.h"
#include "RenderElement.h"
#include "RenderInline.h"
#include "RenderView.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "Settings.h"
#include "SocketProvider.h"
#include "StyledElement.h"
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <wtf/JSONValues.h>

#if PLATFORM(MAC)
#include "LocalDefaultSystemAppearance.h"
#endif

namespace WebCore {

using namespace Inspector;

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

static void buildRendererHighlight(RenderObject* renderer, const HighlightConfig& highlightConfig, Highlight& highlight, InspectorOverlay::CoordinateSystem coordinateSystem)
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
            paddingBox = renderBox.clientBoxRect();
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

        FloatQuad absContentQuad = renderer->localToAbsoluteQuad(FloatRect(contentBox));
        FloatQuad absPaddingQuad = renderer->localToAbsoluteQuad(FloatRect(paddingBox));
        FloatQuad absBorderQuad = renderer->localToAbsoluteQuad(FloatRect(borderBox));
        FloatQuad absMarginQuad = renderer->localToAbsoluteQuad(FloatRect(marginBox));

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

static void buildNodeHighlight(Node& node, const HighlightConfig& highlightConfig, Highlight& highlight, InspectorOverlay::CoordinateSystem coordinateSystem)
{
    RenderObject* renderer = node.renderer();
    if (!renderer)
        return;

    buildRendererHighlight(renderer, highlightConfig, highlight, coordinateSystem);
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

InspectorOverlay::~InspectorOverlay() = default;

void InspectorOverlay::paint(GraphicsContext& context)
{
    if (!shouldShowOverlay())
        return;

#if PLATFORM(MAC)
    LocalDefaultSystemAppearance localAppearance(m_page.mainFrame().document()->useDarkAppearance());
#endif

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
        buildNodeHighlight(*m_highlightNode, m_nodeHighlightConfig, highlight, coordinateSystem);
    else if (m_highlightNodeList) {
        highlight.setDataFromConfig(m_nodeHighlightConfig);
        for (unsigned i = 0; i < m_highlightNodeList->length(); ++i) {
            Highlight nodeHighlight;
            buildNodeHighlight(*(m_highlightNodeList->item(i)), m_nodeHighlightConfig, nodeHighlight, coordinateSystem);
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
        evaluateInOverlay("showPageIndication"_s);
    else
        evaluateInOverlay("hidePageIndication"_s);

    update();
}

bool InspectorOverlay::shouldShowOverlay() const
{
    return m_highlightNode || m_highlightNodeList || m_highlightQuad || m_indicating || m_showingPaintRects || m_showRulers || !m_pausedInDebuggerMessage.isNull();
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
    IntSize frameViewFullSize = view->sizeForVisibleContent(ScrollableArea::IncludeScrollbars);
    overlayView->resize(frameViewFullSize);

    // Clear canvas and paint things.
    IntSize viewportSize = view->sizeForVisibleContent();
    IntPoint scrollOffset = view->scrollPosition();
    reset(viewportSize, scrollOffset);

    // Include scrollbars to avoid masking them by the gutter.
    drawNodeHighlight();
    drawQuadHighlight();
    drawPausedInDebuggerMessage();
    drawPaintRects();

    if (m_showRulers)
        drawRulers();

    // Position DOM elements.
    overlayPage()->mainFrame().document()->resolveStyle(Document::ResolveStyleType::Rebuild);
    if (overlayView->needsLayout())
        overlayView->layoutContext().layout();

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
    auto arrayOfQuads = JSON::ArrayOf<Inspector::Protocol::OverlayTypes::Quad>::create();
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

static Ref<Inspector::Protocol::OverlayTypes::Size> buildObjectForSize(const IntSize& size)
{
    return Inspector::Protocol::OverlayTypes::Size::create()
        .setWidth(size.width())
        .setHeight(size.height())
        .release();
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

    const auto removeDelay = 250_ms;

    MonotonicTime removeTime = MonotonicTime::now() + removeDelay;
    m_paintRects.append(TimeRectPair(removeTime, rootRect));

    if (!m_paintRectUpdateTimer.isActive()) {
        const Seconds paintRectsUpdateInterval { 32_ms };
        m_paintRectUpdateTimer.startRepeating(paintRectsUpdateInterval);
    }

    drawPaintRects();
    forcePaint();
}

void InspectorOverlay::setShowRulers(bool showRulers)
{
    if (m_showRulers == showRulers)
        return;

    m_showRulers = showRulers;

    update();
}

void InspectorOverlay::updatePaintRectsTimerFired()
{
    MonotonicTime now = MonotonicTime::now();
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
    auto arrayOfRects = JSON::ArrayOf<Inspector::Protocol::OverlayTypes::Rect>::create();
    for (const auto& pair : m_paintRects)
        arrayOfRects->addItem(buildObjectForRect(pair.second));

    evaluateInOverlay("updatePaintRects"_s, WTFMove(arrayOfRects));
}

void InspectorOverlay::drawRulers()
{
    evaluateInOverlay("drawRulers"_s);
}

static RefPtr<JSON::ArrayOf<Inspector::Protocol::OverlayTypes::FragmentHighlightData>> buildArrayForRendererFragments(RenderObject* renderer, const HighlightConfig& config)
{
    auto arrayOfFragments = JSON::ArrayOf<Inspector::Protocol::OverlayTypes::FragmentHighlightData>::create();

    Highlight highlight;
    buildRendererHighlight(renderer, config, highlight, InspectorOverlay::CoordinateSystem::View);
    arrayOfFragments->addItem(buildObjectForHighlight(highlight));

    return WTFMove(arrayOfFragments);
}

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
        appendPathCommandAndPoints(pathApplyInfo, "M"_s, pathElement.points, 1);
        break;
    // The points member will contain 1 value.
    case PathElementAddLineToPoint:
        appendPathCommandAndPoints(pathApplyInfo, "L"_s, pathElement.points, 1);
        break;
    // The points member will contain 3 values.
    case PathElementAddCurveToPoint:
        appendPathCommandAndPoints(pathApplyInfo, "C"_s, pathElement.points, 3);
        break;
    // The points member will contain 2 values.
    case PathElementAddQuadCurveToPoint:
        appendPathCommandAndPoints(pathApplyInfo, "Q"_s, pathElement.points, 2);
        break;
    // The points member will contain no values.
    case PathElementCloseSubpath:
        appendPathCommandAndPoints(pathApplyInfo, "Z"_s, nullptr, 0);
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

static RefPtr<Inspector::Protocol::OverlayTypes::ElementData> buildObjectForElementData(Node* node, HighlightType)
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

    if (element.hasClass() && is<StyledElement>(element)) {
        auto classes = JSON::ArrayOf<String>::create();
        HashSet<AtomicString> usedClassNames;
        const SpaceSplitString& classNamesString = downcast<StyledElement>(element).classNames();
        for (size_t i = 0; i < classNamesString.size(); ++i) {
            const AtomicString& className = classNamesString[i];
            if (usedClassNames.contains(className))
                continue;

            usedClassNames.add(className);
            classes->addItem(className);
        }
        elementData->setClasses(WTFMove(classes));
    }

    if (node->isPseudoElement()) {
        if (node->pseudoId() == PseudoId::Before)
            elementData->setPseudoElement("before");
        else if (node->pseudoId() == PseudoId::After)
            elementData->setPseudoElement("after");
    }

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

    if (is<RenderBox>(*renderer)) {
        auto& renderBox = downcast<RenderBox>(*renderer);
        if (RefPtr<Inspector::Protocol::OverlayTypes::ShapeOutsideData> shapeObject = buildObjectForShapeOutside(containingFrame, &renderBox))
            elementData->setShapeOutsideData(WTFMove(shapeObject));
    }

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

    RefPtr<JSON::ArrayOf<Inspector::Protocol::OverlayTypes::FragmentHighlightData>> arrayOfFragmentHighlights = buildArrayForRendererFragments(renderer, m_nodeHighlightConfig);
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

Ref<JSON::ArrayOf<Inspector::Protocol::OverlayTypes::NodeHighlightData>> InspectorOverlay::buildObjectForHighlightedNodes() const
{
    auto highlights = JSON::ArrayOf<Inspector::Protocol::OverlayTypes::NodeHighlightData>::create();

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

    auto pageConfiguration = pageConfigurationWithEmptyClients();
    m_overlayPage = std::make_unique<Page>(WTFMove(pageConfiguration));
    m_overlayPage->setDeviceScaleFactor(m_page.deviceScaleFactor());

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
    auto& writer = loader.activeDocumentLoader()->writer();
    writer.setMIMEType("text/html");
    writer.begin();
    writer.insertDataSynchronously(String(reinterpret_cast<const char*>(InspectorOverlayPage_html), sizeof(InspectorOverlayPage_html)));
    writer.end();

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

void InspectorOverlay::reset(const IntSize& viewportSize, const IntPoint& scrollOffset)
{
    auto configObject = Inspector::Protocol::OverlayTypes::OverlayConfiguration::create()
        .setDeviceScaleFactor(m_page.deviceScaleFactor())
        .setViewportSize(buildObjectForSize(viewportSize))
        .setPageScaleFactor(m_page.pageScaleFactor())
        .setPageZoomFactor(m_page.mainFrame().pageZoomFactor())
        .setScrollOffset(buildObjectForPoint(scrollOffset))
        .setContentInset(buildObjectForSize(IntSize(0, m_page.mainFrame().view()->topContentInset(ScrollView::TopContentInsetType::WebCoreOrPlatformContentInset))))
        .setShowRulers(m_showRulers)
        .release();
    evaluateInOverlay("reset", WTFMove(configObject));
}

static void evaluateCommandInOverlay(Page* page, Ref<JSON::Array>&& command)
{
    page->mainFrame().script().evaluate(ScriptSourceCode(makeString("dispatch(", command->toJSONString(), ')')));
}

void InspectorOverlay::evaluateInOverlay(const String& method)
{
    Ref<JSON::Array> command = JSON::Array::create();
    command->pushString(method);

    evaluateCommandInOverlay(overlayPage(), WTFMove(command));
}

void InspectorOverlay::evaluateInOverlay(const String& method, const String& argument)
{
    Ref<JSON::Array> command = JSON::Array::create();
    command->pushString(method);
    command->pushString(argument);

    evaluateCommandInOverlay(overlayPage(), WTFMove(command));
}

void InspectorOverlay::evaluateInOverlay(const String& method, RefPtr<JSON::Value>&& argument)
{
    Ref<JSON::Array> command = JSON::Array::create();
    command->pushString(method);
    command->pushValue(WTFMove(argument));

    evaluateCommandInOverlay(overlayPage(), WTFMove(command));
}

void InspectorOverlay::freePage()
{
    m_overlayPage = nullptr;
}

} // namespace WebCore
